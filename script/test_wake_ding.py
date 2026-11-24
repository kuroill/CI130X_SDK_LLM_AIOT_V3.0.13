#!/usr/bin/env python3
"""Run actual CI wake C functions with host mocks, not a firmware build.

Both initial and repeated wakes report WAKE_DETECTED and schedule one prompt.
"""
import re
from pathlib import Path

from test_iis_peer_recovery import function, run_fixture

ROOT = Path(__file__).resolve().parents[1]
APP = ROOT / "projects/offline_asr_llm_aiot_iis_sample/app/app_main"
UART = (APP / "ai_uart_i2s_protocol.c").read_text()
SYSTEM = (APP / "system_msg_deal.c").read_text()
HEADER = (ROOT / "components/ci_common/ci_conversation.h").read_text()

def check_player_completion():
    player = (ROOT / "components/cmd_info/prompt_player.c").read_text()
    source = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#define SIMPLE_AUDIO_PLAYER_ENABLE 1
#define pdTRUE 1
#define portMAX_DELAY 0xffffffffU
typedef int BaseType_t;
typedef struct { int select_index; uintptr_t cmd_handle; void (*play_done_callback)(uintptr_t); } voice_play_info_t;
static struct { int semaphore, play_queue, combination_index, combination_number; } prompt_player;
static voice_play_info_t item;
static int queued, recovered, receipts;
static void resume_asr(void) { ++recovered; }
static void xSemaphoreTake(int s, unsigned delay) {}
static void xSemaphoreGive(int s) {}
static int xQueueReceive(int q, voice_play_info_t *v, int delay) {
    if (!queued) return 0;
    *v = item; queued = 0; return 1;
}
static int xQueuePeek(int q, voice_play_info_t *v, int delay) { return 0; }
static int prompt_play_inner(voice_play_info_t *v, bool nested) { return 0; }
static void prompt_play_next_combination(void) {}
static void clean_play_queue(void) {}
static struct { bool flash_prompt_eof; unsigned decode_frame_count, diagnostic_pcm_peak; } sap_info;
static void prompt_on_voice_id_played(uint16_t id) { assert(id == 1000); ++receipts; }
'''
    simple = (ROOT / "components/simple_audio_player/simple_audio_player.c").read_text()
    source += function(simple, "sap_flash_prompt_completed")
    source += function(player, "combination_callback")
    source += r'''
int main(void) {
    prompt_player.combination_number = prompt_player.combination_index = 1;
    sap_info.flash_prompt_eof = true; sap_info.decode_frame_count = 15; sap_info.diagnostic_pcm_peak = 1343;
    item.select_index = -2; item.cmd_handle = 1000;
    queued = 1; combination_callback(-1); assert(receipts == 0 && recovered == 1);
    queued = 1; combination_callback(0); assert(receipts == 1 && recovered == 2);
    combination_callback(0); assert(receipts == 1 && recovered == 2);
    item.select_index = -1; queued = 1; combination_callback(0);
    assert(receipts == 1 && recovered == 3);
    sap_info.flash_prompt_eof = false; item.select_index = -2; queued = 1; combination_callback(0);
    assert(receipts == 1 && recovered == 4);
    sap_info.flash_prompt_eof = true; sap_info.decode_frame_count = 2; queued = 1; combination_callback(0);
    assert(receipts == 1 && recovered == 5);
    sap_info.decode_frame_count = 15; sap_info.diagnostic_pcm_peak = 0; queued = 1; combination_callback(0);
    assert(receipts == 1 && recovered == 6);
}
'''
    run_fixture("ding_completion", source)


def check_wake_model_pairing():
    # Both groups in the project's command table select acoustic model 0.
    # Read embedded IDs so renaming an old .dat cannot mask a mismatch.
    firmware = APP.parents[1] / "firmware"

    def acoustic_version(model):
        match = re.search(rb"<AC-MODEL> (V[0-9]+)", model.read_bytes())
        assert match, f"{model.name}: missing embedded acoustic model ID"
        return match[1].decode("ascii")

    dnn, = (firmware / "dnn").glob("[[]0]*.fefixbin*")
    expected = acoustic_version(dnn)
    for group, role in ((1, "idle wake"), (0, "in-conversation commands")):
        lm, = (firmware / "asr").glob(f"[[]{group}]*.dat")
        actual = acoustic_version(lm)
        assert actual == expected, f"{role}: {lm.name} requires {actual}, packaged DNN is {expected}"


def main():
    for text in (UART, SYSTEM, HEADER, (APP / "ai_uart_i2s_protocol.h").read_text()):
        for removed in ("WAKE_PROMPT", "wake_prompt", "quiet_wake", "QUIET_WAKE", "muted_night"):
            assert removed not in text, removed
    source = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stddef.h>
#define taskENTER_CRITICAL() ((void)0)
#define taskEXIT_CRITICAL() ((void)0)
#define pdMS_TO_TICKS(x) (x)
#define portMAX_DELAY 0xffffffffU
#define mprintf(...) ((void)0)
#define AI_LINK_READY 1
#define AI_UART_MSG_WAKE_DETECTED 0x10
#define AI_UART_STATE_LISTENING 2
#define SAP_STATE_IDLE 0
#define AI_PLAY_STOP_WAIT_MS 250
#define SYS_STATE_UNWAKEUP 0
#define SYS_MSG_TYPE_CMD_INFO 1
#define MSG_CMD_INFO_STATUS_POST_CHANGE_ASR_NORMAL_WORD 2
#define USE_LOWPOWER_OSC_FREQUENCY 1
#define USE_LOWPOWER_DOWN_FREQUENCY 0
#define USE_VPR 1
#define USE_WMAN_VPR 0
#define USE_IR_ENABLE 1
#define pdFALSE 0
#define CONFIG_CI_LOG_UART 0
#define POWER_MODE_NORMAL 1
#define PLAY_ENTER_WAKEUP_EN 1
#define WMAN_PLAY_EN 0
#define WAKEUP_DING_VOICE_ID 1000
typedef unsigned TickType_t;
typedef int cmd_handle_t;
typedef int UART_TypeDef;
typedef struct {
  int msg_type;
  struct { struct { int cmd_info_status; } cmd_info_data; } msg_data;
} sys_msg_t;
static TickType_t tick;
static uint8_t link_state;
static int WakeupMutex, locked, state, stops, frames, last_event, uplink, ding;
static int local_player, hooks, resumed, lowpower_exits, active, vpr, ir;
static uint16_t reported_id;
static TickType_t xTaskGetTickCount(void) { return tick; }
static void xSemaphoreTake(int m, unsigned t) { assert(!locked); locked=1; }
static void xSemaphoreGive(int m) { assert(locked); locked=0; }

static bool ai_uart_i2s_conversation_active(void) { return active; }
static uint16_t cmd_info_get_command_id(int handle) { return (uint16_t)handle; }
static int get_wakeup_state(void) { return state; }
static void set_state_enter_wakeup(unsigned ms) { state=1; }
static void sys_weakup_hook(void) { ++hooks; }
static void pause_asr(int a, int b) {}
static void resume_asr(void) { ++lowpower_exits; }
static void UartPollingSenddone(UART_TypeDef *u) {}
static void power_mode_switch(int mode) { assert(mode==POWER_MODE_NORMAL); }
static int vpr_get_status(void) { return 2; }
static void vpr_run_one_recognition(void) { ++vpr; }
static void *DeviceGetLearnModeDevice(void) { return &ir; }
static void main_device_overLearnIR(int a, int b) { ++ir; }
static void send_msg_to_sys_task(sys_msg_t *msg, void *unused) {
  assert(!locked && state==1);
  assert(msg->msg_type==SYS_MSG_TYPE_CMD_INFO);
  assert(msg->msg_data.cmd_info_data.cmd_info_status==MSG_CMD_INFO_STATUS_POST_CHANGE_ASR_NORMAL_WORD);
  ++resumed;
}
static void (*scheduled)(int);
static void default_play_done_callback(int h) {}
static int completions;
static void ai_uart_i2s_on_ding_done(void) { ++completions; }
static void prompt_play_by_voice_id(int id, void (*cb)(int), bool block) {
  assert(id==WAKEUP_DING_VOICE_ID && block && frames==ding+1 && !local_player); scheduled=cb; ++ding;
}
static void stop_downlink(void) { ++stops; }
static int sap_get_state(void) { return local_player; }
static void sap_stop(void) { local_player=0; }
static bool wait_audio_play_idle(unsigned ms) { return !local_player; }
static void send_frame(uint8_t event, const uint8_t *payload, unsigned len) {
  assert(len==2); ++frames; last_event=event;
  reported_id=(uint16_t)payload[0] | ((uint16_t)payload[1]<<8);
}
static void send_state(uint8_t s) { assert(s==AI_UART_STATE_LISTENING); }
static void send_uplink_ready(void) { ++uplink; }
'''
    source += HEADER
    source += function(UART, "ai_uart_i2s_on_wake")
    source += function(SYSTEM, "play_enter_wakeup_done_cb")
    source += function(SYSTEM, "prompt_on_voice_id_played")
    source += function(SYSTEM, "enter_wakeup_deal")
    source += r'''
int main(void) {
  link_state=AI_LINK_READY; tick=100;
  /* First wake: reported, preempts local playback and plays its ding. */
  state=0; active=0; local_player=1;
  enter_wakeup_deal(15000, 0x1236);
  assert(last_event==AI_UART_MSG_WAKE_DETECTED && reported_id==0x1236);
  assert(ding==1 && frames==1 && stops==1 && uplink==1 && !locked);
  assert(scheduled==play_enter_wakeup_done_cb && completions==0);
  scheduled(0); assert(completions==0 && resumed==1);
  prompt_on_voice_id_played(WAKEUP_DING_VOICE_ID); assert(completions==1);
  /* Repeated wake preempts playback and schedules exactly one local ding. */
  active=1; local_player=1; enter_wakeup_deal(15000, 0x1237);
  assert(last_event==AI_UART_MSG_WAKE_DETECTED && reported_id==0x1237);
  assert(ding==2 && frames==2 && stops==2 && uplink==2 && !locked);
  assert(scheduled==play_enter_wakeup_done_cb && completions==1);
  scheduled(0); assert(completions==1 && resumed==2);
  prompt_on_voice_id_played(WAKEUP_DING_VOICE_ID); assert(completions==2);
  active=0; enter_wakeup_deal(15000, 0x1238);
  assert(ding==3 && scheduled==default_play_done_callback);
  scheduled(0); prompt_on_voice_id_played(7); assert(completions==2 && resumed==2);
  prompt_on_voice_id_played(WAKEUP_DING_VOICE_ID); assert(completions==3);
  puts("CI initial and repeated wake ding regressions passed");
}
'''
    run_fixture("wake_ding", source)
    check_player_completion()
    check_wake_model_pairing()


if __name__ == "__main__":
    main()
