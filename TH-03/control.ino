#include <Arduino.h>
#include <Wire.h>

// --- Thông số cấu hình ---
const unsigned long MAX_RUN_MS = 20000;      // Thời gian chạy tối đa: 20 giây
const unsigned long FAULT_HOLD_MS = 5000;    // Thời gian giữ trạng thái LỖI: 5 giây
const unsigned long DEBOUNCE_MS = 50;        // Thời gian chống dội nút nhấn: 50 ms

// --- Định nghĩa chân phần cứng ---
const int MOTOR_INT1 = 5;    // GPIO5 (MOTOR1_INT1) - PWM LEDC kênh 0
const int MOTOR_INT2 = 17;   // GPIO17 (MOTOR1_INT2) - Mức LOW khi chạy
const int BTN_PIN = 36;      // GPIO36 - Nút nhấn (ngắt ngoài, cần kéo lên ngoài)
const int LED_PIN = 26;      // GPIO26 - LED trạng thái
const int I2C_SDA = 33;      // SDA cảm biến slave (0x08)
const int I2C_SCL = 32;      // SCL cảm biến slave (0x08)
const uint8_t SENSOR_ADDR = 0x08;

// --- Cấu hình PWM (LEDC) ---
const int PWM_CHANNEL = 0;
const int PWM_FREQ = 5000;   // 5 kHz
const int PWM_RES = 8;       // 8 bit (0 - 255)

// Ba mức duty cycle yêu cầu: 25% (64), 50% (127), 90% (229)
int currentDutyIndex = 0;
int dutyValues[3] = {64, 127, 229};
int dutyPercents[3] = {25, 50, 90};

// --- Trợ giúp máy trạng thái ---
enum State { STATE_STOP, STATE_RUN, STATE_FAULT };
State currentState = STATE_STOP;

unsigned long stateStartTime = 0;
unsigned long lastDebounceTime = 0;
volatile unsigned long btnRawCount = 0;
unsigned long lastProcessedBtnCount = 0;

// Timer phần cứng
hw_timer_t *timer = NULL;
volatile bool timerFlag = false;
unsigned long timerCycleCount = 0;

// Hàm phục vụ ngắt ngoài (ISR) cho nút nhấn
void IRAM_ATTR btnISR() {
  btnRawCount++;
}

// Hàm phục vụ ngắt Timer phần cứng (mỗi 2 giây)
void IRAM_ATTR onTimer() {
  timerFlag = true;
}

void setup() {
  Serial.begin(115200);

  // Khởi tạo GPIO
  pinMode(MOTOR_INT2, OUTPUT);
  digitalWrite(MOTOR_INT2, LOW);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  pinMode(BTN_PIN, INPUT); // Chân 36 không có pull-up nội, cần điện trở kéo lên ngoài

  // Cấu hình PWM LEDC kênh 0
  ledcSetup(PWM_CHANNEL, PWM_FREQ, PWM_RES);
  ledcAttachPin(MOTOR_INT1, PWM_CHANNEL);
  ledcWrite(PWM_CHANNEL, 0); // Đặt PWM về 0 ban đầu

  // Khởi tạo I2C
  Wire.begin(I2C_SDA, I2C_SCL);

  // Cấu hình ngắt ngoài cho nút nhấn (bắt cạnh xuống FALLING hoặc thay đổi)
  attachInterrupt(digitalPinToInterrupt(BTN_PIN), btnISR, FALLING);

  // Cấu hình Timer phần cứng 0: Prescaler = 80 (1 MHz -> đếm 1µs), Alarm = 2.000.000 µs (2s), Auto-reload = true
  timer = timerBegin(0, 80, true);
  timerAttachInterrupt(timer, &onTimer, true);
  timerAlarmWrite(timer, 2000000, true);
  timerAlarmEnable(timer);

  stateStartTime = millis();
  Serial.println("System Initialized. State: DUNG (STOP)");
}

// Hàm đọc cảm biến qua I2C từ Arduino slave (0x08)
bool readSensor(float &temp, float &hum) {
  Wire.requestFrom((int)SENSOR_ADDR, 4);
  if (Wire.available() >= 4) {
    uint8_t t_int = Wire.read();
    uint8_t t_dec = Wire.read();
    uint8_t h_int = Wire.read();
    uint8_t h_dec = Wire.read();
    temp = t_int + (t_dec / 100.0);
    hum = h_int + (h_dec / 100.0);
    return true;
  }
  return false;
}

void loop() {
  unsigned long currentMillis = millis();

  // 1. Xử lý chống dội nút nhấn ngoài ISR
  if (btnRawCount != lastProcessedBtnCount) {
    if (currentMillis - lastDebounceTime > DEBOUNCE_MS) {
      lastProcessedBtnCount = btnRawCount;
      lastDebounceTime = currentMillis;

      // Xử lý sự kiện bấm nút chuyển đổi trạng thái
      if (currentState == STATE_STOP) {
        currentState = STATE_RUN;
        stateStartTime = currentMillis;
        Serial.println("[EVENT] Nút nhấn: Chuyển sang CHẠY");
      } else if (currentState == STATE_RUN) {
        currentState = STATE_STOP;
        Serial.println("[EVENT] Nút nhấn: Chuyển về DỪNG");
      } else if (currentState == STATE_FAULT) {
        Serial.println("[EVENT] Nút nhấn bị bỏ qua do đang ở trạng thái LỖI.");
      }
    }
  }

  // 2. Xử lý lệnh từ Serial giao tiếp
  if (Serial.available() > 0) {
    char cmd = Serial.read();
    if (cmd == '1') { currentDutyIndex = 0; Serial.println("Đổi Duty: 25%"); }
    if (cmd == '2') { currentDutyIndex = 1; Serial.println("Đổi Duty: 50%"); }
    if (cmd == '3') { currentDutyIndex = 2; Serial.println("Đổi Duty: 90%"); }
    if (cmd == 'i') { 
      Serial.println("Quét I2C...");
      for(byte addr=1; addr<127; addr++) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
          Serial.print("Tìm thấy thiết bị tại: 0x"); Serial.println(addr, HEX);
        }
      }
    }
    if (cmd == 's') {
      Serial.print("Trạng thái hiện tại: ");
      Serial.println(currentState == STATE_STOP ? "DỪNG" : (currentState == STATE_RUN ? "CHẠY" : "LỖI"));
    }
  }

  // 3. Cập nhật Máy trạng thái & Chấp hành
  float temp = 0, hum = 0;
  bool sensorValid = readSensor(temp, hum);

  switch (currentState) {
    case STATE_STOP:
      digitalWrite(MOTOR_INT2, LOW);
      ledcWrite(PWM_CHANNEL, 0);
      digitalWrite(LED_PIN, LOW);
      break;

    case STATE_RUN:
      // Kiểm tra điều kiện an toàn: Hết thời gian MAX_RUN_MS hoặc cảm biến lỗi -> Chuyển LỖI
      if ((currentMillis - stateStartTime >= MAX_RUN_MS) || !sensorValid) {
        currentState = STATE_FAULT;
        stateStartTime = currentMillis; // Đánh dấu thời điểm bắt đầu đếm thời gian giữ lỗi (FAULT_HOLD_MS)
        Serial.println("[FAULT] Chuyển sang LỖI do Quá thời gian (Timeout) hoặc Mất cảm biến!");
      } else {
        digitalWrite(MOTOR_INT2, LOW);
        ledcWrite(PWM_CHANNEL, dutyValues[currentDutyIndex]);
        digitalWrite(LED_PIN, HIGH); // Bật LED báo trạng thái chạy
      }
      break;

    case STATE_FAULT:
      // Ở trạng thái LỖI, tải luôn tắt, lệnh CHẠY bị bỏ qua
      digitalWrite(MOTOR_INT2, LOW);
      ledcWrite(PWM_CHANNEL, 0);
      digitalWrite(LED_PIN, (currentMillis / 250) % 2); // Nhấp nháy LED báo lỗi

      // Sau 5 giây giữ ở LỖI, tự động chuyển về DỪNG an toàn
      if (currentMillis - stateStartTime >= FAULT_HOLD_MS) {
        currentState = STATE_STOP;
        Serial.println("[INFO] Hết thời gian giữ lỗi, tự động về trạng thái DỪNG an toàn.");
      }
      break;
  }

  // 4. Xử lý Timer phần cứng mỗi 2 giây để in log CSV chuẩn
  if (timerFlag) {
    timerFlag = false;
    timerCycleCount += 2000;

    // In một dòng CSV: timer_cycle_ms, state, duty_pct, temp, hum, sensor_valid
    Serial.print(timerCycleCount);
    Serial.print(",");
    Serial.print(currentState == STATE_STOP ? "DUNG" : (currentState == STATE_RUN ? "CHAY" : "LOI"));
    Serial.print(",");
    Serial.print(dutyPercents[currentDutyIndex]);
    Serial.print(",");
    Serial.print(sensorValid ? String(temp) : "NAN");
    Serial.print(",");
    Serial.print(sensorValid ? String(hum) : "NAN");
    Serial.print(",");
    Serial.println(sensorValid ? "1" : "0");
  }
}