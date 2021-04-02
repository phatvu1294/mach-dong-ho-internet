# Mạch đồng hồ Internet
Mạch đồng hồ Internet được phát triển dựa trên ESP8266

### Giới thiệu
Đây là một ứng dụng nhỏ của ESP8266 sử dụng NTP để cập nhật thời gian và Webserver để tạo trang cấu hình. Các LED 7 thanh sẽ được điều khiển bằng IC 4094 và IC 74HC245.

![Mạch đồng hồ Internet](/images/image-01.png)

### Tính năng
- Đồng hồ Internet hỗ trợ xem giờ một cách linh hoạt, không cần cài phải chỉnh giờ
- LED 7 thanh hiển thị ngày, giờ, nhiệt độ thông qua cảm biến NTC 10K
- Nút nhấn giữ 3 chế độ điều chỉnh (tắt chuông báo thức/reset/cấu hình thông số)
- Hỗ trợ Webserver có sẵn cho phép cài đặt thông số WiFi và tuỳ chỉnh chức năng báo thức

### Thông số kỹ thuật
- Điện áp: 5VDC
- Nút nhấn cấu hình nhiều chế độ:
  + Tắt báo thức
  + Reset
  + Cấu hình Wifi và báo thức

