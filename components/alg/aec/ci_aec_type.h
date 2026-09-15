/**
 * @file ci_adapt_aec.h
 * @brief
 * @version V1.0.0
 * @date 2019.07.09
 *
 * @copyright Copyright (c) 2019 Chipintelli Technology Co., Ltd.
 *
 */
#ifndef CI_AEC_TYPE_H
#define CI_AEC_TYPE_H

#include "ci_adapt_aec.h"
#include "romlib_runtime.h"



#define SAFE_DENOMINATOR        (1.0e-6f)
#define DIV_PCM_AEC             (1.0f/32767.0f )
#define NOT_USE_ARM_MATH_AEC    (0)
#define LAEC_ALPHA_INIT         (0.92f)
#define LAEC_DR_ALPHA_INIT      (0.999f)
#define LINE_PRE_NUM_INIT       (4)//linear prediction线性预测参数
#define DR_SIZE_INIT            (2)//降混响线性预测参数

//DTD_PARA
#define AEC_GAIN_SCALE_1        (3.0f)
#define AEC_GAIN_SCALE_2        (1.3f)
#define AEC_GAIN_SCALE_3        (0.1f)
#define AEC_GAIN_SCALE_4        (0.2f)
#define AEC_GAIN_SCALE_5        (0.35f)
#define AEC_GAIN_SCALE_6        (0.55f)
#define AEC_GAIN_SCALE_7        (0.7f)

#define AEC_GAIN_SCALE_VAD_1        (8.5f)
#define AEC_GAIN_SCALE_VAD_2        (3.5f)
#define AEC_GAIN_SCALE_VAD_3        (0.1f)
#define AEC_GAIN_SCALE_VAD_4        (0.3f)
#define AEC_GAIN_SCALE_VAD_5        (0.6f)
#define AEC_GAIN_SCALE_VAD_6        (1.4f)
#define AEC_GAIN_SCALE_VAD_7        (2.0f)


#define CORR_THR_SINGLE         (1.01f)
#define CORR_THR_DOUBLE         (1.1f)

#define AEC_XD_ALPHA_1          (0.45f)
#define COH_DTD_ALPHA           (0.98f)
 //NLP_PARA
#define CI_NLP_VERSION          (10205)
#define OPT_ENABLE              (1) 


typedef enum {
    STATE_UNCERTAIN = 0,           //0
    STATE_UNCERTAIN_TO_SINGLE,     //1
    STATE_UNCERTAIN_TO_DOUBLE,     //2
    STATE_SINGLE_TALK,             //3
    STATE_DOUBLE_TO_SINGLE,        //4
    STATE_SINGLE_TO_DOUBLE,        //5
    STATE_DOUBLE_TALK,             //6
}TALK_STATE;

typedef struct
{
	float real;
	float image;
} complex_aec;

/**
 * @brief NAEC模式
 *
 */
typedef enum
{
	NAEC_MODEL_NOT_USE = 0,             /*!< 不使用非线性处理模块           */
	NAEC_MODEL_USE_WNLP,                /*!< 使用维纳滤波的非线性处理模块   */
	NAEC_MODEL_USE_SNLP,                /*!< 使用SBSS的非线性处理模块       */
	NAEC_MODEL_USE_SNLP_WNLP,           /*!< 使用SBSS和维纳滤波级联的非线性处理模块   */
}NAEC_MODEL;

typedef  struct
{
	complex_aec** fft_ref_p_out;
	float** frame_date;
	short* window;
	//complex_aec*** W;
	complex_aec** En;
	complex_aec** W0;
	complex_aec** R;
	complex_aec** dW;
	float* tmp_powf_mic;
	float eta;
	int p;//expansion order ,3
	int p_out_num;
	int nlp_flag;

}ci_nlp_s;

typedef  struct
{
	complex_aec** ref_delay;     //用来进行回声预测的参考信号组成的向量，它乘上一个预测向量，求得回声估计信号;    

	ci_nlp_s* nlp_s;
	
    /* Basic parameters */

	aec_config_t *aec_config;
    
    int n_fft;

	int ref_delay_num;
	int mic_delay_num;
	int total_delay_num;

	//Kalman滤波参数
	complex_aec* I;
	complex_aec* Err;
	complex_aec* P1;
	complex_aec* K;
	complex_aec* fai_e;
	complex_aec(*P)[3];
	complex_aec(*g)[3];
	
	//Wiener滤波参数
	float* faiE;
	float* faiS;
	float* faiR;
	float* GG;
	float* faiS_vad;
	float* GG_vad;

	//DTD
	float* ref_pwr_avg;
	float* mic_pwr_avg;
	float* xd_pwr_avg;
	int start_frebin;
	int end_frebin;
}ci_aec_s;

/** @} */

#endif /* CI_AEC_TYPE_H */
