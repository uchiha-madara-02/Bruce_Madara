#include "rf_jammer.h"
#include "core/display.h"
#include "hal/gpio_ll.h"
#include "rf_utils.h"
#include <Arduino.h>
#include <ELECHOUSE_CC1101_SRC_DRV.h>

static const uint32_t MAX_JAM_TIME_MS = 20000;
static const uint32_t MAX_SEQUENCE = 50;
static const uint32_t DURATION_CYCLES = 3;

const uint8_t jamdata[128] = {
    // Khối 1: 0xAA, 0x55 lặp lại
    0xAA,
    0x55,
    0xAA,
    0x55,
    0xAA,
    0x55,
    0xAA,
    0x55,
    0xAA,
    0x55,
    0xAA,
    0x55,
    0xAA,
    0x55,
    0xAA,
    0x55,

    // Khối 2: 0xFF, 0x00 lặp lại
    0xFF,
    0x00,
    0xFF,
    0x00,
    0xFF,
    0x00,
    0xFF,
    0x00,
    0xFF,
    0x00,
    0xFF,
    0x00,
    0xFF,
    0x00,
    0xFF,
    0x00,

    // Khối 3: 0xF0, 0x0F lặp lại
    0xF0,
    0x0F,
    0xF0,
    0x0F,
    0xF0,
    0x0F,
    0xF0,
    0x0F,
    0xF0,
    0x0F,
    0xF0,
    0x0F,
    0xF0,
    0x0F,
    0xF0,
    0x0F,

    // Khối 4: 0xCC, 0x33 lặp lại (11001100, 00110011)
    0xCC,
    0x33,
    0xCC,
    0x33,
    0xCC,
    0x33,
    0xCC,
    0x33,
    0xCC,
    0x33,
    0xCC,
    0x33,
    0xCC,
    0x33,
    0xCC,
    0x33,

    // Khối 5: đếm tăng 0x00..0x0F
    0x00,
    0x01,
    0x02,
    0x03,
    0x04,
    0x05,
    0x06,
    0x07,
    0x08,
    0x09,
    0x0A,
    0x0B,
    0x0C,
    0x0D,
    0x0E,
    0x0F,

    // Khối 6: đếm giảm 0x0F..0x00
    0x0F,
    0x0E,
    0x0D,
    0x0C,
    0x0B,
    0x0A,
    0x09,
    0x08,
    0x07,
    0x06,
    0x05,
    0x04,
    0x03,
    0x02,
    0x01,
    0x00,

    // Khối 7: pattern ngẫu nhiên cố định (tôi chọn sẵn)
    0x1A,
    0x2B,
    0x3C,
    0x4D,
    0x5E,
    0x6F,
    0x78,
    0x89,
    0x9A,
    0xAB,
    0xBC,
    0xCD,
    0xDE,
    0xEF,
    0xF0,
    0x01,

    // Khối 8: toàn 0xFF (sóng mang thuần)
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF
};

RFJammer::RFJammer(bool full) : fullJammer(full) { setup(); }

RFJammer::~RFJammer() { deinitRfModule(); }

void RFJammer::setup() {
    nTransmitterPin = bruceConfigPins.rfTx;
    if (!initRfModule("tx")) return;

    if (bruceConfigPins.rfModule == CC1101_SPI_MODULE) { nTransmitterPin = bruceConfigPins.CC1101_bus.io0; }

    sendRF = true;
    display_banner();

    if (fullJammer) run_full_jammer();
    else run_itmt_jammer();
}

void RFJammer::display_banner() {
    drawMainBorderWithTitle("RF Jammer");
    printSubtitle(String(fullJammer ? "Full Jammer" : "Intermittent Jammer"));
    padprintln("Sending...");
    padprintln("");
    padprintln("");

    tft.setTextColor(getColorVariation(bruceConfig.priColor), bruceConfig.bgColor);
    padprintln("Press [ESC] for options.");
    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
}

void RFJammer::run_full_jammer() {
    digitalWrite(nTransmitterPin, HIGH);
    uint32_t startTime = millis();
    uint32_t lastCheckTime = startTime;

    while (sendRF) {
        static uint8_t microPulse = 0;

        if (micros() % 100 < 2) {
            digitalWrite(nTransmitterPin, LOW);
            delayMicroseconds(1);
            digitalWrite(nTransmitterPin, HIGH);
        }

        uint32_t currentTime = millis();
        if (currentTime - lastCheckTime > 100) {
            lastCheckTime = currentTime;

            if (check(EscPress) || (currentTime - startTime > MAX_JAM_TIME_MS)) {
                sendRF = false;
                returnToMenu = true;
                break;
            }
        }

        if (currentTime % 500 < 10) {
            digitalWrite(nTransmitterPin, LOW);
            delayMicroseconds(5);
            digitalWrite(nTransmitterPin, HIGH);
        }
    }
    digitalWrite(nTransmitterPin, LOW);
}

void RFJammer::run_itmt_jammer() {
    uint32_t startTime = millis();
    uint32_t lastCheckTime = startTime;

    uint32_t sequenceValues[MAX_SEQUENCE];
    for (int i = 0; i < MAX_SEQUENCE; i++) { sequenceValues[i] = 10 * (i + 1); }

    while (sendRF) {
        for (int sequence = 0; sequence < MAX_SEQUENCE && sendRF; sequence++) {
            uint32_t pulseWidth = sequenceValues[sequence];

            for (int duration = 0; duration < DURATION_CYCLES && sendRF; duration++) {
                send_optimized_pulse(pulseWidth);

                uint32_t currentTime = millis();
                if (currentTime - lastCheckTime > 50) {
                    lastCheckTime = currentTime;

                    if (check(EscPress) || (currentTime - startTime > MAX_JAM_TIME_MS)) {
                        sendRF = false;
                        returnToMenu = true;
                        break;
                    }
                }
            }
        }

        if (sendRF) { send_random_pattern(100); }
    }
    digitalWrite(nTransmitterPin, LOW);
}

void RFJammer::send_optimized_pulse(int width) {
    digitalWrite(nTransmitterPin, HIGH);

    for (uint32_t i = 0; i < width; i += 10) {
        digitalWrite(nTransmitterPin, HIGH);
        delayMicroseconds(5);

        if (i % 20 == 0) {
            digitalWrite(nTransmitterPin, LOW);
            delayMicroseconds(2);
            digitalWrite(nTransmitterPin, HIGH);
        }

        delayMicroseconds(5);
    }

    digitalWrite(nTransmitterPin, LOW);

    uint32_t lowPeriod = width + (width % 23);
    for (uint32_t i = 0; i < lowPeriod; i += 10) {
        digitalWrite(nTransmitterPin, LOW);
        delayMicroseconds(10);
    }
}

void RFJammer::send_random_pattern(int numPulses) {
    uint32_t startTime = millis();

    for (int i = 0; i < numPulses && sendRF; i++) {
        uint32_t pulseWidth = 5 + (millis() % 46);

        digitalWrite(nTransmitterPin, HIGH);
        delayMicroseconds(pulseWidth);

        digitalWrite(nTransmitterPin, LOW);

        uint32_t spaceWidth = 5 + (micros() % 96);
        delayMicroseconds(spaceWidth);

        if (millis() - startTime > 100) { break; }
    }
}

// void startJammerRaw(uint32_t frequency, uint32_t durationMs) {
//     if (!initRfModule("", frequency / 1000000.0)) return;

//     int nTransmitterPin = bruceConfigPins.rfTx;
//     if (bruceConfigPins.rfModule == CC1101_SPI_MODULE) {
//         nTransmitterPin = bruceConfigPins.CC1101_bus.io0;

//         ELECHOUSE_cc1101.Init();                        // Khởi tạo lại để sạch cấu hình cũ
//         ELECHOUSE_cc1101.setModulation(2);              // Chế độ ASK/OOK (phá cửa cuốn, remote)
//         ELECHOUSE_cc1101.setPA(12);                     // Công suất tối đa
//         ELECHOUSE_cc1101.setMHZ(frequency / 1000000.0); // Set tần số mục tiêu

//         // --- CẤU HÌNH NÂNG CAO ĐỂ JAM MẠNH HƠN ---
//         ELECHOUSE_cc1101.setDRate(500);           // Tốc độ dữ liệu ảo cực cao
//         ELECHOUSE_cc1101.SpiWriteReg(0x08, 0x32); // Chế độ không đồng bộ (Asynchronous) - BẮT BUỘC
//         ELECHOUSE_cc1101.SpiWriteReg(0x10, 0xF8); // Mở rộng bộ lọc đầu ra

//         // Bật chế độ phát
//         ioExpander.turnPinOnOff(IO_EXP_CC_RX, LOW);
//         ioExpander.turnPinOnOff(IO_EXP_CC_TX, HIGH);
//         ELECHOUSE_cc1101.SetTx();
//     }

//     pinMode(nTransmitterPin, OUTPUT);
//     uint32_t startTime = millis();
//     int loopCounter = 0; // Biến đếm vòng lặp

//     // Vòng lặp phá sóng cường độ cao
//     while (millis() - startTime < durationMs) {
//         // Kỹ thuật Brute-force RF:
//         digitalWrite(nTransmitterPin, HIGH);
//         delayMicroseconds(random(10, 150));

//         digitalWrite(nTransmitterPin, LOW);
//         delayMicroseconds(random(10, 150));

//         loopCounter++;

//         // Kiểm tra nút bấm sau mỗi 50 vòng lặp (khoảng vài ms một lần)
//         if (loopCounter >= 50) {
//             loopCounter = 0;

//             // Nhả CPU 1ms để ESP32 cập nhật trạng thái nút bấm và chống treo Watchdog
//             vTaskDelay(1 / portTICK_PERIOD_MS);

//             // Quét nút bấm ESC bình thường
//             if (check(EscPress)) { break; }
//         }
//     }

//     digitalWrite(nTransmitterPin, LOW);
//     deinitRfModule();
// }

void startJammerRaw(uint32_t frequency, uint32_t durationMs) {

    if (!initRfModule("", frequency / 1000000.0)) return;

    int nTransmitterPin = bruceConfigPins.rfTx;

    if (bruceConfigPins.rfModule == CC1101_SPI_MODULE) {
        nTransmitterPin = bruceConfigPins.CC1101_bus.io0;

        ELECHOUSE_cc1101.Init();
        ELECHOUSE_cc1101.setModulation(2); // ASK/OOK
        ELECHOUSE_cc1101.setPA(12);        // max power
        ELECHOUSE_cc1101.setMHZ(frequency / 1000000.0);

        ELECHOUSE_cc1101.setRxBW(812.50);
        ELECHOUSE_cc1101.setDRate(500);
        ELECHOUSE_cc1101.SpiWriteReg(0x08, 0x32); // async mode
        ELECHOUSE_cc1101.SpiWriteReg(0x10, 0xF8);

        ioExpander.turnPinOnOff(IO_EXP_CC_RX, LOW);
        ioExpander.turnPinOnOff(IO_EXP_CC_TX, HIGH);

        ELECHOUSE_cc1101.SetTx();
    }

    pinMode(nTransmitterPin, OUTPUT);

    uint32_t startTime = millis();
    int loopCounter = 0;

    gpio_dev_t *gpio_dev = &GPIO;

    while (millis() - startTime < durationMs) {

        uint32_t delay1 = random(5, 80);
        uint32_t delay2 = random(5, 80);

        // 🔥 FAST TOGGLE bằng LL
        gpio_ll_set_level(gpio_dev, nTransmitterPin, 1);
        esp_rom_delay_us(delay1);

        gpio_ll_set_level(gpio_dev, nTransmitterPin, 0);
        esp_rom_delay_us(delay2);

        loopCounter++;

        if (loopCounter >= 200) {
            loopCounter = 0;

            vTaskDelay(1 / portTICK_PERIOD_MS);

            if (check(EscPress)) break;
        }
    }

    // đảm bảo tắt pin
    gpio_ll_set_level(gpio_dev, nTransmitterPin, 0);

    deinitRfModule();
}

void rf_jammer_config() {
    // 1. Dữ liệu đầu vào
    const int freqCount = sizeof(subghz_frequency_list) / sizeof(subghz_frequency_list[0]);
    static int currentFreqIdx = 36; // Mặc định 433.920 MHz
    uint32_t jamDuration = 30;

    // 2. Biến điều khiển Menu
    int menuIndex = 0;
    const int totalItems = 4;

    // 3. Cấu hình giao diện cho màn hình
    int topVisibleIndex = 0;
    int lineHeight = 20;   // Thu gọn lại thành 20 giống nrf_jammer
    int headerHeight = 63; // Y bắt đầu của menu
    // Tính toán số lượng mục hiển thị
    int maxVisibleItems = (tftHeight - headerHeight - 10) / lineHeight;

    bool editMode = false;
    bool redraw = true;
    bool exitGui = false;

    while (!exitGui) {
        // --- VẼ MENU ---
        if (redraw) {
            tft.fillScreen(bruceConfig.bgColor); // Xóa màn hình
            drawMainBorder();
            drawMainBorderWithTitle("RF JAMMER CONFIG");
            tft.setTextSize(FM);

            // --- Logic tính toán khung cuộn ---
            if (menuIndex >= topVisibleIndex + maxVisibleItems) {
                topVisibleIndex = menuIndex - maxVisibleItems + 1;
            }
            if (menuIndex < topVisibleIndex) { topVisibleIndex = menuIndex; }

            // --- VẼ KHUNG BAO NGOÀI TOÀN BỘ MENU ---
            int visibleItemsCount = min(maxVisibleItems, totalItems - topVisibleIndex);
            int outerFrameY = headerHeight - 6;
            int outerFrameH = (visibleItemsCount * lineHeight) + 8; // Đệm trên dưới 4px

            // Vẽ khung bo góc bao quanh toàn bộ list
            tft.drawRoundRect(6, outerFrameY, tftWidth - 12, outerFrameH, 6, bruceConfig.priColor);
            // ---------------------------------------

            // --- Vẽ danh sách Menu bên trong ---
            for (int i = 0; i < maxVisibleItems; i++) {
                int itemIndex = topVisibleIndex + i;
                if (itemIndex >= totalItems) break;

                int drawY = headerHeight + (i * lineHeight);

                if (itemIndex == menuIndex) {
                    if (editMode) {
                        // CHẾ ĐỘ CHỈNH SỬA: Tạo hiệu ứng ô nhập liệu (Input Box)
                        tft.fillRoundRect(
                            10, drawY - 2, tftWidth - 16, 18, 5, bruceConfig.priColor
                        ); // Vẽ nền
                        tft.fillRoundRect(
                            12, drawY - 1, tftWidth - 20, 16, 4, bruceConfig.bgColor
                        ); // Khoét lỗ giữa
                        tft.setTextColor(bruceConfig.priColor);
                    } else {
                        // CHẾ ĐỘ CHỌN BÌNH THƯỜNG: Khung đặc, chữ âm bản
                        tft.fillRoundRect(10, drawY - 2, tftWidth - 16, 18, 5, bruceConfig.priColor);
                        tft.setTextColor(bruceConfig.bgColor);
                    }
                    tft.setCursor(14, drawY);
                    tft.print("> ");
                } else {
                    // MỤC KHÔNG ĐƯỢC CHỌN
                    tft.setTextColor(bruceConfig.priColor);
                    tft.setCursor(14, drawY);
                    tft.print("  ");
                }

                // Hiển thị nội dung từng dòng
                switch (itemIndex) {
                    case 0: tft.printf("Freq: %.3f MHz", subghz_frequency_list[currentFreqIdx]); break;
                    case 1: tft.printf("Time: %d Sec", jamDuration); break;
                    case 2: tft.print("START JAMMER"); break; // Bỏ dấu < > đi vì đã có con trỏ >
                    case 3: tft.print("Exit"); break;
                }
            }
            redraw = false;
        }

        // --- Xử lý đầu vào (Nút bấm) ---
        if (check(EscPress)) exitGui = true;

        if (check(NextPress)) {
            if (editMode) {
                if (menuIndex == 0) currentFreqIdx = (currentFreqIdx + 1) % freqCount;
                if (menuIndex == 1) jamDuration += 5;
            } else {
                menuIndex = (menuIndex + 1) % totalItems;
            }
            redraw = true;
            vTaskDelay(100 / portTICK_PERIOD_MS);
        }

        if (check(PrevPress)) {
            if (editMode) {
                if (menuIndex == 0) currentFreqIdx = (currentFreqIdx - 1 + freqCount) % freqCount;
                if (menuIndex == 1 && jamDuration > 5) jamDuration -= 5;
            } else {
                menuIndex = (menuIndex - 1 + totalItems) % totalItems;
            }
            redraw = true;
            vTaskDelay(100 / portTICK_PERIOD_MS);
        }

        if (check(SelPress)) {
            if (menuIndex == 2 && !editMode) { // START
                // --- 1. VẼ GIAO DIỆN BÁO ĐANG JAMMING (POPUP CĂN GIỮA) ---
                tft.fillScreen(bruceConfig.bgColor);

                // Tự động chọn font theo kích thước màn hình
                auto autoFont = (tftHeight >= 240) ? FM : FP;
                tft.setTextSize(autoFont);
                tft.setTextColor(bruceConfig.priColor);

                // Tính toán kích thước và tọa độ khung tự động căn giữa
                int frameW = tftWidth - 20;                 // Cách 2 lề trái/phải 10px
                int frameH = (tftHeight >= 240) ? 120 : 85; // Chiều cao khung tự co giãn
                int frameX = 10;
                int frameY = (tftHeight - frameH) / 2; // Công thức đưa khung vào giữa màn hình dọc

                tft.drawRoundRect(frameX, frameY, frameW, frameH, 6, bruceConfig.priColor);

                // Căn giữa dòng 1: Status
                String statusStr = "TRANSMITTING...";
                tft.setCursor((tftWidth - tft.textWidth(statusStr)) / 2, frameY + frameH * 0.15);
                tft.print(statusStr);

                // Căn giữa dòng 2: Tần số
                char freqStr[32];
                sprintf(freqStr, "Freq: %.3f MHz", subghz_frequency_list[currentFreqIdx]);
                tft.setCursor((tftWidth - tft.textWidth(freqStr)) / 2, frameY + frameH * 0.42);
                tft.print(freqStr);

                // Kẻ vạch ngang phân cách nút bấm
                int lineY = frameY + frameH * 0.68;
                tft.drawLine(frameX + 10, lineY, frameX + frameW - 10, lineY, bruceConfig.priColor);

                // Căn giữa dòng 3: Hướng dẫn
                String escStr = "[ESC] to Stop";
                tft.setCursor((tftWidth - tft.textWidth(escStr)) / 2, frameY + frameH * 0.78);
                tft.print(escStr);

                // --- 2. BẮT ĐẦU PHÁ SÓNG ---
                uint32_t freqHz = (uint32_t)(subghz_frequency_list[currentFreqIdx] * 1000000);
                startJammerRaw(freqHz, jamDuration * 1000);

                // --- 3. ĐÁNH DẤU VẼ LẠI MENU KHI THOÁT ---
                redraw = true;
            } else if (menuIndex == 3 && !editMode) { // EXIT
                exitGui = true;
            } else {
                // Chỉ cho phép bật EditMode cho Freq và Time
                if (menuIndex < 2) editMode = !editMode;
            }

            redraw = true;
            vTaskDelay(200 / portTICK_PERIOD_MS);
        }
    }
}

void cc1101initialize(void) {
    ELECHOUSE_cc1101.Init();
    ELECHOUSE_cc1101.setCCMode(1);
    ELECHOUSE_cc1101.setModulation(2);
    ELECHOUSE_cc1101.setDeviation(47.60);
    ELECHOUSE_cc1101.setChannel(0);
    ELECHOUSE_cc1101.setChsp(199.95);
    ELECHOUSE_cc1101.setRxBW(812.50);
    ELECHOUSE_cc1101.setDRate(9.6);
    ELECHOUSE_cc1101.setPA(12);
    ELECHOUSE_cc1101.setSyncMode(2);
    ELECHOUSE_cc1101.setSyncWord(211, 145);
    ELECHOUSE_cc1101.setAdrChk(0);
    ELECHOUSE_cc1101.setAddr(0);
    ELECHOUSE_cc1101.setWhiteData(0);
    ELECHOUSE_cc1101.setPktFormat(0);
    ELECHOUSE_cc1101.setLengthConfig(1);
    ELECHOUSE_cc1101.setPacketLength(0);
    ELECHOUSE_cc1101.setCrc(0);
    ELECHOUSE_cc1101.setCRC_AF(0);
    ELECHOUSE_cc1101.setDcFilterOff(0);
    ELECHOUSE_cc1101.setManchester(0);
    ELECHOUSE_cc1101.setFEC(0);
    ELECHOUSE_cc1101.setPRE(0);
    ELECHOUSE_cc1101.setPQT(0);
    ELECHOUSE_cc1101.setAppendStatus(0);
}

// void cc1101initialize(void) {
//     ELECHOUSE_cc1101.Init();
//     ELECHOUSE_cc1101.setCCMode(0);      // Chế độ thường
//     ELECHOUSE_cc1101.setModulation(0);  // 2-FSK (hoặc giữ 2 nếu muốn ASK)
//     ELECHOUSE_cc1101.setDeviation(100); // 100 kHz (nếu dùng FSK)
//     ELECHOUSE_cc1101.setChannel(0);
//     ELECHOUSE_cc1101.setChsp(199.95);
//     ELECHOUSE_cc1101.setRxBW(812.50); // Băng thông rộng để quét nhanh
//     ELECHOUSE_cc1101.setDRate(100);   // 100 kbps (tăng tốc độ gửi)
//     ELECHOUSE_cc1101.setPA(12);
//     ELECHOUSE_cc1101.setSyncMode(0); // Tắt đồng bộ
//     ELECHOUSE_cc1101.setPktFormat(0);
//     ELECHOUSE_cc1101.setLengthConfig(1);  // Gói có độ dài cố định
//     ELECHOUSE_cc1101.setPacketLength(64); // Kích thước payload cố định (phù hợp jamdata)
//     ELECHOUSE_cc1101.setCrc(0);
//     ELECHOUSE_cc1101.setManchester(0);
//     ELECHOUSE_cc1101.setFEC(0);
//     ELECHOUSE_cc1101.setWhiteData(0);
// }

void rf_jammer() {
    // 0. Khởi tạo module CC1101 bằng cấu hình bạn đã cung cấp
    cc1101initialize();

    // 1. Dữ liệu đầu vào
    const int freqCount = sizeof(subghz_frequency_list) / sizeof(subghz_frequency_list[0]);
    static int currentFreqIdx = 36; // Dùng cho Single hoặc Start Range (VD: 433.920 MHz)
    static int stopFreqIdx = 40;    // Dùng cho Stop Range (VD: 434.000 MHz)
    static int jamMode = 0;         // 0: Single, 1: Range, 2: Hopper
    uint32_t jamDuration = 30;      // Thời gian (giây)

    // 2. Biến điều khiển Menu
    int menuIndex = 0;
    const int totalItems = 6; // Mode, Freq1, Freq2, Time, START, EXIT

    // 3. Cấu hình giao diện cho màn hình
    int topVisibleIndex = 0;
    int lineHeight = 20;
    int headerHeight = 63;
    int maxVisibleItems = (tftHeight - headerHeight - 10) / lineHeight;

    bool editMode = false;
    bool redraw = true;
    bool exitGui = false;

    // Biến phụ trợ cho CC1101
    byte payload = 64;      // Dữ liệu rác mặc định
    float rangeStep = 0.05; // Bước nhảy cho Range Mode (50kHz)

    while (!exitGui) {
        // --- VẼ MENU (giữ nguyên giao diện của bạn) ---
        if (redraw) {
            tft.fillScreen(bruceConfig.bgColor);
            drawMainBorder();
            drawMainBorderWithTitle("RF JAMMER PRO");
            tft.setTextSize(FM);

            if (menuIndex >= topVisibleIndex + maxVisibleItems) {
                topVisibleIndex = menuIndex - maxVisibleItems + 1;
            }
            if (menuIndex < topVisibleIndex) { topVisibleIndex = menuIndex; }

            int visibleItemsCount = min(maxVisibleItems, totalItems - topVisibleIndex);
            int outerFrameY = headerHeight - 6;
            int outerFrameH = (visibleItemsCount * lineHeight) + 8;

            tft.drawRoundRect(6, outerFrameY, tftWidth - 12, outerFrameH, 6, bruceConfig.priColor);

            for (int i = 0; i < maxVisibleItems; i++) {
                int itemIndex = topVisibleIndex + i;
                if (itemIndex >= totalItems) break;

                int drawY = headerHeight + (i * lineHeight);

                if (itemIndex == menuIndex) {
                    if (editMode) {
                        tft.fillRoundRect(10, drawY - 2, tftWidth - 16, 18, 5, bruceConfig.priColor);
                        tft.fillRoundRect(12, drawY - 1, tftWidth - 20, 16, 4, bruceConfig.bgColor);
                        tft.setTextColor(bruceConfig.priColor);
                    } else {
                        tft.fillRoundRect(10, drawY - 2, tftWidth - 16, 18, 5, bruceConfig.priColor);
                        tft.setTextColor(bruceConfig.bgColor);
                    }
                    tft.setCursor(14, drawY);
                    tft.print("> ");
                } else {
                    tft.setTextColor(bruceConfig.priColor);
                    tft.setCursor(14, drawY);
                    tft.print("  ");
                }

                // Hiển thị nội dung dựa trên Mode
                switch (itemIndex) {
                    case 0:
                        tft.print("Mode: ");
                        if (jamMode == 0) tft.print("Single");
                        else if (jamMode == 1) tft.print("Range");
                        else tft.print("Hopper");
                        break;
                    case 1:
                        if (jamMode == 1) tft.printf("Start: %.3f", subghz_frequency_list[currentFreqIdx]);
                        else if (jamMode == 0)
                            tft.printf("Freq: %.3f", subghz_frequency_list[currentFreqIdx]);
                        else tft.print("Freq: Auto List"); // Hopper dùng toàn bộ list
                        break;
                    case 2:
                        if (jamMode == 1) tft.printf("Stop : %.3f", subghz_frequency_list[stopFreqIdx]);
                        else tft.print("Stop : N/A");
                        break;
                    case 3: tft.printf("Time: %d Sec", jamDuration); break;
                    case 4: tft.print("START JAMMER"); break;
                    case 5: tft.print("Exit"); break;
                }
            }
            redraw = false;
        }

        // --- XỬ LÝ NÚT BẤM (giữ nguyên) ---
        if (check(EscPress)) exitGui = true;

        if (check(NextPress)) {
            if (editMode) {
                if (menuIndex == 0) jamMode = (jamMode + 1) % 3;
                if (menuIndex == 1 && jamMode != 2) currentFreqIdx = (currentFreqIdx + 1) % freqCount;
                if (menuIndex == 2 && jamMode == 1) stopFreqIdx = (stopFreqIdx + 1) % freqCount;
                if (menuIndex == 3) jamDuration += 5;
            } else {
                menuIndex = (menuIndex + 1) % totalItems;
            }
            redraw = true;
            vTaskDelay(100 / portTICK_PERIOD_MS);
        }

        if (check(PrevPress)) {
            if (editMode) {
                if (menuIndex == 0) jamMode = (jamMode - 1 + 3) % 3;
                if (menuIndex == 1 && jamMode != 2)
                    currentFreqIdx = (currentFreqIdx - 1 + freqCount) % freqCount;
                if (menuIndex == 2 && jamMode == 1) stopFreqIdx = (stopFreqIdx - 1 + freqCount) % freqCount;
                if (menuIndex == 3 && jamDuration > 5) jamDuration -= 5;
            } else {
                menuIndex = (menuIndex - 1 + totalItems) % totalItems;
            }
            redraw = true;
            vTaskDelay(100 / portTICK_PERIOD_MS);
        }

        if (check(SelPress)) {
            if (menuIndex == 4 && !editMode) { // START JAMMER
                // ==================== VẼ POPUP ĐANG PHÁ SÓNG ====================
                tft.fillScreen(bruceConfig.bgColor);
                auto autoFont = (tftHeight >= 240) ? FM : FP;
                tft.setTextSize(autoFont);
                tft.setTextColor(bruceConfig.priColor);

                int frameW = tftWidth - 20;
                int frameH = (tftHeight >= 240) ? 120 : 85;
                int frameX = 10;
                int frameY = (tftHeight - frameH) / 2;

                tft.drawRoundRect(frameX, frameY, frameW, frameH, 6, bruceConfig.priColor);

                String statusStr = "JAMMING...";
                tft.setCursor((tftWidth - tft.textWidth(statusStr)) / 2, frameY + frameH * 0.15);
                tft.print(statusStr);

                char modeStr[32];
                if (jamMode == 0) sprintf(modeStr, "%.3f MHz", subghz_frequency_list[currentFreqIdx]);
                else if (jamMode == 1)
                    sprintf(
                        modeStr,
                        "%.3f - %.3f",
                        subghz_frequency_list[currentFreqIdx],
                        subghz_frequency_list[stopFreqIdx]
                    );
                else sprintf(modeStr, "Hopper Mode");

                tft.setCursor((tftWidth - tft.textWidth(modeStr)) / 2, frameY + frameH * 0.42);
                tft.print(modeStr);

                int lineY = frameY + frameH * 0.68;
                tft.drawLine(frameX + 10, lineY, frameX + frameW - 10, lineY, bruceConfig.priColor);

                String escStr = "[ESC] or [SEL] to Stop";
                tft.setCursor((tftWidth - tft.textWidth(escStr)) / 2, frameY + frameH * 0.78);
                tft.print(escStr);

                // ==================== THỰC THI JAMMING (ĐÃ SỬA LỖI) ====================
                // Xóa bộ đệm phím để tránh dừng ngay lập tức do sự kiện cũ
                while (check(SelPress) || check(EscPress)) { vTaskDelay(10 / portTICK_PERIOD_MS); }

                unsigned long startTime = millis();
                unsigned long durationMs = jamDuration * 1000;
                bool userStopped = false;

                // Vòng lặp chính của jammer
                while (!userStopped && (millis() - startTime < durationMs)) {
                    // Kiểm tra phím trước mỗi chu kỳ truyền
                    if (check(EscPress) || check(SelPress)) {
                        userStopped = true;
                        break;
                    }

                    // Xử lý theo chế độ
                    if (jamMode == 0) {
                        // SINGLE MODE
                        ELECHOUSE_cc1101.setMHZ(subghz_frequency_list[currentFreqIdx]);
                        ELECHOUSE_cc1101.SendData((uint8_t *)jamdata, payload);
                        vTaskDelay(1 / portTICK_PERIOD_MS); // Nhường CPU + cho phép kiểm tra phím
                    } else if (jamMode == 1) {
                        // RANGE MODE
                        float startF = subghz_frequency_list[currentFreqIdx];
                        float stopF = subghz_frequency_list[stopFreqIdx];
                        if (startF > stopF) {
                            float temp = startF;
                            startF = stopF;
                            stopF = temp;
                        }

                        for (float f = startF; f <= stopF; f += rangeStep) {
                            if (check(EscPress) || check(SelPress)) {
                                userStopped = true;
                                break;
                            }
                            ELECHOUSE_cc1101.setMHZ(f);
                            ELECHOUSE_cc1101.SendData((uint8_t *)jamdata, payload);
                            vTaskDelay(1 / portTICK_PERIOD_MS);
                        }
                    } else if (jamMode == 2) {
                        // HOPPER MODE
                        for (int i = 0; i < freqCount; i++) {
                            if (check(EscPress) || check(SelPress)) {
                                userStopped = true;
                                break;
                            }
                            ELECHOUSE_cc1101.setMHZ(subghz_frequency_list[i]);
                            ELECHOUSE_cc1101.SendData((uint8_t *)jamdata, payload);
                            vTaskDelay(1 / portTICK_PERIOD_MS);
                        }
                    }
                }

                // ==================== DỌN DẸP SAU KHI JAM ====================
                // Đưa CC1101 về trạng thái IDLE để tránh xung đột SPI với màn hình
                ELECHOUSE_cc1101.setSidle();
                // Đợi một chút cho module ổn định
                vTaskDelay(50 / portTICK_PERIOD_MS);

                // Yêu cầu vẽ lại menu chính
                redraw = true;
                // Không break ở đây, để vòng lặp GUI tiếp tục
            } else if (menuIndex == 5 && !editMode) { // EXIT
                // Thoát đúng cách: đặt cờ và break khỏi vòng lặp GUI
                exitGui = true;
                // Đảm bảo CC1101 dừng nếu đang chạy (dù lúc này không chạy)
                ELECHOUSE_cc1101.setSidle();
            } else {
                // Chỉ cho phép Edit đối với 4 mục đầu tiên
                if (menuIndex < 4) editMode = !editMode;
            }

            redraw = true;
            vTaskDelay(200 / portTICK_PERIOD_MS);
        }
    }

    // Dọn dẹp khi thoát hàm (nếu cần)
    // Có thể gọi thêm một lần nữa để chắc chắn
    ELECHOUSE_cc1101.setSidle();
}

// void rf_jammer() {
//     // 0. Khởi tạo module CC1101 bằng cấu hình bạn đã cung cấp
//     // (Lưu ý: Đảm bảo các biến sck, miso, mosi... đã được định nghĩa toàn cục)
//     cc1101initialize();

//     // 1. Dữ liệu đầu vào
//     const int freqCount = sizeof(subghz_frequency_list) / sizeof(subghz_frequency_list[0]);
//     static int currentFreqIdx = 36; // Dùng cho Single hoặc Start Range (VD: 433.920 MHz)
//     static int stopFreqIdx = 40;    // Dùng cho Stop Range (VD: 434.000 MHz)
//     static int jamMode = 0;         // 0: Single, 1: Range, 2: Hopper
//     uint32_t jamDuration = 30;      // Thời gian (giây)

//     // 2. Biến điều khiển Menu
//     int menuIndex = 0;
//     const int totalItems = 6; // Mode, Freq1, Freq2, Time, START, EXIT

//     // 3. Cấu hình giao diện cho màn hình
//     int topVisibleIndex = 0;
//     int lineHeight = 20;
//     int headerHeight = 63;
//     int maxVisibleItems = (tftHeight - headerHeight - 10) / lineHeight;

//     bool editMode = false;
//     bool redraw = true;
//     bool exitGui = false;

//     // Biến phụ trợ cho CC1101
//     byte payload = 64;      // Dữ liệu rác mặc định
//     float rangeStep = 0.05; // Bước nhảy cho Range Mode (50kHz)

//     while (!exitGui) {
//         // --- VẼ MENU ---
//         if (redraw) {
//             tft.fillScreen(bruceConfig.bgColor);
//             drawMainBorder();
//             drawMainBorderWithTitle("RF JAMMER PRO");
//             tft.setTextSize(FM);

//             if (menuIndex >= topVisibleIndex + maxVisibleItems) {
//                 topVisibleIndex = menuIndex - maxVisibleItems + 1;
//             }
//             if (menuIndex < topVisibleIndex) { topVisibleIndex = menuIndex; }

//             int visibleItemsCount = min(maxVisibleItems, totalItems - topVisibleIndex);
//             int outerFrameY = headerHeight - 6;
//             int outerFrameH = (visibleItemsCount * lineHeight) + 8;

//             tft.drawRoundRect(6, outerFrameY, tftWidth - 12, outerFrameH, 6, bruceConfig.priColor);

//             for (int i = 0; i < maxVisibleItems; i++) {
//                 int itemIndex = topVisibleIndex + i;
//                 if (itemIndex >= totalItems) break;

//                 int drawY = headerHeight + (i * lineHeight);

//                 if (itemIndex == menuIndex) {
//                     if (editMode) {
//                         tft.fillRoundRect(10, drawY - 2, tftWidth - 16, 18, 5, bruceConfig.priColor);
//                         tft.fillRoundRect(12, drawY - 1, tftWidth - 20, 16, 4, bruceConfig.bgColor);
//                         tft.setTextColor(bruceConfig.priColor);
//                     } else {
//                         tft.fillRoundRect(10, drawY - 2, tftWidth - 16, 18, 5, bruceConfig.priColor);
//                         tft.setTextColor(bruceConfig.bgColor);
//                     }
//                     tft.setCursor(14, drawY);
//                     tft.print("> ");
//                 } else {
//                     tft.setTextColor(bruceConfig.priColor);
//                     tft.setCursor(14, drawY);
//                     tft.print("  ");
//                 }

//                 // Hiển thị nội dung dựa trên Mode
//                 switch (itemIndex) {
//                     case 0:
//                         tft.print("Mode: ");
//                         if (jamMode == 0) tft.print("Single");
//                         else if (jamMode == 1) tft.print("Range");
//                         else tft.print("Hopper");
//                         break;
//                     case 1:
//                         if (jamMode == 1) tft.printf("Start: %.3f", subghz_frequency_list[currentFreqIdx]);
//                         else if (jamMode == 0)
//                             tft.printf("Freq: %.3f", subghz_frequency_list[currentFreqIdx]);
//                         else tft.print("Freq: Auto List"); // Hopper dùng toàn bộ list
//                         break;
//                     case 2:
//                         if (jamMode == 1) tft.printf("Stop : %.3f", subghz_frequency_list[stopFreqIdx]);
//                         else tft.print("Stop : N/A");
//                         break;
//                     case 3: tft.printf("Time: %d Sec", jamDuration); break;
//                     case 4: tft.print("START JAMMER"); break;
//                     case 5: tft.print("Exit"); break;
//                 }
//             }
//             redraw = false;
//         }

//         // --- XỬ LÝ NÚT BẤM ---
//         if (check(EscPress)) exitGui = true;

//         if (check(NextPress)) {
//             if (editMode) {
//                 if (menuIndex == 0) jamMode = (jamMode + 1) % 3;
//                 if (menuIndex == 1 && jamMode != 2) currentFreqIdx = (currentFreqIdx + 1) % freqCount;
//                 if (menuIndex == 2 && jamMode == 1) stopFreqIdx = (stopFreqIdx + 1) % freqCount;
//                 if (menuIndex == 3) jamDuration += 5;
//             } else {
//                 menuIndex = (menuIndex + 1) % totalItems;
//             }
//             redraw = true;
//             vTaskDelay(100 / portTICK_PERIOD_MS);
//         }

//         if (check(PrevPress)) {
//             if (editMode) {
//                 if (menuIndex == 0) jamMode = (jamMode - 1 + 3) % 3;
//                 if (menuIndex == 1 && jamMode != 2)
//                     currentFreqIdx = (currentFreqIdx - 1 + freqCount) % freqCount;
//                 if (menuIndex == 2 && jamMode == 1) stopFreqIdx = (stopFreqIdx - 1 + freqCount) %
//                 freqCount; if (menuIndex == 3 && jamDuration > 5) jamDuration -= 5;
//             } else {
//                 menuIndex = (menuIndex - 1 + totalItems) % totalItems;
//             }
//             redraw = true;
//             vTaskDelay(100 / portTICK_PERIOD_MS);
//         }

//         if (check(SelPress)) {
//             if (menuIndex == 4 && !editMode) { // START JAMMER
//                 // 1. VẼ POPUP ĐANG PHÁ SÓNG
//                 tft.fillScreen(bruceConfig.bgColor);
//                 auto autoFont = (tftHeight >= 240) ? FM : FP;
//                 tft.setTextSize(autoFont);
//                 tft.setTextColor(bruceConfig.priColor);

//                 int frameW = tftWidth - 20;
//                 int frameH = (tftHeight >= 240) ? 120 : 85;
//                 int frameX = 10;
//                 int frameY = (tftHeight - frameH) / 2;

//                 tft.drawRoundRect(frameX, frameY, frameW, frameH, 6, bruceConfig.priColor);

//                 String statusStr = "JAMMING...";
//                 tft.setCursor((tftWidth - tft.textWidth(statusStr)) / 2, frameY + frameH * 0.15);
//                 tft.print(statusStr);

//                 char modeStr[32];
//                 if (jamMode == 0) sprintf(modeStr, "%.3f MHz", subghz_frequency_list[currentFreqIdx]);
//                 else if (jamMode == 1)
//                     sprintf(
//                         modeStr,
//                         "%.3f - %.3f",
//                         subghz_frequency_list[currentFreqIdx],
//                         subghz_frequency_list[stopFreqIdx]
//                     );
//                 else sprintf(modeStr, "Hopper Mode");

//                 tft.setCursor((tftWidth - tft.textWidth(modeStr)) / 2, frameY + frameH * 0.42);
//                 tft.print(modeStr);

//                 int lineY = frameY + frameH * 0.68;
//                 tft.drawLine(frameX + 10, lineY, frameX + frameW - 10, lineY, bruceConfig.priColor);

//                 // CẬP NHẬT: Thay đổi text hiển thị trên màn hình
//                 String escStr = "[ESC] or [SEL] to Stop";
//                 tft.setCursor((tftWidth - tft.textWidth(escStr)) / 2, frameY + frameH * 0.78);
//                 tft.print(escStr);

//                 // 2. THỰC THI LOGIC JAMMING (Kết hợp mã CC1101)
//                 unsigned long startTime = millis();
//                 unsigned long durationMs = jamDuration * 1000;
//                 bool isJamming = true;

//                 while (isJamming && (millis() - startTime < durationMs)) {
//                     // CẬP NHẬT: Dừng ngay lập tức nếu bấm ESC HOẶC SEL
//                     if (check(EscPress) || check(SelPress)) {
//                         isJamming = false;
//                         break;
//                     }

//                     if (jamMode == 0) {
//                         // SINGLE MODE
//                         ELECHOUSE_cc1101.setMHZ(subghz_frequency_list[currentFreqIdx]);
//                         ELECHOUSE_cc1101.SendData((uint8_t *)jamdata, payload);
//                     } else if (jamMode == 1) {
//                         // RANGE MODE
//                         float startF = subghz_frequency_list[currentFreqIdx];
//                         float stopF = subghz_frequency_list[stopFreqIdx];
//                         // Đảm bảo Start < Stop để vòng lặp chạy đúng
//                         if (startF > stopF) {
//                             float temp = startF;
//                             startF = stopF;
//                             stopF = temp;
//                         }

//                         for (float f = startF; f <= stopF; f += rangeStep) {
//                             // CẬP NHẬT: Kiểm tra phím ESC hoặc SEL bên trong vòng lặp con
//                             if (check(EscPress) || check(SelPress)) {
//                                 isJamming = false;
//                                 break;
//                             }
//                             ELECHOUSE_cc1101.setMHZ(f);
//                             ELECHOUSE_cc1101.SendData((uint8_t *)jamdata, payload);
//                         }
//                     } else if (jamMode == 2) {
//                         // HOPPER MODE
//                         for (int i = 0; i < freqCount; i++) {
//                             // CẬP NHẬT: Kiểm tra phím ESC hoặc SEL bên trong vòng lặp con
//                             if (check(EscPress) || check(SelPress)) {
//                                 isJamming = false;
//                                 break;
//                             }
//                             ELECHOUSE_cc1101.setMHZ(subghz_frequency_list[i]);
//                             ELECHOUSE_cc1101.SendData((uint8_t *)jamdata, payload);
//                         }
//                     }
//                 }

//                 redraw = true;                        // Vẽ lại menu sau khi xong
//             } else if (menuIndex == 5 && !editMode) { // EXIT
//                 return; // CẬP NHẬT: Trả về và thoát hoàn toàn khỏi hàm rf_jammer() ngay lập tức
//             } else {
//                 // Chỉ cho phép Edit đối với 4 mục đầu tiên
//                 if (menuIndex < 4) editMode = !editMode;
//             }

//             redraw = true;
//             vTaskDelay(200 / portTICK_PERIOD_MS);
//         }
//     }
// }
