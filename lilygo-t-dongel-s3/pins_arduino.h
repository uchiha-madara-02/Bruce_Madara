#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include "soc/soc_caps.h"
#include <stdint.h>

static const uint8_t TX = 44;
static const uint8_t RX = 43;

static const uint8_t SDA = 47;
static const uint8_t SCL = 48;

// Modified elsewhere
static const uint8_t SS = 3;
static const uint8_t MOSI = 17;
static const uint8_t MISO = 8;
static const uint8_t SCK = 18;

#define SERIAL_RX 43
#define SERIAL_TX 44
#define BAD_RX SERIAL_RX
#define BAD_TX SERIAL_TX
#define GPS_SERIAL_TX SERIAL_TX
#define GPS_SERIAL_RX SERIAL_RX
#define USB_as_HID 1

#define BTN_ALIAS "\"OK\""
#define HAS_3_BUTTONS
#define SEL_BTN 0
#define UP_BTN 41
#define DW_BTN 40
#define BTN_ACT LOW
#define DEEPSLEEP_WAKEUP_PIN SEL_BTN

#define RXLED 44
#define TXLED 43
#define LED_ON HIGH
#define LED_OFF LOW

#define USE_CC1101_VIA_SPI
#define CC1101_GDO0_PIN -1
#define CC1101_GDO2_PIN -1
#define CC1101_SS_PIN -1
#define CC1101_MOSI_PIN SPI_MOSI_PIN
#define CC1101_SCK_PIN SPI_SCK_PIN
#define CC1101_MISO_PIN SPI_MISO_PIN

#define USE_NRF24_VIA_SPI
#define NRF24_CE_PIN -1
#define NRF24_SS_PIN -1
#define NRF24_MOSI_PIN SPI_MOSI_PIN
#define NRF24_SCK_PIN SPI_SCK_PIN
#define NRF24_MISO_PIN SPI_MISO_PIN

#define FP 1
#define FM 2
#define FG 3

#define HAS_SCREEN 1
#define ROTATION 1
#define MINBRIGHT (uint8_t)1

#define USER_SETUP_LOADED 1
#define ST7735_DRIVER 1
#define TFT_RGB_ORDER TFT_BGR
#define CGRAM_OFFSET
#define TFT_WIDTH 135
#define TFT_HEIGHT 240
// #define ST7735_REDTAB160x80
#define TFT_BACKLIGHT_ON 1
#define TFT_BL 38
#define TFT_RST 1
#define TFT_DC 2
#define TFT_MISO 8
#define TFT_MOSI 3
#define TFT_SCLK 5
#define TFT_CS 4
#define TOUCH_CS -1 // SDCARD_CS to make sure SDCard works
#define SMOOTH_FONT 1
#define SPI_FREQUENCY 20000000
#define SPI_READ_FREQUENCY 20000000
#define SPI_TOUCH_FREQUENCY 2500000

#define SDCARD_CS -1
#define SDCARD_SCK -1
#define SDCARD_MISO -1
#define SDCARD_MOSI -1

#define GROVE_SDA 47
#define GROVE_SCL 48

#define SPI_SCK_PIN 18
#define SPI_MOSI_PIN 17
#define SPI_MISO_PIN 8
#define SPI_SS_PIN 3

#endif /* Pins_Arduino_h */
