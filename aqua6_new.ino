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

Bounce buttonUpDebouncer = Bounce();
Bounce buttonDownDebouncer = Bounce();
Bounce buttonClickDebouncer = Bounce();
//------------------------------------- rtc ---------------------------------------
#define RTC_CLK 4
#define RTC_DAT 5
#define RTC_RST 6

virtuabotixRTC rtClock(RTC_CLK, RTC_DAT, RTC_RST);

int tempHh;
int tempMm;
//------------------------------------- termometr ---------------------------------
#define TEMPERATURE_SENSOR A5

OneWire oneWire(TEMPERATURE_SENSOR);
DallasTemperature temperatureSensor(&oneWire);

float temperatureAlarmTemplate;
float temperatureFromSensor;
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
//------------------------------------- menu --------------------------------------
//-- wyświetlanie Serial.println(menuStrings[0]);
enum Menu {
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
  "Main screen",
  "Invalid time"
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
int sizeOfMenu;
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

  temperatureFromSensor = 0.0;

  pinMode(PWM_LED, OUTPUT);

  menu = MENU_SCREEN;
  sizeOfMenu = sizeof(Menu) / sizeof(Menu[0]);
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
  rtClock.updateTime();

  //update debouncerów
  buttonUpDebouncer.update();
  buttonDownDebouncer.update();
  buttonClickDebouncer.update();

  //reakcje na przyciski w zależności od menu
  buttonsActionsDependOnMenu();

  //ustaw ledMode w zależności od aktualnego czasu
  setLedModeByTime();

  //odczytaj temperaturę z czujnika
  getTemperatureFromSensor();

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
    case MAIN_SCREEN:
      showMenuScreen();
      break;
    case INVALID_TIME_SCREEN:
      showInvalidTimeAlertScreen();
      break;
    case MENU_SCREEN:
      showMainScreen(
        getDateNowForScreen(),
        getTimeNowForScreen(),
        convertTemperatureToString(temperatureFromSensor));
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
      tempHh = int(rtClock.hours);
      tempMm = int(rtClock.minutes);
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

        menuSelectedLine--;
        if (menuSelectedLine < MENU_SCREEN) menuSelectedLine = MENU_SCREEN;
      }
      if (buttonDownDebouncer.fell()) {
        nokiaLightDelay = nokiaLightDelayTemplate;

        menuSelectedLine++;
        if (menuSelectedLine > sizeOfMenu - 1) sizeOfMenu - 1;
      }
      if (buttonClickDebouncer.fell()) {
        nokiaLightDelay = nokiaLightDelayTemplate;

        menu = menuSelectedLine;
      }
      break;
    case TIME_SUNRISE_START:
      unsigned long tSunriseStartTime = convertTimeToMillis(sunriseStartHh, sunriseStartMm, 0);
      unsigned long tSunriseEndTime = convertTimeToMillis(sunriseEndHh, sunriseEndMm, 0);
      unsigned long tSunsetStartTime = convertTimeToMillis(sunsetStartHh, sunsetStartMm, 0);
      unsigned long tSunsetEndTime = convertTimeToMillis(sunsetEndHh, sunsetEndMm, 0);

      if (buttonUpDebouncer.fell()) {
        nokiaLightDelay = nokiaLightDelayTemplate;

        timeIncremetation(&sunriseStartHh, 23);
      }
      if (buttonDownDebouncer.fell()) {
        nokiaLightDelay = nokiaLightDelayTemplate;

        timeIncremetation(&sunriseStartMm, 59);
      }
      if (buttonClickDebouncer.fell()) {
        nokiaLightDelay = nokiaLightDelayTemplate;

        if (tSunriseStartTime <= tSunriseEndTime) {
          menu = INVALID_TIME_SCREEN;
        } else {
          setValueToEeprom(eepromSunriseStartHh[0], eepromSunriseStartHh[1], sunriseStartHh);
          setValueToEeprom(eepromSunriseStartMm[0], eepromSunriseStartMm[1], sunriseStartMm);

          menu = MENU_SCREEN;
        }
      }
      break;
    case TIME_SUNRISE_END:
      if (buttonUpDebouncer.fell()) {
        nokiaLightDelay = nokiaLightDelayTemplate;

        timeIncremetation(&sunriseEndHh, 23);
      }
      if (buttonDownDebouncer.fell()) {
        nokiaLightDelay = nokiaLightDelayTemplate;

        timeIncremetation(&sunriseEndMm, 59);
      }
      if (buttonClickDebouncer.fell()) {
        nokiaLightDelay = nokiaLightDelayTemplate;

        if (tSunriseEndTime <= tSunsetStartTime) {
          menu = INVALID_TIME_SCREEN;
        } else {
          setValueToEeprom(eepromSunriseEndHh[0], eepromSunriseEndHh[1], sunriseEndHh);
          setValueToEeprom(eepromSunriseEndMm[0], eepromSunriseEndMm[1], sunriseEndMm);

          menu = MENU_SCREEN;
        }
      }
      break;
    case TIME_SUNSET_START:
      if (buttonUpDebouncer.fell()) {
        nokiaLightDelay = nokiaLightDelayTemplate;

        timeIncremetation(&sunsetStartHh, 23);
      }
      if (buttonDownDebouncer.fell()) {
        nokiaLightDelay = nokiaLightDelayTemplate;

        timeIncremetation(&sunsetStartMm, 59);
      }
      if (buttonClickDebouncer.fell()) {
        nokiaLightDelay = nokiaLightDelayTemplate;

        if (tSunsetStartTime <= tSunsetEndTime) {
          menu = INVALID_TIME_SCREEN;
        } else {
          setValueToEeprom(eepromSunsetStartHh[0], eepromSunsetStartHh[1], sunsetStartHh);
          setValueToEeprom(eepromSunsetStartMm[0], eepromSunsetStartMm[1], sunsetStartMm);

          menu = MENU_SCREEN;
        }
      }
      break;
    case TIME_SUNSET_END:
      if (buttonUpDebouncer.fell()) {
        nokiaLightDelay = nokiaLightDelayTemplate;

        timeIncremetation(&sunsetEndHh, 23);
      }
      if (buttonDownDebouncer.fell()) {
        nokiaLightDelay = nokiaLightDelayTemplate;

        timeIncremetation(&sunsetEndMm, 59);
      }
      if (buttonClickDebouncer.fell()) {
        nokiaLightDelay = nokiaLightDelayTemplate;

        if (tSunsetEndTime <= tSunsetStartTime) {
          menu = INVALID_TIME_SCREEN;
        } else {
          setValueToEeprom(eepromSunsetEndHh[0], eepromSunsetEndHh[1], sunsetEndHh);
          setValueToEeprom(eepromSunsetEndMm[0], eepromSunsetEndMm[1], sunsetEndMm);

          menu = MENU_SCREEN;
        }
      }
      break;
    case SET_CLOCK:
      if (buttonUpDebouncer.fell()) {
        nokiaLightDelay = nokiaLightDelayTemplate;

        timeIncremetation(&tempHh, 23);
      }
      if (buttonDownDebouncer.fell()) {
        nokiaLightDelay = nokiaLightDelayTemplate;

        timeIncremetation(&tempMm, 59);
      }
      if (buttonClickDebouncer.fell()) {
        nokiaLightDelay = nokiaLightDelayTemplate;

        rtClock.setDS1302Time(0, tempMm, tempHh, int(rtClock.dayofweek), int(rtClock.dayofmonth), int(rtClock.month), int(rtClock.year));

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

        ledMode = LED_OFF;
      }
      if (buttonDownDebouncer.fell()) {
        nokiaLightDelay = nokiaLightDelayTemplate;

        ledMode = LED_ON;
      }
      if (buttonClickDebouncer.fell()) {
        nokiaLightDelay = nokiaLightDelayTemplate;

        menu = MENU_SCREEN;
      }
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
  if (nokiaLightDelay > 0 || temperatureFromSensor >= temperatureAlarmTemplate) {
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

unsigned long convertTimeToMillis(int hours, int minutes, int seconds) {
  return hours * 3600000 + minutes * 60000 + seconds * 1000;
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
  nokia.clrScr();
  nokia.setFont(SmallFont);
  for (int i = MENU_SCREEN; i < sizeOfMenu; i++) {
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

void showInvalidTimeAlertScreen() {
  nokia.clrScr();
  nokia.setFont(TinyFont);
  nokia.print("The time must be", CENTER, 4);
  nokia.print("greater than", CENTER, 12);
  nokia.print("the previous step", CENTER, 20);
  nokia.print("time and less than", CENTER, 28);
  nokia.print("the next step time!", CENTER, 36);
  nokia.drawRect(0, 0, 83, 47);
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