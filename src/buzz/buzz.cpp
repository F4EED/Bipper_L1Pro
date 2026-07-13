#include "buzz.h"
#include "NodeDB.h"
#include "configuration.h"

#if !defined(ARCH_ESP32) && !defined(ARCH_RP2040) && !defined(ARCH_PORTDUINO)
#include "Tone.h"
#endif

#if defined(HAS_I2S)
#include "main.h"
#include <unordered_map>
#endif

#if !defined(ARCH_PORTDUINO)
extern "C" void delay(uint32_t dwMs);
#endif

struct ToneDuration {
    int frequency_khz;
    int duration_ms;
};

// Some common frequencies.
#define NOTE_SILENT 1
#define NOTE_C3 131
#define NOTE_CS3 139
#define NOTE_D3 147
#define NOTE_DS3 156
#define NOTE_E3 165
#define NOTE_F3 175
#define NOTE_FS3 185
#define NOTE_G3 196
#define NOTE_GS3 208
#define NOTE_A3 220
#define NOTE_AS3 233
#define NOTE_B3 247
#define NOTE_CS4 277
#define NOTE_B4 494
#define NOTE_F5 698
#define NOTE_G6 1568
#define NOTE_E7 2637

#define NOTE_C4 262
#define NOTE_E4 330
#define NOTE_G4 392
#define NOTE_A4 440
#define NOTE_C5 523
#define NOTE_E5 659
#define NOTE_G5 784

const int DURATION_1_16 = 62;  // 1/16 note
const int DURATION_1_8 = 125;  // 1/8 note
const int DURATION_1_4 = 250;  // 1/4 note
const int DURATION_1_2 = 500;  // 1/2 note
const int DURATION_3_4 = 750;  // 3/4 note
const int DURATION_1_1 = 1000; // 1/1 note

#ifdef HAS_I2S
void playTonesRTTTL(const ToneDuration *tone_durations, int size)
{
    // translate ToneDuration[] to RTTTL string and play using audioThread
    static std::unordered_map<int, std::string> freqToNote = {
        {NOTE_C3, "c4"},   {NOTE_CS3, "c#4"}, {NOTE_D3, "d4"},   {NOTE_DS3, "d#4"}, {NOTE_E3, "e4"},   {NOTE_F3, "f4"},
        {NOTE_FS3, "f#4"}, {NOTE_G3, "g4"},   {NOTE_GS3, "g#4"}, {NOTE_A3, "a4"},   {NOTE_AS3, "a#4"}, {NOTE_B3, "b4"},
        {NOTE_C4, "c5"},   {NOTE_E4, "e5"},   {NOTE_G4, "g5"},   {NOTE_A4, "a5"},   {NOTE_C5, "c6"},   {NOTE_E5, "e6"},
        {NOTE_G5, "g6"},   {NOTE_F5, "f6"},   {NOTE_G6, "g7"},   {NOTE_E7, "e8"}};

    char rtttl[128] = "tone:d=32,o=4,b=200:"; // default duration and octave
    for (int i = 0; i < size; i++) {
        const auto &td = tone_durations[i];
        std::string note = "b4";
        if (freqToNote.find(td.frequency_khz) != freqToNote.end()) {
            note = freqToNote[td.frequency_khz];
        }
        int dur = 32; // default duration
        if (td.duration_ms >= 1000)
            dur = 1;
        else if (td.duration_ms >= 500)
            dur = 2;
        else if (td.duration_ms >= 250)
            dur = 4;
        else if (td.duration_ms >= 125)
            dur = 8;
        else if (td.duration_ms >= 62)
            dur = 16;
        else
            dur = 32;

        char noteStr[64];
        snprintf(noteStr, sizeof(noteStr), "%s,%d", note.c_str(), dur);
        strncat(rtttl, noteStr, sizeof(rtttl) - strlen(rtttl) - 1);

        audioThread->beginRttl(rtttl, strlen(rtttl));
        while (audioThread->isPlaying()) {
            delay(10);
        }
        return;
    }
}
#endif

void playTones(const ToneDuration *tone_durations, int size)
{
    if (config.device.buzzer_mode == meshtastic_Config_DeviceConfig_BuzzerMode_DISABLED ||
        config.device.buzzer_mode == meshtastic_Config_DeviceConfig_BuzzerMode_NOTIFICATIONS_ONLY) {
        // Buzzer is disabled or not set to system tones
        return;
    }
#ifdef HAS_I2S
    if (moduleConfig.external_notification.use_i2s_as_buzzer && audioThread) {
        playTonesRTTTL(tone_durations, size);
        return;
    }
#endif
#if defined(PIN_BUZZER)
    if (!config.device.buzzer_gpio)
        config.device.buzzer_gpio = PIN_BUZZER;
#endif
    if (config.device.buzzer_gpio) {
        for (int i = 0; i < size; i++) {
            const auto &tone_duration = tone_durations[i];
            tone(config.device.buzzer_gpio, tone_duration.frequency_khz, tone_duration.duration_ms);
            // to distinguish the notes, set a minimum time between them.
            delay(1.3 * tone_duration.duration_ms);
        }
    }
}

void playBeep()
{
    ToneDuration melody[] = {{NOTE_B3, DURATION_1_16}};
    playTones(melody, sizeof(melody) / sizeof(ToneDuration));
}

void playLongBeep()
{
    ToneDuration melody[] = {{NOTE_B3, DURATION_1_1}};
    playTones(melody, sizeof(melody) / sizeof(ToneDuration));
}

void playGPSEnableBeep()
{
#if defined(R1_NEO) || defined(MUZI_BASE)
    ToneDuration melody[] = {
        {NOTE_F5, DURATION_1_2}, {NOTE_G6, DURATION_1_8}, {NOTE_E7, DURATION_1_4}, {NOTE_SILENT, DURATION_1_2}};
#else
    ToneDuration melody[] = {{NOTE_C3, DURATION_1_8}, {NOTE_FS3, DURATION_1_4}, {NOTE_CS4, DURATION_1_4}};
#endif
    playTones(melody, sizeof(melody) / sizeof(ToneDuration));
}

void playGPSDisableBeep()
{
#if defined(R1_NEO) || defined(MUZI_BASE)
    ToneDuration melody[] = {{NOTE_B4, DURATION_1_16}, {NOTE_B4, DURATION_1_16},   {NOTE_SILENT, DURATION_1_8},
                             {NOTE_F3, DURATION_1_16}, {NOTE_F3, DURATION_1_16},   {NOTE_SILENT, DURATION_1_8},
                             {NOTE_C3, DURATION_1_1},  {NOTE_SILENT, DURATION_1_1}};
#else
    ToneDuration melody[] = {{NOTE_CS4, DURATION_1_8}, {NOTE_FS3, DURATION_1_4}, {NOTE_C3, DURATION_1_4}};
#endif
    playTones(melody, sizeof(melody) / sizeof(ToneDuration));
}

void playStartMelody()
{
    ToneDuration melody[] = {{NOTE_FS3, DURATION_1_8}, {NOTE_AS3, DURATION_1_8}, {NOTE_CS4, DURATION_1_4}};
    playTones(melody, sizeof(melody) / sizeof(ToneDuration));
}

void playShutdownMelody()
{
    ToneDuration melody[] = {{NOTE_CS4, DURATION_1_8}, {NOTE_AS3, DURATION_1_8}, {NOTE_FS3, DURATION_1_4}};
    playTones(melody, sizeof(melody) / sizeof(ToneDuration));
}

void playChirp()
{
    // A short, friendly "chirp" sound for key presses
    ToneDuration melody[] = {{NOTE_AS3, 20}}; // Short AS3 note
    playTones(melody, sizeof(melody) / sizeof(ToneDuration));
}

void playClick()
{
    // A very short "click" sound with minimum delay; ideal for rotary encoder events
    ToneDuration melody[] = {{NOTE_AS3, 1}}; // Very Short AS3
    playTones(melody, sizeof(melody) / sizeof(ToneDuration));
}

void playBoop()
{
    // A short, friendly "boop" sound for button presses
    ToneDuration melody[] = {{NOTE_A3, 50}}; // Very short A3 note
    playTones(melody, sizeof(melody) / sizeof(ToneDuration));
}

void playLongPressLeadUp()
{
    // An ascending lead-up sequence for long press - builds anticipation
    ToneDuration melody[] = {
        {NOTE_C3, 100}, // Start low
        {NOTE_E3, 100}, // Step up
        {NOTE_G3, 100}, // Keep climbing
        {NOTE_B3, 150}  // Peak with longer note for emphasis
    };
    playTones(melody, sizeof(melody) / sizeof(ToneDuration));
}

// Static state for progressive lead-up notes
static int leadUpNoteIndex = 0;
static const ToneDuration leadUpNotes[] = {
    {NOTE_C3, 100}, // Start low
    {NOTE_E3, 100}, // Step up
    {NOTE_G3, 100}, // Keep climbing
    {NOTE_B3, 150}  // Peak with longer note for emphasis
};
static const int leadUpNotesCount = sizeof(leadUpNotes) / sizeof(ToneDuration);

bool playNextLeadUpNote()
{
    if (leadUpNoteIndex >= leadUpNotesCount) {
        return false; // All notes have been played
    }

    // Use playTones to handle buzzer logic consistently
    const auto &note = leadUpNotes[leadUpNoteIndex];
    playTones(&note, 1); // Play single note using existing playTones function

    leadUpNoteIndex++;

    if (leadUpNoteIndex >= leadUpNotesCount) {
        return false; // this was the final note
    }
    return true; // Note was played (playTones handles buzzer availability internally)
}

void resetLeadUpSequence()
{
    leadUpNoteIndex = 0;
}

void playComboTune()
{
    // Quick high-pitched notes with trills
    ToneDuration melody[] = {
        {NOTE_G3, 80},  // Quick chirp
        {NOTE_B3, 60},  // Higher chirp
        {NOTE_CS4, 80}, // Even higher
        {NOTE_G3, 60},  // Quick trill down
        {NOTE_CS4, 60}, // Quick trill up
        {NOTE_B3, 120}  // Ending chirp
    };
    playTones(melody, sizeof(melody) / sizeof(ToneDuration));
}

void play4ClickDown()
{
    ToneDuration melody[] = {{NOTE_G5, 55}, {NOTE_E5, 55}, {NOTE_C5, 60},  {NOTE_A4, 55},  {NOTE_G4, 55},
                             {NOTE_E4, 65}, {NOTE_C4, 80}, {NOTE_G3, 120}, {NOTE_E3, 160}, {NOTE_SILENT, 120}};
    playTones(melody, sizeof(melody) / sizeof(ToneDuration));
}

void play4ClickUp()
{
    // Quick high-pitched notes with trills
    ToneDuration melody[] = {{NOTE_F5, 50}, {NOTE_G6, 45}, {NOTE_E7, 60}};
    playTones(melody, sizeof(melody) / sizeof(ToneDuration));
}

#if defined(GAULIX_PAGER)

static bool gaulixCanBuzz()
{
    return config.device.buzzer_mode != meshtastic_Config_DeviceConfig_BuzzerMode_DISABLED &&
           config.device.buzzer_mode != meshtastic_Config_DeviceConfig_BuzzerMode_NOTIFICATIONS_ONLY;
}

static uint8_t gaulixBuzzerPin()
{
#if defined(PIN_BUZZER)
    if (!config.device.buzzer_gpio) {
        config.device.buzzer_gpio = PIN_BUZZER;
    }
#endif
    return static_cast<uint8_t>(config.device.buzzer_gpio);
}

#if defined(ARCH_NRF52)
#include "HardwarePWM.h"
#include "WVariant.h"

enum { GAULIX_TONE_TOKEN = 0x4761756c }; // 'Gaul'

static NRF_PWM_Type *const gaulixPwm = NRF_PWM2;
static HardwarePWM *const gaulixHwPwm = HwPWMx[2];

static uint64_t gaulixPulseCount(unsigned int frequency, unsigned long durationMs)
{
    if (durationMs == 0) {
        return 0;
    }
    if (durationMs < 1000ULL && durationMs * frequency < 1000ULL) {
        return 1ULL;
    }
    if (UINT64_MAX / frequency < durationMs) {
        return (durationMs / 1000ULL) * frequency;
    }
    return (static_cast<uint64_t>(durationMs) * frequency) / 1000ULL;
}

static bool gaulixPlayToneDuty(uint8_t pin, unsigned int frequency, unsigned long durationMs, uint8_t dutyPercent)
{
    if (pin >= PINS_COUNT || frequency < 20 || frequency > 25000) {
        return false;
    }

    uint8_t duty = dutyPercent;
    if (duty < 1) {
        duty = 1;
    } else if (duty > 80) {
        duty = 80;
    }

    noTone(pin);

    if (!gaulixHwPwm->isOwner(GAULIX_TONE_TOKEN) && !gaulixHwPwm->takeOwnership(GAULIX_TONE_TOKEN)) {
        tone(pin, frequency, durationMs);
        if (durationMs > 0) {
            delay(static_cast<uint32_t>(durationMs * 1.3f));
        }
        return true;
    }

    const uint64_t pulseCount = gaulixPulseCount(frequency, durationMs);
    const uint16_t timePeriod = static_cast<uint16_t>(125000U / frequency);
    uint16_t dutyWithPolarity = static_cast<uint16_t>(0x8000U | ((timePeriod * duty) / 100U));

    uint32_t seq0Refresh = 0;
    uint32_t seq1Refresh = 0;
    uint16_t loopCount = 1;
    nrf_pwm_task_t taskToStart = NRF_PWM_TASK_SEQSTART1;
    nrf_pwm_short_mask_t shorts = NRF_PWM_SHORT_LOOPSDONE_STOP_MASK;

    if (pulseCount == 0) {
        seq0Refresh = 0xFFFFFFU;
        seq1Refresh = 0xFFFFFFU;
        loopCount = 0xFFFFU;
        taskToStart = NRF_PWM_TASK_SEQSTART0;
        shorts = NRF_PWM_SHORT_LOOPSDONE_SEQSTART0_MASK;
    } else if (pulseCount == 1) {
        seq0Refresh = 0;
        seq1Refresh = 0;
        loopCount = 1;
        taskToStart = NRF_PWM_TASK_SEQSTART1;
    } else {
        unsigned int bitsNeeded = 0;
        for (uint64_t v = pulseCount; v; v >>= 1) {
            bitsNeeded++;
        }
        const unsigned int bitsForRefresh = bitsNeeded * 2 / 3;
        const uint32_t totalRefreshCount = 1U << bitsForRefresh;
        const uint32_t fullLoops = static_cast<uint32_t>((pulseCount - 1) / totalRefreshCount);
        const uint32_t extraPulses = static_cast<uint32_t>((pulseCount - 1) % totalRefreshCount);
        uint32_t seq0Count;

        if (extraPulses == 0) {
            seq0Count = totalRefreshCount / 2;
            taskToStart = NRF_PWM_TASK_SEQSTART1;
        } else {
            seq0Count = extraPulses;
            taskToStart = NRF_PWM_TASK_SEQSTART0;
        }
        loopCount = static_cast<uint16_t>(fullLoops + 1);
        seq0Refresh = seq0Count - 1;
        seq1Refresh = (totalRefreshCount - seq0Count) - 1;
    }

    if (gaulixPwm->ENABLE & PWM_ENABLE_ENABLE_Msk) {
        nrf_pwm_task_trigger(gaulixPwm, NRF_PWM_TASK_STOP);
        nrf_pwm_disable(gaulixPwm);
        gaulixPwm->PSEL.OUT[0] = NRF_PWM_PIN_NOT_CONNECTED;
    }

    uint32_t pins[NRF_PWM_CHANNEL_COUNT] = {g_ADigitalPinMap[pin], NRF_PWM_PIN_NOT_CONNECTED, NRF_PWM_PIN_NOT_CONNECTED,
                                                  NRF_PWM_PIN_NOT_CONNECTED};

    nrf_pwm_pins_set(gaulixPwm, pins);
    nrf_pwm_enable(gaulixPwm);
    nrf_pwm_configure(gaulixPwm, NRF_PWM_CLK_125kHz, NRF_PWM_MODE_UP, timePeriod);
    nrf_pwm_decoder_set(gaulixPwm, NRF_PWM_LOAD_COMMON, NRF_PWM_STEP_AUTO);
    nrf_pwm_shorts_set(gaulixPwm, shorts);
    nrf_pwm_int_set(gaulixPwm, 0);
    nrf_pwm_seq_ptr_set(gaulixPwm, 0, &dutyWithPolarity);
    nrf_pwm_seq_ptr_set(gaulixPwm, 1, &dutyWithPolarity);
    nrf_pwm_seq_cnt_set(gaulixPwm, 0, 1);
    nrf_pwm_seq_cnt_set(gaulixPwm, 1, 1);
    nrf_pwm_seq_refresh_set(gaulixPwm, 0, seq0Refresh);
    nrf_pwm_seq_refresh_set(gaulixPwm, 1, seq1Refresh);
    nrf_pwm_seq_end_delay_set(gaulixPwm, 0, 0);
    nrf_pwm_seq_end_delay_set(gaulixPwm, 1, 0);
    nrf_pwm_loop_set(gaulixPwm, loopCount);
    nrf_pwm_event_clear(gaulixPwm, NRF_PWM_EVENT_STOPPED);
    nrf_pwm_task_trigger(gaulixPwm, taskToStart);

    if (durationMs > 0) {
        delay(static_cast<uint32_t>(durationMs + (durationMs / 5U)));
    }

    if (gaulixPwm->ENABLE & PWM_ENABLE_ENABLE_Msk) {
        nrf_pwm_task_trigger(gaulixPwm, NRF_PWM_TASK_STOP);
        nrf_pwm_disable(gaulixPwm);
        gaulixPwm->PSEL.OUT[0] = NRF_PWM_PIN_NOT_CONNECTED;
    }
    gaulixHwPwm->releaseOwnership(GAULIX_TONE_TOKEN);
    return true;
}
#endif // ARCH_NRF52

static void gaulixPlayOneBeep(uint8_t pin, unsigned long durationMs)
{
#if defined(ARCH_NRF52)
    gaulixPlayToneDuty(pin, GAULIX_BUZZER_FREQ_HZ, durationMs, GAULIX_BUZZER_DUTY);
#else
    tone(pin, GAULIX_BUZZER_FREQ_HZ, durationMs);
    if (durationMs > 0) {
        delay(static_cast<uint32_t>(durationMs * 1.3f));
    }
#endif
}

void playGaulixPagerBeep()
{
    if (!gaulixCanBuzz()) {
        return;
    }

    const uint8_t pin = gaulixBuzzerPin();
    if (!pin) {
        return;
    }

    gaulixPlayOneBeep(pin, GAULIX_BUZZER_DURATION_MS);
    delay(GAULIX_BUZZER_PULSE_GAP_MS);
    gaulixPlayOneBeep(pin, GAULIX_BUZZER_DURATION_MS);
}

void playGaulixPagerFinBeep()
{
    if (!gaulixCanBuzz()) {
        return;
    }

    const uint8_t pin = gaulixBuzzerPin();
    if (!pin) {
        return;
    }

    gaulixPlayOneBeep(pin, GAULIX_BUZZER_DURATION_MS / 2);
}

#endif // GAULIX_PAGER