#include <Adafruit_GFX.h>
#include <Adafruit_PCD8544.h>
#include <TimerOne.h>
#include <virtuabotixRTC.h>
#include <avr/eeprom.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Bounce2.h>

//------------------------------------- piny -------------------------------------
#define NOKIA_BACKGROUND_PWM 6
#define NOKIA_RST 7
#define NOKIA_CE 8
#define NOKIA_DC 9
#define NOKIA_DIN 10
#define NOKIA_CLK 11

//biały - masa, fioletowy - up, niebieski - down (w stronę kółka), zielony - klik
#define BUTTON_UP_PIN 2
#define BUTTON_DOWN_PIN 3
#define BUTTON_CLICK_PIN 4

#define RTC_CLK A0
#define RTC_DAT A1
#define RTC_RST A2

#define TEMPERATURE_SENSOR A5

#define PWM_LED 5
//------------------------------------- nokia -------------------------------------
Adafruit_PCD8544 nokia = Adafruit_PCD8544(NOKIA_CLK, NOKIA_DIN, NOKIA_DC, NOKIA_CE, NOKIA_RST);

int nokiaLightDelayTemplate = 50;
int nokiaLightDelay;
//------------------------------------- buttons -----------------------------------
Bounce buttonUpDebouncer = Bounce();
Bounce buttonDownDebouncer = Bounce();
Bounce buttonClickDebouncer = Bounce();
//------------------------------------- rtc ---------------------------------------
virtuabotixRTC rtClock(RTC_CLK, RTC_DAT, RTC_RST);

int tempHh;
int tempMm;

unsigned long currentTime;
unsigned long currentTimeOld;
const unsigned int timeDelta = 10;

unsigned long tSunriseStartTime;
unsigned long tSunriseEndTime;
unsigned long tSunsetStartTime;
unsigned long tSunsetEndTime;
//------------------------------------- termometr ---------------------------------
OneWire oneWire(TEMPERATURE_SENSOR);
DallasTemperature temperatureSensor(&oneWire);

float temperatureAlarmTemplate;
float temperatureFromSensor;
//------------------------------------- led ---------------------------------------
enum LedModes {
  LED_OFF,
  LED_SUNRISE,
  LED_ON,
  LED_SUNSET
};

#define LED_MODE_NAME(ledModeName) #ledModeName

const char* ledModeName[] = {
  LED_MODE_NAME(LED_OFF),
  LED_MODE_NAME(LED_SUNRISE),
  LED_MODE_NAME(LED_ON),
  LED_MODE_NAME(LED_SUNSET)
};

const char* ledModeStrings[] = {
  "Night",
  "Sunrise",
  "Day",
  "Sunset"
};

enum LedModes ledMode;
int pwmValue;

int sunriseStartHh;
int sunriseStartMm;
int sunriseEndHh;
int sunriseEndMm;

int sunsetStartHh;
int sunsetStartMm;
int sunsetEndHh;
int sunsetEndMm;
//------------------------------------- flagi -------------------------------------
bool ignoreTimeForLed;
bool ignoreTimeForSetTime;
//------------------------------------- menu --------------------------------------
//-- wyświetlanie Serial.println(menuStrings[0]);
enum Menus {
  MAIN_SCREEN,
  INVALID_TIME_SCREEN,
  MENU_SCREEN,
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
  IDNAME(MAIN_SCREEN),
  IDNAME(INVALID_TIME_SCREEN),
  IDNAME(MENU_SCREEN),
  IDNAME(TIME_SUNRISE_START),
  IDNAME(TIME_SUNRISE_END),
  IDNAME(TIME_SUNSET_START),
  IDNAME(TIME_SUNSET_END),
  IDNAME(SET_CLOCK),
  IDNAME(SET_TEMP_ALARM),
  IDNAME(SET_LIGHT_ON_OFF)
};

const char* menuStrings[] = {
  "Main screen",
  "Invalid time",
  "[...]",
  "Sunrise start",
  "Sunrise end",
  "Sunset start",
  "Sunset end",
  "Set clock",
  "Temp. alarm",
  "LED on/off"
};

enum Menus menu;
int sizeOfMenus = 10;
enum Menus menuSelectedLine;
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

  pinMode(NOKIA_BACKGROUND_PWM, OUTPUT);

  nokia.begin();
  nokia.setContrast(50);
  nokia.clearDisplay();
  setNokiaLight(true);

  //                      seconds, minutes, hours, day of the week, day of the month, month, year
  //rtClock.setDS1302Time(      0,        32,    12,               1,               21,     2, 2022);

  // setValueToEeprom(eepromSunriseStartHh[0], eepromSunriseStartHh[1], 10);
  // setValueToEeprom(eepromSunriseStartMm[0], eepromSunriseStartMm[1], 0);
  // setValueToEeprom(eepromSunriseEndHh[0], eepromSunriseEndHh[1], 10);
  // setValueToEeprom(eepromSunriseEndMm[0], eepromSunriseEndMm[1], 15);
  // setValueToEeprom(eepromSunsetStartHh[0], eepromSunsetStartHh[1], 18);
  // setValueToEeprom(eepromSunsetStartMm[0], eepromSunsetStartMm[1], 45);
  // setValueToEeprom(eepromSunsetEndHh[0], eepromSunsetEndHh[1], 19);
  // setValueToEeprom(eepromSunsetEndMm[0], eepromSunsetEndMm[1], 0);

  sunriseStartHh = getValueFromEeprom(eepromSunriseStartHh[0], eepromSunriseStartHh[1], 10);
  sunriseStartMm = getValueFromEeprom(eepromSunriseStartMm[0], eepromSunriseStartMm[1], 0);
  sunriseEndHh = getValueFromEeprom(eepromSunriseEndHh[0], eepromSunriseEndHh[1], 10);
  sunriseEndMm = getValueFromEeprom(eepromSunriseEndMm[0], eepromSunriseEndMm[1], 15);
  sunsetStartHh = getValueFromEeprom(eepromSunsetStartHh[0], eepromSunsetStartHh[1], 18);
  sunsetStartMm = getValueFromEeprom(eepromSunsetStartMm[0], eepromSunsetStartMm[1], 45);
  sunsetEndHh = getValueFromEeprom(eepromSunsetEndHh[0], eepromSunsetEndHh[1], 19);
  sunsetEndMm = getValueFromEeprom(eepromSunsetEndMm[0], eepromSunsetEndMm[1], 0);
  temperatureAlarmTemplate = getValueFromEeprom(eepromTemperatureAlarmTemplate[0], eepromTemperatureAlarmTemplate[1], 30.0);

  // Serial.println("sunriseStart" + String(sunriseStartHh) + ":" + String(sunriseStartMm)
  //                + ", sunriseEnd" + String(sunriseEndHh) + ":" + String(sunriseEndMm)
  //                + ", sunsetStart" + String(sunsetStartHh) + ":" + String(sunsetStartMm)
  //                + ", sunsetEnd" + String(sunsetEndHh) + ":" + String(sunsetEndMm));

  tSunriseStartTime = convertTimeToSeconds(sunriseStartHh, sunriseStartMm, 0);
  tSunriseEndTime = convertTimeToSeconds(sunriseEndHh, sunriseEndMm, 0);
  tSunsetStartTime = convertTimeToSeconds(sunsetStartHh, sunsetStartMm, 0);
  tSunsetEndTime = convertTimeToSeconds(sunsetEndHh, sunsetEndMm, 0);

  currentTimeOld = convertTimeToSeconds(rtClock.hours, rtClock.minutes, rtClock.seconds);

  temperatureSensor.begin();
  temperatureSensor.setWaitForConversion(false);

  temperatureFromSensor = 33.0;

  pinMode(PWM_LED, OUTPUT);
  ignoreTimeForLed = false;
  ignoreTimeForSetTime = false;

  menu = MAIN_SCREEN;
  menuSelectedLine = MENU_SCREEN;

  nokiaLightDelay = nokiaLightDelayTemplate;

  // Inicjalizacja debouncerów przycisków
  buttonUpDebouncer.attach(BUTTON_UP_PIN, INPUT_PULLUP);
  buttonUpDebouncer.interval(10);

  buttonDownDebouncer.attach(BUTTON_DOWN_PIN, INPUT_PULLUP);
  buttonDownDebouncer.interval(10);

  buttonClickDebouncer.attach(BUTTON_CLICK_PIN, INPUT_PULLUP);
  buttonClickDebouncer.interval(10);
}
//---------------------------------------------------------------------------------
//------------------------------------ LOOP ---------------------------------------
//---------------------------------------------------------------------------------
void loop() {
  //update zegara z RTC
  updateAndSynchronizeTime();

  //Serial.println("ledMode=" + String(ledMode) + ", currentTime=" + String(currentTime));
  // Serial.println("ledMode=" + String(ledMode)
  //                + ", rtClock=" + String(rtClock.hours) + ":" + String(rtClock.minutes) + ":" + String(rtClock.seconds)
  //                + ", menu=" + String(menu));


  //ustaw ledMode w zależności od aktualnego czasu
  if (!ignoreTimeForLed) {
    setLedModeByTime();
  }

  //steruj głównym oświetleniem LED
  manageLed();

  //update debouncerów
  buttonUpDebouncer.update();
  buttonDownDebouncer.update();
  buttonClickDebouncer.update();

  //reakcje na przyciski w zależności od menu
  buttonsActionsDependOnMenu();

  //odczytaj temperaturę z czujnika
  //getTemperatureFromSensor();

  //steruj podświetleniem wyświetlacza
  manageNokiaLight();

  //pokaż właściwy ekran w zależności od menu
  showScreenDependOnMenu();
}

//------------------------------------ menu ---------------------------------------
void showScreenDependOnMenu() {
  switch (menu) {
    case MAIN_SCREEN:
      showMainScreen(getDateNowForScreen(), getTimeNowForScreen(), convertTemperatureToString(temperatureFromSensor));
      break;
    case INVALID_TIME_SCREEN:
      showInvalidTimeAlertScreen();
      break;
    case MENU_SCREEN:
      showMenuScreen();
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
      showTimeSettingScreen(tempHh, tempMm);
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

void buttonsActionsDependOnMenu() {
  switch (menu) {
    case MAIN_SCREEN:
    case INVALID_TIME_SCREEN:
      if (buttonUpDebouncer.fell() || buttonDownDebouncer.fell() || buttonClickDebouncer.fell()) {
        nokiaLightDelay = nokiaLightDelayTemplate;

        menu = MENU_SCREEN;
      }
      break;
    case MENU_SCREEN:
      if (buttonUpDebouncer.fell()) {
        nokiaLightDelay = nokiaLightDelayTemplate;

        menuSelectedLine = menuSelectedLine - 1;
        if (menuSelectedLine < MENU_SCREEN) menuSelectedLine = MENU_SCREEN;
      }
      if (buttonDownDebouncer.fell()) {
        nokiaLightDelay = nokiaLightDelayTemplate;

        menuSelectedLine = menuSelectedLine + 1;
        if (menuSelectedLine > sizeOfMenus - 1) menuSelectedLine = sizeOfMenus - 1;
      }
      if (buttonClickDebouncer.fell()) {
        nokiaLightDelay = nokiaLightDelayTemplate;

        Serial.println("menuSelectedLine=" + String(menuSelectedLine));

        menu = menuSelectedLine;
        if (menu == MENU_SCREEN) menu = MAIN_SCREEN;
      }
      break;
    case TIME_SUNRISE_START:
      if (buttonUpDebouncer.fell()) {
        nokiaLightDelay = nokiaLightDelayTemplate;

        timeIncremetation(&sunriseStartHh, 23);
      } else if (buttonDownDebouncer.fell()) {
        nokiaLightDelay = nokiaLightDelayTemplate;

        timeIncremetation(&sunriseStartMm, 59);
      } else if (buttonClickDebouncer.fell()) {
        nokiaLightDelay = nokiaLightDelayTemplate;

        tSunriseStartTime = convertTimeToSeconds(sunriseStartHh, sunriseStartMm, 0);

        if (tSunriseStartTime > tSunriseEndTime) {
          menu = INVALID_TIME_SCREEN;

          sunriseStartHh = getValueFromEeprom(eepromSunriseStartHh[0], eepromSunriseStartHh[1], 10);
          sunriseStartMm = getValueFromEeprom(eepromSunriseStartMm[0], eepromSunriseStartMm[1], 0);

          tSunriseStartTime = convertTimeToSeconds(sunriseStartHh, sunriseStartMm, 0);
        } else {
          setValueToEeprom(eepromSunriseStartHh[0], eepromSunriseStartHh[1], sunriseStartHh);
          setValueToEeprom(eepromSunriseStartMm[0], eepromSunriseStartMm[1], sunriseStartMm);

          tSunriseStartTime = convertTimeToSeconds(sunriseStartHh, sunriseStartMm, 0);

          menu = MENU_SCREEN;
        }
      }
      break;
    case TIME_SUNRISE_END:
      if (buttonUpDebouncer.fell()) {
        nokiaLightDelay = nokiaLightDelayTemplate;

        timeIncremetation(&sunriseEndHh, 23);
      } else if (buttonDownDebouncer.fell()) {
        nokiaLightDelay = nokiaLightDelayTemplate;

        timeIncremetation(&sunriseEndMm, 59);
      } else if (buttonClickDebouncer.fell()) {
        nokiaLightDelay = nokiaLightDelayTemplate;

        tSunriseEndTime = convertTimeToSeconds(sunriseEndHh, sunriseEndMm, 0);

        if (tSunriseEndTime > tSunsetStartTime) {
          menu = INVALID_TIME_SCREEN;

          sunriseEndHh = getValueFromEeprom(eepromSunriseEndHh[0], eepromSunriseEndHh[1], 10);
          sunriseEndMm = getValueFromEeprom(eepromSunriseEndMm[0], eepromSunriseEndMm[1], 15);

          tSunriseEndTime = convertTimeToSeconds(sunriseEndHh, sunriseEndMm, 0);
        } else {
          setValueToEeprom(eepromSunriseEndHh[0], eepromSunriseEndHh[1], sunriseEndHh);
          setValueToEeprom(eepromSunriseEndMm[0], eepromSunriseEndMm[1], sunriseEndMm);

          tSunriseEndTime = convertTimeToSeconds(sunriseEndHh, sunriseEndMm, 0);

          menu = MENU_SCREEN;
        }
      }
      break;
    case TIME_SUNSET_START:
      if (buttonUpDebouncer.fell()) {
        nokiaLightDelay = nokiaLightDelayTemplate;

        timeIncremetation(&sunsetStartHh, 23);
      } else if (buttonDownDebouncer.fell()) {
        nokiaLightDelay = nokiaLightDelayTemplate;

        timeIncremetation(&sunsetStartMm, 59);
      } else if (buttonClickDebouncer.fell()) {
        nokiaLightDelay = nokiaLightDelayTemplate;

        tSunsetStartTime = convertTimeToSeconds(sunsetStartHh, sunsetStartMm, 0);

        if (tSunsetStartTime > tSunsetEndTime) {
          menu = INVALID_TIME_SCREEN;

          sunsetStartHh = getValueFromEeprom(eepromSunsetStartHh[0], eepromSunsetStartHh[1], 18);
          sunsetStartMm = getValueFromEeprom(eepromSunsetStartMm[0], eepromSunsetStartMm[1], 45);

          tSunsetStartTime = convertTimeToSeconds(sunsetStartHh, sunsetStartMm, 0);
        } else {
          setValueToEeprom(eepromSunsetStartHh[0], eepromSunsetStartHh[1], sunsetStartHh);
          setValueToEeprom(eepromSunsetStartMm[0], eepromSunsetStartMm[1], sunsetStartMm);

          tSunsetStartTime = convertTimeToSeconds(sunsetStartHh, sunsetStartMm, 0);

          menu = MENU_SCREEN;
        }
      }
      break;
    case TIME_SUNSET_END:
      if (buttonUpDebouncer.fell()) {
        nokiaLightDelay = nokiaLightDelayTemplate;

        timeIncremetation(&sunsetEndHh, 23);
      } else if (buttonDownDebouncer.fell()) {
        nokiaLightDelay = nokiaLightDelayTemplate;

        timeIncremetation(&sunsetEndMm, 59);
      } else if (buttonClickDebouncer.fell()) {
        nokiaLightDelay = nokiaLightDelayTemplate;

        tSunsetEndTime = convertTimeToSeconds(sunsetEndHh, sunsetEndMm, 0);

        if (tSunsetEndTime < tSunsetStartTime) {
          menu = INVALID_TIME_SCREEN;

          sunsetEndHh = getValueFromEeprom(eepromSunsetEndHh[0], eepromSunsetEndHh[1], 19);
          sunsetEndMm = getValueFromEeprom(eepromSunsetEndMm[0], eepromSunsetEndMm[1], 0);

          tSunsetEndTime = convertTimeToSeconds(sunsetEndHh, sunsetEndMm, 0);
        } else {
          setValueToEeprom(eepromSunsetEndHh[0], eepromSunsetEndHh[1], sunsetEndHh);
          setValueToEeprom(eepromSunsetEndMm[0], eepromSunsetEndMm[1], sunsetEndMm);

          tSunsetEndTime = convertTimeToSeconds(sunsetEndHh, sunsetEndMm, 0);

          menu = MENU_SCREEN;
        }
      }
      break;
    case SET_CLOCK:
      if (!ignoreTimeForSetTime) {
        tempHh = rtClock.hours;
        tempMm = rtClock.minutes;
      }

      if (buttonUpDebouncer.fell()) {
        nokiaLightDelay = nokiaLightDelayTemplate;

        ignoreTimeForSetTime = true;

        timeIncremetation(&tempHh, 23);
      } else if (buttonDownDebouncer.fell()) {
        nokiaLightDelay = nokiaLightDelayTemplate;

        ignoreTimeForSetTime = true;

        timeIncremetation(&tempMm, 59);
      } else if (buttonClickDebouncer.fell()) {
        nokiaLightDelay = nokiaLightDelayTemplate;

        if (tempMm != rtClock.minutes || tempHh != rtClock.hours) {
          rtClock.setDS1302Time(0, tempMm, tempHh, int(rtClock.dayofweek), int(rtClock.dayofmonth), int(rtClock.month), int(rtClock.year));
        }

        ignoreTimeForSetTime = false;

        menu = MENU_SCREEN;
      }
      break;
    case SET_TEMP_ALARM:
      if (buttonUpDebouncer.fell()) {
        nokiaLightDelay = nokiaLightDelayTemplate;

        temperatureAlarmTemplate = temperatureAlarmTemplate + 0.1;
        if (temperatureAlarmTemplate > 99.9) temperatureAlarmTemplate = 99.9;
      }
      if (buttonDownDebouncer.fell()) {
        nokiaLightDelay = nokiaLightDelayTemplate;

        temperatureAlarmTemplate = temperatureAlarmTemplate - 0.1;
        if (temperatureAlarmTemplate < 0.0) temperatureAlarmTemplate = 0.0;
      }
      if (buttonClickDebouncer.fell()) {
        nokiaLightDelay = nokiaLightDelayTemplate;

        setValueToEeprom(eepromTemperatureAlarmTemplate[0], eepromTemperatureAlarmTemplate[1], temperatureAlarmTemplate);

        menu = MENU_SCREEN;
      }
      break;
    case SET_LIGHT_ON_OFF:
      if (buttonUpDebouncer.fell()) {
        nokiaLightDelay = nokiaLightDelayTemplate;

        ignoreTimeForLed = true;

        ledMode = LED_ON;
      }
      if (buttonDownDebouncer.fell()) {
        nokiaLightDelay = nokiaLightDelayTemplate;

        ignoreTimeForLed = true;

        ledMode = LED_OFF;
      }
      if (buttonClickDebouncer.fell()) {
        nokiaLightDelay = nokiaLightDelayTemplate;

        ignoreTimeForLed = false;

        menu = MENU_SCREEN;
      }
      break;
  }
}
//------------------------------------ LED ----------------------------------------
void setLedModeByTime() {
  if (tSunriseStartTime <= currentTime && currentTime <= tSunriseEndTime) {
    ledMode = LED_SUNRISE;
  } else if (tSunriseEndTime <= currentTime && currentTime <= tSunsetStartTime) {
    ledMode = LED_ON;
  } else if (tSunsetStartTime <= currentTime && currentTime <= tSunsetEndTime) {
    ledMode = LED_SUNSET;
  } else {
    ledMode = LED_OFF;
  }
}

void manageLed() {
  switch (ledMode) {
    case LED_SUNRISE:
      analogWrite(PWM_LED, smoothPWMIncrease(tSunriseStartTime, tSunriseEndTime, true, 255));
      break;
    case LED_ON:
      analogWrite(PWM_LED, 255);
      break;
    case LED_SUNSET:
      analogWrite(PWM_LED, smoothPWMIncrease(tSunsetStartTime, tSunsetEndTime, false, 255));
      break;
    case LED_OFF:
      analogWrite(PWM_LED, 0);
      break;
  }
}

int smoothPWMIncrease(unsigned long animationStart, unsigned long animationEnd, bool increase, int steps) {
  if (currentTime >= animationStart && currentTime <= animationEnd) {
    unsigned long elapsedTime = currentTime - animationStart;

    pwmValue = map(elapsedTime, 0, animationEnd - animationStart, 0, steps);

    if (!increase) {
      pwmValue = steps - pwmValue;  // Jeżeli malejemy, odwróć wartości
    }

    //Serial.println("pwmValue=" + String(pwmValue));

    return pwmValue;
  }
}
//------------------------------------ NOKIA --------------------------------------
void setNokiaLight(bool state) {
  if (state) {
    analogWrite(NOKIA_BACKGROUND_PWM, 100);  // nokia świeci
  } else {
    analogWrite(NOKIA_BACKGROUND_PWM, 255);  // nokia nie świeci
  }
}

void manageNokiaLight() {
  //if (nokiaLightDelay > 0 || temperatureFromSensor >= temperatureAlarmTemplate) {
  if (nokiaLightDelay > 0) {
    setNokiaLight(true);
  } else {
    setNokiaLight(false);
    nokiaLightDelay = 0;
  }
  nokiaLightDelay--;
}
//------------------------------------ TEMPERATURA --------------------------------
float getTemperatureFromSensor() {
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

unsigned long convertTimeToSeconds(int hours, int minutes, int seconds) {
  return hours * 3600UL + minutes * 60UL + seconds;
}

void updateAndSynchronizeTime() {
  bool isSynchronized = false;
  do {
    rtClock.updateTime();
    currentTime = convertTimeToSeconds(rtClock.hours, rtClock.minutes, rtClock.seconds);

    //Serial.print("currentTime=" + String(currentTime) + ", currentTimeOld=" + currentTimeOld);

    if (currentTime > currentTimeOld && (currentTimeOld + timeDelta) > currentTime) {
      //Serial.println(", syncrhonized OK");

      isSynchronized = true;
    } else {
      //Serial.println(", syncrhonized ERROR !!!");

      delay(1050);

      currentTimeOld = convertTimeToSeconds(rtClock.hours, rtClock.minutes, rtClock.seconds);
    }
  } while (!isSynchronized);
}

void timeIncremetation(int* tTime, int maxValue) {
  *tTime += 1;
  if (*tTime > maxValue) *tTime = 0;
}
//------------------------------------ EEPROM -------------------------------------
template<typename T>
T getValueFromEeprom(int tStartByte, int tSizeByte, T defaultValue) {
  T result;
  eeprom_read_block(&result, tStartByte, tSizeByte);
  if (isnan(result)) result = defaultValue;
  return result;
}

template<typename T>
void setValueToEeprom(int tStartByte, int tSizeByte, T newValue) {
  eeprom_write_block(&newValue, tStartByte, tSizeByte);
}
//------------------------------------ SCREENS ------------------------------------
void showMenuScreen() {
  nokia.clearDisplay();
  nokia.setTextSize(1);
  nokia.setTextColor(BLACK);

  for (int i = MENU_SCREEN; i < sizeOfMenus; i++) {
    int k = 0;
    if (menuSelectedLine >= MENU_SCREEN + 4) {
      k = 30;
    }

    nokia.setCursor(0, i * 10 - MENU_SCREEN * 10 - k);
    nokia.print(menuStrings[i]);
    nokia.setCursor(78, i * 10 - MENU_SCREEN * 10 - k);
    nokia.write(124);  // wybór w menu - kreseczki

    if (menuSelectedLine == i) {
      nokia.setCursor(78, i * 10 - MENU_SCREEN * 10 - k);
      nokia.write(218);  // wybór w menu - prostokąt
    }
  }
  nokia.display();
}

void showMainScreen(String tDateNow, String tTimeNow, String tTemperature) {
  nokia.clearDisplay();
  nokia.setTextSize(1);
  nokia.setTextColor(BLACK);

  nokia.setCursor(19, 0);
  nokia.print(tTimeNow);

  nokia.setCursor(6, 13);
  nokia.setTextSize(3);
  nokia.print(tTemperature);

  nokia.setCursor(78, 11);
  nokia.setTextSize(1);
  nokia.print("*");

  nokia.setCursor(6, 40);
  String tLedModeString = ledModeStrings[ledMode];
  nokia.print(tLedModeString + " (" + String(pwmValue) + ")");

  nokia.display();
}

void showTimeSettingScreen(int tHh, int tMm) {
  nokia.clearDisplay();
  nokia.setTextColor(BLACK);

  nokia.setTextSize(1);
  nokia.setCursor(0, 0);
  nokia.print("UP - hours");
  nokia.setCursor(0, 8);
  nokia.print("DOWN - minutes");
  nokia.drawLine(0, 17, 84, 17, BLACK);

  nokia.setTextSize(3);

  nokia.setCursor(4, 22);
  nokia.print(leadingZero(String(tHh)));

  nokia.setCursor(35, 22);
  nokia.print(":");

  nokia.setCursor(48, 22);
  nokia.print(leadingZero(String(tMm)));

  nokia.display();
}

void showTemperatureAlarmTemplateSettingScreen(String tTemperatureAlarm) {
  nokia.clearDisplay();
  nokia.setTextSize(1);
  nokia.setTextColor(BLACK);

  nokia.setCursor(0, 0);
  nokia.print("Set temp.\nby UP and DOWN");
  nokia.drawLine(0, 17, 84, 17, BLACK);

  nokia.setCursor(6, 22);
  nokia.setTextSize(3);
  nokia.print(tTemperatureAlarm);

  nokia.setCursor(78, 20);
  nokia.setTextSize(1);
  nokia.print("*");
  
  nokia.display();
}

void showLedOnOffManualScreen() {
  nokia.clearDisplay();
  nokia.setTextSize(1);
  nokia.setTextColor(BLACK);

  nokia.setCursor(0, 0);
  nokia.print("UP leds ON");
  nokia.setCursor(0, 8);
  nokia.print("DOWN leds OFF");
  nokia.drawLine(0, 17, 84, 17, BLACK);

  nokia.setTextSize(2);
  nokia.setCursor(0, 20);
  nokia.print(ledModeStrings[ledMode]);

  nokia.display();
}

void showInvalidTimeAlertScreen() {
  nokia.clearDisplay();
  nokia.setTextSize(1);
  nokia.setTextColor(BLACK);

  nokia.setCursor(0, 0);
  nokia.print("The time must be greater\nthan the pre-\nvious step and\nless than\nthe next step!");

  nokia.display();
}