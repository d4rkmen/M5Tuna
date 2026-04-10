/**
 * @file hal_cardputer.cpp
 * @author Forairaaaaa
 * @brief
 * @version 0.1
 * @date 2023-09-22
 *
 * @copyright Copyright (c) 2023
 *
 */
#include "hal_cardputer.h"
#include "common_define.h"
#include "display/display.hpp"
#include "esp_log.h"

static const char* TAG = "HAL";

using namespace HAL;

void HalCardputer::_init_i2c()
{
    ESP_LOGI(TAG, "init i2c");
    _i2c = new I2CMaster();
}

void HalCardputer::_init_display()
{
    ESP_LOGI(TAG, "init display");

    // Display (custom LGFX class with ST7789V2 panel config)
    _display = new LGFX;

    // Canvas (full-screen sprite)
    _canvas = new LGFX_Sprite(_display);
    _canvas->createSprite(_display->width(), _display->height());

    _display->setBrightness(200);
}

void HalCardputer::_init_keyboard()
{
    ESP_LOGI(TAG, "init keyboard");
    _keyboard = new KEYBOARD::Keyboard(this);
    _board_type = _keyboard->boardType();
}

#ifdef HAVE_MIC
void HalCardputer::_init_mic()
{
    ESP_LOGI(TAG, "init mic");
    _mic = new Mic(this);

    if (_board_type == BoardType::CARDPUTER_ADV)
    {
        auto cfg = _mic->config();
        cfg.pin_bck = 41;
        cfg.pin_ws = 43;
        cfg.pin_data_in = 46;
        cfg.over_sampling = 1;   // do not change!
        cfg.magnification = 220; // do not change!
        _mic->config(cfg);
        ESP_LOGI(TAG, "CardPuter ADV: mic uses ES8311 I2S codec (bck=41, ws=43, din=46)");
    }
}
#endif

#ifdef HAVE_SPEAKER
void HalCardputer::_init_speaker()
{
    ESP_LOGI(TAG, "init speaker");
    _speaker = new Speaker(this);
}
#endif

void HalCardputer::_init_led()
{
    ESP_LOGI(TAG, "init led");
    _led = new LED(RGB_LED_GPIO);
}

#ifdef HAVE_BATTERY
void HalCardputer::_init_bat()
{
    ESP_LOGI(TAG, "init battery");
    _battery = new Battery();
}
#endif

#ifdef HAVE_WIFI
void HalCardputer::_init_wifi() { _wifi = new WiFi(_settings); }
#endif

void HalCardputer::init()
{
    ESP_LOGI(TAG, "HAL init");

    // Disable LoRa module NSS to prevent SPI bus conflicts
    gpio_set_direction((gpio_num_t)LORA_NSS_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level((gpio_num_t)LORA_NSS_PIN, 1);

    _init_i2c();
    _init_display();
    _init_keyboard();

    // ES8311 codec only on Cardputer ADV
    if (_board_type == BoardType::CARDPUTER_ADV)
    {
        _es8311 = new ES8311(this);
        _es8311->init();
    }

#ifdef HAVE_SPEAKER
    _init_speaker();
#endif
#ifdef HAVE_MIC
    _init_mic();
#endif
    _init_led();
#ifdef HAVE_BATTERY
    _init_bat();
#endif
#ifdef HAVE_WIFI
    _init_wifi();
#endif
}

#ifdef HAVE_BATTERY
uint8_t HalCardputer::getBatLevel()
{
    float voltage = getBatVoltage();
    uint8_t result = 0;
    if (voltage >= 4.12f)
        result = 100;
    else if (voltage >= 3.88f)
        result = 75;
    else if (voltage >= 3.61f)
        result = 50;
    else if (voltage >= 3.40f)
        result = 25;
    else
        result = 0;
    return result;
}

float HalCardputer::getBatVoltage() { return _battery->get_voltage(); }
#endif
