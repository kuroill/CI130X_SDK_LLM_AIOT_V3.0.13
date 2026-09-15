/**
  ******************************************************************************
  * @文件    noise_energy_estimation_api.h
  * @版本    V1.0.0
  * @日期    2026-04-29
  * @概要    vad
  ******************************************************************************
  * @注意
  *
  * 版权归chipintelli公司所有，未经允许不得使用或修改
  *
  ******************************************************************************
  */ 

#ifndef  NOISE_ENERGY_ESTIMATION_API_H
#define  NOISE_ENERGY_ESTIMATION_API_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"{
#endif

typedef struct
{
	bool noise_est_debug;
}noise_est_config_t;

typedef struct
{
    float *noise_en_save;
    float *win_size_energy;
    float min_engergy;

    float noise_energy;
    uint8_t save_idx ;
    uint8_t min_smooth_save_idx ;
    float short_energy;  
}noise_estimation_t;


/**
 * @brief 初始化噪声估计
 * 
 * @return int8_t 
 */
void* noise_estimation_init(void *module_config);

/**
 * @brief 计算一帧的能量
 * 
 * @param pcm 一帧的pcm数据集
 * @param en  保存一帧的能量
 * @param len 数据长度
 */
void compute_energy_in_frames(const short * pcm, float *en, uint16_t len);

/**
 * @brief 获取噪声能量
 * 
 * @param energy 当前帧能量
 * @param noise_log_energy 返回估计的噪声对数能量
 * @return int 
 */
int noise_estimation_deal(void *handle, float energy , float *noise_log_energy);


#ifdef __cplusplus
}
#endif
#endif  /* NOISE_ENERGY_ESTIMATION_API_H */
