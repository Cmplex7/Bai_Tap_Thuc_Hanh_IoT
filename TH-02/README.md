# Bài 2 — Đọc và kiểm tra cảm biến
Họ tên: Trần Thành Đạt · MSSV: 23110303 · Nhóm: Cá nhân

## File trong repo này
- `sensor(delay).ino` — bản dùng delay()
- `sensor(millis).ino` — bản dùng millis()
- `data.csv`
- `bieudo.png`

## 1 Cấu hình (setup)
- **Môi trường:** Em chọn cách A (đo ngắn) sử dụng phần mềm mô phỏng Wokwi. 
- **Cảm biến và chân nối:** Sử dụng cảm biến nhiệt độ ẩm DHT22 (mô phỏng cho DHT11). Chân tín hiệu được nối vào GPIO 14 của ESP32. 
- **Rèn thêm:** Dùng một biến trở nối vào GPIO 34 để kiểm tra ADC và một module LCD1602 nối vào GPIO 21, GPIO 22 để quét I2C. 
- **Chu kỳ đọc:** 2000ms (2 giây) dựa theo khuyến nghị datasheet của DHT để tránh treo cảm biến.

## 2. Bảng sơ đồ chân cắm ESP32 

| Tên thiết bị / Linh kiện | Tín hiệu mạch | Chân GPIO |
| **Cảm biến Nhiệt độ / Độ ẩm** | DATA_RH | GPIO 14 |
| **Giao tiếp I2C (Màn hình/Cảm biến)** | SDA / SCL | GPIO 21 / GPIO 22 |
| **Biến trở** | (ADC_IN giả lập) | GPIO 34 |
| **Dải đèn chiếu sáng 3** | ARRAY_LED3 | GPIO 14 |

## 3 Cách báo lỗi
- Khi tiến hành ngắt kết nối cảm biến, khi chương trình đang chạy (Simulator trên Wokwi) không cho phép chỉnh sửa sơ đồ mạch / dây cắm nên em dừng chương trình và tiến hành xóa dây cắm
khỏi cảm biến. Kết quả trả về lỗi 'NAN'.
- **Dòng log thực tế trích xuất từ Serial Monitor:**
`2585,nan,nan,C,0`

## 4 Bộ lọc đã chọn
- **Bộ lọc áp dụng:** Trung bình trượt (Moving Average) với kích thước cửa sổ 5 mẫu (window size = 5).
- **Độ mượt:** Trong dữ liệu gốc, em tiến hành thay đổi nhiệt độ liên tục trên cảm biến khiến nhiệt độ biến động rất mạnh và biểu đồ có các đỉnh nhọn bất thường. Sau khi lọc, các dao động mạnh bị triệt tiêu, đường kẻ trở nên mềm mại hơn.
- **Độ trễ :** Do phản ứng dựa trên trung bình của 5 mẫu trước đó, đường tín hiệu lọc bị trễ pha khoảng 4-6 giây so với tín hiệu gốc mỗi khi giá trị thực tế thay đổi đột ngột.

## 5 Kết quả thí nghiệm lập lịch
- **Bản A (dùng delay):** Khi gõ phím gửi qua Serial Monitor trong lúc chờ `delay(2000)`, vi điều khiển bị đứng im và không ghi nhận trạng thái ngay lập tức. Phải chờ hết 2 giây thì chương trình mới phản hồi phím bấm.
- **Bản B (dùng millis):** Hàm đọc không chặn. Vi điều khiển phản hồi tác vụ gõ phím từ Serial Monitor ngay lập tức trong lúc vẫn chờ đủ chu kỳ 2 giây.
