/**
  ******************************************************************************
  * @file    ci_doa_apply.h
  * @author  lt
  * @version V1.0.0
  * @date    2022.03.03
  * @brief 
  ******************************************************************************
  **/

#ifndef __CI_DOA_APPLY_H__
#define __CI_DOA_APPLY_H__

typedef enum
{
    DOA_WAKE_WORD_OUT_ANGLE = 1,   //唤醒词输出角度
    DOA_CMD_WORD_OUT_ANGLE,        //命令词输出角度
    DOA_WAKE_OR_CMD_OUT_ANGLE,     //唤醒词和命令词都都输出角度
} doa_angle_out_type_t;

typedef struct
{
    //初始化参数
    int doa_angle_out_type;
    int rad_num;
    int count_lenth;
    int doa_resolution;
}doa_apply_config_t;


int ci_doa_deal_for_application(void *doa, float **fft_in, short *mic_l);
bool set_doa_out_type(int type);    //设置doa角度输出类型

#endif  //__CI_DOA_APPLY_H__