//------------------------------------- led ---------------------------------------
#define PWM_LED 3

enum LedModes {
  LED_OFF,
  LED_SUNRISE,
  LED_SUNSET,
  LED_ON
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

int hh = 18;
int mm = 44;

//------------------------------------- setup ---------------------------------------
void setup() {
  Serial.begin(9600);

  pinMode(PWM_LED, OUTPUT);

  sunriseStartHh = 10;
  sunriseStartMm = 0;
  sunriseEndHh = 10;
  sunriseEndMm = 15;

  sunsetStartHh = 18;
  sunsetStartMm = 45;
  sunsetEndHh = 19;
  sunsetEndMm = 0;
}

void loop() {
  setLedModeByTime();

  manageLed();

  delay(500);
}

void setLedModeByTime() {
  unsigned long tNow = convertTimeToMillis(hh, mm, 0) + millis();

  unsigned long tSunriseEndTime = convertTimeToMillis(sunriseEndHh, sunriseEndMm, 0);
  unsigned long tSunsetStartTime = convertTimeToMillis(sunsetStartHh, sunsetStartMm, 0);

  if (convertTimeToMillis(sunriseStartHh, sunriseStartMm, 0) <= tNow && tNow <= tSunriseEndTime) {
    ledMode = LED_SUNRISE;
  } else if (tSunriseEndTime <= tNow && tNow <= tSunsetStartTime) {
    ledMode = LED_ON;
  } else if (tSunsetStartTime <= tNow && tNow <= convertTimeToMillis(sunsetEndHh, sunsetEndMm, 0)) {
    ledMode = LED_SUNSET;
  } else {
    ledMode = LED_OFF;
  }
}

void manageLed() {
  switch (ledMode) {
    case LED_SUNRISE:
      smoothPWMIncrease(
        convertTimeToMillis(sunriseStartHh, sunriseStartMm, 0),
        convertTimeToMillis(sunriseEndHh, sunriseEndMm, 0),
        true,
        255);
      break;
    case LED_ON:
      analogWrite(PWM_LED, 255);
      break;
    case LED_SUNSET:
      smoothPWMIncrease(
        convertTimeToMillis(sunsetStartHh, sunsetStartMm, 0),
        convertTimeToMillis(sunsetEndHh, sunsetEndMm, 0),
        false,
        255);
      break;
    case LED_OFF:
      analogWrite(PWM_LED, 0);
      break;
  }
}

void smoothPWMIncrease(unsigned long animationStart, unsigned long animationEnd, bool increase, int steps) {
  unsigned long currentTime = convertTimeToMillis(hh, mm, 0) + millis();

  if (currentTime >= animationStart && currentTime <= animationEnd) {
    unsigned long elapsedTime = currentTime - animationStart;

    int i = map(elapsedTime, 0, animationEnd - animationStart, 0, steps);

    if (!increase) {
      i = steps - i;  // Jeżeli malejemy, odwróć wartości
    }

    int pwmValue = map(i, 0, steps, 0, 255);

    Serial.println("pwmValue=" + String(pwmValue));

    analogWrite(PWM_LED, pwmValue);
  }
}

unsigned long convertTimeToMillis(int hours, int minutes, int seconds) {
  return hours * 3600000 + minutes * 60000 + seconds * 1000;
}

String millisToTime(unsigned long millis) {
  unsigned long seconds = millis / 1000;
  unsigned long minutes = seconds / 60;
  unsigned long hours = minutes / 60;

  // Oblicz pozostałe minuty i sekundy
  seconds %= 60;
  minutes %= 60;

  // Tworzenie stringa w formacie hh:mm:ss
  char result[9];  // 8 znaków dla czasu + 1 dla zakończenia ciągu znaków
  snprintf(result, sizeof(result), "%02lu:%02lu:%02lu", hours, minutes, seconds);

  return String(result);
}