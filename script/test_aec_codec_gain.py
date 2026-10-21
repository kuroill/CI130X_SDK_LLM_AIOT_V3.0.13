#!/usr/bin/env python3
"""Run the real AEC codec-gain functions against host codec/status mocks.

This verifies source-level routing and ALC transitions, not the contents of an
existing BNPU archive, a CI firmware build, mailbox timing, or acoustic AEC.
"""

from pathlib import Path
import re

from test_iis_peer_recovery import function, run_fixture


ROOT = Path(__file__).resolve().parents[1]
BASIC_ALG = (ROOT / "components/alg/basic_alg/ci_basic_alg.c").read_text()
STATUS = (ROOT / "components/status_share/status_share.h").read_text()
CONFIG = (
    ROOT / "projects/offline_asr_llm_aiot_iis_sample/app/app_main/ci_ssp_config.c"
).read_text()


def fixture(dual, external_ref, inner_ref, external_mic, wf0153):
    # Use the production enum, config type, and selected project's initializer.
    state = re.search(
        r"typedef enum\s*\{[^}]*\}\s*ci_ss_aec_alc_state;", STATUS
    )
    config = re.search(r"const aec_config_t aec_config\s*=\s*\{.*?\};", CONFIG, re.S)
    assert state, "AEC ALC state enum"
    assert config, "project AEC configuration"
    source = f'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "{ROOT / 'components/alg/aec/ci_adapt_aec.h'}"
#define OFFLINE_DUAL_MIC_ALG_SUPPORT {dual}
#define IF_USE_ANOTHER_CODEC_TO_GET_REF {external_ref}
#define REF_IN_FROM_INNER_CODEC {inner_ref}
#define MIC_RECORD_IIS_SELECT {external_mic}
#define LENWELL_AEC_REF_WF0153 {wf0153}
#define REF_GAIN (LENWELL_AEC_REF_WF0153 ? 4 : 0)
#define HOST_MIC_RECORD_CODEC_ID 1
#define mprintf(...) ((void)0)
'''
    source += state.group() + "\n" + config.group() + r'''
enum { CM_CHA_LEFT, CM_CHA_RIGHT };
enum { DISABLE, ENABLE };
enum { CI_SS_AEC_ALC_STATE };
typedef unsigned long status_t;
typedef struct { int mic_channel_num, ref_channel_num; } Capture;
static Capture capture;
static struct { Capture *audio_capture; } g_ci_ssp_registe = { &capture };
static struct { struct { const void *module_config; } aec; } g_ci_ssp_config = {
    { &aec_config }
};

static int gain[2], alc[2], gain_writes[2], alc_writes[2], query_count;
static status_t alc_state;
static status_t ciss_get(int key) {
    assert(key == CI_SS_AEC_ALC_STATE);
    return alc_state;
}
static void ciss_set(int key, status_t value) {
    assert(key == CI_SS_AEC_ALC_STATE);
    alc_state = value;
}
static int cm_set_codec_adc_gain(int codec, int channel, int value) {
    assert(codec == HOST_MIC_RECORD_CODEC_ID);
    assert(channel == CM_CHA_LEFT || channel == CM_CHA_RIGHT);
    gain[channel] = value;
    ++gain_writes[channel];
    return 0;
}
static int cm_set_codec_alc(int codec, int channel, int enabled) {
    assert(codec == HOST_MIC_RECORD_CODEC_ID);
    assert(channel == CM_CHA_LEFT || channel == CM_CHA_RIGHT);
    assert(enabled == DISABLE || enabled == ENABLE);
    alc[channel] = enabled;
    ++alc_writes[channel];
    return 0;
}
static int cm_get_codec_alc_state_npu(int codec) {
    assert(codec == HOST_MIC_RECORD_CODEC_ID);
    assert(alc_state == CI_SS_AEC_ALC_SETTING);
    ++query_count;
    /* Immediate mock of the MCU mailbox response: either channel's ALC being
     * enabled requests a disable/restore cycle (codec_manage_inner_port.c). */
    alc_state = alc[0] || alc[1] ? CI_SS_AEC_ALC_OPENED_TO_CLOSE : CI_SS_AEC_ALC_CLOSED;
    return 0;
}
'''
    for name in (
        "aec_host_codec_right_is_ref",
        "aec_control_alc_disable_port",
        "aec_control_alc_enable_port",
        "aec_start_alc_process",
        "aec_end_alc_process",
    ):
        source += function(BASIC_ALG, name)
    source += r'''
static void reset(int left_alc, int right_alc, int right_gain) {
    gain[0] = 20;
    gain[1] = right_gain;
    alc[0] = left_alc;
    alc[1] = right_alc;
    gain_writes[0] = gain_writes[1] = 0;
    alc_writes[0] = alc_writes[1] = 0;
    query_count = 0;
    alc_state = CI_SS_AEC_ALC_IDLE;
}

static void assert_playing(bool right_is_ref) {
    assert(gain[0] == 8);
    assert(gain[1] == (right_is_ref ? REF_GAIN : 8));
    assert(alc[0] == DISABLE && alc[1] == DISABLE);
}

static void assert_stopped(bool right_is_ref, bool was_enabled) {
    assert(gain[0] == 20);
    assert(alc[0] == was_enabled);
    if (right_is_ref) {
        /* REF never inherits MIC's idle gain or an enabled ALC. */
        assert(gain[1] == REF_GAIN && alc[1] == DISABLE);
    } else if (OFFLINE_DUAL_MIC_ALG_SUPPORT) {
        assert(gain[1] == 20 && alc[1] == was_enabled);
    } else {
        /* Preserve the existing single-MIC algorithm's non-REF right channel:
         * ending AEC does not enable or restore that channel. */
        assert(gain[1] == 8 && alc[1] == DISABLE);
    }
}

static void test_direct_controls(bool right_is_ref) {
    /* A direct disable must clear even an accidentally enabled REF ALC. */
    reset(ENABLE, ENABLE, 27);
    aec_control_alc_disable_port();
    assert_playing(right_is_ref);
    assert(alc_writes[0] == 1 && alc_writes[1] == 1);
    assert(gain_writes[0] == 1 && gain_writes[1] == 1);
    aec_control_alc_enable_port();
    assert_stopped(right_is_ref, true);
    assert(query_count == 0 && alc_state == CI_SS_AEC_ALC_IDLE);
}

static void test_cycle(bool right_is_ref, bool was_enabled) {
    reset(was_enabled, right_is_ref ? DISABLE : was_enabled, right_is_ref ? REF_GAIN : 20);
    assert(aec_start_alc_process() == was_enabled);
    assert(query_count == 1);
    assert(alc_state == (was_enabled ? CI_SS_AEC_ALC_CLOSED_TO_OPEN : CI_SS_AEC_ALC_CLOSED));
    assert_playing(right_is_ref);
    if (!was_enabled) {
        /* Already-disabled ALC is not toggled just to apply fixed gains. */
        assert(alc_writes[0] == 0 && alc_writes[1] == 0);
    }
    for (int frame = 0; frame < 8; ++frame) {
        assert(!aec_start_alc_process());
        assert(query_count == 1);
        assert_playing(right_is_ref);
    }
    int previous_right_gain_writes = gain_writes[1];
    int previous_right_alc_writes = alc_writes[1];
    assert(aec_end_alc_process() == was_enabled);
    assert_stopped(right_is_ref, was_enabled);
    if (!right_is_ref && !OFFLINE_DUAL_MIC_ALG_SUPPORT) {
        assert(gain_writes[1] == previous_right_gain_writes);
        assert(alc_writes[1] == previous_right_alc_writes);
    }
    /* The actual application owner resets the ALC state after ending AEC. */
    ciss_set(CI_SS_AEC_ALC_STATE, CI_SS_AEC_ALC_IDLE);
    for (int cycle = 0; cycle < 200; ++cycle) {
        assert(aec_start_alc_process() == was_enabled);
        assert_playing(right_is_ref);
        assert(aec_end_alc_process() == was_enabled);
        assert_stopped(right_is_ref, was_enabled);
        ciss_set(CI_SS_AEC_ALC_STATE, CI_SS_AEC_ALC_IDLE);
    }
    assert(query_count == 201);
    int previous_gain_writes = gain_writes[0] + gain_writes[1];
    int previous_alc_writes = alc_writes[0] + alc_writes[1];
    assert(!aec_end_alc_process()); /* Idle end must not change codec state. */
    assert(gain_writes[0] + gain_writes[1] == previous_gain_writes);
    assert(alc_writes[0] + alc_writes[1] == previous_alc_writes);
}

static void test_topology(int microphones, int references, bool right_is_ref) {
    capture = (Capture){ microphones, references };
    assert(aec_host_codec_right_is_ref() == right_is_ref);
    test_direct_controls(right_is_ref);
    test_cycle(right_is_ref, false);
    test_cycle(right_is_ref, true);
}

int main(void) {
    assert(aec_config.alc_off_codec_adc_gain_mic == 8);
    assert(aec_config.alc_off_codec_adc_gain_ref == REF_GAIN);
'''
    # Expected physical topology is independent of the algorithm-buffer flag.
    expected_internal_ref = not (external_ref or inner_ref or external_mic)
    source += f"    test_topology(1, 1, {'true' if expected_internal_ref else 'false'});\n"
    source += r'''
    test_topology(2, 1, false);
    test_topology(1, 0, false);
    test_topology(2, 0, false);
    test_topology(1, 2, false);
    printf("AEC codec gain passed: dual=%d externalRef=%d innerRef=%d externalMic=%d; "
           "5 topologies, original ALC on/off, direct controls, repeated start, 200 restart cycles\n",
           OFFLINE_DUAL_MIC_ALG_SUPPORT, IF_USE_ANOTHER_CODEC_TO_GET_REF,
           REF_IN_FROM_INNER_CODEC, MIC_RECORD_IIS_SELECT);
}
'''
    return source


def main():
    for dual in (0, 1):
        for topology, flags in (
            ("internal", (0, 0, 0)),
            ("external_ref", (1, 0, 0)),
            ("inner_ref", (0, 1, 0)),
            ("external_mic", (0, 0, 1)),
        ):
            for wf0153 in (0, 1):
                run_fixture(f"aec_gain_dual{dual}_{topology}_wf{wf0153}", fixture(dual, *flags, wf0153))
    print("Host source regression only: existing BNPU archives and hardware AEC remain unverified.")


if __name__ == "__main__":
    main()
