


#define BLYNK_TEMPLATE_ID "TMPL6IxiFgPDc"
#define BLYNK_TEMPLATE_NAME "Project Progress"
#define BLYNK_AUTH_TOKEN "Nqjv-YkP0WWP-tLYiB0f0Y6GW4Wyhnun"

#include <WiFi.h>
#include <Wire.h>
#include <BlynkSimpleEsp32.h>
#include <Grove_Motor_Driver_TB6612FNG.h>

// Wi-Fi details
char ssid[] = "Shamsa";
char pass[] = "shamsabin3";

// Hardware
#define BUZZER_PIN A3

MotorDriver motor;

// Motor direction settings
// Change 1 to -1 if a motor runs backward
const int LEFT_DIRECTION = 1;
const int RIGHT_DIRECTION = 1;

// Speed
int motorSpeed = 150;

// Movement
enum Movement {
  STOPPED,
  FORWARD,
  BACKWARD,
  LEFT,
  RIGHT
};

Movement currentMovement = STOPPED;

// Timers
unsigned long lastWiFiCheck = 0;
unsigned long lastReconnect = 0;
unsigned long lastCommand = 0;
unsigned long hornEndTime = 0;

bool hornActive = false;

// Safety settings
const unsigned long COMMAND_TIMEOUT = 3000;
const int WEAK_WIFI_RSSI = -75;

// -------- Motor Control --------

void stopMotors() {
  motor.dcMotorStop(MOTOR_CHA);
  motor.dcMotorStop(MOTOR_CHB);
  currentMovement = STOPPED;
}

void runMotors(int leftSpeed, int rightSpeed) {
  motor.dcMotorRun(
    MOTOR_CHA,
    leftSpeed * LEFT_DIRECTION
  );

  motor.dcMotorRun(
    MOTOR_CHB,
    rightSpeed * RIGHT_DIRECTION
  );
}

void applyMovement() {
  if (WiFi.status() != WL_CONNECTED ||
      !Blynk.connected()) {
    stopMotors();
    return;
  }

  switch (currentMovement) {
    case FORWARD:
      runMotors(motorSpeed, motorSpeed);
      break;

    case BACKWARD:
      runMotors(-motorSpeed, -motorSpeed);
      break;

    case LEFT:
      runMotors(-motorSpeed, motorSpeed);
      break;

    case RIGHT:
      runMotors(-motorSpeed, motorSpeed);
      break;

    default:
      stopMotors();
      break;
  }
}

// -------- Blynk Controls --------

// V0 - Forward
BLYNK_WRITE(V0) {
  if (param.asInt() == 1) {
    currentMovement = FORWARD;
    lastCommand = millis();
    applyMovement();
  } else if (currentMovement == FORWARD) {
    stopMotors();
  }
}

// V1 - Backward
BLYNK_WRITE(V1) {
  if (param.asInt() == 1) {
    currentMovement = BACKWARD;
    lastCommand = millis();
    applyMovement();
  } else if (currentMovement == BACKWARD) {
    stopMotors();
  }
}

// V2 - Left
BLYNK_WRITE(V2) {
  if (param.asInt() == 1) {
    currentMovement = LEFT;
    lastCommand = millis();
    applyMovement();
  } else if (currentMovement == LEFT) {
    stopMotors();
  }
}

// V3 - Right
BLYNK_WRITE(V3) {
  if (param.asInt() == 1) {
    currentMovement = RIGHT;
    lastCommand = millis();
    applyMovement();
  } else if (currentMovement == RIGHT) {
    stopMotors();
  }
}

// V4 - Stop
BLYNK_WRITE(V4) {
  if (param.asInt() == 1) {
    stopMotors();
  }
}

// V5 - Speed slider (0-255)
BLYNK_WRITE(V5) {
  motorSpeed = constrain(param.asInt(), 0, 255);
  lastCommand = millis();

  if (currentMovement != STOPPED) {
    applyMovement();
  }
}

// V6 - Horn
BLYNK_WRITE(V6) {
  if (param.asInt() == 1) {
    tone(BUZZER_PIN, 1000);
    hornActive = true;
    hornEndTime = millis() + 300;
  } else {
    noTone(BUZZER_PIN);
    hornActive = false;
  }
}

// -------- Blynk Connection --------

BLYNK_CONNECTED() {
  stopMotors();

  Blynk.virtualWrite(V0, 0);
  Blynk.virtualWrite(V1, 0);
  Blynk.virtualWrite(V2, 0);
  Blynk.virtualWrite(V3, 0);
  Blynk.virtualWrite(V4, 0);
  Blynk.virtualWrite(V5, motorSpeed);
  Blynk.virtualWrite(V6, 0);

  Serial.println("Blynk connected");
}

// -------- Wi-Fi Monitoring --------

void checkWiFi() {
  if (millis() - lastWiFiCheck < 1000) {
    return;
  }

  lastWiFiCheck = millis();

  if (WiFi.status() != WL_CONNECTED ||
      !Blynk.connected()) {
    stopMotors();
    Serial.println("Connection lost - motors stopped");
    return;
  }

  int rssi = WiFi.RSSI();

  Serial.print("WiFi RSSI: ");
  Serial.print(rssi);
  Serial.println(" dBm");

  if (rssi >= -60) {
    Serial.println("WiFi: GOOD");
  } else if (rssi >= WEAK_WIFI_RSSI) {
    Serial.println("WiFi: FAIR");
  } else {
    Serial.println("WiFi: WEAK");
  }
}

// -------- Setup --------

void setup() {
  Serial.begin(115200);

  pinMode(BUZZER_PIN, OUTPUT);
  noTone(BUZZER_PIN);

  Wire.begin();

  motor.init();
  stopMotors();

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);

  Blynk.config(BLYNK_AUTH_TOKEN);

  lastCommand = millis();

  Serial.println("SMART Robotic Tank Started");
}

// -------- Loop --------

void loop() {
  if (WiFi.status() == WL_CONNECTED &&
      Blynk.connected()) {
    Blynk.run();
  }

  // Connection-loss safety
  if (WiFi.status() != WL_CONNECTED ||
      !Blynk.connected()) {
    if (currentMovement != STOPPED) {
      stopMotors();
    }
  }

  // Command timeout
  if (currentMovement != STOPPED &&
      millis() - lastCommand > COMMAND_TIMEOUT) {
    stopMotors();
  }

  checkWiFi();

  // Stop horn after 300 ms
  if (hornActive &&
      (long)(millis() - hornEndTime) >= 0) {
    noTone(BUZZER_PIN);
    hornActive = false;
  }

  // Reconnection
  if (millis() - lastReconnect >= 5000) {
    lastReconnect = millis();

    if (WiFi.status() != WL_CONNECTED) {
      stopMotors();
      WiFi.reconnect();
    } else if (!Blynk.connected()) {
      stopMotors();
      Blynk.connect(1000);
    }
  }
}
