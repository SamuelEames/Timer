/* Big screen timer! - Largely based off examples in MD_Parola library
	* 2 modes implemented
		* Stopwatch (counts up to 999 seconds, then stops)
		* Reaction test timer
	* Switch between modes by holding both buttons for 10 seconds

DISPLAY INFO
Display the time in a double height display with a fixed width font.
- Time is shown in a user defined seven segment font
- MD_MAX72XX library can be found at https://github.com/MajicDesigns/MD_MAX72XX

Each font file has the lower part of a character as ASCII codes 0-127 and the
upper part of the character in ASCII code 128-255. Adding 128 to each lower
character creates the correct index for the upper character.
The upper and lower portions are managed as 2 zones 'stacked' on top of each other
so that the module numbers are in the sequence shown below:

* Modules (like FC-16) that can fit over each other with no gap
 n n-1 n-2 ... n/2+1   <- this direction top row
 n/2 ... 3  2  1  0    <- this direction bottom row

Sending the original string to the lower zone and the modified (+128) string to the
upper zone creates the complete message on the display.

*/

// Include the things!
#include <MD_Parola.h>
#include <MD_MAX72xx.h>
#include <SPI.h>
#include "Font_Data.h"
#include "ENUMVars.h"

// DISPLAY SETUP
#define HARDWARE_TYPE 	MD_MAX72XX::FC16_HW
#define MAX_ZONES 			2
#define ZONE_SIZE 			5
#define MAX_DEVICES 		(MAX_ZONES * ZONE_SIZE)

#define ZONE_UPPER  		1
#define ZONE_LOWER  		0

// IO Setup
#define DISP_CLK_PIN   	15
#define DISP_DATA_PIN  	16
#define DISP_CS_PIN    	10
#define BEEP_PIN 				5

#define BTN_START_PIN   18
#define BTN_RESET_PIN   19
#define LED_PIN     		6
#define RAND_ANALOG_PIN	21 												// Analog pin -- Leave pin floating -- used to seed random number generator

// Rection Test Vars
#define RT_DELAY_MIN 		1500											// Minimum delay period for start of reaction test
#define RT_DELAY_MAX		8000											// Maximum delay period for start of reaction test

timerStates currentState = ST_SW_READY;
timerStates lastState = ST_NULL;


// Hardware SPI connection
MD_Parola P = MD_Parola(HARDWARE_TYPE, DISP_CS_PIN, MAX_DEVICES);

#define SPEED_TIME  		0
#define PAUSE_TIME  		0

#define MAX_MESG  			9


#define DISP_REFRESH 		50 												// (ms) update rate of display
#define DEBOUNCE				300												// (ms) button debounce
#define FLASH 					300												// (ms) Period timer flashes with once stopped

#define TIMEOUT					FLASH * 34								// Timeout - change state if nothing happened in this time 


// Global variables
char  dispText_L[MAX_MESG];
char  dispText_H[MAX_MESG];


void getSWTimerText(char *psz, uint32_t elapsedMillis)
{
	// Generates string from given value
	uint8_t  secs, secs_remain, mins_remainder;
	uint16_t mins;

	elapsedMillis += 60000000 - 5000;

	secs 						= elapsedMillis / 1000;									// Calculate seconds
	secs_remain 		= elapsedMillis % 1000; 								// Remainder of milliseconds
	mins  					= elapsedMillis / 60000;
	mins_remainder 	= (elapsedMillis % 60000) / 1000;

	if (elapsedMillis < 1000)
		sprintf(psz, "%c%03d", '.', elapsedMillis);										// ".000" .ms-ms-ms
	else if (elapsedMillis < 10000 )
		sprintf(psz, "%01d%c%02d", secs, '.', secs_remain /10);				// "0.00" s.ms-ms
	else if (elapsedMillis < 100000)								// We only have space for 3 characters and '.'
		sprintf(psz, "%02d%c%01d", secs, '.', secs_remain /100);			// "00.0" ss.ms
	else if (elapsedMillis < 600000)															// (60*9+60)*1000 = 600000
		sprintf(psz, "%01d%c%02d", mins, ':', mins_remainder);		// "0.00" m.ss
	else if (elapsedMillis < 6000000)															// (60*99+60)*1000 = 6000000
		sprintf(psz, "%02d%c%01d", mins, ':', mins_remainder /10);		// "00.0" mm.s 
	else if (elapsedMillis >= 60000000) 													// 1000*60*1000 = 60000000
	{
		currentState = ST_SW_READY;										// Stop timer after 999 mins (I'm assuming no one will use this longer than 16 or so hours)
		beepPattern(3, 50);
	}
	else
		sprintf(psz, "%d", mins);																			// "000" mmm

	return;
}

void beepPattern(uint8_t num, uint8_t period)
{
	// Beeps given number of times with beep length = period
	// Note: this is a blocking function because I used delay

	for (uint8_t i = 0; i < num; ++i)
	{
		digitalWrite(BEEP_PIN, LOW);
		delay(period);
		digitalWrite(BEEP_PIN, HIGH);
		delay(period);
	}

	return;
}


void createHString(char *pH, char *pL)
{
	// Create use low string to create high string
	for (; *pL != '\0'; pL++)
		*pH++ = *pL | 0x80;														// offset character

	*pH = '\0';																			// terminate the string

	return;
}


void checkGameChanger()
{
	// Changes game mode if reset button held for TIMEOUT period (about 10s)
	static uint32_t modetimer_start = 0;

	if (!digitalRead(BTN_RESET_PIN)) 								// Hold reset button
	{
		if (millis() - modetimer_start > TIMEOUT)
		{
			if (currentState < ST_RT_LOBBY) 						// Change mode & beep to indicate change
			{
				currentState = ST_RT_LOBBY;								// Change to Reaction Test mode
				beepPattern(5, 50);
			}
			else
			{
				currentState = ST_SW_READY; 							// Change to StopWatch mode
				beepPattern(3, 50);
			}
			lastState = ST_NULL;
			modetimer_start = millis(); 								// Reset mode change timer
		}
	}
	else
		modetimer_start = millis();

	return;
}


void setup(void)
{
	// Initialise pins
	pinMode(BTN_START_PIN, INPUT_PULLUP);
	pinMode(BTN_RESET_PIN, INPUT_PULLUP);
	pinMode(LED_PIN, OUTPUT);
	pinMode(RAND_ANALOG_PIN, INPUT);
	pinMode(BEEP_PIN, OUTPUT);

	// initialise the LED display
	P.begin(MAX_ZONES);

	// Set up zones for 2 halves of the display
	P.setZone(ZONE_LOWER, 0, ZONE_SIZE - 1);
	P.setZone(ZONE_UPPER, ZONE_SIZE, MAX_DEVICES - 1);

	P.displayZoneText(ZONE_LOWER, dispText_L, PA_CENTER, SPEED_TIME, PAUSE_TIME, PA_PRINT, PA_NO_EFFECT);
	P.displayZoneText(ZONE_UPPER, dispText_H, PA_CENTER, SPEED_TIME, PAUSE_TIME, PA_PRINT, PA_NO_EFFECT);

	P.setIntensity(2); 															// Brightness Range: 0-15

	// Turn off beeper
	delay(50);
	digitalWrite(BEEP_PIN, HIGH);					
}



void loop()
{
	static uint32_t timer_startTime = 0; 						// Start time of timer
	static uint32_t disp_lastTime = 0; 							// used for refesh screen interval
	static uint32_t timer_elapsed = 0;							// Elapsed time of timer

	bool startBtn = 1; 															// Buttons are normally high, so initialise variables to that
	static bool startBtn_last = 1;
	bool resetBtn = 1;
	static bool resetBtn_Last = 1;

	static bool flasher;
	static uint8_t Brightness = 0;
	static int8_t direction = 1;

	static uint16_t RT_Delay;

	switch (currentState)
	{
		case ST_SW_READY:
			// Start screen for stopwatch
			if (lastState != currentState)
			{
				lastState = currentState;
				analogWrite(LED_PIN, 255); 
				sprintf(dispText_L, "---");
				createHString(dispText_H, dispText_L);

				P.setIntensity(4);
				P.setFont(numeric7SegDouble_V2);
				P.setCharSpacing(2); 											// double height --> double spacing
			}

			resetBtn = digitalRead(BTN_RESET_PIN);			// Get button states
			startBtn = digitalRead(BTN_START_PIN);

			if (!startBtn && resetBtn) 									// Start game if (only) start button pressed
			{
				timer_startTime = millis();
				currentState = ST_SW_RUN;
				startBtn_last = startBtn;
				digitalWrite(BEEP_PIN, LOW);
			}
			break;

		case ST_SW_RUN:
			// Set button LED Brightness
			analogWrite(LED_PIN, Brightness);
			if (currentState != lastState)
				lastState = currentState;

			Brightness += direction;
			if (Brightness > 255)
				direction *= -1;
			else if (Brightness < 0)
				direction *= -1;

			//Check button
			if (millis() - timer_startTime > DEBOUNCE)	// don't allow resets in first second
			{
				startBtn = digitalRead(BTN_START_PIN);
				digitalWrite(BEEP_PIN, HIGH);

				if (!startBtn) 														// If start button pressed...
				{
					if (startBtn_last)											// and wasn't prevously
						currentState = ST_SW_STOP;						// Change state! (debounce in next state)
				}
				startBtn_last = startBtn;
			}
		
			// Show timer
			getSWTimerText(dispText_L, millis() - timer_startTime);
			createHString(dispText_H, dispText_L);

			break;


		case ST_SW_STOP:
			// Flashes stopwatch result

			// TODO - Update to be reset by start button like in reaction test
			analogWrite(LED_PIN, 0);

			if (currentState != lastState)
			{
				lastState = currentState;
				disp_lastTime = millis();
			}

			if (millis() - disp_lastTime >= FLASH)
			{
				disp_lastTime = millis();
				flasher = !flasher;
			}

			// flashes display by alternating overall brightness
			if (flasher)
				P.setIntensity(0);
			else
				P.setIntensity(5);

			startBtn = digitalRead(BTN_START_PIN);			// Get button states
			resetBtn = digitalRead(BTN_RESET_PIN);

			if (!resetBtn && startBtn) 									// If (only) reset button pressed...
			{
				if (resetBtn_Last)												// and wasn't prevously
				{
					currentState = ST_SW_READY;							// Reset!
					P.setIntensity(2);
				}
			}
			resetBtn_Last = resetBtn;										// Save button states
			startBtn_last = startBtn;				

			break;

		case ST_RT_LOBBY:
			// Display "Reaction Test" on screen

			if (lastState != currentState)
			{
				lastState = currentState;
				analogWrite(LED_PIN, 255);

				P.setIntensity(7);
				P.setFont(SE_CapsNums_V2);								// Change font to single height
				P.setCharSpacing(1); 											// 1 Column space

				// Display message
				sprintf(dispText_H, "REACTION");
				sprintf(dispText_L, "TEST");
			}


			// Check for button press to begin game
			startBtn = digitalRead(BTN_START_PIN);

			if (!startBtn)
			{
				if (startBtn_last)
					currentState = ST_RT_READY;
			}

			startBtn_last = startBtn;	

			break;


		case ST_RT_READY:
			// Waits for random time length before starting reaction test timer
			if (lastState != currentState)
			{
				analogWrite(LED_PIN, 0);

				// Display "ready"
				P.setIntensity(4);
				P.setFont(numeric7SegDouble_V2);
				P.setCharSpacing(1);
				sprintf(dispText_L, "READY");
				createHString(dispText_H, dispText_L);

				if	(!digitalRead(BTN_START_PIN))
					break; 																	// do nothing until button is released from previous state
				lastState = currentState;

				randomSeed(analogRead(RAND_ANALOG_PIN)); 	// Seed random number generator
				RT_Delay = random(RT_DELAY_MIN, RT_DELAY_MAX);
				timer_startTime = millis(); 							// Initiate random delay timer timer
			}

			// Wait for random time length (after button released from previous state)
			if (timer_startTime + RT_Delay < millis())
			{
				currentState = ST_RT_GO;

				// Setup 'GO' text so we don't waste time updating display during reaction test
				P.setCharSpacing(2);
				sprintf(dispText_L, "GO!");
				createHString(dispText_H, dispText_L);

				// Turn on all the indicators to help player
				analogWrite(LED_PIN, 255);
				digitalWrite(BEEP_PIN, LOW);
				timer_startTime = millis();								// Start timing!
			}

			// Check button for early presses (begin checking after 1s for debounce reasons, etc) 
			if (millis() - timer_startTime > RT_DELAY_MIN/2)
			{
				if (!digitalRead(BTN_START_PIN))
				{
					timer_elapsed = 0;											// Record elapsed time
					currentState = ST_RT_STOP;							// Jump to new state
				}
			}
			break;


		case ST_RT_GO:
			// (Accurately) measure elapsed time
			if (lastState != currentState)
				lastState = currentState;

			// Check for button press
			if (!digitalRead(BTN_START_PIN))
			{
				timer_elapsed = millis() - timer_startTime;		// Record elapsed time
				currentState = ST_RT_STOP;								// Jump to new state
			}
			// Stop if >10s has elapsed
			else if (millis() - timer_startTime > TIMEOUT)
			{
				timer_elapsed = TIMEOUT;									// Record elapsed time
				currentState = ST_RT_STOP;								// Jump to new state
			}
			break;


		case ST_RT_STOP:
			// Shows result of reaction test

			if (currentState != lastState)							// If first run of this state
			{
				lastState = currentState;
				startBtn_last = digitalRead(BTN_START_PIN);

				digitalWrite(BEEP_PIN, HIGH);

				if (timer_elapsed == 0) 									// Button was pushed early
				{
					sprintf(dispText_L, "EARLY");
					P.setCharSpacing(1);
				}
				else if (timer_elapsed >= TIMEOUT) 				// Button was pushed late (timed out)
				{
					sprintf(dispText_L, "LATE");
					P.setCharSpacing(1);
				}
				else  																		// Display reaction time 
				{
					P.setCharSpacing(2);
					getSWTimerText(dispText_L, timer_elapsed);
				}

				createHString(dispText_H, dispText_L);
				// P.displayAnimate();

				timer_startTime = millis();
				while ((millis() - timer_startTime) > DEBOUNCE)
					timer_startTime = millis();

				analogWrite(LED_PIN, 255);
			}


			if (millis() - disp_lastTime >= FLASH)
			{
				disp_lastTime = millis();
				flasher = !flasher;
			}

			// flashes display by alternating overall brightness
			if (flasher)
				P.setIntensity(0);
			else
				P.setIntensity(5);


			if (millis() - timer_startTime > TIMEOUT) 				// After timeout period elapsed, go back to lobby
				currentState = ST_RT_LOBBY;
			else if (millis() - timer_startTime > DEBOUNCE)		// Check for button press to play again
			{
				startBtn = digitalRead(BTN_START_PIN);					// Get button states

				if (!startBtn && startBtn_last) 								// If start button pressed & wasn't perviously
					currentState = ST_RT_READY;										// Reset!

				startBtn_last = startBtn;	
			}

			break;


		default:
			break;

	}

	if (lastState != ST_RT_GO)
	{
		P.displayReset();
		P.displayAnimate();
		checkGameChanger();
	}
}



// TODO - add beeps to stopwatch when button pressed
