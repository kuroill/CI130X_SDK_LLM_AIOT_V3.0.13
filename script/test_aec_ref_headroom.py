#!/usr/bin/env python3
"""Exercise real board capture settings and ADC register writes on the host.

This verifies independent REF analog headroom and unchanged MIC/DAC settings.
It cannot prove the analog waveform, acoustic AEC or Windows firmware image.
"""

from pathlib import Path
import re

from test_iis_peer_recovery import function, run_fixture


ROOT = Path(__file__).resolve().parents[1]
BOARD = (ROOT / "driver/boards/CI-D06GT01D.c").read_text()
HEADER = (ROOT / "driver/ci130x_chip_driver/inc/ci130x_codec.h").read_text()
DRIVER = (ROOT / "driver/ci130x_chip_driver/src/ci130x_codec.c").read_text()


def declaration(kind, name):
    match = re.search(r"typedef " + kind + r"\s*\{[^}]*\}\s*" + name + r";", HEADER)
    assert match, name
    return match.group() + "\n"


def fixture(aec, external_ref, external_mic, auto_gain):
    host = BOARD.index("const cm_codec_hw_info_t host_mic_hw_info")
    # Preserve the production preprocessor branches, rather than copying gains.
    start = BOARD.index("#if (MIC_DIFF_SINGLE == 0)", host)
    end = BOARD.index("    .codec_if =", start)
    initializer = BOARD[start:end].replace(".codec_gain.", ".")
    source = f'''
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#define USE_AEC_MODULE {aec}
#define IF_USE_ANOTHER_CODEC_TO_GET_REF {external_ref}
#define MIC_RECORD_IIS_SELECT {external_mic}
#define AUTO_GET_AEC_GAIN {auto_gain}
#define MIC_DIFF_SINGLE 0
#define REF_DIFF_SINGLE 1
'''
    for name in ("inner_codec_input_mode_t", "inner_codec_mic_amplify_t", "inner_codec_cha_sel_t"):
        source += declaration("enum", name)
    source += declaration("struct", "inner_codec_adc_config_t")
    source += "static inner_codec_adc_config_t capture = {\n" + initializer + "};\n"
    source += r'''
static struct {
    uint32_t reg23, reg24, reg25, reg26, reg29;
    uint32_t pga_gain_reg[2], adc_dig_gain_reg[2];
    uint32_t reg6, reg2d;
} codec;
#define CODEC (&codec)
static void _delay_10us_240M(unsigned count) { (void)count; }
'''
    for name in (
        "get_pga_gain_reg_data_from_db", "inner_codec_up_ibas_adc",
        "inner_codec_set_mic_gain", "inner_codec_adc_enable",
        "inner_codec_adc_dig_gain_set", "inner_codec_set_pga_gain",
    ):
        source += function(DRIVER, name)
    source += r'''
int main(void) {
    bool right_is_ref = USE_AEC_MODULE && !IF_USE_ANOTHER_CODEC_TO_GET_REF && !MIC_RECORD_IIS_SELECT;
    assert(capture.codec_adc_input_mode_l == INNER_CODEC_INPUT_MODE_DIFF);
    assert(capture.codec_adc_mic_amp_l == INNER_CODEC_MIC_AMP_12dB);
    assert(capture.pga_gain_l == (AUTO_GET_AEC_GAIN ? 20.0f : 28.5f));
    assert(capture.dig_gain_l == 1.0f && capture.dig_gain_r == 1.0f);
    assert(capture.codec_adc_mic_amp_r == (right_is_ref ? INNER_CODEC_MIC_AMP_0dB : INNER_CODEC_MIC_AMP_12dB));
    assert(capture.pga_gain_r == (right_is_ref || AUTO_GET_AEC_GAIN ? 0.0f : 28.5f));
    codec.reg6 = 255; codec.reg2d = 31;
    inner_codec_adc_enable(&capture);
    inner_codec_adc_dig_gain_set(INNER_CODEC_LEFT_CHA, (int)capture.dig_gain_l);
    inner_codec_adc_dig_gain_set(INNER_CODEC_RIGHT_CHA, (int)capture.dig_gain_r);
    assert((codec.pga_gain_reg[0] >> 5) == INNER_CODEC_MIC_AMP_12dB);
    assert((codec.pga_gain_reg[1] >> 5) == capture.codec_adc_mic_amp_r);
    if (right_is_ref) {
        assert(codec.pga_gain_reg[1] == 0x0c); /* Unity front end and PGA from initialization. */
        for (int cycle = 0; cycle < 200; ++cycle) {
            inner_codec_set_pga_gain(INNER_CODEC_LEFT_CHA, 8);
            inner_codec_set_pga_gain(INNER_CODEC_RIGHT_CHA, 0);
            assert(codec.pga_gain_reg[0] == 0x71); /* Existing playback MIC sensitivity. */
            assert(codec.pga_gain_reg[1] == 0x0c); /* PGA writes cannot restore the MIC boost. */
            inner_codec_set_pga_gain(INNER_CODEC_LEFT_CHA, 20);
            assert(codec.pga_gain_reg[0] == 0x79); /* Existing idle MIC sensitivity. */
            assert(codec.pga_gain_reg[1] == 0x0c);
        }
    }
    assert(codec.adc_dig_gain_reg[0] == 199 && codec.adc_dig_gain_reg[1] == 199);
    assert(codec.reg6 == 255 && codec.reg2d == 31); /* Capture settings never change DAC output. */
}
'''
    return source


def main():
    for aec in (0, 1):
        for external_ref, external_mic in ((0, 0), (1, 0), (0, 1)):
            for auto_gain in (0, 1):
                name = f"ref_headroom_{aec}_{external_ref}_{external_mic}_{auto_gain}"
                run_fixture(name, fixture(aec, external_ref, external_mic, auto_gain))
    print("REF analog gain: 12 board configurations, real ADC/PGA writes and 200 playback cycles passed; MIC, digital gains and DAC preserved.")
    print("Host source checks only; REF clipping, acoustic noise and human barge-in need hardware validation.")


if __name__ == "__main__":
    main()
