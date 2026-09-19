#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "sdk_default_config.h"
#include "user_config.h"
#include "romlib_runtime.h"
#include "codec_manage_outside_port.h"
#include "status_share.h"
#include "ci_log.h"
#include "ci_basic_alg.h"
#include "bnpu_mem_manage.h"
#include "ci_audio_wrapfft.h"
#include "ci_adapt_aec.h"
#include "ci_aec_type.h"
#include "temporal_vad_api.h"
#include "alc_auto_switch.h"
#include "bnpu_math.h"
#include "alg_preprocess.h"

#define ENG_CMPT_FRAME   50

extern ci_ssp_config_t g_ci_ssp_config;

//门限控制
alg_th_s *alg_th_create(const alg_thr_config_t *alg_thr_config)
{
    alg_th_s *alg_th = (alg_th_s *)bnpu_remote_calloc(1, sizeof(alg_th_s));
    alg_th->alg_thr_config = alg_thr_config;

    alg_th_module th_module = alg_th->alg_thr_config->th_module;

    float fre_resolution = 31.25f;                                                //频率分辨率 8000/256
    int start_fre_num = (int)(alg_th->alg_thr_config->start_Hz / fre_resolution); //开始计算频点
    int end_fre_num = (int)(alg_th->alg_thr_config->end_Hz / fre_resolution);     //结束计算频点

    float alg_throshold = alg_th->alg_thr_config->alg_throshold;

    alg_th->decide_frame_len = alg_th->alg_thr_config->set_alg_thr_window_size;

    float frame_shift = (float)(AUDIO_CAP_POINT_NUM_PER_FRM); //时域累加点数
    float gain_thr = ci_sqrt_f32(frame_shift);

    alg_th->max_idx = 35;
    alg_th->sg_eng = (float *)bnpu_remote_calloc(alg_th->max_idx, sizeof(float));

    if (DENOISE == th_module)
    {
        alg_th->threshold = alg_throshold * gain_thr * ((end_fre_num - start_fre_num) / 256.0f); //5000 65dB 2400 60dB
        alg_th->default_alg_threshold = alg_th->threshold;
    }
    else if (BEAMFORMING == th_module)
    {
        alg_th->threshold = alg_throshold * gain_thr;
    }
    else if (DEREVERB == th_module)
    {
        alg_th->threshold = alg_throshold * gain_thr;
    }

    return alg_th;
}
//动态alc改变幅值大小后，需要调整个算法模块的门限值,alc关闭时默认门限置为0，开启时回复默认值
void reset_threshold_for_alc_switch(alg_th_s *alg_th)
{
    if(NULL == alg_th)
        return ;
    static int pre_state = 1;
    status_t ci_ss_alc_state = ciss_get(CI_SS_ALC_STATE); //alc状态

    if (ci_ss_alc_state != pre_state)
    {
        if (CI_SS_ALC_OFF == ci_ss_alc_state)
        {
            alg_th->threshold = 0.0f;
        }
        else if (CI_SS_ALC_UP == ci_ss_alc_state)
        {
            alg_th->threshold = alg_th->default_alg_threshold;
        }
    }
    pre_state = ci_ss_alc_state;
}


static float get_back_energy(alg_th_s *handle, float psd)
{
    //float back_energy = handle->back_energy;
    static int eng_idx = 0;
    static float back_energy = 0.0f;
    static float p = 0.0f;

    float alpha_p = 0.2f;
    float *sg_eng = handle->sg_eng;
    int max_idx = handle->max_idx;

    sg_eng[eng_idx] = psd;
    eng_idx++;

    if (eng_idx >= max_idx)
    {
        eng_idx = 0;
    }
    float min_eng = sg_eng[0];
    for (int i = 0; i < max_idx; i++)
    {
        if (min_eng > sg_eng[i])
        {
            min_eng = sg_eng[i];
        }
    }

    if (psd > 3.5f * min_eng)
    {
        p = alpha_p * p + (1.0f - alpha_p);
    }
    else
    {
        p = alpha_p * p;
    }

    if (p < 0.01f)
    {
        back_energy = back_energy * p + (0.95f * back_energy + 0.05f * psd) * (1.0f - p);
    }

    return back_energy;
}

bool ci_ssp_module_thr_control(alg_th_s *alg_th, float *fft)
{
    static bool enable_flag = false;                 //alg_th->flag;
    int decide_frame_len = alg_th->decide_frame_len; //20;
    static int decide_frame_num = 0;

    float threshold = alg_th->threshold;
    float cur_threshold = 0.0f;
    float alpha = 0.85f;
    float alpha_threshold = 1.0f;

    static float frm_eng_ave = 0.0f; //alg_th->frm_eng_ave;

    static int enable_count = 0;
    static int disenable_count = 0;

    int fft_fre_num = 256;
    int index = fft_fre_num / 4;

    float frm_eng0 = 0.0f;
    float frm_eng1 = 0.0f;
    float frm_eng2 = 0.0f;
    float frm_eng3 = 0.0f;
    float cur_frm_eng_psd = 0.0f;
    for (int i = 0; i < index; i++)
    {
        frm_eng0 += fft[2 * (i << 2)] * fft[2 * (i << 2)] + fft[2 * (i << 2) + 1] * fft[2 * (i << 2) + 1];
        frm_eng1 += fft[2 * ((i << 2) + 1)] * fft[2 * ((i << 2) + 1)] + fft[2 * ((i << 2) + 1) + 1] * fft[2 * ((i << 2) + 1) + 1];
        frm_eng2 += fft[2 * ((i << 2) + 2)] * fft[2 * ((i << 2) + 2)] + fft[2 * ((i << 2) + 2) + 1] * fft[2 * ((i << 2) + 2) + 1];
        frm_eng3 += fft[2 * ((i << 2) + 3)] * fft[2 * ((i << 2) + 3)] + fft[2 * ((i << 2) + 3) + 1] * fft[2 * ((i << 2) + 3) + 1];
    }

    cur_frm_eng_psd = (frm_eng0 + frm_eng1) + (frm_eng2 + frm_eng3);

    cur_frm_eng_psd = cur_frm_eng_psd / fft_fre_num;

    cur_frm_eng_psd = ci_sqrt_f32(cur_frm_eng_psd); // sum(FFT(n)^2)/N

    status_t vad_state = ciss_get(CI_SS_VAD_STATE);                    //asr_vad 获取vad_start时刻
    if ((CI_SS_VAD_IDLE == vad_state) || (CI_SS_VAD_END == vad_state)) //vad非语音段
    {
        frm_eng_ave = get_back_energy(alg_th, cur_frm_eng_psd);
    }

    if (enable_flag)
    {
        alpha_threshold = 0.8f;
    }
    else
    {
        alpha_threshold = 1.2f;
    }

    cur_threshold = alpha_threshold * threshold;

    if (decide_frame_num == decide_frame_len)
    {
        disenable_count = 0;
        enable_count = 0;
        decide_frame_num = 0;
    }
    if (frm_eng_ave < cur_threshold)
    {
        disenable_count++;
    }
    else
    {
        enable_count++;
    }

    if ((decide_frame_num == (decide_frame_len - 1)) && (disenable_count > decide_frame_len * 0.8f))
    {
        enable_flag = false;
    }
    else if ((decide_frame_num == (decide_frame_len - 1)) && (enable_count > decide_frame_len * 0.8f))
    {
        enable_flag = true;
    }
    else if ((decide_frame_num == (decide_frame_len - 1)) && ((enable_count <= decide_frame_len * 0.8f) || (disenable_count <= decide_frame_len * 0.8f)))
    {
        enable_flag = false;
    }

    decide_frame_num++;

    return enable_flag;
}

float db_buff[50]={0.0f};
// float var_buff[10]={0.0f};
// ci_ss_wakeup_state_t pre_wakeup_state_bf = CI_SS_NO_WAKEUP;
// ci_ss_wakeup_state_t now_wakeup_state_bf = CI_SS_NO_WAKEUP;
extern volatile int g_has_zero;
bool ci_bf_module_thr_control(void *wrapfft_audio_t)
{
    bool enable_flag = false;   
    static float mic_db = 0.0f; 
    static float mic_db_smooth = 0.0f;
    static int eng_buff_idx = 0;
    ci_wrapfft_audio *wrapfft_audio = (ci_wrapfft_audio *)wrapfft_audio_t;      
    
    float eng_left_mic = 0.0f;
    for (int i = 0; i < AUDIO_CAP_POINT_NUM_PER_FRM; i++)
    {
        eng_left_mic += (wrapfft_audio->mic[0][i] * wrapfft_audio->mic[0][i]);
    }
    float eng_left_mic_avg = hard_fsqrt(eng_left_mic / AUDIO_CAP_POINT_NUM_PER_FRM) / 32767.0f;
    float mic_db_cur = 20 * log10f(eng_left_mic_avg / 5.0f);
    //db_buff[eng_buff_idx] = mic_db_cur;
    mic_db += mic_db_cur;
    eng_buff_idx++;
    if (eng_buff_idx == ENG_CMPT_FRAME)
    {   
        float mic_db_avg = mic_db /ENG_CMPT_FRAME;
        mic_db_smooth = 0.1f  *mic_db_smooth + 0.9f * mic_db_avg;
        eng_buff_idx = 0;
        mic_db = 0.0f;
    }
    
    if((mic_db_smooth+100.0f > 35.0f) && (g_has_zero ==0))
    {
        enable_flag = true;
    }
    
    return enable_flag;
}

extern ci_ssp_registe_t g_ci_ssp_registe;
//ALC开关增益控制
void inner_codec_left_alc_enable_port(void)
{  
    cm_set_codec_alc(HOST_MIC_RECORD_CODEC_ID, CM_CHA_LEFT, ENABLE);
    cm_set_codec_adc_gain(HOST_MIC_RECORD_CODEC_ID, CM_CHA_LEFT, 20);
	#if (OFFLINE_DUAL_MIC_ALG_SUPPORT)
    if(2 == g_ci_ssp_registe.audio_capture->mic_channel_num)
    {
        cm_set_codec_alc(HOST_MIC_RECORD_CODEC_ID, CM_CHA_RIGHT, ENABLE);
        cm_set_codec_adc_gain(HOST_MIC_RECORD_CODEC_ID, CM_CHA_RIGHT, 20);
    }  
   	#endif
}

void inner_codec_alc_disable_port(void)
{
    
    int alc_off_codec_adc_gain = ((alc_auto_switch_config_t*)(g_ci_ssp_config.alc_auto_switch.module_config))->alc_off_codec_adc_gain;
    cm_set_codec_alc(HOST_MIC_RECORD_CODEC_ID, CM_CHA_LEFT, DISABLE);
    cm_set_codec_adc_gain(HOST_MIC_RECORD_CODEC_ID, CM_CHA_LEFT, alc_off_codec_adc_gain);
	#if (OFFLINE_DUAL_MIC_ALG_SUPPORT)
    if(2 == g_ci_ssp_registe.audio_capture->mic_channel_num)
    {
        cm_set_codec_alc(HOST_MIC_RECORD_CODEC_ID, CM_CHA_RIGHT, DISABLE);
        cm_set_codec_adc_gain(HOST_MIC_RECORD_CODEC_ID, CM_CHA_RIGHT, alc_off_codec_adc_gain);
    }
   	#endif
}

int get_vad_idle_flag_port(void)
{
    status_t vad_state = ciss_get(CI_SS_VAD_STATE); //asr_vad 获取vad_start时刻

    if (CI_SS_VAD_IDLE == vad_state || CI_SS_VAD_END == vad_state)
    {
        return 0;
    }

    return 1;
}

//aec播报状态控制
//extern aec_config_t aec_config;


static bool aec_alc_state = true;
bool get_aec_alc_state(void)
{
    return aec_alc_state;
}


void* set_aec_gain(void* wrapfft_audio_t)
{
    ci_wrapfft_audio *wrapfft_audio_st = (ci_wrapfft_audio *)wrapfft_audio_t;
    int fft_size = wrapfft_audio_st->fft_size;
    float* fft = wrapfft_audio_st->fft_mic_out[0];  
    float aec_gain = ((aec_config_t*)(g_ci_ssp_config.aec.module_config))->aec_gain;
    for (int i = 0; i < fft_size; i++)
    { 
        fft[2 * i] *= aec_gain;
        fft[2 * i + 1] *= aec_gain;
    }

    return wrapfft_audio_st;
}
#if 0
static bool aec_process_enable_flag = true;
bool get_aec_process_enable_flag()
{
    return aec_process_enable_flag;
}

static bool aec_ref_flag_set(void *wrapfft_audio_t)
{
    ci_wrapfft_audio *wrapfft_audio_st = (ci_wrapfft_audio *)wrapfft_audio_t;
    float *fft_ref = wrapfft_audio_st->fft_ref_out[0];
    float *fft_mic = wrapfft_audio_st->fft_mic_out[0];
    
    float aec_threshold = ((aec_config_t*)(g_ci_ssp_config.aec.module_config))->aec_enable_threshold;//参考幅值判断门限
    float aec_mic_div_ref_thr = ((aec_config_t*)(g_ci_ssp_config.aec.module_config))->aec_mic_div_ref_thr;//参考幅值判断门限
    static bool aec_flag = true;
    
    static float frm_eng_ave = 0;
    float alpha = 0.6f;
    float cur_frm_eng = 0.0f;
   
    int fft_size = wrapfft_audio_st->fft_size; 
    int frame_size = wrapfft_audio_st->iis_input_frame_len/2;//32K采样，抽取160/256个点,16K采样，抽取80/128个点
    int start_frebin = 8;
    int end_frebin = 180;
    int cur_frebin = end_frebin - start_frebin;
   
    float cur_frm_eng_mic = 0.0f;
    float frm_eng_ave_mic = 0.0f;
    for (int i = start_frebin; i < end_frebin; i++)
    {
        cur_frm_eng += fabsf(fft_ref[2 * i]); //抽取
        cur_frm_eng_mic += fabsf(fft_mic[2 * i]); //抽取
    }
   
    frm_eng_ave =  alpha * frm_eng_ave +  (1.0f - alpha)* cur_frm_eng;
    frm_eng_ave_mic =  alpha * frm_eng_ave_mic +  (1.0f - alpha)* cur_frm_eng_mic;
     
    float mic_div_ref = frm_eng_ave_mic / ( frm_eng_ave + 1e-6f);
    aec_process_enable_flag = true;
    if ((frm_eng_ave < aec_threshold))
    {
        aec_flag = false;
    }
    else
    {
        aec_flag = true;
        if ((mic_div_ref < aec_mic_div_ref_thr))
        {
            aec_process_enable_flag = false;          
        }
    }
    return aec_flag;
}
#endif
bool aec_ref_flag_set(void *wrapfft_audio_t)
{
    ci_wrapfft_audio *wrapfft_audio_st = (ci_wrapfft_audio *)wrapfft_audio_t;
    float cur_frm_eng_ref = 0.0f;
    static float frm_eng_ref = 0.0f;
    aec_config_t* aec_config = (aec_config_t*)(g_ci_ssp_config.aec.module_config);
 	float aec_ref_thr = aec_config->aec_enable_threshold;//参考幅值判断门限
    complex_aec ** ref_fft = (complex_aec **)wrapfft_audio_st->fft_ref_out;

    for (int i = 5; i < 134; i++)//计算ref 5-134频点的能量 ，播报声非常小人耳勉强能听到与播报声听不到的区间，并且能量集中在区域选出的频点
    {
        cur_frm_eng_ref += ref_fft[0][i].real * ref_fft[0][i].real + ref_fft[0][i].image * ref_fft[0][i].image;
	}

    frm_eng_ref = 0.7 * frm_eng_ref + 0.3 * ci_sqrt_f32(cur_frm_eng_ref);
	wrapfft_audio_st->cur_aec_ref_eng = frm_eng_ref;

    /*参考信号能量小于阈值不进行AEC*/
    if(frm_eng_ref <=  aec_ref_thr)
    {
        return false; 
    }
    else
    {
        return true;
    }
    
}

/* In the single-mic internal-codec AEC topology, the host codec's left
 * channel is MIC and its right channel is REF. USE_AEC currently also defines
 * OFFLINE_DUAL_MIC_ALG_SUPPORT for the algorithm buffers, so that compile-time
 * flag cannot be used to decide whether the physical right channel is a second
 * microphone. Doing so applies the MIC gain to REF and can saturate the AEC
 * reference at high playback volume. */
static bool aec_host_codec_right_is_ref(void)
{
#if IF_USE_ANOTHER_CODEC_TO_GET_REF || REF_IN_FROM_INNER_CODEC || MIC_RECORD_IIS_SELECT
    return false;
#else
    return (1 == g_ci_ssp_registe.audio_capture->mic_channel_num) &&
           (1 == g_ci_ssp_registe.audio_capture->ref_channel_num);
#endif
}

void aec_control_alc_disable_port(void)
{
    // mprintf("aec_control_alc_disable_port1\r\n");
    int alc_off_codec_adc_gain_mic = ((aec_config_t*)(g_ci_ssp_config.aec.module_config))->alc_off_codec_adc_gain_mic;
    int alc_off_codec_adc_gain_ref = ((aec_config_t*)(g_ci_ssp_config.aec.module_config))->alc_off_codec_adc_gain_ref;

    cm_set_codec_alc(HOST_MIC_RECORD_CODEC_ID, CM_CHA_LEFT, DISABLE);
    cm_set_codec_adc_gain(HOST_MIC_RECORD_CODEC_ID, CM_CHA_LEFT, alc_off_codec_adc_gain_mic);
    cm_set_codec_alc(HOST_MIC_RECORD_CODEC_ID, CM_CHA_RIGHT, DISABLE);
    cm_set_codec_adc_gain(
        HOST_MIC_RECORD_CODEC_ID,
        CM_CHA_RIGHT,
        aec_host_codec_right_is_ref() ? alc_off_codec_adc_gain_ref : alc_off_codec_adc_gain_mic);
    mprintf(
        "[AEC] fixed_gain micDb=%d hostRightRole=%s hostRightDb=%d\r\n",
        alc_off_codec_adc_gain_mic,
        aec_host_codec_right_is_ref() ? "ref" : "mic",
        aec_host_codec_right_is_ref() ? alc_off_codec_adc_gain_ref : alc_off_codec_adc_gain_mic);
}
void aec_control_alc_enable_port(void)
{
    // mprintf("aec_control_alc_enable_port1\r\n");
    cm_set_codec_alc(HOST_MIC_RECORD_CODEC_ID, CM_CHA_LEFT, ENABLE);
    cm_set_codec_adc_gain(HOST_MIC_RECORD_CODEC_ID, CM_CHA_LEFT, 20);
    if(aec_host_codec_right_is_ref())
    {
        int alc_off_codec_adc_gain_ref = ((aec_config_t*)(g_ci_ssp_config.aec.module_config))->alc_off_codec_adc_gain_ref;
        cm_set_codec_alc(HOST_MIC_RECORD_CODEC_ID, CM_CHA_RIGHT, DISABLE);
        cm_set_codec_adc_gain(HOST_MIC_RECORD_CODEC_ID, CM_CHA_RIGHT, alc_off_codec_adc_gain_ref);
    }
#if OFFLINE_DUAL_MIC_ALG_SUPPORT
    else
    {
        cm_set_codec_alc(HOST_MIC_RECORD_CODEC_ID, CM_CHA_RIGHT, ENABLE);
        cm_set_codec_adc_gain(HOST_MIC_RECORD_CODEC_ID, CM_CHA_RIGHT, 20);
    }
#endif
}

static bool aec_start_alc_process(void)
{
    if(CI_SS_AEC_ALC_IDLE == ciss_get(CI_SS_AEC_ALC_STATE))
    {
        uint16_t cnt = 1000;
        ciss_set(CI_SS_AEC_ALC_STATE,CI_SS_AEC_ALC_SETTING);
        cm_get_codec_alc_state_npu(HOST_MIC_RECORD_CODEC_ID);
        while(CI_SS_AEC_ALC_SETTING == ciss_get(CI_SS_AEC_ALC_STATE) && cnt--);
        if(cnt == 0)
        {
            mprintf("ALC err\r\n");
            return false;
        }
    }
    /*进入AEC时开启了ALC，则关闭ALC*/
    if(CI_SS_AEC_ALC_OPENED_TO_CLOSE == ciss_get(CI_SS_AEC_ALC_STATE))
    {
        ciss_set(CI_SS_AEC_ALC_STATE,CI_SS_AEC_ALC_CLOSED_TO_OPEN);
        aec_control_alc_disable_port();
        return true;
    }
    else
    {
        int alc_off_codec_adc_gain_mic = ((aec_config_t*)(g_ci_ssp_config.aec.module_config))->alc_off_codec_adc_gain_mic;
        int alc_off_codec_adc_gain_ref = ((aec_config_t*)(g_ci_ssp_config.aec.module_config))->alc_off_codec_adc_gain_ref;
        cm_set_codec_adc_gain(HOST_MIC_RECORD_CODEC_ID, CM_CHA_LEFT, alc_off_codec_adc_gain_mic);
        cm_set_codec_adc_gain(
            HOST_MIC_RECORD_CODEC_ID,
            CM_CHA_RIGHT,
            aec_host_codec_right_is_ref() ? alc_off_codec_adc_gain_ref : alc_off_codec_adc_gain_mic);
        return false;
    }
}
static bool aec_end_alc_process(void)
{
    /*进入AEC时开启了ALC，则重新开启*/
    if(CI_SS_AEC_ALC_CLOSED_TO_OPEN == ciss_get(CI_SS_AEC_ALC_STATE))
    {
        aec_control_alc_enable_port();
        return true;
    }
    if(CI_SS_AEC_ALC_CLOSED == ciss_get(CI_SS_AEC_ALC_STATE))
    {
        cm_set_codec_adc_gain(HOST_MIC_RECORD_CODEC_ID, CM_CHA_LEFT, 20);
        if(aec_host_codec_right_is_ref())
        {
            int alc_off_codec_adc_gain_ref = ((aec_config_t*)(g_ci_ssp_config.aec.module_config))->alc_off_codec_adc_gain_ref;
            cm_set_codec_alc(HOST_MIC_RECORD_CODEC_ID, CM_CHA_RIGHT, DISABLE);
            cm_set_codec_adc_gain(HOST_MIC_RECORD_CODEC_ID, CM_CHA_RIGHT, alc_off_codec_adc_gain_ref);
        }
#if OFFLINE_DUAL_MIC_ALG_SUPPORT
        else
        {
            cm_set_codec_adc_gain(HOST_MIC_RECORD_CODEC_ID, CM_CHA_RIGHT, 20);
        }
#endif
    }
    return false;
}
bool ci_aec_application_enable(void *wrapfft_audio_t)
{
    static bool aec_enable = false;
    static bool alc_open_to_close_flag = false;
    static bool alc_close_to_open_flag = false;
    static int32_t aec_delay_start_count = 0;
    static int32_t aec_delay_end_count = 0;
    static bool aec_work_enable = false;
    static bool ampl_aec_alc_en_flag = true;
    static int8_t ampl_aec_start_count = 0;
    static int8_t ampl_aec_end_count = 0;
    static uint8_t play_state_aec_idle_cnt = 0; //避免播报状态模式ref_thr设置不对,或者有干扰导致的能量波动大于阈值一直不结束AEC
    aec_control_mode_t aec_control_mode = ((aec_config_t*)(g_ci_ssp_config.aec.module_config))->aec_control_mode; //COMPUTE_REF_AMPL_MODE; //ENABLE_PLAYING_STATE_MODE;

    switch (aec_control_mode)
    {
        case ENABLE_PLAYING_STATE_MODE:
        {
            status_t ci_ss_play_state = ciss_get(CI_SS_PLAY_STATE); //播报状态
            if (CI_SS_PLAY_STATE_PLAYING == ci_ss_play_state)
            {
                aec_enable = aec_ref_flag_set(wrapfft_audio_t); //播报状态下进行aec处理，还有门限控制;
                alc_open_to_close_flag = aec_start_alc_process(); //alc关闭时计数清0;
                if (alc_open_to_close_flag)
                {
                    aec_delay_start_count = 3;
                }
                   
                if (aec_delay_start_count-- > 0) //alc开始时，前3帧不进行aec处理
                {
                    aec_alc_state = false;
                    return false;
                }
                else
                {
                    aec_alc_state = true;
                }

                if(!aec_work_enable && aec_enable)
                {
                    play_state_aec_idle_cnt = 60;        //播放结束最多再做60帧AEC
                    aec_work_enable = true;
                    parameters_config_aec_normal(true);	 //修改为AEC下的VAD参数
                    ciss_set(CI_SS_AEC_WORK_STATE, 1);   //aec工作状态
                }

            }
            else if(CI_SS_PLAY_STATE_PLAYING_TO_IDLE == ci_ss_play_state)
            {
                aec_enable = aec_ref_flag_set(wrapfft_audio_t); //在播报状态切换到非播报状态时，需要门限控制结束;
                if(!aec_enable || play_state_aec_idle_cnt == 0)
                {
                    alc_close_to_open_flag = aec_end_alc_process();
                    if (alc_close_to_open_flag)
                    {
                        aec_delay_end_count = 1;
                    }
                    ciss_set(CI_SS_AEC_ALC_STATE,CI_SS_AEC_ALC_IDLE);//复位ALC状态
                    if (aec_delay_end_count-- > 0) //aec结束时开了ALC，多处理一帧AEC
                    {
                        aec_alc_state = true;
                        return true;
                    }
                    else
                    {
                        aec_alc_state = false;
                    }
                    play_state_aec_idle_cnt = 0;
                    aec_work_enable = false;
                    parameters_config_aec_normal(false); //返回默认VAD参数	
                    ciss_set(CI_SS_AEC_WORK_STATE, 0);   //aec工作状态
                    ciss_set(CI_SS_PLAY_STATE,CI_SS_PLAY_STATE_IDLE);
                    aec_delay_start_count = 0;
                    aec_delay_end_count = 0;
                }
                else
                {
                    play_state_aec_idle_cnt--;
                }
                
            }
            else if (CI_SS_PLAY_STATE_IDLE == ci_ss_play_state)
            {
                aec_enable = false;
                aec_alc_state = false;
            }
            break;
        }
        case COMPUTE_REF_AMPL_MODE:
        {
            aec_enable = aec_ref_flag_set(wrapfft_audio_t); //AEC仅由门限控制
            if (aec_enable)
            {
                if(ampl_aec_alc_en_flag)
                {
                    alc_open_to_close_flag = aec_start_alc_process();
                }
                if (alc_open_to_close_flag)
                {
                    alc_open_to_close_flag = false;
                    ampl_aec_alc_en_flag = false;
                    aec_delay_start_count = 3;
                }
                   
                if (aec_delay_start_count-- > 0) //alc开始时，前3帧不进行aec处理
                {
                    aec_alc_state = false;
                    return false;
                }
                else
                {
                    aec_alc_state = true;
                }
                if(ampl_aec_end_count>0)
                ampl_aec_end_count--;
                if(!aec_work_enable)
                {
                    if(ampl_aec_start_count > 3)
                    {
                        aec_work_enable = true;
                        ampl_aec_start_count = 0;
                        ciss_set(CI_SS_AEC_WORK_STATE, 1);   //aec工作状态
                    }
                    ampl_aec_start_count++;
                }
            }
            else
            {
                alc_close_to_open_flag = aec_end_alc_process();
                if(CI_SS_AEC_ALC_IDLE != ciss_get(CI_SS_AEC_ALC_STATE))
                {
                    ciss_set(CI_SS_AEC_ALC_STATE,CI_SS_AEC_ALC_IDLE);//复位ALC状态
                }
                if (alc_close_to_open_flag)
                {
                    aec_delay_end_count = 1;
                }
                
                if (aec_delay_end_count-- > 0) //aec结束时开了ALC，多处理一帧AEC
                {
                    aec_alc_state = true;
                    return true;
                }
                else
                {
                    aec_alc_state = false;
                }
                if(ampl_aec_start_count>0)
                ampl_aec_start_count--;
                if(aec_work_enable)
                {
                    if(ampl_aec_end_count > 3)
                    {
                        aec_work_enable = false;
                        ampl_aec_end_count = 0;
                        ciss_set(CI_SS_AEC_WORK_STATE, 0);   //aec工作状态
                    }
                    ampl_aec_end_count++;
                }
            }

            if(aec_delay_start_count<0)
            {
                aec_delay_start_count = -1;
            }
            if(aec_delay_end_count<0)
            {
                aec_delay_end_count = -1;
            }
            break;
        }
        default:
        {
            // CI_ASSERT(0,"\n");
        }
    }

    return aec_enable;
}
