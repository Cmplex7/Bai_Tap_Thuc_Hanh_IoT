# Bài 1 — Sơ đồ nhà kính theo mẫu

Họ tên: Trần Thành Đạt
MSSV: 23110303

## 1. Cấu hình của nhóm

| Mục | Mẫu để tham khảo | Cá nhân điền |
|---|---|---|
| Bộ điều khiển | ESP32, node01 | node23110303 |
| Cảm biến | DHT11 | DHT11 (Chân DATA_RH - GPIO23) |
| Đầu ra thử | LED trên breadboard | LED báo |
| Nơi chạy broker/dịch vụ | PC của nhóm | PC cá nhân |
| Topic số đo | nhom01/node01/telemetry | 23110303/node23110303/telemetry |
| Topic lệnh | nhom01/node01/cmd | 23110303/node23110303/cmd |
| Topic xác nhận | nhom01/node01/ack | 23110303/node23110303/ack |

## 2. Năm sơ đồ theo mẫu

- Sơ đồ chức năng: .\UC-01_Chuc_nang.png
- Luồng số đo: .SD-01_So_Do_Sequence.png
- Luồng lệnh: .\SD-02_Lenh_Sequence.png
- Quy trình xử lý lệnh (Activity): .\AD-01_Quy_Trinh_Activity.png
- Triển khai trên kit (Deployment): .\DD-01_Trien_Khai_Deployment.png
- Tệp nguồn sơ đồ: .\01-chuc-nang.puml, .\02-luong-so-do.puml, .\03-luong-lenh.puml, .\04-hoat-dong.puml, .\05-trien-khai.puml

## 3. Bảng chân kết nối nhà kính (Schematic trang 4 - ESP32)

| Tên thiết bị / Linh kiện | Ký hiệu / Đường tín hiệu | Chân GPIO ESP32 | Linh kiện trung gian 
|---|---|---|---|---|
| Cảm biến Nhiệt độ/Độ ẩm | `DATA_RH` | GPIO 23 | Điện trở kéo lên `R27` | 
| Màn hình / Cảm biến I²C | `SDA` / `SCL` | GPIO 21 / GPIO 22 | Điện trở đệm dòng | 
| Đèn LED đỏ chỉ thị | `LED_R` | GPIO 25 | Transistor `MMBT3904` (`Q18`) | 
| Đèn LED xanh chỉ thị| `LED_G` | GPIO 26 | Transistor `MMBT3904` (`Q12`) |
| Động cơ 1 (MOTOR 1) | `MOTOR1_INT1` / `MOTOR1_INT2` | GPIO 32 / GPIO 33 | IC công suất `44950LITR` (`U2`) | 
| Động cơ 2 (MOTOR 2) | `MOTOR2_INT1` / `MOTOR2_INT2` | GPIO 18 / GPIO 19 | IC công suất `44950LITR` (`U3`) | 
| Dải LED (ARRAY_LED3) | `ARRAY_LED3` | GPIO 14 | Mạch đệm `N531` (`U11`) | 
| Dải LED (ARRAY_LED4) | `ARRAY_LED4` | GPIO 13 | Mạch đệm `N531` (`U12`) | 

## 4. Giải thích ngắn theo mẫu

- **Hai họ vi điều khiển trong nhà kính là gì ?**: ESP32 và STM32 (hoặc PIC/ATmega).
- **Cảm biến nhiệt độ độ ẩm phía ESP32 tên gì ?**: DHT11
- **DATA_RH nối GPIO nào ?**: nối với GPIO 23 của ESP32 
- **Cảm biến này dùng kiểu giao tiếp nào ?**: Dùng chuẩn giao tiếp 1-Wire
- **SDA nối GPIO nào ?**: SDA được nối với GPIO 21 của ESP32
- **SCL nối GPIO nào ?**: SCL được nối với GPIO 22 của ESP32
- **MOTOR1_INT1 nối GPIO nào ?**: được nối với chân GPIO 32 của ESP32
- **MOTOR2_INT2 nối GPIO nào ?**: được nối với chân GPIO 33 của ESP32
- **Truy vết từ hai GPIO tới động cơ. Vì sao không nối tải trực tiếp vào GPIO ?**: Chân GPIO của ESP32 chỉ chịu được dòng max 20-40mA, trong khi động cơ tiêu thụ dòng điện hoạt động và dòng khởi động lớn hơn rất nhiều (vài trăm mA -> Ampe). Nối trực tiếp sẽ gây tụt áp, cháy hỏng chân vi điều khiển.
- **Đường tín hiệu LED_R (điều khiển đèn LED đỏ) nối với chân GPIO nào của ESP32 (U1) ?**: nối vào chân GPIO 25 của ESP32
- **Đường tín hiệu LED_G (điều khiển đèn LED xanh) nối với chân GPIO nào của ESP32 (U1) ?**: nối vào chân GPIO 26 của ESP32
- **LED đỏ D3 và LED xanh D16 được điều khiển trực tiếp từ GPIO hay qua linh kiện nào? Truy vết một nhánh.**: Các LED báo không được điều khiển trực tiếp qua GPIO mà phải đi qua Transitor kích đệm. Truy nhánh: GPIO25 -> chân Base của Transitor MMBT3904. Transitor mở: LED_RED D3 -> Transitor -> GND
- **Đường tín hiệu MOTOR2_INT1 nối với chân GPIO số mấy của ESP32 ?**: được nối với chân GPIO 18 của ESP32
- **Đường tín hiệu MOTOR2_INT2 nối với chân GPIO số mấy của ESP32 ?**: được nối với chân GPIO 19 của ESP32
- **MOTOR2 đi qua IC nào và ra đầu nối nào? Vì sao không nhầm với MOTOR1 ?**: MOTOR2 đi qua IC lái công suất 44950LITR rồi mới xuất ra cọc đấu nối dành riêng cho MOTOR2. Không được nhầm với MOTOR1 vì MOTOR1 sử dụng cặp GPIO 32/33 đi qua IC công suất U2, có đường tín hiệu & vị trí khác nhau trên mạch nguyên lý.
- **Phân biệt LED báo đỏ/xanh với hai nhánh ARRAY_LED. ARRAY_LED3/4 nối GPIO nào và đi tới nhánh nào ?**: LED_R và LED_G là LED đơn hiển thị trạng thái, công suất nhỏ. ARRAY_LED3 & ARRAY_LED4 là các dải LED công suất lớn hơn, phải đi qua IC mạch đệm công suất N531. Kết nối: ARRAY_LED3 nối vào GPIO 14; ARRAY_LED4 nối vào GPIO 13
- **Theo nhà kính này, một số đo DHT11 được đọc ở chân nào? Nếu bổ sung điều khiển MOTOR1 qua MQTT thì lệnh phải đi tới hai GPIO nào, qua mạch nào ?**: Số đo DHT11 được vi điều khiển đọc qua GPIO 23. Nếu bổ sung điều khiển qua MQTT: Lệnh MQTT từ Broker được ESP32 xử lý, xuất tín hiệu ra GPIO 32 và GPIO 33, đi qua IC cầu H 44950LITR để điều khiển đóng/mở/đảo chiều động cơ 1.

## 5. Phần kiểm chứng để lại cho các bài sau

- Trên kit/PC/RPi: Cấu hình Mosquitto MQTT Broker, xây dựng dịch vụ backend Python và nạp chương trình lên bo ESP32.
- Trên nhà kính chung: Đấu nối phần cứng thật, kiểm tra công suất kích lái động cơ và đo đạc tín hiệu thực địa.
