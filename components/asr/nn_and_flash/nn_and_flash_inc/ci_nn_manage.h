#ifndef __CI_NN_MANAGE_H
#define __CI_NN_MANAGE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef enum
{
    NN_TYPE_NAME_CINN = 0,
    NN_TYPE_NAME_ECAPA_TDNN,
    NN_TYPE_NAME_DOA_TDNN,
	NN_TYPE_NAME_DENOISE_NN,
    NN_TYPE_NAME_LSTM,
    NN_TYPE_NAME_DNN,
    NN_TYPE_NAME_GRU,
    NN_TYPE_NAME_CINN2,
    NN_TYPE_NAME_NULL,
}nn_type_name_t;


typedef enum
{
    NN_INIT_OPERTATE = 0,
    NN_RELEASE_OPERTATE,
    NN_COMPUTE_OPERTATE,
    NN_REQ_CLEAR,
}nn_operate_type_t;


typedef struct 
{
    uint8_t cinn_equipment_num;//cinn设备编号，有可能有多个cinn设备同时存在
    int cha_num;
}ci_nn_manage_cinn_compute_info_t;




typedef struct 
{
    int16_t* in_data;
    float* out_data
}ci_nn_ecapa_tdnn_info_t;


typedef struct 
{
    void* handle;
    float* fft_in;
    float* fft_out;
}ci_denoise_nn_cmpt_info_t;

typedef struct 
{
    void* handle;
    float** fft_in;
    float* fft_out;
    short *mic_l;
}ci_nn_doa_tdnn_cmpt_info_t;


typedef struct 
{
    uint8_t denoise_nn_equipment_num;//cinn设备编号，有可能有多个cinn设备同时存在
    ci_denoise_nn_cmpt_info_t denoise_nn_cmpt;
}ci_nn_manage_denoise_nn_compute_info_t;


typedef struct 
{
    nn_type_name_t nn_type;//NN种类：cinn、lstm、dnn、cnn、gru等
    nn_operate_type_t op_type;//NN控制类型：初始化、释放、计算
    union 
    {
        ci_nn_manage_cinn_compute_info_t cinn_cmp_info;
        ci_nn_ecapa_tdnn_info_t ecapa_tdnn_cmpt_info;
        ci_nn_doa_tdnn_cmpt_info_t doa_tdnn_cmpt_info;
		ci_nn_manage_denoise_nn_compute_info_t denoise_nn_cmpt_info;
    };
    //计算需要的一些信息:nn设备的编号（一直类型的NN可能会有多中类型）
}nn_ctr_msg_t;//nn控制消息的结构体;

#ifdef __cplusplus
}
#endif

#endif 