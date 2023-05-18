#include <LCD5110_Graph.h>
#include <ClickEncoder.h>
#include <TimerOne.h>
#include <virtuabotixRTC.h>
#include <avr/eeprom.h>
#include <OneWire.h>
#include <DallasTemperature.h>


LCD5110 disp(8, 9, 10, 12, 11);
extern uint8_t SmallFont[];
extern unsigned char TinyFont[];
extern uint8_t BigNumbers[];

ClickEncoder encoder(4, 5, 7, 4);
int16_t oldEncPos, encPos;
uint8_t buttonState;

virtuabotixRTC rtClock(A0, 2, 3);

OneWire oneWire(A5); // termometr
DallasTemperature tempSensor(&oneWire);

int lightDelay = 100;
int tLightDelay = 0;
int lightDelayImpulse = 20;
int tLightDelayImpulse = 0;
int lightDelayImpulseSecond = 0;

int menu = 0;
String menuOne[] = {"[..]", "Blue sunrise", "Mix sunrise", "Sunrise", "Mix sunset", "Blue sunset", "Sunset", "Set clock", "Temp. alarm", "Light on/off"};
int menuOnePos = 0;

int hh = 0;
int mm = 0;

float temp = 0;
char temperature[6];
float tempAlarm = 0.0;
char temperatureAlarm[6];

//adresacja pamieci eeprom (nieulotnej): nazwa/początkowy bajt/ilosc zajmowanych bajtow
//blueSunriseHh 0,2    mixSunriseHh 4,2    sunriseHh 8,2    mixSunsetHh 12,2   blueSunsetHh 16,2    sunsetHh 20,2
//blueSunriseMm 2,2    mixSunriseMn 6,2    sunriseMm 10,2   mixSunsetMm 14,2   blueSunsetMm 18,2    sunsetMm 22,2
//tempAlarm 24,4

int lightMode = 0;
int tempLightModeManual = 0;
//int oldLightMode = 0;
int relay1 = 13;
int relayHh = 0;
int relayMm = 0;
boolean relayFactor;

void setup() {
  //Serial.begin(9600);

  Timer1.initialize(1000);
  Timer1.attachInterrupt(timerIsr);
  encoder.setAccelerationEnabled(true);
  oldEncPos = -1;

  disp.InitLCD();
  disp.setFont(SmallFont);
  analogWrite(6, 255); // podświetlenie
  disp.clrScr();

  eeprom_read_block(&tempAlarm, 24, 4);
  if (isnan(tempAlarm)) {
    tempAlarm = 30.0;
  }

  tempSensor.begin();
  tempSensor.setWaitForConversion(false);

  //                      seconds, minutes, hours, day of the week, day of the month, month, year
  //rtClock.setDS1302Time(      0,        32,    12,               1,               21,     2, 2022);


  pinMode(relay1, OUTPUT);
  digitalWrite(relay1, HIGH);
  relayFactor = true;

  tLightDelayImpulse = lightDelayImpulse;
}

void loop() {
  rtClock.updateTime();

  setTemperature();

  lightOnOff();

  if (tLightDelayImpulse < lightDelayImpulse && tLightDelayImpulse > 0) {
    tLightDelayImpulse--;
    if (lightDelayImpulseSecond > 1) {
      tLightDelay = lightDelay;
    }
  } else {
    tLightDelayImpulse = lightDelayImpulse;
    lightDelayImpulseSecond = 0;
  }
  //Serial.println("tLightDelayImpulse=" + String(tLightDelay) + ", tLightDelayImpulse=" + String(tLightDelayImpulse) + ", lightDelayImpulseSecond=" + String(lightDelayImpulseSecond));

  if (menu == 0) {
    showTempAndClock();
  } else if (menu == 1) {
    showMenuOne(menuOnePos);
  } else if (menu >= 11 && menu <= 17) {
    showTimeSetting();
  } else if (menu == 18) {
    showTempAlarmSetting();
  } else if (menu == 19) {
    showInvalidTimeAlert();
  } else if (menu == 20) {
    showManualOnOff();
  }

  encPos += encoder.getValue();
  if (encPos != oldEncPos) {
    setLightDelayImpulse();

    if (menu == 1) {
      if (oldEncPos > encPos) {
        menuOnePos--;
        if (menuOnePos < 0) menuOnePos = 0;
      } else {
        menuOnePos++;
        if (menuOnePos > 9) menuOnePos = 9;
      }
      showMenuOne(menuOnePos);
    } else if (menu >= 11 && menu <= 17) {
      setHhMmByEncPos();
    } else if (menu == 18) {
      setTempAlarmByEncPos();
    } else if (menu == 20) {
      setTempLightModeManualByEncPos();
    }

    oldEncPos = encPos;
  }

  buttonState = encoder.getButton();
  if (buttonState != 0) {
    setLightDelayImpulse();

    if (menu == 0) {
      menu = 1;
      menuOnePos = 0;
    } else if (menu == 1) {
      if (menuOnePos == 0) {
        menu = 0;
      } else if (menuOnePos >= 1 && menuOnePos <= 6) {
        setMenuAndSetHhMmFromEeprom((10 + menuOnePos), ((menuOnePos - 1) * 4), (menuOnePos * 4 - 2));
      } else if (menuOnePos == 7) {
        menu = 17;
        hh = int(rtClock.hours);
        mm = int(rtClock.minutes);
      } else if (menuOnePos == 8) {
        menu = 18;
      } else if (menuOnePos == 9) {
        menu = 20;
        //oldLightMode = lightMode;
        if (lightMode == 4) {
          tempLightModeManual = 1;
        } else if (lightMode > 0 && lightMode < 4) {
          tempLightModeManual = 2;
        } else {
          tempLightModeManual = 0;
        }
      }
    } else if (menu >= 11 && menu <= 16) {
      backToMenuOneWithPosAndSetEepromFromHhMm((menu - 10), ((menuOnePos - 1) * 4), (menuOnePos * 4 - 2));
    } else if (menu == 17) {
      rtClock.setDS1302Time(0, mm, hh, int(rtClock.dayofweek), int(rtClock.dayofmonth), int(rtClock.month), int(rtClock.year));
      backToMenuOne(7);
    } else if (menu == 18) {
      eeprom_write_block(&tempAlarm, 24, 4);
      backToMenuOne(8);
    } else if (menu == 19) {
      backToMenuOne(menuOnePos);
    } else if (menu == 20) {
      if (tempLightModeManual == 0) {
        //lightMode = oldLightMode;
        backToMenuOne(9);
      } else if (tempLightModeManual == 1) {
        digitalWrite(relay1, HIGH);
        lightMode = 5;
      } else if (tempLightModeManual == 2) {
        digitalWrite(relay1, LOW);
        lightMode = 5;
      }

      //backToMenuOne(9);
    }
  }

  manageRelay();

  //Serial.println("lightMode=" + String(lightMode) + ", tempLightModeManual=" + String(tempLightModeManual));
}

void lightOnOff() {
  if (tLightDelay > 0 || temp >= tempAlarm) {
    analogWrite(6, 50);
  } else {
    analogWrite(6, 255);
    tLightDelay = 0;
  }
  tLightDelay--;
}

void setLightDelayImpulse() {
  tLightDelayImpulse--;
  if (tLightDelayImpulse < lightDelayImpulse) {
    lightDelayImpulseSecond++;
  }
}

void setRelayStateAndFactor(boolean value) {
  digitalWrite(relay1, value);
  relayFactor = false;
}

void setRelayHhMmFromEeprom(int tHh, int tMm) {
  eeprom_read_block(&relayHh, tHh, 2);
  eeprom_read_block(&relayMm, tMm, 2);
}

void timerIsr() {
  encoder.service();
}

void showMenuOne(int invLine) {
  disp.clrScr();
  disp.setFont(SmallFont);
  for (int i = 0; i < 10; i++) {
    if (invLine == i) disp.invertText(true);
    if (invLine > 7) {
      disp.print(menuOne[i], LEFT, (i - 6) * 9);
    } else if (invLine > 5) {
      disp.print(menuOne[i], LEFT, (i - 4) * 9);
    } else if (invLine > 3) {
      disp.print(menuOne[i], LEFT, (i - 2) * 9);
    } else {
      disp.print(menuOne[i], LEFT, i * 9);
    }
    disp.invertText(false);
  }
  disp.update();
}

void showTempAndClock() {
  disp.clrScr();
  disp.setFont(TinyFont);
  disp.print(leadZero(String(rtClock.dayofmonth)) + "/" + leadZero(String(rtClock.month)) + "/" + leadZero(String(rtClock.year)), 9, 2);
  disp.drawLine(0, 8, 83, 8);
  disp.drawLine(0, 0, 83, 0);
  hh = int(rtClock.hours);
  mm = int(rtClock.minutes);
  disp.print(leadZero(String(hh)) + ":" + leadZero(String(mm)) + ":" + leadZero(String(rtClock.seconds)), RIGHT, 2);
  disp.setFont(BigNumbers);
  disp.print(temperature, 9, 15);
  disp.setFont(SmallFont);
  disp.print("o", 66, 14);
  disp.print("C", 73, 16);
  disp.drawLine(0, 47, 84, 47);
  drawLightModeRect();
  disp.update();
}

void showTimeSetting() {
  disp.clrScr();
  disp.setFont(BigNumbers);
  disp.print(leadZero(String(hh)), 7, 20);
  disp.print(".", CENTER, 14);
  disp.print(".", CENTER, 8);
  disp.print(leadZero(String(mm)), 49, 20);
  disp.setFont(TinyFont);
  disp.invertText(true);
  disp.drawLine(0, 0, 84, 0);
  disp.print(" left  - set hours    ", 0, 1);
  disp.drawLine(0, 8, 84, 8);
  disp.print(" right - set minutes  ", 0, 9);
  disp.invertText(false);
  disp.setFont(SmallFont);
  disp.update();
}

void showTempAlarmSetting() {
  dtostrf(tempAlarm, 3, 1, temperatureAlarm);

  disp.clrScr();
  disp.setFont(BigNumbers);
  disp.print(temperatureAlarm, 9, 20);
  disp.setFont(SmallFont);
  disp.print("o", 66, 19);
  disp.print("C", 73, 21);
  disp.setFont(TinyFont);
  disp.invertText(true);
  disp.drawLine(0, 0, 84, 0);
  disp.print(" left  - lower temp.  ", 0, 1);
  disp.drawLine(0, 8, 84, 8);
  disp.print(" right - higher temp. ", 0, 9);
  disp.invertText(false);
  disp.setFont(SmallFont);
  disp.update();
}

void showInvalidTimeAlert() {
  disp.clrScr();
  disp.setFont(TinyFont);
  disp.print("The time must be", CENTER, 4);
  disp.print("greater than", CENTER, 12);
  disp.print("the previous step", CENTER, 20);
  disp.print("time and less than", CENTER, 28);
  disp.print("the next step time!", CENTER, 36);
  disp.drawRect(0, 0, 83, 47);
  disp.update();
}

void showManualOnOff() {
  disp.clrScr();

  disp.setFont(TinyFont);
  disp.invertText(true);
  disp.drawLine(0, 0, 84, 0);
  disp.print("  turn left or right  ", 0, 1);
  disp.invertText(false);

  disp.setFont(SmallFont);

  if (tempLightModeManual == 0) {
    disp.invertText(true);
    disp.print("     ", 0, 15);
    disp.print("     ", 0, 29);
    disp.print(" <-  ", 0, 22);
    disp.invertText(false);
    disp.print("     ", 30, 15);
    disp.print("     ", 30, 29);
    disp.print(" OFF ", 30, 22);
    disp.print("    ", 60, 15);
    disp.print("    ", 60, 29);
    disp.print(" ON ", 60, 22);
  } else if (tempLightModeManual == 1) {
    disp.print("     ", 0, 15);
    disp.print("     ", 0, 29);
    disp.print(" <-  ", 0, 22);
    disp.invertText(true);
    disp.print("     ", 30, 15);
    disp.print("     ", 30, 29);
    disp.print(" OFF ", 30, 22);
    disp.invertText(false);
    disp.print("    ", 60, 15);
    disp.print("    ", 60, 29);
    disp.print(" ON ", 60, 22);
  } else if (tempLightModeManual == 2) {
    disp.print("     ", 0, 15);
    disp.print("     ", 0, 29);
    disp.print(" <-  ", 0, 22);
    disp.print("     ", 30, 15);
    disp.print("     ", 30, 29);
    disp.print(" OFF ", 30, 22);
    disp.invertText(true);
    disp.print("    ", 60, 15);
    disp.print("    ", 60, 29);
    disp.print(" ON ", 60, 22);
    disp.invertText(false);
  }

  disp.update();
}

void setTempLightModeManualByEncPos() {
  if (oldEncPos > encPos) {
    tempLightModeManual--;
    if (tempLightModeManual < 0) tempLightModeManual = 0;
  } else {
    tempLightModeManual++;
    if (tempLightModeManual > 2) tempLightModeManual = 2;
  }
}

void setTemperature() {
  if (tempSensor.isConversionComplete()) {
    tempSensor.requestTemperatures();
    temp = tempSensor.getTempCByIndex(0);
    dtostrf(temp, 3, 1, temperature);
  }
}

void setTempAlarmByEncPos() {
  if (oldEncPos > encPos) {
    tempAlarm = tempAlarm - 0.1;
    if (tempAlarm <= 0.0) tempAlarm = 0.0;
  } else {
    tempAlarm = tempAlarm + 0.1;
    if (tempAlarm >= 99.9) tempAlarm = 99.9;
  }
}

void setHhMmByEncPos() {
  if (oldEncPos > encPos) {
    hh++;
    if (hh > 23) hh = 0;
  } else {
    mm++;
    if (mm > 59) mm = 0;
  }
}

void backToMenuOne(int backToMenuOnePos) {
  menu = 1;
  menuOnePos = backToMenuOnePos;
  disp.invert(false);
}

void setMenuAndSetHhMmFromEeprom(int menuNum, int hhEepromFirstByte, int mmEepromFirstByte) {
  menu = menuNum;
  eeprom_read_block(&hh, hhEepromFirstByte, 2);
  eeprom_read_block(&mm, mmEepromFirstByte, 2);
}

void backToMenuOneWithPosAndSetEepromFromHhMm(int menuOnePosNum, int hhEepromFirstByte, int mmEepromFirstByte) {
  int prevHh = 0;
  int prevMm = -1;
  int nextHh = 23;
  int nextMm = 60;
  if (hhEepromFirstByte >= 4) {
    eeprom_read_block(&prevHh, hhEepromFirstByte - 4, 2);
    eeprom_read_block(&prevMm, mmEepromFirstByte - 4, 2);
  }
  if (hhEepromFirstByte <= 16) {
    eeprom_read_block(&nextHh, hhEepromFirstByte + 4, 2);
    eeprom_read_block(&nextMm, mmEepromFirstByte + 4, 2);
  }

  int prevTime = prevHh * 60 + prevMm;
  int nextTime = nextHh * 60 + nextMm;
  int nowTime = hh * 60 + mm;

  if (nowTime > prevTime && nowTime < nextTime) {
    eeprom_write_block(&hh, hhEepromFirstByte, 2);
    eeprom_write_block(&mm, mmEepromFirstByte, 2);
    backToMenuOne(menuOnePosNum);
  } else {
    menu = 19;
  }
}

void drawLightModeRect() {
  if (lightMode == 1) {           //white
    disp.clrRect(0, 0, 6, 8);
    disp.drawRect(0, 0, 6, 8);
  } else if (lightMode == 2) {    //mix
    disp.clrRect(0, 0, 6, 8);
    disp.drawRect(0, 0, 6, 8);
    disp.drawLine(2, 2, 3, 2);
    disp.drawLine(4, 2, 5, 2);
    disp.drawLine(2, 4, 3, 4);
    disp.drawLine(4, 4, 5, 4);
    disp.drawLine(2, 6, 3, 6);
    disp.drawLine(4, 6, 5, 6);
  } else if (lightMode == 3) {    //blue
    disp.clrRect(0, 0, 6, 8);
    disp.drawRect(0, 0, 6, 8);
    disp.drawLine(0, 2, 6, 2);
    disp.drawLine(0, 4, 6, 4);
    disp.drawLine(0, 6, 6, 6);
    disp.drawLine(2, 0, 2, 8);
    disp.drawLine(4, 0, 4, 8);
    disp.drawLine(6, 0, 6, 8);
  } else if (lightMode == 4) {    //black
    disp.clrRect(0, 0, 6, 8);
    disp.drawRect(0, 0, 6, 8);
    disp.drawRect(1, 1, 5, 7);
    disp.drawRect(2, 2, 4, 6);
    disp.drawRect(3, 3, 3, 5);
  } else if (lightMode == 5) {    //manual
    disp.clrRect(0, 0, 6, 8);
    disp.setFont(SmallFont);
    disp.print("M", 0, 1);
  } else {
    disp.clrRect(0, 0, 6, 8);
  }
}

String leadZero(String s) {
  if (s.length() < 2) {
    return "0" + s;
  }
  return s;
}

void displaySleep() {
  analogWrite(6, 255);
  disp.enableSleep();
}

void displayWakeup() {
  disp.disableSleep();
}

void manageRelay() {
  int sec = rtClock.seconds;
  setRelayHhMmFromEeprom(0, 2); //blueSunrise
  if (rtClock.hours == relayHh && rtClock.minutes == relayMm) {
    if (sec == 0 && relayFactor) {
      displaySleep();
      setRelayStateAndFactor(HIGH);
    } else if (sec == 2 && relayFactor) {
      setRelayStateAndFactor(LOW); //white
    } else if (sec == 4 && relayFactor) {
      setRelayStateAndFactor(HIGH);
    } else if (sec == 6 && relayFactor) {
      setRelayStateAndFactor(LOW); //mix
    } else if (sec == 8 && relayFactor) {
      setRelayStateAndFactor(HIGH);
    } else if (sec == 10 && relayFactor) {
      setRelayStateAndFactor(LOW); //blue
      lightMode = 3;
      displayWakeup();
    } else if (sec == 1 || sec == 3 || sec == 5 || sec == 7 || sec == 9 || sec == 11) {
      relayFactor = true;
    }
  }
  setRelayHhMmFromEeprom(4, 6); //mixSunrise
  if (rtClock.hours == relayHh && rtClock.minutes == relayMm) {
    if (sec == 0 && relayFactor) {
      displaySleep();
      setRelayStateAndFactor(HIGH); //blue off
    } else if (sec == 2 && relayFactor) {
      setRelayStateAndFactor(LOW); //white
    } else if (sec == 4 && relayFactor) {
      setRelayStateAndFactor(HIGH);
    } else if (sec == 6 && relayFactor) {
      setRelayStateAndFactor(LOW); //mix
      lightMode = 2;
      displayWakeup();
    } else if (sec == 1 || sec == 3 || sec == 5 || sec == 7) {
      relayFactor = true;
    }
  }
  setRelayHhMmFromEeprom(8, 10); //sunrise
  if (rtClock.hours == relayHh && rtClock.minutes == relayMm) {
    if (sec == 0 && relayFactor) {
      displaySleep();
      setRelayStateAndFactor(HIGH); //mix off
    } else if (sec == 2 && relayFactor) {
      setRelayStateAndFactor(LOW); //blue
    } else if (sec == 4 && relayFactor) {
      setRelayStateAndFactor(HIGH);
    } else if (sec == 6 && relayFactor) {
      setRelayStateAndFactor(LOW); //white
      lightMode = 1;
      displayWakeup();
    } else if (sec == 1 || sec == 3 || sec == 5 || sec == 7) {
      relayFactor = true;
    }
  }
  setRelayHhMmFromEeprom(12, 14); //mixSunset
  if (rtClock.hours == relayHh && rtClock.minutes == relayMm) {
    if (sec == 0 && relayFactor) {
      displaySleep();
      setRelayStateAndFactor(HIGH); //white off
    } else if (sec == 2 && relayFactor) {
      setRelayStateAndFactor(LOW); //mix
      lightMode = 2;
      displayWakeup();
    } else if (sec == 1 || sec == 3) {
      relayFactor = true;
    }
  }
  setRelayHhMmFromEeprom(16, 18); //blueSunset
  if (rtClock.hours == relayHh && rtClock.minutes == relayMm) {
    if (sec == 0 && relayFactor) {
      displaySleep();
      setRelayStateAndFactor(HIGH); //mix off
    } else if (sec == 2 && relayFactor) {
      setRelayStateAndFactor(LOW); //blue
      lightMode = 3;
      displayWakeup();
    } else if (sec == 1 || sec == 3) {
      relayFactor = true;
    }
  }
  setRelayHhMmFromEeprom(20, 22); //sunset
  if (rtClock.hours == relayHh && rtClock.minutes == relayMm) {
    if (sec == 0 && relayFactor) {
      displaySleep();
      setRelayStateAndFactor(HIGH); //blue off
      lightMode = 4;
      displayWakeup();
    } else if (sec == 1) {
      relayFactor = true;
    }
  }
}
