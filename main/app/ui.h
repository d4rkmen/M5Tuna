#pragma once

#include "hal/hal.h"
#include <string>
#include "defines.h"
#include "utils/theme/theme_define.h"
#include "utils/anim/hl_text.h"
// Placeholder defines - adjust as needed
#define NOTE_CIRCLE_RADIUS 60
#define PITCH_CIRCLE_RADIUS 60
#define MAX_PITCH_DEVIATION_PX 120
#define NOTE_TEXT_FONT FONT_16
#define OCTAVE_TEXT_FONT FONT_16
#define TARGET_COLOR TFT_ORANGE
#define SUCCESS_COLOR TFT_GREEN
#define SIGNAL_LOST_HOLD_TIME 1000
#define BACKGROUND_COLOR TFT_BLACK
#define NOTE_TEXT_COLOR TFT_BLACK
#define PITCH_CIRCLE_COLOR TFT_CYAN

#define SMOOTH_FACTOR 0.18f
#define IN_TUNE_CENTS 5.0f
#define IN_TUNE_PX (IN_TUNE_CENTS / 50.0f * MAX_PITCH_DEVIATION_PX)
#define OUT_OF_TUNE_PX (IN_TUNE_PX * 2.0f)
#define IN_TUNE_STABLE_MS 300

class TunerUI
{
private:
    HAL::Hal* _hal;
    LGFX_Sprite* _canvas;

    // Current state
    float _current_freq;
    std::string _target_note;
    int _target_octave;
    float _target_freq;
    float _display_offset_x;
    float _target_offset_x;

    bool _in_tune;
    uint32_t _in_tune_since;

    bool _needs_update;
    TunerMode _mode;
    uint8_t _max_strings;
    uint8_t _cur_string;

    uint32_t _strings_rendered_time;
    uint32_t _signal_lost_time;
    UTILS::HL_TEXT::HLTextContext_t _hint_ctx;
    void _calculate_pitch_offset();

public:
    TunerUI(HAL::Hal* hal);
    ~TunerUI();

    void init(); // Optional initialization if needed
    void update_freq(float current_freq, const std::string& target_note, int target_octave, float target_freq);
    bool render(); // Returns true if the canvas was updated
    void update_mode(TunerMode mode);
    void update_string(uint8_t string);
    bool isInTune() const { return _in_tune; }
};