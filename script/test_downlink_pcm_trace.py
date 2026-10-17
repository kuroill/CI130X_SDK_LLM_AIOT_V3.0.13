#!/usr/bin/env python3
"""Check the actual CI PCM copy/trace function without building firmware.

Proves sample preservation and diagnostic discrimination, not I2S wire timing,
DAC behavior, or the cause of a hardware noise report.
"""

from pathlib import Path
import re

from test_iis_peer_recovery import function, run_fixture


SDK = Path(__file__).resolve().parents[1]
SOURCE = (
    SDK
    / "projects/offline_asr_llm_aiot_iis_sample/app/app_main/ai_uart_i2s_protocol.c"
).read_text()


def main():
    trace_type = re.search(
        r"typedef struct\s*\{[^}]*\}\s*ai_downlink_pcm_trace_t;", SOURCE
    )
    assert trace_type, "production trace type missing"
    fixture = r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
'''
    fixture += trace_type.group() + "\n"
    fixture += function(SOURCE, "copy_downlink_pcm_with_trace")
    fixture += r'''
int main(void) {
    /* Little-endian PCM bytes spell "foobar": canonical FNV-1a vector. */
    const int16_t stereo[] = {
        0, 0, 0x6f66, 0x6f66, 0x626f, 0x626f, 0, 0, 0x7261, 0x7261
    };
    const int16_t expected[] = {0, 0x6f66, 0x626f, 0, 0x7261};
    int16_t out[8] = {0};
    ai_downlink_pcm_trace_t whole = {.nonzero_hash=0x811c9dc5U};
    assert(copy_downlink_pcm_with_trace(stereo,out,5,&whole)==3);
    assert(memcmp(out,expected,sizeof(expected))==0);
    assert(whole.nonzero_hash==0xbf9cf968U);
    assert(whole.peak==0x7261 && whole.stereo_mismatches==0);

    /* Hardware chunk boundaries and extra bus silence must not change hash. */
    ai_downlink_pcm_trace_t split = {.nonzero_hash=0x811c9dc5U};
    assert(copy_downlink_pcm_with_trace(stereo,out,2,&split)==1);
    assert(copy_downlink_pcm_with_trace(stereo+4,out+2,3,&split)==2);
    const int16_t silence[] = {0,0,0,0};
    assert(copy_downlink_pcm_with_trace(silence,out,2,&split)==0);
    assert(split.nonzero_hash==whole.nonzero_hash && split.peak==whole.peak);
    assert(split.stereo_mismatches==0);

    /* No gain or truncation is allowed, including signed full-scale values. */
    const int16_t limits[] = {-32768,-32768,32767,32767,-1,-1,1,1};
    const int16_t limits_expected[] = {-32768,32767,-1,1};
    ai_downlink_pcm_trace_t boundary = {.nonzero_hash=0x811c9dc5U};
    assert(copy_downlink_pcm_with_trace(limits,out,4,&boundary)==4);
    assert(memcmp(out,limits_expected,sizeof(limits_expected))==0);
    assert(boundary.peak==32768 && boundary.stereo_mismatches==0);

    /* Detect nonzero loss/reordering, and distinguish right-slot corruption. */
    ai_downlink_pcm_trace_t lost = {.nonzero_hash=0x811c9dc5U};
    copy_downlink_pcm_with_trace(stereo+4,out,3,&lost);
    assert(lost.nonzero_hash!=whole.nonzero_hash);
    const int16_t reordered[] = {0x626f,0x626f,0x6f66,0x6f66,0x7261,0x7261};
    ai_downlink_pcm_trace_t swapped = {.nonzero_hash=0x811c9dc5U};
    copy_downlink_pcm_with_trace(reordered,out,3,&swapped);
    assert(swapped.nonzero_hash!=whole.nonzero_hash);
    const int16_t right_fault[] = {0x6f66,0,0x626f,0x626f,0x7261,0x7261};
    ai_downlink_pcm_trace_t wrong_slot = {.nonzero_hash=0x811c9dc5U};
    copy_downlink_pcm_with_trace(right_fault,out,3,&wrong_slot);
    assert(wrong_slot.nonzero_hash==whole.nonzero_hash);
    assert(wrong_slot.stereo_mismatches==1);
    assert(out[0]==0x6f66); /* DAC input remains the original left sample. */
    puts("PCM trace: samples preserved, FNV vector, silence/chunk invariance, signed limits, loss/order/slot discrimination passed");
}
'''
    run_fixture("downlink_pcm_trace", fixture)


if __name__ == "__main__":
    main()
