#include "EthernetMenu.h"
#if !defined(LITE_VERSION)
#include "core/display.h"
#include "core/settings.h"
#include "core/utils.h"
#include "modules/ethernet/ARPScanner.h"
#include "modules/ethernet/DHCPStarvation.h"
#include "modules/ethernet/EthernetHelper.h"
#include "modules/ethernet/MACFlooding.h"

void EthernetMenu::start_ethernet() {
    eth = new EthernetHelper();
    if (!eth->setup()) {
        displayError("W5500 not found");
        delete eth;
        eth = nullptr;
        return;
    }
    while (!eth->is_connected()) { delay(100); }
}

void EthernetMenu::optionsMenu() {
    options = {
        {"Scan Hosts",
         [this]() {
             start_ethernet();
             if (eth != nullptr) {
                 run_arp_scanner();
                 eth->stop();
             } else {
                 displayError("W5500 not found");
             }
         }                        },
        {"DHCP Starvation",
         [this]() {
             start_ethernet();
             if (eth != nullptr) {
                 DHCPStarvation();
                 eth->stop();
             } else {
                 displayError("W5500 not found");
             }
         }                        },
        {"MAC Flooding",    [this]() {
             start_ethernet();
             if (eth != nullptr) {
                 MACFlooding();
                 eth->stop();
             } else {
                 displayError("W5500 not found");
             }
         }}
    };
    addOptionToMainMenu();

    delay(200);

    loopOptions(options, MENU_TYPE_SUBMENU, "Ethernet");
}

void EthernetMenu::drawIcon(float scale) {
    clearIconArea();

    int iconW = scale * 30;     // Nửa chiều rộng
    int iconH = scale * 40;     // Chiều cao thân trên
    int smallerH = scale * 16;  // Chiều cao/rộng phần thụt vào (vai)
    int pinHeight = scale * 16; // Độ dài 4 chân cáp

    int lineWidth = 2;

    // Tự động căn giữa tâm hoàn hảo (không dùng số fix cứng)
    int totalHeight = iconH + smallerH;
    int Y = iconCenterY - (totalHeight / 2);

    int starterX = iconCenterX - iconW;
    int finalX = iconCenterX + iconW;

    // Dùng fillRect thay cho drawRect để vẽ các nét đặc mượt mà hơn

    // 1. Vẽ khung chính (Nắp trên, Cạnh trái, Cạnh phải)
    tft.fillRect(starterX, Y, iconW * 2, lineWidth, bruceConfig.priColor);       // Top
    tft.fillRect(starterX, Y, lineWidth, iconH, bruceConfig.priColor);           // Left
    tft.fillRect(finalX - lineWidth, Y, lineWidth, iconH, bruceConfig.priColor); // Right

    // 2. Vẽ vai thụt vào ôm cáp (Vai trái, Vai phải)
    tft.fillRect(starterX, Y + iconH, smallerH, lineWidth, bruceConfig.priColor);
    tft.fillRect(finalX - smallerH, Y + iconH, smallerH, lineWidth, bruceConfig.priColor);

    // 3. Vẽ 2 thành đứng phía dưới
    tft.fillRect(starterX + smallerH - lineWidth, Y + iconH, lineWidth, smallerH, bruceConfig.priColor);
    tft.fillRect(finalX - smallerH, Y + iconH, lineWidth, smallerH, bruceConfig.priColor);

    // 4. Vẽ đáy đóng lại
    int bottomW = (finalX - smallerH) - (starterX + smallerH - lineWidth) + lineWidth;
    tft.fillRect(
        starterX + smallerH - lineWidth, Y + iconH + smallerH, bottomW, lineWidth, bruceConfig.priColor
    );

    // 5. Vẽ 4 chân cáp (Tính toán chia đều không gian tự động)
    float spacing = (float)(iconW * 2) / 5.0; // Chia khoảng cách ra làm 5 phần cho 4 chân
    for (size_t i = 1; i <= 4; i++) {
        int pinX = starterX + (int)(i * spacing) - (lineWidth / 2);
        tft.fillRect(pinX, Y, lineWidth, pinHeight, bruceConfig.priColor);
    }
}
#endif
