/*******************************************************************************
* ControLeo Reflow Oven Controller
* Author: Peter Easton
* Website: whizoo.com
* Software written to work best on ControLeo2 (www.whizoo.com)
*
* This is an example of a reflow oven controller. The reflow curve below is for a
* lead-free profile.  The Reflow Wizard is capable of self-calibrating a reflow oven
* to the J-STD-20 reflow specification.
*
* Temperature (Degree Celcius)                 Magic Happens Here!
* 245-|                                               x  x  
*     |                                            x        x
*     |                                         x              x
*     |                                      x                    x
* 200-|                                   x                          x
*     |                              x    |                          |   x   
*     |                         x         |                          |       x
*     |                    x              |                          |
* 150-|               x                   |                          |
*     |             x |                   |                          |
*     |           x   |                   |                          | 
*     |         x     |                   |                          | 
*     |       x       |                   |                          | 
*     |     x         |                   |                          |
*     |   x           |                   |                          |
* 30 -| x             |                   |                          |
*     |<  60 - 90 s  >|<    60 - 120 s   >|<       60 - 150 s       >|
*     | Preheat Stage |   Soaking Stage   |       Reflow Stage       | Cool
*  0  |_ _ _ _ _ _ _ _|_ _ _ _ _ _ _ _ _ _|_ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ 
*                                                                Time (Seconds)
*
* This firmware builds on the work of other talented individuals:
* ==========================================
* Rocketscream (www.rocketscream.com)
* Produced the Arduino reflow oven shield and code that inspired this project.
*
* ==========================================
* Limor Fried of Adafruit (www.adafruit.com)
* Author of Arduino MAX6675 library. Adafruit has been the source of tonnes of
* tutorials, examples, and libraries for everyone to learn.
*
* Disclaimer
* ==========
* Dealing with high voltage is a very dangerous act! Please make sure you know
* what you are dealing with and have proper knowledge before hand. Your use of 
* any information or materials on this reflow oven controller is entirely at 
* your own risk, for which we shall not be liable. 
*
* Released under WTFPL license.
*
* Revision  Description
* ========  ===========
* 1.0       Initial public release. (21 October 2014)
* 1.1       Bug fixes (30 December 2014)
*           - Oven temperature might not reach configured maximum temperature
*           - Adjusted values so learning happens faster
*           - Other minor improvements
* 1.2       Improvements (6 January 2014)
*           - Take temperature readings every 0.125s and average them.  Fixes errors
*             generated by noise from convection fans
*           - Refined learning mode so learning should happen in fewer runs
*           - Improved loop duration timer to enhance timing
*           - Moved some strings from RAM to Flash (there is only 2.5Kb of RAM)
* 1.3       No user-facing changes (19 January 2015)
*           - Fixed compiler warnings for Arduino 1.5.8
* 1.4       Added support for servos (6 February 2015)
*           - When using a servo, please connect a large capacitor (e.g. 220uF, 16V)
*             between +5V and GND to avoid a microcontroller reset when the servo
*             stall current causes the voltage to drop.  Small servos may not need
*             a capacitor
*           - Connect the servo to +5V, GND and D3
*           - The open and close positions are configured in the Settings menu
*           - The oven door will open when the reflow is done, and close at 50C.
*             The door is also closed when ControLeo2 is turned on.
*******************************************************************************/


// ***** INCLUDES *****
#include <ControLeo2.h>
#include "ReflowWizard.h"

// ***** TYPE DEFINITIONS *****

ControLeo2_LiquidCrystal lcd;

int mode = 0;

void setup() {
  // *********** Start of ControLeo2 initialization ***********
  // Set up the buzzer and buttons
  pinMode(CONTROLEO_BUZZER_PIN, OUTPUT);
  pinMode(CONTROLEO_BUTTON_TOP_PIN, INPUT_PULLUP);
  pinMode(CONTROLEO_BUTTON_BOTTOM_PIN, INPUT_PULLUP);
  // Set the relays as outputs and turn them off
  // The relay outputs are on D4 to D7 (4 outputs)
  for (int i=4; i<8; i++) {
    pinMode(i, OUTPUT);
    digitalWrite(i, LOW);
  }
  // Set up the LCD's number of rows and columns 
  lcd.begin(16, 2);
  // Create the degree symbol for the LCD - you can display this with lcd.print("\1") or lcd.write(1)
  unsigned char degree[8]  = {12,18,18,12,0,0,0,0};
  lcd.createChar(1, degree);
  // *********** End of ControLeo2 initialization ***********
  
  // Log data to the computer using USB
  Serial.begin(57600);
  
  // Initialize the timer used to take thermocouple readings and control the servo
  initializeTimer();

  // Write the initial message on the LCD screen
  lcdPrintLine(0, "   ControLeo2");
  lcdPrintLine(1, "Reflow Oven v1.4");
  delay(100);
  playTones(TUNE_STARTUP);
  delay(3000);
  lcd.clear();
  
  // Initialize the EEPROM, after flashing bootloader
  InitializeSettingsIfNeccessary();
  
  // Go straight to reflow menu if learning is complete
  if (getSetting(SETTING_LEARNING_MODE) == false)
    mode = 2;
  
  printToSerial(STR_CONTROLEO2_REFLOW_OVEN_CONTROLLER_VX_X);
  
  // Make sure the oven door is closed
  setServoPosition(getSetting(SETTING_SERVO_CLOSED_DEGREES), 1000);
}


// The main menu has 3 options
boolean (*action[NO_OF_MODES])() = {Testing, Config, Reflow};
const char* modes[NO_OF_MODES] = {"Test Outputs?", "Setup?", "Start Reflow?"};


// This loop is executed 20 times per second
void loop()
{
  static boolean drawMenu = true;
  static boolean showMainMenu = true;
  static int counter = 0;
  static unsigned long nextLoopTime = 50; // Should be 3000 + 100 + fudge factor + 50 - but no harm making it 50!
  
  
  if (showMainMenu) {
    if (drawMenu) {
      drawMenu = false;
      lcdPrintLine(0, modes[mode]);
      lcdPrintLine(1, "          Yes ->");
    }
    
    // Update the temperature roughtly once per second
    if (counter++ % 20 == 0)
      displayTemperature(getCurrentTemperature());
    
    // Get the button press to select the mode or move to the next mode
    switch (getButton()) {
    case CONTROLEO_BUTTON_TOP:
      // Move to the next mode
      mode = (mode + 1) % NO_OF_MODES;
      drawMenu = true;
      break;
    case CONTROLEO_BUTTON_BOTTOM:
      // Move to the selected mode
      showMainMenu = false;
      drawMenu = true;
      break;
    }
  }
  else {
    // Go to the mode's menu system
    if ((*action[mode])() == NEXT_MODE)
      showMainMenu = true;
  }
  
  // Execute this loop 20 times per second (every 50ms)
  if (millis() < nextLoopTime)
    delay(nextLoopTime - millis());
  nextLoopTime += 50;
}


// Determine if a button was pressed (with debounce)
// A button can only be pressed once every 200ms. If a button is
// pressed and held, a button press will be generated every 200ms.
// Returns:
//   CONTROLEO_BUTTON_NONE if no button are pressed
//   CONTROLEO_BUTTON_TOP if the top button was pressed
//   CONTROLEO_BUTTON_BOTTOM if the bottom button was pressed
// Note: If both buttons are pressed simultaneously, CONTROLEO_BUTTON_TOP will be returned
#define DEBOUNCE_INTERVAL  200

int getButton()
{
  static long lastChangeMillis = 0;
  long nowMillis = millis();
  int buttonValue;
  
  // If insufficient time has passed, just return none pressed
  if (lastChangeMillis + DEBOUNCE_INTERVAL > nowMillis)
    return CONTROLEO_BUTTON_NONE;
  
  // Read the current button status
  buttonValue = CONTROLEO_BUTTON_NONE;
  if (digitalRead(CONTROLEO_BUTTON_TOP_PIN) == LOW) {
    buttonValue = CONTROLEO_BUTTON_TOP;
    playTones(TUNE_TOP_BUTTON_PRESS);
  }
  else if (digitalRead(CONTROLEO_BUTTON_BOTTOM_PIN) == LOW) {
    buttonValue = CONTROLEO_BUTTON_BOTTOM;
    playTones(TUNE_BOTTOM_BUTTON_PRESS);
  }
    
  // Note the time the button was pressed
  if (buttonValue != CONTROLEO_BUTTON_NONE)
   lastChangeMillis = nowMillis;
  
  return buttonValue;
}


// Display a line on the LCD screen
// The provided string is padded to take up the whole line
// There is less flicker when overwriting characters on the screen, compared
// to clearing the screen and writing new information
void lcdPrintLine(int line, const char* str) {
  char buffer[17] = "                ";
  // Sanity check on the parameters
  if (line < 0 || line > 1 || !str || strlen(str) > 16)
    return;
  lcd.setCursor(0, line);
  strncpy(buffer, str, strlen(str));
  lcd.print(buffer);
}


// Displays the temperature in the bottom left corner of the LCD display
void displayTemperature(double temperature) {
  lcd.setCursor(0, 1);
  if (THERMOCOUPLE_FAULT(temperature)) {
    lcd.print("        ");
    return;
  }
  lcd.print(temperature);
  // Print degree Celsius symbol
  lcd.print("\1C ");  
}


