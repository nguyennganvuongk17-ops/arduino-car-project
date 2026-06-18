#include <SoftwareSerial.h>

// =====================================================
// 1. BLUETOOTH HC-05
// =====================================================
const byte BT_RX_PIN = 2;   // Arduino RX nhận từ TX của HC-05
const byte BT_TX_PIN = 3;   // Arduino TX gửi đến RX của HC-05
SoftwareSerial mySerial(BT_RX_PIN, BT_TX_PIN);

// =====================================================
// 2. CẢM BIẾN
// =====================================================
const byte FLAME_SENSOR_PIN = 4;   // Cảm biến lửa chân DO
const byte GAS_SENSOR_PIN   = A0;  // Cảm biến gas chân AO

// =====================================================
// 3. MOTOR
// =====================================================
const byte MOTOR_B1_PIN = 5;
const byte MOTOR_B2_PIN = 6;
const byte MOTOR_A1_PIN = 9;
const byte MOTOR_A2_PIN = 10;

// =====================================================
// 4. THIẾT BỊ PHỤ
// =====================================================
const byte BUZZER_PIN    = 7;
const byte BT_STATE_PIN  = 8;
const byte HEADLIGHT_PIN = 11;  // Đèn trước U/u
const byte PUMP_RELAY_PIN = 12; // Relay điều khiển bơm nước
const byte TAILLIGHT_PIN = 13;  // Đèn sau / đèn cảnh báo

// =====================================================
// 5. CẤU HÌNH BUZZER
// =====================================================
const byte BUZZER_ON  = HIGH;
const byte BUZZER_OFF = LOW;

// =====================================================
// 6. CẤU HÌNH RELAY BƠM NƯỚC
// =====================================================
const byte RELAY_ON  = HIGH;
const byte RELAY_OFF = LOW;

// =====================================================
// 7. CẤU HÌNH CẢM BIẾN (ĐÃ TỐI ƯU SIÊU TỐC)
// =====================================================
const byte FLAME_DETECTED_LEVEL = LOW; // Có lửa = LOW (Đổi thành HIGH nếu module của bạn ngược lại)
const int GAS_THRESHOLD = 300;         // Ngưỡng gas

// Giữ nguyên cấu hình Gas ổn định chống nhiễu
const unsigned long GAS_CONFIRM_MS = 800;
const unsigned long GAS_CLEAR_MS = 2000;

// =====================================================
// 8. BIẾN ĐIỀU KHIỂN
// =====================================================
int vSpeed = 220;

char command = 'S';
char moveCommand = 'S';
char lastCommand = ' ';
bool headlightOn = false;
bool taillightOn = false;
bool parkingLightOn = false;
bool hazardLightOn = false;

bool fireMode = false;
bool gasMode = false;

unsigned long gasStartMs = 0;
unsigned long gasClearStartMs = 0;

unsigned long hornUntil = 0;
unsigned long manualPumpUntil = 0;
unsigned long lastSensorPrint = 0;

const bool DEBUG_SENSOR = true;

// =====================================================
// SETUP
// =====================================================
void setup() {
  pinMode(MOTOR_A1_PIN, OUTPUT);
  pinMode(MOTOR_A2_PIN, OUTPUT);
  pinMode(MOTOR_B1_PIN, OUTPUT);
  pinMode(MOTOR_B2_PIN, OUTPUT);

  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(PUMP_RELAY_PIN, OUTPUT);
  pinMode(HEADLIGHT_PIN, OUTPUT);
  pinMode(TAILLIGHT_PIN, OUTPUT);

  pinMode(BT_STATE_PIN, INPUT);
  pinMode(FLAME_SENSOR_PIN, INPUT);
  pinMode(GAS_SENSOR_PIN, INPUT);

  Serial.begin(9600);
  mySerial.begin(9600);

  digitalWrite(BUZZER_PIN, BUZZER_OFF);
  digitalWrite(PUMP_RELAY_PIN, RELAY_OFF);
  digitalWrite(HEADLIGHT_PIN, LOW);
  digitalWrite(TAILLIGHT_PIN, LOW);

  stopMotors();

  Serial.println("He thong da san sang!");
}

// =====================================================
// LOOP
// =====================================================
void loop() {
  readBluetoothCommand();

  int flameValue = digitalRead(FLAME_SENSOR_PIN);
  int gasValue = analogRead(GAS_SENSOR_PIN);

  bool rawFlameDetected = (flameValue == FLAME_DETECTED_LEVEL);
  bool rawGasDetected = (gasValue >= GAS_THRESHOLD);

  updateFireMode(rawFlameDetected);
  updateGasMode(rawGasDetected);

  printSensorValue(gasValue, flameValue);

  handleLights();
  handlePump();
  handleBuzzer();

  if (fireMode || gasMode) {
    stopMotors();
    return;
  }

  controlMotors(moveCommand);
}

// =====================================================
// ĐỌC LỆNH BLUETOOTH
// =====================================================
void readBluetoothCommand() {
  while (mySerial.available()) {
    command = mySerial.read();

    if (command == '\n' || command == '\r' || command == ' ') continue;
    if (command >= '0' && command <= '9') continue;

    if (command != lastCommand) {
      Serial.print("Lenh nhan duoc: ");
      Serial.println(command);
      lastCommand = command;
    }

    if (command == 'F' || command == 'B' || command == 'L' || command == 'R' ||
        command == 'G' || command == 'H' || command == 'I' || command == 'J' ||
        command == 'S') {
      moveCommand = command;
    }
    else if (command == 'U') headlightOn = true;
    else if (command == 'u') headlightOn = false;
    else if (command == 'V') taillightOn = true;
    else if (command == 'v') taillightOn = false;
    else if (command == 'W') parkingLightOn = true;
    else if (command == 'w') parkingLightOn = false;
    else if (command == 'X') hazardLightOn = true;
    else if (command == 'x') hazardLightOn = false;
    else if (command == 'Y') hornUntil = millis() + 300;
    else if (command == 'y') hornUntil = 0;
    else if (command == 'Z') {
      manualPumpUntil = millis() + 3000;
      stopMotors();
      moveCommand = 'S';
      Serial.println("Test bom nuoc 3 giay");
    }
  }
}

// =====================================================
// XỬ LÝ CHẾ ĐỘ PHÁT HIỆN LỬA (ĐÃ SỬA: CHẠY TỨC THÌ)
// =====================================================
void updateFireMode(bool rawFlameDetected) {
  // Nếu phát hiện có lửa, kích hoạt trạng thái báo động ngay lập tức
  if (rawFlameDetected) {
    if (!fireMode) {
      fireMode = true;
      stopMotors();
      moveCommand = 'S';
      Serial.println("CHÚ Ý: Có lửa! Báo động và bật bơm ngay lập tức!");
    }
  } 
  // Nếu mắt cảm biến không còn thấy lửa, hủy báo động ngay lập tức
  else {
    if (fireMode) {
      fireMode = false;
      stopMotors();
      moveCommand = 'S';
      Serial.println("Đã hết lửa. Tắt bơm, hệ thống an toàn.");
    }
  }
}

// =====================================================
// XỬ LÝ CHẾ ĐỘ GAS
// =====================================================
void updateGasMode(bool rawGasDetected) {
  unsigned long now = millis();

  if (!gasMode) {
    if (rawGasDetected) {
      if (gasStartMs == 0) gasStartMs = now;
      if (now - gasStartMs >= GAS_CONFIRM_MS) {
        gasMode = true;
        gasClearStartMs = 0;
        stopMotors();
        moveCommand = 'S';
        Serial.println("CANH BAO: Phat hien gas! Xe dung de an toan.");
      }
    } else {
      gasStartMs = 0;
    }
  }
  else {
    if (rawGasDetected) {
      gasClearStartMs = 0;
    } else {
      if (gasClearStartMs == 0) gasClearStartMs = now;
      if (now - gasClearStartMs >= GAS_CLEAR_MS) {
        gasMode = false;
        gasStartMs = 0;
        gasClearStartMs = 0;
        stopMotors();
        moveCommand = 'S';
        Serial.println("Gas da tro ve muc an toan. Quay lai che do dieu khien.");
      }
    }
  }
}

// =====================================================
// ĐIỀU KHIỂN MOTOR
// =====================================================
void controlMotors(char state) {
  if (vSpeed <= 0) {
    stopMotors();
    return;
  }

  int turnSpeed = (vSpeed * 75) / 100;
  if (turnSpeed < 130) turnSpeed = 130;

  if (state == 'F') {
    analogWrite(MOTOR_A1_PIN, vSpeed);
    analogWrite(MOTOR_A2_PIN, 0);
    analogWrite(MOTOR_B1_PIN, vSpeed);
    analogWrite(MOTOR_B2_PIN, 0);
  }
  else if (state == 'B') {
    analogWrite(MOTOR_A1_PIN, 0);
    analogWrite(MOTOR_A2_PIN, vSpeed);
    analogWrite(MOTOR_B1_PIN, 0);
    analogWrite(MOTOR_B2_PIN, vSpeed);
  }
  else if (state == 'L') {
    analogWrite(MOTOR_A1_PIN, vSpeed);
    analogWrite(MOTOR_A2_PIN, 0);
    analogWrite(MOTOR_B1_PIN, 0);
    analogWrite(MOTOR_B2_PIN, vSpeed);
  }
  else if (state == 'R') {
    analogWrite(MOTOR_A1_PIN, 0);
    analogWrite(MOTOR_A2_PIN, vSpeed);
    analogWrite(MOTOR_B1_PIN, vSpeed);
    analogWrite(MOTOR_B2_PIN, 0);
  }
  else if (state == 'G') {
    analogWrite(MOTOR_A1_PIN, vSpeed);
    analogWrite(MOTOR_A2_PIN, 0);
    analogWrite(MOTOR_B1_PIN, turnSpeed);
    analogWrite(MOTOR_B2_PIN, 0);
  }
  else if (state == 'H') {
    analogWrite(MOTOR_A1_PIN, turnSpeed);
    analogWrite(MOTOR_A2_PIN, 0);
    analogWrite(MOTOR_B1_PIN, vSpeed);
    analogWrite(MOTOR_B2_PIN, 0);
  }
  else if (state == 'I') {
    analogWrite(MOTOR_A1_PIN, 0);
    analogWrite(MOTOR_A2_PIN, vSpeed);
    analogWrite(MOTOR_B1_PIN, 0);
    analogWrite(MOTOR_B2_PIN, turnSpeed);
  }
  else if (state == 'J') {
    analogWrite(MOTOR_A1_PIN, 0);
    analogWrite(MOTOR_A2_PIN, turnSpeed);
    analogWrite(MOTOR_B1_PIN, 0);
    analogWrite(MOTOR_B2_PIN, vSpeed);
  }
  else {
    stopMotors();
  }
}

void stopMotors() {
  analogWrite(MOTOR_A1_PIN, 0);
  analogWrite(MOTOR_A2_PIN, 0);
  analogWrite(MOTOR_B1_PIN, 0);
  analogWrite(MOTOR_B2_PIN, 0);
}

// =====================================================
// XỬ LÝ ĐÈN
// =====================================================
void handleLights() {
  bool blinkState = ((millis() / 250) % 2 == 0);

  if (hazardLightOn || fireMode || gasMode) {
    digitalWrite(HEADLIGHT_PIN, blinkState ? HIGH : LOW);
    digitalWrite(TAILLIGHT_PIN, blinkState ? HIGH : LOW);
    return;
  }

  if (parkingLightOn) {
    digitalWrite(HEADLIGHT_PIN, HIGH);
    digitalWrite(TAILLIGHT_PIN, HIGH);
    return;
  }

  digitalWrite(HEADLIGHT_PIN, headlightOn ? HIGH : LOW);
  digitalWrite(TAILLIGHT_PIN, taillightOn ? HIGH : LOW);
}

// =====================================================
// XỬ LÝ BƠM NƯỚC
// =====================================================
void handlePump() {
  bool manualPumpActive = millis() < manualPumpUntil;

  if (fireMode || manualPumpActive) {
    digitalWrite(PUMP_RELAY_PIN, RELAY_ON);
  } else {
    digitalWrite(PUMP_RELAY_PIN, RELAY_OFF);
  }
}

// =====================================================
// XỬ LÝ CÒI
// =====================================================
void handleBuzzer() {
  unsigned long now = millis();

  if (fireMode || gasMode) {
    bool beepState = ((now / 120) % 2 == 0);
    digitalWrite(BUZZER_PIN, beepState ? BUZZER_ON : BUZZER_OFF);
    return;
  }

  if (now < hornUntil) {
    digitalWrite(BUZZER_PIN, BUZZER_ON);
  } else {
    digitalWrite(BUZZER_PIN, BUZZER_OFF);
  }
}

// =====================================================
// IN GIÁ TRỊ CẢM BIẾN
// =====================================================
void printSensorValue(int gasValue, int flameValue) {
  if (!DEBUG_SENSOR) return;

  if (millis() - lastSensorPrint >= 1000) {
    Serial.print("Gas: ");
    Serial.print(gasValue);
    Serial.print(" | Flame: ");
    Serial.print(flameValue);
    Serial.print(" | FireMode: ");
    Serial.print(fireMode ? "YES" : "NO");
    Serial.print(" | GasMode: ");
    Serial.println(gasMode ? "YES" : "NO");
    lastSensorPrint = millis();
  }
}