#include <virtuabotixRTC.h>

//------------------------------------- UWAGI -------------------------------------
// na pinach 5 i 6 występują jakieś zakłócenia - https://www.arduino.cc/reference/en/language/functions/analog-io/analogwrite/
//------------------------------------- led ---------------------------------------
#define PWM_LED 5

enum LedModes {
  LED_OFF,
  LED_SUNRISE,
  LED_ON,
  LED_SUNSET
};
enum LedModes ledMode;

int sunriseStartHh;
int sunriseStartMm;
int sunriseEndHh;
int sunriseEndMm;

int sunsetStartHh;
int sunsetStartMm;
int sunsetEndHh;
int sunsetEndMm;
//------------------------------------- rtc ---------------------------------------
#define RTC_CLK A0
#define RTC_DAT A1
#define RTC_RST A2

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
//------------------------------------- setup ---------------------------------------
void setup() {
  //Serial.begin(9600);

  pinMode(PWM_LED, OUTPUT);

  sunriseStartHh = 10;
  sunriseStartMm = 0;
  sunriseEndHh = 10;
  sunriseEndMm = 15;

  sunsetStartHh = 18;
  sunsetStartMm = 45;
  sunsetEndHh = 19;
  sunsetEndMm = 0;

  tSunriseStartTime = convertTimeToSeconds(sunriseStartHh, sunriseStartMm, 0);
  tSunriseEndTime = convertTimeToSeconds(sunriseEndHh, sunriseEndMm, 0);
  tSunsetStartTime = convertTimeToSeconds(sunsetStartHh, sunsetStartMm, 0);
  tSunsetEndTime = convertTimeToSeconds(sunsetEndHh, sunsetEndMm, 0);

  currentTimeOld = convertTimeToSeconds(rtClock.hours, rtClock.minutes, rtClock.seconds);

  //                      seconds, minutes, hours, day of the week, day of the month, month, year
  //rtClock.setDS1302Time(0, 24, 14, 2, 21, 2, 2024);
}

void loop() {
  updateAndSynchronizeTime();

  //Serial.println("ledMode=" + String(ledMode) + ", currentTime=" + String(currentTime));
  Serial.println("ledMode=" + String(ledMode) + ", rtClock=" + String(rtClock.hours) + ":" + String(rtClock.minutes) + ":" + String(rtClock.seconds));

  setLedModeByTime();

  manageLed();

  delay(1000);
}

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

    int pwmValue = map(elapsedTime, 0, animationEnd - animationStart, 0, steps);

    if (!increase) {
      pwmValue = steps - pwmValue;  // Jeżeli malejemy, odwróć wartości
    }

    Serial.println("pwmValue=" + String(pwmValue));

    return pwmValue;
  }
}

unsigned long convertTimeToSeconds(int hours, int minutes, int seconds) {
  return hours * 3600UL + minutes * 60UL + seconds;
}

void updateAndSynchronizeTime() {
  bool isSynchronized = false;
  do {
    rtClock.updateTime();
    currentTime = convertTimeToSeconds(rtClock.hours, rtClock.minutes, rtClock.seconds);

    Serial.print("currentTime="+String(currentTime) + ", currentTimeOld="+currentTimeOld);

    if (currentTime > currentTimeOld && (currentTimeOld + timeDelta) > currentTime) {
      Serial.println(", syncrhonized OK");
      
      isSynchronized = true;
    } else {
      Serial.println(", syncrhonized ERROR !!!");
      
      delay(1050);
      
      currentTimeOld = convertTimeToSeconds(rtClock.hours, rtClock.minutes, rtClock.seconds);
    }
  } while (!isSynchronized);
}
