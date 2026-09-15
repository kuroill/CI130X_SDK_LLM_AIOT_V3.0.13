/**
  ******************************************************************************
  * @文件    temporal_vad_api.h
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

#ifndef  __TEMPORAL_VAD_API_H__
#define  __TEMPORAL_VAD_API_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C"{
#endif

//TVAD 参数
typedef struct 
{
    uint8_t tvad_start_sen;
    uint8_t tvad_end_delay;
    uint8_t vad_upload_mode;
    uint8_t tvad_valid_num;
    uint32_t tvad_on_timeout;
    int8_t tvad_timeout_enable;
    float tvad_min_speech_energy;
}tvad_config_t;

typedef enum      
{
    TD_VAD_OUT_OTHER   = 0,  // 外部获取的静音状态和其他的状态
    TD_VAD_OUT_START   = 1,  // 外部获取的 TVAD_START 标识
    TD_VAD_OUT_END     = 2,  // 外部获取的 TVAD_END 标识
}tvad_transition_t;  

struct vad_context ;
typedef struct vad_context *tvad_handle;

//获取句柄
tvad_handle get_tvad_handle(void);

void tvad_init(tvad_handle tvad);

void tvad_destory(tvad_handle tvad);
/**
 * @brief 进入和退出aec时切换一次参数
 * 
*/
void parameters_config_aec_normal(bool aec_flag);
/**
 * @brief  外界强制内部tvad为end状态
 * 
 * @return int 
 */
uint8_t frcoe_temporal_vad_toidle(void);

/**
 * @brief 设置tvad end 延迟帧数。也是后窗口大小
 * 
 * @param delay_frames 延迟帧数;取值在20~40
 * @return 0 : 修改成功；-1：修改失败
 */
int8_t tvad_set_end_delay_frames(const uint8_t delay_frames);

/**
 * @brief 设置tvad 云端上传模式
 * 
 * @param mode 1:前置检查，等待判断语音有效后，再回退语音上传 
 *             2:后置检查，优先实时上传语音，检查有效性结果后通知WIFI端
 * @return 0 : 修改成功；-1：修改失败
 */
int8_t tvad_set_upload_mode(const uint8_t mode);

/**
 * @brief 设置判断活跃语音有效的帧数阈值
 * 
 * @param valid_frame_num 有效帧数帧数;取值在18~35；
 * @return 0 : 修改成功；-1：修改失败
 */
int8_t tvad_set_max_valid_frame_num(const uint8_t valid_frame_num);


/**
 * @brief Set the tvad sensitity level object
 * 
 * @param level  灵敏度等级，设置0,1,2，对应低，中，高。灵敏度等级越高越灵敏，
 * @return uint8_t 0:设置成功；-1：设置失败
 */
uint8_t set_tvad_sensitity_level(uint8_t level);

/**
 * @brief 设置tvad end的超时时间
 * 
 * @param out_time_end_frames 超时时间;取值在100~15000;对应1s到15s；
 * @return 0 : 修改成功；-1：修改失败
 */
int8_t tvad_set_out_time_end(const uint32_t out_time_end_frames);

/**
 * @brief 获取触发的vad_start是否为正常
 * @note 检测当vad_start是否为真实活跃语音（可能是误触，阈值设为VAD_VALID_FRAMES帧）
 *  每一帧的vad是否有效必须先调用，vad_process_frame，进行计算
 * @param tvad 
 * @return uint8_t 0:处于无效状态；1：处于有效状态
 */

uint8_t get_voice_is_valid(tvad_handle tvad);
/**
 * @brief Get the tvad state object
 * 
 * @param tvad tvad状态
 * @param pcm pcm数据
 * @param len pcm长度
 * @return tvad_transition_t 
 */
tvad_transition_t get_tvad_state(tvad_handle tvad, const short * pcm, uint16_t len);

/**
 * @brief 设置tvad 最小人声能量阈值
 * 
 * @param enable_val 最小人声能量阈值;取值在40000~300000
 * @return 无
 */
int8_t tvad_set_min_speech_energy(const float min_energy);

/**
 * @brief 设置tvad end超时使能
 * 
 * @param enable_val 1:使能 0：不使能
 * @return 无
 */
void tvad_set_end_delay_enable(const int8_t enable_val);
#if !DEBUG_ZDT
bool calc_update_tvad_state(short *pcm_data, int pcm_data_size);
#endif

int ci_tvad_version(void);

#ifdef __cplusplus
}
#endif
#endif  /* __TEMPORAL_VAD_API_H__ */
