#pragma once

#include "hal/hal.h"
#include <string>
#include "defines.h"
// Placeholder defines - adjust as needed
#define NOTE_CIRCLE_RADIUS 60
#define PITCH_CIRCLE_RADIUS 60
#define MAX_PITCH_DEVIATION_PX 120          // Max pixels the pitch circle can move left/right
#define NOTE_TEXT_FONT &fonts::efontEN_16   // Placeholder M5GFX Font
#define OCTAVE_TEXT_FONT &fonts::efontEN_16 // Placeholder M5GFX Font
#define TARGET_COLOR TFT_ORANGE             // Use standard M5GFX orange
#define TUNING_COLOR TFT_GREENYELLOW
#define SUCCESS_COLOR TFT_GREEN
#define SIGNAL_LOST_HOLD_TIME 1000
#define BACKGROUND_COLOR TFT_BLACK
#define NOTE_TEXT_COLOR TFT_BLACK
#define PITCH_CIRCLE_COLOR TFT_CYAN

class TunerUI
{
private:
    HAL::Hal* _hal;
    LGFX_Sprite* _canvas;
    LGFX_Sprite* _text;

    // Current state
    float _current_freq;
    std::string _target_note;
    int _target_octave;
    float _target_freq;
    float _pitch_offset_x; // Calculated offset for the pitch circle

    bool _needs_update;
    TunerMode _mode;
    uint8_t _max_strings;
    uint8_t _cur_string;

    uint32_t _strings_rendered_time;
    uint32_t _signal_lost_time;
    void _calculate_pitch_offset();

public:
    TunerUI(HAL::Hal* hal);
    ~TunerUI();

    void init(); // Optional initialization if needed
    void update_freq(float current_freq, const std::string& target_note, int target_octave, float target_freq);
    bool render(); // Returns true if the canvas was updated
    void update_mode(TunerMode mode);
    void update_string(uint8_t string);
    void animateHintText(const char* text);
    void animateHintReset();
};