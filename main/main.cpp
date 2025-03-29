/**
 * @file main.cpp
 * @author d4rkmen
 * @brief
 * @version 1.0
 * @date 2025-03-21
 *
 * @copyright Copyright (c) 2025
 *
 */
#include <stdio.h>
#include "hal/hal_cardputer.h"
#include "app/utils/common_define.h"
#ifdef HAVE_SETTINGS
#include "settings/settings.h"
#endif
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "defines.h"
#include "pitch_detector_task.h"
#include "app/ui.h"
#include <string>

static const char* TAG = "M5Tuna";

static bool is_repeat = false;
static bool is_start = false;
// keyboard constants
#define KEY_HOLD_MS 800
#define KEY_REPEAT_MS 200

extern void pitch_detector_task(void* pvParameter);
TaskHandle_t detectorTaskHandle;
TaskHandle_t guiTaskHandle;
QueueHandle_t frequencyQueue;

using namespace HAL;
#ifdef HAVE_SETTINGS
using namespace SETTINGS;
#endif

#ifdef HAVE_SETTINGS
Settings settings;
#endif
HalCardputer hal;

// Define string notes for each instrument (using note enum values)
const TunerNoteName GuitarStrings[6] = {
    NOTE_E, // E2 (lowest)
    NOTE_A, // A2
    NOTE_D, // D3
    NOTE_G, // G3
    NOTE_B, // B3
    NOTE_E  // E4 (highest)
};

const TunerNoteName UkuleleStrings[4] = {
    NOTE_G, // G4 (highest)
    NOTE_C, // C4
    NOTE_E, // E4
    NOTE_A  // A4
};

const TunerNoteName ViolinStrings[4] = {
    NOTE_G, // G3 (lowest)
    NOTE_D, // D4
    NOTE_A, // A4
    NOTE_E  // E5 (highest)
};

// Define octaves for each string of each instrument
const int GuitarOctaves[6] = {2, 2, 3, 3, 3, 4};
const int UkuleleOctaves[4] = {3, 4, 4, 4};
const int ViolinOctaves[4] = {3, 4, 4, 5};

const float GuitarFrequencies[6] = {
    82.41f,  // E2
    110.00f, // A2
    146.83f, // D3
    196.00f, // G3
    246.94f, // B3
    329.63f, // E4
};

const float UkuleleFrequencies[4] = {
    196.00f, // G3
    261.63f, // C4
    329.63f, // E4
    440.00f, // A4
};

const float ViolinFrequencies[4] = {
    196.00f, // G3
    293.66f, // E4
    440.00f, // A4
    659.26f, // E5
};

// Standard frequencies for each note (would need to be expanded for all notes)
const float NoteFrequencies[12] = {
    261.63f, // C
    277.18f, // C#
    293.66f, // D
    311.13f, // D#
    329.63f, // E
    349.23f, // F
    369.99f, // F#
    392.00f, // G
    415.30f, // G#
    440.00f, // A
    466.16f, // A#
    493.88f  // B
};

// Helper function to get note frequency with octave adjustment
float getNoteFrequency(TunerNoteName note, int octave)
{
    // A4 is 440Hz (note A, octave 4)
    // For each octave difference from 4, multiply/divide by 2
    float baseFreq = NoteFrequencies[note];
    // Adjust for C4 reference to the actual octave
    return baseFreq * pow(2, octave - 4);
}

const char* noteNames[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
std::string getNoteString(TunerNoteName noteName)
{
    if (noteName >= NOTE_C && noteName <= NOTE_B)
    {
        return noteNames[static_cast<int>(noteName)];
    }
    return "";
}

uint8_t _get_max_strings(TunerMode mode)
{
    // Set max strings based on current mode
    switch (mode)
    {
    case MODE_GUITAR:
        return 6;
    case MODE_UKULELE:
    case MODE_VIOLIN:
        return 4;
        break;
    default:
        return 0; // Not applicable in auto mode
    }
}

void tuner_gui_task(void* pvParameter)
{
    ESP_LOGI(TAG, "tuner_gui_task started");
    Hal* hal = (Hal*)pvParameter;
    FrequencyInfo receivedFreqInfo;

    TunerUI* tunerUI = new TunerUI(hal);
    if (!tunerUI)
    {
        ESP_LOGE(TAG, "Failed to create TunerUI");
        return;
    }
    // Initialize mode tracking variables
    TunerMode currentMode = MODE_GUITAR;
    int maxStrings = _get_max_strings(currentMode);
    int currentString = maxStrings - 1;
    tunerUI->update_mode(currentMode);
    tunerUI->update_string(currentString);
    while (1)
    {
        // Get current frequency info
        if (!xQueuePeek(frequencyQueue, &receivedFreqInfo, 0))
            receivedFreqInfo.frequency = -1;

        // Handle keyboard input with debouncing
        hal->keyboard()->updateKeyList();
        if (hal->keyboard()->isPressed())
        {
            // Mode switching with UP/DOWN
            if (hal->keyboard()->isKeyPressing(KEY_NUM_RIGHT))
            {
                if (!is_repeat || !hal->keyboard()->waitForRelease(KEY_NUM_RIGHT, is_start ? KEY_HOLD_MS : KEY_REPEAT_MS))
                {
                    is_start = !is_repeat;
                    is_repeat = true;

                    // Circular mode change (UP)
                    currentMode = static_cast<TunerMode>((currentMode + 1) % MODE_COUNT);
                    maxStrings = _get_max_strings(currentMode);
                    currentString = maxStrings - 1;
                    tunerUI->update_mode(currentMode);
                }
            }
            else if (hal->keyboard()->isKeyPressing(KEY_NUM_LEFT))
            {
                if (!is_repeat || !hal->keyboard()->waitForRelease(KEY_NUM_LEFT, is_start ? KEY_HOLD_MS : KEY_REPEAT_MS))
                {
                    is_start = !is_repeat;
                    is_repeat = true;
                    // Circular mode change (DOWN)
                    currentMode = static_cast<TunerMode>((currentMode + MODE_COUNT - 1) % MODE_COUNT);
                    maxStrings = _get_max_strings(currentMode);
                    currentString = maxStrings - 1;
                    tunerUI->update_mode(currentMode);
                }
            }
            else if (hal->keyboard()->isKeyPressing(KEY_NUM_UP))
            {
                // String switching with LEFT/RIGHT (only applicable in non-auto modes)
                if (currentMode != MODE_AUTO)
                {
                    if (!is_repeat || !hal->keyboard()->waitForRelease(KEY_NUM_UP, is_start ? KEY_HOLD_MS : KEY_REPEAT_MS))
                    {
                        is_start = !is_repeat;
                        is_repeat = true;
                        // Circular string change (LEFT - previous string)
                        currentString = (currentString + maxStrings - 1) % maxStrings;
                        ESP_LOGI(TAG, "String changed to %d", currentString);
                        tunerUI->update_string(currentString);
                    }
                }
            }
            else if (hal->keyboard()->isKeyPressing(KEY_NUM_DOWN))
            {
                if (currentMode != MODE_AUTO)
                {
                    if (!is_repeat || !hal->keyboard()->waitForRelease(KEY_NUM_DOWN, is_start ? KEY_HOLD_MS : KEY_REPEAT_MS))
                    {
                        is_start = !is_repeat;
                        is_repeat = true;
                        // Circular string change
                        currentString = (currentString + 1) % maxStrings;
                        ESP_LOGI(TAG, "String changed to %d", currentString);
                        tunerUI->update_string(currentString);
                    }
                }
            }
        }
        else
            is_repeat = false;

        // For auto mode, use frequency detector's output
        // For other modes, use predefined target notes

        float targetFreq = 0.0f;
        std::string targetNote;
        int targetOctave = -1;
        float currentFreq = receivedFreqInfo.frequency;

        if (currentMode == MODE_AUTO)
        {
            // Use frequency detector's output directly
            targetNote = getNoteString(receivedFreqInfo.targetNote);
            targetOctave = receivedFreqInfo.targetOctave;
            targetFreq = receivedFreqInfo.targetFrequency;
        }
        else
        {
            // Use predefined target note based on instrument and string
            TunerNoteName noteEnum;

            switch (currentMode)
            {
            case MODE_GUITAR:
                noteEnum = GuitarStrings[currentString];
                targetOctave = GuitarOctaves[currentString];
                targetFreq = GuitarFrequencies[currentString];
                break;
            case MODE_UKULELE:
                noteEnum = UkuleleStrings[currentString];
                targetOctave = UkuleleOctaves[currentString];
                targetFreq = UkuleleFrequencies[currentString];
                break;
            case MODE_VIOLIN:
                noteEnum = ViolinStrings[currentString];
                targetOctave = ViolinOctaves[currentString];
                targetFreq = ViolinFrequencies[currentString];
                break;
            default:
                noteEnum = NOTE_A; // Failsafe
                targetOctave = 4;
                targetFreq = getNoteFrequency(noteEnum, targetOctave);
                break;
            }

            targetNote = getNoteString(noteEnum);
        }

        // Update UI
        tunerUI->update_freq(currentFreq, targetNote, targetOctave, targetFreq);

        // Render and update canvas if needed
        if (tunerUI->render())
        {
            hal->canvas_update();
        }
        delay(5);
    }
}

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "M5Tuna Guitar Tuner - Starting...");

    hal.init();

    frequencyQueue = xQueueCreate(FREQUENCY_QUEUE_LENGTH, sizeof(FrequencyInfo));
    if (frequencyQueue == NULL)
    {
        ESP_LOGE(TAG, "Frequency Queue creation failed!");
    }
    else
    {
        ESP_LOGI(TAG, "Frequency Queue created successfully!");
    }

    xTaskCreatePinnedToCore(pitch_detector_task, "pitch_detector", 4096, &hal, 10, &detectorTaskHandle, 1);

    xTaskCreatePinnedToCore(tuner_gui_task, "tuner_gui", 4096, &hal, 5, &guiTaskHandle, 0);

    ESP_LOGI(TAG, "Initialization complete. Tasks running.");
}
