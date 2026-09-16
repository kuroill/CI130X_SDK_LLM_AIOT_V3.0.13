/**
  ******************************************************************************
  * @file    ci_ssp_config.c
  * @version V1.0.0
  * @date    2021.08.17
  * @brief 
  ******************************************************************************
  */

#include <stdint.h>
#include <stdbool.h>

#include "alg_preprocess.h"
#include "ci_audio_wrapfft.h"
#include "ci_tra_denoise.h"
#include "ci_bf.h"
#include "ci_dereverb.h"
#include "alc_auto_switch.h"
#include "ci_adapt_aec.h"
#include "ci_gcc_doa.h"
#include "ci_nn_doa_init_param.h"
#include "ci_pwk.h"
#include "nn_denoise_api.h"
#include "sed_manage.h"
#include "ci_agc.h"
#include "sdk_default_config.h"
#include "status_share.h"
#include "temporal_vad_api.h"
#include "noise_energy_estimation_api.h"
//采音板音频输出配置
const iis_out_audio_config_t iis_out_audio_config =
{
	.alg_enable = true,
	.iis_out_enable = USE_IIS1_OUT_PRE_RSLT_AUDIO || AUDIO_DATA_UPLOAD_BY_IIS,			 //用于iis采音。
    .uart_out_enable = DEBUG_AUDIO_UART_UPLOAD_EN,			 //用于uart采音。
	.iis_left_channel = DST1,		 //向ESP输出AEC+ANS1-061处理后的单声道
	.iis_right_channel = DST1,		 //复制到两个physical slot，保持现有ESP I2S格式
	.vad_mark_enable = false,		 //是否附带vad标签，默认左通道输出vad标签。
	.ssp_dst_cover_micl_enble = false //处理过后的音频dst覆盖原始micl数据,16k采样数据
};
//alc_auto_switch模块配置
const alc_auto_switch_config_t alc_auto_switch_config =
{
	.alc_auto_frame_size = AUDIO_CAP_POINT_NUM_PER_FRM * 2,
	.thr_for_non_stationary_on = 10000.0f,			//非稳态噪声下对应的上门限
	.thr_for_non_stationary_off = 3000.0f,			//非稳态噪声下对应的下门限
	.thr_for_stationary_on = 7000.0f,				//稳态噪声下对应的上门限
	.thr_for_stationary_off = 1000.0f,				//稳态噪声下对应的下门限
	.alc_auto_switch_ban_mode = NON_STATIONARY_BAN, //默认非稳态噪声下严格控制，尽可能不进行alc_auto_switch操作。
	.alc_off_codec_adc_gain = 20
};


//stft_istft模块配置
const stft_istft_config_t stft_istft_config =
{
//#if 512 == FFT_MODEL
	.alg_enable = true,
#if !INNER_CODEC_AUDIO_IN_USE_RESAMPLE
	.sample_frequency = 16000, //采样率
	.frame_size = 512,		   //窗长
	.frame_shift = AUDIO_CAP_POINT_NUM_PER_FRM,		   //帧移
	.fft_frm_size = 512,	   //fft输入有效长度的点数
	.fft_size = 257,		   //fft的正频分量点数-1+直流分量点数
	.result_out_channel = 1,   // 输出通道数result_out_channel;
	.psd_compute_channel_num = 0,//计算mic的psd
	.time_pre_emphasis_enable = false,
	.downsampled_enable = false, 
	.fe_psd_enable = false		 //是否计算psd，以计算特征
#else
	.sample_frequency = 32000, //采样率
	.frame_size = 1024,		   //窗长
	.frame_shift = AUDIO_CAP_POINT_NUM_PER_FRM * 2,		   //帧移
	.fft_frm_size = 1024,	   //fft输入有效长度的点数
	.fft_size = 513,		   //fft的正频分量点数-1+直流分量点数
	#if DEEP_SEPARATE_ENABLE
	.result_out_channel = 2,   // 输出通道数result_out_channel;
	.psd_compute_channel_num = 1,//计算原mic的psd
	#else
	.result_out_channel = 1,   // 输出通道数result_out_channel;
	.psd_compute_channel_num = 0,//计算原mic的psd
	#endif
	.time_pre_emphasis_enable = false,
	.downsampled_enable = true, 
	#if DEEP_SEPARATE_ENABLE
	.fe_psd_enable = true		//是否计算psd，以计算特征
	#else
	.fe_psd_enable = false		//是否计算psd，以计算特征
	#endif
#endif
};

//就近唤醒模块配置
pwk_config_t ci_pwk_config = 
{
	.alg_enable = true
};

//传统denoise模块配置
const denoise_config_t tra_denoise_config =
{
	.alg_enable = true,
	.start_Hz = 0,	          //降噪起始频率 单位Hz
	.end_Hz = 0,			  //降噪结束频率 单位Hz,降噪范围变大，需要调大TRA_DENOISE_USE_BNPU_SRAM_SIZE内存
	.fre_resolution = 31.25f, //频率分辨率 单位Hz
	.aggr_mode = 1,            //算法处理的效果等级:0，1，2，处理效果依次增强，失真也会变大
	.set_denoise_threshold = 7000.0f,	  //默认帧平均幅值>=7000起效
	.set_denoise_thr_window_size = 20 //门限判断窗长
};

#if DENOISE_STRENGTH_ADAPT_ENABLE
//NN噪声估计模块配置
noise_est_config_t nn_noise_est_config = 
{
	.noise_est_debug    = false,             //噪声估计调试打印使能，在降噪算法自适应beta模式下，首先调试打印判断需要开启beta自适应的最小阈值，小于该阈值则使用默认降噪强度
};
#endif
//NN 降噪模块配置
denoise_nn_config_t nn_denoise_config = 
{
	.alg_enable = true,
	.alpha_forget = 0.75f,
	.denoise_mode = 0,
	.denoise_beta = 1.0f,					//降噪强度系数范围（0.0 < beta <= 1.0）1.0为默认降噪强度（完全的降噪效果），beta越小降噪强度越低

#if DENOISE_STRENGTH_ADAPT_ENABLE
	.denoise_beta_adaptive_mode =  true,	//beta自适应模式，配合NN噪声估计模块使用
#else
	.denoise_beta_adaptive_mode =  false,	//beta自适应模式，配合NN噪声估计模块使用
#endif
	.denoise_ratio_upper_limit 	=  1.0f,	//beta上限值
	.denoise_ratio_lower_limit 	=  0.87f,	//beta下限值,推荐大于0.85
	//由以下2个阈值均分成4个噪声能量对数区，每个分区使用 beta上下限均分的4个beta值;让高噪环境使用小beta保留人声，低噪环境使用大beta完全相信降噪能够保留人声
	//大于denoise_energy_highest_thr或者在4个分区中最大的分区都使用beta上限值，小于denoise_energy_lowest_thr则beta使用1.0
	.denoise_energy_highest_thr =  90.0f,	//噪声能量对数最高阈值
	.denoise_energy_lowest_thr  =  75.0f,	//噪声能量对数最低阈值
};

//哭声检测模块
const sed_config_t sed_config_cry = 
{
	.alg_enable = true,
	.threshold = 0.55f,
	.mode = 1,
	.times = 3
};
//鼾声检测模块
const sed_config_t sed_config_snore = 
{
	.alg_enable = true,
	.threshold = 0.55f,
	.mode = 1,
	.times = 3
};

//GCC DOA模块配置
const gcc_doa_config_t gcc_doa_config =
{
	.alg_enable = true,
	.distance = 40,      //麦克风间距范围30~80mm
	.min_frebin = 40,    //最小频点（不可改参数）
	.max_frebin = 110,   //最大频点（不可改参数）
	.samplerate = 16000, //采样率（不可改参数）
	.corr_alpha = 0.1f,   //互相关矩阵平滑参数（不可改参数）
	.num_frebin = 15,    //挑选频点个数（不可改参数）
	.denoise_en = false, //false:为不开启降噪，true:为开启降噪
	.energy_thr = 0.2f,   //能量阈值,范围0~1，建议步长为0.1
	.reverb_miu = 0.1f,   //降混响算法的步长（不可改参数）
	.doa_resolut = 8,    //doa分辨率（不可改参数）

	/*麦间距(mm)对应的scale配置值如下所示
	distance | distance_scale
		30	 |	   0.6f
		40	 |	   0.7f		
		50	 |	   0.75f
		60	 |	   0.8f
		70	 |	   0.7f			
		80	 |	   0.65f					
	*/
	.distance_scale = 0.7f         //导向向量补偿因子 
};

//NN DOA模块配置
const nn_doa_config_t nn_doa_config = 
{
	.eliminate62_5 = 2            // 消除62.5Hz以下低频噪声影响，0：关闭，2：开启
};

//降混响模块配置,startHZ:算法起效的起始频率  endHZ:算法起效的结束频率  范围:0-8KHZ,调大会增加一定的算法力和内存消耗
//160HZ-4800HZ 消耗28KB内存  0-8000HZ 消耗49KB内存
const dereverb_config_t dereverb_config =
{
	.alg_enable = true,
#if DEREVERB_FREQ_RANGE_INDEX
	.startHz = 0.0f,
	.endHz = 8000.0f
#else
    .startHz = 160.0f,
	.endHz = 4800.0f
#endif
};

//BF模块配置
const bf_config_t bf_config =
{
	.alg_enable = true,
	.distance = 40,
	.angle = 90,
	.freq = 20,
	.frame_wkup = 100,
	.frame_rt = 40,
	.wkup_result_thr = 45,     //ASR分值的阈值，值设置的越大，误识会降低(肯能也会影响识别)，根据具体项目去调整
	.bf_deepse_mode = BF_DEEPSE_MODE,
	.bf_asr_valid_mode = BF_ASR_VALID_MODE
};

// AEC模块配置
const aec_config_t aec_config =
{
	.alg_enable = true,
	#if (USE_BEAMFORMING_MODULE && BF_DEEPSE_MODE)
	.mic_channel_num = 2,						   //麦克风信号通道数
	#else
	.mic_channel_num = 1,
	#endif
	.ref_channel_num = 1,						       //参考信号通道数
	.aec_control_mode = ENABLE_PLAYING_STATE_MODE,     //ENABLE_PLAYING_STATE_MODE:根据播报状态进行aec控制;  COMPUTE_REF_AMPL_MODE:根据参考幅值大小进行aec控制
	.aec_gain = 1.0f,							       //增益数值
	.aec_enable_threshold = 2000.0f,	               //参考信号判断门限值  
	/* Cascade NLP mode 2 and mode 1 to suppress the residual playback echo
	 * that otherwise survives long enough to satisfy the barge-in confirmer. */
	.nlp_flag = 3,
	.aggr_mode = 1,
	.fft_size = 256,	   //频域处理频点数
	/*AEC处理时使用的增益*/
	.alc_off_codec_adc_gain_mic = 20,    //可调，单双麦都使用该增益
    .alc_off_codec_adc_gain_ref = 4,     //可调,仅使用内部codec作参考回路时使用，外部codec需在es7243e_init函数中设置alc_cfg_str.max_gain值
	.dtd_ratio = 1.0f,
};

//vox模块配置
vox_config_t vox_config = 
{
	//.agc_split_boundary = VOX_VAD_THRE_DB_DEFAULT,  	//判断语音的阈值 45~60dB；高灵敏度 45~48dB 默认48dB；中灵敏度 49~52dB 默认50dB；低灵敏度 53~60dB 默认53dB
	
    .agc_gate_h = -4000.0f, 
    .agc_gate_l = -6500.0f,
    .agc_gate_end = -3000.0f,

	.vad_timeout_enable = VAD_TIMEOUT_CHECK,		// vad算法内部是否进行超时检测
	.vad_on_max_timeout = VAD_FORCE_OVER_NUM_TIME,	// 强制结束录音的时间(单位S)
};
//nn vad模块配置
tvad_config_t nn_vad_config = 
{
	.tvad_start_sen = NN_VAD_SENSITIVITY,			//灵敏度等级，设置0,1,2，对应低，中，高。灵敏度等级越高越灵敏;配合tvad_valid_num调试TVAD的灵敏度
	.tvad_end_delay = NN_VAD_END_DELAY,				//后窗口大小， TD_VAD_ON2IDLE 最多持续的帧数 ；取值20~40
	.vad_upload_mode = 1,							//VAD语音有效性检查模型：1-前置检查，等待判断语音有效后，再回退语音上传 2-后置检查，优先实时上传语音，检查有效性结果后通知WIFI端
	.tvad_valid_num = NN_VAD_VALID_NUM,    			//TD_VAD_ON 状态的至少满足的有效帧数 ，低于该阈值识别结果无效 
	.tvad_on_timeout = VAD_FORCE_OVER_NUM_TIME*100,	//vad on连续持续帧数，每帧10ms
	.tvad_timeout_enable = VAD_TIMEOUT_CHECK,		// vad算法内部是否进行超时检测
	.tvad_min_speech_energy = 40000.0f,				//最小人声能量,根据mic增益调增,范围24000.0f~550000.0f对应40db~50db人声，越小支持的拾音越远；
};
//.module_config域配置为NULL表示关闭此项。
ci_ssp_config_t ci_ssp = {
	#if USE_ALC_AUTO_SWITCH_MODULE
	.alc_auto_switch = 	{.module_config = &alc_auto_switch_config},
	#else
	.alc_auto_switch = 	{.module_config = NULL},
	#endif

	#if USE_NN_DENOISE && DENOISE_STRENGTH_ADAPT_ENABLE
	.nn_noise_est = 	{.module_config = &nn_noise_est_config},
	#endif

	.stft = 			{.module_config = &stft_istft_config},
	#if USE_NN_DOA
	.doa = 				{.module_config = &nn_doa_config},
	#elif USE_GCC_DOA
	.doa =              {.module_config = &gcc_doa_config},
	#else
	.doa = 				{.module_config = NULL},
	#endif

	#if USE_AEC_MODULE
	 .aec = 			{.module_config = &aec_config},
	#else
	.aec = 				{.module_config = NULL},
	#endif

	#if USE_DEREVERB_MODULE
	.dereverb = 		{.module_config = &dereverb_config},
	#else
	.dereverb = 		{.module_config = NULL},
	#endif

	#if USE_BEAMFORMING_MODULE
	.bf = 				{.module_config = &bf_config},
	#else
	.bf = 				{.module_config = NULL},
	#endif

	#if USE_NN_DENOISE
	.nn_denoise = 		{.module_config = &nn_denoise_config},
	#elif USE_TRA_DENOISE
	.tra_denoise = 		{.module_config = &tra_denoise_config},
	#else
	.nn_denoise = 		{.module_config = NULL},
	#endif
	#if USE_NN_VAD
	.nn_vad  =          {.module_config = &nn_vad_config},
	#else
	.nn_vad  =  		{.module_config = NULL},
	#endif
	#if USE_PWK
	.pwk =              {.module_config = &ci_pwk_config}, 
	#else
	.pwk = 				{.module_config = NULL},
	#endif

	.istft = 			{.module_config = &stft_istft_config},
	.iis_out_audio = 	{.module_config = &iis_out_audio_config},

	#if USE_SED_CRY
	.sed = 				{.module_config = &sed_config_cry},
	#elif USE_SED_SNORE
	.sed = 				{.module_config = &sed_config_snore},
	#else
	.sed = 			    {.module_config = NULL},
	#endif

};


audio_capture_t audio_capture =
{
    #if INNER_CODEC_AUDIO_IN_USE_RESAMPLE
    .frame_length = AUDIO_CAP_POINT_NUM_PER_FRM * 2, // frame_length; 32k采样，输入点数为320
    #else
    .frame_length = AUDIO_CAP_POINT_NUM_PER_FRM, // frame_length;16k采样，输入点数为160
    #endif

    #if USE_BEAMFORMING_MODULE  || USE_NN_DOA || USE_GCC_DOA || USE_DEREVERB_MODULE || USE_DUAL_MIC_ANY
    .mic_channel_num = 2, // mic_channel_num;
    #else
    .mic_channel_num = 1,                        // mic_channel_num;
    #endif

    #if USE_AEC_MODULE
    .ref_channel_num = 1,                        //ref_channel_num;
    #else
    .ref_channel_num = 0,                        //ref_channel_num;
    #endif
};


