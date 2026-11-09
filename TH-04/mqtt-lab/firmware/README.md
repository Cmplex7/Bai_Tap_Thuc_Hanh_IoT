# Bài 4 — Gửi dữ liệu qua MQTT
Họ tên: Trần Thành Đạt · MSSV: 23110303 · Nhóm: Cá nhân

## File trong repo này
- docker-compose.yml — cấu hình broker (đã bỏ bí mật)
- firmware/ — code FreeRTOS cho ESP32 (PlatformIO)
- topic_payload.md — mô tả topic và cấu trúc payload
- task_queue.png — sơ đồ task/queue và nơi triển khai
- log_10min.txt — log subscribe liên tục 10 phút
- log_disconnect.txt, esp32_disconnect.log — log thử ngắt/nối lại broker
- log_queue_congestion.txt — log thí nghiệm nghẽn queue

## 4.1 Payload thực tế
Node `node01` publish lên topic `ThanhDat/node01/telemetry` mỗi 2 giây (QoS 0).
Một payload thật nhận được từ ESP32 (lấy từ `log_10min.txt`):

{"device_id":"node01","seq":33,"ts":66160,"lat":null,"lon":null,"unit":"deg","sats":0,"rssi":-84,"valid":false}

| Trường | Ý nghĩa |
|---|---|
| `device_id` | Mã thiết bị, khớp với topic. |
| `seq` | Số thứ tự bản tin, tăng 1 mỗi lần gửi; dùng để phát hiện bản tin bị rớt. |
| `ts` | Thời gian tính bằng mili giây từ lúc ESP32 khởi động (66160 ms ≈ 66 giây). |
| `lat`, `lon` | Vĩ độ, kinh độ từ module GPS NEO-6M. `null` vì GPS chưa có dữ liệu thực tế. |
| `unit` | Đơn vị của `lat`, `lon` (độ). |
| `sats` | Số vệ tinh module báo. `0` là giá trị hợp lệ: module vẫn gửi dữ liệu nhưng chưa thấy vệ tinh. |
| `rssi` | Cường độ WiFi mà ESP32 thu được (dBm). |
| `valid` | Cờ chất lượng: `true` chỉ khi GPS có dữ liệu, ngược lại `false`. |

Cách phân biệt `null` và `0`: giá trị không đo được (chưa có vị trí GPS) là `null`,
không dùng 0 để tránh nhầm với tọa độ thật. `sats: 0` là số đo hợp lệ, còn `null`
sẽ xuất hiện nếu module không gửi dữ liệu nào (ví dụ đấu dây sai).

Lưu ý: mô hình của em không có cảm biến nhiệt độ, độ ẩm hay mưa nên node gửi
các số đo thật mà phần cứng có: dữ liệu từ module GPS và RSSI WiFi. Em đặt module trong nhà
nên GPS không bắt được vệ tinh, vì vậy `valid:false` và tọa độ là `null`; đây là kết quả thật.

### Kết quả log 10 phút
Cửa sổ nhận (`mosquitto_sub`) chạy trên cùng PC với broker, trong một terminal riêng. Log lưu ở `log_10min.txt`: nhận 313 bản tin, `seq` từ 45 đến 357,
đúng 313 bản kỳ vọng, không rớt bản tin nào (khoảng 10 phút 26 giây).

## 4.2 Task và queue
Firmware chia thành 3 task FreeRTOS, trao đổi dữ liệu qua một queue có giới hạn:

| Task | Core | Ưu tiên | Việc làm |
|---|---|---|---|
| `sensor` | 1 | 2 | Đọc dữ liệu NMEA từ GPS, mỗi 2 giây đóng gói một bản ghi (seq, ts, tọa độ, sats, rssi, valid) và đẩy vào queue. |
| `mqtt` | 0 | 1 | Giữ kết nối WiFi/MQTT, tự thử nối lại khi mất, lấy bản ghi từ queue và publish. |
| `safety` | 1 | 3 | Chạy theo chu kỳ riêng 500 ms (`vTaskDelayUntil`), xác định trạng thái DUNG/CHAY/LOI theo độ mới của dữ liệu, điều khiển chân an toàn và in heartbeat. |

Queue tạo bằng `xQueueCreate(10, sizeof(Record))`: tối đa **10 bản ghi**.
Task `sensor` đẩy bản ghi với thời gian chờ 0 (không bao giờ chờ). Task `mqtt` nhận từ queue với
timeout 200 ms. Task `safety` không dùng queue và không gọi MQTT; nó chỉ đọc số bản ghi đang chờ để in log.

**Chính sách khi queue đầy: bỏ bản cũ nhất.** Khi `xQueueSend` báo đầy, `sensor` lấy bản cũ nhất ra
(`xQueueReceive` với thời gian chờ 0), bỏ đi, tăng bộ đếm `dropped`, rồi đẩy bản mới vào. Em chọn cách này
vì với số liệu theo dõi thì bản mới quan trọng hơn bản cũ. Chờ cho đến khi queue có chỗ trống sẽ làm
`sensor` bị chặn, còn ghi đè kiểu `xQueueOverwrite` chỉ dùng được với queue có đúng 1 phần tử.

**Vì sao task an toàn không được chờ vô hạn vào MQTT.** Một lệnh gọi mạng có thể treo bất cứ lúc nào
khi mất WiFi hoặc broker tắt (kết nối TCP có thể mất vài giây mới báo lỗi). Nếu vòng an toàn phải chờ MQTT
không giới hạn thì nó bị treo theo, và khi đó mọi lỗi hoặc hết giờ đều không được đưa về trạng thái an toàn.
Vì vậy `safety` chạy ở core riêng với chu kỳ cố định, không đụng đến MQTT hay queue theo kiểu chờ, còn mọi chỗ
có chờ trong hệ thống đều có timeout.

**Thí nghiệm nghẽn queue** (`log_queue_congestion.txt`): em đặt `MQTT_SLOW_TEST = 1` để task `mqtt`
chờ thêm 8 giây sau mỗi bản tin, trong khi `sensor` sinh một bản mỗi 2 giây. Trong 96 giây quan sát
(`t=29663` ms đến `t=125663` ms): queue đầy ở `queue=10`, `sent` chỉ tăng từ 4 lên 16 (1 bản mỗi 8 giây),
`dropped` tăng từ 0 lên 36 (khớp tính toán 0,5 − 0,125 = 0,375 bản/giây), còn `beat` của task an toàn tăng
từ 60 lên 252, đúng 500 ms mỗi nhịp và `state` luôn là `CHAY`. Số bản ghi: 16 đã gửi + 36 bị bỏ + 10 còn trong
queue = 62, bằng số bản đã sinh ra.

## 4.3 Thí nghiệm ngắt kết nối
Trong lúc ESP32 đang publish ổn định, em dừng broker bằng `docker compose stop broker`
lúc khoảng 16:25:03 (bản tin cuối nhận được là `seq 48` lúc 16:25:02) và khởi động lại
bằng `docker compose start broker` lúc khoảng 16:27:13. Broker tắt khoảng 2 phút 10 giây,
lâu hơn mức 30 giây gợi ý. Giờ trong `log_disconnect.txt` là UTC, giờ trong
`esp32_disconnect.log` là giờ máy (UTC+7).

**Vòng an toàn vẫn chạy.** Trong suốt lúc mất broker (`mqtt=0`), `beat` của task an toàn
vẫn tăng đều: từ 260 (`t=129662`) lên 456 (`t=227662`) là 196 nhịp trong 98 giây,
đúng chu kỳ 500 ms, không mất nhịp nào, và `state` luôn là `CHAY`. Task MQTT chỉ in
`[MQTT] that bai rc=-2` rồi thử lại, không làm các task khác bị chặn.

**Hành vi của queue.** Queue đầy (`queue=10`) sau khoảng 20 giây, sau đó cứ mỗi 2 giây
bỏ một bản cũ nhất. Kết quả: `dropped=56`, tương ứng đúng 56 bản `seq 49` đến `104`
bị thiếu trong `log_disconnect.txt`.

**Phục hồi.** Lúc 16:27:14.554 node tự kết nối lại (`[MQTT] da ket noi`), gửi dồn 10
bản còn giữ trong queue (`seq 105` đến `114`, khoảng 15 ms), `queue` về 0 rồi tiếp tục
gửi đều mỗi 2 giây (`seq 115` lúc 16:27:16). Công thức kiểm tra: `sent + dropped + queue`
(58 + 56 + 0 = 114) bằng số `seq` đã sinh ra.
