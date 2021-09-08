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

// SETUP DEBUG MESSAGES
// #define DEBUG   //If you comment this line, the DPRINT & DPRINTLN lines are defined as blank.
#ifdef DEBUG
	#define DPRINT(...)		Serial.print(__VA_ARGS__)		//DPRINT is a macro, debug print
	#define DPRINTLN(...)	Serial.println(__VA_ARGS__)	//DPRINTLN is a macro, debug print with new line
#else
	#define DPRINT(...)												//now defines a blank line
	#define DPRINTLN(...)											//now defines a blank line
#endif


// Include the things!
#include <MD_Parola.h>
#include <MD_MAX72xx.h>
#include <SPI.h>
#include "Font_Data.h"
#include "ENUMVars.h"

// DISPLAY SETUP
#define HARDWARE_TYPE 	MD_MAX72XX::FC16_HW
#define MAX_ZONES 		2
#define ZONE_SIZE 		5
#define MAX_DEVICES 		(MAX_ZONES * ZONE_SIZE)

#define ZONE_UPPER  		1
#define ZONE_LOWER  		0

// IO Setup
#define DISP_CLK_PIN   	15
#define DISP_DATA_PIN  	16
#define DISP_CS_PIN    	10

#define BTN_START_PIN   18
#define BTN_RESET_PIN   19
#define LED_PIN     		6
#define RAND_ANALOG_PIN	21 				// Analog pin -- Leave pin floating -- used to seed random number generator

// Rection Test Vars
#define RT_DELAY_MIN 		1500			// Minimum delay period for start of reaction test
#define RT_DELAY_MAX		8000			// Maximum delay period for start of reaction test

timerStates currentState = ST_RT_LOBBY;
timerStates lastState = ST_SW_READY;


// Hardware SPI connection
MD_Parola P = MD_Parola(HARDWARE_TYPE, DISP_CS_PIN, MAX_DEVICES);

#define SPEED_TIME  		75
#define PAUSE_TIME  		0

#define MAX_MESG  		6


#define DISP_REFRESH 	100 		// (ms) update rate of display
#define DEBOUNCE			300		// (ms) button debounce
#define FLASH 				300		// (ms) Period timer flashes with once stopped

#define TIMEOUT				FLASH * 34 		// Timeout - change state if nothing happened in this time 


// Global variables
char  dispText_L[MAX_MESG];    // mm:ss\0
char  dispText_H[MAX_MESG];


void getSWTimerText(char *psz, uint32_t elapsedMillis)
{
	// Generates string from given value
	uint16_t  secs, ms_10, ms_100;

	secs 		= (elapsedMillis / 1000);							// Calculate seconds
	ms_10 	= (elapsedMillis % 1000) / 10; 					// Remainder of milliseconds
	ms_100 	= (elapsedMillis % 1000) / 100; 				// Remainder of milliseconds


	if (elapsedMillis < 1000)
		sprintf(psz, "%c%03d", '.', elapsedMillis);
	else if (elapsedMillis < 10000 )
		sprintf(psz, "%01d%c%02d", secs, '.', ms_10);
	else if (elapsedMillis < 100000)							// We only have space for 3 characters and '.'
		sprintf(psz, "%02d%c%01d", secs, '.', ms_100);
	else if (elapsedMillis > 999000)
		currentState = ST_SW_READY;							// Stop timer after 999 seconds (Can't dispay over 999 seconds)
	else
		sprintf(psz, "%d", secs);

	return;
}


void createHString(char *pH, char *pL)
{
	// Create use low string to create high string
	for (; *pL != '\0'; pL++)
		*pH++ = *pL | 0x80;											// offset character

	*pH = '\0';															// terminate the string

	return;
}


void checkGameChanger()
{
	// Changes game mode if both buttons held for 10 seconds while games not running
	;

	return;
}


void setup(void)
{
	// Initialise pins
	pinMode(BTN_START_PIN, INPUT_PULLUP);
	pinMode(BTN_RESET_PIN, INPUT_PULLUP);
	pinMode(LED_PIN, OUTPUT);
	pinMode(RAND_ANALOG_PIN, INPUT);

	// initialise the LED display
	P.begin(MAX_ZONES);

	// Set up zones for 2 halves of the display
	P.setZone(ZONE_LOWER, 0, ZONE_SIZE - 1);
	P.setZone(ZONE_UPPER, ZONE_SIZE, MAX_DEVICES - 1);

	P.setIntensity(2);

	delay(100);						// For some reason the display likes a delay here... not sure why 


	Serial.begin(115200);						// Open comms line
	// while (!Serial) ; 							// Wait for serial port to be available
}



void loop()
{
	static uint32_t timer_startTime = 0; 	// Start time of timer
	static uint32_t disp_lastTime = 0; 	// used for refesh screen interval
	static uint32_t timer_elapsed = 0;	// Elapsed time of timer

	static bool startBtn = 1; 			// Buttons are normally high, so initialise variables to that
	static bool startBtn_last = 1;
	static bool resetBtn = 1;
	static bool resetBtn_Last = 1;

	static bool flasher;
	static uint8_t Brightness = 0;
	static int8_t direction = 1;

	static uint16_t RT_Delay;

	switch (currentState)
	{
		case ST_SW_READY:
			analogWrite(LED_PIN, 255); 

			if (P.getZoneStatus(ZONE_LOWER) && P.getZoneStatus(ZONE_UPPER))
			{
				if (millis() - disp_lastTime > DISP_REFRESH)
				{
					disp_lastTime = millis();
					
					sprintf(dispText_L, "00.0");
					createHString(dispText_H, dispText_L);
					
					P.displayReset();				
					P.synchZoneStart();			// Ensure lower & upper zones are in sync
				}
			}
			P.displayAnimate();											// Show timer

			resetBtn = digitalRead(BTN_RESET_PIN);				// Get button states
			startBtn = digitalRead(BTN_START_PIN);

			if (!startBtn && resetBtn) 							// Start game if (only) start button pressed
			{
				timer_startTime = millis();
				currentState = ST_SW_RUN;
				startBtn_last = startBtn;
			}
			break;

		case ST_SW_RUN:

			// Set button LED Brightness
			analogWrite(LED_PIN, Brightness);

			Brightness += direction;
			if (Brightness > 255)
				direction *= -1;
			else if (Brightness < 0)
				direction *= -1;

		
			// Show timer
			if (P.getZoneStatus(ZONE_LOWER) && P.getZoneStatus(ZONE_UPPER))
			{
				// Adjust the time string if we have to. It will be adjusted
				// every second at least for the flashing colon separator.
				if (millis() - disp_lastTime >= DISP_REFRESH)
				{
					disp_lastTime = millis();
					getSWTimerText(dispText_L, millis() - timer_startTime);
					createHString(dispText_H, dispText_L);
					P.displayReset();
					P.synchZoneStart();			// synchronise the start
				}
			}
			P.displayAnimate();

			//Check button
			if (millis() - timer_startTime > DEBOUNCE)			// don't allow resets in first second
			{
				startBtn = digitalRead(BTN_START_PIN);

				if (!startBtn) 									// If start button pressed...
				{
					if (startBtn_last)							// and wasn't prevously
						currentState = ST_SW_STOP;
				}
				startBtn_last = startBtn;
			}

			break;


		case ST_SW_STOP:

			analogWrite(LED_PIN, 0);

			if (P.getZoneStatus(ZONE_LOWER) && P.getZoneStatus(ZONE_UPPER))
			{
				if (millis() - disp_lastTime >= FLASH)
				{
					disp_lastTime = millis();
					flasher = !flasher;
					P.displayReset();
					P.synchZoneStart();
				}
			}
			P.displayAnimate();

			// flashes display by alternating overall brightness
			if (flasher)
				P.setIntensity(0);
			else
				P.setIntensity(5);

			startBtn = digitalRead(BTN_START_PIN);		// Get button states
			resetBtn = digitalRead(BTN_RESET_PIN);

			if (!resetBtn && startBtn) 					// If (only) reset button pressed...
			{
				if (resetBtn_Last)							// and wasn't prevously
				{
					currentState = ST_SW_READY;			// Reset!
					P.setIntensity(2);
				}
			}
			resetBtn_Last = resetBtn;						// Save button states
			startBtn_last = startBtn;				

			break;

		case ST_RT_LOBBY:
			// Display "Reaction Test" on screen

			if (lastState != currentState)
			{
				Serial.println("LOBBY");
				lastState = currentState;
				analogWrite(LED_PIN, 255);


				P.setIntensity(7);
				P.setFont(SE_CapsNums_V1);		// Change font to single height
				P.setCharSpacing(1); 					// 1 Column space

				// Display message
				P.displayZoneText(ZONE_UPPER, "REACTION", PA_CENTER, SPEED_TIME, PAUSE_TIME, PA_PRINT, PA_PRINT);
				P.displayZoneText(ZONE_LOWER, "TEST", PA_CENTER, SPEED_TIME, PAUSE_TIME, PA_PRINT, PA_PRINT);
				P.synchZoneStart();
				P.displayAnimate();
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

			if (lastState != currentState)
			{
				lastState = currentState;

				analogWrite(LED_PIN, 0);

				// Display "ready"
				P.setIntensity(7);
				P.setFont(numeric7SegDouble_V2);
				P.setCharSpacing(1);

				sprintf(dispText_L, "READY");
				createHString(dispText_H, dispText_L);

				P.displayZoneText(ZONE_LOWER, dispText_L, PA_CENTER, SPEED_TIME, PAUSE_TIME, PA_PRINT, PA_NO_EFFECT);
				P.displayZoneText(ZONE_UPPER, dispText_H, PA_CENTER, SPEED_TIME, PAUSE_TIME, PA_PRINT, PA_NO_EFFECT);
				P.synchZoneStart();

				Serial.println("READY");

				delay(100);
				P.displayAnimate();


				while	(!digitalRead(BTN_START_PIN))
					; // do nothing until button is released from previous state

				randomSeed(analogRead(RAND_ANALOG_PIN)); 		// Seed random number generator
				RT_Delay = random(RT_DELAY_MIN, RT_DELAY_MAX);
				timer_startTime = millis(); 												// Initiate random delay timer timer
			}



			// Wait for random time length (after button released from previous state)
			if (timer_startTime + RT_Delay < millis())
			{
				currentState = ST_RT_GO;

				analogWrite(LED_PIN, 255);

				P.setFont(numeric7SegDouble_V2);
				P.setCharSpacing(2);

				sprintf(dispText_L, "GO!");
				createHString(dispText_H, dispText_L);

				P.displayZoneText(ZONE_LOWER, dispText_L, PA_CENTER, SPEED_TIME, PAUSE_TIME, PA_PRINT, PA_NO_EFFECT);
				P.displayZoneText(ZONE_UPPER, dispText_H, PA_CENTER, SPEED_TIME, PAUSE_TIME, PA_PRINT, PA_NO_EFFECT);
				P.synchZoneStart();
				P.displayAnimate();


				timer_startTime = millis();
			}

			// Check button for early presses (begin checking after 1s for debounce reasons, etc) 
			if (millis() - timer_startTime > RT_DELAY_MIN/2)
			{
				if (!digitalRead(BTN_START_PIN))
				{
					timer_elapsed = 0;								// Record elapsed time
					currentState = ST_RT_STOP;					// Jump to new state
				}
			}
			break;


		case ST_RT_GO:
			// Display "GO" and start timing
			if (lastState != currentState)
			{
				Serial.println("GO!");
				lastState = currentState;
			}

			// Check for button press
			if (!digitalRead(BTN_START_PIN))
			{
				timer_elapsed = millis() - timer_startTime;		// Record elapsed time
				currentState = ST_RT_STOP;											// Jump to new state
			}

			// Stop if >10s has elapsed
			else if (millis() - timer_startTime > TIMEOUT)
			{
				timer_elapsed = TIMEOUT;										// Record elapsed time
				currentState = ST_RT_STOP;											// Jump to new state
			}
			break;


		case ST_RT_STOP:

			if (currentState != lastState)
			{
				lastState = currentState;
				timer_startTime = millis();

				analogWrite(LED_PIN, 255);


				// Serial.println("STOP");
				// Serial.print("Reaction time = ");
				// Serial.print(timer_elapsed);
				// Serial.println(" milli secs");

				if (timer_elapsed == 0) 								// Button was pushed early
				{
					sprintf(dispText_L, "EARLY");
					P.setCharSpacing(1);
				}
				else if (timer_elapsed >= TIMEOUT) 	// Button was pushed late (timed out)
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
				P.displayZoneText(ZONE_LOWER, dispText_L, PA_CENTER, SPEED_TIME, PAUSE_TIME, PA_PRINT, PA_NO_EFFECT);
				P.displayZoneText(ZONE_UPPER, dispText_H, PA_CENTER, SPEED_TIME, PAUSE_TIME, PA_PRINT, PA_NO_EFFECT);
			}


			if (P.getZoneStatus(ZONE_LOWER) && P.getZoneStatus(ZONE_UPPER))
			{
				if (millis() - disp_lastTime >= FLASH)
				{
					disp_lastTime = millis();
					flasher = !flasher;
					P.displayReset();
					P.synchZoneStart();
				}
			}
			P.displayAnimate();

			// flashes display by alternating overall brightness
			if (flasher)
				P.setIntensity(0);
			else
				P.setIntensity(5);

			// Check for button press to play again
			startBtn = digitalRead(BTN_START_PIN);		// Get button states
			resetBtn = digitalRead(BTN_RESET_PIN);

			if (resetBtn && !startBtn) 					// If (only) reset button pressed...
			{
				if (startBtn_last)							// and wasn't prevously
				{
					currentState = ST_RT_READY;			// Reset!
					P.setIntensity(2);
				}
			}
			resetBtn_Last = resetBtn;						// Save button states
			startBtn_last = startBtn;	


			// After timeout period elapsed, go back to lobby
			if (millis() - timer_startTime > TIMEOUT)
			{
				currentState = ST_RT_LOBBY;
				delay(100);															// Delay for display to get its act together
			}

			break;


		default:
			break;

	}
}
