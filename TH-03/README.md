# Bài 3 — Điều khiển thiết bị an toàn

**Họ và tên:** Trần Thành Đạt · **MSSV:** 23110303 · **Nhóm**: Cá nhân


## File trong repo này
- `control.ino` — Firmware máy trạng thái DỪNG / CHẠY / LỖI (tích hợp ngắt ngoài và timer phần cứng).
- `fsm_diagram.png` — Sơ đồ trạng thái của hệ thống.
- `secrets.h.example`- Cấu hình wifi

## ⚙️ Thông số đã chọn
- **Thời gian chạy tối đa (`MAX_RUN_MS`):** 20 giây *(trong khoảng gợi ý 10–30 s)*.
- **Thời gian giữ trạng thái LỖI trước khi về DỪNG (`FAULT_HOLD_MS`):** 5 giây.
- **Thời gian chống dội nút (`DEBOUNCE_MS`):** 50 ms.
- **Chu kỳ timer phần cứng:** 2 giây.
- **Mức Duty Cycle PWM (thang 8 bit):** 
  - 25% (64)
  - 50% (127)
  - 90% (229)

---

## 4.1 Cấu hình phần cứng
- **Tải:** Bơm DC, điều khiển thông qua driver A4950ELJTR.
- **GPIO5 (`MOTOR1_INT1`):** Đầu vào IN1 của A4950, xuất tín hiệu PWM bằng LEDC kênh 0, tần số 5 kHz, độ phân giải 8 bit.
- **GPIO17 (`MOTOR1_INT2`):** Đầu vào IN2 của A4950, luôn giữ ở mức `LOW` khi bơm chạy.
- **GPIO36:** Nút nhấn, sử dụng ngắt ngoài. Chân này không có điện trở kéo lên nội (internal pull-up), do đó cần lắp thêm điện trở kéo lên bên ngoài.
- **GPIO26:** LED trạng thái.
- **Giao tiếp I2C:** SDA = `GPIO33`, SCL = `GPIO32`, dùng để đọc cảm biến nhiệt độ và độ ẩm từ Arduino slave tại địa chỉ `0x08`.
- **Trạng thái khởi động (`setup()`):** Thiết lập GPIO17 và LED về mức `LOW`, đồng thời đưa PWM về `0` trước khi kích hoạt ngắt và timer.

---

## 4.2 Ngắt ngoài
- **Hàm xử lý ngắt (ISR - `btnISR`):** Chỉ thực hiện tăng biến đếm `volatile` (`btnRawCount`), tuyệt đối không gọi hàm `Serial.print()` hay `delay()`.
- **Chống dội (Debounce):** Được thực hiện hoàn toàn ngoài ISR. Vòng lặp `loop()` ghi nhận thời điểm xuất hiện cạnh mới cuối cùng và chỉ công nhận một lần bấm hợp lệ khi tín hiệu đã duy trì ổn định trong 50 ms. Phương pháp này hoàn toàn sử dụng thời gian phần mềm thay vì phần cứng.

---

## 4.3 Timer phần cứng và PWM
- **Timer 0:** Cấu hình với prescaler 80 (đếm tần số 1 MHz), giá trị alarm 2.000.000 µs (2 giây), chế độ tự nạp lại (auto-reload).
- **ISR Timer:** Chỉ thực hiện đặt cờ `timerFlag`. 
- **Xử lý trong `loop()`:** Mỗi khi cờ được xử lý, chương trình tiến hành đọc cảm biến, cập nhật máy trạng thái và in ra một dòng log dạng CSV có chứa cột `timer_cycle_ms`.

---

## 4.4 Máy trạng thái và các ca thử nghiệm
### Thiết kế máy trạng thái:
`DỪNG` ⇄ `CHẠY` *(bằng nút nhấn hoặc lệnh web)* -> `LỖI` *(khi hết `MAX_RUN_MS` hoặc cảm biến không hợp lệ)* ->`DỪNG` *(sau `FAULT_HOLD_MS`)*.

- Khi đang ở trạng thái **LỖI**, mọi lệnh **CHẠY** đều bị bỏ qua.
- Khi ở trạng thái **DỪNG**, bơm luôn ở trạng thái tắt.

### Các ca thử nghiệm (Đầu vào và Kỳ vọng):
* **Ca 1:** Gửi lệnh bật khi đang ở trạng thái **DỪNG** và chưa hết giờ -> **CHẠY**.
* **Ca 2:** Để hệ thống chạy đến khi vượt quá `MAX_RUN_MS` mà không gửi lệnh tắt -> **LỖI** (TIMEOUT) -> **DỪNG**.
* **Ca 3:** Rút cảm biến trong lúc đang **CHẠY** -> **LỖI** (SENSOR_INVALID) -> **DỪNG**.

---

## 🎛️ Lệnh điều khiển
- **Nút nhấn vật lý:** Chuyển đổi qua lại giữa `DỪNG ⇄ CHẠY`.
- **Giao diện Web (`http://IP/`):**
  - `/api/cmd?action=start|stop`
  - `/api/duty?pct=25|50|90`
  - `/api/status`
- **Giao tiếp Serial (Baudrate 115200):**
  - `'1'`, `'2'`, `'3'`: Thiết lập mức Duty Cycle tương ứng.
  - `'i'`: Quét bus I2C.
  - `'s'`: In trạng thái hiện tại của hệ thống.