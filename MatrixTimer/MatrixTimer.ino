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



// Global variables
char  dispText_L[MAX_MESG];    // mm:ss\0
char  dispText_H[MAX_MESG];


void getSWTimerText(char *psz, uint32_t elapsedMillis)
{
	// Generates string from given value
	uint16_t  s, ms;

	s = elapsedMillis / 1000;								// Calculate seconds
	ms = (elapsedMillis % 1000) / 100; 					// First decimal place of seconds

	if (elapsedMillis < 100000)							// We only have space for 3 characters
		sprintf(psz, "%02d%c%01d", s, '.', ms);
	else if (elapsedMillis > 999000)
		currentState = ST_SW_READY;							// Stop timer after 999 seconds
		//sprintf(psz, "%c%c%c", '0', '0', '0');			// Can't dispay over 999 seconds
	else
		sprintf(psz, "%02d", s);

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

	// initialise the LED display
	P.begin(MAX_ZONES);

	// Set up zones for 2 halves of the display
	P.setZone(ZONE_LOWER, 0, ZONE_SIZE - 1);
	P.setZone(ZONE_UPPER, ZONE_SIZE, MAX_DEVICES - 1);
	P.setFont(numeric7SegDouble);


	P.setCharSpacing(P.getCharSpacing() * 2); 				// double height --> double spacing

	P.displayZoneText(ZONE_LOWER, dispText_L, PA_CENTER, SPEED_TIME, PAUSE_TIME, PA_PRINT, PA_NO_EFFECT);
	P.displayZoneText(ZONE_UPPER, dispText_H, PA_CENTER, SPEED_TIME, PAUSE_TIME, PA_PRINT, PA_NO_EFFECT);

	P.setIntensity(2);
}


// Global variables
uint8_t  curText;
const char *pc = "READY";


void loop(void)
{
	static uint32_t timer_startTime = 0; 	// Start time of timer
	static uint32_t lastTime = 0; 	// used for refesh screen interval

	static bool startBtn = 1; 			// Buttons are normally high, so initialise variables to that
	static bool startBtn_last = 1;
	static bool resetBtn = 1;
	static bool resetBtn_Last = 1;

	static bool flasher;
	static uint8_t Brightness = 0;
	static int direction = 1;

	switch (currentState)
	{
		case ST_SW_READY:
			analogWrite(LED_PIN, 255); 

			if (P.getZoneStatus(ZONE_LOWER) && P.getZoneStatus(ZONE_UPPER))
			{
				if (millis() - lastTime > DISP_REFRESH)
				{
					lastTime = millis();
					
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
				if (millis() - lastTime >= DISP_REFRESH)
				{
					lastTime = millis();
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
				if (millis() - lastTime >= FLASH)
				{
					lastTime = millis();
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

			// Change font to single height
			P.setFont(SE_CapsNums_V1);
			P.setCharSpacing(1); 
			P.displayZoneText(ZONE_UPPER, "REACTION", PA_CENTER, SPEED_TIME, PAUSE_TIME, PA_PRINT, PA_PRINT);
			P.displayZoneText(ZONE_LOWER, "TEST", PA_CENTER, SPEED_TIME, PAUSE_TIME, PA_PRINT, PA_PRINT);


			P.displayAnimate();

			// Check for button press to begin game


			break;



		case ST_RT_READY:

			// Display "ready"

			// Wait for random time length (after button released from previous state)

			// Check button for early presses (begin checking after 1s for debounce reasons, etc) 

			// Display "GO" and start timing

			break;

		case ST_RT_GO:

			// Check for button press

			break;

		case ST_RT_STOP:

			// Display reaction time 

			// Check for button press to play again

			// Time out after 30 second sand go back to lobby

			break;


		default:
			break;

	}
}
