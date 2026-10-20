#!/usr/bin/env python3
"""Exercise real CI PA/HPOUT and downlink code with host register/task mocks.

Checks switching order, gain/capture preservation, drain/cancel/failure cleanup,
and stale STOP isolation. Does not build CI firmware or prove acoustic silence.
"""

from pathlib import Path
import re

from test_iis_peer_recovery import function, run_fixture


ROOT = Path(__file__).resolve().parents[1]
APP = ROOT / "projects/offline_asr_llm_aiot_iis_sample/app/app_main"
PLAYER = (ROOT / "components/simple_audio_player/simple_audio_player.c").read_text()
BOARD = (ROOT / "driver/boards/CI-D06GT01D.c").read_text()
CODEC = (ROOT / "driver/ci130x_chip_driver/src/ci130x_codec.c").read_text()
UART = (APP / "ai_uart_i2s_protocol.c").read_text()
PROMPT = (ROOT / "components/cmd_info/prompt_player.c").read_text()
CONFIG = (APP / "user_config.h").read_text()


def define(source, name):
    match = re.search(r"(?m)^#define\s+" + name + r"\s+([0-9]+)U?\b", source)
    assert match, name
    return int(match[1])


def main():
    assert define(CONFIG, "PLAYER_CONTROL_PA") == 1
    assert define(CONFIG, "PLAYBACK_DAC_DIGITAL_GAIN_DB") == 10
    assert define(CONFIG, "VOLUME_OUTPUT_MAX_PERCENT") == 100
    assert re.search(r"#define\s+IF_JUST_CLOSE_HPOUT_WHILE_NO_PLAY\s+1", CONFIG)
    # The existing local-player delay moves into the shared hardware executor.
    local_start = function(PLAYER, "sap_start_play_inner")
    assert "audio_play_hw_pa_da_ctl(ENABLE, PLAYER_CONTROL_PA != 0)" in local_start
    assert "vTaskDelay(pdMS_TO_TICKS(100))" not in local_start
    assert "pa_start_flag" not in PLAYER + PROMPT

    trace_type = re.search(
        r"typedef struct\s*\{[^}]*\}\s*ai_downlink_pcm_trace_t;", UART
    )
    assert trace_type
    source = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static void mprintf(const char *format, ...) { (void)format; }
#define pdMS_TO_TICKS(x) (x)
#define portMAX_DELAY 0xffffffffU
#define pdTRUE 1
#define PLAYER_CONTROL_PA 1
#define SIMPLE_AUDIO_PLAYER_ENABLE 1
#define IF_JUST_CLOSE_HPOUT_WHILE_NO_PLAY 1
#define portTICK_PERIOD_MS 1
typedef unsigned TickType_t;
typedef enum { DISABLE, ENABLE } FunctionalState;
typedef enum { CODEC_INPUT, CODEC_OUTPUT } io_direction_t;
enum { PD, pin_0, PLAY_CODEC_ID, PLAY_PRE_AUDIO_CODEC_ID, SAP_STATE_IDLE, SAP_STATE_PLAYING,
       CI_SS_PLAY_STATE, CI_SS_PLAY_STATE_PLAYING,
       CI_SS_PLAY_STATE_PLAYING_TO_IDLE, AI_UART_STATE_DOWNLINK_PLAYING };
static struct { unsigned reg2b, reg2d, reg6, adc_gain, adc_enabled; } codec;
#define CODEC (&codec)
static int g_pa_pin_valid_level, gpio_level, pa_writes, settle_delays;
static int input_muted, play_state, local_state, stop_requested, local_stop_ok;
static int gain_applications, state_reports, codec_stops, downlink_mutex;
static int held, busy_buffers, busy_stuck, downlink_starting;
static TickType_t now;
static TickType_t hpout_opened_at, pa_disabled_at;
static uint8_t downlink_enabled, downlink_codec_started, i2s_rx_ready;
static uint32_t downlink_bytes, downlink_nonzero_samples;
static uint32_t downlink_generation, downlink_rx_overruns;
static uint8_t downlink_fault_event_queued;
static int32_t downlink_fault_detail;
static int discard_error;
static uint8_t downlink_play_buffer[2048];
typedef struct { struct { unsigned block_num, buffer_num, block_size,
    buffer_size; void *pcm_buffer; } play_buffer_info; } cm_pcm_buffer_info_t;
typedef struct { unsigned sample_rate, channel_flag, sample_depth; } cm_sound_info_t;
#define AUDIO_CAP_POINT_NUM_PER_FRM 256
#define IIS_DW_16BIT 16
static int pa_enabled(void) { return gpio_level == g_pa_pin_valid_level; }
static int hpout_muted(void) { return (CODEC->reg2b & 4U) == 0; }
static void gpio_set_level(int level) {
    /* HPOUT and its gain must be settled before the PA becomes audible. */
    if (level == g_pa_pin_valid_level) {
        assert(!hpout_muted() && gain_applications > 0);
        assert(now - hpout_opened_at >= AUDIO_PLAY_HPOUT_SETTLE_MS);
    } else {
        pa_disabled_at = now;
    }
    if (downlink_starting) assert(input_muted);
    gpio_level = level;
    ++pa_writes;
}
static void gpio_set_output_high_level(int port, int pin) { gpio_set_level(1); }
static void gpio_set_output_low_level(int port, int pin) { gpio_set_level(0); }
static uint8_t gpio_get_input_level(int port, int pin) { return gpio_level; }
static void inner_codec_dac_enable(bool first) { assert(!"AEC DAC power changed"); }
static void inner_codec_dac_disable(void) { assert(!"AEC DAC power changed"); }
static void vTaskDelay(TickType_t delay) {
    if (delay == AUDIO_PLAY_PA_SETTLE_MS) {
        assert(pa_enabled() && !hpout_muted());
        if (downlink_starting) {
            assert(input_muted && play_state == CI_SS_PLAY_STATE_PLAYING);
            assert(!downlink_enabled);
        }
        ++settle_delays;
    } else if (delay == AUDIO_PLAY_HPOUT_SETTLE_MS) {
        assert(!pa_enabled());
    }
    now += delay;
}
static TickType_t xTaskGetTickCount(void) { return now; }
static void xSemaphoreTake(int mutex, unsigned timeout) { assert(!held); held = 1; }
static void xSemaphoreGive(int mutex) { assert(held); held = 0; }
static int ciss_get(int key) { assert(key == CI_SS_PLAY_STATE); return play_state; }
static void ciss_set(int key, int value) { assert(key == CI_SS_PLAY_STATE); play_state = value; }
static int sap_get_state(void) { return local_state; }
static void sap_stop(void) { ++stop_requested; }
static uint8_t wait_audio_play_idle(unsigned timeout) {
    assert(timeout == AI_PLAY_STOP_WAIT_MS);
    if (!local_stop_ok) return 0;
    local_state = SAP_STATE_IDLE;
    return 1;
}
static void cm_set_codec_mute(int index, int dir, int channels, FunctionalState en) {
    assert(index == PLAY_CODEC_ID && dir == CODEC_OUTPUT && channels == 3);
    input_muted = en;
}
static void cm_config_pcm_buffer(int index, int dir, cm_pcm_buffer_info_t *info) {
    assert(held && index == PLAY_CODEC_ID && dir == CODEC_OUTPUT);
    assert(info->play_buffer_info.pcm_buffer == downlink_play_buffer);
}
static void cm_config_codec(int index, int dir, cm_sound_info_t *info) {
    assert(info->sample_rate == 16000 && info->channel_flag == 1);
    assert(info->sample_depth == IIS_DW_16BIT);
}
static void audio_play_apply_output_gain(void) {
    assert(!pa_enabled());
    if (downlink_starting) assert(input_muted);
    ++gain_applications;
}
static void send_state(int state) {
    assert(state == AI_UART_STATE_DOWNLINK_PLAYING);
    assert(!held && pa_enabled() && !hpout_muted() && !input_muted);
    assert(downlink_enabled && settle_delays > 0);
    ++state_reports;
}
static void cm_report_output_diagnostics(int index, const char *owner) {
    assert(index == PLAY_CODEC_ID && strcmp(owner, "downlink") == 0);
}
static int cm_start_codec(int index, io_direction_t dir);
static int cm_stop_codec(int index, io_direction_t dir);
static int cm_discard_codec_input(int index, uint32_t *overruns) {
    assert(held && index == PLAY_PRE_AUDIO_CODEC_ID);
    *overruns = 7; /* Idle overruns are the new epoch's baseline, not a fault. */
    return discard_error;
}
static int cm_get_codec_busy_buffer_number(int index, io_direction_t dir) {
    assert(pa_enabled()); /* Natural FINISH must not close the PA before drain. */
    int result = busy_buffers;
    if (busy_buffers > 0 && !busy_stuck) --busy_buffers;
    return result;
}
typedef struct { void (*play_done_callback)(unsigned); unsigned cmd_handle; } voice_play_info_t;
static struct { int play_queue, combination_number, combination_index; } prompt_player;
static int xQueueReceive(int queue, voice_play_info_t *info, unsigned wait) { return 0; }
static void resume_asr(void) { }
'''
    # All timing constants come from the production files.
    constants = ""
    for text, name in (
        (PLAYER, "AUDIO_PLAY_PA_SETTLE_MS"),
        (PLAYER, "AUDIO_PLAY_HPOUT_SETTLE_MS"),
        (UART, "AI_PLAY_STOP_WAIT_MS"),
        (UART, "AI_DOWNLINK_PCM_BUFFER_COUNT"),
        (UART, "AI_DOWNLINK_FINISH_TIMEOUT_MS"),
        (UART, "AI_DOWNLINK_FINISH_POLL_MS"),
    ):
        constants += f"#define {name} {define(text, name)}U\n"
    source = constants + source
    source += trace_type.group() + "\nstatic ai_downlink_pcm_trace_t downlink_pcm_trace;\n"
    fault_type = re.search(r"typedef enum\s*\{[^}]*\}\s*ai_downlink_fault_t;", UART)
    assert fault_type
    source += fault_type.group() + "\nstatic ai_downlink_fault_t downlink_fault;\n"
    source += function(UART, "report_downlink_fault")
    source += function(UART, "freeze_downlink_fault")
    pa_type = re.search(r"typedef enum\s*\{[^}]*\}\s*audio_pa_state_t;", PLAYER)
    assert pa_type
    source += pa_type.group() + "\nstatic audio_pa_state_t audio_pa_state;\n"
    # Observe real HPOUT bit changes; an accidental codec callback while PA is
    # enabled must fail this fixture, even if the high-level order looks right.
    source += function(CODEC, "inner_codec_hpout_is_mute").replace(
        "inner_codec_hpout_is_mute", "raw_hpout_mute", 1
    )
    source += r'''
static void inner_codec_hpout_is_mute(bool en) {
    if (en != hpout_muted()) {
        assert(!pa_enabled());
        if (en && pa_writes) assert(now - pa_disabled_at >= AUDIO_PLAY_HPOUT_SETTLE_MS);
        if (!en) hpout_opened_at = now;
    }
    raw_hpout_mute(en);
}
'''
    for text, name in (
        (CODEC, "icodec_start"),
        (CODEC, "icodec_stop"),
        (BOARD, "power_amplifier_on"),
        (BOARD, "power_amplifier_off"),
        (PLAYER, "audio_play_hw_pa_da_ctl"),
    ):
        source += function(text, name)
    callbacks = re.search(r"\.codec_if\s*=\s*(\{.*?\})", BOARD, re.S)
    assert callbacks
    initializer = re.sub(
        r"(?m)^\s*\.codec_(?:init|config|ioctl)\s*=[^\n]*\n", "", callbacks[1]
    )
    source += "static struct { int (*codec_start)(io_direction_t); int (*codec_stop)(io_direction_t); } board_callbacks = " + initializer + ";\n"
    source += r'''
static int cm_start_codec(int index, io_direction_t dir) {
    assert(held && input_muted);
    return board_callbacks.codec_start ? board_callbacks.codec_start(dir) : 0;
}
static int cm_stop_codec(int index, io_direction_t dir) {
    assert(held && input_muted);
    ++codec_stops;
    busy_buffers = 0;
    return board_callbacks.codec_stop ? board_callbacks.codec_stop(dir) : 0;
}
'''
    for text, name in (
        (PROMPT, "clean_play_queue"),
        (UART, "stop_downlink"),
        (UART, "finish_downlink"),
        (UART, "start_downlink"),
    ):
        source += function(text, name)
    source += r'''
static void reset(int active_level) {
    codec = (typeof(codec)){ .reg2d=31, .reg6=255, .adc_gain=19, .adc_enabled=1 };
    g_pa_pin_valid_level = active_level;
    gpio_level = !active_level;
    audio_pa_state = AUDIO_PA_STATE_UNKNOWN;
    pa_writes = settle_delays = codec_stops = gain_applications = state_reports = 0;
    input_muted = 1;
    local_state = SAP_STATE_IDLE;
    local_stop_ok = 1;
    stop_requested = held = busy_buffers = busy_stuck = downlink_starting = 0;
    downlink_enabled = downlink_codec_started = 0;
    downlink_generation = downlink_rx_overruns = downlink_fault_event_queued = 0;
    downlink_fault = AI_DOWNLINK_FAULT_NONE;
    downlink_fault_detail = discard_error = 0;
    i2s_rx_ready = 1;
    play_state = 0;
    now = 0;
    hpout_opened_at = pa_disabled_at = 0;
}
static void assert_gain_and_capture_unchanged(void) {
    assert(codec.reg2d == 31 && codec.reg6 == 255);
    assert(codec.adc_gain == 19 && codec.adc_enabled == 1);
}
static void begin_downlink(void) {
    downlink_starting = 1;
    assert(start_downlink());
    downlink_starting = 0;
    assert(downlink_codec_started && downlink_enabled);
    assert(pa_enabled() && !hpout_muted() && !input_muted);
    assert_gain_and_capture_unchanged();
}
static void test_polarity(int active_level) {
    reset(active_level);
    /* The legacy no-PA-control call still only changes HPOUT. */
    audio_play_hw_pa_da_ctl(ENABLE, false);
    assert(!hpout_muted() && !pa_enabled() && pa_writes == 0 && settle_delays == 0);
    audio_play_hw_pa_da_ctl(DISABLE, false);
    assert(hpout_muted() && pa_writes == 0);

    for (int cycle = 0; cycle < 200; ++cycle) {
        begin_downlink();
        int writes = pa_writes, delays = settle_delays;
        assert(start_downlink()); /* Duplicate START cannot re-toggle the PA. */
        assert(pa_writes == writes && settle_delays == delays);
        downlink_nonzero_samples = 42;
        busy_buffers = 3;
        assert(finish_downlink(42));
        assert(!pa_enabled() && hpout_muted() && input_muted);
        assert(!downlink_codec_started && !downlink_enabled);
        assert(play_state == CI_SS_PLAY_STATE_PLAYING_TO_IDLE);
        assert_gain_and_capture_unchanged();
    }
    begin_downlink();
    stop_downlink(); /* User interruption/peer loss uses the same cleanup. */
    assert(!pa_enabled() && hpout_muted());
    assert_gain_and_capture_unchanged();

    /* An old STOP must not cut the next wake ding or touch its codec. */
    local_state = SAP_STATE_PLAYING;
    audio_play_hw_pa_da_ctl(ENABLE, true);
    int writes = pa_writes, stops = codec_stops;
    int delays = settle_delays;
    /* The real board callbacks must not switch HPOUT between local segments. */
    if (board_callbacks.codec_stop) board_callbacks.codec_stop(CODEC_OUTPUT);
    audio_play_hw_pa_da_ctl(ENABLE, true);
    assert(pa_enabled() && !hpout_muted());
    assert(pa_writes == writes && settle_delays == delays);
    stop_downlink();
    assert(pa_enabled() && !hpout_muted());
    assert(pa_writes == writes && codec_stops == stops);
    prompt_player.combination_number = 1;
    prompt_player.combination_index = 1;
    clean_play_queue(); /* Real final local-prompt cleanup closes the PA. */
    assert(!pa_enabled() && hpout_muted() && audio_pa_state == AUDIO_PA_STATE_OFF);
    assert(prompt_player.combination_number == 0 && prompt_player.combination_index == 0);
    assert_gain_and_capture_unchanged();
}
static void test_failures(void) {
    reset(0);
    i2s_rx_ready = 0;
    assert(!start_downlink() && pa_writes == 0);
    i2s_rx_ready = 1;
    local_state = SAP_STATE_PLAYING;
    local_stop_ok = 0;
    assert(!start_downlink() && pa_writes == 0 && stop_requested == 1);
    local_stop_ok = 1;
    begin_downlink();
    assert(stop_requested == 2);
    assert(!finish_downlink(99)); /* Missing PCM stays an error, then closes PA. */
    assert(!pa_enabled() && hpout_muted() && !downlink_enabled);
    assert(downlink_fault == AI_DOWNLINK_FAULT_FINISH_PCM_TIMEOUT);
    assert(!finish_downlink(0)); /* Even a changed/late count cannot erase failure. */
    assert_gain_and_capture_unchanged();
    begin_downlink();
    downlink_nonzero_samples = 99;
    busy_buffers = 2;
    busy_stuck = 1;
    assert(!finish_downlink(99)); /* Drain timeout also closes through the owner. */
    assert(!pa_enabled() && hpout_muted() && !downlink_codec_started);
    assert(downlink_fault == AI_DOWNLINK_FAULT_FINISH_DRAIN_TIMEOUT);
    assert(!finish_downlink(99)); /* stop cleared busy queue, not the failure fact. */
    assert_gain_and_capture_unchanged();
    busy_stuck = 0;
    begin_downlink();
    downlink_nonzero_samples = 99;
    xSemaphoreTake(downlink_mutex, portMAX_DELAY);
    freeze_downlink_fault(AI_DOWNLINK_FAULT_DAC_QUEUE, -2);
    xSemaphoreGive(downlink_mutex);
    assert(!finish_downlink(99)); /* Expected count cannot hide an ingress fault. */
    assert(!pa_enabled() && downlink_fault == AI_DOWNLINK_FAULT_DAC_QUEUE);
    stop_downlink();
    assert(!finish_downlink(99)); /* Duplicate STOP/late FINISH retain failure. */
    begin_downlink();
    assert(downlink_fault == AI_DOWNLINK_FAULT_NONE && downlink_rx_overruns == 7);
    stop_downlink();
    discard_error = -1;
    assert(!start_downlink());
    assert(!pa_enabled() && hpout_muted() && !downlink_codec_started);
}
int main(void) {
    assert(!board_callbacks.codec_start && !board_callbacks.codec_stop);
    test_polarity(0);
    test_polarity(1);
    test_failures();
    puts("PA boundaries: both polarities, HPOUT transitions behind disabled PA, settled input/gain before PA on, real callback routing, 400 finish cycles, duplicate STOP isolation and timeout cleanup passed");
}
'''
    run_fixture("pa_idle_mute", source)


if __name__ == "__main__":
    main()
