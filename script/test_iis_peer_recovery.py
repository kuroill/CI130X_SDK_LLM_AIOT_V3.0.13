#!/usr/bin/env python3
"""Host regression of actual CI C functions with mocked registers/FreeRTOS.

This does not build a CI image or prove slave-clock/DMA behavior on hardware.
Address fields are widened only in the host fixture to preserve pointer values.
"""

from pathlib import Path
import os
import re
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[1]
CODEC = (ROOT / "components/codec_manager/codec_manager.c").read_text()
IIS = (ROOT / "driver/ci130x_chip_driver/src/ci130x_iis.c").read_text()
UPLINK = (ROOT / "components/audio_pre_rslt_iis_out/ci130x_audio_pre_rslt_out.c").read_text()
APP = ROOT / "projects/offline_asr_llm_aiot_iis_sample/app/app_main"
UART = (APP / "ai_uart_i2s_protocol.c").read_text()


def function(source, name):
    match = re.search(r"(?m)^(?:static )?\w+ " + name + r"\([^;]*?\)\s*\{", source)
    assert match, name
    start = source.index("{", match.start())
    depth = 1
    cursor = start + 1
    # Ignore braces in C strings/comments while locating the function boundary.
    tokens = re.compile(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*.*?\*/|//[^\n]*|[{}]', re.S)
    for token in tokens.finditer(source, cursor):
        if token.group() == "{":
            depth += 1
        elif token.group() == "}":
            depth -= 1
        if depth == 0:
            return source[match.start():token.end()] + "\n"
    raise AssertionError(name)


def run_fixture(name, source):
    with tempfile.TemporaryDirectory(prefix="lenwell-ci-recovery-") as folder:
        c_path = Path(folder) / (name + ".c")
        executable = Path(folder) / name
        c_path.write_text(source)
        subprocess.run(
            [os.environ.get("CC", "cc"), "-std=gnu11", "-Wall", "-Wextra", "-Werror",
             "-Wno-unused-function", "-Wno-unused-parameter", "-Wno-unused-variable", "-Wno-sign-compare",
             str(c_path), "-o", str(executable)], check=True,
        )
        subprocess.run([str(executable)], check=True, timeout=15)


COMMON = r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <setjmp.h>
#define pdTRUE 1
#define pdFALSE 0
#define pdPASS 1
#define portMAX_DELAY 0xffffffffU
#define pdMS_TO_TICKS(x) (x)
#define portTICK_PERIOD_MS 1
#define mprintf(...) ((void)0)
#define ci_logdebug(...) ((void)0)
#define ci_logerr(...) ((void)0)
#define portYIELD_FROM_ISR(w) ((void)(w))
#define portENTER_CRITICAL() ((void)0)
#define portEXIT_CRITICAL() ((void)0)
#define taskENTER_CRITICAL() ((void)0)
#define taskEXIT_CRITICAL() ((void)0)
typedef int BaseType_t;
typedef unsigned UBaseType_t;
typedef uint32_t TickType_t;
static int in_isr;
typedef struct { int held, isr_safe; } Mutex;
typedef Mutex *SemaphoreHandle_t;
static BaseType_t xSemaphoreTake(Mutex *m, unsigned wait) {
    assert(!in_isr && wait==0);
    if(m->held) return 0;
    m->held = 1; return 1;
}
static void xSemaphoreGive(Mutex *m) { assert(!in_isr && m->held); m->held = 0; }
static BaseType_t xSemaphoreTakeFromISR(Mutex *m, BaseType_t *w) {
    assert(in_isr && m->isr_safe);
    if(m->held) return 0;
    m->held = 1; return 1;
}
static void xSemaphoreGiveFromISR(Mutex *m, BaseType_t *w) {
    assert(in_isr && m->isr_safe && m->held); m->held = 0;
}
'''


def codec_fixture():
    # Actual codec private state, IIS register layout and register control.
    types = CODEC[CODEC.index("typedef struct"):CODEC.index("extern unsigned char send_buf_addr")]
    registers = IIS[IIS.index("typedef volatile struct iis_register"):IIS.index("void iis_wait_load_ctrl")]
    source = COMMON + registers + r'''
#include <stddef.h>
_Static_assert(offsetof(iis_register_t,iis_tx_ctrl)==0x10,"TX register offset");
_Static_assert(offsetof(iis_register_t,iis_load_ctrl)==0x30,"LOAD register offset");
#define uint32_t uintptr_t
#define int32_t intptr_t
#define MAX_CODEC_NUM 3
#define ZERO_BUFFER_SIZE 1024
#define AUDIO_PLAYER_ENABLE 0
typedef uintptr_t iis_base_t;
typedef int IISDMAChax;
typedef int iis_tx_channal_t;
typedef int FunctionalState;
enum { IISDMACha0, IISDMACha1, IISDMACha2 };
enum { DISABLE, ENABLE, IIS_SLAVE, IIS_MASTER };
enum { IISxDMA_RX_EN = 0, IISxDMA_TX_EN = 1, IISDMA_TXAAD_Sel_ADDR0 = 0 };
typedef struct { uintptr_t data[32]; unsigned count; } Queue;
typedef Queue *QueueHandle_t;
static int xQueueSend(Queue *q, const void *p, unsigned wait) {
    if(q->count == 22) return 0;
    uintptr_t value; memcpy(&value, p, sizeof(value));
    for(unsigned i=0; i<q->count; ++i) assert(q->data[i] != value);
    q->data[q->count++] = value; return 1;
}
static int xQueueReceive(Queue *q, void *p, unsigned wait) {
    if(!q->count) return 0;
    memcpy(p, &q->data[0], sizeof(uintptr_t));
    --q->count; memmove(q->data, q->data+1, q->count*sizeof(uintptr_t)); return 1;
}
static int xQueueSendFromISR(Queue *q, const void *p, BaseType_t *w) { return xQueueSend(q,p,0); }
static int xQueueReceiveFromISR(Queue *q, void *p, BaseType_t *w) { return xQueueReceive(q,p,0); }
static unsigned uxQueueMessagesWaiting(Queue *q) { assert(!in_isr); return q->count; }
static iis_register_t regs;
#define IIS0 ((uintptr_t)&regs)
#define IIS1 1U
#define IIS2 2U
#define IISDMA0 0
typedef struct { iis_base_t IISx; int iis_mode_sel, tx_cha; } cm_iis_info_t;
typedef struct {
    cm_iis_info_t output_iis;
    struct { int (*codec_start)(int); int (*codec_stop)(int); } codec_if;
} cm_codec_hw_info_t;
typedef int cm_sound_info_t;
typedef struct { unsigned buffer_size, block_size, buffer_num; } cm_play_buffer_info_t;
typedef int cm_record_buffer_info_t;
static int dma_tx, dma_rx = 1, global_dma = 1, interrupt_enabled, hardware_calls, dma_arms;
static uintptr_t dma_slots[2];
static int check_curr_trap(void) { return in_isr; }
static void IISDMA_ChannelENConfig(int d, int c, int io, int on) {
    assert(c==0 && io==IISxDMA_TX_EN); dma_tx=on; ++hardware_calls;
    if(on) ++dma_arms;
}
static void IISDMA_ADDRRollBackINT(int d, int c, int io, int on) { assert(c==0 && io==IISxDMA_TX_EN); interrupt_enabled=on; }
static void IISDMA_ChannelIntENConfig(int d, int c, int io, int on) { assert(c==0 && io==IISxDMA_TX_EN && !on); }
static void IISxDMA_TXADDRRollbackInterruptClear(int d,int c,int a) { assert(c==0); }
static void IISDMA_RXTXClear(int d,int c,int io) { assert(c==0 && io==IISxDMA_TX_EN); }
static void IISDMA_INT_Clear(int d, unsigned bits) { assert(bits==((1U<<10)|(1U<<2))); }
static void IISxDMA_TADDR0(int d,int c,uintptr_t addr) { assert(c==0); dma_slots[0]=addr; }
static void IISxDMA_TADDR1(int d,int c,uintptr_t addr) { assert(c==0); dma_slots[1]=addr; }
static void IISDMA_EN(int d, int on) { assert(on==ENABLE); global_dma=on; }
static int IISDMA_TX_ADDR_Get(int d,int c) { return 0; }
#define CI_ASSERT(x, ...) assert(x)
'''
    source += types
    for name in ("iis_tx_try_enable",):
        source += function(IIS, name)
    for name in ("get_iis_dma_channel", "cm_start_output_dma", "cm_is_external_slave_output",
                 "cm_try_start_slave_output", "cm_cancel_slave_output", "cm_output_interrupt_handler"):
        source += function(CODEC, name)
    source += r'''
static Queue idle, busy;
static Mutex lock;
static void pool(void) {
    uintptr_t all[24]; unsigned n=0;
    for(unsigned i=0;i<idle.count;++i) all[n++]=idle.data[i];
    for(unsigned i=0;i<busy.count;++i) all[n++]=busy.data[i];
    uintptr_t current=cm.codec_info[0].play_buffer_queue.data_begin_addr;
    uintptr_t previous=cm.codec_info[0].play_buffer_queue.last_data_begin_addr;
    if(current && current!=(uintptr_t)cm.zero_buffer) all[n++]=current;
    if(previous) all[n++]=previous;
    assert(n==22);
    for(unsigned i=0;i<n;++i) {
        assert(all[i]>=0x10000 && all[i]<0x10000+22*1024);
        for(unsigned j=i+1;j<n;++j) assert(all[i]!=all[j]);
    }
    assert(!lock.held && dma_rx==1 && global_dma==1);
}
static void reset(void) {
    memset(&cm,0,sizeof(cm)); memset((void*)&regs,0,sizeof(regs));
    memset(&idle,0,sizeof(idle)); memset(&busy,0,sizeof(busy)); lock.held=0;
    cm.semaphore=&lock; cm.reg_flag[0]=1;
    codec_info_t *p=&cm.codec_info[0];
    p->codec_hw_info.output_iis=(cm_iis_info_t){IIS0,IIS_SLAVE,0};
    p->play_buffer_queue.idle_queue=&idle; p->play_buffer_queue.busy_queue=&busy;
    p->play_buffer_info=(cm_play_buffer_info_t){1024,1024,22};
    for(unsigned i=0;i<22;++i) { uintptr_t a=0x10000+i*1024; assert(xQueueSend(&idle,&a,0)); }
    dma_tx=0; hardware_calls=0; dma_arms=0; in_isr=0;
}
static void enqueue(unsigned count) {
    while(count--) { uintptr_t a; assert(xQueueReceive(&idle,&a,0)); assert(xQueueSend(&busy,&a,0)); }
}
int main(void) {
    reset(); enqueue(10);
    assert(cm_try_start_slave_output(0,11)==1);
    assert(!dma_tx && !regs.iis_load_ctrl.val && busy.count==10); pool();
    enqueue(1);
    assert(cm_try_start_slave_output(0,11)==1);
    /* Preserve the known-good 895785f ordering: DMA is armed while the first
     * TX enable load is pending, with the first PCM buffer already owned. */
    assert(dma_tx && dma_arms==1 && busy.count==10 && interrupt_enabled);
    assert(regs.iis_load_ctrl.val==(1U<<10)); pool();
    uintptr_t first=dma_slots[0], second=dma_slots[1];
    for(int i=0;i<10000;++i) assert(cm_try_start_slave_output(0,11)==1);
    assert(dma_arms==1 && busy.count==10 && !lock.held);
    assert(dma_slots[0]==first && dma_slots[1]==second); pool();
    BaseType_t w=0;
    for(int i=0;i<30;++i) { cm_output_interrupt_handler(0,&w); pool(); }
    assert(busy.count==0 && idle.count==22); /* IRQs work during STARTING */
    regs.iis_load_ctrl.val=0; /* clocks acknowledge enable before next task poll */
    assert(cm_try_start_slave_output(0,11)==0 && dma_tx && dma_arms==1); pool();
    assert(cm.codec_info[0].slave_output_state==CM_SLAVE_OUTPUT_RUNNING);
    for(int i=0;i<100;++i) assert(cm_try_start_slave_output(0,11)==0);
    assert(dma_arms==1); /* no re-prefill or repeated DMA start on underrun */
    assert(cm_cancel_slave_output(0)==0 && !dma_tx && !interrupt_enabled); pool();
    assert(idle.count==22 && busy.count==0);
    for(int i=0;i<10000;++i) assert(cm_cancel_slave_output(0)==0);
    cm_output_interrupt_handler(0,&w); pool(); /* stale IRQ cannot double free */
    assert(cm_try_start_slave_output(0,11)==1 && !dma_tx);
    regs.iis_load_ctrl.val=0; enqueue(11);
    assert(cm_try_start_slave_output(0,11)==1 && dma_tx); /* DMA before new enable */
    regs.iis_load_ctrl.val=0;
    assert(cm_try_start_slave_output(0,11)==0 && dma_tx); pool();
    for(int i=0;i<30;++i) { cm_output_interrupt_handler(0,&w); pool(); }
    assert(cm_cancel_slave_output(0)==0); pool(); /* zero buffer is never returned */

    reset(); enqueue(22); assert(cm_try_start_slave_output(0,11)==1 && dma_tx);
    for(int i=0;i<4;++i) { cm_output_interrupt_handler(0,&w); pool(); }
    assert(cm_cancel_slave_output(0)==0); pool(); /* cancel pending enable */
    cm_output_interrupt_handler(0,&w); pool(); /* stale IRQ after STARTING cancel */
    enqueue(11);
    assert(cm_try_start_slave_output(0,11)==1 && regs.iis_tx_ctrl[0].tx_en==1 && !dma_tx);
    regs.iis_load_ctrl.val=0;
    assert(cm_try_start_slave_output(0,11)==1 && regs.iis_tx_ctrl[0].tx_en==0 && !dma_tx);
    regs.iis_load_ctrl.val=0;
    assert(cm_try_start_slave_output(0,11)==1 && regs.iis_tx_ctrl[0].tx_en==1 && dma_tx);
    regs.iis_load_ctrl.val=0;
    assert(cm_try_start_slave_output(0,11)==0 && dma_tx); pool();

    reset(); enqueue(11); regs.iis_load_ctrl.val=1; /* pending RX load on shared IIS0 */
    assert(cm_try_start_slave_output(0,11)==1 && dma_tx && regs.iis_load_ctrl.val==1);
    assert(!regs.iis_tx_ctrl[0].tx_en); pool();
    regs.iis_load_ctrl.val=0;
    assert(cm_try_start_slave_output(0,11)==1 && dma_arms==1 && regs.iis_tx_ctrl[0].tx_en);
    regs.iis_load_ctrl.val=0;
    assert(cm_try_start_slave_output(0,11)==0 && dma_arms==1); pool();

    for(int i=0;i<2000;++i) {
        assert(cm_cancel_slave_output(0)==0); pool();
        enqueue(11); regs.iis_load_ctrl.val=0;
        assert(cm_try_start_slave_output(0,11)==1 && dma_tx);
        regs.iis_load_ctrl.val=0; assert(cm_try_start_slave_output(0,11)==0); pool();
        cm_output_interrupt_handler(0,&w); pool();
    }
    assert(cm_cancel_slave_output(0)==0); pool();
    uintptr_t borrowed; assert(xQueueReceive(&idle,&borrowed,0));
    assert(cm_cancel_slave_output(0)==-1 && !lock.held); /* ownership error is visible */
    assert(xQueueSend(&idle,&borrowed,0)); assert(cm_cancel_slave_output(0)==0); pool();
    lock.held=1; int calls=hardware_calls;
    assert(cm_cancel_slave_output(0)==1 && cm_try_start_slave_output(0,11)==1 && calls==hardware_calls);
    lock.held=0; in_isr=1;
    assert(cm_cancel_slave_output(0)==-1 && cm_try_start_slave_output(0,11)==-2);
    in_isr=0;
    cm.codec_info[0].codec_hw_info.output_iis.IISx=IIS1;
    assert(cm_cancel_slave_output(0)==-1 && cm_try_start_slave_output(0,11)==-1);
    assert(cm_cancel_slave_output(-1)==-1 && cm_try_start_slave_output(3,11)==-1);
    puts("codec: DMA-before-TX, prefill only once, STARTING IRQs/cancel, shared RX load, 2000 reconnects, pool conservation passed");
}
'''
    return source


def main():
    run_fixture("codec", codec_fixture())
    run_fixture("uplink", uplink_fixture())
    run_fixture("uart", uart_fixture())


def uplink_fixture():
    source = codec_fixture().split("int main(void) {")[0]
    source += r'''
#include <stdbool.h>
#define AI_UART_CONTROL_EN 1
#define USE_AUDIO_UPLOAD_BY_IIS 1
#define USE_IIS1_OUT_PRE_RSLT_AUDIO 1
#define AUDIO_CAP_POINT_NUM_PER_FRM 4
#define IIS_UPLOAD_IS_WAKEUP 0
typedef struct { int16_t *asr_src_data; } ci_wrapfft_audio;
static struct { unsigned block_size; } init_str;
static struct {
    struct { unsigned block_size; } init_str;
    bool codec_start_flag[MAX_CODEC_NUM];
    unsigned send_data_cnt, write_data_cnt;
} sg_init_tmp_str;
static struct { int upload_audio_by_denoise; } gCiasAiotFuncParam;
static Mutex producer_gate={.isr_safe=1};
static Mutex *uplink_gate=&producer_gate;
static Mutex *xSemaphoreCreateBinary(void) {
    assert(!in_isr); producer_gate.held=1; return &producer_gate;
}
static volatile uint8_t uplink_start_event_queued;
static int ready=1;
#define PLAY_PRE_AUDIO_CODEC_ID 0
#define CI_SS_AEC_WORK_STATE 0
static int aec_working;
static int ciss_get(int key) { return aec_working; }
static int ai_uart_i2s_peer_ready(void) { return ready; }
static void report_aec_path_levels(ci_wrapfft_audio *a, unsigned count) {}
static int cm_start_codec(int id,int io) { assert(0 && "slave must use nonblocking lifecycle"); return 0; }
static int cm_stop_codec(int id,int io) { assert(0 && "slave must use DMA cancellation"); return 0; }
#define CODEC_OUTPUT 1
typedef int io_direction_t;
typedef struct { int msg_type; } sys_msg_t;
enum { SYS_MSG_TYPE_AI_I2S_UPLINK_START=144 };
static int queued_start, queue_full, inline_delivery, posted_starts;
void audio_pre_rslt_handle_start(void);
static void audio_pre_rslt_out_codec_init(void);
static void ai_uart_i2s_on_audio_ready(void) { assert(!producer_gate.held); }
static int send_msg_to_sys_task(sys_msg_t *event, void *w) {
    assert(!in_isr && !producer_gate.held && event->msg_type==SYS_MSG_TYPE_AI_I2S_UPLINK_START);
    if(queue_full) return 0;
    ++posted_starts;
    if(inline_delivery) audio_pre_rslt_handle_start();
    else { assert(!queued_start); queued_start=1; }
    return 1;
}
'''
    for name in ("cm_get_pcm_buffer", "cm_write_codec", "cm_get_codec_busy_buffer_number"):
        source += function(CODEC, name)
    for name in ("uplink_try_take", "uplink_give", "codec_output_ctl", "audio_pre_rslt_request_start",
                 "audio_pre_rslt_handle_start", "audio_pre_rslt_upload_by_iis",
                 "audio_pre_rslt_stop", "audio_pre_rslt_start", "audio_pre_rslt_out_play_card_init"):
        source += function(UPLINK, name)
    source += r'''
static void produce(int16_t *left,int16_t *right,ci_wrapfft_audio *audio,int isr) {
    in_isr=isr;
    audio_pre_rslt_upload_by_iis(left,right,audio);
    assert(!producer_gate.held);
    in_isr=0;
}
static void deliver_start(void) {
    assert(queued_start && !in_isr);
    queued_start=0;
    audio_pre_rslt_handle_start();
}
static void poll_start(void) {
    audio_pre_rslt_request_start();
    if(queued_start) deliver_start();
}
static void audio_pre_rslt_out_codec_init(void) {
    assert(producer_gate.held);
    /* Publication is forbidden while codec initialization is incomplete. */
    in_isr=1;
    audio_pre_rslt_upload_by_iis(NULL,NULL,NULL);
    in_isr=0;
    audio_pre_rslt_request_start();
    assert(!queued_start && producer_gate.held);
}
int main(void) {
    static int16_t pcm[22][512];
    int16_t left[4]={11,22,33,44}, right[4]={1,2,3,4};
    ci_wrapfft_audio audio={left};
    reset();
    audio_pre_rslt_out_play_card_init();
    assert(!producer_gate.held && !queued_start);
    reset(); idle.count=0;
    for(int i=0;i<22;++i) { void *p=pcm[i]; assert(xQueueSend(&idle,&p,0)); }
    sg_init_tmp_str.init_str.block_size=16;
    /* The actual mailbox producer runs in ISR context. Task-only semaphore
     * APIs fail in this fixture instead of silently working as in old mocks. */
    produce(left,right,&audio,1);
    assert(pcm[0][0]==11 && pcm[0][1]==11 && pcm[0][6]==44 && pcm[0][7]==44);
    gCiasAiotFuncParam.upload_audio_by_denoise=1;
    produce(left,right,&audio,1);
    assert(pcm[1][0]==1 && pcm[1][1]==1 && pcm[1][6]==4 && pcm[1][7]==4);
    gCiasAiotFuncParam.upload_audio_by_denoise=0; aec_working=1;
    produce(left,right,&audio,1);
    assert(pcm[2][0]==1 && pcm[2][1]==1);
    aec_working=0;
    for(int i=0;i<27;++i) produce(left,right,&audio,1);
    assert(busy.count==22 && !dma_tx && !producer_gate.held && !lock.held);
    int calls=hardware_calls;
    audio_pre_rslt_request_start();
    assert(queued_start && uplink_start_event_queued && !dma_tx && calls==hardware_calls);
    for(int i=0;i<100;++i) audio_pre_rslt_request_start();
    assert(posted_starts==1); /* one coalesced event while system owner is busy */
    deliver_start();
    assert(dma_tx && !uplink_start_event_queued); /* DMA armed before TX load completes */
    BaseType_t w=0;
    for(int i=0;i<30;++i) cm_output_interrupt_handler(0,&w);
    assert(busy.count==0 && idle.count==22);
    regs.iis_load_ctrl.val=0;
    poll_start(); /* owner completes a pending start even after all prefill was consumed */
    assert(dma_tx && sg_init_tmp_str.codec_start_flag[0] && !producer_gate.held);
    calls=hardware_calls;
    for(int i=0;i<100;++i) poll_start();
    assert(calls==hardware_calls); /* no repeated DMA start */
    for(int i=0;i<3;++i) cm_output_interrupt_handler(0,&w);
    produce(left,right,&audio,1);
    ready=0; unsigned count=busy.count;
    produce(left,right,&audio,1); assert(busy.count==count);
    producer_gate.held=1;
    in_isr=1; audio_pre_rslt_upload_by_iis(left,right,&audio); in_isr=0;
    assert(producer_gate.held && busy.count==count); /* ISR never waits for the owner */
    assert(audio_pre_rslt_stop()==0); producer_gate.held=0;
    lock.held=1; assert(audio_pre_rslt_stop()==0 && !producer_gate.held); lock.held=0;
    assert(audio_pre_rslt_stop()==1 && !dma_tx && idle.count==22 && !busy.count);
    assert(!sg_init_tmp_str.codec_start_flag[0] && !producer_gate.held);
    ready=1; regs.iis_load_ctrl.val=0;
    for(int i=0;i<12;++i) produce(left,right,&audio,1);
    assert(!dma_tx && !sg_init_tmp_str.codec_start_flag[0]);
    poll_start();
    regs.iis_load_ctrl.val=0;
    poll_start();
    assert(dma_tx && sg_init_tmp_str.codec_start_flag[0]);
    /* Queued start work must not revive an old peer or canceled PCM. */
    assert(audio_pre_rslt_stop()==1);
    for(int i=0;i<12;++i) produce(left,right,&audio,0); /* direct task producer remains supported */
    audio_pre_rslt_request_start(); assert(queued_start);
    ready=0; assert(audio_pre_rslt_stop()==1); calls=hardware_calls;
    deliver_start(); assert(!dma_tx && calls==hardware_calls && idle.count==22);
    ready=1;
    for(int i=0;i<12;++i) produce(left,right,&audio,1);
    queue_full=1; audio_pre_rslt_request_start();
    assert(!uplink_start_event_queued && !queued_start);
    queue_full=0; inline_delivery=1;
    regs.iis_load_ctrl.val=0; poll_start();
    regs.iis_load_ctrl.val=0; poll_start();
    assert(dma_tx && !uplink_start_event_queued && sg_init_tmp_str.codec_start_flag[0]);
    inline_delivery=0;
    /* Repeated real producer/task-owner reconnects keep all 22 buffers. */
    for(int cycle=0;cycle<2000;++cycle) {
        ready=0; assert(audio_pre_rslt_stop()==1 && idle.count==22 && !busy.count);
        ready=1;
        for(int i=0;i<12;++i) produce(left,right,&audio,1);
        regs.iis_load_ctrl.val=0; poll_start();
        regs.iis_load_ctrl.val=0; poll_start();
        assert(dma_tx && sg_init_tmp_str.codec_start_flag[0]);
        in_isr=1;
        for(int i=0;i<5;++i) cm_output_interrupt_handler(0,&w);
        in_isr=0;
    }
    assert(audio_pre_rslt_stop()==1 && idle.count==22);
    assert(dma_rx==1 && global_dma==1);
    puts("uplink: real ISR/task producers, system-owned start, full prefill retry, queue pressure/preemption, stale work, 2000 reconnects passed");
}
'''
    return source


def uart_fixture():
    source = COMMON + r'''
#include <stdbool.h>
static TickType_t now;
static int iterations, max_iterations, ping_count, pong_count, stop_count, start_count;
static int audio_result=1, audio_stop_count, audio_start_count, last_ack, firmware_count;
static int start_poll_count;
static int queue_full, inline_delivery, queued;
static jmp_buf loop_done;
static TickType_t xTaskGetTickCount(void) { return now; }
static void vTaskDelay(unsigned delay) { now+=delay; if(++iterations>=max_iterations) longjmp(loop_done,1); }
typedef struct { uint8_t type, seq; uint16_t len; uint8_t payload[8]; } ai_uart_i2s_command_t;
typedef struct { int msg_type; } sys_msg_t;
enum { SYS_MSG_TYPE_AI_UART_TIMEOUT=143 };
#define AI_UART_CONTROL_UART 0
#define AI_UART_TASK_POLL_MS 50
#define CI_CONVERSATION_COMMAND_SET_STATE 0x29
#define CI_CONVERSATION_STATE_INACTIVE 0
#define CI_CONVERSATION_STATE_WAITING 2
typedef int UART_TypeDef;
'''
    for line in UART.splitlines():
        if re.match(r"#define AI_UART_(?:MSG_|STATE_|ACK_|HEARTBEAT_MS|PEER_TIMEOUT_MS|FIRMWARE_INFO_INTERVAL_MS)", line):
            source += line + "\n"
    state_start = UART.index("typedef enum\n{\n    AI_LINK_LOST")
    state_end = UART.index("static volatile uint8_t current_state", state_start)
    source += UART[state_start:state_end]
    source += r'''
static volatile uint8_t current_state=AI_UART_STATE_WAKEUP_WAIT;
static volatile uint8_t conversation_state;
static volatile TickType_t conversation_state_tick, last_peer_tick, last_firmware_info_tick;
static volatile uint8_t i2s_rx_ready=1, firmware_info_sent;
static volatile uint32_t dropped_commands;
static void send_frame(uint8_t type,const uint8_t *p,unsigned n) {
    if(type==AI_UART_MSG_PING) ++ping_count;
    if(type==AI_UART_MSG_PONG) ++pong_count;
}
static void send_firmware_info(void) { ++firmware_count; firmware_info_sent=1; last_firmware_info_tick=now; }
static void send_ack(uint8_t seq,uint8_t status) { last_ack=status; }
static void send_state(uint8_t state) { current_state=state; }
static void stop_downlink(void) { ++stop_count; }
static int start_downlink(void) { ++start_count; return 1; }
static int finish_downlink(uint32_t count) { return 1; }
static int audio_pre_rslt_stop(void) { ++audio_stop_count; return audio_result; }
static void audio_pre_rslt_start(void) { ++audio_start_count; }
static void audio_pre_rslt_request_start(void) { assert(!in_isr); ++start_poll_count; }
static void set_state_exit_wakeup(void) {}
static void vol_set_from_esp_percent(unsigned percent) {}
static int persist_ota_mcu_status(int status) { return 1; }
static void UartPollingSenddone(UART_TypeDef *p) {}
static void dpmu_software_reset_system_config(void) {}
static void report_aec_runtime(void) {}
void ai_uart_i2s_handle_timeout(void);
static int send_msg_to_sys_task(sys_msg_t *event, void *w) {
    assert(event->msg_type==SYS_MSG_TYPE_AI_UART_TIMEOUT);
    if(queue_full) return 0;
    if(inline_delivery) ai_uart_i2s_handle_timeout();
    else { assert(!queued); queued=1; }
    return 1;
}
'''
    for name in ("set_conversation_state", "mark_peer_rx", "progress_peer_loss", "ai_uart_i2s_handle_timeout",
                 "ai_uart_i2s_handle_command", "heartbeat_task", "ai_uart_i2s_peer_ready"):
        source += function(UART, name)
    source += r'''
static void heartbeat(unsigned polls) {
    int previous_polls=start_poll_count;
    iterations=0; max_iterations=polls;
    if(!setjmp(loop_done)) heartbeat_task(NULL);
    assert(start_poll_count==previous_polls+(int)polls);
}
static void deliver(void) { assert(queued); queued=0; ai_uart_i2s_handle_timeout(); }
int main(void) {
    ai_uart_i2s_command_t pong={.type=AI_UART_MSG_PONG};
    ai_uart_i2s_command_t ping={.type=AI_UART_MSG_PING,.len=3};
    ai_uart_i2s_command_t start={.type=AI_UART_MSG_START_DOWNLINK};
    now=100; ai_uart_i2s_handle_command(&pong); assert(ai_uart_i2s_peer_ready());
    now=4000; heartbeat(1); assert(queued && timeout_event_queued);
    ai_uart_i2s_handle_command(&pong); deliver();
    assert(link_state==AI_LINK_READY && !stop_count); /* stale timeout */
    now=8000; audio_result=0; heartbeat(1); deliver();
    assert(link_state==AI_LINK_STOPPING && !ai_uart_i2s_peer_ready());
    int stopped=stop_count, audio_stopped=audio_stop_count, pings=ping_count;
    heartbeat(100);
    assert(ping_count>=pings+4 && stop_count==stopped && audio_stop_count==audio_stopped);
    assert(queued && timeout_event_queued); /* one coalesced event; heartbeat stays alive */
    ai_uart_i2s_handle_command(&ping);
    assert(pong_count==1 && link_state==AI_LINK_STOPPING);
    ai_uart_i2s_handle_command(&start); assert(last_ack==AI_UART_ACK_FAILED && !start_count);
    audio_result=1; ai_uart_i2s_handle_command(&pong);
    assert(link_state==AI_LINK_READY && audio_start_count==2 && firmware_count==2);
    deliver(); assert(link_state==AI_LINK_READY); /* old queued event cannot stop new epoch */
    ai_uart_i2s_handle_command(&start); assert(last_ack==AI_UART_ACK_OK && start_count==1);
    now+=4000; queue_full=1; heartbeat(2);
    assert(!timeout_event_queued && !queued && link_state==AI_LINK_READY);
    queue_full=0; inline_delivery=1; heartbeat(1);
    assert(link_state==AI_LINK_LOST && !timeout_event_queued); /* consumer preempts enqueue */
    ai_uart_i2s_handle_command(&pong); assert(ai_uart_i2s_peer_ready());
    last_peer_tick=UINT32_MAX-1000; now=2000; heartbeat(1);
    assert(link_state==AI_LINK_LOST); /* tick wrap still expires after 3001 ms */
    i2s_rx_ready=0; ai_uart_i2s_handle_command(&pong); now+=4000;
    audio_stopped=audio_stop_count; heartbeat(1);
    assert(link_state==AI_LINK_LOST && audio_stopped==audio_stop_count);
    puts("UART: heartbeat independence, single cleanup owner, stale timeout, retry ACK, queue pressure/preemption, tick wrap passed");
}
'''
    return source


if __name__ == "__main__":
    main()
