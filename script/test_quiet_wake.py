#!/usr/bin/env python3
"""Run actual CI wake/lease C functions with host mocks, not a firmware build."""
from pathlib import Path

from test_iis_peer_recovery import function, run_fixture

ROOT = Path(__file__).resolve().parents[1]
APP = ROOT / "projects/offline_asr_llm_aiot_iis_sample/app/app_main"
UART = (APP / "ai_uart_i2s_protocol.c").read_text()
SYSTEM = (APP / "system_msg_deal.c").read_text()
HEADER = (ROOT / "components/ci_common/ci_conversation.generated.h").read_text()


def main():
    command = function(UART, "ai_uart_i2s_handle_command")
    policy_case = command[command.index("case CI_WAKE_PROMPT_COMMAND_SET_POLICY:"):
                          command.index("case AI_UART_MSG_ENTER_WAKEUP_WAIT:")]
    assert "1U != cmd->len" in policy_case
    assert "CI_WAKE_PROMPT_POLICY_QUIET_NIGHT" in policy_case
    assert "set_wake_prompt_policy(cmd->payload[0])" in policy_case
    assert "AI_UART_ACK_OK" in policy_case and "AI_UART_ACK_FAILED" in policy_case
    assert "set_wake_prompt_policy(CI_WAKE_PROMPT_POLICY_NORMAL)" in function(
        UART, "ai_uart_i2s_handle_timeout")
    wait_case = command[command.index("case AI_UART_MSG_ENTER_WAKEUP_WAIT:"):
                        command.index("case AI_UART_MSG_ENTER_OTA_MODE:")]
    assert "set_wake_prompt_policy" not in wait_case
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
static TickType_t tick, wake_prompt_policy_tick;
static uint8_t wake_prompt_policy, link_state;
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
static void play_enter_wakeup_done_cb(int h) {}
static void repeat_wakeup_ding_done_callback(int h) {}
static void prompt_play_by_voice_id(int id, void (*cb)(int), bool block) {
  assert(id==WAKEUP_DING_VOICE_ID); ++ding;
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
    for name in ("set_wake_prompt_policy", "ai_uart_i2s_wake_prompt_quiet", "ai_uart_i2s_on_wake"):
        source += function(UART, name)
    source += function(SYSTEM, "enter_wakeup_deal")
    source += r'''
int main(void) {
  link_state=AI_LINK_READY; tick=100;
  assert(!ai_uart_i2s_wake_prompt_quiet());
  set_wake_prompt_policy(CI_WAKE_PROMPT_POLICY_QUIET_NIGHT);
  assert(ai_uart_i2s_wake_prompt_quiet());
  tick+=4999; assert(ai_uart_i2s_wake_prompt_quiet());
  ++tick; assert(!ai_uart_i2s_wake_prompt_quiet());
  set_wake_prompt_policy(CI_WAKE_PROMPT_POLICY_QUIET_NIGHT);
  link_state=0; assert(!ai_uart_i2s_wake_prompt_quiet());
  link_state=AI_LINK_READY;
  tick=UINT32_MAX-100; set_wake_prompt_policy(CI_WAKE_PROMPT_POLICY_QUIET_NIGHT);
  tick=100; assert(ai_uart_i2s_wake_prompt_quiet()); /* tick rollover */
  state=0; active=0; local_player=1;
  enter_wakeup_deal(15000, 0x1234);
  assert(last_event==CI_WAKE_PROMPT_EVENT_QUIET_WAKE && reported_id==0x1234);
  assert(!ding && resumed==1 && hooks==1 && uplink==1 && stops==1 && !locked);
  assert(lowpower_exits==1 && vpr==1 && ir==1 && state==1);
  /* Repeated quiet wake preserves interruption and does not fabricate DING_DONE. */
  active=1; enter_wakeup_deal(15000, 0x1235);
  assert(frames==2 && !ding && resumed==2 && stops==2 && !locked);
  assert(last_event==CI_WAKE_PROMPT_EVENT_QUIET_WAKE);
  set_wake_prompt_policy(CI_WAKE_PROMPT_POLICY_NORMAL);
  active=0; state=0; enter_wakeup_deal(15000, 0x1236);
  assert(last_event==AI_UART_MSG_WAKE_DETECTED && ding==1 && frames==3);
  active=1; enter_wakeup_deal(15000, 0x1237);
  assert(last_event==AI_UART_MSG_WAKE_DETECTED && ding==1 && resumed==3);
  puts("CI quiet-wake lease, expiry, wraparound, wake hooks and event regressions passed");
}
'''
    run_fixture("quiet_wake", source)


if __name__ == "__main__":
    main()
