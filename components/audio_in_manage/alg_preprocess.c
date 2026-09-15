/**
  ******************************************************************************
  * @file    alg_preprocess.c
  * @version V1.0.0
  * @date    2019.04.04
  * @brief 
  ******************************************************************************
  */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include "asr_process_callback.h"
#include "romlib_runtime.h"
#include "asr_api.h"
#include "ci_log.h"
#include "sdk_default_config.h"
#include "debug_time_consuming.h"

#include "alc_auto_switch.h"
#include "ci_tra_denoise.h"
#include "ci_audio_wrapfft.h"
#include "ci_basic_alg.h"
#include "ci_bf.h"
#include "ci_dereverb.h"
#include "ci_adapt_aec.h"
#include "ci_gcc_doa.h"
#include "ci_doa_apply.h"
#include "ci_pwk.h"
#include "ci_nn_doa_init_param.h"
#include "nn_denoise_api.h"
#include "ci_log_config.h"
#include "ci130x_audio_pre_rslt_out.h"
#include "alg_preprocess.h"
#include "status_share.h"
#include "bnpu_mem_manage.h"
#include "bnpu_math.h"
#include "temporal_vad_api.h"
#if USE_SED
#include "sed_manage.h"
#include "serial_asr_flow.h"
#endif
#include "user_config.h"
#include "remote_api_for_bnpu.h"
#include "status_share.h"
#include "noise_energy_estimation_api.h"

extern int get_vad_state(float *fft_data, float *);
extern int apply_asr_data_deal(int vad_state, float **psd, short *pcm_data, int total_channel_nums);
#if USE_SED
extern int serial_asr_apply_asr_data_deal(int vad_state, float *psd, short *pcm_data, fe_cmpt_info_t *fe_cmpt_info_p);
sed_config_t *g_sed_config_para;
#endif
extern int set_pcm_vad_mark_flag(short *pcm_data, int frame_len);
static alg_th_s *sg_denoise_alg_th = NULL;
ci_wrapfft_audio *g_wrapfft_audio = NULL;
nn_doa_config_t  *g_nn_doa_config = NULL;
gcc_doa_config_t *g_gcc_doa_config = NULL;
aec_config_t     *g_aec_config = NULL;
ci_ssp_registe_t  g_ci_ssp_registe;
float             g_fft_rslt_array[16] = {0};

ci_ssp_config_t g_ci_ssp_config = {
	.alc_auto_switch = {true, ci_alc_auto_switch_module_init, ci_alc_auto_switch_processing, NULL},
#if USE_NN_DENOISE
	.nn_noise_est = {true, ci_nn_noise_est_init, ci_nn_noise_est_processing, NULL},
#else
	.nn_noise_est = {false, NULL, NULL, NULL},
#endif
	.stft = {true, ci_stft_module_init, ci_stft_processing, NULL},
#if OFFLINE_DUAL_MIC_ALG_SUPPORT
#if (USE_NN_DOA || USE_GCC_DOA)
	.doa = {false, ci_doa_module_init, ci_doa_processing, NULL},
#else
	.doa = {false, NULL, NULL, NULL},
#endif
#endif
#if USE_AEC_MODULE
	.aec = {true, ci_aec_module_init, ci_aec_processing, NULL},
#else
	.aec = {false, NULL, NULL, NULL},
#endif
#if OFFLINE_DUAL_MIC_ALG_SUPPORT
#if USE_DEREVERB_MODULE
	.dereverb = {true, ci_dereverb_module_init, ci_dereverb_processing, NULL},
#else
	.dereverb = {false, NULL, NULL, NULL},
#endif
#if USE_BEAMFORMING_MODULE
	.bf = {true, ci_bf_module_init, ci_bf_processing, NULL},
#else
	.bf = {false, NULL, NULL, NULL},
#endif
#endif
#if USE_NN_DENOISE
	.nn_denoise = {true, ci_nn_denoise_module_init, ci_nn_denoise_processing, NULL},
#elif USE_TRA_DENOISE
	.tra_denoise = {true, ci_tra_denoise_module_init, ci_tra_denoise_processing, NULL},
#else
	.tra_denoise = {false, NULL, NULL, NULL},
#endif
#if USE_NN_VAD
	.nn_vad = {false, ci_nn_vad_module_init, NULL, NULL},
#else
	.nn_vad = {false, NULL, NULL, NULL},
#endif
#if USE_PWK
	.pwk = {true, ci_pwk_module_init, ci_pwk_processing, NULL}, //ci_pwk_processing
#else
	.pwk = {false, NULL, NULL, NULL},
#endif
	.istft = {true, ci_istft_module_init, ci_istft_processing, NULL},
	.iis_out_audio = {true, ci_iis_out_audio_module_init, ci_iis_out_audio_processing, NULL},
#if USE_SED
	.sed = {true, ci_sed_module_init, ci_sed_processing, NULL},
#else
	.sed = {false, NULL, NULL, NULL},
#endif
};

//ssp_registe
void set_ssp_registe(audio_capture_t *ac, ci_ssp_st *ci_ssp, int module_num)
{
	g_ci_ssp_registe.audio_capture = ac;
	g_ci_ssp_registe.ci_ssp = (ci_ssp_st *)&g_ci_ssp_config;
	g_ci_ssp_registe.module_num = module_num;
	for (int i = 0; i < module_num; i++)
	{
		g_ci_ssp_registe.ci_ssp[i].module_config = ci_ssp[i].module_config;
	}
	//config cha num
	int cha_num = ciss_get(CI_SS_CHA_NUM);
	set_asr_cha_num(cha_num);

	//CI_ASSERT(cha_num <= CHANNEL_NUMS, "\n");
}

//alc_auto
void *ci_alc_auto_switch_module_init(void *module_config, void *wrapfft_audio_t)
{
	uint32_t size_start = get_alg_malloc_size() + get_remote_calloc_size();
	void *handle_ptr;
	handle_ptr = alc_auto_switch_create(module_config);
	if(!handle_ptr)
	{
		CI_ASSERT(0, "\n");
	}
	uint32_t size_stop = get_alg_malloc_size() + get_remote_calloc_size();
	uint32_t alc_auto_switch_version = get_alc_auto_switch_version();
	ci_logdebug(LOG_SSP_MODULE, "ci_alc_auto_switch_size = %d\n", size_stop - size_start);
	ci_logdebug(LOG_SSP_MODULE, "alc_auto_switch_version = %d\n", alc_auto_switch_version);
	return handle_ptr;
}
int ci_alc_auto_switch_processing(void *handle, void *wrapfft_audio_t)
{
	int ret = 0;
	//alc_auto_switch_config_t *alc_auto_switch_config = (alc_auto_switch_config_t *)handle;
	ci_wrapfft_audio *wrapfft_audio_st = (ci_wrapfft_audio *)wrapfft_audio_t;

	int frame_length = wrapfft_audio_st->iis_input_frame_len;
	status_t ci_ss_mic_voice_state = ciss_get(CI_SS_MIC_VOICE_STATUE); //mute状态
	status_t ci_ss_play_state = ciss_get(CI_SS_PLAY_STATE);			   //播报状态

	if ((CI_SS_MIC_VOICE_NORMAL == ci_ss_mic_voice_state) && (CI_SS_PLAY_STATE_IDLE == ci_ss_play_state))
	{
		ret = switch_alc_state_automatic(handle, wrapfft_audio_st->mic[0], frame_length);
	}
	reset_threshold_for_alc_switch(sg_denoise_alg_th);    //动态alc开关改变幅值大小影响门限值判断
	return ret;
}

//stft_istft模块初始化
void *ci_stft_module_init(void *module_config, void *wrapfft_audio_t)
{
	void *handle_ptr;
	handle_ptr = ci_stft_create(module_config, wrapfft_audio_t);
	if(!handle_ptr)
	{
		CI_ASSERT(0, "\n");
	}
	return handle_ptr;
}


int ci_stft_processing(void *handle, void *wrapfft_audio_t)
{
	int ret = ci_stft_deal(handle, wrapfft_audio_t);

	//给psd结果到CPU
	ci_wrapfft_audio *str = (ci_wrapfft_audio *)wrapfft_audio_t;

	if (32000 == str->module_config->sample_frequency)
	{
		float *fft_rslt = str->fft_mic_out[0];
		for (int i = 0; i < 15; i++)
		{
			float psd_cmpt = 0.0f;
			int index = i * 16 + 2;
			int cmpt_num = 1;
			for (int j = 0; j < cmpt_num; j++)
			{
				psd_cmpt += fft_rslt[2 * index] * fft_rslt[2 * index] + fft_rslt[2 * index + 1] * fft_rslt[2 * index + 1];
				index++;
			}
			psd_cmpt = psd_cmpt / (float)cmpt_num;
			g_fft_rslt_array[i] = psd_cmpt;
		}

		uint32_t data[2] = {0};
		data[0] = g_fft_rslt_array;
		data[1] = 15;
		REMOTE_CALL(deal_one_frm_fft_rslt_callback)
		((void *)data);
	}
	return ret;
}
//istft
void *ci_istft_module_init(void *module_config, void *wrapfft_audio_t)
{
	wrapfft_audio_t = ci_istft_create(module_config, wrapfft_audio_t);

	return wrapfft_audio_t;
}
int ci_istft_processing(void *handle, void *wrapfft_audio_t)
{
	int ret = ci_istft_deal(handle, wrapfft_audio_t);
	return ret;
}

void * ci_nn_noise_est_init(void *module_config, void *wrapfft_audio_t)
{
	uint32_t size_start = get_alg_malloc_size() + get_remote_calloc_size();
	void *handle_ptr;
	handle_ptr = noise_estimation_init(module_config);
	if(!handle_ptr)
	{
		CI_ASSERT(0, "\n");
	}
	uint32_t size_stop = get_alg_malloc_size() + get_remote_calloc_size();
	noise_est_config_t *noise_est_config = (noise_est_config_t *)module_config;
	uint32_t ai_noise_est_version = get_ai_noise_est_version();
	ci_logdebug(LOG_SSP_MODULE, "ci_ai_noise_est_size = %d\n", size_stop - size_start);
	ci_logdebug(LOG_SSP_MODULE, "ai_noise_est_version = %d\n", ai_noise_est_version);

	return handle_ptr;
}
void * ci_nn_noise_est_processing(void *handle, void *wrapfft_audio_t)
{
	float frame_energy = 0.0f; 
	ci_wrapfft_audio *wrapfft_audio_st = (ci_wrapfft_audio *)wrapfft_audio_t;
	compute_energy_in_frames(wrapfft_audio_st->mic[0],&frame_energy,AUDIO_CAP_POINT_NUM_PER_FRM);
	noise_estimation_deal(handle,frame_energy,(float *)&wrapfft_audio_st->noise_log_energy);
}

//传统降噪模块
void *ci_tra_denoise_module_init(void *module_config, void *wrapfft_audio_t)
{
	int ret = 0;
	uint32_t size_start = get_alg_malloc_size() + get_remote_calloc_size();
	void *handle_ptr = ci_denoise_create(module_config);
	if(!handle_ptr)
	{
		ciss_set(CI_SS_TRA_DENOISE_EN, 0);
		return NULL;
	}
	denoise_config_t *module_config_thr = (denoise_config_t *)module_config;
	//门限控制设置、初始化
	const alg_thr_config_t alg_thr_config = {
		.th_module = DENOISE,													  //算法模块判断
		.start_Hz = module_config_thr->start_Hz,								  //算法模块处理起始频率 单位Hz
		.end_Hz = module_config_thr->end_Hz,									  //算法模块处理降噪结束频率 单位Hz
		.alg_throshold = module_config_thr->set_denoise_threshold,				  //门限的值
		.set_alg_thr_window_size = module_config_thr->set_denoise_thr_window_size //判断窗长
	};
	sg_denoise_alg_th = (alg_th_s *)alg_th_create(&alg_thr_config);
	uint32_t size_stop = get_alg_malloc_size() + get_remote_calloc_size();
	uint32_t denoise_version = ci_denoise_version();
	ci_logdebug(LOG_SSP_MODULE, "tra denosie use mem size = %d\n", size_stop - size_start);
	ci_logdebug(LOG_SSP_MODULE, "denoise version = %d\n", denoise_version);
	return handle_ptr;
}

int ci_tra_denoise_processing(void *handle, void *wrapfft_audio_t)
{
	int ret = 0;
	void *denoise = handle;
	if(!ciss_get(CI_SS_TRA_DENOISE_EN))
	{
		return 0;
	}
	ci_wrapfft_audio *wrapfft_audio_st = (ci_wrapfft_audio *)wrapfft_audio_t;
	status_t ci_ss_mic_voice_state = ciss_get(CI_SS_MIC_VOICE_STATUE);
	if (CI_SS_MIC_VOICE_NORMAL == ci_ss_mic_voice_state)
	{
		bool enable_flag = ci_ssp_module_thr_control(sg_denoise_alg_th, wrapfft_audio_st->fft_mic_out[0]);
		if (enable_flag)
		{
			ret = ci_tra_denoise_deal(denoise, wrapfft_audio_st->fft_mic_out[0], wrapfft_audio_st->fft_mic_out[0]); //fft_输入_fft_输出
		}
	}
	return ret;
}

// nn_denoise模块
#if USE_NN_DENOISE
void *ci_nn_denoise_module_init(void *module_config, void *wrapfft_audio_t)
{
	int ret = 0;
	uint32_t size_start = get_alg_malloc_size() + get_remote_calloc_size();
	void *handle_ptr = ci_nn_denoise_create(module_config);
	if(!handle_ptr)
	{
		CI_ASSERT(0, "\n");
	}

	uint32_t size_stop = get_alg_malloc_size() + get_remote_calloc_size();
	uint32_t denoise_version = ci_nn_denoise_version();
	ci_logdebug(LOG_SSP_MODULE, "nn denosie use mem size = %d\n", size_stop - size_start);
	ci_logdebug(LOG_SSP_MODULE, "nn denoise version = %d\n", denoise_version);
	return handle_ptr;
}

int ci_nn_denoise_processing(void *handle, void *wrapfft_audio_t)
{
#if USE_AEC_MODULE
	if(ciss_get(CI_SS_AEC_WORK_STATE))
	return 0;
#endif
	int ret = 0;
	ci_wrapfft_audio *wrapfft_audio_st = (ci_wrapfft_audio *)wrapfft_audio_t;
	uint16_t frm_size = FREQ_SIZE * 2 * sizeof(float);
    memcpy(wrapfft_audio_st->ssp_dst_data,  wrapfft_audio_st->fft_mic_out[0], 256*2*sizeof(float));
	ret = ci_nn_denoise_deal(handle, wrapfft_audio_st->fft_mic_out[0], wrapfft_audio_st->fft_mic_out[0],wrapfft_audio_st->noise_log_energy);     //fft_输入_fft_输出
	return ret;
}
#endif

//tvad 参数初始化
void ci_nn_vad_module_init(void *module_config, void *wrapfft_audio_t)
{
	//mprintf("========tvad_init\n");
    tvad_handle tvad = NULL;
    tvad = get_tvad_handle(); //返回句柄
	uint32_t size_start = get_alg_malloc_size() + get_remote_calloc_size();
    tvad_init(tvad);
	tvad_config_t* m_tvad_config = (tvad_config_t*)module_config;
    uint8_t tvad_start_sen = m_tvad_config->tvad_start_sen;
    uint32_t tvad_on_timeout = m_tvad_config->tvad_on_timeout;
    uint8_t tvad_valid_num = m_tvad_config->tvad_valid_num;
    uint8_t tvad_end_delay = m_tvad_config->tvad_end_delay;
    int8_t tvad_timeout_enable = m_tvad_config->tvad_timeout_enable;
    float tvad_min_speech_energy = m_tvad_config->tvad_min_speech_energy;
	uint8_t vad_upload_mode = m_tvad_config->vad_upload_mode;
    set_tvad_sensitity_level(tvad_start_sen);
    tvad_set_out_time_end(tvad_on_timeout);
    tvad_set_max_valid_frame_num(tvad_valid_num);
    tvad_set_end_delay_frames(tvad_end_delay);
    tvad_set_end_delay_enable(tvad_timeout_enable);
    tvad_set_min_speech_energy(tvad_min_speech_energy);
	tvad_set_upload_mode(vad_upload_mode);
	uint32_t tvad_version = ci_tvad_version();
	uint32_t size_stop = get_alg_malloc_size() + get_remote_calloc_size();
	ci_logdebug(LOG_SSP_MODULE, "tvad use mem size = %d\n", size_stop - size_start);
	ci_logdebug(LOG_SSP_MODULE, "tvad_version = %d\n", tvad_version);
   
}

#if USE_NN_DOA || USE_GCC_DOA
//AI-DOA模块-新的doa模块
doa_apply_config_t g_doa_apply_config = {
	.doa_angle_out_type = DOA_WAKE_OR_CMD_OUT_ANGLE //默认命令词和唤醒词输出角度
};
/**************************
 * 配置doa输出角度类型
 * return: 0-成功  -1失败
 * **********************/
bool set_doa_out_type(int type)
{
	ci_logdebug(LOG_SYS_INFO, "set_doa_out_type = %d\r\n", type);
	if((type < DOA_WAKE_WORD_OUT_ANGLE) || (type > DOA_WAKE_OR_CMD_OUT_ANGLE))
	{
		return false;
	}
	else
	{
		g_doa_apply_config.doa_angle_out_type = type;
		return true;
	}
}

void *ci_doa_module_init(void *module_config, void *wrapfft_audio_t)
{
	uint32_t doa_version = 0;

	cm_set_codec_alc(HOST_MIC_RECORD_CODEC_ID, CM_CHA_LEFT, DISABLE);
	cm_set_codec_adc_gain(HOST_MIC_RECORD_CODEC_ID, CM_CHA_LEFT, 20);
	cm_set_codec_alc(HOST_MIC_RECORD_CODEC_ID, CM_CHA_RIGHT, DISABLE);
	cm_set_codec_adc_gain(HOST_MIC_RECORD_CODEC_ID, CM_CHA_RIGHT, 20);

	uint32_t size_start = get_alg_malloc_size() + get_remote_calloc_size();
	void *handle_ptr = NULL;
#if USE_NN_DOA
	handle_ptr = ci_nn_doa_create(module_config);
	if(!handle_ptr)
	{
		CI_ASSERT(0, "\n");
	}
	g_nn_doa_config = module_config;
	doa_version = ci_nn_doa_version();
#elif USE_GCC_DOA
	handle_ptr = ci_gcc_doa_create(module_config);
	if(!handle_ptr)
	{
		CI_ASSERT(0, "\n");
	}
	g_gcc_doa_config = module_config;
	doa_version = ci_gcc_doa_version();
#endif
	uint32_t size_stop = get_alg_malloc_size() + get_remote_calloc_size();
	ci_logdebug(LOG_SSP_MODULE, "doa_version = %d\n", doa_version);
#if !USE_AEC_MODULE
	ci_logdebug(LOG_SSP_MODULE, "doa use mem size = %d\n", size_stop - size_start);
#endif
	return handle_ptr;
}

int ci_doa_processing(void *handle, void *wrapfft_audio_t)
{
#if USE_CWSL && (USE_NN_DOA||USE_GCC_DOA)
	if (ciss_get(CI_SS_CWSL_IN_REG)) //如果在学习状态，直接返回
	{
		return 0;
	}
#endif
// #if USE_AEC_MODULE && (USE_NN_DOA || USE_GCC_DOA)
// 	if (ciss_get(CI_SS_DOA_INIT_STATE) == 1)
// 	{
// 		if (ciss_get(CI_SS_CUR_DOA_AEC_STATE) != 1)
// 		{
// 			ciss_set(CI_SS_CUR_DOA_AEC_STATE, 1);
// 			ciss_set(CI_SS_AEC_WORK_STATE, 0);
// 			#if USE_NN_DOA
// 			if (g_nn_doa_config)
// 			{
// 				ci_doa_module_init(g_nn_doa_config, NULL);
// 				nn_doa_param_t *ptr = get_nn_doa_param();
// 				if(ptr)
// 				{
// 					ptr->init_doa_param_flag = 1;
// 				}
// 			}
// 			else
// 			{
// 				ci_logdebug(LOG_SSP_MODULE, "error, g_nn_doa_config is null\n");
// 			}
// 			#elif USE_GCC_DOA
// 			if (g_gcc_doa_config)
// 			{
// 				ci_doa_module_init(g_gcc_doa_config, NULL);
// 				doa_s *ptr = get_gcc_doa_param();
// 				if(ptr)
// 				{
// 					ptr->init_doa_param_flag = 1;
// 				}
// 			}
// 			else
// 			{
// 				ci_logdebug(LOG_SSP_MODULE, "error, g_nn_doa_config is null\n");
// 			}
// 			#endif
// 		}
// 	}
// 	if (ciss_get(CI_SS_CUR_DOA_AEC_STATE) == 2)
// 	{
// 		return 0;
// 	}
// #endif
	ci_wrapfft_audio *wrapfft_audio_st = (ci_wrapfft_audio *)wrapfft_audio_t;
	int ret = 0;
	ret = ci_doa_deal_for_application(handle, wrapfft_audio_st->fft_mic_out, wrapfft_audio_st->mic[0]);

	return ret;
}

#else
bool set_doa_out_type(int type)
{
	
}

#endif //USE_NN_DOA

//DEVERB 模块
void *ci_dereverb_module_init(void *module_config, void *wrapfft_audio_t)
{
	uint32_t size_start = get_alg_malloc_size() + get_remote_calloc_size();

	cm_set_codec_alc(HOST_MIC_RECORD_CODEC_ID, CM_CHA_LEFT, DISABLE);
	cm_set_codec_adc_gain(HOST_MIC_RECORD_CODEC_ID, CM_CHA_LEFT, 20);
	cm_set_codec_alc(HOST_MIC_RECORD_CODEC_ID, CM_CHA_RIGHT, DISABLE);
	cm_set_codec_adc_gain(HOST_MIC_RECORD_CODEC_ID, CM_CHA_RIGHT, 20);
	void *handle_ptr = ci_dereverb_create(module_config);
	if(!handle_ptr)
	{
		CI_ASSERT(0, "\n");
	}
	uint32_t size_stop = get_alg_malloc_size() + get_remote_calloc_size();
	ci_logdebug(LOG_SSP_MODULE, "ci_dereverb_size = %d\n", size_stop - size_start);
	uint32_t dereverb_version = ci_dereverb_version();
	ci_logdebug(LOG_SSP_MODULE, "dereverb_version = %d\n", dereverb_version);
	return handle_ptr;
}
int ci_dereverb_processing(void *handle, void *wrapfft_audio_t)
{
    int ret = 0;
	ci_wrapfft_audio *wrapfft_audio_st = (ci_wrapfft_audio *)wrapfft_audio_t;

	if (CI_SS_PLAY_STATE_IDLE == ciss_get(CI_SS_PLAY_STATE)) //(enable_flag)
	{
		ret = ci_dereverb_deal(handle, wrapfft_audio_st->fft_mic_out, wrapfft_audio_st->fft_mic_out);
	}
	return ret;
}

//BF模块
void *ci_bf_module_init(void *module_config, void *wrapfft_audio_t)
{
	cm_set_codec_alc(HOST_MIC_RECORD_CODEC_ID, CM_CHA_LEFT, DISABLE);
	cm_set_codec_adc_gain(HOST_MIC_RECORD_CODEC_ID, CM_CHA_LEFT, 20);
	cm_set_codec_alc(HOST_MIC_RECORD_CODEC_ID, CM_CHA_RIGHT, DISABLE);
	cm_set_codec_adc_gain(HOST_MIC_RECORD_CODEC_ID, CM_CHA_RIGHT, 20);

	uint32_t size_start = get_alg_malloc_size() + get_remote_calloc_size();
	void *handle_ptr = ci_bf_create(module_config);
	if(!handle_ptr)
	{
		CI_ASSERT(0, "\n");
	}
	uint32_t size_stop = get_alg_malloc_size() + get_remote_calloc_size();
	ci_logdebug(LOG_SSP_MODULE, "ci_bf_malloc_size = %d\n", size_stop - size_start);

	uint32_t bf_version = ci_bf_version();
	ci_logdebug(LOG_SSP_MODULE, "bf_version = %d\n", bf_version);
	
	return handle_ptr;
}

int ci_bf_processing(void *handle, void *wrapfft_audio_t)
{
	int ret = 0;
	void *bf = handle;
	ci_wrapfft_audio *wrapfft_audio_st = (ci_wrapfft_audio *)wrapfft_audio_t;
	
	status_t ci_ss_play_state_bf = ciss_get(CI_SS_PLAY_STATE);
	int try_count = 0;
	bool enable_flag = ci_bf_module_thr_control(wrapfft_audio_st);
	if ((CI_SS_PLAY_STATE_IDLE == ci_ss_play_state_bf) && (enable_flag))
	{
		ret = ci_bf_deal(bf, wrapfft_audio_st->fft_mic_out[0], wrapfft_audio_st->fft_mic_out[1], wrapfft_audio_st->fft_mic_out);
	}
	return ret;
}

//aec模块
void *ci_aec_module_init(void *module_config, void *wrapfft_audio_t)
{
	uint32_t size_start = get_alg_malloc_size() + get_remote_calloc_size();
	void *handle_ptr = ci_aec_create(module_config);
	if(!handle_ptr)
	{
		CI_ASSERT(0, "\n");
	}
	g_aec_config = module_config;
	uint32_t size_stop = get_alg_malloc_size() + get_remote_calloc_size();
	uint32_t aec_version = ci_adapt_aec_version();
	ci_logdebug(LOG_SSP_MODULE, "aec_version = %d\n", aec_version);
#if !USE_NN_DOA && !USE_GCC_DOA
	ci_logdebug(LOG_SSP_MODULE, "aec use mem size = %d\n", size_stop - size_start);
#endif
	aec_config_t *aec_config1 = module_config;

	return handle_ptr;
}
//aec单帧耗时700us，加上vad通路，需要880us
int ci_aec_processing(void *handle, void *wrapfft_audio_t)
{
	#if 0//TIME_CONT_EN
    static uint8_t timer_init_flag = 1;
    if(timer_init_flag)
    {
        init_timer0();
        timer_init_flag = 0;
    }
    timer0_start_count();
    #endif
#if USE_CWSL && USE_AEC_MODULE
	if (ciss_get(CI_SS_CWSL_IN_REG)) //如果在学习状态，直接返回
	{
		return 0;
	}
#endif
	int ret = 0;
	ci_wrapfft_audio *wrapfft_audio_st = (ci_wrapfft_audio *)wrapfft_audio_t;
	bool aec_process_enable = ci_aec_application_enable(wrapfft_audio_st);
	if (aec_process_enable)
	{
		ret = ci_adapt_aec_deal(handle, wrapfft_audio_st->fft_mic_out, wrapfft_audio_st->fft_ref_out, wrapfft_audio_st->fft_mic_out,wrapfft_audio_st->ssp_dst_data);
	}
	else
	{
		/*nothing */
	}
	if(get_aec_alc_state())
	{
		wrapfft_audio_st =  set_aec_gain(wrapfft_audio_st);
	}


	return ret;
}

//音频输出模块
void *ci_iis_out_audio_module_init(void *module_config, void *wrapfft_audio_t)
{
	return module_config;
}
short *iis_audio_choose(audio_channel_model channel_model, ci_wrapfft_audio *wrapfft_audio_t)
{
	short *dst_data;
	//ci_logdebug(LOG_SYS_INFO, "channel_model = %d\r\n", channel_model);
	switch (channel_model)
	{
	case MICL: //mic左通道
	{
		dst_data = wrapfft_audio_t->mic[0];
		break;
	}
	case MICR: //mic右通道
	{
		dst_data = wrapfft_audio_t->mic[1];
		break;
	}
	case REFL: //ref左通道
	{
		dst_data = wrapfft_audio_t->ref[0];
		break;
	}
	case REFR: //ref右通道
	{
		dst_data = wrapfft_audio_t->ref[1];
		break;
	}
	case DST1: //数据处理后的通道
	{
		dst_data = wrapfft_audio_t->dst[0];
		//ci_logdebug(LOG_SYS_INFO, "DST1:dst_data=%p\n",dst_data);
		break;
	}
	case DST2: //数据处理后的通道，算法处理存在输出双通道的情况
	{
		dst_data = wrapfft_audio_t->dst[1];
		break;
	}
	default: //默认配置mic左通道
	{
		dst_data = wrapfft_audio_t->mic[0];
	}
	}
	if (NULL != dst_data)
	{
		//32k采样率数据通过采音板输出需要抽取转成16k数据
		if (32000 == wrapfft_audio_t->module_config->sample_frequency)
		{
			//32k数据进过istft处理后会降采样成16k采样率数据跳过抽取操作
			if (DST1 != channel_model && DST2 != channel_model)
			{
				int frame_len = wrapfft_audio_t->iis_input_frame_len / 2;
				for (int i = 0; i < frame_len; i++)
				{
					dst_data[i] = dst_data[2 * i];
				}
				//ci_logdebug(LOG_SYS_INFO, "DST1:dst_data=%p\n",dst_data);
			}
		}
	}
	else
	{
		ci_logdebug(LOG_SSP_MODULE, "iis_result_out_error:\n");
	}

	return dst_data;
}
int ci_iis_out_audio_processing(void *handle, void *wrapfft_audio_t)
{
	iis_out_audio_config_t *iis_out_audio = (iis_out_audio_config_t *)handle;
	ci_wrapfft_audio *wrapfft_audio_st = (ci_wrapfft_audio *)wrapfft_audio_t;
	int frame_len = wrapfft_audio_st->iis_input_frame_len;
	if (32000 == wrapfft_audio_st->module_config->sample_frequency)
	{
		frame_len /= 2;
	}

	if (iis_out_audio->iis_out_enable || iis_out_audio->uart_out_enable)
	{
		short *left_data, *right_data;
		audio_channel_model left_channel_model = iis_out_audio->iis_left_channel;
		audio_channel_model right_channel_model = iis_out_audio->iis_right_channel;

		left_data = iis_audio_choose(left_channel_model, wrapfft_audio_st);
		right_data = iis_audio_choose(right_channel_model, wrapfft_audio_st);

        //添加vad标签
		if (iis_out_audio->vad_mark_enable)
		{
			set_pcm_vad_mark_flag(left_data, frame_len);
		}
		uint32_t wrapfft_audio_addr = wrapfft_audio_st;
		audio_pre_rslt_write_data(left_data, right_data, wrapfft_audio_addr);   //采音频通过该接口输出
	}

	if ((iis_out_audio->ssp_dst_cover_micl_enble) && (NULL != wrapfft_audio_st->dst[0]))
	{

		MASK_ROM_LIB_FUNC->newlibcfunc.memcpy_p(wrapfft_audio_st->mic[0], wrapfft_audio_st->dst[0], sizeof(int16_t) * frame_len);
	}
}

//sed模块
#if USE_SED
void *ci_sed_module_init(void *module_config, void *wrapfft_audio_t)
{
	g_sed_config_para = (sed_config_t *)module_config;

	return g_sed_config_para;
}

int ci_sed_processing(void *handle, void *wrapfft_audio_t)
{
	int ret = 0;
	return ret;
}
#endif


void ci_ssp_init()
{

	//初始化前端算法
	int ssp_module_num = g_ci_ssp_registe.module_num;
	ci_ssp_st *ci_ssp_p = g_ci_ssp_registe.ci_ssp;

	g_wrapfft_audio = (ci_wrapfft_audio *)bnpu_remote_calloc(1, sizeof(ci_wrapfft_audio));
	if(!g_wrapfft_audio)
	{
		CI_ASSERT(0, "\n");
	}
	g_wrapfft_audio->mic_channel_num = g_ci_ssp_registe.audio_capture->mic_channel_num;
	g_wrapfft_audio->ref_channel_num = g_ci_ssp_registe.audio_capture->ref_channel_num;
	g_wrapfft_audio->iis_input_frame_len = g_ci_ssp_registe.audio_capture->frame_length;

	for (int i = 0; i < ssp_module_num; i++, ci_ssp_p++)
	{
		if (ci_ssp_p->func_init && ci_ssp_p->module_config)
		{
			ci_ssp_p->handle = ci_ssp_p->func_init(ci_ssp_p->module_config, g_wrapfft_audio);
		}
	}
}

void status_clear_ssp()
{
	ciss_set(CI_SS_CMD_STATE_FOR_SSP, CI_SS_CMD_IS_NULL);
}

int ci_ssp_processing()
{
	int ret = 0;
	int ssp_module_num = g_ci_ssp_registe.module_num;
	ci_ssp_st *ci_ssp_p = g_ci_ssp_registe.ci_ssp;

	//语音算法前端处理：stft_istft、aec、dr、bf、denoise、iis_out
	for (int i = 0; i < ssp_module_num; i++, ci_ssp_p++)
	{
		if (ci_ssp_p->module_config && ci_ssp_p->func_process)
		{
			if (ci_ssp_p->func_process == ci_aec_processing)
			{
				aec_config_t *p_aec_config = (aec_config_t *)ci_ssp_p->module_config;
				if (p_aec_config->alg_enable) //aec算法使能
				{
					ret += ci_ssp_p->func_process(ci_ssp_p->handle, g_wrapfft_audio);
				}
			}
			else
			{
				ret += ci_ssp_p->func_process(ci_ssp_p->handle, g_wrapfft_audio);
			}
		}
	}
	return 1;
}
