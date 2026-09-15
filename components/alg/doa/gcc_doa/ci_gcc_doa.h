/**
  ******************************************************************************
  * @file    ci_gcc_doa.h
  * @version V1.0.0
  * @date    2019.07.09
  * @brief 
  ******************************************************************************
  **/
#ifndef __CI_GCC_DOA_H__
#define __CI_GCC_DOA_H__
#include "bnpu_mem_manage.h"
#include "bnpu_math.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>

// the version is 1.00.00
#define CI_DOA_VERSION 10415
#define INTERNAL_DEVELOPER_VERSION 20220302

#define VOICE_SPEED_DEFAULT (343)
#define DELTA_DEFAULT (1.0e-7f)
#define LAGNUM_DEFAULT (8)
#define L_P_DEFAULT (2)
#define MAX_FRAME_BUFFER (6) // (40)
#define FRAME_SIZE (512)
#define FRAME_SHIFT (256)
#define CI_DOA_CHANNEL_CNT (2)
#define M_PI (3.1415926f)
#define FRAME_CNT (30)
#define DIFF_SCORE_THRESH (0.005)
#define MAX_SCORE_THRESH (0.35)
#define NOISE_THRESH_HIGH (35)
#define NOISE_THRESH_LOW (10)

#define MY_ABSC(a) hard_fsqrt((a).real *(a).real + (a).image * (a).image)
#define MY_MEMMOVE(dst, src, n) memmove((dst), (src), (n) * sizeof(*(dst)) + 0 * ((dst) - (src)))
#define CHECK_MALLOC_RETURN_IF_FAILED(a) \
    do                                   \
    {                                    \
        if (a)                           \
        {                                \
            return -1;                   \
        }                                \
    } while (0)

/**
 * @addtogroup doa
 * @{
 */

typedef struct
{
    bool alg_enable;
    //初始化参数
    int distance;
    int min_frebin;
    int max_frebin;
    int samplerate;
    float corr_alpha;
    int num_frebin;
    bool denoise_en;
    float energy_thr;
    float reverb_miu;
    int doa_resolut;
    float distance_scale;
} gcc_doa_config_t;

typedef struct
{
    int ang_idx;
    int ang_buf_full;
    float ang_buf[MAX_FRAME_BUFFER];
} ang_buf_s;

typedef struct
{
    ang_buf_s ang_buf;
    Complex **AA;
    Complex **R_Corr;
    Complex **refVec;
    Complex **W_Matr;
    Complex **R_Corr_1;
    Complex **R_Corr_2;
    float *Cor_abs;
    float *fft_ns1;
    float *fft_ns2;
    short *abs_indx;
    float rad;
    float Corr_psd;
    float alpha_default;
    float energy_thresh;
    float miu_default;
    /* Basic parameters */
    int nFFT;
    int num_channel;
    int num_frame;
    int sample_rate;
    int num_frq;
    bool doa_denoise_en;
    /* Parameters of doa */
    int numRef;
    int min_frq_doa_band;
    int max_frq_doa_band;
    int total_doa_band;
    float scale;
    uint8_t init_doa_param_flag; //初始化doa参数标记
} doa_s;
#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief  获取算法版本信息
     * 
     * @return int 版本id, 整数型，例如返回10000, 表示版本号为 1.0.0;
     */
    int ci_doa_version(void);

    /**
     * @brief  初始化当前算法模块
     * 
     * @param module_config    双麦DOA算法配置文件
     * @return void*      若返回空NULL，则表示模块创建失败，否则表示创建成功;
     */
    void *ci_gcc_doa_create(void *module_config);
    int ci_gcc_doa_deal(void *handle, float **fft_in);
    int ci_get_doa_angle(void);
    doa_s *get_gcc_doa_param(void);
    float judge_output_angle(float *score, uint32_t valid_frame_cnt, float *frame_cnt_energy); //doa后验判断输出角度
    void doa_score_deal(float *rslt_score, float *frame_value, int *lag_index);                //doa打分函数
#ifdef __cplusplus
}
#endif

/** @} */

#endif /* __CI_GCC_DOA_H__ */
