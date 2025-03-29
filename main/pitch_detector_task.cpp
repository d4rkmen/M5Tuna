/*
 * Copyright (c) 2024 Boyd Timothy. All rights reserved.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "hal.h"

#include "defines.h"
#include "pitch_detector_task.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_timer.h"

//
// Q DSP Library for Pitch Detection
//
#include <q/pitch/pitch_detector.hpp>
#include <q/fx/dynamic.hpp>
#include <q/fx/clip.hpp>
#include <q/fx/signal_conditioner.hpp>
#include <q/support/decibel.hpp>
#include <q/support/duration.hpp>
#include <q/support/literals.hpp>
#include <q/support/pitch_names.hpp>
#include <q/support/pitch.hpp>

//
// Smoothing Filters
//
#include "app/utils/exponential_smoother.hpp"
#include "app/utils/OneEuroFilter.h"
#include "app/utils/MovingAverage.hpp"
#include "app/utils/MedianFilter.hpp"

static const char* TAG = "PitchDetector";

namespace q = cycfi::q;
using namespace q::literals;
using std::fixed;

using namespace cycfi::q::pitch_names;
using frequency = cycfi::q::frequency;
using pitch = cycfi::q::pitch;
CONSTEXPR frequency low_fs = cycfi::q::pitch_names::B[0];  // Lowest string on a 5-string bass
CONSTEXPR frequency high_fs = cycfi::q::pitch_names::C[7]; // Setting this higher helps to catch the high harmonics

ExponentialSmoother smoother(EXP_SMOOTHING);
OneEuroFilter oneEUFilter(EU_FILTER_ESTIMATED_FREQ, EU_FILTER_MIN_CUTOFF, EU_FILTER_BETA, EU_FILTER_DERIVATIVE_CUTOFF);
OneEuroFilter oneEUFilter2(EU_FILTER_ESTIMATED_FREQ, EU_FILTER_MIN_CUTOFF_2, EU_FILTER_BETA_2, EU_FILTER_DERIVATIVE_CUTOFF_2);
MovingAverage movingAverage(5);
MedianFilter medianMovingFilter(3, true);
// MedianFilter medianFilter(5, false);

extern QueueHandle_t frequencyQueue;
static TaskHandle_t s_task_handle;

static int16_t adc_buffer[TUNER_FRAME_SIZE];
static std::vector<float> in(TUNER_FRAME_SIZE); // a vector of values to pass into qlib

/// @brief Function to compute the closest note and cent deviation
inline esp_err_t get_frequency_info(float input_freq, FrequencyInfo* freqInfo)
{
    if (input_freq <= 0.0f)
    {
        // Set frequency info to indicate invalid/no frequency
        freqInfo->frequency = input_freq;
        freqInfo->cents = 0.0f;
        freqInfo->targetFrequency = -1.0f;
        freqInfo->targetNote = NOTE_NONE; // Assuming NOTE_NONE indicates no note
        freqInfo->targetOctave = -1;
        return ESP_FAIL;
    }

    // Calculate the MIDI note number (floating point) relative to A4 (MIDI note 69)
    // Use double for intermediate calculations for better precision
    double midi_note_float = 12.0 * log2(static_cast<double>(input_freq) / A4_FREQ) + 69.0;

    // Round to the nearest integer MIDI note
    int midi_note = static_cast<int>(round(midi_note_float));

    // Optional: Add checks for valid MIDI note range (e.g., 0-127) if necessary
    // if (midi_note < 0 || midi_note > 127) { /* Handle out of range */ return ESP_FAIL; }

    // Calculate note index (0=C, 1=C#, ..., 11=B)
    // Ensure the result is non-negative, standard C++ % can yield negative results for negative inputs
    int note_index = (midi_note % 12 + 12) % 12;

    // Calculate octave number. MIDI note 60 is C4. Octave = floor(midi_note / 12) - 1
    // Integer division in C++ truncates towards zero, which works like floor for positive numbers.
    // For negative midi_note values, adjustments might be needed if supporting very low frequencies,
    // but typically midi_note will be >= 0 for audible frequencies.
    int octave = midi_note / 12 - 1;

    // Calculate the frequency of the determined MIDI note
    double closest_note_freq = A4_FREQ * pow(2.0, (static_cast<double>(midi_note) - 69.0) / 12.0);

    // Calculate the cent deviation
    double cents_deviation = 1200.0 * log2(static_cast<double>(input_freq) / closest_note_freq);

    // Populate the output struct
    freqInfo->frequency = input_freq;
    freqInfo->targetFrequency = static_cast<float>(closest_note_freq);
    // Ensure TunerNoteName enum matches the 0=C, 1=C#, ..., 11=B mapping
    freqInfo->targetNote = static_cast<TunerNoteName>(note_index);
    freqInfo->targetOctave = octave;
    freqInfo->cents = static_cast<float>(cents_deviation);

    return ESP_OK;
}

void pitch_detector_task(void* pvParameter)
{
    // Prep ADC
    HAL::Hal* hal = (HAL::Hal*)pvParameter;
    memset(adc_buffer, 0xcc, sizeof(adc_buffer));

    // Get the pitch detector ready
    q::pitch_detector pd(low_fs, high_fs, TUNER_SAMPLE_RATE, -40_dB);

    // auto const&                 bits = pd.bits();
    // auto const&                 edges = pd.edges();
    // q::bitstream_acf<>          bacf{ bits };
    // auto                        min_period = as_float(high_fs.period()) * TUNER_SAMPLE_RATE;

    // q::peak_envelope_follower   env{ 30_ms, TUNER_SAMPLE_RATE };
    // q::one_pole_lowpass         lp{high_fs, TUNER_SAMPLE_RATE};
    // q::one_pole_lowpass         lp2(low_fs, TUNER_SAMPLE_RATE);

    // constexpr float             slope = 1.0f/2;
    // constexpr float             makeup_gain = 2;
    // q::compressor               comp{ -18_dB, slope };
    // q::clip                     clip;

    // float                       onset_threshold = lin_float(-28_dB);
    // float                       release_threshold = lin_float(-60_dB);
    // float                       threshold = onset_threshold;

    auto sc_conf = q::signal_conditioner::config{};
    auto sig_cond = q::signal_conditioner{sc_conf, low_fs, high_fs, TUNER_SAMPLE_RATE};
    q::one_pole_lowpass lp{high_fs, TUNER_SAMPLE_RATE};
    q::one_pole_lowpass lp2{low_fs, TUNER_SAMPLE_RATE};

    s_task_handle = xTaskGetCurrentTaskHandle();
    // TODO start microphone
    auto cfg = hal->mic()->config();
    cfg.dma_buf_count = 8;
    cfg.dma_buf_len = TUNER_FRAME_SIZE;
    cfg.over_sampling = 2;
    cfg.noise_filter_level = 0;
    cfg.sample_rate = TUNER_SAMPLE_RATE;
    cfg.magnification = 4;
    cfg.use_adc = false;
    hal->mic()->config(cfg);
    hal->mic()->begin();

    TunerNoteName lastSeenNote = NOTE_NONE;
    int sameNoteSeenCount = 0;
    FrequencyInfo freqInfo;
    FrequencyInfo noFreq = {
        .frequency = -1,
        .cents = -1,
        .targetFrequency = -1,
        .targetNote = NOTE_NONE,
        .targetOctave = -1,
    };

    TickType_t ticksBetweenFreqDetection = pdMS_TO_TICKS(1);

    while (1)
    {
        while (hal->mic()->isRecording() < 2)
        {

            // recodr the mic
            hal->mic()->record((int16_t*)adc_buffer, TUNER_FRAME_SIZE);
            // Get the data out of the ADC Conversion Result.
            float maxVal = adc_buffer[0];
            float minVal = adc_buffer[0];
            // ESP_LOGI(TAG, "Bytes read: %ld", num_of_bytes_read);
            for (int i = 0; i < TUNER_FRAME_SIZE; i++)
            {
                // Do a first pass by just storing the raw values into the float array
                in[i] = adc_buffer[i];

                // Track the min and max values we see so we can convert to values between -1.0f and +1.0f
                if (in[i] > maxVal)
                {
                    maxVal = in[i];
                }
                if (in[i] < minVal)
                {
                    minVal = in[i];
                }
            }

            // Bail out if the input does not meet the minimum criteria
            float range = maxVal - minVal;
            // ESP_LOGI(TAG, "min: %.0f  max: %.0f  range: %.0f", minVal, maxVal, range);
            if (range < TUNER_READING_DIFF_MINIMUM)
            {
                // ESP_LOGI(TAG, "No frequency detected");
                xQueueOverwrite(frequencyQueue, &noFreq);
                // set_current_frequency(-1); // Indicate to the UI that there's no frequency available
                // oneEUFilter.reset(); // Reset the 1EU filter so the next frequency it detects will be as fast as possible
                // oneEUFilter2.reset();
                smoother.reset();
                movingAverage.reset();
                // medianMovingFilter.reset();
                // medianFilter.reset();
                pd.reset();

                lastSeenNote = NOTE_NONE;
                sameNoteSeenCount = 0;

                vTaskDelay(ticksBetweenFreqDetection);
                continue;
            }
            // Normalize the values between -1.0 and +1.0 before processing with qlib.
            // float midVal = range / 2;
            float midVal = std::max(abs(minVal), abs(maxVal));
            // ESP_LOGI(TAG, "min: %.0f  max: %.0f  range: %.0f  mid: %.0f", minVal, maxVal, range, midVal);
            for (auto i = 0; i < TUNER_FRAME_SIZE; i++)
            {
                // float newPosition = in[i] - midVal - minVal;
                float normalizedValue = in[i] / midVal;
                float s = normalizedValue;
                // s = medianMovingFilter.addValue(s);

                // Signal Conditioner
                s = sig_cond(s);

                // Send in each value into the pitch detector
                if (pd(s) == true)
                { // calculated a frequency
                    auto f = pd.get_frequency() * 2;
                    // 1EU Filtering
                    TimeStamp time_seconds = (double)esp_timer_get_time() / 1000000;
                    oneEUFilter.setFrequency(f);
                    f = (float)oneEUFilter.filter((double)f, (TimeStamp)time_seconds);

                    f = movingAverage.addValue(f);
                    f = smoother.smooth(f);

                    oneEUFilter2.setFrequency(f);
                    f = (float)oneEUFilter2.filter((double)f, time_seconds);
                    // cutoff overtones
                    // if (f > 440)
                    //     continue;
                    if (get_frequency_info(f, &freqInfo) == ESP_OK)
                    {
                        // Only show frequency info if we've seen the
                        // same target note more than once in a row.
                        // Doing this seems to help prevent sporadic
                        // notes from appearing right as you pluck a
                        // string.
                        ESP_LOGI(TAG, "freq: %.2f, range: %.0f", freqInfo.frequency, range);

                        if (lastSeenNote == freqInfo.targetNote)
                        {
                            sameNoteSeenCount++;
                        }
                        else
                        {
                            sameNoteSeenCount = 0;
                        }
                        lastSeenNote = freqInfo.targetNote;

                        if (sameNoteSeenCount > 1)
                        {
                            ESP_LOGI(TAG,
                                     "Frequency: %.2f, Note: %d, Octave: %d, Cents: %.2f",
                                     freqInfo.frequency,
                                     freqInfo.targetNote,
                                     freqInfo.targetOctave,
                                     freqInfo.cents);
                            xQueueOverwrite(frequencyQueue, &freqInfo);
                        }
                    }
                }
            }

            vTaskDelay(pdMS_TO_TICKS(ticksBetweenFreqDetection));
        }
    }
}