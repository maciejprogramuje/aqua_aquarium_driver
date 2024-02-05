#include <LCD5110_Graph.h>
#include <TimerOne.h>
#include <virtuabotixRTC.h>
#include <avr/eeprom.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Bounce2.h>

//------------------------------------- nokia -------------------------------------
#define NOKIA_SCK 7
#define NOKIA_MOSI 8
#define NOKIA_DC 9
#define NOKIA_RST 10
#define NOKIA_CS 11

LCD5110 nokia(NOKIA_SCK, NOKIA_MOSI, NOKIA_DC, NOKIA_RST, NOKIA_CS);

extern uint8_t SmallFont[];
extern unsigned char TinyFont[];
extern uint8_t BigNumbers[];

int nokiaLightDelayTemplate = 100;
int nokiaLightDelay;
//------------------------------------- buttons -----------------------------------
#define BUTTON_UP_PIN A2
#define BUTTON_DOWN_PIN A3
#define BUTTON_CLICK_PIN A4

int buttonUp;
int buttonDown;
int buttonClick;
//------------------------------------- rtc ---------------------------------------
#define RTC_CLK 4
#define RTC_DAT 5
#define RTC_RST 6

virtuabotixRTC rtClock(RTC_CLK, RTC_DAT, RTC_RST);

bool isTimePassed = false;
//------------------------------------- termometr ---------------------------------
#define TEMPERATURE_SENSOR A5

OneWire oneWire(TEMPERATURE_SENSOR);
DallasTemperature temperatureSensor(&oneWire);

float temperatureAlarmTemplate;
//------------------------------------- led ---------------------------------------
#define PWM_LED 3

enum LedModes {
  LED_OFF,
  LED_SUNRISE,
  LED_SUNSET,
  LED_ON
};
int ledMode;

int sunriseStartHh;
int sunriseStartMm;
int sunriseEndHh;
int sunriseEndMm;

int sunsetStartHh;
int sunsetStartMm;
int sunsetEndHh;
int sunsetEndMm;
//------------------------------------- screens -----------------------------------
enum Screens {
  MAIN_SCREEN,
  MENU_SCREEN,
  SET_TIME_SCREEN,
  SET_TEMP_ALARM_SCREEN,
  SET_LIGHT_ON_OFF_SCREEN
};
//------------------------------------- menu --------------------------------------
//-- wyświetlanie Serial.println(menuStrings[0]);
enum Menu {
  MAIN_MENU,
  BACK_TO_MAIN_SCREEN,
  TIME_SUNRISE_START,
  TIME_SUNRISE_END,
  TIME_SUNSET_START,
  TIME_SUNSET_END,
  SET_CLOCK,
  SET_TEMP_ALARM,
  SET_LIGHT_ON_OFF
};

#define IDNAME(menuName) #menuName

const char* menuNames[] = {
  IDNAME(MAIN_MENU),
  IDNAME(BACK_TO_MAIN_SCREEN),
  IDNAME(TIME_SUNRISE_START),
  IDNAME(TIME_SUNRISE_END),
  IDNAME(TIME_SUNSET_START),
  IDNAME(TIME_SUNSET_END),
  IDNAME(SET_CLOCK),
  IDNAME(SET_TEMP_ALARM),
  IDNAME(SET_LIGHT_ON_OFF)
};

const char* menuStrings[] = {
  "Main menu",
  "[...]",
  "Sunrise start",
  "Sunrise end",
  "Sunset start",
  "Sunset end",
  "Set clock",
  "Temp. alarm",
  "LED on/off"
};

int menu;
int menuSelectedLine;
//------------------------------------- eeprom ------------------------------------
//-- adresacja pamieci eeprom (nieulotnej)
//-- nazwa { początkowy bajt, ilosc zajmowanych bajtow }
int eepromSunriseStartHh[] = { 0, 2 };
int eepromSunriseStartMm[] = { 2, 2 };

int eepromSunriseEndHh[] = { 4, 2 };
int eepromSunriseEndMm[] = { 6, 2 };

int eepromSunsetStartHh[] = { 8, 2 };
int eepromSunsetStartMm[] = { 10, 2 };

int eepromSunsetEndHh[] = { 12, 2 };
int eepromSunsetEndMm[] = { 14, 2 };

int eepromTemperatureAlarmTemplate[] = { 16, 4 };

//---------------------------------------------------------------------------------
//------------------------------------ setup --------------------------------------
//---------------------------------------------------------------------------------
void setup() {
  Serial.begin(9600);

  nokia.InitLCD();
  nokia.setFont(SmallFont);
  setNokiaLight(false);
  nokia.clrScr();

  //                      seconds, minutes, hours, day of the week, day of the month, month, year
  //rtClock.setDS1302Time(      0,        32,    12,               1,               21,     2, 2022);

  temperatureAlarmTemplate = getValueFromEeprom(eepromTemperatureAlarmTemplate[0], eepromTemperatureAlarmTemplate[1], 30.0);
  sunriseStartHh = getValueFromEeprom(eepromSunriseStartHh[0], eepromSunriseStartHh[1], 10);
  sunriseStartMm = getValueFromEeprom(eepromSunriseStartMm[0], eepromSunriseStartMm[1], 0);
  sunriseEndHh = getValueFromEeprom(eepromSunriseEndHh[0], eepromSunriseEndHh[1], 10);
  sunriseEndMm = getValueFromEeprom(eepromSunriseEndMm[0], eepromSunriseEndMm[1], 15);
  sunsetStartHh = getValueFromEeprom(eepromSunsetStartHh[0], eepromSunsetStartHh[1], 18);
  sunsetStartMm = getValueFromEeprom(eepromSunsetStartMm[0], eepromSunsetStartMm[1], 45);
  sunsetEndHh = getValueFromEeprom(eepromSunsetEndHh[0], eepromSunsetEndHh[1], 19);
  sunsetEndMm = getValueFromEeprom(eepromSunsetEndMm[0], eepromSunsetEndMm[1], 0);

  temperatureSensor.begin();
  temperatureSensor.setWaitForConversion(false);

  pinMode(PWM_LED, OUTPUT);

  menu = MAIN_SCREEN;
  menuSelectedLine = 1;

  nokiaLightDelay = nokiaLightDelayTemplate;

  buttonUp = -1;
  buttonDown = -1;
  buttonClick = -1;
}
//---------------------------------------------------------------------------------
//------------------------------------ LOOP ---------------------------------------
//---------------------------------------------------------------------------------
void loop() {
  //update zegara z RTC
  rtClock.updateTime();

  //ustaw ledMode w zależności od aktualnego czasu
  setLedModeByTime();

  //steruj głównym oświetleniem LED
  manageLed();

  //steruj podświetleniem wyświetlacza
  manageNokiaLight();

  //pokaż właściwy ekran w zależności od menu
  showScreenDependOnMenu();


  //Serial.println("lightMode=" + String(lightMode) + ", tempLightModeManual=" + String(tempLightModeManual));
}

//------------------------------------ menu ---------------------------------------
void showScreenDependOnMenu() {
  switch (menu) {
    case MAIN_MENU:
      showMenuScreen();
      break;
    case BACK_TO_MAIN_SCREEN:
      showMainScreen(
        getDateNowForScreen(),
        getTimeNowForScreen(),
        convertTemperatureToString(getTemperatureFromSensor()));
      break;
    case TIME_SUNRISE_START:
      showTimeSettingScreen(sunriseStartHh, sunriseStartMm);
      break;
    case TIME_SUNRISE_END:
      showTimeSettingScreen(sunriseEndHh, sunriseEndMm);
      break;
    case TIME_SUNSET_START:
      showTimeSettingScreen(sunsetStartHh, sunsetStartMm);
      break;
    case TIME_SUNSET_END:
      showTimeSettingScreen(sunsetEndHh, sunsetEndMm);
      break;
    case SET_CLOCK:
      showTimeSettingScreen(int(rtClock.hours), int(rtClock.minutes));
      break;
    case SET_TEMP_ALARM:
      showTemperatureAlarmTemplateSettingScreen(
        convertTemperatureToString(temperatureAlarmTemplate));
      break;
    case SET_LIGHT_ON_OFF:
      showLedOnOffManualScreen();
      break;
  }
}
//------------------------------------ LED ----------------------------------------
void setLedModeByTime() {
  unsigned long tNow = millis();

  unsigned long tSunriseStartTime = convertTimeToMillis(sunriseStartHh, sunriseStartMm, 0);
  unsigned long tSunriseEndTime = convertTimeToMillis(sunriseEndHh, sunriseEndMm, 0);
  unsigned long tSunsetStartTime = convertTimeToMillis(sunsetStartHh, sunsetStartMm, 0);
  unsigned long tSunsetEndTime = convertTimeToMillis(sunsetEndHh, sunsetEndMm, 0);

  if (tSunriseStartTime <= tNow && tNow <= tSunriseEndTime) {
    ledMode = LED_SUNRISE;
  } else if (tSunriseEndTime <= tNow && tNow <= tSunsetStartTime) {
    ledMode = LED_ON;
  } else if (tSunsetStartTime <= tNow && tNow <= tSunsetEndTime) {
    ledMode = LED_SUNSET;
  } else {
    ledMode = LED_OFF;
  }
}

void manageLed() {
  switch (ledMode) {
    case LED_SUNRISE:
      unsigned long sunriseStartTime = convertTimeToMillis(sunriseStartHh, sunriseStartMm, 0);
      unsigned long sunriseEndTime = convertTimeToMillis(sunriseEndHh, sunriseEndMm, 0);

      smoothPWMIncrease(sunriseStartTime, sunriseEndTime, true, 256);
      break;
    case LED_ON:
      analogWrite(PWM_LED, 255);
      break;
    case LED_SUNSET:
      unsigned long sunsetStartTime = convertTimeToMillis(sunsetStartHh, sunsetStartMm, 0);
      unsigned long sunsetEndTime = convertTimeToMillis(sunsetEndHh, sunsetEndMm, 0);

      smoothPWMIncrease(sunsetStartTime, sunsetEndTime, false, 256);
      break;
    case LED_OFF:
      analogWrite(PWM_LED, 0);
      break;
  }
}

void smoothPWMIncrease(unsigned long animationStart, unsigned long animationEnd, bool increase, int steps) {
  unsigned long currentTime = millis();

  if (currentTime >= animationStart && currentTime <= animationEnd) {
    int elapsedTime = currentTime - animationStart;
    int i = map(elapsedTime, 0, animationEnd - animationStart, 0, steps);

    if (!increase) {
      i = steps - i;  // Jeżeli malejemy, odwróć wartości
    }

    int pwmValue = map(i, 0, steps, 0, 255);
    analogWrite(PWM_LED, pwmValue);
  }
}
//------------------------------------ NOKIA --------------------------------------
void setNokiaLight(bool state) {
  if (state) {
    analogWrite(6, 50);  // nokia świeci
  } else {
    analogWrite(6, 255);  // nokia nie świeci
  }
}

void manageNokiaLight() {
  if (nokiaLightDelay > 0) {
    setNokiaLight(true);
  } else {
    setNokiaLight(false);
    nokiaLightDelay = 0;
  }
  nokiaLightDelay--;
}

void resetNokiaLightDelay() {
  nokiaLightDelay = nokiaLightDelayTemplate;
}
//------------------------------------ TEMPERATURA --------------------------------
float getTemperatureFromSensor() {
  float temperatureFromSensor = 0.0;

  if (temperatureSensor.isConversionComplete()) {
    temperatureSensor.requestTemperatures();
    temperatureFromSensor = temperatureSensor.getTempCByIndex(0);
  }

  return temperatureFromSensor;
}

String convertTemperatureToString(float tTemperatureFloat) {
  char tTemperatureStr[6];
  dtostrf(tTemperatureFloat, 3, 1, tTemperatureStr);
  return tTemperatureStr;
}
//------------------------------------ UTLIS --------------------------------------
String leadingZero(String s) {
  if (s.length() < 2) {
    return "0" + s;
  }
  return s;
}

String getDateNowForScreen() {
  return leadingZero(String(rtClock.dayofmonth))
         + "/"
         + leadingZero(String(rtClock.month))
         + "/"
         + leadingZero(String(rtClock.year));
}

String getTimeNowForScreen() {
  return leadingZero(String(int(rtClock.hours)))
         + ":"
         + leadingZero(String(int(rtClock.minutes)))
         + ":"
         + leadingZero(String(rtClock.seconds));
}

template <typename T>
T getValueFromEeprom(int tStartByte, int tSizeByte, T defaultValue) {
  T result;
  eeprom_read_block(&result, tStartByte, tSizeByte);
  if (isnan(result)) result = defaultValue;
  return result;
}

unsigned long convertTimeToMillis(int hours, int minutes, int seconds) {
  return hours * 3600000 + minutes * 60000 + seconds * 1000;
}
//------------------------------------ SCREENS ------------------------------------
void showMenuScreen() {
  nokia.clrScr();
  nokia.setFont(SmallFont);
  for (int i = 1; i < sizeof(menuStrings) / sizeof(menuStrings[0]); i++) {
    if (menuSelectedLine == i) nokia.invertText(true);
    if (menuSelectedLine > 7) {
      nokia.print(menuStrings[i], LEFT, (i - 6) * 9);
    } else if (menuSelectedLine > 5) {
      nokia.print(menuStrings[i], LEFT, (i - 4) * 9);
    } else if (menuSelectedLine > 3) {
      nokia.print(menuStrings[i], LEFT, (i - 2) * 9);
    } else {
      nokia.print(menuStrings[i], LEFT, i * 9);
    }
    nokia.invertText(false);
  }
  nokia.update();
}

void showMainScreen(String tDateNow, String tTimeNow, String tTemperature) {
  nokia.clrScr();
  nokia.setFont(TinyFont);
  nokia.print(tDateNow, 9, 2);
  nokia.drawLine(0, 8, 83, 8);
  nokia.drawLine(0, 0, 83, 0);
  nokia.print(tTimeNow, RIGHT, 2);
  nokia.setFont(BigNumbers);
  nokia.print(tTemperature, 9, 15);
  nokia.setFont(SmallFont);
  nokia.print("o", 66, 14);
  nokia.print("C", 73, 16);
  nokia.drawLine(0, 47, 84, 47);
  drawLedModeRectangle();
  nokia.update();
}

void showTimeSettingScreen(int tHh, int tMm) {
  nokia.clrScr();
  nokia.setFont(BigNumbers);
  nokia.print(leadingZero(String(tHh)), 7, 20);
  nokia.print(".", CENTER, 14);
  nokia.print(".", CENTER, 8);
  nokia.print(leadingZero(String(tMm)), 49, 20);
  nokia.setFont(TinyFont);
  nokia.invertText(true);
  nokia.drawLine(0, 0, 84, 0);
  nokia.print(" UP  - set hours      ", 0, 1);
  nokia.drawLine(0, 8, 84, 8);
  nokia.print(" DOWN - set minutes   ", 0, 9);
  nokia.invertText(false);
  nokia.setFont(SmallFont);
  nokia.update();
}

void showTemperatureAlarmTemplateSettingScreen(String tTemperatureAlarm) {
  nokia.clrScr();
  nokia.setFont(BigNumbers);
  nokia.print(tTemperatureAlarm, 9, 20);
  nokia.setFont(SmallFont);
  nokia.print("o", 66, 19);
  nokia.print("C", 73, 21);
  nokia.setFont(TinyFont);
  nokia.invertText(true);
  nokia.drawLine(0, 0, 84, 0);
  nokia.print(" UP  - higher temp.   ", 0, 1);
  nokia.drawLine(0, 8, 84, 8);
  nokia.print(" DOWN - lower temp.   ", 0, 9);
  nokia.invertText(false);
  nokia.setFont(SmallFont);
  nokia.update();
}

void showLedOnOffManualScreen() {
  nokia.clrScr();
  nokia.setFont(TinyFont);
  nokia.invertText(true);
  nokia.drawLine(0, 0, 84, 0);
  nokia.print(" UP  - leds off      ", 0, 1);
  nokia.drawLine(0, 8, 84, 8);
  nokia.print(" DOWN - leds on      ", 0, 9);
  nokia.invertText(false);
  nokia.setFont(SmallFont);

  const char* offText = " OFF ";
  const char* onText = " ON ";

  nokia.invertText(ledMode == LED_OFF);
  nokia.print(offText, 30, 22);
  nokia.invertText(false);
  nokia.print(offText, 60, 22);

  nokia.invertText(ledMode == LED_ON);
  nokia.print(onText, 60, 22);
  nokia.invertText(false);
  nokia.print(onText, 30, 22);

  nokia.update();
}

//------------------------------------ LED MODE RECTANGLES ------------------------
void drawLedModeRectangle() {
  nokia.clrRect(0, 0, 6, 8);
  nokia.drawRect(0, 0, 6, 8);

  switch (ledMode) {
    case LED_SUNRISE:
      drawSunrise();
      break;

    case LED_SUNSET:
      drawSunset();
      break;

    case LED_OFF:
      drawOff();
      break;

    case LED_ON:
      // Draw rectangle for LED_ON case
      break;

    default:
      break;
  }
}

void drawSunrise() {
  nokia.drawLine(2, 2, 3, 2);
  nokia.drawLine(4, 2, 5, 2);
  nokia.drawLine(2, 4, 3, 4);
  nokia.drawLine(4, 4, 5, 4);
  nokia.drawLine(2, 6, 3, 6);
  nokia.drawLine(4, 6, 5, 6);
}

void drawSunset() {
  nokia.drawLine(0, 2, 6, 2);
  nokia.drawLine(0, 4, 6, 4);
  nokia.drawLine(0, 6, 6, 6);
  nokia.drawLine(2, 0, 2, 8);
  nokia.drawLine(4, 0, 4, 8);
  nokia.drawLine(6, 0, 6, 8);
}

void drawOff() {
  nokia.drawRect(1, 1, 5, 7);
  nokia.drawRect(2, 2, 4, 6);
  nokia.drawRect(3, 3, 3, 5);
}








// void setMenuAndSetHhMmFromEeprom(int hhEepromFirstByte, int mmEepromFirstByte) {
//   eeprom_read_block(&hh, hhEepromFirstByte, 2);
//   eeprom_read_block(&mm, mmEepromFirstByte, 2);
// }

// void backToMainMenu(int backToMainMenuLine) {
// menu = 1;
// //mainMenuLine = backToMainMenuLine;
// nokia.invert(false);
// }

// void backToMainMenuWithPosAndSetEepromFromHhMm(int mainMenuLineNum, int hhEepromFirstByte, int mmEepromFirstByte) {
// int prevHh = 0;
// int prevMm = -1;
// int nextHh = 23;
// int nextMm = 60;
// if (hhEepromFirstByte >= 4) {
//   eeprom_read_block(&prevHh, hhEepromFirstByte - 4, 2);
//   eeprom_read_block(&prevMm, mmEepromFirstByte - 4, 2);
// }
// if (hhEepromFirstByte <= 16) {
//   eeprom_read_block(&nextHh, hhEepromFirstByte + 4, 2);
//   eeprom_read_block(&nextMm, mmEepromFirstByte + 4, 2);
// }

// int prevTime = prevHh * 60 + prevMm;
// int nextTime = nextHh * 60 + nextMm;
// int nowTime = hh * 60 + mm;

// if (nowTime > prevTime && nowTime < nextTime) {
//   eeprom_write_block(&hh, hhEepromFirstByte, 2);
//   eeprom_write_block(&mm, mmEepromFirstByte, 2);
//   backToMainMenu(mainMenuLineNum);
// } else {
//   menu = 18;
// }
// }


//==================================================================================================================
//obsługa enkodera
//==================================================================================================================
// void manageMenuByEncoderRotate() {
//   encPos += encoder.getValue();
//   if (encPos != oldEncPos) {
//     resetNokiaLightDelay();

//     if (menu == 1) {
//       if (oldEncPos > encPos) {
//         mainMenuLine--;
//         if (mainMenuLine < 0) mainMenuLine = 0;
//       } else {
//         mainMenuLine++;
//         if (mainMenuLine > 8) mainMenuLine = 8;
//       }
//       showMainMenuWithInvertedLine(mainMenuLine);
//     } else if (menu == 12 || menu <= 13 || menu == 16) {
//       setHhMmByEncPos();
//     } else if (menu == 17) {
//       setTemperatureAlarmTemplateByEncPos();
//     } else if (menu == 19) {
//       manualLedOnOffByEncPos();
//     }

//     oldEncPos = encPos;
//   }
// }

// void manageMenuByEncoderClick() {
//   buttonState = encoder.getButton();
//   if (buttonState != 0) {
//     resetNokiaLightDelay();

//     if (menu == 0) {
//       menu = 1;
//       mainMenuLine = 0;
//     } else if (menu == 1) {
//       if (mainMenuLine == 0) {
//         menu = 0;
//       } else if (mainMenuLine == 1 || mainMenuLine == 2) {
//         menu = 10 + mainMenuLine;
//         setMenuAndSetHhMmFromEeprom(((mainMenuLine - 1) * 4), (mainMenuLine * 4 - 2));
//       } else if (mainMenuLine == 3 || mainMenuLine == 4) {
//         menu = 10 + mainMenuLine;
//         setMenuAndSetTimeLenhgtFromEeprom(mainMenuLine * 4 - 2);
//       } else if (mainMenuLine == 5) {
//         menu = 15;
//         hh = int(rtClock.hours);
//         mm = int(rtClock.minutes);
//       } else if (mainMenuLine == 6) {
//         menu = 16;
//       } else if (mainMenuLine == 7) {
//         menu = 18;
//       }
//     } else if (menu == 11 || menu == 12) {
//       backToMainMenuWithPosAndSetEepromFromHhMm((menu - 10), ((mainMenuLine - 1) * 4), (mainMenuLine * 4 - 2));
//     } else if (menu == 13 || menu == 14) {
//       backToMainMenuWithPosAndSetEepromFromTimeLenght();
//     } else if (menu == 15) {
//       rtClock.setDS1302Time(0, mm, hh, int(rtClock.dayofweek), int(rtClock.dayofmonth), int(rtClock.month), int(rtClock.year));
//       backToMainMenu(7);
//     } else if (menu == 17) {
//       eeprom_write_block(&temperatureAlarmTemplate, 24, 4);
//       backToMainMenu(8);
//     } else if (menu == 18) {
//       backToMainMenu(mainMenuLine);
//     } else if (menu == 19) {
//       // if (tempLightModeManual == 0) {
//       //   backToMenuOne(9);
//       // } else if (tempLightModeManual == 1) {
//       //   digitalWrite(relay1, HIGH);
//       //   lightMode = 5;
//       // } else if (tempLightModeManual == 2) {
//       //   digitalWrite(relay1, LOW);
//       //   lightMode = 5;
//       // }
//     }
//   }
// }

// void setHhMmByEncPos() {
//   if (oldEncPos > encPos) {
//     hh++;
//     if (hh > 23) hh = 0;
//   } else {
//     mm++;
//     if (mm > 59) mm = 0;
//   }
// }





//void setTemperatureAlarmTemplateByEncPos() {
// if (oldEncPos > encPos) {
//   temperatureAlarmTemplate = temperatureAlarmTemplate - 0.1;
//   if (temperatureAlarmTemplate <= 0.0) temperatureAlarmTemplate = 0.0;
// } else {
//   temperatureAlarmTemplate = temperatureAlarmTemplate + 0.1;
//   if (temperatureAlarmTemplate >= 99.9) temperatureAlarmTemplate = 99.9;
// }
//}

//void showInvalidTimeAlertScreen() {
// nokia.clrScr();
// nokia.setFont(TinyFont);
// nokia.print("The time must be", CENTER, 4);
// nokia.print("greater than", CENTER, 12);
// nokia.print("the previous step", CENTER, 20);
// nokia.print("time and less than", CENTER, 28);
// nokia.print("the next step time!", CENTER, 36);
// nokia.drawRect(0, 0, 83, 47);
// nokia.update();
//}


// void manageRelay() {
//   int sec = rtClock.seconds;

//   setRelayHhMmFromEeprom(20, 22); //sunset
//   if (rtClock.hours == relayHh && rtClock.minutes == relayMm) {
//     if (sec == 0 && relayFactor) {
//       displaySleep();
//       setRelayStateAndFactor(HIGH); //blue off
//       lightMode = 4;
//       displayWakeup();
//     } else if (sec == 1) {
//       relayFactor = true;
//     }
//   }
// }