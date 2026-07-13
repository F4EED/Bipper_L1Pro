#pragma once

void playBeep();
void playLongBeep();
void playStartMelody();
void playShutdownMelody();
void playGPSEnableBeep();
void playGPSDisableBeep();
void playComboTune();
void play4ClickDown();
void play4ClickUp();
void playBoop();
void playChirp();
void playClick();
void playLongPressLeadUp();
bool playNextLeadUpNote();  // Play the next note in the lead-up sequence
void resetLeadUpSequence(); // Reset the lead-up sequence to start from beginning

#if defined(GAULIX_PAGER)
// Buzzer tuning (override in variant.h if needed). Duty capped at 80 % for piezo safety.
#ifndef GAULIX_BUZZER_DUTY
#define GAULIX_BUZZER_DUTY 80
#endif
#ifndef GAULIX_BUZZER_FREQ_HZ
#define GAULIX_BUZZER_FREQ_HZ 2700
#endif
#ifndef GAULIX_BUZZER_HIGH_FREQ_HZ
#define GAULIX_BUZZER_HIGH_FREQ_HZ 3100
#endif
#ifndef GAULIX_BUZZER_LOW_FREQ_HZ
#define GAULIX_BUZZER_LOW_FREQ_HZ 2400
#endif
#ifndef GAULIX_BUZZER_DURATION_MS
#define GAULIX_BUZZER_DURATION_MS 220
#endif
#ifndef GAULIX_BUZZER_PULSE_GAP_MS
#define GAULIX_BUZZER_PULSE_GAP_MS 70
#endif
#ifndef GAULIX_CONTINUOUS_BEEP_INTERVAL_MS
#define GAULIX_CONTINUOUS_BEEP_INTERVAL_MS 1500
#endif
#ifndef GAULIX_LOW_BATTERY_DUTY
#define GAULIX_LOW_BATTERY_DUTY 45
#endif
#ifndef GAULIX_LOW_BATTERY_FREQ_HZ
#define GAULIX_LOW_BATTERY_FREQ_HZ 2400
#endif
#ifndef GAULIX_LOW_BATTERY_DURATION_MS
#define GAULIX_LOW_BATTERY_DURATION_MS 120
#endif
#ifndef GAULIX_LOW_BATTERY_CHECK_MS
#define GAULIX_LOW_BATTERY_CHECK_MS 30000
#endif
#ifndef GAULIX_LOW_BATTERY_REPEAT_MS
#define GAULIX_LOW_BATTERY_REPEAT_MS 300000
#endif

// Louder alert beep for Gaulix Bipper (PWM duty, resonance freq, double pulse).
void playGaulixPagerBeep();
void playGaulixPagerPimPom();
void playGaulixPagerFinBeep();
// Softer single beep when battery is at or below 10 % (not charging).
void playGaulixLowBatteryBeep();
#endif