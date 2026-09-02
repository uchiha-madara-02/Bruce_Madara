#include "nrf_jammer.h"
#include "RF24.h"
#include "core/display.h"
#include "core/mykeyboard.h"
#include "nrf_common.h"
#include <globals.h>

void centerString(String text) {
    int x = (tftWidth - tft.textWidth(text)) / 2;
    if (x < 0) x = 0;
    int y = tftHeight - 20;
    tft.setCursor(x, y);
    tft.print(text);
}

// void nrf_jammer() {
//     // --- 1. KHỞI TẠO NRF24 ---
//     NRF24_MODE mode = NRF_MODE_SPI;
//     uint8_t NRFOnline = 0;
//     uint8_t NRFSPI = 0;

//     if (!nrf_start(mode)) {
//         displayError("NRF24 not found");
//         vTaskDelay(100 / portTICK_PERIOD_MS);
//         return;
//     }

//     if (CHECK_NRF_SPI(mode)) {
//         NRFradio.powerUp();
//         NRFradio.setAutoAck(false);
//         NRFradio.stopListening();
//         NRFradio.setRetries(0, 0);
//         NRFradio.setPALevel(RF24_PA_MAX, true); // Đảm bảo công suất phát tối đa
//         NRFradio.setDataRate(RF24_2MBPS);       // Tốc độ truyền dữ liệu cao nhất
//         NRFradio.setCRCLength(RF24_CRC_DISABLED);
//         NRFradio.startConstCarrier(RF24_PA_MAX, 50); // Phát sóng mang liên tục
//         if (!NRFradio.setDataRate(RF24_2MBPS)) {}
//         NRFSPI = 1;
//     }

//     // --- 2. CÁC BIẾN CẤU HÌNH ---
//     int startChannel = 1;
//     int stopChannel = 125;
//     int stepSize = 2;
//     bool isRandomMode = false;

//     // Đã bỏ các mục BLE Test không cần thiết, cập nhật lại Menu
//     const char *menuItems[] = {
//         "Test", "WiFi", "BLEch", "BLE Adv Pri", "Bluetooth", "USB", "Video Stream", "RC", "Full", "Exit"
//     };
//     int menuMax = 9; // Đã cập nhật lại số lượng tối đa của menu

//     const int menuStartY = 65;
//     const int menuGap = 20;
//     const int bottomMargin = 10;
//     int availableHeight = tftHeight - menuStartY - bottomMargin;
//     const int itemsPerPage = max(1, availableHeight / menuGap);

//     int menuIndex = 0;
//     int listOffset = 0;
//     bool redraw = true;
//     bool runJammer = false;
//     bool hopmenu = true;
//     bool fullRedraw = true;

//     vTaskDelay(350 / portTICK_PERIOD_MS);
//     if (CHECK_NRF_UART(mode) || CHECK_NRF_BOTH(mode)) NRFSerial.println("RADIOS");

//     // --- 3. VÒNG LẶP MENU ---
//     while (hopmenu) {
//         if (CHECK_NRF_UART(mode) || CHECK_NRF_BOTH(mode)) {
//             if (NRFSerial.available()) {
//                 String incomingNRFs = NRFSerial.readStringUntil('\n');
//                 incomingNRFs.trim();
//                 if (incomingNRFs.length() == 1 && isDigit(incomingNRFs.charAt(0))) {
//                     NRFOnline = (incomingNRFs.toInt());
//                     if (CHECK_NRF_BOTH(mode)) { NRFOnline = (incomingNRFs.toInt()) + NRFSPI; }
//                     redraw = true;
//                 }
//             }
//         }

//         if (redraw) {
//             // 1. CHỈ VẼ VIỀN VÀ TIÊU ĐỀ 1 LẦN DUY NHẤT
//             if (fullRedraw) {
//                 drawMainBorder();
//                 drawMainBorderWithTitle("NRF Jammer Config");
//                 tft.setTextSize(FM);
//                 int frameHeight = (itemsPerPage * menuGap) + 4;
//                 tft.drawRoundRect(8, menuStartY - 4, tftWidth - 16, frameHeight, 4, bruceConfig.priColor);
//                 fullRedraw = false; // Tắt cờ đi, từ giờ không vẽ lại viền nữa
//             }

//             if (menuIndex >= listOffset + itemsPerPage) { listOffset = menuIndex - itemsPerPage + 1; }
//             if (menuIndex < listOffset) { listOffset = menuIndex; }

//             // 2. VẼ LẠI CÁC MỤC MENU (CẬP NHẬT HIGHLIGHT THÔNG MINH)
//             for (int i = 0; i < itemsPerPage; i++) {
//                 int actualIndex = listOffset + i;
//                 int yPos = menuStartY + (i * menuGap);

//                 // --- BƯỚC QUAN TRỌNG: Xóa thanh highlight cũ ---
//                 // Dùng màu nền (bgColor) vẽ đè lên nguyên 1 dòng để xóa sạch dấu vết cũ
//                 tft.fillRoundRect(8, yPos - 2, tftWidth - 16, 18, 4, bruceConfig.bgColor);

//                 // Nếu cuộn qua trang cuối (hết item) thì chỉ xóa nền trống, không vẽ chữ
//                 if (actualIndex > menuMax) continue;

//                 // --- VẼ HIGHLIGHT MỚI VÀ CHỮ ---
//                 if (actualIndex == menuIndex) {
//                     // Dòng đang chọn: Nền màu chính, chữ màu nền
//                     tft.fillRoundRect(8, yPos - 2, tftWidth - 16, 18, 4, bruceConfig.priColor);
//                     tft.setTextColor(bruceConfig.bgColor);
//                 } else {
//                     // Dòng không chọn: Chữ màu chính
//                     tft.setTextColor(bruceConfig.priColor);
//                 }
//                 tft.setCursor(12, yPos);
//                 tft.println(menuItems[actualIndex]);
//             }
//             tft.setTextColor(bruceConfig.priColor);
//             redraw = false;
//         }

//         if (check(EscPress)) {
//             NRFradio.powerDown();
//             return;
//         }
//         if (check(NextPress)) {
//             menuIndex++;
//             if (menuIndex > menuMax) menuIndex = 0;
//             redraw = true;
//             vTaskDelay(150 / portTICK_PERIOD_MS);
//         }
//         if (check(PrevPress)) {
//             menuIndex--;
//             if (menuIndex < 0) menuIndex = menuMax;
//             redraw = true;
//             vTaskDelay(150 / portTICK_PERIOD_MS);
//         }

//         if (check(SelPress)) {
//             switch (menuIndex) {
//                 case 0:
//                     startChannel = 1;
//                     stopChannel = 125;
//                     stepSize = 1;
//                     isRandomMode = true;
//                     runJammer = true;
//                     hopmenu = false;
//                     break;
//                 case 1:
//                     startChannel = 2;
//                     stopChannel = 77;
//                     stepSize = 2;
//                     isRandomMode = false;
//                     runJammer = true;
//                     hopmenu = false;
//                     break;
//                 case 2:
//                     startChannel = 2;
//                     stopChannel = 41;
//                     stepSize = 2;
//                     isRandomMode = false;
//                     runJammer = true;
//                     hopmenu = false;
//                     break;
//                 case 3:
//                     startChannel = 31;
//                     stopChannel = 81;
//                     stepSize = 2;
//                     isRandomMode = false;
//                     runJammer = true;
//                     hopmenu = false;
//                     break;
//                 case 4:
//                     startChannel = 2;
//                     stopChannel = 80;
//                     stepSize = 2;
//                     isRandomMode = false;
//                     runJammer = true;
//                     hopmenu = false;
//                     break;
//                 case 5:
//                     startChannel = 40;
//                     stopChannel = 60;
//                     stepSize = 2;
//                     isRandomMode = false;
//                     runJammer = true;
//                     hopmenu = false;
//                     break;
//                 case 6:
//                     startChannel = 70;
//                     stopChannel = 80;
//                     stepSize = 2;
//                     isRandomMode = false;
//                     runJammer = true;
//                     hopmenu = false;
//                     break;
//                 case 7:
//                     startChannel = 1;
//                     stopChannel = 7;
//                     stepSize = 2;
//                     isRandomMode = false;
//                     runJammer = true;
//                     hopmenu = false;
//                     break;
//                 case 8:
//                     startChannel = 1;
//                     stopChannel = 125;
//                     stepSize = 2;
//                     isRandomMode = false;
//                     runJammer = true;
//                     hopmenu = false;
//                     break;
//                 case 9: NRFradio.powerDown(); return;
//             }
//         }
//     }

//     // --- 4. VÒNG LẶP TẤN CÔNG (ĐÃ TỐI ƯU HÓA TỐC ĐỘ) ---
//     if (runJammer) {
//         drawMainBorder();
//         drawMainBorderWithTitle(isRandomMode ? "NRF RANDOM TEST" : "NRF SUPER JAMMER");
//         tft.setTextSize(FM);

//         tft.setCursor(10, 70);
//         tft.println("STATUS : " + String(NRFOnline) + " ACTIVE");
//         tft.setCursor(10, 90);
//         tft.printf("Range : %d - %d", startChannel, stopChannel);

//         if (CHECK_NRF_SPI(mode)) {
//             if (isRandomMode) {
//                 // Tối ưu Random: Quét 50 kênh ngẫu nhiên liên tục rồi mới kiểm tra nút bấm 1 lần
//                 while (!check(SelPress)) {
//                     for (int i = 0; i < 100; i++) {
//                         int randomCh = startChannel + (rand() % (stopChannel - startChannel + 1));
//                         NRFradio.setChannel(randomCh);
//                     }
//                 }
//             } else {
//                 // Tối ưu Tuần tự: Dùng vòng lặp FOR quét thẳng một mạch từ đầu đến cuối dải kênh.
//                 // Vi điều khiển không bị gián đoạn bởi việc kiểm tra nút bấm giữa chừng!
//                 while (!check(SelPress)) {
//                     for (int ch = startChannel; ch <= stopChannel; ch++) {
//                         NRFradio.setChannel(ch);
//                     }
//                 }
//             }
//         } else {
//             // Dành cho chế độ UART (chỉ chờ bấm thoát, không chạy vòng lặp tốn tài nguyên)
//             while (!check(SelPress)) { vTaskDelay(50 / portTICK_PERIOD_MS); }
//         }

//         // Tắt bộ phát sau khi thoát vòng lặp để bảo vệ NRF24 không bị quá nhiệt
//         if (CHECK_NRF_SPI(mode)) {
//             NRFradio.stopConstCarrier();
//             NRFradio.powerDown();
//         }
//         if (CHECK_NRF_UART(mode) || CHECK_NRF_BOTH(mode)) NRFSerial.println("OFF");
//     }
// }

// void nrf_jammer() {
//     // --- 1. KHỞI TẠO NRF24 (SPI hoặc UART) ---
//     NRF24_MODE mode = NRF_MODE_SPI;
//     uint8_t NRFOnline = 0;
//     uint8_t NRFSPI = 0;

//     if (!nrf_start(mode)) {
//         displayError("NRF24 not found");
//         vTaskDelay(100 / portTICK_PERIOD_MS);
//         return;
//     }

//     if (CHECK_NRF_SPI(mode)) {
//         NRFradio.powerUp();
//         NRFradio.setAutoAck(false);
//         NRFradio.setRetries(0, 0);
//         NRFradio.setPALevel(RF24_PA_MAX, true);
//         NRFradio.setDataRate(RF24_2MBPS);
//         NRFradio.setCRCLength(RF24_CRC_DISABLED);
//         NRFradio.setChannel(0);
//         NRFradio.stopListening();
//         NRFradio.startConstCarrier(RF24_PA_MAX, 50);

//         // Chuẩn bị payload gây nhiễu (toàn 0xFF)
//         uint8_t txBuf[32];
//         memset(txBuf, 0xFF, 32);
//         NRFradio.setPayloadSize(32);
//         NRFradio.openWritingPipe(0xB3B4B5B6F1LL); // Địa chỉ tùy ý
//         NRFradio.startWrite(txBuf, 32, false);    // Bắt đầu gửi (sẽ gửi lại trong vòng lặp)

//         if (!NRFradio.setDataRate(RF24_2MBPS)) NRFSPI = 1;
//     }

//     // --- 2. CẤU HÌNH JAMMER (BLE mặc định) ---
//     int startChannel = 2; // Kênh BLE thấp nhất (2402 MHz)
//     int stopChannel = 80; // Kênh BLE cao nhất (2480 MHz)
//     int stepSize = 1;     // Quét từng kênh
//     bool isRandomMode = false;
//     int dwellTimeUs = 500; // Thời gian giữ mỗi kênh (microseconds)

//     // Menu các chế độ
//     const char *menuItems[] = {
//         "BLE Full (2-80)",
//         "BLE Adv (37-39)",
//         "BLE Data (2-38)",
//         "BLE Random",
//         "WiFi 2.4G (1-14)",
//         "Full Range (1-125)",
//         "Exit"
//     };
//     int menuMax = 6;

//     const int menuStartY = 65;
//     const int menuGap = 20;
//     const int bottomMargin = 10;
//     int availableHeight = tftHeight - menuStartY - bottomMargin;
//     const int itemsPerPage = max(1, availableHeight / menuGap);

//     int menuIndex = 0;
//     int listOffset = 0;
//     bool redraw = true;
//     bool runJammer = false;
//     bool hopmenu = true;
//     bool fullRedraw = true;

//     vTaskDelay(350 / portTICK_PERIOD_MS);
//     if (CHECK_NRF_UART(mode) || CHECK_NRF_BOTH(mode)) NRFSerial.println("RADIOS");

//     // --- 3. VÒNG LẶP HIỂN THỊ MENU ---
//     while (hopmenu) {
//         // Đọc trạng thái từ UART (nếu có)
//         if (CHECK_NRF_UART(mode) || CHECK_NRF_BOTH(mode)) {
//             if (NRFSerial.available()) {
//                 String incomingNRFs = NRFSerial.readStringUntil('\n');
//                 incomingNRFs.trim();
//                 if (incomingNRFs.length() == 1 && isDigit(incomingNRFs.charAt(0))) {
//                     NRFOnline = (incomingNRFs.toInt());
//                     if (CHECK_NRF_BOTH(mode)) { NRFOnline = (incomingNRFs.toInt()) + NRFSPI; }
//                     redraw = true;
//                 }
//             }
//         }

//         // Vẽ menu (chỉ khi cần)
//         if (redraw) {
//             if (fullRedraw) {
//                 drawMainBorder();
//                 drawMainBorderWithTitle("NRF Jammer Config");
//                 tft.setTextSize(FM);
//                 int frameHeight = (itemsPerPage * menuGap) + 4;
//                 tft.drawRoundRect(8, menuStartY - 4, tftWidth - 16, frameHeight, 4, bruceConfig.priColor);
//                 fullRedraw = false;
//             }

//             // Cập nhật offset cuộn
//             if (menuIndex >= listOffset + itemsPerPage) listOffset = menuIndex - itemsPerPage + 1;
//             if (menuIndex < listOffset) listOffset = menuIndex;

//             // Vẽ từng dòng menu
//             for (int i = 0; i < itemsPerPage; i++) {
//                 int actualIndex = listOffset + i;
//                 int yPos = menuStartY + (i * menuGap);
//                 // Xóa nền
//                 tft.fillRoundRect(8, yPos - 2, tftWidth - 16, 18, 4, bruceConfig.bgColor);
//                 if (actualIndex > menuMax) continue;
//                 if (actualIndex == menuIndex) {
//                     tft.fillRoundRect(8, yPos - 2, tftWidth - 16, 18, 4, bruceConfig.priColor);
//                     tft.setTextColor(bruceConfig.bgColor);
//                 } else {
//                     tft.setTextColor(bruceConfig.priColor);
//                 }
//                 tft.setCursor(12, yPos);
//                 tft.println(menuItems[actualIndex]);
//             }
//             tft.setTextColor(bruceConfig.priColor);
//             redraw = false;
//         }

//         // Xử lý phím bấm
//         if (check(EscPress)) {
//             if (CHECK_NRF_SPI(mode)) NRFradio.powerDown();
//             return;
//         }
//         if (check(NextPress)) {
//             menuIndex++;
//             if (menuIndex > menuMax) menuIndex = 0;
//             redraw = true;
//             vTaskDelay(150 / portTICK_PERIOD_MS);
//         }
//         if (check(PrevPress)) {
//             menuIndex--;
//             if (menuIndex < 0) menuIndex = menuMax;
//             redraw = true;
//             vTaskDelay(150 / portTICK_PERIOD_MS);
//         }
//         if (check(SelPress)) {
//             switch (menuIndex) {
//                 case 0: // BLE Full
//                     startChannel = 2;
//                     stopChannel = 80;
//                     stepSize = 1;
//                     isRandomMode = false;
//                     runJammer = true;
//                     hopmenu = false;
//                     break;
//                 case 1: // BLE Advertising channels (37,38,39)
//                     startChannel = 37;
//                     stopChannel = 39;
//                     stepSize = 1;
//                     isRandomMode = false;
//                     runJammer = true;
//                     hopmenu = false;
//                     break;
//                 case 2: // BLE Data channels (0-36 -> NRF 2-38)
//                     startChannel = 2;
//                     stopChannel = 38;
//                     stepSize = 1;
//                     isRandomMode = false;
//                     runJammer = true;
//                     hopmenu = false;
//                     break;
//                 case 3: // BLE Random
//                     startChannel = 2;
//                     stopChannel = 80;
//                     stepSize = 1;
//                     isRandomMode = true;
//                     runJammer = true;
//                     hopmenu = false;
//                     break;
//                 case 4: // WiFi 2.4GHz (kênh 1-14)
//                     startChannel = 1;
//                     stopChannel = 14;
//                     stepSize = 1;
//                     isRandomMode = false;
//                     runJammer = true;
//                     hopmenu = false;
//                     break;
//                 case 5: // Full Range (1-125)
//                     startChannel = 1;
//                     stopChannel = 125;
//                     stepSize = 1;
//                     isRandomMode = false;
//                     runJammer = true;
//                     hopmenu = false;
//                     break;
//                 case 6: // Exit
//                     if (CHECK_NRF_SPI(mode)) NRFradio.powerDown();
//                     return;
//             }
//         }
//     }

//     // --- 4. THỰC HIỆN JAMMING ---
//     if (runJammer) {
//         drawMainBorder();
//         drawMainBorderWithTitle(isRandomMode ? "RANDOM JAMMER" : "BLE JAMMER");
//         tft.setTextSize(FM);
//         tft.setCursor(10, 70);
//         tft.println("STATUS: ACTIVE");
//         tft.setCursor(10, 90);
//         tft.printf("Channels: %d - %d (step %d)", startChannel, stopChannel, stepSize);
//         tft.setCursor(10, 110);
//         tft.printf("Dwell time: %d us", dwellTimeUs);

//         if (CHECK_NRF_SPI(mode)) {
//             uint8_t txBuf[32];
//             memset(txBuf, 0xFF, 32);

//             if (isRandomMode) {
//                 // Chế độ ngẫu nhiên: chọn kênh ngẫu nhiên, phát trong dwellTimeUs
//                 while (!check(SelPress)) {
//                     int ch = startChannel + (rand() % (stopChannel - startChannel + 1));
//                     NRFradio.setChannel(ch);
//                     unsigned long startUs = micros();
//                     while (micros() - startUs < dwellTimeUs) {
//                         NRFradio.startWrite(txBuf, 32, false);
//                         delayMicroseconds(10); // Chờ gửi xong
//                     }
//                 }
//             } else {
//                 // Chế độ tuần tự: quét lần lượt các kênh
//                 while (!check(SelPress)) {
//                     for (int ch = startChannel; ch <= stopChannel; ch += stepSize) {
//                         NRFradio.setChannel(ch);
//                         unsigned long startUs = micros();
//                         while (micros() - startUs < dwellTimeUs) {
//                             NRFradio.startWrite(txBuf, 32, false);
//                             delayMicroseconds(10);
//                         }
//                     }
//                 }
//             }
//             // Tắt radio sau khi kết thúc
//             NRFradio.powerDown();
//         } else if (CHECK_NRF_UART(mode) || CHECK_NRF_BOTH(mode)) {
//             // Nếu dùng module UART (ví dụ HC-05 chạy lệnh AT), chỉ chờ thoát
//             while (!check(SelPress)) { vTaskDelay(50 / portTICK_PERIOD_MS); }
//             NRFSerial.println("OFF");
//         }
//     }
// }

// void nrf_jammer() {
//     // --- 1. KHỞI TẠO NRF24 (SPI hoặc UART) ---
//     NRF24_MODE mode = NRF_MODE_SPI;
//     uint8_t NRFOnline = 0;
//     uint8_t NRFSPI = 0;

//     if (!nrf_start(mode)) {
//         displayError("NRF24 not found");
//         vTaskDelay(100 / portTICK_PERIOD_MS);
//         return;
//     }

//     if (CHECK_NRF_SPI(mode)) {
//         NRFradio.powerUp();
//         NRFradio.setAutoAck(false);
//         NRFradio.setRetries(0, 0);
//         NRFradio.setPALevel(RF24_PA_MAX, true);
//         NRFradio.setDataRate(RF24_2MBPS);
//         NRFradio.setCRCLength(RF24_CRC_DISABLED);
//         NRFradio.setChannel(0);
//         NRFradio.stopListening();

//         // SỬ DỤNG LOGIC TỪ HOPPER: Phát sóng mang liên tục thay vì gửi gói tin
//         NRFradio.startConstCarrier(RF24_PA_MAX, 50);

//         if (!NRFradio.setDataRate(RF24_2MBPS)) NRFSPI = 1;
//     }

//     // --- 2. CẤU HÌNH JAMMER (BLE mặc định) ---
//     int startChannel = 2; // Kênh BLE thấp nhất (2402 MHz)
//     int stopChannel = 80; // Kênh BLE cao nhất (2480 MHz)
//     int stepSize = 1;     // Quét từng kênh
//     bool isRandomMode = false;
//     int dwellTimeUs = 500; // Thời gian giữ mỗi kênh (microseconds)

//     // Menu các chế độ
//     const char *menuItems[] = {
//         "BLE Full (2-80)",
//         "BLE Adv (37-39)",
//         "BLE Data (2-38)",
//         "BLE Random",
//         "WiFi 2.4G (1-14)",
//         "Full Range (1-125)",
//         "Exit"
//     };
//     int menuMax = 6;

//     const int menuStartY = 65;
//     const int menuGap = 20;
//     const int bottomMargin = 10;
//     int availableHeight = tftHeight - menuStartY - bottomMargin;
//     const int itemsPerPage = max(1, availableHeight / menuGap);

//     int menuIndex = 0;
//     int listOffset = 0;
//     bool redraw = true;
//     bool runJammer = false;
//     bool hopmenu = true;
//     bool fullRedraw = true;

//     vTaskDelay(350 / portTICK_PERIOD_MS);
//     if (CHECK_NRF_UART(mode) || CHECK_NRF_BOTH(mode)) NRFSerial.println("RADIOS");

//     // --- 3. VÒNG LẶP HIỂN THỊ MENU ---
//     while (hopmenu) {
//         // Đọc trạng thái từ UART (nếu có)
//         if (CHECK_NRF_UART(mode) || CHECK_NRF_BOTH(mode)) {
//             if (NRFSerial.available()) {
//                 String incomingNRFs = NRFSerial.readStringUntil('\n');
//                 incomingNRFs.trim();
//                 if (incomingNRFs.length() == 1 && isDigit(incomingNRFs.charAt(0))) {
//                     NRFOnline = (incomingNRFs.toInt());
//                     if (CHECK_NRF_BOTH(mode)) { NRFOnline = (incomingNRFs.toInt()) + NRFSPI; }
//                     redraw = true;
//                 }
//             }
//         }

//         // Vẽ menu (chỉ khi cần)
//         if (redraw) {
//             if (fullRedraw) {
//                 drawMainBorder();
//                 drawMainBorderWithTitle("NRF Jammer Config");
//                 tft.setTextSize(FM);
//                 int frameHeight = (itemsPerPage * menuGap) + 4;
//                 tft.drawRoundRect(8, menuStartY - 4, tftWidth - 16, frameHeight, 4, bruceConfig.priColor);
//                 fullRedraw = false;
//             }

//             // Cập nhật offset cuộn
//             if (menuIndex >= listOffset + itemsPerPage) listOffset = menuIndex - itemsPerPage + 1;
//             if (menuIndex < listOffset) listOffset = menuIndex;

//             // Vẽ từng dòng menu
//             for (int i = 0; i < itemsPerPage; i++) {
//                 int actualIndex = listOffset + i;
//                 int yPos = menuStartY + (i * menuGap);
//                 // Xóa nền
//                 tft.fillRoundRect(8, yPos - 2, tftWidth - 16, 18, 4, bruceConfig.bgColor);
//                 if (actualIndex > menuMax) continue;
//                 if (actualIndex == menuIndex) {
//                     tft.fillRoundRect(8, yPos - 2, tftWidth - 16, 18, 4, bruceConfig.priColor);
//                     tft.setTextColor(bruceConfig.bgColor);
//                 } else {
//                     tft.setTextColor(bruceConfig.priColor);
//                 }
//                 tft.setCursor(12, yPos);
//                 tft.println(menuItems[actualIndex]);
//             }
//             tft.setTextColor(bruceConfig.priColor);
//             redraw = false;
//         }

//         // Xử lý phím bấm
//         if (check(EscPress)) {
//             if (CHECK_NRF_SPI(mode)) NRFradio.powerDown();
//             return;
//         }
//         if (check(NextPress)) {
//             menuIndex++;
//             if (menuIndex > menuMax) menuIndex = 0;
//             redraw = true;
//             vTaskDelay(150 / portTICK_PERIOD_MS);
//         }
//         if (check(PrevPress)) {
//             menuIndex--;
//             if (menuIndex < 0) menuIndex = menuMax;
//             redraw = true;
//             vTaskDelay(150 / portTICK_PERIOD_MS);
//         }
//         if (check(SelPress)) {
//             switch (menuIndex) {
//                 case 0: // BLE Full
//                     startChannel = 2;
//                     stopChannel = 80;
//                     stepSize = 1;
//                     isRandomMode = false;
//                     runJammer = true;
//                     hopmenu = false;
//                     break;
//                 case 1: // BLE Advertising channels (37,38,39)
//                     startChannel = 37;
//                     stopChannel = 39;
//                     stepSize = 1;
//                     isRandomMode = false;
//                     runJammer = true;
//                     hopmenu = false;
//                     break;
//                 case 2: // BLE Data channels (0-36 -> NRF 2-38)
//                     startChannel = 2;
//                     stopChannel = 38;
//                     stepSize = 1;
//                     isRandomMode = false;
//                     runJammer = true;
//                     hopmenu = false;
//                     break;
//                 case 3: // BLE Random
//                     startChannel = 2;
//                     stopChannel = 80;
//                     stepSize = 1;
//                     isRandomMode = true;
//                     runJammer = true;
//                     hopmenu = false;
//                     break;
//                 case 4: // WiFi 2.4GHz (kênh 1-14)
//                     startChannel = 1;
//                     stopChannel = 14;
//                     stepSize = 1;
//                     isRandomMode = false;
//                     runJammer = true;
//                     hopmenu = false;
//                     break;
//                 case 5: // Full Range (1-125)
//                     startChannel = 1;
//                     stopChannel = 125;
//                     stepSize = 1;
//                     isRandomMode = false;
//                     runJammer = true;
//                     hopmenu = false;
//                     break;
//                 case 6: // Exit
//                     if (CHECK_NRF_SPI(mode)) {
//                         NRFradio.stopConstCarrier();
//                         NRFradio.powerDown();
//                     }
//                     return;
//             }
//         }
//     }

//     // --- 4. THỰC HIỆN JAMMING BẰNG LOGIC CỦA HOPPER ---
//     if (runJammer) {
//         drawMainBorder();
//         drawMainBorderWithTitle(isRandomMode ? "RANDOM JAMMER" : "BLE JAMMER");
//         tft.setTextSize(FM);
//         tft.setCursor(10, 70);
//         tft.println("STATUS: ACTIVE");
//         tft.setCursor(10, 90);
//         tft.printf("Channels: %d - %d (step %d)", startChannel, stopChannel, stepSize);
//         tft.setCursor(10, 110);
//         tft.printf("Dwell time: %d us", dwellTimeUs);

//         if (CHECK_NRF_SPI(mode)) {
//             int currentChannel = startChannel; // Biến dùng cho chế độ tuần tự

//             // Vòng lặp chính ép chết dải tần
//             while (!check(SelPress)) {
//                 if (isRandomMode) {
//                     // Chế độ ngẫu nhiên: nhảy kênh lộn xộn
//                     currentChannel = startChannel + (rand() % (stopChannel - startChannel + 1));
//                 } else {
//                     // Chế độ tuần tự: trượt tuần tự qua các kênh (giống hệt hopper)
//                     currentChannel += stepSize;
//                     if (currentChannel > stopChannel) currentChannel = startChannel;
//                 }

//                 // Cập nhật tần số phát sóng mang liên tục
//                 NRFradio.setChannel(currentChannel);

//                 // Giữ ở kênh này một chút để đảm bảo làm nhiễu hiệu quả,
//                 // hoặc bạn có thể set dwellTimeUs = 0 để quét tối đa tốc độ
//                 if (dwellTimeUs > 0) { delayMicroseconds(dwellTimeUs); }
//             }

//             // Tắt radio sau khi kết thúc vòng lặp
//             NRFradio.stopConstCarrier(); // Rất quan trọng: tắt sóng mang
//             NRFradio.powerDown();
//         } else if (CHECK_NRF_UART(mode) || CHECK_NRF_BOTH(mode)) {
//             while (!check(SelPress)) { vTaskDelay(50 / portTICK_PERIOD_MS); }
//             NRFSerial.println("OFF");
//         }
//     }
// }v1

void nrf_jammer() {
    // --- 1. KHỞI TẠO NRF24 (SPI hoặc UART) ---
    NRF24_MODE mode = NRF_MODE_SPI;
    uint8_t NRFOnline = 0;
    uint8_t NRFSPI = 0;

    if (!nrf_start(mode)) {
        displayError("NRF24 not found");
        vTaskDelay(100 / portTICK_PERIOD_MS);
        return;
    }

    // if (CHECK_NRF_SPI(mode)) {
    //     NRFradio.powerUp();
    //     NRFradio.setAutoAck(false);
    //     NRFradio.setRetries(0, 0);
    //     NRFradio.setPALevel(RF24_PA_MAX, true);
    //     NRFradio.setDataRate(RF24_2MBPS);
    //     NRFradio.setCRCLength(RF24_CRC_DISABLED);
    //     NRFradio.setChannel(0);
    //     NRFradio.stopListening();

    //     // SỬ DỤNG LOGIC TỪ HOPPER: Phát sóng mang liên tục thay vì gửi gói tin
    //     NRFradio.startConstCarrier(RF24_PA_MAX, 50);

    //     if (!NRFradio.setDataRate(RF24_2MBPS)) NRFSPI = 1;
    // }

    if (CHECK_NRF_SPI(mode)) {
        // 🔥 SỬA LỖI THƯ VIỆN: phải powerUp() trước khi startConstCarrier
        NRFradio.powerUp();
        // delay(5);
        // NRFradio.setAutoAck(false);
        // NRFradio.stopListening();
        // NRFradio.setRetries(0, 0);
        // NRFradio.setPayloadSize(5);  ////SET VALUE ON RF24.CPP
        // NRFradio.setAddressWidth(3); ////SET VALUE ON RF24.CPP
        // NRFradio.setPALevel(RF24_PA_MAX, true);
        // NRFradio.setDataRate(RF24_2MBPS);
        // NRFradio.setCRCLength(RF24_CRC_DISABLED);
        // NRFradio.printPrettyDetails();

        // // Khởi tạo sóng mang liên tục (CW) một lần duy nhất
        // NRFradio.startConstCarrier(RF24_PA_MAX, 45);
        NRFradio.setAutoAck(false);
        NRFradio.setRetries(0, 0);
        NRFradio.setPALevel(RF24_PA_MAX, true);
        NRFradio.setDataRate(RF24_2MBPS);
        NRFradio.setCRCLength(RF24_CRC_DISABLED);
        NRFradio.setChannel(0);
        NRFradio.stopListening();
        NRFradio.startConstCarrier(RF24_PA_MAX, 50);

        if (!NRFradio.setDataRate(RF24_2MBPS)) NRFSPI = 1;
    }

    // --- 2. CẤU HÌNH JAMMER (BLE mặc định) ---
    int startChannel = 2; // Kênh BLE thấp nhất (2402 MHz)
    int stopChannel = 80; // Kênh BLE cao nhất (2480 MHz)
    int stepSize = 1;     // Quét từng kênh
    bool isRandomMode = false;

    const char *menuItems[] = {
        "Test", "WiFi", "BLEch", "BLE Adv Pri", "Bluetooth", "USB", "Video Stream", "RC", "Full", "Exit"
    };
    int menuMax = 9;

    const int menuStartY = 65;
    const int menuGap = 20;
    const int bottomMargin = 10;
    int availableHeight = tftHeight - menuStartY - bottomMargin;
    const int itemsPerPage = max(1, availableHeight / menuGap);

    int menuIndex = 0;
    int listOffset = 0;
    bool redraw = true;
    bool runJammer = false;
    bool hopmenu = true;
    bool fullRedraw = true;

    vTaskDelay(350 / portTICK_PERIOD_MS);
    if (CHECK_NRF_UART(mode) || CHECK_NRF_BOTH(mode)) NRFSerial.println("RADIOS");

    // --- 3. VÒNG LẶP HIỂN THỊ MENU ---
    while (hopmenu) {
        // Đọc trạng thái từ UART (nếu có)
        if (CHECK_NRF_UART(mode) || CHECK_NRF_BOTH(mode)) {
            if (NRFSerial.available()) {
                String incomingNRFs = NRFSerial.readStringUntil('\n');
                incomingNRFs.trim();
                if (incomingNRFs.length() == 1 && isDigit(incomingNRFs.charAt(0))) {
                    NRFOnline = (incomingNRFs.toInt());
                    if (CHECK_NRF_BOTH(mode)) { NRFOnline = (incomingNRFs.toInt()) + NRFSPI; }
                    redraw = true;
                }
            }
        }

        // Vẽ menu (chỉ khi cần)
        if (redraw) {
            if (fullRedraw) {
                drawMainBorder();
                drawMainBorderWithTitle("NRF Jammer Config");
                tft.setTextSize(FM);
                int frameHeight = (itemsPerPage * menuGap) + 4;
                tft.drawRoundRect(8, menuStartY - 4, tftWidth - 16, frameHeight, 4, bruceConfig.priColor);
                fullRedraw = false;
            }

            // Cập nhật offset cuộn
            if (menuIndex >= listOffset + itemsPerPage) listOffset = menuIndex - itemsPerPage + 1;
            if (menuIndex < listOffset) listOffset = menuIndex;

            // Vẽ từng dòng menu
            for (int i = 0; i < itemsPerPage; i++) {
                int actualIndex = listOffset + i;
                int yPos = menuStartY + (i * menuGap);
                // Xóa nền
                tft.fillRoundRect(10, yPos - 2, tftWidth - 20, 18, 4, bruceConfig.bgColor);
                if (actualIndex > menuMax) continue;
                if (actualIndex == menuIndex) {
                    tft.fillRoundRect(10, yPos - 2, tftWidth - 20, 18, 4, bruceConfig.priColor);
                    tft.setTextColor(bruceConfig.bgColor);
                } else {
                    tft.setTextColor(bruceConfig.priColor);
                }
                tft.setCursor(14, yPos);
                tft.println(menuItems[actualIndex]);
            }
            tft.setTextColor(bruceConfig.priColor);
            redraw = false;
        }

        // Xử lý phím bấm
        if (check(EscPress)) {
            if (CHECK_NRF_SPI(mode)) NRFradio.powerDown();
            return;
        }
        if (check(NextPress)) {
            menuIndex++;
            if (menuIndex > menuMax) menuIndex = 0;
            redraw = true;
            vTaskDelay(150 / portTICK_PERIOD_MS);
        }
        if (check(PrevPress)) {
            menuIndex--;
            if (menuIndex < 0) menuIndex = menuMax;
            redraw = true;
            vTaskDelay(150 / portTICK_PERIOD_MS);
        }

        if (check(SelPress)) {
            switch (menuIndex) {
                case 0:
                    startChannel = 1;
                    stopChannel = 125;
                    stepSize = 1;
                    isRandomMode = true;
                    runJammer = true;
                    hopmenu = false;
                    break;
                case 1:
                    startChannel = 2;
                    stopChannel = 77;
                    stepSize = 1;
                    isRandomMode = false;
                    runJammer = true;
                    hopmenu = false;
                    break;
                case 2:
                    startChannel = 2;
                    stopChannel = 41;
                    stepSize = 1;
                    isRandomMode = false;
                    runJammer = true;
                    hopmenu = false;
                    break;
                case 3:
                    startChannel = 31;
                    stopChannel = 81;
                    stepSize = 1;
                    isRandomMode = false;
                    runJammer = true;
                    hopmenu = false;
                    break;
                case 4:
                    startChannel = 2;
                    stopChannel = 80;
                    stepSize = 1;
                    isRandomMode = false;
                    runJammer = true;
                    hopmenu = false;
                    break;
                case 5:
                    startChannel = 40;
                    stopChannel = 60;
                    stepSize = 1;
                    isRandomMode = false;
                    runJammer = true;
                    hopmenu = false;
                    break;
                case 6:
                    startChannel = 70;
                    stopChannel = 80;
                    stepSize = 1;
                    isRandomMode = false;
                    runJammer = true;
                    hopmenu = false;
                    break;
                case 7:
                    startChannel = 1;
                    stopChannel = 7;
                    stepSize = 1;
                    isRandomMode = false;
                    runJammer = true;
                    hopmenu = false;
                    break;
                case 8:
                    startChannel = 1;
                    stopChannel = 125;
                    stepSize = 1;
                    isRandomMode = false;
                    runJammer = true;
                    hopmenu = false;
                    break;
                case 9:
                    if (CHECK_NRF_SPI(mode)) {
                        NRFradio.stopConstCarrier();
                        NRFradio.powerDown();
                    }
                    return;
            }
        }
    }

    // --- 4. THỰC HIỆN JAMMING BẰNG LOGIC CỦA HOPPER ---
    if (runJammer) {
        drawMainBorder();
        drawMainBorderWithTitle(isRandomMode ? "RANDOM JAMMER" : "BLE JAMMER");
        tft.setTextSize(FM);
        tft.setCursor(10, 70);
        tft.println("STATUS: ACTIVE");
        tft.setCursor(10, 90);
        tft.printf("Channels: %d - %d (step %d)", startChannel, stopChannel, stepSize);

        if (CHECK_NRF_SPI(mode)) {
            int currentChannel = startChannel; // Biến dùng cho chế độ tuần tự

            // Vòng lặp chính ép chết dải tần
            while (!check(SelPress)) {
                if (isRandomMode) {
                    // Chế độ ngẫu nhiên: nhảy kênh lộn xộn
                    currentChannel = startChannel + (rand() % (stopChannel - startChannel + 1));
                } else {
                    // Chế độ tuần tự: trượt tuần tự qua các kênh (giống hệt hopper)
                    currentChannel += stepSize;
                    if (currentChannel > stopChannel) currentChannel = startChannel;
                }

                // Cập nhật tần số phát sóng mang liên tục
                NRFradio.setChannel(currentChannel);
            }

            // Tắt radio sau khi kết thúc vòng lặp
            NRFradio.stopConstCarrier(); // Rất quan trọng: tắt sóng mang
            NRFradio.powerDown();
        } else if (CHECK_NRF_UART(mode) || CHECK_NRF_BOTH(mode)) {
            while (!check(SelPress)) { vTaskDelay(50 / portTICK_PERIOD_MS); }
            NRFSerial.println("OFF");
        }
    }
}

// // =====================================================================
// //  NRF Jammer - CW ONLY (Không Flooding) + Mảng kênh tối ưu
// // =====================================================================

// // Các mảng kênh (đặt ở đầu file hoặc trong hàm nếu chỉ dùng ở đây)
// static const uint8_t CH_WIFI[] PROGMEM = {
//     1,  3,  5,  7,  9,  11, 13, 15, 17, 19, 21, 23, // WiFi ch 1
//     26, 28, 30, 32, 34, 36, 38, 40, 42,             // WiFi ch 6
//     51, 53, 55, 57, 59, 61, 63, 65, 67, 69, 71, 73  // WiFi ch 11
// };
// static const uint8_t CH_BLE[] PROGMEM = {2,  4,  6,  8,  10, 12, 14, 16, 18, 20, 22, 24, 26, 28,
//                                          30, 32, 34, 36, 38, 40, 42, 44, 46, 48, 50, 52, 54, 56,
//                                          58, 60, 62, 64, 66, 68, 70, 72, 74, 76, 78, 80};
// static const uint8_t CH_BLE_ADV[] PROGMEM = {2, 26, 80};
// static const uint8_t CH_BLUETOOTH[] PROGMEM = {2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16,
// 17,
//                                                18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32,
//                                                33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
//                                                48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62,
//                                                63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77,
//                                                78, 79, 80};
// static const uint8_t CH_USB[] PROGMEM = {40, 50, 60};
// static const uint8_t CH_VIDEO[] PROGMEM = {70, 75, 80};
// static const uint8_t CH_RC[] PROGMEM = {1, 3, 5, 7};

// void nrf_jammer() {
//     // --- 1. KHỞI TẠO NRF24 (SPI hoặc UART) ---
//     NRF24_MODE mode = NRF_MODE_SPI;
//     uint8_t NRFOnline = 0;
//     uint8_t NRFSPI = 0;

//     if (!nrf_start(mode)) {
//         displayError("NRF24 not found");
//         vTaskDelay(100 / portTICK_PERIOD_MS);
//         return;
//     }

//     if (CHECK_NRF_SPI(mode)) {
//         // 🔥 SỬA LỖI THƯ VIỆN: phải powerUp() trước khi startConstCarrier
//         NRFradio.powerUp();
//         delay(5);
//         NRFradio.setAutoAck(false);
//         NRFradio.stopListening();
//         NRFradio.setRetries(0, 0);
//         NRFradio.setPayloadSize(5);  ////SET VALUE ON RF24.CPP
//         NRFradio.setAddressWidth(3); ////SET VALUE ON RF24.CPP
//         NRFradio.setPALevel(RF24_PA_MAX, true);
//         NRFradio.setDataRate(RF24_2MBPS);
//         NRFradio.setCRCLength(RF24_CRC_DISABLED);
//         NRFradio.printPrettyDetails();

//         // Khởi tạo sóng mang liên tục (CW) một lần duy nhất
//         NRFradio.startConstCarrier(RF24_PA_MAX, 45);

//         if (!NRFradio.setDataRate(RF24_2MBPS)) NRFSPI = 1;
//     }

//     // --- 2. MENU LỰA CHỌN CHẾ ĐỘ (giữ nguyên giao diện cũ) ---
//     const char *menuItems[] = {
//         "Test", "WiFi", "BLEch", "BLE Adv Pri", "Bluetooth", "USB", "Video Stream", "RC", "Full", "Exit"
//     };
//     int menuMax = 9;

//     const int menuStartY = 65;
//     const int menuGap = 20;
//     const int bottomMargin = 10;
//     int availableHeight = tftHeight - menuStartY - bottomMargin;
//     const int itemsPerPage = max(1, availableHeight / menuGap);

//     int menuIndex = 0;
//     int listOffset = 0;
//     bool redraw = true;
//     bool runJammer = false;
//     bool hopmenu = true;
//     bool fullRedraw = true;

//     // Thông tin chế độ được chọn
//     const uint8_t *selectedChannels = nullptr;
//     size_t channelCount = 0;
//     bool isRandomMode = false;
//     int randomMin = 1, randomMax = 125;
//     const char *modeTitle = "JAMMER";

//     vTaskDelay(350 / portTICK_PERIOD_MS);
//     if (CHECK_NRF_UART(mode) || CHECK_NRF_BOTH(mode)) NRFSerial.println("RADIOS");

//     // --- 3. VÒNG LẶP MENU ---
//     while (hopmenu) {
//         // Đọc UART (nếu có)
//         if (CHECK_NRF_UART(mode) || CHECK_NRF_BOTH(mode)) {
//             if (NRFSerial.available()) {
//                 String incomingNRFs = NRFSerial.readStringUntil('\n');
//                 incomingNRFs.trim();
//                 if (incomingNRFs.length() == 1 && isDigit(incomingNRFs.charAt(0))) {
//                     NRFOnline = incomingNRFs.toInt();
//                     if (CHECK_NRF_BOTH(mode)) NRFOnline += NRFSPI;
//                     redraw = true;
//                 }
//             }
//         }

//         if (redraw) {
//             if (fullRedraw) {
//                 drawMainBorder();
//                 drawMainBorderWithTitle("NRF Jammer Config");
//                 tft.setTextSize(FM);
//                 int frameHeight = (itemsPerPage * menuGap) + 4;
//                 tft.drawRoundRect(8, menuStartY - 4, tftWidth - 16, frameHeight, 4, bruceConfig.priColor);
//                 fullRedraw = false;
//             }

//             if (menuIndex >= listOffset + itemsPerPage) listOffset = menuIndex - itemsPerPage + 1;
//             if (menuIndex < listOffset) listOffset = menuIndex;

//             for (int i = 0; i < itemsPerPage; i++) {
//                 int actualIndex = listOffset + i;
//                 int yPos = menuStartY + (i * menuGap);
//                 tft.fillRoundRect(10, yPos - 2, tftWidth - 20, 18, 4, bruceConfig.bgColor);
//                 if (actualIndex > menuMax) continue;
//                 if (actualIndex == menuIndex) {
//                     tft.fillRoundRect(10, yPos - 2, tftWidth - 20, 18, 4, bruceConfig.priColor);
//                     tft.setTextColor(bruceConfig.bgColor);
//                 } else {
//                     tft.setTextColor(bruceConfig.priColor);
//                 }
//                 tft.setCursor(14, yPos);
//                 tft.println(menuItems[actualIndex]);
//             }
//             tft.setTextColor(bruceConfig.priColor);
//             redraw = false;
//         }

//         // Xử lý phím
//         if (check(EscPress)) {
//             if (CHECK_NRF_SPI(mode)) {
//                 NRFradio.stopConstCarrier();
//                 NRFradio.powerDown();
//             }
//             return;
//         }
//         if (check(NextPress)) {
//             menuIndex++;
//             if (menuIndex > menuMax) menuIndex = 0;
//             redraw = true;
//             vTaskDelay(150 / portTICK_PERIOD_MS);
//         }
//         if (check(PrevPress)) {
//             menuIndex--;
//             if (menuIndex < 0) menuIndex = menuMax;
//             redraw = true;
//             vTaskDelay(150 / portTICK_PERIOD_MS);
//         }

//         if (check(SelPress)) {
//             switch (menuIndex) {
//                 case 0: // Test
//                     isRandomMode = true;
//                     randomMin = 1;
//                     randomMax = 125;
//                     modeTitle = "TEST RANDOM";
//                     break;
//                 case 1: // WiFi
//                     selectedChannels = CH_WIFI;
//                     channelCount = sizeof(CH_WIFI);
//                     modeTitle = "WIFI JAMMER";
//                     break;
//                 case 2: // BLEch
//                     selectedChannels = CH_BLE;
//                     channelCount = sizeof(CH_BLE);
//                     modeTitle = "BLE CH JAMMER";
//                     break;
//                 case 3: // BLE Adv Pri
//                     selectedChannels = CH_BLE_ADV;
//                     channelCount = sizeof(CH_BLE_ADV);
//                     modeTitle = "BLE ADV JAMMER";
//                     break;
//                 case 4: // Bluetooth
//                     selectedChannels = CH_BLUETOOTH;
//                     channelCount = sizeof(CH_BLUETOOTH);
//                     modeTitle = "BLUETOOTH JAMMER";
//                     break;
//                 case 5: // USB
//                     selectedChannels = CH_USB;
//                     channelCount = sizeof(CH_USB);
//                     modeTitle = "USB JAMMER";
//                     break;
//                 case 6: // Video Stream
//                     selectedChannels = CH_VIDEO;
//                     channelCount = sizeof(CH_VIDEO);
//                     modeTitle = "VIDEO JAMMER";
//                     break;
//                 case 7: // RC
//                     selectedChannels = CH_RC;
//                     channelCount = sizeof(CH_RC);
//                     modeTitle = "RC JAMMER";
//                     break;
//                 case 8: // Full (quét toàn bộ 2-80)
//                     selectedChannels = CH_BLUETOOTH;
//                     channelCount = sizeof(CH_BLUETOOTH);
//                     modeTitle = "FULL JAMMER";
//                     break;
//                 case 9: // Exit
//                     if (CHECK_NRF_SPI(mode)) {
//                         NRFradio.stopConstCarrier();
//                         NRFradio.powerDown();
//                     }
//                     return;
//             }
//             runJammer = true;
//             hopmenu = false;
//         }
//     }

//     // --- 4. THỰC HIỆN JAMMING (CW thuần túy) ---
//     if (runJammer) {
//         drawMainBorder();
//         drawMainBorderWithTitle(modeTitle);
//         tft.setTextSize(FM);
//         tft.setCursor(10, 70);
//         tft.println("STATUS: ACTIVE");
//         tft.setCursor(10, 90);
//         if (!isRandomMode && selectedChannels) {
//             tft.printf("Channels: %d items", channelCount);
//         } else {
//             tft.printf("Random: %d-%d MHz", 2400 + randomMin, 2400 + randomMax);
//         }

//         if (CHECK_NRF_SPI(mode)) {
//             size_t idx = 0;
//             uint8_t currentChannel = 0;

//             while (!check(SelPress)) {
//                 // Xác định kênh tiếp theo
//                 if (isRandomMode) {
//                     currentChannel = randomMin + (rand() % (randomMax - randomMin + 1));
//                 } else {
//                     currentChannel = pgm_read_byte(&selectedChannels[idx]);
//                     idx++;
//                     if (idx >= channelCount) idx = 0;
//                 }

//                 // Chuyển kênh – sóng mang tự động theo, không cần delay
//                 NRFradio.setChannel(currentChannel);
//             }

//             // Tắt sóng mang và power down
//             NRFradio.stopConstCarrier();
//             NRFradio.powerDown();
//         } else if (CHECK_NRF_UART(mode) || CHECK_NRF_BOTH(mode)) {
//             while (!check(SelPress)) { vTaskDelay(50 / portTICK_PERIOD_MS); }
//             NRFSerial.println("OFF");
//         }
//     }
// }

void nrf_channel_jammer() {
    int OnX = 0;
    NRF24_MODE mode = nrf_setMode();
    uint8_t NRFOnline = 1;
    uint8_t NRFSPI = 0;
    if (nrf_start(mode)) {

        int channel = 50;
        bool redraw = true;
        if (CHECK_NRF_SPI(mode)) {
            NRFradio.powerUp();
            NRFradio.setAutoAck(false);
            NRFradio.setRetries(0, 0);
            NRFradio.setPALevel(RF24_PA_MAX, true);
            NRFradio.setDataRate(RF24_2MBPS);
            NRFradio.setCRCLength(RF24_CRC_DISABLED);
            NRFradio.setChannel(0);
            NRFradio.stopListening();
            NRFradio.startConstCarrier(RF24_PA_MAX, 50);
            if (!NRFradio.setDataRate(RF24_2MBPS)) {
                // Optionally log error or handle failure
            }
            NRFSPI = 1;
        }

        drawMainBorder();
        vTaskDelay(50 / portTICK_PERIOD_MS);
        NRFSerial.println("RADIOS");
        vTaskDelay(50 / portTICK_PERIOD_MS);

        while (!check(SelPress)) {
            if (CHECK_NRF_UART(mode) || CHECK_NRF_BOTH(mode)) {
                if (OnX == 0) {
                    NRFSerial.println("RADIOS");
                    vTaskDelay(250 / portTICK_PERIOD_MS);
                }
                if (NRFSerial.available()) {
                    String incomingNRFs = NRFSerial.readStringUntil('\n');
                    incomingNRFs.trim();
                    if (incomingNRFs.length() == 1 && isDigit(incomingNRFs.charAt(0))) {

                        NRFOnline = (incomingNRFs.toInt());
                        if (CHECK_NRF_BOTH(mode)) { NRFOnline = (incomingNRFs.toInt()) + NRFSPI; }
                        redraw = true;
                        OnX = 1;
                    }
                }
            }

            if (redraw) {
                int freq = 2400 + channel;
                tft.setCursor(10, 35);
                tft.setTextSize(FM);
                tft.println("NRF Channel Jammer");
                tft.setCursor(10, tft.getCursorY() + 25);
                tft.println("STATUS : " + String(NRFOnline) + " ACTIVE");
                tft.fillRect(10, 100, tftWidth - 20, FM * LH, bruceConfig.bgColor);
                tft.setCursor(10, 100);
                tft.print("MODE : CH " + String(channel));
                tft.setCursor(10, 116);
                tft.fillRect(10, 116, tftWidth - 20, FM * LH, bruceConfig.bgColor);
                tft.printf("Freq : %d MHz", freq);
                if (CHECK_NRF_UART(mode) || CHECK_NRF_BOTH(mode)) {
                    NRFSerial.println("CH_" + String(channel));
                }
                tft.drawRoundRect(5, 5, tftWidth - 10, tftHeight - 10, 5, bruceConfig.priColor);
                redraw = false;
                vTaskDelay(200 / portTICK_PERIOD_MS);
            }

            if (check(NextPress)) {

                channel++;
                if (channel > 125) channel = 1;
                if (CHECK_NRF_SPI(mode)) {
                    NRFradio.setChannel(channel);
                    NRFradio.startConstCarrier(RF24_PA_MAX, channel);
                }

                redraw = true;
            }
            if (check(PrevPress)) {

                channel--;
                if (channel < 1) channel = 125;
                if (CHECK_NRF_SPI(mode)) {
                    NRFradio.setChannel(channel);
                    NRFradio.startConstCarrier(RF24_PA_MAX, channel);
                }
                redraw = true;
            }
        }

        if (CHECK_NRF_SPI(mode)) NRFradio.stopConstCarrier();
        if (CHECK_NRF_UART(mode) || CHECK_NRF_BOTH(mode)) { NRFSerial.println("OFF"); }

    } else {
        displayError("NRF24 not found");
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
}

void nrf_channel_hopper() {
    NRF24_MODE mode = NRF_MODE_SPI;
    uint8_t NRFOnline = 0;
    uint8_t NRFSPI = 0;

    // --- Khởi tạo NRF ---
    if (!nrf_start(mode)) {
        displayError("NRF24 not found");
        vTaskDelay(100 / portTICK_PERIOD_MS);
        return;
    }

    if (CHECK_NRF_SPI(mode)) {
        NRFradio.powerUp();
        NRFradio.setAutoAck(false);
        NRFradio.setRetries(0, 0);
        NRFradio.setPALevel(RF24_PA_MAX, true);
        NRFradio.setDataRate(RF24_2MBPS);
        NRFradio.setCRCLength(RF24_CRC_DISABLED);
        NRFradio.setChannel(0);
        NRFradio.stopListening();
        NRFradio.startConstCarrier(RF24_PA_MAX, 50);
        if (!NRFradio.setDataRate(RF24_2MBPS)) NRFSPI = 1;
    }

    int startChannel = 0;
    int stopChannel = 80;
    int stepSize = 2;

    // --- Biến Menu ---
    int menuIndex = 0;
    const int totalItems = 5;
    int topVisibleIndex = 0;

    // Cấu hình hiển thị
    int headerHeight = 60;
    int lineHeight = 20;
    // Trừ hao thêm pixel để tránh đè viền dưới
    int maxVisibleItems = (tftHeight - headerHeight - 15) / lineHeight;

    bool redraw = true;
    bool editMode = false;
    bool runJammer = false;
    bool hopmenu = true;

    vTaskDelay(350 / portTICK_PERIOD_MS);
    NRFSerial.println("RADIOS");
    vTaskDelay(100 / portTICK_PERIOD_MS);

    while (hopmenu) {
        // --- Xử lý UART ---
        if (CHECK_NRF_UART(mode) || CHECK_NRF_BOTH(mode)) {
            if (NRFSerial.available()) {
                String incomingNRFs = NRFSerial.readStringUntil('\n');
                incomingNRFs.trim();
                if (incomingNRFs.length() == 1 && isDigit(incomingNRFs.charAt(0))) {
                    NRFOnline = (incomingNRFs.toInt());
                    if (CHECK_NRF_BOTH(mode)) { NRFOnline = (incomingNRFs.toInt()) + NRFSPI; }
                    redraw = true;
                }
            }
        }

        if (redraw) {
            // 1. Vẽ khung cơ bản & Tiêu đề
            drawMainBorder();
            drawMainBorderWithTitle("NRF Hopper Config");
            tft.setTextSize(FM);

            // 2. Chuẩn bị nội dung Menu
            String menuItems[5];
            menuItems[0] = "Start : CH " + String(startChannel);
            menuItems[1] = "Stop  : CH " + String(stopChannel);
            menuItems[2] = "Step  : " + String(stepSize) + " mhz";
            menuItems[3] = "Start Jammer";
            menuItems[4] = "Exit";

            // 3. Logic Cuộn
            if (menuIndex >= topVisibleIndex + maxVisibleItems) {
                topVisibleIndex = menuIndex - maxVisibleItems + 1;
            }
            if (menuIndex < topVisibleIndex) { topVisibleIndex = menuIndex; }

            // 4. Xóa vùng Menu cũ (FIX: Co nhỏ vùng xóa để không xóa mất viền)
            // X bắt đầu từ 7 (thay vì 5), Width trừ đi 14 (thay vì 10)
            tft.fillRect(
                7, headerHeight, tftWidth - 14, tftHeight - headerHeight - 7, bruceConfig.bgColor
            ); // <--- FIX QUAN TRỌNG

            // 5. Vẽ các mục Menu
            for (int i = 0; i < maxVisibleItems; i++) {
                int itemIndex = topVisibleIndex + i;
                if (itemIndex >= totalItems) break;

                int drawY = headerHeight + (i * lineHeight);

                if (itemIndex == menuIndex) {
                    // Vẽ thanh chọn (Highlight) co vào trong 1 chút để đẹp hơn
                    tft.drawRect(
                        7, drawY - 2, tftWidth - 14, lineHeight - 2, bruceConfig.priColor
                    ); // <--- FIX WIDTH

                    if (editMode) {
                        tft.fillRect(
                            9, drawY - 2, tftWidth - 18, lineHeight - 2, bruceConfig.priColor
                        ); // <--- FIX WIDTH
                        tft.setTextColor(bruceConfig.bgColor);
                    } else {
                        tft.setTextColor(bruceConfig.priColor);
                    }
                } else {
                    tft.setTextColor(bruceConfig.priColor);
                }

                tft.setCursor(12, drawY); // <--- FIX: Dịch chữ vào 1 chút (12px)
                tft.print(menuItems[itemIndex]);

                tft.setTextColor(bruceConfig.priColor);
            }

            // 6. (Tùy chọn) Vẽ lại viền bo tròn một lần nữa để chắc chắn không bị mất
            tft.drawRoundRect(
                5, 5, tftWidth - 10, tftHeight - 10, 5, bruceConfig.priColor
            ); // <--- FIX: Vẽ đè lại viền

            if (CHECK_NRF_UART(mode) || CHECK_NRF_BOTH(mode)) {
                NRFSerial.println(
                    "HOPPER_" + String(startChannel) + "_" + String(stopChannel) + "_" + String(stepSize)
                );
            }
            redraw = false;
        }

        // --- Các nút bấm (Giữ nguyên) ---
        if (check(EscPress)) {
            NRFradio.powerDown();
            hopmenu = false;
            return;
        }

        if (check(NextPress)) {
            if (editMode) {
                if (menuIndex == 0) startChannel = (startChannel % 125) + 1;
                if (menuIndex == 1) stopChannel = (stopChannel % 125) + 1;
                if (menuIndex == 2) stepSize = (stepSize % 10) + 1;
            } else {
                menuIndex++;
                if (menuIndex >= totalItems) menuIndex = 0;
            }
            redraw = true;
            vTaskDelay(150 / portTICK_PERIOD_MS);
        }

        if (check(PrevPress)) {
            if (editMode) {
                if (menuIndex == 0) startChannel = (startChannel - 2 + 125) % 125 + 1;
                if (menuIndex == 1) stopChannel = (stopChannel - 2 + 125) % 125 + 1;
                if (menuIndex == 2) stepSize = (stepSize - 2 + 10) % 10 + 1;
            } else {
                menuIndex--;
                if (menuIndex < 0) menuIndex = totalItems - 1;
            }
            redraw = true;
            vTaskDelay(150 / portTICK_PERIOD_MS);
        }

        if (check(SelPress)) {
            if (menuIndex == 3 && !editMode) {
                runJammer = true;
                hopmenu = false;
            } else if (menuIndex == 4 && !editMode) {
                hopmenu = false;
                return;
            } else {
                if (menuIndex < 3) editMode = !editMode;
            }
            redraw = true;
            vTaskDelay(150 / portTICK_PERIOD_MS);
        }
    }

    // --- Màn hình Jammer đang chạy (Giữ nguyên) ---
    if (runJammer) {
        int channel = startChannel;
        drawMainBorder();

        int safeBottom = tftHeight - 10;
        int y = 35;
        int lh = FM * LH;

        drawMainBorderWithTitle("NRF Hopper Jammer");
        tft.setTextSize(FM);

        y += lh + 10;
        if (y < safeBottom) {
            tft.setCursor(10, y);
            tft.println("STATUS : " + String(NRFOnline) + " ACTIVE");
        }

        y += lh + 5;
        if (y < safeBottom) {
            tft.setCursor(10, y);
            tft.printf("Range : %d - %d", startChannel, stopChannel);
        }

        y += lh + 5;
        if (y < safeBottom) {
            tft.setCursor(10, y);
            tft.printf("Step  : %d", stepSize);
        }

        while (!check(SelPress)) {
            channel += stepSize;
            if (channel > stopChannel) channel = startChannel;
            if (CHECK_NRF_SPI(mode)) NRFradio.setChannel(channel);
        }

        if (CHECK_NRF_SPI(mode)) {
            NRFradio.stopConstCarrier();
            NRFradio.powerDown();
        }
        if (CHECK_NRF_UART(mode) || CHECK_NRF_BOTH(mode)) {
            NRFSerial.println("OFF");
            NRFradio.powerDown();
        }
    }
}
