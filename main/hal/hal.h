/**
 * @file hal.h
 * @author Forairaaaaa
 * @brief
 * @version 0.1
 * @date 2023-09-18
 *
 * @copyright Copyright (c) 2023
 *
 */
#pragma once
#include "board.h"
#include "LovyanGFX.h"
#include "i2c/i2c_master.h"
#include "keyboard/keyboard.h"
#include "speaker/speaker.h"
#include "mic/mic.h"
#include "es8311/es8311.h"
#include "bat/battery.h"
#include "led/led.h"
#include <iostream>
#include <string>

namespace HAL
{
    /**
     * @brief Hal base for DI
     *
     */
    class Hal
    {
    protected:
        LGFX_Device* _display;
        LGFX_Sprite* _canvas;

        KEYBOARD::Keyboard* _keyboard;
        I2CMaster* _i2c;
#ifdef HAVE_MIC
        Mic* _mic;
#endif
#ifdef HAVE_SPEAKER
        Speaker* _speaker;
#endif
        ES8311* _es8311;
        Battery* _battery;
        LED* _led;
        BoardType _board_type;

    public:
        Hal()
            : _display(nullptr), _canvas(nullptr), _keyboard(nullptr), _i2c(nullptr)
#ifdef HAVE_MIC
              ,
              _mic(nullptr)
#endif
#ifdef HAVE_SPEAKER
              ,
              _speaker(nullptr)
#endif
              ,
              _es8311(nullptr), _battery(nullptr), _led(nullptr)
#ifdef HAVE_WIFI
              ,
              _wifi(nullptr)
#endif
              ,
              _board_type(BoardType::AUTO_DETECT)
        {
        }

        // Getter
        inline LGFX_Device* display() { return _display; }
        inline LGFX_Sprite* canvas() { return _canvas; }
        inline KEYBOARD::Keyboard* keyboard() { return _keyboard; }
        inline I2CMaster* i2c() { return _i2c; }
#ifdef HAVE_MIC
        inline Mic* mic() { return _mic; }
#endif
#ifdef HAVE_SPEAKER
        inline Speaker* speaker() { return _speaker; }
#endif
#ifdef HAVE_WIFI
        inline WiFi* wifi() { return _wifi; }
#endif
        inline ES8311* es8311() { return _es8311; }
        inline Battery* bat() { return _battery; }
        inline LED* led() { return _led; }
        inline BoardType board_type() const { return _board_type; }

        // Canvas
        inline void canvas_update() { _canvas->pushSprite(0, 0); }

        // Override
        virtual std::string type() { return "null"; }
        virtual void init() {}

#ifdef HAVE_SPEAKER
        virtual void playLastSound() {}
        virtual void playNextSound() {}
        virtual void playKeyboardSound() {}
        virtual void playErrorSound() {}
        virtual void playDeviceConnectedSound() {}
        virtual void playDeviceDisconnectedSound() {}
#endif
#ifdef HAVE_BATTERY
        virtual uint8_t getBatLevel() { return 100; }
        virtual float getBatVoltage() { return 4.15f; }
#endif
    };
} // namespace HAL
