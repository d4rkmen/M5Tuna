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
#ifdef HAVE_SETTINGS
#include "settings/settings.h"
#endif
namespace HAL
{
    class HalCardputer : public Hal
    {
    private:
        void _init_display();
        void _init_keyboard();
#ifdef HAVE_MIC
        void _init_mic();
#endif
#ifdef HAVE_SPEAKER
        void _init_speaker();
#endif
        void _init_button();
#ifdef HAVE_BATTERY
        void _init_bat();
#endif
#ifdef HAVE_SDCARD
        void _init_sdcard();
#endif
#ifdef HAVE_USB
        void _init_usb();
#endif
#ifdef HAVE_WIFI
        void _init_wifi();
#endif

    public:
        HalCardputer(
#ifdef HAVE_SETTINGS
            SETTINGS::Settings* settings
#endif
            )
            : Hal(
#ifdef HAVE_SETTINGS
                  settings
#endif
              )
        {
        }
        std::string type() override { return "cardputer"; }
        void init() override;
#ifdef HAVE_SPEAKER
        void playErrorSound() override
        {
            // _speaker->setVolume(64);
            _speaker->tone(1000, 100);
            _speaker->tone(800, 100);
            _speaker->tone(700, 20);
        }
        void playKeyboardSound() override
        {
            // _speaker->setVolume(72);
            _speaker->tone(5000, 20);
        }
        void playLastSound() override
        {
            // _speaker->setVolume(32);
            _speaker->tone(6000, 20);
        }
        void playNextSound() override
        {
            // _speaker->setVolume(64);
            _speaker->tone(7000, 20);
        }
        void playDeviceConnectedSound()
        {
            // _speaker->setVolume(64);
            _speaker->tone(1000, 100);
            vTaskDelay(50);
            _speaker->tone(1500, 200);
        }
        void playDeviceDisconnectedSound()
        {
            // _speaker->setVolume(64);
            _speaker->tone(1500, 100);
            vTaskDelay(50);
            _speaker->tone(1000, 200);
        }
#endif
#ifdef HAVE_BATTERY
        uint8_t getBatLevel() override;
        double getBatVoltage() override;
#endif
#ifdef HAVE_SDCARD
        void _init_sdcard();
#endif
#ifdef HAVE_USB
        void _init_usb();
#endif
    public:
    };
} // namespace HAL
