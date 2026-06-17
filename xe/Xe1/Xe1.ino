#include <SoftwareSerial.h>

// =====================================================
// 1. KHAI BÁO CHÂN BLUETOOTH HC-05
// =====================================================
const byte BT_RX_PIN = 2;   // Arduino RX nhận từ TX của HC-05
const byte BT_TX_PIN = 3;   // Arduino TX gửi đến RX của HC-05
SoftwareSerial mySerial(BT_RX_PIN, BT_TX_PIN);

// =====================================================
// 2. KHAI BÁO CHÂN CẢM BIẾN
// =====================================================
const byte FLAME_SENSOR_PIN = 4;   // Cảm biến lửa chân DO
const byte GAS_SENSOR_PIN   = A0;  // Cảm biến gas chân AO

// =====================================================
// 3. KHAI BÁO CHÂN ĐỘNG CƠ (Phải là chân PWM: 5, 6, 9, 10)
// =====================================================
const byte MOTOR_B1_PIN = 5;
const byte MOTOR_B2_PIN = 6;
const byte MOTOR_A1_PIN = 9;
const byte MOTOR_A2_PIN = 10;

// =====================================================
// 4. KHAI BÁO CHÂN THIẾT BỊ PHỤ
// =====================================================
const byte BUZZER_PIN   = 7;   // Còi buzzer
const byte BT_STATE_PIN = 8;   // Chân STATE của HC-05 nếu có dùng
const byte RELAY_PIN    = 12;  // Relay

// =====================================================
// 5. CẤU HÌNH RELAY (Đổi HIGH/LOW tùy loại relay bạn mua)
// =====================================================
const byte RELAY_ON  = HIGH;
const byte RELAY_OFF = LOW;

// =====================================================
// 6. CẤU HÌNH CẢM BIẾN
// =====================================================
const int GAS_THRESHOLD = 400;          // Ngưỡng gas, cần chỉnh theo thực tế
const byte FLAME_DETECTED_LEVEL = LOW;  // Nhiều cảm biến lửa phát hiện lửa = LOW

// =====================================================
// 7. BIẾN ĐIỀU KHIỂN
// =====================================================
int vSpeed = 200;

char command = 'S';       // Lệnh vừa nhận từ Bluetooth
char moveCommand = 'S';   // Lệnh di chuyển hiện tại
char lastCommand = ' ';

bool manualHorn = false;   // Trạng thái còi bật/tắt bằng app
bool manualRelay = false;  // Trạng thái relay bật/tắt bằng app

unsigned long lastSensorPrint = 0;

// =====================================================
// SETUP
// =====================================================
void setup() {
  pinMode(MOTOR_A1_PIN, OUTPUT);
  pinMode(MOTOR_A2_PIN, OUTPUT);
  pinMode(MOTOR_B1_PIN, OUTPUT);
  pinMode(MOTOR_B2_PIN, OUTPUT);

  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(RELAY_PIN, OUTPUT);

  pinMode(BT_STATE_PIN, INPUT);
  pinMode(FLAME_SENSOR_PIN, INPUT);
  pinMode(GAS_SENSOR_PIN, INPUT);

  Serial.begin(9600);
  mySerial.begin(9600);

  digitalWrite(RELAY_PIN, RELAY_OFF);
  digitalWrite(BUZZER_PIN, LOW);
  stopMotors();

  Serial.println("He thong da san sang!");

  // Tiếng chíp báo khởi động
  digitalWrite(BUZZER_PIN, HIGH); delay(100);
  digitalWrite(BUZZER_PIN, LOW);  delay(100);
  digitalWrite(BUZZER_PIN, HIGH); delay(100);
  digitalWrite(BUZZER_PIN, LOW);
}

// =====================================================
// LOOP
// =====================================================
void loop() {
  // 1. Đọc lệnh Bluetooth
  readBluetoothCommand();

  // 2. Đọc cảm biến gas và cảm biến lửa
  int gasValue = analogRead(GAS_SENSOR_PIN);
  int flameValue = digitalRead(FLAME_SENSOR_PIN);

  bool gasDetected = gasValue >= GAS_THRESHOLD;
  bool flameDetected = flameValue == FLAME_DETECTED_LEVEL;
  bool dangerDetected = gasDetected || flameDetected;

  // 3. In giá trị cảm biến ra Serial Monitor mỗi 1 giây
  if (millis() - lastSensorPrint >= 1000) {
    Serial.print("Gas value: ");
    Serial.print(gasValue);
    Serial.print(" | Flame value: ");
    Serial.print(flameValue);
    Serial.print(" | Danger: ");
    Serial.println(dangerDetected ? "YES" : "NO");

    lastSensorPrint = millis();
  }

  // 4. Nếu phát hiện gas hoặc lửa thì ưu tiên an toàn
  if (dangerDetected) {
    stopMotors();                        // Dừng xe ngay
    digitalWrite(RELAY_PIN, RELAY_ON);  // Bật relay dập lửa/quạt hút
    alarmBuzzer();                      // Còi cảnh báo nhanh
    return;                             // Thoát vòng lặp hiện tại, không chạy lệnh dưới
  }

  // 5. Nếu không nguy hiểm thì xe chạy bình thường
  controlMotors(moveCommand);

  // 6. Điều khiển còi
  handleBuzzer();

  // 7. Điều khiển relay bằng app nếu không có nguy hiểm
  if (manualRelay) {
    digitalWrite(RELAY_PIN, RELAY_ON);
  } else {
    digitalWrite(RELAY_PIN, RELAY_OFF);
  }
}

// =====================================================
// HÀM ĐỌC LỆNH BLUETOOTH
// =====================================================
void readBluetoothCommand() {
  if (mySerial.available()) {
    command = mySerial.read();

    if (command != lastCommand) {
      Serial.print("Lenh nhan duoc: ");
      Serial.println(command);
      lastCommand = command;
    }

    // Điều chỉnh tốc độ
    if (command == '0')      vSpeed = 0;
    else if (command == '4') vSpeed = 100;
    else if (command == '6') vSpeed = 155;
    else if (command == '7') vSpeed = 180;
    else if (command == '8') vSpeed = 200;
    else if (command == '9') vSpeed = 230;
    else if (command == 'q') vSpeed = 255;

    // Lệnh di chuyển xe
    else if (command == 'F' || command == 'G' || command == 'I' ||
             command == 'B' || command == 'H' || command == 'J' ||
             command == 'R' || command == 'L' || command == 'S') {
      moveCommand = command;
    }

    // Bật/tắt còi bằng app
    else if (command == 'V')  manualHorn = true;
    else if (command == 'v')  manualHorn = false;

    // Bật/tắt relay bằng app
    else if (command == 'X')  manualRelay = true;
    else if (command == 'x')  manualRelay = false;
  }
}

// =====================================================
// HÀM ĐIỀU KHIỂN ĐỘNG CƠ
// =====================================================
void controlMotors(char state) {
  int turnSpeed = vSpeed / 2;
  if (vSpeed > 0 && turnSpeed < 80) {
    turnSpeed = 80; // Giữ tốc độ tối thiểu để bánh không bị kẹt khi rẽ
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
    // Tiến rẽ trái
    analogWrite(MOTOR_A1_PIN, vSpeed);
    analogWrite(MOTOR_A2_PIN, 0);
    analogWrite(MOTOR_B1_PIN, turnSpeed);
    analogWrite(MOTOR_B2_PIN, 0);
  }
  else if (state == 'I') {
    // Tiến rẽ phải
    analogWrite(MOTOR_A1_PIN, turnSpeed);
    analogWrite(MOTOR_A2_PIN, 0);
    analogWrite(MOTOR_B1_PIN, vSpeed);
    analogWrite(MOTOR_B2_PIN, 0);
  }
  else if (state == 'H') {
    // Lùi rẽ trái
    analogWrite(MOTOR_A1_PIN, 0);
    analogWrite(MOTOR_A2_PIN, vSpeed);
    analogWrite(MOTOR_B1_PIN, 0);
    analogWrite(MOTOR_B2_PIN, turnSpeed);
  }
  else if (state == 'J') {
    // Lùi rẽ phải
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
// HÀM DỪNG XE
// =====================================================
void stopMotors() {
  analogWrite(MOTOR_A1_PIN, 0);
  analogWrite(MOTOR_A2_PIN, 0);
  analogWrite(MOTOR_B1_PIN, 0);
  analogWrite(MOTOR_B2_PIN, 0);
}

// =====================================================
// HÀM XỬ LÝ CÒI BÌNH THƯỜNG (Kêu ngắt quãng chậm)
// =====================================================
void handleBuzzer() {
  if (manualHorn || moveCommand == 'B' || moveCommand == 'H' || moveCommand == 'J') {
    if ((millis() / 200) % 2 == 0) {
      digitalWrite(BUZZER_PIN, HIGH);
    } else {
      digitalWrite(BUZZER_PIN, LOW);
    }
  } else {
    digitalWrite(BUZZER_PIN, LOW);
  }
}

// =====================================================
// HÀM CÒI CẢNH BÁO GAS/LỬA (Kêu ngắt quãng nhanh dồn dập)
// =====================================================
void alarmBuzzer() {
  if ((millis() / 100) % 2 == 0) {
    digitalWrite(BUZZER_PIN, HIGH);
  } else {
    digitalWrite(BUZZER_PIN, LOW);
  }
}