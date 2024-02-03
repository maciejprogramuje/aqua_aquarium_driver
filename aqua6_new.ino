#include <LCD5110_Graph.h>
#include <TimerOne.h>
#include <virtuabotixRTC.h>
#include <avr/eeprom.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Bounce2.h>


//------------------------------------- nokia -------------------------------------
LCD5110 nokia(8, 9, 10, 12, 11);
extern uint8_t SmallFont[];
extern unsigned char TinyFont[];
extern uint8_t BigNumbers[];

int nokiaLightDelayTemplate = 100;
int nokiaLightDelay;
//------------------------------------- buttons -----------------------------------
//ClickEncoder encoder(4, 5, 7, 4);
int buttonUp;
int buttonDown;
int buttonClick;
//------------------------------------- rtc ---------------------------------------
virtuabotixRTC rtClock(A0, 2, 3);

bool isTimePassed = false;
//------------------------------------- termometr ---------------------------------
OneWire oneWire(A5);
DallasTemperature temperatureSensor(&oneWire);

float temperatureAlarmTemplate;
//------------------------------------- led ---------------------------------------
enum LedModes {
  LED_OFF,
  LED_MIX,
  LED_ON
};
int ledMode;
//------------------------------------- screens -----------------------------------
enum Screens {
  MAIN_SCREEN,
  MENU_SCREEN,
  SET_TIME_SCREEN,
  SET_LENGTH_SCREEN,
  SET_TEMP_ALARM_SCREEN,
  SET_LIGHT_ON_OFF_SCREEN
};
//------------------------------------- menu --------------------------------------
//-- wyświetlanie Serial.println(menuStrings[0]);
enum Menu {
  MAIN_MENU,
  BACK_TO_MAIN_SCREEN,
  TIME_SUNRISE,
  TIME_SUNSET,
  LENGTH_SUNRISE,
  LENGTH_SUNSET,
  SET_CLOCK,
  SET_TEMP_ALARM,
  SET_LIGHT_ON_OFF
};

#define IDNAME(menuName) #menuName

const char* menuNames[] = {
  IDNAME(MAIN_MENU),
  IDNAME(BACK_TO_MAIN_SCREEN),
  IDNAME(TIME_SUNRISE),
  IDNAME(TIME_SUNSET),
  IDNAME(LENGTH_SUNRISE),
  IDNAME(LENGTH_SUNSET),
  IDNAME(SET_CLOCK),
  IDNAME(SET_TEMP_ALARM),
  IDNAME(SET_LIGHT_ON_OFF)
};

const char* menuStrings[] = {
  "Main menu",
  "[...]",
  "Sunrise time",
  "Sunset time",
  "Sunrise length",
  "Sunset length",
  "Set clock",
  "Temp. alarm",
  "Light on/off"
};

int menu;
int menuSelectedLine;
//------------------------------------- eeprom ------------------------------------
//-- adresacja pamieci eeprom (nieulotnej)
//-- nazwa { początkowy bajt, ilosc zajmowanych bajtow }
int eepromSunriseHh[] = { 0, 2 };
int eepromSunriseMm[] = { 2, 2 };

int eepromSunsetHh[] = { 4, 2 };
int eepromSunsetMm[] = { 6, 2 };

int eepromSunriseLenght[] = { 8, 2 };
int eepromSunsetLenght[] = { 10, 2 };

int eepromTemperatureAlarmTemplate[] = { 12, 4 };

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

  temperatureAlarmTemplate = getFromEeprom(eepromTemperatureAlarmTemplate[0], eepromTemperatureAlarmTemplate[1]);
  if (isnan(temperatureAlarmTemplate)) temperatureAlarmTemplate = 30.0;

  temperatureSensor.begin();
  temperatureSensor.setWaitForConversion(false);

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
  ledMode = setLedModeFromEepromByTime();

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
        getDateNow(),
        getTimeNow(),
        convertTemperatureToString(getTemperatureFromSensor()));
      break;
    case TIME_SUNRISE:
      showTimeSettingScreen(
        getFromEeprom(eepromSunriseHh[0], eepromSunriseHh[1]),
        getFromEeprom(eepromSunriseMm[0], eepromSunriseMm[1]));
      break;
    case TIME_SUNSET:
      showTimeSettingScreen(
        getFromEeprom(eepromSunsetHh[0], eepromSunsetHh[1]),
        getFromEeprom(eepromSunsetMm[0], eepromSunsetMm[1]));
      break;
    case SET_CLOCK:
      showTimeSettingScreen(
        int(rtClock.hours),
        int(rtClock.minutes));
      break;
    case LENGTH_SUNRISE:
      showTimeLengthSettingScreen(
        getFromEeprom(eepromSunriseLenght[0], eepromSunriseLenght[1]));
      break;
    case LENGTH_SUNSET:
      showTimeLengthSettingScreen(
        getFromEeprom(eepromSunsetLenght[0], eepromSunsetLenght[1]));
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
LedModes setLedModeFromEepromByTime() {
  int tNow = convertToMyTime(
    int(rtClock.hours),
    int(rtClock.minutes));

  int tSunriseTime = convertToMyTime(
    getFromEeprom(eepromSunriseHh[0], eepromSunriseHh[1]),
    getFromEeprom(eepromSunriseMm[0], eepromSunriseMm[1]));

  int tSunsetTime = convertToMyTime(
    getFromEeprom(eepromSunsetHh[0], eepromSunsetHh[1]),
    getFromEeprom(eepromSunsetMm[0], eepromSunsetMm[1]));

  int tSunriseLenght = getFromEeprom(eepromSunriseLenght[0], eepromSunriseLenght[1]);
  int tSunsetLenght = getFromEeprom(eepromSunsetLenght[0], eepromSunsetLenght[1]);

  if ((tSunriseTime < tNow < (tSunriseTime + tSunriseLenght))
      || ((tSunsetTime - tSunsetLenght) < tNow < tSunsetTime)) {
    return LED_MIX;
  } else if ((tSunriseTime + tSunriseLenght) < tNow < (tSunsetTime - tSunsetLenght)) {
    return LED_ON;
  } else {
    return LED_OFF;
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

String getDateNow() {
  return leadingZero(String(rtClock.dayofmonth))
         + "/"
         + leadingZero(String(rtClock.month))
         + "/"
         + leadingZero(String(rtClock.year));
}

String getTimeNow() {
  return leadingZero(String(int(rtClock.hours)))
         + ":"
         + leadingZero(String(int(rtClock.minutes)))
         + ":"
         + leadingZero(String(rtClock.seconds));
}

int getFromEeprom(int tStartByte, int tSizeByte) {
  int result = 0;
  eeprom_read_block(&result, tStartByte, tSizeByte);
  return result;
}

int convertToMyTime(int h, int m) {
  return h * 10000 + m * 100;  // np 132500 to godzina 13:25:00
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

void showTimeLengthSettingScreen(int tMin) {
  nokia.clrScr();
  nokia.setFont(BigNumbers);
  nokia.print(leadingZero(String(tMin)), 49, 20);
  nokia.setFont(TinyFont);
  nokia.invertText(true);
  nokia.drawLine(0, 0, 84, 0);
  nokia.print(" UP  - add minute     ", 0, 1);
  nokia.drawLine(0, 8, 84, 8);
  nokia.print(" DOWN - subtract      ", 0, 9);
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
  if (ledMode == LED_OFF) {
    nokia.invertText(true);
    nokia.print("     ", 30, 15);
    nokia.print("     ", 30, 29);
    nokia.print(" OFF ", 30, 22);
    nokia.invertText(false);
    nokia.print("    ", 60, 15);
    nokia.print("    ", 60, 29);
    nokia.print(" ON ", 60, 22);
  } else {
    nokia.print("     ", 30, 15);
    nokia.print("     ", 30, 29);
    nokia.print(" OFF ", 30, 22);
    nokia.invertText(true);
    nokia.print("    ", 60, 15);
    nokia.print("    ", 60, 29);
    nokia.print(" ON ", 60, 22);
    nokia.invertText(false);
  }
  nokia.update();
}

void drawLedModeRectangle() {
  switch (ledMode) {
    case LED_ON:
      nokia.clrRect(0, 0, 6, 8);
      nokia.drawRect(0, 0, 6, 8);
      break;
    case LED_MIX:
      nokia.clrRect(0, 0, 6, 8);
      nokia.drawRect(0, 0, 6, 8);
      nokia.drawLine(2, 2, 3, 2);
      nokia.drawLine(4, 2, 5, 2);
      nokia.drawLine(2, 4, 3, 4);
      nokia.drawLine(4, 4, 5, 4);
      nokia.drawLine(2, 6, 3, 6);
      nokia.drawLine(4, 6, 5, 6);
      break;
    case LED_OFF:
      nokia.clrRect(0, 0, 6, 8);
      nokia.drawRect(0, 0, 6, 8);
      nokia.drawRect(1, 1, 5, 7);
      nokia.drawRect(2, 2, 4, 6);
      nokia.drawRect(3, 3, 3, 5);
      break;
    default:
      nokia.clrRect(0, 0, 6, 8);
  }
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