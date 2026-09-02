#include "core/main_menu.h"
#include "core/display.h"
#include "core/utils.h"
#include <algorithm> // Thư viện cần thiết cho std::find
#include <globals.h>

MainMenu::MainMenu() {
    _menuItems.push_back(&wifiMenu);
    _menuItems.push_back(&bw16Menu);
    _menuItems.push_back(&bleMenu);

#if !defined(LITE_VERSION)
    _menuItems.push_back(&ethernetMenu);
#endif

    _menuItems.push_back(&rfMenu);
    _menuItems.push_back(&rfidMenu);
    _menuItems.push_back(&irMenu);

#if defined(FM_SI4713) && !defined(LITE_VERSION)
    _menuItems.push_back(&fmMenu);
#endif

    _menuItems.push_back(&gpsMenu);
    _menuItems.push_back(&nrf24Menu);

#if !defined(LITE_VERSION)
#if !defined(DISABLE_INTERPRETER)
    _menuItems.push_back(&scriptsMenu);
#endif
    _menuItems.push_back(&loraMenu);
#endif

    _menuItems.push_back(&fileMenu);
    _menuItems.push_back(&othersMenu);
    _menuItems.push_back(&clockMenu);

#if !defined(LITE_VERSION)
    _menuItems.push_back(&connectMenu);
#endif
    _menuItems.push_back(&configMenu);

    _totalItems = _menuItems.size();
}

MainMenu::~MainMenu() {}

void MainMenu::hideAppsMenu() {
    bool useTheme = false;
    if (_totalItems > 0 && _menuItems[0]->checkTheme()) { useTheme = true; }

    auto items = this->getItems();
RESTART:
    options.clear();
    for (auto item : items) {
        String label = item->getName();
        std::vector<String> l = bruceConfig.disabledMenus;
        bool enabled = std::find(l.begin(), l.end(), label) == l.end();
        options.push_back({label, [this, label]() { bruceConfig.addDisabledMenu(label); }, enabled});
    }
    options.push_back({"Show All", [=]() { bruceConfig.disabledMenus.clear(); }, true});
    addOptionToMainMenu();
    loopOptions(options);
    bruceConfig.saveFile();
    if (!returnToMenu) goto RESTART;
}

void MainMenu::begin(void) {
    _totalItems = _menuItems.size();
    if (_totalItems == 0) return;

    bool useTheme = false;
    if (_menuItems[0]->checkTheme()) { useTheme = true; }

    // =========================================================
    // LỌC DANH SÁCH APP TỪ FILE JSON CHO CẢ 2 TRƯỜNG HỢP
    // =========================================================
    std::vector<MenuItemInterface *> activeItems;
    std::vector<String> l = bruceConfig.disabledMenus;

    for (int i = 0; i < _totalItems; i++) {
        if (std::find(l.begin(), l.end(), _menuItems[i]->getName()) == l.end()) {
            activeItems.push_back(_menuItems[i]);
        }
    }

    int activeCount = activeItems.size();
    if (activeCount == 0) {
        activeItems = _menuItems;
        activeCount = _totalItems;
    }

    if (useTheme) {
        // =========================================================
        // THUẬT TOÁN ĐOẠN 3: GIAO DIỆN KHI CÓ THEME
        // =========================================================
        returnToMenu = false;
        options = {};

        for (int i = 0; i < activeCount; i++) {
            MenuItemInterface *itemPtr = activeItems[i];
            options.push_back(
                {itemPtr->getName(),
                 [itemPtr]() { itemPtr->optionsMenu(); },
                 false,
                 [](void *menuItem, bool shouldRender) {
                     if (!shouldRender) return false;
                     drawMainBorder(false);

                     MenuItemInterface *obj = static_cast<MenuItemInterface *>(menuItem);
                     float scale = float((float)tftWidth / (float)240);
                     if (bruceConfigPins.rotation & 0b01) scale = float((float)tftHeight / (float)135);
                     obj->draw(scale);
#if defined(HAS_TOUCH)
                     TouchFooter();
#endif
                     return true;
                 },
                 itemPtr}
            );
        }

        if (_currentIndex >= activeCount) _currentIndex = 0;
        _currentIndex = loopOptions(options, MENU_TYPE_MAIN, "Main Menu", _currentIndex);

    } else {
        // =========================================================
        // THUẬT TOÁN ĐOẠN 4: GIAO DIỆN KHI KHÔNG CÓ THEME
        // =========================================================
        bool redraw = true;
        if (_currentIndex >= activeCount) _currentIndex = 0;

        auto updateNeighbors = [&](int idx) {
            int prevIdx = (idx - 1 + activeCount) % activeCount;
            int nextIdx = (idx + 1) % activeCount;
            activeItems[idx]->setNeighbors(activeItems[prevIdx], activeItems[nextIdx]);
        };

        auto drawProgressBar = [&]() {
            int topSpace = tftHeight * 0.12;
            if (topSpace < 22) topSpace = 22;
            int barY = topSpace + 4;
            int barH = tftHeight * 0.04;
            if (barH < 5) barH = 5;
            if (barH > 10) barH = 10;
            int barX = tftWidth * 0.05;
            int barW = tftWidth - (barX * 2);

            tft.fillRect(0, topSpace, tftWidth, barH + 8, bruceConfig.bgColor);
            tft.drawFastVLine(barX, barY, barH, bruceConfig.priColor);
            tft.drawFastHLine(barX, barY, 4, bruceConfig.priColor);
            tft.drawFastHLine(barX, barY + barH - 1, 4, bruceConfig.priColor);
            tft.drawFastVLine(barX + barW - 1, barY, barH, bruceConfig.priColor);
            tft.drawFastHLine(barX + barW - 4, barY, 4, bruceConfig.priColor);
            tft.drawFastHLine(barX + barW - 4, barY + barH - 1, 4, bruceConfig.priColor);

            if (activeCount > 0) {
                int maxSegW = barW - 6;
                int segW = maxSegW / activeCount;
                int space = (segW > 2) ? 1 : 0;
                for (int i = 0; i <= _currentIndex; i++) {
                    tft.fillRect(barX + 3 + i * segW, barY + 1, segW - space, barH - 2, bruceConfig.priColor);
                }
            }
        };

        while (true) {
            if (redraw) {
                updateNeighbors(_currentIndex);
                drawProgressBar();
                activeItems[_currentIndex]->draw(1.0, 0, false);
                redraw = false;
            }

            int topSpace = tftHeight * 0.12;
            if (topSpace < 22) topSpace = 22;
            int barH = tftHeight * 0.04;
            if (barH < 5) barH = 5;
            if (barH > 10) barH = 10;
            int progressBarBottom = topSpace + barH + 8;

            int marginX = tftWidth * 0.05;
            int usableWidth = tftWidth - (marginX * 2);
            int availableH = tftHeight - progressBarBottom;

            float screenRatio = (float)tftWidth / tftHeight;
            float widthFactor = (screenRatio < 1.1) ? 0.38 : 0.45;

            int textH = 26 + (tftHeight * 0.02);
            int gapY = availableH * 0.04;

            int maxBoxH = availableH - textH - gapY - 15;
            int maxBoxW = usableWidth * widthFactor;
            int cw = (maxBoxW < maxBoxH) ? maxBoxW : maxBoxH;
            int sw = cw * 0.60;
            int boxGap = tftWidth * 0.05;

            int slideDist = (cw / 2) + (sw / 2) + boxGap;
            int maxAllowedSpacing = (tftWidth / 2) - marginX - (sw / 2) - 4;
            if (slideDist > maxAllowedSpacing) { slideDist = maxAllowedSpacing; }

            if (check(NextPress) || check(DownPress)) {
                float curve[] = {0.35, 0.75};
                for (int i = 0; i < 2; i++) {
                    activeItems[_currentIndex]->draw(1.0, -(int)(slideDist * curve[i]), true);
                }
                _currentIndex = (_currentIndex + 1) % activeCount;
                redraw = true;

            } else if (check(PrevPress) || check(UpPress)) {
                float curve[] = {0.35, 0.75};
                for (int i = 0; i < 2; i++) {
                    activeItems[_currentIndex]->draw(1.0, (int)(slideDist * curve[i]), true);
                }
                _currentIndex = (_currentIndex - 1 + activeCount) % activeCount;
                redraw = true;
            }

            if (check(SelPress)) {
                tft.fillScreen(bruceConfig.bgColor);
                activeItems[_currentIndex]->optionsMenu();
                tft.fillScreen(bruceConfig.bgColor);
                redraw = true;
                returnToMenu = false;
            }

            // [FIX LỖI CHÍNH Ở ĐÂY]: Giải phóng Menu để hệ thống chạy file JS
            if (interpreter_state > 0) { break; }

            vTaskDelay(50 / portTICK_PERIOD_MS);
        }
    }
}
