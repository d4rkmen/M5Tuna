/**
 * @file hal_cardputer.h
 * @author Forairaaaaa
 * @brief
 * @version 0.1
 * @date 2023-09-22
 *
 * @copyright Copyright (c) 2023
 *
 */
#include "hal.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define RGB_LED_GPIO 21
#define LORA_NSS_PIN 5

namespace HAL
{
    class HalCardputer : public Hal
    {
    private:
        void _init_i2c();
        void _init_display();
        void _init_keyboard();
#ifdef HAVE_MIC
        void _init_mic();
#endif
#ifdef HAVE_SPEAKER
        void _init_speaker();
#endif
        void _init_led();
#ifdef HAVE_BATTERY
        void _init_bat();
#endif

    public:
        HalCardputer() : Hal() {}
        std::string type() override
        {
            switch (_board_type)
            {
            case HAL::BoardType::CARDPUTER:
                return "v1.x";
            case HAL::BoardType::CARDPUTER_ADV:
                return "ADV";
            default:
                return "unknown";
            }
        }
        void init() override;
#ifdef HAVE_SPEAKER
        void playErrorSound() override
        {
            _speaker->tone(1000, 100);
            _speaker->tone(800, 100);
            _speaker->tone(700, 20);
        }
        void playKeyboardSound() override { _speaker->tone(5000, 20); }
        void playLastSound() override { _speaker->tone(6000, 20); }
        void playNextSound() override { _speaker->tone(7000, 20); }
        void playDeviceConnectedSound() override
        {
            _speaker->tone(1000, 100);
            vTaskDelay(pdMS_TO_TICKS(50));
            _speaker->tone(1500, 200);
        }
        void playDeviceDisconnectedSound() override
        {
            _speaker->tone(1500, 100);
            vTaskDelay(pdMS_TO_TICKS(50));
            _speaker->tone(1000, 200);
        }
#endif
#ifdef HAVE_BATTERY
        uint8_t getBatLevel() override;
        float getBatVoltage() override;
#endif
    };
} // namespace HAL
