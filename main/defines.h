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
#if !defined(TUNER_GLOBAL_DEFINES)
#define TUNER_GLOBAL_DEFINES

typedef enum
{
    NOTE_C = 0,
    NOTE_C_SHARP,
    NOTE_D,
    NOTE_D_SHARP,
    NOTE_E,
    NOTE_F,
    NOTE_F_SHARP,
    NOTE_G,
    NOTE_G_SHARP,
    NOTE_A,
    NOTE_A_SHARP,
    NOTE_B,
    NOTE_NONE
} TunerNoteName;

inline const char* name_for_note(TunerNoteName note)
{
    switch (note)
    {
    case NOTE_C:
        return "C";
    case NOTE_C_SHARP:
        return "C#";
    case NOTE_D:
        return "D";
    case NOTE_D_SHARP:
        return "D#";
    case NOTE_E:
        return "E";
    case NOTE_F:
        return "F";
    case NOTE_F_SHARP:
        return "F#";
    case NOTE_G:
        return "G";
    case NOTE_G_SHARP:
        return "G#";
    case NOTE_A:
        return "A";
    case NOTE_A_SHARP:
        return "A#";
    case NOTE_B:
        return "B";
    case NOTE_NONE:
        return "-";
    default:
        return "Unknown";
    }
}

typedef struct
{
    float frequency;
    float cents;
    float targetFrequency;
    TunerNoteName targetNote;
    int targetOctave;
} FrequencyInfo;

typedef enum : uint8_t
{
    tunerBypassTypeTrue = 0,
    tunerBypassTypeBuffered,
} TunerBypassType;

// Define tuning modes
typedef enum
{
    MODE_AUTO = 0,
    MODE_GUITAR,
    MODE_UKULELE,
    MODE_VIOLIN,
    MODE_COUNT // Number of modes
} TunerMode;

//
// RTOS Queues
//

// The pitch detector task will always write the latest value of detection on
// the queue. Use a length of 1 so we can use xQueueOverwrite and xQueuePeek so
// the very latest frequency info is always available to anywhere.

#define FREQUENCY_QUEUE_LENGTH 1
#define FREQUENCY_QUEUE_ITEM_SIZE sizeof(FrequencyInfo)

//
// Pitch Detector Related
//

#define TUNER_FRAME_SIZE 1024
#define TUNER_SAMPLE_RATE (16 * 1000) // 16kHz

// If the difference between the minimum and maximum input values
// is less than this value, discard the reading and do not evaluate
// the frequency. This should help cut down on the noise from the
// OLED and only attempt to read frequency information when an
// actual input signal is being read.
// #define TUNER_READING_DIFF_MINIMUM      80
#define TUNER_READING_DIFF_MINIMUM 600

//
// Smoothing
//

// 1EU Filter
#define EU_FILTER_ESTIMATED_FREQ 110

#define EU_FILTER_MIN_CUTOFF 1.0
#define EU_FILTER_BETA ((float)0.05)
#define EU_FILTER_DERIVATIVE_CUTOFF 1.0

#define EU_FILTER_MIN_CUTOFF_2 0.5
#define EU_FILTER_BETA_2 ((float)0.05)
#define EU_FILTER_DERIVATIVE_CUTOFF_2 1.0

// Exponential Smoothing
#define EXP_SMOOTHING ((float)0.5)

#define A4_FREQ 440.0

#define HINT_ANIMATION_SPEED 20
#define HINT_ANIMATION_DELAY 1500

#define STRINGS_DISPLAY_TIME_MS 2000

uint8_t _get_max_strings(TunerMode mode);

#endif