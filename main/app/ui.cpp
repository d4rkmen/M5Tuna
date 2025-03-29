#include "ui.h"
#include "esp_log.h"
#include <cmath>  // For std::log2, std::abs
#include <string> // For std::to_string
#include <app/utils/common_define.h>
#include <app/assets/tuna.h>

// Tuning parameters
const float CENTS_PER_SEMITONE = 100.0f;
// Define how many cents correspond to the maximum pixel deviation
const float MAX_DEVIATION_CENTS = 50.0f; // +/- 50 cents (half a semitone) maps to MAX_PITCH_DEVIATION_PX

static const char* TAG = "UI";
static const char* mode_names[] = {"AUTO", "GUITAR", "UKULELE", "VIOLIN"};

static const char* control_hint = "[LEFT]-[RIGHT] MODE [UP]-[DOWN] STRING";
static const char* control_hint_auto = "[LEFT]-[RIGHT] MODE";
static int hint_char_index = -1;
static uint32_t hint_update_time = 0;
static uint32_t hint_timeout = HINT_ANIMATION_DELAY;

TunerUI::TunerUI(HAL::Hal* hal)
    : _hal(hal), _canvas(_hal->canvas()), _text(new LGFX_Sprite(_hal->canvas())), _current_freq(0.0f), _target_note(""),
      _target_octave(-1), _target_freq(0.0f), _pitch_offset_x(0.0f), _needs_update(true), _mode(MODE_GUITAR), _max_strings(6),
      _cur_string(5), _strings_rendered_time(0), _signal_lost_time(0)
{
    // _text = new LGFX_Sprite(_hal->canvas());
    _text->createSprite(_hal->canvas()->width(), _hal->canvas()->height());
    init();
}

TunerUI::~TunerUI()
{
    _text->deleteSprite();
    delete _text;
}

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
    _text->fillScreen(TFT_TRANSPARENT);
    _text->pushImage(center_x - 48 / 2, 16, 48, 24, image_data_tuna, TFT_ORANGE);
    _text->setFont(NOTE_TEXT_FONT);
    _text->setTextSize(2);
    _text->setTextColor(TFT_WHITE, TFT_TRANSPARENT); // Text color, background color (transparent)
    _text->drawCenterString("M5Tuna", center_x, center_y - 20);
    _text->setTextColor(NOTE_TEXT_COLOR, TFT_TRANSPARENT); // Text color, background color (transparent)
    _text->drawCenterString(BUILD_NUMBER, center_x, center_y + 10);
    _text->pushSprite(_canvas, 0, 0, TFT_TRANSPARENT);
    _hal->canvas_update();
    delay(2000);
    _needs_update = true; // Ensure initial render
}

void TunerUI::_calculate_pitch_offset()
{
    if (_target_freq <= 0 || _current_freq <= 0)
    {
        _pitch_offset_x = 0; // No valid frequencies, center the pitch circle
        return;
    }

    // Calculate difference in cents
    // Cents = 1200 * log2(f2 / f1)
    float cents_difference = CENTS_PER_SEMITONE * 12.0f * std::log2(_current_freq / _target_freq);

    // Clamp the cents difference to the maximum deviation range
    cents_difference = std::max(-MAX_DEVIATION_CENTS, std::min(MAX_DEVIATION_CENTS, cents_difference));

    // Map cents difference to pixel offset
    // Linear mapping: offset = (cents / max_cents) * max_pixels
    _pitch_offset_x = (cents_difference / MAX_DEVIATION_CENTS) * MAX_PITCH_DEVIATION_PX;
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
    // reset hint animation
    animateHintReset();
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

    if (!_needs_update)
    {
        return false; // Nothing changed, no need to re-render
    }

    // Clear the canvas (or relevant area if optimizing)
    _canvas->fillScreen(BACKGROUND_COLOR);

    // Calculate center positions
    int center_x = _canvas->width() / 2;
    int center_y = _canvas->height() / 2;

    // 1. Draw the filled orange target note circle
    // if pitch is in the range of 10 cents, draw the circle in green
    _canvas->fillCircle(center_x, center_y, NOTE_CIRCLE_RADIUS, TARGET_COLOR);

    // 2. Draw the note name and octave number inside the target circle

    _text->fillScreen(TFT_TRANSPARENT);
    _text->setTextColor(NOTE_TEXT_COLOR, TFT_TRANSPARENT); // Text color, background color (transparent)
    // draw strings
    if (is_draw_strings)
    {
        _text->setFont(NOTE_TEXT_FONT);
        _text->setTextSize(1);
        // draw strings where selected is hightlighted
        const uint8_t string_h = _text->fontHeight() + 2;
        const uint8_t string_w = _text->textWidth("000");
        const uint16_t all_strings_h = string_h * _max_strings;
        const uint16_t start_y = center_y - all_strings_h / 2;
        for (uint8_t i = 0; i < _max_strings; i++)
        {
            if (i == _cur_string)
            {
                _text->fillRoundRect(-8, start_y + i * string_h, string_w + 16, string_h, 4, TFT_LIGHTGREY);
                _text->setTextColor(TFT_BLACK, TFT_LIGHTGREY);
            }
            else
            {
                _text->drawRoundRect(-8, start_y + i * string_h, string_w + 16, string_h, 4, TFT_LIGHTGREY);
                _text->setTextColor(TFT_LIGHTGREY, TFT_TRANSPARENT);
            }
            _text->drawCenterString(std::to_string(_max_strings - i).c_str(), string_w / 2, start_y + i * string_h + 1);
        }
    }
    // Draw Note Name (Large)
    _text->setTextColor(NOTE_TEXT_COLOR, TFT_TRANSPARENT); // Text color, background color (transparent)
    _text->setFont(NOTE_TEXT_FONT);
    _text->setTextSize(6);
    // Adjust Y position slightly upwards to make space for octave
    _text->drawCenterString(_target_note.c_str(), center_x, center_y - _text->fontHeight() / 2 - 14);

    // Draw Octave Number (Smaller) - if valid
    if (_target_octave >= 0)
    {
        _text->setFont(OCTAVE_TEXT_FONT);
        _text->setTextSize(2);
        std::string octave_str = std::to_string(_target_octave);
        _text->drawCenterString(octave_str.c_str(), center_x, center_y + _text->fontHeight() / 2 + 8);
    }
    // draw title
    _text->setFont(NOTE_TEXT_FONT);
    _text->setTextSize(1);
    _text->setTextColor(NOTE_TEXT_COLOR, TFT_TRANSPARENT); // Text color, background color (transparent)
    _text->drawCenterString(mode_names[_mode], center_x, 10);

    // 3. Draw the empty pitch circle at the calculated offset
    if (_current_freq > 0)
    {
        int pitch_circle_center_x = center_x + static_cast<int>(_pitch_offset_x);
        int color = SUCCESS_COLOR;
        int r = PITCH_CIRCLE_RADIUS;
        // draw arrows (triangles) pointed to the pitch circle
        if (abs(_pitch_offset_x) > 10)
        {
            color = TUNING_COLOR;
            r = PITCH_CIRCLE_RADIUS - 2;
            if (_pitch_offset_x > 0)
            {
                _canvas->fillTriangle(pitch_circle_center_x + PITCH_CIRCLE_RADIUS + 20,
                                      center_y,
                                      pitch_circle_center_x + PITCH_CIRCLE_RADIUS + 20 + 10,
                                      center_y - 10,
                                      pitch_circle_center_x + PITCH_CIRCLE_RADIUS + 20 + 10,
                                      center_y + 10,
                                      TFT_DARKGRAY);
            }
            else
            {
                _canvas->fillTriangle(pitch_circle_center_x - PITCH_CIRCLE_RADIUS - 20,
                                      center_y,
                                      pitch_circle_center_x - PITCH_CIRCLE_RADIUS - 20 - 10,
                                      center_y - 10,
                                      pitch_circle_center_x - PITCH_CIRCLE_RADIUS - 20 - 10,
                                      center_y + 10,
                                      TFT_DARKGRAY);
            }
        }
        _canvas->fillCircle(pitch_circle_center_x, center_y, r, color);
    }
    // draw control hint
    // _text->setFont(&fonts::efontEN_10);
    // _text->setTextSize(1);
    // _text->setTextColor(TFT_DARKGREY, TFT_TRANSPARENT); // Text color, background color (transparent)
    // _text->drawCenterString(_mode == MODE_AUTO ? control_hint_auto : control_hint, center_x, _text->height() - 12);

    animateHintText(_mode == MODE_AUTO ? control_hint_auto : control_hint);
    // draw the text to the canvas
    _text->pushSprite(_canvas, 0, 0, TFT_TRANSPARENT);

    _needs_update = false; // Mark as updated
    return true;           // Canvas was updated
}

void TunerUI::animateHintReset()
{
    hint_update_time = 0;
    hint_char_index = -1;
    hint_timeout = HINT_ANIMATION_DELAY;
}

void TunerUI::animateHintText(const char* text)
{
    int y_offset = _text->height() - 12;

    _text->setFont(&fonts::efontEN_10);
    _text->setTextSize(1);
    _text->setTextColor(TFT_SILVER, TFT_TRANSPARENT);
    _text->drawCenterString(text, _text->width() / 2, y_offset);

    if (hint_char_index >= 0)
    {
        char highlighted_char[2] = {text[hint_char_index], '\0'};
        _text->setTextColor(TFT_WHITE, TFT_TRANSPARENT);

        // Calculate position for the single character
        int char_width = _text->textWidth(text);
        int start_x = _text->width() / 2 - char_width / 2;
        int char_pos = hint_char_index * _text->textWidth("0");
        _text->drawString(highlighted_char, start_x + char_pos, y_offset);
    }

    uint32_t now = millis();
    if ((now - hint_update_time) > hint_timeout)
    {
        hint_char_index++;
        if (text[hint_char_index] != '\0')
        {
            hint_timeout = HINT_ANIMATION_SPEED;
        }
        else
        {
            hint_char_index = -1;
            hint_timeout = HINT_ANIMATION_DELAY;
        }

        hint_update_time = now;
    }
}
