#include "ci130x_npu_instr.h"
#include "ci130x_system.h"
#include <stdbool.h>

#ifndef __CI130X_NPU_H
#define	__CI130X_NPU_H


typedef enum
{
    CI_NPU_OP_SIGNED = 1,            //当前操作是有符号的
    CI_NPU_OP_UNSIGNED = 0,          //当前操作是无符号的
}ci_npu_op_is_signed_t;


typedef enum
{
    CI_NPU_W_BUFFER_INDEX_A = 0,     //A buffer
    CI_NPU_W_BUFFER_INDEX_B = 1,     //B buffer
	CI_NPU_W_BUFFER_INDEX_ERR = 2,   //错误的buffer
}ci_npu_w_buffer_index_t;


typedef enum
{
    CI_NPU_BATCH_OUT_8BIT = 0,
    CI_NPU_BATCH_OUT_16BIT = 1,
    CI_NPU_BATCH_OUT_32BIT_INT = 2,
    CI_NPU_BATCH_OUT_FLOAT = 3,
}ci_npu_batch_out_mode_t;


void npu_load_w_int_en(FunctionalState cmd);
uint32_t npu_load_w_int_state(void);
void npu_load_w_int_clear(void);
void npu_operate_x_signed_ctr(ci_npu_op_is_signed_t is_signed);
void npu_operate_w_signed_ctr(ci_npu_op_is_signed_t is_signed);
void npu_w_buffer_num_set(ci_npu_w_buffer_index_t buf_num);
void cinn_model_load_down_call_back(void);
void npu_float_rslt_en(FunctionalState cmd);
void npu_float_to_int(float max_val,float min_val,float dqt,uint32_t src,uint16_t dst,uint16_t size,ci_npu_batch_out_mode_t mode);


/**
 * @brief 根据查找表判断该位是否有效
 *
 * @param lut_base_addr 查找表基地址
 * @param num 检查的是第几个数是否有效
 * @return true 该点有效
 * @return false 该点无效
 */
static inline bool get_n_is_valid_from_lut(uint32_t lut_base_addr,uint32_t num)
{
	uint32_t* lut_p = (uint32_t*)lut_base_addr;
	int idx = num/32;
	int bit = num%32;
	if(lut_p[idx] & (1 << bit))
	{
		return true;
	}
	else
	{
		return false;
	}
}

uint32_t get_output_num_from_lut(uint32_t lut_addr,uint32_t num);
void npu_normal_quant(uint16_t size,uint32_t src,uint16_t dst,float dqt,ci_npu_batch_out_mode_t out_mode);
void npu_relu_max_config(FunctionalState cmd,float max_value);
void npu_relu_quant(uint16_t size,uint32_t src,uint16_t dst,float dqt,ci_npu_batch_out_mode_t out_mode);
void npu_pack_wrap_config(uint16_t dst_addr,uint16_t cnt,bool gate,bool bias_gate,bool dynamic_relu);
void softmax_float_to_int16(float max_val,float min_val,float dqt,uint32_t src,uint16_t dst,uint16_t size);
void softmax_float_to_int16_any_size(float max_val,float min_val,float dqt,uint32_t src,uint16_t dst,uint16_t size);
void npu_memcpy(void* dst,void* src,uint32_t size);
uint32_t tcm_buffer_alloc(uint16_t size);

void npu_w_buffer_load_offset_config(bool en);
void ci_npu_load_wt(uint8_t buffer_num,unsigned short size, unsigned int src_addr);
void npu_8_half_8_byte_mac_pack_enable(FunctionalState cmd);

void ci_npu_float_to_int16_any_size(uint32_t src_addr,uint32_t dst_addr,uint32_t num,
                            float max_level,float min_level,float scale);

#endif
