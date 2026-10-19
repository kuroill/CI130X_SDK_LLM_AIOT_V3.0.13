#!/usr/bin/env python3
"""Exercise actual CI diagnostic C functions on the host, without a CI build.

Checks bounded ISR-to-task snapshots, all-frame clipping statistics, unmodified
PCM/gain and DMA silence classification. Does not verify acoustic audio quality.
"""

from pathlib import Path
import re

from test_iis_peer_recovery import function, run_fixture


ROOT = Path(__file__).resolve().parents[1]
UPLINK = (ROOT / "components/audio_pre_rslt_iis_out/ci130x_audio_pre_rslt_out.c").read_text()
CODEC = (ROOT / "components/codec_manager/codec_manager.c").read_text()
PLAYER = (ROOT / "components/simple_audio_player/simple_audio_player.c").read_text()
UART = (
    ROOT / "projects/offline_asr_llm_aiot_iis_sample/app/app_main/ai_uart_i2s_protocol.c"
).read_text()


COMMON = r'''
#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef unsigned UBaseType_t;
static int in_isr, output_lines;
static unsigned irq_mask;
static char output[16384];
static unsigned output_size;
static int check_curr_trap(void) { return in_isr; }
static unsigned mask_interrupts(void) { unsigned old=irq_mask; irq_mask=99; return old; }
static void restore_interrupts(unsigned old) { assert(irq_mask==99); irq_mask=old; }
#define portSET_INTERRUPT_MASK_FROM_ISR() mask_interrupts()
#define portCLEAR_INTERRUPT_MASK_FROM_ISR(old) restore_interrupts(old)
#define portENTER_CRITICAL() assert(!irq_mask)
#define portEXIT_CRITICAL() assert(!irq_mask)
static void mprintf(const char *format, ...) {
    assert(!in_isr && !irq_mask); /* Diagnostic producer/ISR must never print. */
    va_list args; va_start(args,format);
    int n=vsnprintf(output+output_size,sizeof(output)-output_size,format,args);
    va_end(args); assert(n>0 && n<(int)(sizeof(output)-output_size));
    output_size+=(unsigned)n; ++output_lines;
}
static void clear_output(void) { output_size=0; output_lines=0; output[0]=0; }
'''


def aec_fixture():
    source = COMMON + r'''
enum { CI_SS_PLAY_STATE, CI_SS_AEC_WORK_STATE, CI_SS_PLAY_STATE_PLAYING=7 };
static int playing, aec_working;
static unsigned ciss_get(int key) { return key==CI_SS_PLAY_STATE ? playing : aec_working; }
static struct {
    uint32_t pga_gain_reg[2], adc_dig_gain_reg[2];
    struct { uint32_t reg49; } alc_reg[2];
    uint32_t rega, reg2d, reg6;
} codec;
#define CODEC (&codec)
typedef struct { int16_t *mic[1], *ref[1], *dst[1]; } ci_wrapfft_audio;
'''
    start = UPLINK.index("#define AEC_PATH_REPORT_CAPACITY")
    end = UPLINK.index("static void queue_aec_path_report", start)
    source += UPLINK[start:end]
    for name in (
        "queue_aec_path_report",
        "report_aec_path_levels",
        "audio_pre_rslt_report_diagnostics",
    ):
        source += function(UPLINK, name)
    source += r'''
static int16_t mic[4]={32767,-32768,100,-100};
static int16_t ref[4]={32767,32767,32767,32767};
static int16_t dst[4]={1,-1,0,0};
static int16_t zeros[4];
static ci_wrapfft_audio audio={{mic},{ref},{dst}};
static ci_wrapfft_audio quiet={{zeros},{zeros},{zeros}};
static void collect(ci_wrapfft_audio *a) {
    in_isr=1; irq_mask=7;
    report_aec_path_levels(a,4);
    assert(irq_mask==7); irq_mask=0; in_isr=0;
}
static void end_playback(void) { playing=0; collect(NULL); }
static void drain(void) {
    while(aec_path_report_count) audio_pre_rslt_report_diagnostics();
}
int main(void) {
    codec.pga_gain_reg[0]=0x71; codec.pga_gain_reg[1]=0x6c;
    codec.adc_dig_gain_reg[0]=199; codec.adc_dig_gain_reg[1]=197;
    codec.reg2d=31; codec.reg6=255;
    playing=CI_SS_PLAY_STATE_PLAYING; aec_working=1;
    ci_wrapfft_audio missing={0};
    collect(NULL); collect(&missing);
    assert(!aec_path_total_frames && !aec_path_report_count);
    int16_t saved_mic[4],saved_ref[4],saved_dst[4];
    memcpy(saved_mic,mic,sizeof mic); memcpy(saved_ref,ref,sizeof ref); memcpy(saved_dst,dst,sizeof dst);
    collect(&audio); /* A transient in the first frame must not be missed. */
    for(int i=1;i<8;++i) collect(&quiet);
    assert(!output_lines && aec_path_report_count==1);
    aec_path_report_t *r=&aec_path_reports[aec_path_report_head];
    assert(r->playback==1 && r->phase==0 && r->total_frames==8);
    assert(r->window.frames==8 && r->window.samples==32 && r->window.aec_frames==8);
    assert(r->window.peak[0]==32768 && r->window.clipped[0]==2);
    assert(r->window.peak[1]==32767 && r->window.clipped[1]==4);
    assert(r->window.sum[1]==131068 && r->window.abs_sum[1]==131068);
    assert(r->window.peak[2]==1 && !r->window.clipped[2]);
    assert(!memcmp(saved_mic,mic,sizeof mic) && !memcmp(saved_ref,ref,sizeof ref));
    assert(!memcmp(saved_dst,dst,sizeof dst));
    /* Reporting uses registers captured with this window, not later live gain. */
    codec.pga_gain_reg[1]=0x7f;
    in_isr=1; audio_pre_rslt_report_diagnostics(); in_isr=0;
    assert(aec_path_report_count==1 && !output_lines);
    audio_pre_rslt_report_diagnostics();
    assert(output_lines==2 && !aec_path_report_count);
    assert(strstr(output,"micClipPermille=62 refClipPermille=125"));
    assert(strstr(output,"refPgaReg=0x6c") && !strstr(output,"refPgaReg=0x7f"));
    clear_output();
    for(int i=0;i<64;++i) collect(&quiet);
    r=&aec_path_reports[aec_path_report_head];
    assert(r->phase==1 && r->total_frames==72 && r->window.frames==64);
    drain(); clear_output();
    for(int i=0;i<3;++i) collect(&audio);
    end_playback();
    r=&aec_path_reports[aec_path_report_head];
    assert(r->phase==2 && r->total_frames==75 && r->window.samples==12);
    drain(); clear_output();
    /* A sub-128ms local ding still publishes its complete end window. */
    playing=CI_SS_PLAY_STATE_PLAYING; aec_working=0;
    collect(&audio); collect(&audio); end_playback();
    r=&aec_path_reports[aec_path_report_head];
    assert(r->playback==2 && r->phase==2 && r->window.frames==2);
    assert(!r->window.aec_frames && r->window.clipped[1]==8);
    drain(); clear_output();
    /* A delayed heartbeat cannot block audio or grow a queue without bound. */
    for(int i=0;i<5;++i) {
        playing=CI_SS_PLAY_STATE_PLAYING; collect(&audio); end_playback();
    }
    assert(aec_path_report_count==AEC_PATH_REPORT_CAPACITY && aec_path_dropped_reports==1);
    assert(!output_lines && irq_mask==0);
    drain(); assert(strstr(output,"droppedReports=1"));
    assert(codec.reg2d==31 && codec.reg6==255 && codec.pga_gain_reg[1]==0x7f);
    assert(!memcmp(saved_mic,mic,sizeof mic) && !memcmp(saved_dst,dst,sizeof dst));
    /* Capture original PA feedback without writing samples or printing in ISR. */
    clear_output(); playing=CI_SS_PLAY_STATE_PLAYING;
    for(int i=0;i<8;++i) collect(&quiet);
    int16_t snapshot_mic[64]={0},snapshot_ref[66],snapshot_dst[64]={0};
    for(int i=0;i<66;++i) snapshot_ref[i]=0x1234;
    ci_wrapfft_audio snapshot={{snapshot_mic},{snapshot_ref+1},{snapshot_dst}};
    in_isr=1; report_aec_path_levels(&snapshot,64); in_isr=0;
    assert(aec_ref_samples==64 && aec_ref_frame==9 && !output_lines);
    end_playback(); drain();
    assert(strstr(output,"[REF_PCM] playback="));
    assert(strstr(output,"selectedFrame=9 frameAbs=298240 samples=64"));
    assert(strstr(output,"encoding=s16le hex=341234123412"));
    for(int i=0;i<66;++i) assert(snapshot_ref[i]==0x1234);
    /* Exact window completion still delivers REF once, without zero-sample divisions. */
    clear_output(); playing=CI_SS_PLAY_STATE_PLAYING;
    for(int i=0;i<8;++i) collect(&quiet);
    in_isr=1;
    for(int i=0;i<64;++i) report_aec_path_levels(&snapshot,64);
    in_isr=0;
    end_playback(); drain();
    assert(strstr(output,"[REF_PCM] playback="));
    assert(!strstr(output,"phase=end frames=0"));
    puts("AEC diagnostics: complete windows, transient/short ding, clip/DC counts, original bounded REF waveform, immutable PCM/register snapshots and task-only printing passed");
}
'''
    return source


def dma_fixture():
    trace = re.search(r"typedef struct\s*\{[^}]*\}cm_output_diagnostics_t;", CODEC)
    assert trace
    source = COMMON + trace.group() + r'''
enum { MAX_CODEC_NUM=2 };
typedef struct {
    volatile cm_output_diagnostics_t output_diagnostics;
    struct { unsigned channel_flag,sample_rate; } play_sound_info;
    struct { unsigned block_size; } play_buffer_info;
} codec_info_t;
static struct { codec_info_t codec_info[2]; unsigned char zero_buffer[1024]; char reg_flag[2]; } cm;
'''
    for name in ("cm_trace_output_block", "cm_report_output_diagnostics"):
        # Widen only addresses in this host fixture; CI uses 32-bit addresses.
        source += function(CODEC, name).replace(
            "uint32_t address", "uintptr_t address"
        ).replace("(uint32_t)cm.zero_buffer", "(uintptr_t)cm.zero_buffer")
    source += r'''
int main(void) {
    cm.reg_flag[0]=1;
    codec_info_t *info=&cm.codec_info[0];
    info->play_sound_info.channel_flag=1; info->play_sound_info.sample_rate=16000;
    info->play_buffer_info.block_size=512;
    uintptr_t zero=(uintptr_t)cm.zero_buffer;
    in_isr=1;
    cm_trace_output_block(info,zero); cm_trace_output_block(info,zero);
    cm_trace_output_block(info,0x1000); cm_trace_output_block(info,0x1200);
    cm_trace_output_block(info,zero); cm_trace_output_block(info,zero);
    cm_trace_output_block(info,0x1400); /* An internal zero gap is now proven. */
    cm_trace_output_block(info,zero); /* A final pending tail is not an internal gap. */
    cm_output_diagnostics_t t=info->output_diagnostics;
    assert(t.pcm_blocks==3 && t.zero_blocks==5 && t.startup_zero_blocks==2);
    assert(t.resumed_gaps==1 && t.resumed_zero_blocks==2 && t.max_resumed_zero_blocks==2);
    assert(t.pending_zero_blocks==1 && !output_lines);
    cm_report_output_diagnostics(0,"downlink"); assert(!output_lines);
    in_isr=0; cm_report_output_diagnostics(0,"downlink");
    assert(strstr(output,"blockUs=16000") && strstr(output,"resumedGaps=1"));
    assert(strstr(output,"startupZeroBlocks=2") && strstr(output,"tailZeroBlocks=1"));
    assert(!memcmp(&t,(const void*)&info->output_diagnostics,sizeof t));
    cm_report_output_diagnostics(-1,"invalid"); cm_report_output_diagnostics(2,"invalid");
    cm_report_output_diagnostics(1,"unregistered"); assert(output_lines==1);
    clear_output(); info->play_sound_info.channel_flag=3;
    cm_report_output_diagnostics(0,"local"); assert(strstr(output,"blockUs=8000"));
    puts("DMA diagnostics: scheduled blocks, startup/internal/tail silence separation, block timing, immutable snapshot and invalid/ISR calls passed");
}
'''
    return source


def local_pcm_fixture():
    denominator = re.search(r"^#define ESP_VOLUME_TO_LOCAL_PROMPT_DENOMINATOR .+$", PLAYER, re.M)
    assert denominator
    source = COMMON + denominator.group() + r'''

static uint16_t g_audio_play_pcm_gain_percent=100;
static struct {
    unsigned gain_ratio_percent;
    uint32_t diagnostic_pcm_samples,diagnostic_pcm_peak,diagnostic_pcm_clipped;
} sap_info;
'''
    source += function(PLAYER, "audio_play_apply_local_pcm_gain")
    source += r'''
int main(void) {
    int16_t pcm[]={-32768,32767,-10000,10000};
    sap_info.gain_ratio_percent=50;
    audio_play_apply_local_pcm_gain(pcm,sizeof pcm);
    const int16_t expected[]={-6553,6553,-2000,2000};
    assert(!memcmp(pcm,expected,sizeof pcm));
    assert(sap_info.diagnostic_pcm_samples==4 && sap_info.diagnostic_pcm_peak==6553);
    assert(!sap_info.diagnostic_pcm_clipped && !output_lines);
    g_audio_play_pcm_gain_percent=5;
    int16_t low[]={-32768,32767};
    audio_play_apply_local_pcm_gain(low,sizeof low);
    assert(low[0]==-327 && low[1]==327 && sap_info.diagnostic_pcm_samples==6);
    puts("Local PCM diagnostics: signed full scale/low volume, original half-volume mapping and no extra gain/prints passed");
}
'''
    return source


def main():
    collector = function(UPLINK, "report_aec_path_levels")
    assert "mprintf" not in collector and "printf" not in function(UPLINK, "queue_aec_path_report")
    assert "audio_pre_rslt_report_diagnostics();" in function(UART, "heartbeat_task")
    assert "cm_report_output_diagnostics" not in function(CODEC, "cm_output_interrupt_handler")
    run_fixture("aec_path_diagnostics", aec_fixture())
    run_fixture("dma_output_diagnostics", dma_fixture())
    run_fixture("local_pcm_diagnostics", local_pcm_fixture())


if __name__ == "__main__":
    main()
