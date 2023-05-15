/*****************
  Tally light ESP32 for Blackmagic ATEM switcher

  Version 2.1

  A wireless (WiFi) tally light for Blackmagic Design
  ATEM video switchers, based on the M5StickCPlus ESP32 development
  board and the Arduino IDE.

  For more information, see:
  https://oneguyoneblog.com/2020/06/13/tally-light-esp32-for-blackmagic-atem-switcher/

  Based on the work of Kasper Skårhøj:
  https://github.com/kasperskaarhoj/SKAARHOJ-Open-Engineering
  
  Download SkaarhojPgmspace,ATEMbase, ATEMstd from here
  https://github.com/kasperskaarhoj/SKAARHOJ-Open-Engineering/tree/master/ArduinoLibs

******************/
#include <M5StickCPlus.h>
#include <WiFi.h>
#include <SkaarhojPgmspace.h>
#include <ATEMbase.h>
#include <ATEMstd.h>

IPAddress switcherIp(192, 168, 1, 1);  // IP address of the ATEM switcher
ATEMstd AtemSwitcher;

const char* ssid = "";
const char* password = "";

// Set this to 1 if you want the orientation to update automatically
#define AUTOUPDATE_ORIENTATION 1

/////////////////////////////////////////////////////////////
// You probably don't need to change things below this line
#define LED_PIN 10

int orientation = 0;
int orientationPrevious = 0;
int orientationMillisPrevious = millis();
int buttonBMillis = 0;

int cameraNumber = 1;
bool showBatteryLevel = false;

int previewTallyPrevious = 1;
int programTallyPrevious = 1;
int cameraNumberPrevious = cameraNumber;
bool showBatteryLevelPrevious = showBatteryLevel;

void setup() {
  Serial.begin(9600);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.println("Connecting to WiFi..");
  }
  Serial.println("Connected to the WiFi network");

  M5.begin();
  M5.Imu.Init();

  M5.Lcd.setRotation(orientation);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);

  AtemSwitcher.begin(switcherIp);
  AtemSwitcher.serialOutput(0x80);
  AtemSwitcher.connect();
}

void setOrientation() {
  float accX = 0, accY = 0, accZ = 0;
  M5.Imu.getAccelData(&accX, &accY, &accZ);
  Serial.printf("%.2f   %.2f   %.2f \n", accX * 1000, accY * 1000, accZ * 1000);

  if (accZ < .9) {
    if (accX > .6) {
      orientation = 1;
    } else if (accX < .4 && accX > -.5) {
      if (accY > 0) {
        orientation = 0;
      } else {
        orientation = 2;
      }
    } else {
      orientation = 3;
    }
  }

  if (orientation != orientationPrevious) {
    Serial.printf("Orientation changed to %d\n", orientation);
    M5.Lcd.setRotation(orientation);
  }
}

void loop() {

  M5.update();

  if (AUTOUPDATE_ORIENTATION) {
    if (orientationMillisPrevious + 500 < millis()) {
      setOrientation();
      orientationMillisPrevious = millis();
    }
  }

  // Buttons Press
  if (M5.BtnA.wasPressed()) {
    buttonBMillis = millis();
    showBatteryLevel = !showBatteryLevel;
    refreshScreen();
  }

  if (M5.BtnB.isPressed())
      AtemSwitcher.changeProgramInput(cameraNumber);

  if (M5.BtnA.isPressed() && buttonBMillis != 0 && buttonBMillis < millis() - 500) {
    Serial.println("Changing camera number");
    cameraNumber = (cameraNumber % 8) + 1;
    Serial.printf("New camera number: %d\n", cameraNumber);

    buttonBMillis = 0;
  }

  // Check for packets, respond to them etc. Keeping the connection alive!
  AtemSwitcher.runLoop();
  refreshScreen();  
}

void refreshScreen()
{
  int programTally = AtemSwitcher.getProgramTally(cameraNumber);
  int previewTally = AtemSwitcher.getPreviewTally(cameraNumber);

  if ((orientation != orientationPrevious) 
        || (cameraNumber != cameraNumberPrevious) 
        || (programTallyPrevious != programTally) 
        || (previewTallyPrevious != previewTally)
        || (showBatteryLevel != showBatteryLevelPrevious)) 
  {  
  if (programTally && !previewTally) {            // only program
      drawLabel(TFT_MAROON, TFT_BLACK, LOW);
    } else if (programTally && previewTally) {    // program AND preview
      drawLabel(TFT_PURPLE, TFT_BLACK, LOW);
    } else if (previewTally && !programTally) {   // only preview
      drawLabel(TFT_DARKGREEN, TFT_BLACK, HIGH);
    } else if (!previewTally || !programTally) {  // neither
      drawLabel(TFT_BLACK, TFT_DARKGREY, HIGH);
    }
  }

  programTallyPrevious = programTally;
  previewTallyPrevious = previewTally;
  cameraNumberPrevious = cameraNumber;
  orientationPrevious = orientation;
  showBatteryLevelPrevious = showBatteryLevel;
}

void drawLabel(unsigned long int screenColor, unsigned long int labelColor, bool ledValue) {
  digitalWrite(LED_PIN, ledValue);
  M5.Lcd.fillScreen(screenColor);
  M5.Lcd.setTextColor(labelColor);
  drawStringInCenter(String(cameraNumber), 8);

  showBatteryInfo();
}

void showBatteryInfo()
{
  int batteryLevel = getBatteryLevel();

  if(batteryLevel <= 15 || showBatteryLevel)
  {
    float voltage = M5.Axp.GetBatVoltage();
    float current = M5.Axp.GetBatCurrent();

    char batteryStatus[9];
    char chargingIcon = current == 0 ? ' ' : (current > 0 ? '+' : '-');
    sprintf(batteryStatus, "%c%.2fv", chargingIcon, voltage);

    M5.Lcd.setTextSize(2);
    M5.Lcd.setCursor(M5.Lcd.width() - 30,10);

    M5.Lcd.setTextDatum(TL_DATUM);
    M5.Lcd.drawString(batteryStatus, 1, 5, 1);

    if(batteryLevel <= 15) 
      M5.Lcd.setTextColor(TFT_RED);
    else if(batteryLevel <= 40) 
      M5.Lcd.setTextColor(TFT_YELLOW);
    else
      M5.Lcd.setTextColor(TFT_GREEN);

    M5.Lcd.setTextDatum(TR_DATUM);
    M5.Lcd.drawString(String(batteryLevel)+ "%", M5.Lcd.width() - 1, 5, 1);
  }
}


void drawStringInCenter(String input, int font) {
  int datumPrevious = M5.Lcd.getTextDatum();
  M5.Lcd.setTextDatum(MC_DATUM);
  M5.Lcd.setTextSize(1);
  M5.Lcd.drawString(input, M5.Lcd.width() / 2, M5.Lcd.height() / 2, font);
  M5.Lcd.setTextDatum(datumPrevious);
}


int getBatteryLevel(void) {
  uint16_t vbatData = M5.Axp.GetVbatData();
  double vbat = vbatData * 1.1 / 1000;
  double percentage = 100.0 * ((vbat - 3.4) / (4.07 - 3.4));

  if(percentage <= 0)
    return 0;
  else if(percentage >= 100)
    return 100;
  else 
    return percentage;
}
