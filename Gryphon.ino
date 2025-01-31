//Brushless Solenoid Nerf - written by meffken (Instagram: meffken_baut)
//Have fun building your awesome and powerful foam dart blaster <3
// v0.0 Original from https://www.printables.com/model/276689-gryphon-brushless-mod
// v0.1 Modified for FTW solenoid and other stuffs by Karl Hunt
// v0.2 Added better OLED and settings menu - Karl Hunt
// v0.3 Added 2 & 3 shot burst modes - Karl Hunt
// v0.4 Added spin down function and setting - Karl Hunt
// v0.5 Moved all fire routine into seperate function and fixed rev down issue. Slight bug in that now single fire will keep firing if you hold down the trigger (not a major issue though it is low ROF) - Karl Hunt
// v0.6 Storing settings in the eeprom so that they persist reboots. They will be read and stored into variables at boot and then saved to eeprom on settings menu exit. - Karl Hunt
// v0.7 Added Trigger Release value to stop single/burst firing in a loop, Full Auto always fires at least one dart and Solenoid has it's own void. - RB
// v0.8 BEWARE I changed the ESC pins to 10 & 11 - Karl Hunt
// v0.9 Change power to pulse time instead of servo angle - Karl Hunt
// v0.10 Merged RB changes into mine - RB solenoid function and other minor changes.
// v0.11.0 Moved images to a seperate file images.h
// v0.12.0 Added solenoid/motor disconnect toggle in code for testing, added kid mode, added rev trigger override, added more functions for calculating rotation speed and setting motor speed. Also rev trigger on Pin 7. Added Preheat skip if motors are not slow.
// v0.12.1 Fixes to above changes - RB
// v0.12.2 Added Rev Trigger Mode to swap between true Rev trigger and Rev Override
// v0.12.4 Cleaned Up Main Screen, made a little nicer to look at
// v0.12.5 Added a constant motor spin mode to the rev trigger options - but 100% text no longer works
// v0.12.6 Fix solenoid no delay logic by checking how much time has passed in ms between trigger pulls. If it is more than half the preheat value then skip preheat.
// Attempting to rewrite the code to make is simpler for rev trigger.

String version = "QT01";

#include <Servo.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <EEPROM.h>
#include <Fonts/Org_01.h>

#define OLED_RESET -1     // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_WIDTH 128  // OLED display Breite, in pixels
#define SCREEN_HEIGHT 64  // OLED display Höhe, in pixels
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

Servo Motor1;
Servo Motor2;

int debug = 1;  // enable debug mode

const int ButtonPin = 12;  //Connect to Ground for firing
const int ESC1Pin = 10;    // change from 12 to 10 (10 is a pwm pin)
const int ESC2Pin = 11;
const int SelectorPin1 = 4;  //Connect to Ground for a change of the shooting mode   -- Press both buttons to access the settings menu.
const int SelectorPin2 = 5;  //Connect to Ground for a change of the power
const int SolenoidPin = 3;   //Connect this to your Mosfet (I used the wiring of: https://create.arduino.cc/projecthub/Clark3DPR/full-auto-3d-printed-brushless-nerf-blaster-arduino-control-a711b0 )

//Rev Trigger
const int revPin = 7;                  // Blasters Rev Trigger, not used for revving but as a quick override to swap modes.
unsigned long timeofLastDebounce = 0;  // the last time the output pin was toggled
unsigned long delayofDebounce = 100;   // the debounce time; increase if the output flickers
unsigned long timeOfLastTriggerDebounce = 0;
unsigned long delayOfTriggerDebounce = 25;

unsigned long revTime = 0;

int revState = 0;
int revTrigger = 0;

int triggerState = 0;

unsigned long lastFireTime = 0;

const int BuzzerPin = 2;  //buzzer to arduino pin 2

int solenoidOn = 0;  // 1 = Remove solenoid in code for testing
int motorsOn = 0;    // 1 = Remove motors in code for testing

// testing if the trigger was pressed less than the preheat time
int currentPress = 0;
int lastPress = 0;

const int minRotationSpeed = 1040;               // Motor Minimum rotation  speed
const int maxRotationSpeed = 1960;               // Motor Maximum rotation  speed
const int rotationChangeInterval =10;           // Increment to change speed
int actualRotationSpeed = minRotationSpeed;      // The variable actually used by the motors.
int areWheelsAlreadySpinning = 0;                // Variable to know whether to skip Preheat or not.
int preHeatSkipMinRotationSpeed = 1050;
int rotationSpeedIntermediaery = 0;
//String rotationSpeedDisplay = String(rotationChangeInterval) + "%";  // Used for displaying Power Percentage
String rotationSpeedDisplay = String(rotationChangeInterval);
//String rotationSpeedDisplay = "20";


int fireMode = 1;                                         //Firing fireMode 1)single 2)burst 3)full-auto
int BurstAmount = 3;                                      // How many shots is burst?
String burstAmountText = "BURST " + String(BurstAmount);  // Text to hold the Burst Amount menu value
int RotationSpeedPercent = rotationChangeInterval;        // What percentage the power is;
int RotationSpeed = minRotationSpeed;                     // 1050-1950
int pos = 0;                                              //Used by spin down function

int PreheatMin = 0;             //Used for settings menu
int PreheatMax = 800;             //Used for settings menu
int Preheat = 200;                //Time in mS for speeding up the Flywheels before pushing a Dart
int SolenoidOnMin = 15;           //Used for settings menu
int SolenoidOnMax = 200;          //Used for settings menu
int SolenoidOnTime = 25;          //Time in mS for turning on the solenoid (keep this just enough for pushing darts in the flywheels securely) 25 seems good for FTW solenoid on 3s
int SolenoidOffMin = 25;          //Used for settings menu
int SolenoidOffMax = 200;         //Used for settings menu
int SolenoidOffTime = 40;         //Time in mS for turning off the solenoid (keep this just enough for retracting the pusher completely) 40 seems good foo;or FTW solenoid on 3s with double spring
int SolenoidOffTimeFullAuto = 0;  // Additional delay for slowing down full auto.
int SpinDownTimeMin = 0;          //Used for settings menu
int SpinDownTimeMax = 50;         //Used for settings menu
int SpinDownTime = 3;

// EEPROM addresses 2 bytes for each int starting at 0
int EEP_Preheat = 0;  // since this number is bigger than 255 (8bit int) we may need to do some math or something to make it smaller. Possible dived by ten then store, read and multiply by ten?
int EEP_SolenoidOnTime = 2;
int EEP_SolenoidOffTime = 4;
int EEP_SpinDownTime = 6;
int EEP_Status = 20;  // Boolean to see if we have saved to eeprom before.
int EEP_wipe = 0;     // Set to 1 to wipe the EEProm variables. IMPORTANT only do this once then set back to 0 again otherwise you will damage the EEProm.

int Button1State = 1;       //Tracking button presses
int Button2State = 1;       //Tracking button presses
int triggerRelease = 0;     //Used to track if trigger has been released
int revTriggerPressed = 0;  //Used to track if trigger has been released

int settings_state = 0;      //Is the setttings menu active? Used to switch display modes.
int settings_item = 1;       //Tracking current selected settings option
int settings_max_items = 5;  //Total number of settings items. Used to loop through options.

int lastTriggerState = HIGH;

void setup() {
  // EEProm read
  if (EEP_wipe == 1) {
    wipeEEProm();
  } else {
    readEEProm();
  }

  Serial.begin(9600);  // open the serial port at 9600 bps:
  debugMSG("Ready");

  display.begin(SSD1306_SWITCHCAPVCC, 0x3c);  // I2C address = 0x3C
  display.clearDisplay();
  display.setCursor(16, 48);
  display.setTextSize(2);
  display.setTextColor(WHITE);
  displayTextCenter(version, 35);
  display.display();
  delay(2000);
  rotationSpeedDisplay = String(RotationSpeedPercent) + "%";
  updateMenu();

  pinMode(ButtonPin, INPUT_PULLUP);
  pinMode(SelectorPin1, INPUT_PULLUP);
  pinMode(SelectorPin2, INPUT_PULLUP);
  pinMode(SolenoidPin, OUTPUT);

  pinMode(revPin, INPUT_PULLUP);
  pinMode(BuzzerPin, OUTPUT);  // Set buzzer - pin 2 as an output

  digitalWrite(SolenoidPin, LOW);

  Motor1.attach(ESC1Pin, 1000, 2000);
  Motor2.attach(ESC2Pin, 1000, 2000);
  Motor1.write(1000);
  Motor2.write(1000);

  tone(BuzzerPin, 4000, 200);
  delay(400);
  tone(BuzzerPin, 4000, 200);
  delay(400);
  noTone(BuzzerPin);

  
}

void loop() {
  revTrigger = !digitalRead(revPin);
 //debugMSG(String(revTrigger));
  if(revState != revTrigger){
    if(revTrigger == 1){
      SetMotorState(RotationSpeed);
      revTime = millis();
      revState = 1;
    }
    else{
      if((lastFireTime + 200) < millis()){
        SetMotorState(0);
        revState = 0;
      }
    }
  }
  triggerState = digitalRead(ButtonPin);
  if(revState && (revTime + Preheat < millis()) && !triggerState && !triggerRelease) {
        fire();
    }
  // fire
  
  if (triggerRelease) {
    if(triggerState == lastTriggerState){
      if (millis() > timeOfLastTriggerDebounce + delayOfTriggerDebounce && triggerState) {
      triggerRelease = 0;
      }      
    }
    else {
      timeOfLastTriggerDebounce = millis();
    }
    lastTriggerState = triggerState;
  }

  Button1State = digitalRead(SelectorPin1);
  Button2State = digitalRead(SelectorPin2);

  if (Button1State == 0) {
    tone(BuzzerPin, 3500, 50);
    delay(20);
    if (Button2State == 0) {
      settings();
    } else {
      if (settings_state == 0) {
        if (RotationSpeedPercent <= (100 - rotationChangeInterval)) {
          RotationSpeedPercent = RotationSpeedPercent + rotationChangeInterval;
        } else {
          RotationSpeedPercent = rotationChangeInterval;
        }
        debugMSG(String(RotationSpeedPercent));

        rotationSpeedDisplay = String(RotationSpeedPercent) + "%";
        //rotationSpeedDisplay = String(RotationSpeedPercent);
        updateMenu();
      } else {
        settings_value_change();
      }
    }
    delay(200);
  }

  if (Button2State == 0) {
    tone(BuzzerPin, 3400, 50);
    delay(20);
    if (Button1State == 0) {
      settings();
    } else {
      if (settings_state == 0) {
        fireMode++;
        updateMenu();
      } else {
        if (settings_item < settings_max_items) {
          settings_item++;
          settings();
        } else {
          settings_item = 1;
          settings();
        }
      }
      delay(200);
    }
  }
}
// End of void loop

  void updateMenu() {
    display.clearDisplay();
    display.drawRect(0, 0, 128, 64, 1);
    display.drawLine(0, 41, 128, 41, 1);
    display.setFont(&Org_01);
    display.setTextColor(WHITE);
    //Serial.println(fireMode);

    switch (fireMode) {

      case 1:
        //Display: Semi-Auto
        display.setTextSize(2);
        displayTextCenter("SINGLE", 55);
        break;

      case 2:
        //Display: Burst
        display.setTextSize(2);
        displayTextCenter("BURST", 55);
        break;

      case 3:
        //Display: Full-Auto
        display.setTextSize(2);
        displayTextCenter("FULLAUTO", 55);
        break;

      case 4:
        fireMode = 1;
        // Back to single again
        display.setTextSize(2);
        displayTextCenter("SINGLE", 55);
        break;
    }

    calcRotationSpeed(RotationSpeedPercent);
    displayRotationSpeed();

    display.display();
  }

  void calcRotationSpeed(float rotationPercentage) {
    if (rotationPercentage < 1) {
      rotationPercentage = 1;
    } else if (rotationPercentage > 100) {
      rotationPercentage = 100;
    }
    RotationSpeed = minRotationSpeed + ((rotationPercentage / 100) * (maxRotationSpeed - minRotationSpeed));
  }

  void displayRotationSpeed() {
    display.setTextSize(5);
    displayTextCenter(rotationSpeedDisplay, 30);
  }

  void displayTextCenter(String text, int verticalPosition) {
    int16_t x1;
    int16_t y1;
    uint16_t width;
    uint16_t height;

    display.getTextBounds(text, 0, 0, &x1, &y1, &width, &height);
    display.setCursor((SCREEN_WIDTH - width) / 2, verticalPosition);
    display.println(text);  // text to display
  }

  void settings() {
    settings_state = 1;
    display.clearDisplay();

    // Title
    display.setTextSize(2);
    display.setCursor(10, 0);
    display.println("Settings");

    // options
    display.setTextSize(1);
    display.setCursor(10, 23);
    display.println("Preheat:");
    display.setCursor(100, 23);
    display.print(Preheat);
    if (settings_item == 1) {
      display.setCursor(5, 23);
      display.print(">");
    }

    display.setCursor(10, 31);
    display.println("Solenoid On:");
    display.setCursor(100, 31);
    display.print(SolenoidOnTime);
    if (settings_item == 2) {
      display.setCursor(5, 31);
      display.print(">");
    }

    display.setCursor(10, 39);
    display.println("Solenoid Off:");
    display.setCursor(100, 39);
    display.print(SolenoidOffTime);
    if (settings_item == 3) {
      display.setCursor(5, 39);
      display.print(">");
    }

    display.setCursor(10, 47);
    display.println("Spindown Int:");
    display.setCursor(100, 47);
    display.print(SpinDownTime);
    if (settings_item == 4) {
      display.setCursor(5, 47);
      display.print(">");
    }

    display.setCursor(10, 55);
    display.println("Exit");
    if (settings_item == 5) {
      display.setCursor(5, 55);
      display.print(">");
    }

    display.display();
  }

  void settings_value_change() {
    switch (settings_item) {
      case 1:
        if (Preheat < PreheatMax) {
          Preheat = Preheat + 50;
        } else {
          Preheat = PreheatMin;
        }
        settings();
        break;

      case 2:
        if (SolenoidOnTime < SolenoidOnMax) {
          SolenoidOnTime = SolenoidOnTime + 5;
        } else {
          SolenoidOnTime = SolenoidOnMin;
        }
        settings();
        break;

      case 3:
        if (SolenoidOffTime < SolenoidOffMax) {
          SolenoidOffTime = SolenoidOffTime + 5;
        } else {
          SolenoidOffTime = SolenoidOffMin;
        }
        settings();
        break;

      case 4:
        if (SpinDownTime < SpinDownTimeMax) {
          SpinDownTime = SpinDownTime + 1;
        } else {
          SpinDownTime = SpinDownTimeMin;
        }
        settings();
        break;

      case 5:
        settings_state = 0;
        saveEEProm();
        updateMenu();
        break;
    }
  }

  void fire() {
    debugMSG("FireCalled");
    triggerRelease = 1;
    timeOfLastTriggerDebounce = millis();

    if (fireMode == 1) {
      fireSolenoid();
      //spinDown();
    }

    if (fireMode == 2) {
      for (int i = 1; i <= BurstAmount; i++) {
        fireSolenoid();
      }
      //spinDown();
    }

    if (fireMode == 3) {
      fireSolenoid();
      while (!digitalRead(ButtonPin)) {
        fireSolenoid();
        //delay(SolenoidOffTimeFullAuto);  // Variable that allows us to slow down the rate of full auto if we want to
      }
      //spinDown();
    }
    lastFireTime = millis();
  }

  void SetMotorState(int stateRequest) {
    Motor1.write(stateRequest);
    Motor2.write(stateRequest);
  }

  // FIRE Solenoid
  void fireSolenoid() {
    if (solenoidOn == 0) {
      digitalWrite(SolenoidPin, HIGH);
      delay(SolenoidOnTime);
      digitalWrite(SolenoidPin, LOW);
      delay(SolenoidOffTime);
    }
  }

  // save EEProm status if we never saved to it before.
  void saveEEProm() {
    EEPROM.update(EEP_Preheat, Preheat / 10);
    EEPROM.update(EEP_SolenoidOnTime, SolenoidOnTime);
    EEPROM.update(EEP_SolenoidOffTime, SolenoidOffTime);
    EEPROM.update(EEP_SpinDownTime, SpinDownTime);

    if (EEPROM.read(EEP_Status) != 1) {
      EEPROM.update(EEP_Status, 1);
    }
  }

  // Read EEProm values and update the variables if we have written them before
  void readEEProm() {
    if (EEPROM.read(EEP_Status) == 1) {
      Preheat = EEPROM.read(EEP_Preheat) * 10;
      SolenoidOnTime = EEPROM.read(EEP_SolenoidOnTime);
      SolenoidOffTime = EEPROM.read(EEP_SolenoidOffTime);
      SpinDownTime = EEPROM.read(EEP_SpinDownTime);
    }
  }

  // Wipe EEProm values
  void wipeEEProm() {
    EEPROM.update(EEP_Preheat, 0);
    EEPROM.update(EEP_SolenoidOnTime, 0);
    EEPROM.update(EEP_SolenoidOffTime, 0);
    EEPROM.update(EEP_SpinDownTime, 0);
    EEPROM.update(EEP_Status, 0);
  }

  void debugMSG(String myString) {
    if (debug == 1) {
      Serial.println(myString);
    }
  }