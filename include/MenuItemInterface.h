#ifndef __MENU_ITEM_INTERFACE_H__
#define __MENU_ITEM_INTERFACE_H__

#if !defined(USE_M5GFX)
#include <TFT_eSPI.h>
#endif

#include "core/display.h"
#include <globals.h>

class MenuItemInterface {
public:
    virtual ~MenuItemInterface() = default;
    virtual void optionsMenu(void) = 0;
    virtual void drawIcon(float scale = 1) = 0;

    virtual void drawIconImg() {
        drawImg(
            *bruceConfig.themeFS(),
            bruceConfig.getThemeItemImg(themePath()),
            0,
            imgCenterY,
            true,
            bruceConfig.theme.gifDuration,
            false
        );
    }
    virtual bool hasTheme() = 0;
    virtual String themePath() = 0;

    bool checkTheme() { return hasTheme() && themePath() != ""; }
    String getName() const { return _name; }

    MenuItemInterface *_prevItem = nullptr;
    MenuItemInterface *_nextItem = nullptr;

    void setNeighbors(MenuItemInterface *prev, MenuItemInterface *next) {
        _prevItem = prev;
        _nextItem = next;
    }

    void draw(float scale = 1, int offsetX = 0, bool isAnimating = false) {
        if (rotation != bruceConfigPins.rotation) resetCoordinates();

        if (checkTheme()) {
            // =========================================================
            // ĐOẠN 1: GIAO DIỆN THEME (CHỐNG NHÁY CHỮ)
            // =========================================================

#if defined(USE_M5GFX)
            M5.Display.clearClipRect();
#else
            ((TFT_eSPI *)&tft)->resetViewport();
#endif

            if (isAnimating) return; // Đang trượt thì không vẽ lại Theme để tránh lag

            drawIconImg();
            if (bruceConfig.theme.label) drawTitle(scale);
            drawStatusBar();

        } else {
            // =========================================================
            // ĐOẠN 2: GIAO DIỆN 3 ICON (COVER FLOW) + SCALE CHUẨN
            // =========================================================

            int topSpace = tftHeight * 0.12;
            if (topSpace < 22) topSpace = 22;
            int barH = tftHeight * 0.04;
            if (barH < 5) barH = 5;
            if (barH > 10) barH = 10;
            int progressBarBottom = topSpace + barH + 8;

            int marginX = tftWidth * 0.05;
            int usableWidth = tftWidth - (marginX * 2);
            int availableH = tftHeight - progressBarBottom;

            int textH = 26 + (tftHeight * 0.02);
            int gapY = availableH * 0.04;

            float screenRatio = (float)tftWidth / tftHeight;
            float widthFactor;
            float sideRatio;
            float iconSizeTweak;
            int iconOffsetY = 0;

            if (tftWidth <= 135 || tftHeight <= 135) {
                widthFactor = 0.48;
                sideRatio = 0.65;
                iconSizeTweak = 0.85;
                iconOffsetY = 0;
            } else if ((tftWidth > 135 && tftWidth <= 180) || (tftHeight > 135 && tftHeight <= 180)) {
                widthFactor = 0.42;
                sideRatio = 0.65;
                iconSizeTweak = 1.25;
                iconOffsetY = 0;
            } else if (screenRatio > 0.8 && screenRatio < 1.2) {
                widthFactor = 0.34;
                sideRatio = 0.60;
                iconSizeTweak = 1.8;
                iconOffsetY = 0;
            } else if (tftWidth <= 135 && tftHeight <= 135) {
                widthFactor = 0.42;
                sideRatio = 0.60;
                iconSizeTweak = 0.55;
                iconOffsetY = 0;
            } else {
                widthFactor = (screenRatio < 1.0) ? 0.35 : 0.39;
                sideRatio = 0.62;
                iconSizeTweak = 2.1;
                iconOffsetY = 0;
            }

            int maxBoxH = availableH - textH - gapY - 15;
            int maxBoxW = usableWidth * widthFactor;
            int cw = (maxBoxW < maxBoxH) ? maxBoxW : maxBoxH;
            int ch = cw;
            int sw = cw * sideRatio;
            int sh = ch * sideRatio;

            int boxGap = tftWidth * 0.05;
            int spacing = (cw / 2) + (sw / 2) + boxGap;

            int maxAllowedSpacing = (tftWidth / 2) - marginX - (sw / 2) - 4;
            if (spacing > maxAllowedSpacing) { spacing = maxAllowedSpacing; }

            int textW = cw + (usableWidth * 0.20);
            if (textW > usableWidth) textW = usableWidth;

            int totalContentH = ch + gapY + textH;
            int cy = progressBarBottom + (availableH - totalContentH) / 2 + (ch / 2);

            int cxCenter = tftWidth / 2;
            int cxLeft = cxCenter - spacing;
            int cxRight = cxCenter + spacing;

            int paddingY = 2;
            int vpY = (cy - ch / 2) - paddingY;
            int vpH = ch + (paddingY * 2);

            int textY = cy + ch / 2 + gapY;

            if (isAnimating) {
                tft.fillRect(marginX, vpY, usableWidth, vpH, bruceConfig.bgColor);
                tft.fillRect(0, textY, tftWidth, textH + 5, bruceConfig.bgColor);
            } else {
                tft.fillRect(
                    0, progressBarBottom, tftWidth, tftHeight - progressBarBottom, bruceConfig.bgColor
                );
            }

#if defined(USE_M5GFX)
            M5.Display.setClipRect(marginX, vpY, usableWidth, vpH);
#else
            ((TFT_eSPI *)&tft)->setViewport(marginX, vpY, usableWidth, vpH, false);
#endif

            // [TÍNH TỶ LỆ KÉP]: Kết hợp Scale Gốc và Thu phóng 3D
            float baseScale = ((float)cw / iconAreaW) * iconSizeTweak;
            if (baseScale <= 0) baseScale = 1.0;
            float finalScale = scale * baseScale;

            auto getDynamicScale = [&](int currentX) -> float {
                int dist = abs(currentX - cxCenter);
                int maxDist = spacing;
                if (dist > maxDist) dist = maxDist;
                float ratio = (float)dist / maxDist;
                return 1.0 - (ratio * (1.0 - sideRatio));
            };

            // [HÀM BẢO VỆ]: Trị dứt điểm lỗi mất Icon trên TFT 1.54 inch
            auto drawSpoofedIcon = [&](MenuItemInterface *item, int x, int y, float dynScale, float s) {
                if (!item) return;
                // Nhớ các biến gốc
                int oldCX = item->iconCenterX;
                int oldCY = item->iconCenterY;
                int oldAX = item->iconAreaX;
                int oldAY = item->iconAreaY;
                int oldAW = item->iconAreaW;
                int oldAH = item->iconAreaH;

                // Cấp lại vùng an toàn (Bounding Box) nhỏ gọn cho từng Icon
                int currentW = cw * dynScale;
                int currentH = ch * dynScale;

                item->iconCenterX = x;
                item->iconCenterY = y;
                item->iconAreaW = currentW;
                item->iconAreaH = currentH;
                item->iconAreaX = x - currentW / 2;
                item->iconAreaY = y - currentH / 2;

                // Vẽ Icon (Lúc này lệnh clearIconArea sẽ không bị chém lấn sang nhà hàng xóm)
                item->drawIcon(s);

                // Trả về nguyên trạng
                item->iconCenterX = oldCX;
                item->iconCenterY = oldCY;
                item->iconAreaX = oldAX;
                item->iconAreaY = oldAY;
                item->iconAreaW = oldAW;
                item->iconAreaH = oldAH;
            };

            // 1. Vẽ Icon Trái
            float dynPrev = getDynamicScale(cxLeft + offsetX);
            drawSpoofedIcon(_prevItem, cxLeft + offsetX, cy + iconOffsetY, dynPrev, finalScale * dynPrev);

            // 2. Vẽ Icon Phải
            float dynNext = getDynamicScale(cxRight + offsetX);
            drawSpoofedIcon(_nextItem, cxRight + offsetX, cy + iconOffsetY, dynNext, finalScale * dynNext);

            // 3. Vẽ Icon Trung Tâm (Vẽ cuối cùng để nổi lên trên cùng)
            float dynMain = getDynamicScale(cxCenter + offsetX);
            drawSpoofedIcon(this, cxCenter + offsetX, cy + iconOffsetY, dynMain, finalScale * dynMain);

#if defined(USE_M5GFX)
            M5.Display.clearClipRect();
#else
            ((TFT_eSPI *)&tft)->resetViewport();
#endif

            // Vẽ các khung cố định bên ngoài (Như những ô cửa sổ)
            if (!isAnimating) {
                if (_prevItem)
                    tft.drawRoundRect(cxLeft - sw / 2, cy - sh / 2, sw, sh, 4, bruceConfig.priColor);
                if (_nextItem)
                    tft.drawRoundRect(cxRight - sw / 2, cy - sh / 2, sw, sh, 4, bruceConfig.priColor);
                tft.drawRoundRect(cxCenter - cw / 2, cy - ch / 2, cw, ch, 6, bruceConfig.priColor);

                int cornerLen = cw / 4;
                int t = (tftWidth > 200) ? 4 : 3;
                tft.fillRect(cxCenter - cw / 2, cy - ch / 2, cornerLen, t, bruceConfig.priColor);
                tft.fillRect(cxCenter - cw / 2, cy - ch / 2, t, cornerLen, bruceConfig.priColor);
                tft.fillRect(
                    cxCenter + cw / 2 - cornerLen + 1, cy - ch / 2, cornerLen, t, bruceConfig.priColor
                );
                tft.fillRect(cxCenter + cw / 2 - t + 1, cy - ch / 2, t, cornerLen, bruceConfig.priColor);
                tft.fillRect(cxCenter - cw / 2, cy + ch / 2 - t + 1, cornerLen, t, bruceConfig.priColor);
                tft.fillRect(
                    cxCenter - cw / 2, cy + ch / 2 - cornerLen + 1, t, cornerLen, bruceConfig.priColor
                );
                tft.fillRect(
                    cxCenter + cw / 2 - cornerLen + 1, cy + ch / 2 - t + 1, cornerLen, t, bruceConfig.priColor
                );
                tft.fillRect(
                    cxCenter + cw / 2 - t + 1, cy + ch / 2 - cornerLen + 1, t, cornerLen, bruceConfig.priColor
                );

                tft.fillRoundRect(cxCenter - textW / 2, textY, textW, textH, 4, bruceConfig.priColor);
                tft.drawRoundRect(
                    cxCenter - textW / 2 + 2, textY + 2, textW - 4, textH - 4, 2, bruceConfig.bgColor
                );

                tft.setTextSize(FM);
                tft.setTextColor(bruceConfig.bgColor, bruceConfig.priColor);
                int nchars = (textW - 4) / (LW * FM);
                int textOffsetY = textY + (textH - (8 * FM)) / 2 + 1;
                tft.drawCentreString(getName().substring(0, nchars), cxCenter, textOffsetY, 1);

                drawStatusBar();
            } else {
                // Khi trượt, các ô cửa sổ vẫn giữ im vị trí để Icon trôi qua
                if (_prevItem)
                    tft.drawRoundRect(cxLeft - sw / 2, cy - sh / 2, sw, sh, 4, bruceConfig.priColor);
                if (_nextItem)
                    tft.drawRoundRect(cxRight - sw / 2, cy - sh / 2, sw, sh, 4, bruceConfig.priColor);
                tft.drawRoundRect(cxCenter - cw / 2, cy - ch / 2, cw, ch, 6, bruceConfig.priColor);
            }
        }
    }

    void drawArrows(float scale = 1) {
        tft.fillRect(arrowAreaX, iconAreaY, arrowAreaW, iconAreaH, bruceConfig.bgColor);
        tft.fillRect(
            tftWidth - arrowAreaX - arrowAreaW, iconAreaY, arrowAreaW, iconAreaH, bruceConfig.bgColor
        );
        int arrowSize = scale * 10;
        int lineWidth = scale * 3;
        int arrowX = BORDER_PAD_X + 1.5 * arrowSize;
        int arrowY = iconCenterY + 1.5 * arrowSize;
        tft.drawWideLine(
            arrowX,
            arrowY,
            arrowX + arrowSize,
            arrowY + arrowSize,
            lineWidth,
            bruceConfig.priColor,
            bruceConfig.bgColor
        );
        tft.drawWideLine(
            arrowX,
            arrowY,
            arrowX + arrowSize,
            arrowY - arrowSize,
            lineWidth,
            bruceConfig.priColor,
            bruceConfig.bgColor
        );
        tft.drawWideLine(
            tftWidth - arrowX,
            arrowY,
            tftWidth - arrowX - arrowSize,
            arrowY + arrowSize,
            lineWidth,
            bruceConfig.priColor,
            bruceConfig.bgColor
        );
        tft.drawWideLine(
            tftWidth - arrowX,
            arrowY,
            tftWidth - arrowX - arrowSize,
            arrowY - arrowSize,
            lineWidth,
            bruceConfig.priColor,
            bruceConfig.bgColor
        );
    }

    void drawTitle(float scale = 1) {
        int titleY = iconCenterY + iconAreaH / 2 + FG;
        tft.setTextSize(FM);
        tft.drawPixel(0, 0, 0);
        tft.fillRect(arrowAreaX, titleY, tftWidth - 2 * arrowAreaX, LH * FM, bruceConfig.bgColor);
        int nchars = (tftWidth - 16) / (LW * FM);
        tft.drawCentreString(getName().substring(0, nchars), iconCenterX, titleY, 1);
    }

protected:
    String _name = "";
    uint8_t rotation = ROTATION;
    int iconAreaH =
        ((tftHeight - 2 * BORDER_PAD_Y) % 2 == 0 ? tftHeight - 2 * BORDER_PAD_Y
                                                 : tftHeight - 2 * BORDER_PAD_Y + 1);
    int iconAreaW = iconAreaH;
    int iconCenterX = tftWidth / 2;
    int iconCenterY = tftHeight / 2;
    int imgCenterY = 13;
    int iconAreaX = iconCenterX - iconAreaW / 2;
    int iconAreaY = iconCenterY - iconAreaH / 2;
    int arrowAreaX = BORDER_PAD_X;
    int arrowAreaW = iconAreaX - arrowAreaX;

    MenuItemInterface(const String &name) : _name(name) {}

    void clearIconArea(void) {
        tft.fillRect(iconAreaX, iconAreaY, iconAreaW, iconAreaH, bruceConfig.bgColor);
    }
    void clearImgArea(void) { tft.fillRect(7, 27, tftWidth - 14, tftHeight - 34, bruceConfig.bgColor); }

    void resetCoordinates(void) {
        if (tftWidth > tftHeight) {
            iconAreaH =
                ((tftHeight - 2 * BORDER_PAD_Y) % 2 == 0 ? tftHeight - 2 * BORDER_PAD_Y
                                                         : tftHeight - 2 * BORDER_PAD_Y + 1);
        } else {
            iconAreaH =
                ((tftWidth - 2 * BORDER_PAD_Y) % 2 == 0 ? tftWidth - 2 * BORDER_PAD_Y
                                                        : tftWidth - 2 * BORDER_PAD_Y + 1);
        }
        iconAreaW = iconAreaH;
        iconCenterX = tftWidth / 2;
        iconCenterY = tftHeight / 2;
        iconAreaX = iconCenterX - iconAreaW / 2;
        iconAreaY = iconCenterY - iconAreaH / 2;
        arrowAreaX = BORDER_PAD_X;
        arrowAreaW = iconAreaX - arrowAreaX;
        rotation = bruceConfigPins.rotation;
    }
};

#endif // __MENU_ITEM_INTERFACE_H__
