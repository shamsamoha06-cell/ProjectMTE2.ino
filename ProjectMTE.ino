
#define BLYNK_TEMPLATE_ID "TMPL6IxiFgPDc"
#define BLYNK_TEMPLATE_NAME "Project Progress"
#define BLYNK_AUTH_TOKEN "Nqjv-YkP0WWP-tLYiB0f0Y6GW4Wyhnun"


#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include "Grove_Motor_Driver_TB6612FNG.h"
#include <Wire.h>
#include <U8x8lib.h>

char auth[] = BLYNK_AUTH_TOKEN;
char ssid[] = "Shamsa";
char pass[] = "shamsabin3";

MotorDriver motor;
U8X8_SSD1306_128X64_NONAME_HW_I2C oled(U8X8_PIN_NONE);

const int buzzerPin = A3;
const int weakSignalThreshold = -75;

int motorSpeed = 200;
int currentDirection = 0;
int wifiRSSI = 0;

bool hornActive = false;
bool beepActive = false;
bool dancing = false;
bool danceComplete = false;
bool weakSignalWarning = false;
bool previousConnection = false;

unsigned long beepUntil = 0;
unsigned long nextBeepAt = 0;
unsigned long lastDisplay = 0;
unsigned long lastDanceStep = 0;
unsigned long lastSignalCheck = 0;
unsigned long lastWarning = 0;
unsigned long lastWiFiAlarm = 0;
unsigned long lastReconnectAttempt = 0;

int danceStep = 0;
int warningBeepsRemaining = 0;

const int danceNotes[] = {
  523, 659, 784, 659,
  587, 698, 880, 698,
  659, 784, 988, 784,
  880, 784, 659, 1047
};

const int danceDurations[] = {
  300, 300, 300, 300,
  300, 300, 300, 300,
  300, 300, 300, 500
};

const int danceMoves[] = {
  1, 1, 3, 3,
  2, 2, 4, 4,
  1, 3, 1, 4,
  3, 4, 3, 0
};

const int totalDanceSteps =
  sizeof(danceNotes) / sizeof(danceNotes[0]);

void buzzerOff() {
  ledcWriteTone(buzzerPin, 0);
  beepActive = false;
}

void playDanceNote(int frequency) {
  ledcWriteTone(buzzerPin, frequency);
  ledcWrite(buzzerPin, 127);
}

void shortBeep() {
  if (hornActive || dancing) return;

  warningBeepsRemaining = 0;
  ledcWriteTone(buzzerPin, 2000);
  beepActive = true;
  beepUntil = millis() + 100;
}

void startWarningBeeps(int count) {
  if (hornActive || dancing) return;

  warningBeepsRemaining = count;
  nextBeepAt = millis();
}

void updateBuzzer() {
  if (hornActive || dancing) return;

  unsigned long now = millis();

  if (beepActive &&
      (long)(now - beepUntil) >= 0) {
    buzzerOff();
  }

  if (warningBeepsRemaining > 0 &&
      !beepActive &&
      (long)(now - nextBeepAt) >= 0) {
    ledcWriteTone(buzzerPin, 3000);
    beepActive = true;
    beepUntil = now + 120;
    nextBeepAt = now + 300;
    warningBeepsRemaining--;
  }
}

void setHorn(bool enabled) {
  if (dancing) return;

  hornActive = enabled;
  warningBeepsRemaining = 0;
  beepActive = false;

  ledcWriteTone(buzzerPin, enabled ? 1500 : 0);
}

const char* directionName() {
  if (dancing) return "DANCING";

  switch (currentDirection) {
    case 1: return "FORWARD";
    case 2: return "BACKWARD";
    case 3: return "LEFT";
    case 4: return "RIGHT";
    default: return "STOP";
  }
}

const char* signalQuality() {
  if (WiFi.status() != WL_CONNECTED) return "NO WIFI";
  if (wifiRSSI >= -60) return "GOOD";
  if (wifiRSSI >= -70) return "FAIR";
  return "WEAK";
}

void updateOLED() {
  oled.clearDisplay();

  oled.setCursor(0, 0);
  oled.print("JDP ROBOT");

  oled.setCursor(0, 1);
  oled.print("Move:");
  oled.print(directionName());

  oled.setCursor(0, 2);
  oled.print("Speed:");
  oled.print(motorSpeed);

  oled.setCursor(0, 3);
  oled.print("WiFi:");
  oled.print(signalQuality());

  oled.setCursor(0, 4);
  oled.print("RSSI:");
  if (WiFi.status() == WL_CONNECTED) {
    oled.print(wifiRSSI);
    oled.print(" dBm");
  } else {
    oled.print("N/A");
  }

  oled.setCursor(0, 5);
  oled.print("Blynk:");
  oled.print(Blynk.connected() ? "ONLINE" : "OFFLINE");

  oled.setCursor(0, 7);
  if (WiFi.status() != WL_CONNECTED) {
    oled.print("NO WIFI!");
  } else if (dancing) {
    oled.print("DANCE MODE!");
  } else if (danceComplete) {
    oled.print("DANCE COMPLETE");
  } else if (weakSignalWarning) {
    oled.print("WEAK WIFI!");
  }
}

void driveMotors(int direction) {
  currentDirection = direction;

  if (motorSpeed == 0 || direction == 0) {
    motor.dcMotorStop(MOTOR_CHA);
    motor.dcMotorStop(MOTOR_CHB);
    return;
  }

  switch (direction) {
    case 1:
      motor.dcMotorRun(MOTOR_CHA, motorSpeed);
      motor.dcMotorRun(MOTOR_CHB, -motorSpeed);
      break;

    case 2:
      motor.dcMotorRun(MOTOR_CHA, -motorSpeed);
      motor.dcMotorRun(MOTOR_CHB, motorSpeed);
      break;

    case 3:
      motor.dcMotorRun(MOTOR_CHA, -motorSpeed);
      motor.dcMotorRun(MOTOR_CHB, -motorSpeed);
      break;

    case 4:
      motor.dcMotorRun(MOTOR_CHA, motorSpeed);
      motor.dcMotorRun(MOTOR_CHB, motorSpeed);
      break;
  }
}

void stopRobot() {
  driveMotors(0);
  updateOLED();
}

void moveRobot(int direction) {
  if (dancing) return;
  if (WiFi.status() != WL_CONNECTED) return;
  if (!Blynk.connected()) return;

  danceComplete = false;
  driveMotors(direction);
  updateOLED();
}

void updateSpeed() {
  driveMotors(currentDirection);
  updateOLED();
}

void cancelDance() {
  dancing = false;
  danceComplete = false;
  danceStep = 0;
  buzzerOff();
  driveMotors(0);
  updateOLED();
}

void startDance() {
  if (dancing) return;
  if (WiFi.status() != WL_CONNECTED) return;
  if (!Blynk.connected()) return;

  setHorn(false);
  buzzerOff();
  warningBeepsRemaining = 0;

  dancing = true;
  danceComplete = false;
  danceStep = 0;
  lastDanceStep = millis();

  driveMotors(danceMoves[0]);
  playDanceNote(danceNotes[0]);
  updateOLED();
}

void updateDance() {
  if (!dancing) return;

  unsigned long now = millis();

  if (now - lastDanceStep <
      (unsigned long)danceDurations[danceStep]) {
    return;
  }

  lastDanceStep = now;
  danceStep++;

  if (danceStep >= totalDanceSteps) {
    dancing = false;
    danceComplete = true;
    driveMotors(0);
    buzzerOff();
    updateOLED();
    return;
  }

  driveMotors(danceMoves[danceStep]);
  playDanceNote(danceNotes[danceStep]);
  updateOLED();
}

void updateWiFiSignal() {
  unsigned long now = millis();

  if (now - lastSignalCheck < 1000) return;
  lastSignalCheck = now;

  if (WiFi.status() == WL_CONNECTED) {
    wifiRSSI = WiFi.RSSI();
    weakSignalWarning =
      wifiRSSI <= weakSignalThreshold;

    if (weakSignalWarning &&
        (lastWarning == 0 ||
         now - lastWarning >= 10000)) {
      lastWarning = now;
      startWarningBeeps(3);
    }

    if (!weakSignalWarning) {
      lastWarning = 0;
    }
  } else {
    wifiRSSI = 0;
    weakSignalWarning = false;
    lastWarning = 0;
  }
}

void updateWiFiAlarm() {
  static bool wifiWasConnected = false;
  static bool alarmStarted = false;

  unsigned long now = millis();
  bool wifiConnected =
    WiFi.status() == WL_CONNECTED;

  if (wifiConnected) {
    if (alarmStarted) {
      alarmStarted = false;
      warningBeepsRemaining = 0;
      buzzerOff();
    }
    wifiWasConnected = true;
    return;
  }

  if (!alarmStarted) {
    alarmStarted = true;
    lastWiFiAlarm = now - 5000;

    dancing = false;
    danceComplete = false;
    danceStep = 0;
    hornActive = false;
    warningBeepsRemaining = 0;
    buzzerOff();
    driveMotors(0);
    updateOLED();
  }

  if (now - lastWiFiAlarm >= 5000) {
    lastWiFiAlarm = now;
    startWarningBeeps(3);
  }

  wifiWasConnected = false;
}

void maintainConnection() {
  unsigned long now = millis();

  if (WiFi.status() != WL_CONNECTED) {
    if (now - lastReconnectAttempt >= 3000) {
      lastReconnectAttempt = now;
      WiFi.reconnect();
    }
    return;
  }

  if (!Blynk.connected() &&
      now - lastReconnectAttempt >= 3000) {
    lastReconnectAttempt = now;
    Blynk.connect(100);
  }
}

BLYNK_WRITE(V0) {
  if (dancing) return;

  if (param.asInt()) {
    moveRobot(1);
    shortBeep();
  } else if (currentDirection == 1) {
    stopRobot();
  }
}

BLYNK_WRITE(V1) {
  if (dancing) return;

  if (param.asInt()) {
    moveRobot(2);
    shortBeep();
  } else if (currentDirection == 2) {
    stopRobot();
  }
}

BLYNK_WRITE(V2) {
  if (dancing) return;

  if (param.asInt()) {
    moveRobot(3);
    shortBeep();
  } else if (currentDirection == 3) {
    stopRobot();
  }
}

BLYNK_WRITE(V3) {
  if (dancing) return;

  if (param.asInt()) {
    moveRobot(4);
    shortBeep();
  } else if (currentDirection == 4) {
    stopRobot();
  }
}

BLYNK_WRITE(V4) {
  if (param.asInt()) {
    cancelDance();
    setHorn(false);
    stopRobot();
    shortBeep();
  }
}

BLYNK_WRITE(V5) {
  motorSpeed = constrain(param.asInt(), 0, 255);
  updateSpeed();
}

BLYNK_WRITE(V6) {
  setHorn(param.asInt() == 1);
}

BLYNK_WRITE(V7) {
  if (param.asInt()) startDance();
}

BLYNK_CONNECTED() {
  Blynk.syncVirtual(V5);
  updateOLED();
}

void setup() {
  Serial.begin(115200);
  Wire.begin();

  oled.begin();
  oled.setFlipMode(1);
  oled.setFont(u8x8_font_chroma48medium8_r);

  ledcAttach(buzzerPin, 2000, 8);
  buzzerOff();

  motor.init();
  driveMotors(0);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);
  Blynk.config(auth);

  previousConnection = false;
  updateOLED();
}

void loop() {
  Blynk.run();

  updateWiFiAlarm();
  updateDance();
  updateBuzzer();
  updateWiFiSignal();
  maintainConnection();

  bool connected =
    WiFi.status() == WL_CONNECTED &&
    Blynk.connected();

  if (previousConnection && !connected) {
    cancelDance();
    setHorn(false);
    driveMotors(0);
  }

  previousConnection = connected;

  if (millis() - lastDisplay >= 500) {
    lastDisplay = millis();
    updateOLED();
  }
}
