#!/usr/bin/env python3
"""Execute production owned-PCM/worker C functions with deterministic host mocks.

No firmware compilation, register access, hardware playback or DAC quality claim.
"""
from pathlib import Path
import re

from test_iis_peer_recovery import function, run_fixture

ROOT = Path(__file__).resolve().parents[1]
APP = ROOT / "projects/offline_asr_llm_aiot_iis_sample/app/app_main"
UART = (APP / "ai_uart_i2s_protocol.c").read_text()
CODEC = (ROOT / "components/codec_manager/codec_manager.c").read_text()


def ctype(source, name):
    match = re.search(r"typedef (?:struct|enum)\s*\{[^}]*\}\s*" + name + ";", source)
    assert match, name
    return match.group() + "\n"


def codec_fixture():
    source = r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
/* Only the codec host fixture widens SDK address fields; production is 32-bit. */
#define uint32_t uintptr_t
#define int32_t intptr_t
#define MAX_CODEC_NUM 2
#define pdTRUE 1
#define portENTER_CRITICAL() (++critical)
#define portEXIT_CRITICAL() (--critical)
typedef unsigned UBaseType_t;
typedef int BaseType_t, IISDMAChax;
typedef struct { void *items[16]; unsigned n, capacity; int fail_send; } Queue;
typedef Queue *QueueHandle_t;
typedef Queue *SemaphoreHandle_t;
static int critical, in_isr, writes;
static int check_curr_trap(void) { return in_isr; }
static int xQueueReceive(Queue *q, void **out, unsigned wait) {
    assert(wait == 0);
    if (!q->n) return 0;
    *out=q->items[0]; --q->n;
    memmove(q->items, q->items+1, q->n*sizeof(void *));
    return 1;
}
static int xQueueSend(Queue *q, void *const *p, unsigned wait) {
    assert(wait == 0);
    if (q->fail_send || q->n == q->capacity) return 0;
    q->items[q->n++]=*p; ++writes; return 1;
}
static int xSemaphoreTake(Queue *q, unsigned wait) {
    assert(wait == 0 && critical);
    if (!q->n) return 0;
    --q->n; return 1;
}
static void xSemaphoreGiveFromISR(Queue *q, BaseType_t *w) {
    assert(in_isr && !critical && q->n < q->capacity);
    ++q->n;
}
static unsigned uxQueueMessagesWaiting(Queue *q) { return q->n; }
typedef struct { void *pcm_buffer; uint32_t buffer_size, block_size, block_num; } cm_record_buffer_info_t;
typedef struct { uint32_t buffer_size; } cm_play_buffer_info_t;
'''
    source += ctype(CODEC, "play_buffer_queue_t") + ctype(CODEC, "record_buffer_queue_t")
    source += r'''
typedef struct {
    record_buffer_queue_t record_buffer_queue;
    cm_record_buffer_info_t record_buffer_info;
    play_buffer_queue_t play_buffer_queue;
    cm_play_buffer_info_t play_buffer_info;
} codec_info_t;
static struct { codec_info_t codec_info[2]; int reg_flag[2], dma_channel_to_codec_index[2]; } cm;
'''
    for name in ("cm_get_input_overruns", "cm_copy_codec_input", "cm_discard_codec_input",
                 "cm_try_write_codec_pcm", "cm_input_interrupt_handler"):
        source += function(CODEC, name)
    source += r'''
static Queue completed, idle, busy;
static int16_t ring[4][8], dac[2][4];
static void produce(void) {
    BaseType_t w=0; in_isr=1; cm_input_interrupt_handler(0,&w); in_isr=0;
}
static void setup(void) {
    memset(&cm,0,sizeof(cm));
    completed=(Queue){.capacity=4};
    idle=(Queue){.capacity=2}; busy=(Queue){.capacity=2};
    cm.reg_flag[0]=1;
    codec_info_t *i=&cm.codec_info[0];
    i->record_buffer_info=(cm_record_buffer_info_t){
        .pcm_buffer=ring,.buffer_size=sizeof(ring),.block_size=sizeof(ring[0]),.block_num=4};
    i->record_buffer_queue=(record_buffer_queue_t){
        .write_addr=(uintptr_t)ring,.read_addr=(uintptr_t)ring,.recorded_count_sem=&completed};
    i->play_buffer_queue=(play_buffer_queue_t){.idle_queue=&idle,.busy_queue=&busy};
    i->play_buffer_info.buffer_size=sizeof(dac[0]);
    idle.items[idle.n++]=dac[0]; idle.items[idle.n++]=dac[1];
    for(unsigned a=0;a<4;++a) for(unsigned b=0;b<8;++b) ring[a][b]=10*a+b+1;
}
int main(void) {
    setup();
    uint32_t bytes=0, overruns=0;
    int16_t copy[8], snapshot[8], mono[4]={1,-2,3,-4};
    assert(cm_copy_codec_input(0,copy,sizeof(copy),&bytes,&overruns)==0 && bytes==0);
    produce();
    assert(cm_copy_codec_input(0,copy,sizeof(copy),&bytes,&overruns)==1);
    assert(bytes==sizeof(copy) && overruns==0);
    memcpy(snapshot,copy,sizeof(copy));
    memset(ring[0],0xff,sizeof(ring[0])); /* DMA reuse cannot mutate the owned copy. */
    assert(!memcmp(copy,snapshot,sizeof(copy)));
    assert(cm_try_write_codec_pcm(0,mono,sizeof(mono))==1);
    assert(!memcmp(busy.items[0],mono,sizeof(mono)) && idle.n==1 && busy.n==1);
    assert(cm_try_write_codec_pcm(0,mono,sizeof(mono))==1);
    for(int j=0;j<100;++j) assert(cm_try_write_codec_pcm(0,mono,sizeof(mono))==0);
    assert(idle.n==0 && busy.n==2); /* Temporary full never borrows a phantom loan. */
    setup(); busy.fail_send=1;
    assert(cm_try_write_codec_pcm(0,mono,sizeof(mono))==-2 && idle.n==2 && !busy.n);
    idle.fail_send=1;
    assert(cm_try_write_codec_pcm(0,mono,sizeof(mono))==-3); /* Explicit irrecoverable return failure. */
    setup();
    for(int j=0;j<7;++j) produce();
    assert(cm_get_input_overruns(0,&overruns)==0 && overruns==4 && completed.n==3);
    assert(cm_discard_codec_input(0,&overruns)==0 && overruns==4 && completed.n==0);
    assert(cm.codec_info[0].record_buffer_queue.read_addr==cm.codec_info[0].record_buffer_queue.write_addr);
    assert(cm_copy_codec_input(0,copy,sizeof(copy),&bytes,&overruns)==0);
    produce();
    assert(cm_copy_codec_input(0,copy,sizeof(copy),&bytes,&overruns)==1 && overruns==4);
    assert(cm_get_input_overruns(-1,&overruns)==-1);
    assert(cm_copy_codec_input(2,copy,sizeof(copy),&bytes,&overruns)==-1);
    assert(cm_copy_codec_input(0,copy,1,&bytes,&overruns)==-1);
    assert(cm_copy_codec_input(0,NULL,sizeof(copy),&bytes,&overruns)==-1);
    assert(cm_try_write_codec_pcm(-1,mono,sizeof(mono))==-1);
    assert(cm_try_write_codec_pcm(0,mono,1)==-1);
    in_isr=1;
    assert(cm_copy_codec_input(0,copy,sizeof(copy),&bytes,&overruns)==-1);
    assert(cm_try_write_codec_pcm(0,mono,sizeof(mono))==-1);
    assert(critical==0);
    puts("owned PCM codec: stable copies, finite discard, ISR overruns, full queue, exact loan return and explicit return failure passed");
}
'''
    return source


def worker_fixture():
    source = r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#define pdMS_TO_TICKS(x) (x)
#define portMAX_DELAY 0xffffffffU
#define pdPASS 1
#define PLAYER_CONTROL_PA 1
#define AUDIO_CAP_POINT_NUM_PER_FRM 256
#define AI_DOWNLINK_DAC_WAIT_MS 100
#define AI_DOWNLINK_FINISH_TIMEOUT_MS 1500
#define AI_DOWNLINK_FINISH_POLL_MS 5
#define portTICK_PERIOD_MS 1
static void mprintf(const char *format, ...) { (void)format; }
typedef uint32_t TickType_t;
enum { PLAY_PRE_AUDIO_CODEC_ID, PLAY_CODEC_ID, CODEC_OUTPUT, ENABLE, DISABLE,
       CI_SS_PLAY_STATE, CI_SS_PLAY_STATE_PLAYING, CI_SS_PLAY_STATE_PLAYING_TO_IDLE,
       AI_UART_STATE_LISTENING, SYS_MSG_TYPE_AI_DOWNLINK_FAULT };
typedef struct { int msg_type; union { uint32_t ai_downlink_generation; } msg_data; } sys_msg_t;
static int downlink_mutex, held, owner_system, pa_on=1, codec_stops, muted;
static uint8_t downlink_enabled, downlink_codec_started, i2s_rx_ready=1, downlink_fault_event_queued;
static uint32_t downlink_generation, downlink_rx_overruns, downlink_bytes, downlink_nonzero_samples;
static int32_t downlink_fault_detail;
static TickType_t now;
static uint32_t rx_overruns;
static int copies, writes, available, write_result, queue_full, queue_posts, inline_delivery, discards;
static int overrun_on_copy, read_error, busy_buffers, fail_during_finish;
static uint32_t queued_generation;
static int16_t rx[512], last_pcm[256];
static int xSemaphoreTake(int m,unsigned wait) { assert(!held); held=1; return 1; }
static void xSemaphoreGive(int m) { assert(held); held=0; }
static TickType_t xTaskGetTickCount(void) { return now; }
static void vTaskDelay(TickType_t t);
static int cm_get_input_overruns(int index,uint32_t *n) {
    assert(held); *n=rx_overruns; return read_error;
}
static int cm_discard_codec_input(int index,uint32_t *n) {
    assert(held); available=0; *n=rx_overruns; ++discards; return read_error;
}
static int cm_copy_codec_input(int index,void *out,uint32_t capacity,uint32_t *size,uint32_t *overruns) {
    assert(held && capacity==sizeof(rx));
    if(!available) { *size=0; *overruns=rx_overruns; return 0; }
    --available; ++copies; memcpy(out,rx,sizeof(rx));
    *size=sizeof(rx);
    if(overrun_on_copy) ++rx_overruns;
    *overruns=rx_overruns;
    return 1;
}
static int cm_try_write_codec_pcm(int index,const void *pcm,uint32_t size) {
    assert(held && size==sizeof(last_pcm));
    if(write_result==1) { memcpy(last_pcm,pcm,size); ++writes; }
    return write_result;
}
static void cm_set_codec_mute(int a,int b,int c,int en) {
    assert(owner_system && held); muted=1;
}
static void audio_play_hw_pa_da_ctl(int en,bool unused) {
    assert(owner_system && held && muted && en==DISABLE); pa_on=0;
}
static void cm_stop_codec(int a,int b) {
    assert(owner_system && held && !pa_on && muted); ++codec_stops;
}
static void cm_report_output_diagnostics(int i,const char *s) { assert(owner_system && !held); }
static int ciss_get(int key) { return CI_SS_PLAY_STATE_PLAYING; }
static void ciss_set(int key,int value) { assert(owner_system); }
static void send_state(int state) { assert(owner_system && !held && !pa_on); }
static int cm_get_codec_busy_buffer_number(int a,int b) { return busy_buffers; }
/* Analog output gate model (simple_audio_player.c owns the real registers). */
static int gate_active, gate_percent, gate_steps, gate_releases;
static int32_t audio_play_output_target_percent(void) { return 100; }
static int32_t audio_play_output_gate_step(int32_t step) {
    assert(held && gate_active && pa_on);
    ++gate_steps;
    gate_percent += step;
    if (gate_percent >= 100) { gate_percent = 100; gate_active = 0; }
    return gate_percent;
}
static void audio_play_output_gate_release(void) {
    assert(owner_system && held && !pa_on); /* Restored only behind a closed PA. */
    if (gate_active) ++gate_releases;
    gate_active = 0;
}
void ai_uart_i2s_handle_downlink_fault(uint32_t generation);
static int send_msg_to_sys_task(sys_msg_t *event,void *w) {
    assert(!held && event->msg_type==SYS_MSG_TYPE_AI_DOWNLINK_FAULT);
    if(queue_full) return 0;
    ++queue_posts; queued_generation=event->msg_data.ai_downlink_generation;
    if(inline_delivery) {
        owner_system=1; ai_uart_i2s_handle_downlink_fault(queued_generation); owner_system=0;
    }
    return 1;
}
'''
    ramp = re.search(r"(?m)^#define\s+AI_OUTPUT_GATE_RAMP_FRAMES\s+([0-9]+)", UART)
    assert ramp
    source += f"#define AI_OUTPUT_GATE_RAMP_FRAMES {ramp[1]}\n"
    for name in ("ai_downlink_pcm_trace_t", "ai_downlink_fault_t", "ai_downlink_pending_t",
                 "ai_output_gate_state_t"):
        source += ctype(UART, name)
    source += r'''
static ai_downlink_pcm_trace_t downlink_pcm_trace;
static ai_downlink_fault_t downlink_fault;
static ai_downlink_pending_t downlink_pending;
static int16_t downlink_input_copy[512];
static ai_output_gate_state_t downlink_gate_state;
static TickType_t downlink_gate_closed_tick;
static uint32_t downlink_gate_silent_frames;
'''
    for name in ("report_downlink_fault", "stop_downlink", "freeze_downlink_fault",
                 "finish_downlink", "copy_downlink_pcm_with_trace",
                 "post_downlink_fault", "ai_uart_i2s_handle_downlink_fault", "pump_downlink_once"):
        source += function(UART, name)
    source += r'''
static void vTaskDelay(TickType_t t) {
    assert(!held); now+=t;
    if(fail_during_finish) {
        xSemaphoreTake(downlink_mutex,portMAX_DELAY);
        freeze_downlink_fault(AI_DOWNLINK_FAULT_DAC_TIMEOUT,100);
        xSemaphoreGive(downlink_mutex);
        fail_during_finish=0;
    }
}
static void epoch(void) {
    ++downlink_generation; downlink_enabled=downlink_codec_started=1;
    downlink_fault=AI_DOWNLINK_FAULT_NONE; downlink_fault_detail=0;
    downlink_fault_event_queued=0; downlink_rx_overruns=rx_overruns;
    downlink_bytes=downlink_nonzero_samples=0;
    downlink_pcm_trace=(ai_downlink_pcm_trace_t){.nonzero_hash=0x811c9dc5U};
    pa_on=1; muted=0; write_result=1; read_error=overrun_on_copy=0;
    available=0; copies=writes=queue_posts=queue_full=inline_delivery=0;
    now=0; busy_buffers=fail_during_finish=0;
    for(unsigned i=0;i<256;++i) rx[2*i]=rx[2*i+1]=(int16_t)(i+1);
}
static void deliver(uint32_t generation) {
    owner_system=1; ai_uart_i2s_handle_downlink_fault(generation); owner_system=0;
}
int main(void) {
    epoch(); available=1; write_result=0;
    assert(!pump_downlink_once() && copies==1 && !writes && !downlink_nonzero_samples);
    int16_t expected[256]; for(unsigned i=0;i<256;++i) expected[i]=rx[2*i];
    memset(rx,0xff,sizeof(rx)); /* Ring may be recycled while DAC is unavailable. */
    for(now=1;now<80;++now) {
        assert(!pump_downlink_once() && copies==1 && downlink_enabled);
        assert(!held); /* STOP is never excluded by the DAC wait. */
    }
    write_result=1;
    assert(pump_downlink_once() && writes==1 && copies==1);
    assert(!memcmp(last_pcm,expected,sizeof(expected)));
    assert(downlink_nonzero_samples==256 && downlink_bytes==512);
    epoch(); available=1; write_result=0; pump_downlink_once();
    uint32_t old=downlink_generation;
    owner_system=1; stop_downlink(); owner_system=0;
    assert(!pa_on && !held);
    epoch(); available=1; rx[0]=rx[1]=777;
    assert(pump_downlink_once() && last_pcm[0]==777); /* Old pending frame not reused. */
    int stops=codec_stops; deliver(old); assert(codec_stops==stops && pa_on);
    epoch(); available=1; write_result=0; pump_downlink_once(); now=100;
    assert(!pump_downlink_once() && downlink_fault==AI_DOWNLINK_FAULT_DAC_TIMEOUT);
    assert(!downlink_enabled && pa_on); /* Worker freezes, never touches PA/codec. */
    queue_full=1; post_downlink_fault(); assert(!downlink_fault_event_queued && !queue_posts);
    queue_full=0; post_downlink_fault(); post_downlink_fault();
    assert(queue_posts==1 && downlink_fault_event_queued);
    deliver(queued_generation); assert(!pa_on && downlink_fault==AI_DOWNLINK_FAULT_DAC_TIMEOUT);
    owner_system=1; assert(!finish_downlink(0)); stop_downlink(); assert(!finish_downlink(0)); owner_system=0;
    epoch(); available=1; write_result=-2;
    assert(!pump_downlink_once() && downlink_fault==AI_DOWNLINK_FAULT_DAC_QUEUE);
    assert(!writes && !downlink_nonzero_samples && downlink_pcm_trace.nonzero_hash==0x811c9dc5U);
    inline_delivery=1; post_downlink_fault(); assert(!pa_on && !downlink_fault_event_queued);
    epoch(); available=1; write_result=-3;
    assert(!pump_downlink_once() && downlink_fault_detail==-3);
    epoch(); available=1; write_result=0; pump_downlink_once(); ++rx_overruns;
    assert(!pump_downlink_once() && downlink_fault==AI_DOWNLINK_FAULT_RX_OVERRUN && copies==1);
    epoch(); available=1; overrun_on_copy=1;
    assert(!pump_downlink_once() && downlink_fault==AI_DOWNLINK_FAULT_RX_OVERRUN && !writes);
    epoch(); downlink_enabled=0; available=3; rx_overruns+=9;
    uint32_t baseline=downlink_rx_overruns;
    assert(!pump_downlink_once() && !available && discards>0);
    assert(downlink_rx_overruns==baseline && downlink_fault==AI_DOWNLINK_FAULT_NONE);
    epoch(); assert(!pump_downlink_once() && downlink_fault==AI_DOWNLINK_FAULT_NONE);
    read_error=-1; assert(!pump_downlink_once() && downlink_fault==AI_DOWNLINK_FAULT_RX);
    epoch(); fail_during_finish=1;
    owner_system=1; assert(!finish_downlink(99)); owner_system=0;
    assert(now==5 && !pa_on && downlink_fault==AI_DOWNLINK_FAULT_DAC_TIMEOUT);
    epoch(); available=1; write_result=0; now=UINT32_MAX-50;
    pump_downlink_once(); now=50;
    assert(!pump_downlink_once() && downlink_fault==AI_DOWNLINK_FAULT_DAC_TIMEOUT);
    /* Output gate: digital silence never opens HPOUT; the first real frame
     * ramps it over AI_OUTPUT_GATE_RAMP_FRAMES writes, then it stays open. */
    epoch(); memset(rx,0,sizeof(rx));
    downlink_gate_state=AI_OUTPUT_GATE_CLOSED; downlink_gate_silent_frames=0;
    gate_active=1; gate_percent=0; gate_steps=gate_releases=0;
    for(int frame=0; frame<5; ++frame) { available=1; assert(pump_downlink_once()); }
    assert(downlink_gate_state==AI_OUTPUT_GATE_CLOSED && gate_active && !gate_steps);
    assert(downlink_gate_silent_frames==5 && !downlink_nonzero_samples);
    rx[0]=rx[1]=3; available=1; assert(pump_downlink_once());
    assert(downlink_gate_state==AI_OUTPUT_GATE_RAMPING && gate_steps==1 && gate_percent<100);
    for(int frame=1; frame<AI_OUTPUT_GATE_RAMP_FRAMES; ++frame) { available=1; assert(pump_downlink_once()); }
    assert(downlink_gate_state==AI_OUTPUT_GATE_OPEN && !gate_active && gate_percent==100);
    assert(gate_steps==AI_OUTPUT_GATE_RAMP_FRAMES);
    memset(rx,0,sizeof(rx)); available=1; assert(pump_downlink_once());
    assert(downlink_gate_state==AI_OUTPUT_GATE_OPEN && gate_steps==AI_OUTPUT_GATE_RAMP_FRAMES);
    /* A stream stopped before any reply audio releases the gate behind the PA. */
    epoch(); memset(rx,0,sizeof(rx));
    downlink_gate_state=AI_OUTPUT_GATE_CLOSED; gate_active=1; gate_percent=0; gate_releases=0;
    available=1; assert(pump_downlink_once());
    owner_system=1; stop_downlink(); owner_system=0;
    assert(downlink_gate_state==AI_OUTPUT_GATE_OPEN && !gate_active && gate_releases==1);
    puts("owned PCM worker: retry integrity, STOP/START epoch isolation, system-only faults, queue retry/preemption, sticky FINISH failure, idle drain, overruns and tick wrap passed");
}
'''
    return source


def main():
    pump = function(UART, "pump_downlink_once")
    assert "cm_read_codec(" not in UART
    assert "cm_get_pcm_buffer(" not in UART
    assert "cm_write_codec(" not in UART
    assert "vTaskDelay" not in pump
    assert "stop_downlink(" not in pump
    assert "SYS_MSG_TYPE_AI_DOWNLINK_FAULT" in (APP / "user_msg_deal.c").read_text()
    assert "ai_uart_i2s_handle_downlink_fault" in (APP / "ai_uart_i2s_protocol.h").read_text()
    assert "[DOWNLINK_GUARD] version=1" in UART
    run_fixture("downlink_owned_codec", codec_fixture())
    run_fixture("downlink_owned_worker", worker_fixture())


if __name__ == "__main__":
    main()
