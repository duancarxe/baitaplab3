                           Lab 3 ĐỀ 1:Hệ thống giám sát và cảnh báo người chạy 
                               sử dụng ESP32, BMP180, FreeRTOS và MQTT

-Họ tên: Lê Phú Duẩn

-MSV:231432190

-Lớp: KỸ THUẬT ĐIỆN TỬ VÀ TIN HỌC CÔNG NGHIỆP 2 - K64

-Link project Wokwi: [Xem mô phỏng trên Wokwi](https://wokwi.com/projects/480044935015154689)

-Báo cáo: [Bao_cao_Lab3_De1.docx](https://github.com/user-attachments/files/34674955/Bao_cao_Lab3_De1.docx)

-Video demo:[video_demo.zip](https://github.com/user-attachments/files/34676514/video_demo.zip)


1. Giới thiệu dự án

-Runner Monitoring System là hệ thống giám sát các thông số môi trường trong quá trình người chạy vận động, sử dụng ESP32 kết hợp với cảm biến BMP180.
-Hệ thống có nhiệm vụ đo nhiệt độ, áp suất khí quyển và tính toán độ cao. Dữ liệu được ESP32 xử lý, sau đó gửi qua WiFi lên MQTT Broker để hiển thị trên Web Dashboard.
-Ngoài chức năng giám sát, hệ thống còn có khả năng cảnh báo tự động bằng LED và Buzzer khi nhiệt độ hoặc độ thay đổi độ cao vượt ngưỡng cài đặt. Người dùng cũng có thể điều khiển --LED và Buzzer trực tiếp trên giao diện Web

 2. Chức năng chính

-Đo nhiệt độ và áp suất khí quyển bằng cảm biến BMP180.
-Tính toán độ cao dựa trên áp suất khí quyển.
-Theo dõi sự thay đổi độ cao so với lần đo đầu tiên.
-Cảnh báo khi nhiệt độ hoặc độ thay đổi độ cao vượt ngưỡng.
-Điều khiển LED và Buzzer tự động.
-Điều khiển LED và Buzzer thủ công thông qua Web Dashboard.
-Gửi dữ liệu cảm biến lên MQTT Broker bằng WiFi.
-Hiển thị thông số và biểu đồ theo thời gian thực trên Website.
-Sử dụng 3 task FreeRTOS để xử lý các công việc đồng thời.

3. Sơ đồ kết nối phần cứng

| Thiết bị | Chân ESP32 |
|---|---|
| BMP180 - SDA | GPIO 21 |
| BMP180 - SCL | GPIO 22 |
| BMP180 - VCC | 3V3 |
| BMP180 - GND | GND |
| LED cảnh báo | GPIO 25 |
| Buzzer | GPIO 26 |

Kết nối:
-BMP180 giao tiếp với ESP32 thông qua chuẩn I2C.
-LED được nối với GPIO25 thông qua điện trở 330Ω.
-Buzzer được nối với GPIO26.
-Các thiết bị sử dụng chung chân GND.

## 4. Kiến trúc FreeRTOS

Hệ thống sử dụng 3 task FreeRTOS, mỗi task đảm nhận một công việc riêng:

| Task | Core | Priority | Stack | Chức năng |
|---|---:|---:|---:|---|
| TaskReadData | 1 | 2 | 4096 | Đọc cảm biến BMP180, tính toán thông số và kiểm tra cảnh báo |
| TaskSendData | 0 | 1 | 6144 | Kết nối WiFi, MQTT, gửi dữ liệu và nhận lệnh từ Web |
| TaskAlert | 1 | 2 | 4096 | Xử lý cảnh báo và điều khiển LED, Buzzer |

### Sơ đồ hoạt động


              ESP32 + BMP180
                    |
                    v
             TaskReadData
                Core 1
               /      \
              /        \
       dataQueue     dataAlert
           |             |
           v             v
      TaskSendData    TaskAlert
        Core 0         Core 1
           |             |
           v             v
       MQTT Broker    LED + Buzzer
           |
           v
      Web Dashboard


**Các thành phần trao đổi dữ liệu:**

- `dataQueue`: Lưu dữ liệu cảm biến để TaskSendData lấy và gửi lên MQTT.
- `dataAlert`: Truyền dữ liệu cảnh báo cho TaskAlert xử lý.
- `serialMutex`: Tránh xung đột khi nhiều task cùng sử dụng Serial.
- `controlMux`: Bảo vệ trạng thái điều khiển LED và Buzzer thủ công.

 5. Nguyên lý hoạt động và cảnh báo

 5.1. Giám sát độ cao
 
Khi hệ thống bắt đầu hoạt động, ESP32 lưu lại độ cao ở lần đo đầu tiên làm giá trị ban đầu.
Ở những lần đo tiếp theo, hệ thống tính độ thay đổi độ cao:
Nếu độ thay đổi độ cao lớn hơn hoặc bằng 100m, hệ thống kích hoạt cảnh báo.

  5.2. Giám sát nhiệt độ

Hệ thống sử dụng hai ngưỡng nhiệt độ:
| Nhiệt độ | Trạng thái |
|---|---|
| Dưới 10°C | Cảnh báo |
| Từ 10°C đến 35°C | Bình thường |
| Trên 35°C | Cảnh báo |

  5.3. Điều khiển LED và Buzzer
| Trạng thái | LED | Buzzer |
|---|---|---|
| Không có cảnh báo | Tắt | Tắt |
| Có cảnh báo tự động | Bật | Bật |
| Người dùng bật thủ công | Bật | Bật |
Hệ thống kết hợp cảnh báo tự động và điều khiển thủ công. Khi cảnh báo tự động vẫn còn, LED và Buzzer tiếp tục hoạt động dù người dùng gửi lệnh tắt thủ công.
## 6. Giao tiếp MQTT
Hệ thống sử dụng HiveMQ làm MQTT Broker để truyền dữ liệu giữa ESP32 và Web Dashboard.
-**Broker:** `broker.hivemq.com`
-**ESP32:** TCP Port `1883`
-**Web Dashboard:** WebSocket TLS Port `8884`
-**WebSocket URL:** `wss://broker.hivemq.com:8884/mqtt`
### Các Topic sử dụng

| Topic | Chức năng |
|---|---|
| `runner/telemetry` | ESP32 gửi dữ liệu cảm biến lên Web |
| `runner/control/led` | Web gửi lệnh điều khiển LED |
| `runner/control/buzzer` | Web gửi lệnh điều khiển Buzzer |

### Dữ liệu gửi lên MQTT

ESP32 gửi dữ liệu theo định dạng JSON, bao gồm:

| Tên dữ liệu | Ý nghĩa |
|---|---|
| `temperature` | Nhiệt độ môi trường (°C) |
| `pressure` | Áp suất khí quyển (hPa) |
| `altitude` | Độ cao hiện tại (m) |
| `initialAltitude` | Độ cao ban đầu (m) |
| `altitudeChange` | Độ thay đổi độ cao (m) |
| `altitudeAlert` | Trạng thái cảnh báo độ cao |
| `temperatureAlert` | Trạng thái cảnh báo nhiệt độ |
| `altitudeThreshold` | Ngưỡng thay đổi độ cao |
| `tempMax` | Ngưỡng nhiệt độ tối đa |
| `tempMin` | Ngưỡng nhiệt độ tối thiểu |

## 7. Giao diện Web Dashboard

Web Dashboard được xây dựng bằng HTML, CSS và JavaScript, sử dụng MQTT WebSocket để nhận dữ liệu từ ESP32.

Các chức năng chính:

-Hiển thị nhiệt độ môi trường.
-Hiển thị áp suất khí quyển.
-Hiển thị độ cao hiện tại và độ thay đổi độ cao.
-Theo dõi trạng thái cảnh báo.
-Hiển thị biểu đồ nhiệt độ và độ cao theo thời gian thực.
-Bật/tắt LED thông qua MQTT.
-Bật/tắt Buzzer thông qua MQTT.

## 8. Kết quả thực hiện

Sau khi hoàn thành, hệ thống đáp ứng các chức năng chính:

-ESP32 đọc được dữ liệu từ cảm biến BMP180.
-Tính toán được độ cao và độ thay đổi độ cao.
-Ba task FreeRTOS hoạt động theo đúng cấu hình Core đã thiết lập.
-Dữ liệu được gửi lên MQTT Broker.
-Web Dashboard nhận và hiển thị dữ liệu.
-Biểu đồ nhiệt độ và độ cao được cập nhật theo thời gian thực.
-LED và Buzzer hoạt động khi có cảnh báo.
-Điều khiển thiết bị thủ công thông qua Website.

## 9. Cấu trúc dự án

Dự án được chia thành các thư mục để dễ quản lý và tìm kiếm file.


```
├── README.md                  # Giới thiệu dự án
├── ESP32/
│   └── runner_monitoring.ino   # Code ESP32 (FreeRTOS + MQTT)
├── Web/
│   └── index.html             # Giao diện Web Dashboard
├── Simulation/
│   └── diagram.json           # Sơ đồ mạch Wokwi
├── Images/
│   ├── dashboard.png          # Hình ảnh Dashboard
│   └── simulation.png         # Hình ảnh mô phỏng
└── libraries.txt              # Thư viện sử dụng
```

