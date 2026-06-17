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

// Nếu buzzer của bạn bị ngược, đổi thành:
// const byte BUZZER_ON  = LOW;
// const byte BUZZER_OFF = HIGH;

// =====================================================
// 6. CẤU HÌNH RELAY BƠM NƯỚC
// =====================================================
const byte RELAY_ON  = HIGH;
const byte RELAY_OFF = LOW;

// Nếu relay của bạn bật ngược, đổi thành:
// const byte RELAY_ON  = LOW;
// const byte RELAY_OFF = HIGH;

// =====================================================
// 7. CẤU HÌNH CẢM BIẾN
// =====================================================
// Nhiều cảm biến lửa loại module: có lửa = LOW, không lửa = HIGH
const byte FLAME_DETECTED_LEVEL = LOW;

// Nếu cảm biến lửa của bạn có lửa = HIGH, đổi thành:
// const byte FLAME_DETECTED_LEVEL = HIGH;

// Ngưỡng gas, cần chỉnh theo giá trị thực tế trên Serial Monitor
const int GAS_THRESHOLD = 300;

// Chống báo giả: phải thấy lửa liên tục 600ms mới kích hoạt
const unsigned long FIRE_CONFIRM_MS = 600;

// Khi hết lửa, phải sạch liên tục 2000ms mới tắt bơm
const unsigned long FIRE_CLEAR_MS = 2000;

// Bơm chạy tối thiểu 3000ms khi đã phát hiện lửa
const unsigned long PUMP_MIN_MS = 3000;

// Gas cũng cần ổn định để tránh nhiễu
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

unsigned long flameStartMs = 0;
unsigned long flameClearStartMs = 0;
unsigned long fireModeStartMs = 0;

unsigned long gasStartMs = 0;
unsigned long gasClearStartMs = 0;

unsigned long hornUntil = 0;
unsigned long manualPumpUntil = 0;
unsigned long lastSensorPrint = 0;

// Bật true nếu muốn xem giá trị cảm biến
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

  bool rawFlameDetected = flameValue == FLAME_DETECTED_LEVEL;
  bool rawGasDetected = gasValue >= GAS_THRESHOLD;

  updateFireMode(rawFlameDetected);
  updateGasMode(rawGasDetected);

  printSensorValue(gasValue, flameValue);

  // Cập nhật đèn, còi, bơm
  handleLights();
  handlePump();
  handleBuzzer();

  // Nếu đang có lửa hoặc gas thì xe phải dừng
  if (fireMode || gasMode) {
    stopMotors();
    return;
  }

  // Không có nguy hiểm thì xe chạy bình thường
  controlMotors(moveCommand);
}

// =====================================================
// ĐỌC LỆNH BLUETOOTH
// =====================================================
void readBluetoothCommand() {
  while (mySerial.available()) {
    command = mySerial.read();

    // Bỏ qua ký tự xuống dòng hoặc khoảng trắng nếu app có gửi
    if (command == '\n' || command == '\r' || command == ' ') {
      continue;
    }

    // Bỏ qua số từ joystick như F99, B99, L60, R60
    // Làm vậy để xe không bị giật/delay do đọc nhầm số thành lệnh tốc độ
    if (command >= '0' && command <= '9') {
      continue;
    }

    if (command != lastCommand) {
      Serial.print("Lenh nhan duoc: ");
      Serial.println(command);
      lastCommand = command;
    }

    // ================== LỆNH DI CHUYỂN ==================
    if (command == 'F' || command == 'B' || command == 'L' || command == 'R' ||
    command == 'G' || command == 'H' || command == 'I' || command == 'J' ||
        command == 'S') {
      moveCommand = command;
    }

    // ================== ĐÈN TRƯỚC U/u ==================
    else if (command == 'U') {
      headlightOn = true;
    }
    else if (command == 'u') {
      headlightOn = false;
    }

    // ================== ĐÈN SAU V/v ==================
    else if (command == 'V') {
      taillightOn = true;
    }
    else if (command == 'v') {
      taillightOn = false;
    }

    // ================== ĐÈN ĐỖ W/w ==================
    else if (command == 'W') {
      parkingLightOn = true;
    }
    else if (command == 'w') {
      parkingLightOn = false;
    }

    // ================== ĐÈN CẢNH BÁO X/x ==================
    else if (command == 'X') {
      hazardLightOn = true;
    }
    else if (command == 'x') {
      hazardLightOn = false;
    }

    // ================== CÒI Y ==================
    else if (command == 'Y') {
      // Nếu app gửi Y liên tục khi giữ nút thì còi sẽ kêu liên tục.
      // Nếu app chỉ gửi một lần thì còi kêu ngắn 300ms.
      hornUntil = millis() + 300;
    }
    else if (command == 'y') {
      hornUntil = 0;
    }

    // ================== SPECIAL FUNCTION Z ==================
    else if (command == 'Z') {
      // Test bơm nước thủ công trong 3 giây
      manualPumpUntil = millis() + 3000;
      stopMotors();
      moveCommand = 'S';
      Serial.println("Test bom nuoc 3 giay");
    }
  }

  // Nếu có nối chân STATE HC-05 vào D8, có thể mở đoạn này
  /*
  if (digitalRead(BT_STATE_PIN) == LOW) {
    moveCommand = 'S';
  }
  */
}

// =====================================================
// XỬ LÝ CHẾ ĐỘ PHÁT HIỆN LỬA
// =====================================================
void updateFireMode(bool rawFlameDetected) {
  unsigned long now = millis();

  if (!fireMode) {
    if (rawFlameDetected) {
      if (flameStartMs == 0) {
        flameStartMs = now;
      }

      if (now - flameStartMs >= FIRE_CONFIRM_MS) {
        fireMode = true;
        fireModeStartMs = now;
        flameClearStartMs = 0;

        stopMotors();
        moveCommand = 'S';

        Serial.println("CANH BAO: Phat hien lua! Dung xe va bat bom nuoc.");
      }
    } else {
      flameStartMs = 0;
    }
  }

  else {
    if (rawFlameDetected) {
      flameClearStartMs = 0;
    } else {
      if (flameClearStartMs == 0) {
        flameClearStartMs = now;
      }

      bool clearEnough = now - flameClearStartMs >= FIRE_CLEAR_MS;
      bool pumpLongEnough = now - fireModeStartMs >= PUMP_MIN_MS;

      if (clearEnough && pumpLongEnough) {
        fireMode = false;
        flameStartMs = 0;
        flameClearStartMs = 0;

        stopMotors();
        moveCommand = 'S';

        Serial.println("Lua da het on dinh. Tat bom va quay lai che do dieu khien.");
      }
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
      if (gasStartMs == 0) {
        gasStartMs = now;
      }

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
      if (gasClearStartMs == 0) {
        gasClearStartMs = now;
      }

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
// Theo đúng app:
// F: tiến
// B: lùi
// L: xoay trái
// R: xoay phải
// G: tiến trái
// H: tiến phải
// I: lùi trái
// J: lùi phải
// S: dừng
// =====================================================
void controlMotors(char state) {
  if (vSpeed <= 0) {
    stopMotors();
    return;
  }

  int turnSpeed = (vSpeed * 75) / 100;

  if (turnSpeed < 130) {
    turnSpeed = 130;
  }

  if (state == 'F') {
    // Tiến
    analogWrite(MOTOR_A1_PIN, vSpeed);
    analogWrite(MOTOR_A2_PIN, 0);
    analogWrite(MOTOR_B1_PIN, vSpeed);
    analogWrite(MOTOR_B2_PIN, 0);
  }

  else if (state == 'B') {
    // Lùi
    analogWrite(MOTOR_A1_PIN, 0);
    analogWrite(MOTOR_A2_PIN, vSpeed);
    analogWrite(MOTOR_B1_PIN, 0);
    analogWrite(MOTOR_B2_PIN, vSpeed);
  }

  else if (state == 'L') {
    // Xoay trái tại chỗ
    analogWrite(MOTOR_A1_PIN, vSpeed);
    analogWrite(MOTOR_A2_PIN, 0);
    analogWrite(MOTOR_B1_PIN, 0);
    analogWrite(MOTOR_B2_PIN, vSpeed);
  }

  else if (state == 'R') {
    // Xoay phải tại chỗ
    analogWrite(MOTOR_A1_PIN, 0);
    analogWrite(MOTOR_A2_PIN, vSpeed);
    analogWrite(MOTOR_B1_PIN, vSpeed);
    analogWrite(MOTOR_B2_PIN, 0);
  }

  else if (state == 'G') {
    // Tiến trái
    analogWrite(MOTOR_A1_PIN, vSpeed);
    analogWrite(MOTOR_A2_PIN, 0);
    analogWrite(MOTOR_B1_PIN, turnSpeed);
    analogWrite(MOTOR_B2_PIN, 0);
  }

  else if (state == 'H') {
    // Tiến phải
    analogWrite(MOTOR_A1_PIN, turnSpeed);
    analogWrite(MOTOR_A2_PIN, 0);
    analogWrite(MOTOR_B1_PIN, vSpeed);
    analogWrite(MOTOR_B2_PIN, 0);
  }

  else if (state == 'I') {
    // Lùi trái
    analogWrite(MOTOR_A1_PIN, 0);
    analogWrite(MOTOR_A2_PIN, vSpeed);
    analogWrite(MOTOR_B1_PIN, 0);
    analogWrite(MOTOR_B2_PIN, turnSpeed);
  }

  else if (state == 'J') {
    // Lùi phải
    analogWrite(MOTOR_A1_PIN, 0);
    analogWrite(MOTOR_A2_PIN, turnSpeed);
    analogWrite(MOTOR_B1_PIN, 0);
    analogWrite(MOTOR_B2_PIN, vSpeed);
  }

  else {
    stopMotors();
  }
}

// =====================================================
// DỪNG MOTOR
// =====================================================
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

  // Khi có cảnh báo hoặc bật hazard, đèn nhấp nháy
  if (hazardLightOn || fireMode || gasMode) {
    digitalWrite(HEADLIGHT_PIN, blinkState ? HIGH : LOW);
    digitalWrite(TAILLIGHT_PIN, blinkState ? HIGH : LOW);
    return;
  }

  // Đèn đỗ bật cả đèn trước và sau
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

  // Khi có lửa hoặc gas thì còi báo tự động
  if (fireMode || gasMode) {
    bool beepState = ((now / 120) % 2 == 0);
    digitalWrite(BUZZER_PIN, beepState ? BUZZER_ON : BUZZER_OFF);
    return;
  }

  // Bình thường chỉ bấm Y thì còi mới kêu
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
  if (!DEBUG_SENSOR) {
    return;
  }

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