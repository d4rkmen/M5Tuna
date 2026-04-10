#include "ui.h"
#include "esp_log.h"
#include <cmath>  // For std::log2, std::abs
#include <string> // For std::to_string
#include "common_define.h"
#include <app/assets/tuna.h>

// Tuning parameters
const float CENTS_PER_SEMITONE = 100.0f;
// Define how many cents correspond to the maximum pixel deviation
const float MAX_DEVIATION_CENTS = 50.0f; // +/- 50 cents (half a semitone) maps to MAX_PITCH_DEVIATION_PX

static const char* TAG = "UI";
static const char* mode_names[] = {"AUTO", "GUITAR", "UKULELE", "VIOLIN"};

static const char* control_hint = "[\u2190][\u2192] MODE [\u2191][\u2193] STRING";
static const char* control_hint_auto = "[\u2190][\u2192] MODE";

TunerUI::TunerUI(HAL::Hal* hal)
    : _hal(hal), _canvas(_hal->canvas()), _current_freq(0.0f), _target_note(""), _target_octave(-1), _target_freq(0.0f),
      _display_offset_x(0.0f), _target_offset_x(0.0f), _in_tune(false), _in_tune_since(0), _needs_update(true),
      _mode(MODE_GUITAR), _max_strings(6), _cur_string(5), _strings_rendered_time(0), _signal_lost_time(0)
{
    init();
    UTILS::HL_TEXT::hl_text_init(&_hint_ctx, _canvas, HINT_ANIMATION_SPEED, HINT_ANIMATION_DELAY);
}

TunerUI::~TunerUI() { UTILS::HL_TEXT::hl_text_free(&_hint_ctx); }

void TunerUI::init()
{
    // draw circle animation
    int center_x = _canvas->width() / 2;
    int center_y = _canvas->height() / 2;
    for (int r = 160; r > NOTE_CIRCLE_RADIUS; r--)
    {
        _canvas->fillScreen(BACKGROUND_COLOR);
        _canvas->fillCircle(center_x, center_y, r, TARGET_COLOR);
        _hal->canvas_update();
        // exponential delay
        delay((r - NOTE_CIRCLE_RADIUS) / 20);
    }
    // print version
    _canvas->pushImage(center_x - 48 / 2, 16, 48, 24, image_data_tuna, TFT_ORANGE);
    _canvas->setFont(NOTE_TEXT_FONT);
    _canvas->setTextSize(2);
    _canvas->setTextColor(TFT_WHITE);
    _canvas->drawCenterString("M5Tuna", center_x, center_y - 20);
    _canvas->setTextColor(NOTE_TEXT_COLOR);
    _canvas->drawCenterString(BUILD_NUMBER, center_x, center_y + 10);
    _hal->canvas_update();
    delay(2000);
    _needs_update = true; // Ensure initial render
}

void TunerUI::_calculate_pitch_offset()
{
    if (_target_freq <= 0 || _current_freq <= 0)
    {
        _target_offset_x = 0;
        return;
    }

    float cents_difference = CENTS_PER_SEMITONE * 12.0f * std::log2(_current_freq / _target_freq);
    cents_difference = std::max(-MAX_DEVIATION_CENTS, std::min(MAX_DEVIATION_CENTS, cents_difference));
    _target_offset_x = (cents_difference / MAX_DEVIATION_CENTS) * MAX_PITCH_DEVIATION_PX;
}

void TunerUI::update_freq(float current_freq, const std::string& target_note, int target_octave, float target_freq)
{
    static float last_freq = -1;
    if ((current_freq != _current_freq || target_freq != _target_freq || target_note != _target_note ||
         target_octave != _target_octave))
    {
        uint32_t current_time = millis();
        if ((current_freq < 0) && (last_freq != current_freq) && (current_time - _signal_lost_time >= SIGNAL_LOST_HOLD_TIME))
        {
            // signal lost
            _signal_lost_time = current_time;
        }
        // if signal lost, don't update the target note and octave
        if ((current_time - _signal_lost_time > SIGNAL_LOST_HOLD_TIME))
        {
            _current_freq = current_freq;
            _target_note = target_note;
            _target_octave = target_octave;
            _target_freq = target_freq;
        }
        last_freq = current_freq;

        _calculate_pitch_offset();
        _needs_update = true;
    }
}

void TunerUI::update_mode(TunerMode mode)
{
    _mode = mode;
    _needs_update = true;
    _max_strings = _get_max_strings(mode);
    UTILS::HL_TEXT::hl_text_reset(&_hint_ctx);
    // highest string is the current string
    update_string(_max_strings - 1);
}

void TunerUI::update_string(uint8_t string)
{
    _cur_string = string;
    _strings_rendered_time = millis();
    _needs_update = true;
}

bool TunerUI::render()
{
    uint32_t current_time = millis();
    static uint32_t last_render_time = 0;

    bool is_draw_strings = (current_time - _strings_rendered_time < STRINGS_DISPLAY_TIME_MS) && _mode != MODE_AUTO;
    _needs_update |= is_draw_strings;

    if (current_time - last_render_time > 33)
    {
        last_render_time = current_time;
        _needs_update = true;
    }

    // Smooth interpolation runs every frame regardless of _needs_update
    float prev_display = _display_offset_x;
    if (_current_freq > 0)
        _display_offset_x += (_target_offset_x - _display_offset_x) * SMOOTH_FACTOR;
    else
        _display_offset_x += (0.0f - _display_offset_x) * SMOOTH_FACTOR;

    if (std::abs(_display_offset_x - prev_display) > 0.3f)
        _needs_update = true;

    // In-tune stability with hysteresis
    float abs_offset = std::abs(_display_offset_x);
    float threshold = _in_tune ? OUT_OF_TUNE_PX : IN_TUNE_PX;
    if (_current_freq > 0 && abs_offset <= threshold)
    {
        if (_in_tune_since == 0)
            _in_tune_since = current_time;
        if ((current_time - _in_tune_since) >= IN_TUNE_STABLE_MS)
            _in_tune = true;
    }
    else
    {
        _in_tune = false;
        _in_tune_since = 0;
    }

    if (!_needs_update)
    {
        return false;
    }

    _canvas->fillScreen(BACKGROUND_COLOR);

    int center_x = _canvas->width() / 2;
    int center_y = _canvas->height() / 2;

    _canvas->fillCircle(center_x, center_y, NOTE_CIRCLE_RADIUS, TARGET_COLOR);

    if (_current_freq > 0)
    {
        int pitch_circle_center_x = center_x + static_cast<int>(_display_offset_x);

        if (_in_tune)
        {
            _canvas->fillCircle(pitch_circle_center_x, center_y, PITCH_CIRCLE_RADIUS, SUCCESS_COLOR);
            _canvas->drawCircle(center_x, center_y, NOTE_CIRCLE_RADIUS + 3, TFT_WHITE);
            _canvas->drawCircle(center_x, center_y, NOTE_CIRCLE_RADIUS + 4, TFT_WHITE);
        }
        else
        {
            // Red (far) -> yellow (close) color gradient
            float t = 1.0f - std::min(abs_offset / (float)MAX_PITCH_DEVIATION_PX, 1.0f);
            uint16_t color = _canvas->color565(255, (uint8_t)(t * 255), 0);

            int arrow_dir = (_display_offset_x > 0) ? 1 : -1;
            int arrow_x = pitch_circle_center_x + arrow_dir * (PITCH_CIRCLE_RADIUS + 20);
            _canvas->fillTriangle(arrow_x,
                                  center_y,
                                  arrow_x + arrow_dir * 10,
                                  center_y - 10,
                                  arrow_x + arrow_dir * 10,
                                  center_y + 10,
                                  TFT_DARKGRAY);

            _canvas->fillCircle(pitch_circle_center_x, center_y, PITCH_CIRCLE_RADIUS - 2, color);
        }
    }

    // Waveform visualization: symmetric filled bars + bright contour
    if (g_waveform_active)
    {
        int w = _canvas->width();
        uint16_t fill_color = TFT_DARKGRAY;
        uint16_t edge_color = TFT_LIGHTGREY;

        for (int x = 0; x < w; x++)
        {
            int idx = x * WAVEFORM_SAMPLES / w;
            int amp = abs(g_waveform[idx]);
            if (amp < 1)
                continue;
            _canvas->drawFastVLine(x, WAVEFORM_Y - amp, amp * 2, fill_color);
        }

        for (int x = 1; x < w; x++)
        {
            int idx0 = (x - 1) * WAVEFORM_SAMPLES / w;
            int idx1 = x * WAVEFORM_SAMPLES / w;
            int a0 = abs(g_waveform[idx0]);
            int a1 = abs(g_waveform[idx1]);
            _canvas->drawLine(x - 1, WAVEFORM_Y - a0, x, WAVEFORM_Y - a1, edge_color);
            _canvas->drawLine(x - 1, WAVEFORM_Y + a0, x, WAVEFORM_Y + a1, edge_color);
        }
    }

    // 2. Draw the note name and octave number inside the target circle

    // draw strings
    if (is_draw_strings)
    {
        _canvas->setFont(NOTE_TEXT_FONT);
        _canvas->setTextSize(1);
        const uint8_t string_h = _canvas->fontHeight() + 2;
        const uint8_t string_w = _canvas->textWidth("000");
        const uint16_t all_strings_h = string_h * _max_strings;
        const uint16_t start_y = center_y - all_strings_h / 2;
        for (uint8_t i = 0; i < _max_strings; i++)
        {
            if (i == _cur_string)
            {
                _canvas->fillRoundRect(0, start_y + i * string_h, string_w + 8, string_h, 4, TFT_LIGHTGREY);
                _canvas->setTextColor(TFT_BLACK);
            }
            else
            {
                _canvas->drawRoundRect(0, start_y + i * string_h, string_w + 8, string_h, 4, TFT_LIGHTGREY);
                _canvas->setTextColor(TFT_LIGHTGREY);
            }
            _canvas->drawCenterString(std::to_string(_max_strings - i).c_str(), (string_w + 8) / 2, start_y + i * string_h + 1);
        }
    }
    // Draw Note Name (Large)
    _canvas->setTextColor(NOTE_TEXT_COLOR);
    _canvas->setFont(NOTE_TEXT_FONT);
    _canvas->setTextSize(6);
    _canvas->drawCenterString(_target_note.c_str(), center_x, center_y - _canvas->fontHeight() / 2 - 14);

    // Draw Octave Number (Smaller) - if valid
    if (_target_octave >= 0)
    {
        _canvas->setFont(OCTAVE_TEXT_FONT);
        _canvas->setTextSize(2);
        std::string octave_str = std::to_string(_target_octave);
        _canvas->drawCenterString(octave_str.c_str(), center_x, center_y + _canvas->fontHeight() / 2 + 8);
    }
    // draw title
    _canvas->setFont(NOTE_TEXT_FONT);
    _canvas->setTextSize(1);
    _canvas->setTextColor(NOTE_TEXT_COLOR);
    _canvas->drawCenterString(mode_names[_mode], center_x, 10);

    const char* hint = (_mode == MODE_AUTO) ? control_hint_auto : control_hint;
    int hint_y = _canvas->height() - _hint_ctx.sprite->height();
    _needs_update |= UTILS::HL_TEXT::hl_text_render(&_hint_ctx, hint, 0, hint_y, TFT_SILVER, TFT_WHITE, BACKGROUND_COLOR);

    _needs_update = false;
    return true;
}
