/**
 * @file ci130x_npu_instr.h
 * @author xushuai (shuai.xu@chipintelli.com)
 * @brief 3代npu 指令集软件函数实现
 * @version 0.1
 * @date 2026-02-03
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#ifndef __CI130X_NPU_INSTR_H
#define	__CI130X_NPU_INSTR_H

#include "ci130x_system.h"
#include "sdk_default_config.h"
#include "NN_memmap.h"

#define NUM_HEX_COV_TO_FLOAT_1(x) (*(float*)(&(x)))

/**
 * @brief 读寄存器
 * 
 * @param addr NN内部寄存器的偏移地址
 * @return int32_t 寄存器值
 */
static inline uint32_t npu_csr_rd(uint8_t addr)
{
	int32_t rd ;
	
	// .insn r opcode, {xd,xs1,xs2}, func7 , rd, rs1, rs2
	asm volatile (
		".insn r 0x0b, 0x6, 0x00, %0, %1, x0"
		:"=r"(rd)
		:"r"(addr)
	);

	return rd ;
}


/**
 * @brief 写寄存器
 * 
 * @param addr NN内部寄存器的偏移地址
 * @param wdata 需要写的寄存器的值
 * @return int32_t 无意义
 */
static inline uint32_t npu_csr_wr(uint8_t addr, uint32_t wdata)
{
	int32_t rd ;
	
	// .insn r opcode, {xd,xs1,xs2}, func7 , rd, rs1, rs2
	asm volatile (
		".insn r 0x0b, 0x7, 0x01, %0, %1, %2"
		:"=r"(rd)
		:"r"(addr), "r"(wdata)
	);
	__asm volatile("fence");
	return rd ;
}


/**
 * @brief 加载数据（从系统RAM到NN内部的TCM偏移地址）
 * 
 * @param size 数据长度（byte）
 * @param tcm_ptr NN内部TCM偏移地址
 * @param src_addr 系统RAM地址
 * @return int32_t 无意义
 */
static inline uint32_t npu_load(uint16_t tcm_ptr,uint32_t src_addr,uint32_t size)
{
	int32_t rd;
	uint32_t rs1 = (size << 16) | (tcm_ptr - NPU_OFFSET_ADDR);
	asm volatile (
		".insn r 0x0b, 0x7, 0x08, %0, %1, %2"
		:"=r"(rd)
		:"r"(rs1), "r"(src_addr)
	);

	__asm volatile("fence");//内存栅栏，防止优化乱序执行
	
	return rd ;
}


/**
 * @brief 加载模型到A buffer（从NN外部加载模型到NN内部的模型buffer）
 * 
 * @param size 大小（byte）
 * @param src_addr 模型地址
 * @return int32_t 无意义
 */
static inline int32_t npu_load_wt_to_A(uint16_t size, uint32_t src_addr)
{
	int32_t rd;
	uint32_t rs1 = (size << 16) ;
	// .insn r opcode, {xd,xs1,xs2}, func7 , rd, rs1, rs2
	asm volatile (
		".insn r 0x0b, 0x7, 0x0A, %0, %1, %2"
		:"=r"(rd)
		:"r"(rs1), "r"(src_addr)
	);

	__asm volatile("fence");

	return rd ;
}


static inline int32_t npu_load_wt_to_A_offset(uint16_t size, uint32_t src_addr,uint16_t offset)
{
	int32_t rd;
	uint32_t rs1 = (size << 16) | offset>>2;
	// .insn r opcode, {xd,xs1,xs2}, func7 , rd, rs1, rs2
	asm volatile (
		".insn r 0x0b, 0x7, 0x0A, %0, %1, %2"
		:"=r"(rd)
		:"r"(rs1), "r"(src_addr)
	);

	__asm volatile("fence");

	return rd ;
}


/**
 * @brief 加载模型到B buffer（从NN外部加载模型到NN内部的模型buffer）
 * 
 * @param size 大小（byte）
 * @param src_addr 模型地址
 * @return int32_t 无意义
 */
static inline int32_t npu_load_wt_to_B(uint16_t size, uint32_t src_addr)
{
	int32_t rd;
	uint32_t rs1 = (size << 16) ;
	// .insn r opcode, {xd,xs1,xs2}, func7 , rd, rs1, rs2
	asm volatile (
		".insn r 0x0b, 0x7, 0x0B, %0, %1, %2"
		:"=r"(rd)
		:"r"(rs1), "r"(src_addr)
	);

	__asm volatile("fence");

	return rd ;
}


static inline int32_t npu_load_wt_to_B_offset(uint16_t size, uint32_t src_addr,uint16_t offset)
{
	int32_t rd;
	uint32_t rs1 = (size << 16) | offset>>2;
	// .insn r opcode, {xd,xs1,xs2}, func7 , rd, rs1, rs2
	asm volatile (
		".insn r 0x0b, 0x7, 0x0B, %0, %1, %2"
		:"=r"(rd)
		:"r"(rs1), "r"(src_addr)
	);

	__asm volatile("fence");

	return rd ;
}


static inline int32_t npu_load_lut(uint16_t size, uint32_t src_addr)
{
	int32_t rd;
	uint32_t rs1 = (size << 16) ;
	// .insn r opcode, {xd,xs1,xs2}, func7 , rd, rs1, rs2
	asm volatile (
		".insn r 0x0b, 0x7, 0x0C, %0, %1, %2"
		:"=r"(rd)
		:"r"(rs1), "r"(src_addr)
	);

	__asm volatile("fence");

	return rd ;
}


/**
 * @brief 读模型buffer里面的数据（必须word操作，地址也要word对其）
 * 
 * @param offset 偏移地址
 * @return int32_t 无意义
 */
static inline int32_t npu_fetch_wt(uint16_t offset)
{
	int32_t rd;
	uint16_t rs1 = offset ;
	// .insn r opcode, {xd,xs1,xs2}, func7 , rd, rs1, rs2
	asm volatile (
		".insn r 0x0b, 0x6, 0x18, %0, %1, x0"
		:"=r"(rd)
		:"r"(rs1)
	);

	__asm volatile("fence");

	return rd ;
}


/**
 * @brief 将NN内部TCM里面的数据搬运到系统RAM
 * 
 * @param size 大小（byte）
 * @param tcm_ptr NN内部TCM的偏移地址
 * @param dest_addr 系统RAM地址
 * @return int32_t 无意义
 */
static inline int32_t npu_store(uint32_t dest_addr,uint16_t tcm_ptr,uint16_t size)
{
	int32_t rd;
	uint32_t rs1 = (size << 16) | (tcm_ptr - NPU_OFFSET_ADDR);
	// .insn r opcode, {xd,xs1,xs2}, func7 , rd, rs1, rs2
	asm volatile (
		".insn r 0x0b, 0x7, 0x09, %0, %1, %2"
		:"=r"(rd)
		:"r"(rs1), "r"(dest_addr)
	);

	__asm volatile("fence");

	return rd ;
}


/**
 * @brief 计算8bit乘法
 * 
 * @param w_ptr 模型偏移地址（NN内部模型buffer的偏移地址）
 * @param x_ptr 向量起始地址（NN内部向量的偏移地址）
 * @param vect_len 向量长度（这个函数表示多少个8bit数据）
 * @return int32_t 计算结果
 */
static inline int32_t npu_mac_w_8bit_x_8bit(uint16_t w_ptr,
								uint16_t x_ptr,
								uint16_t vect_len)
{
	int32_t rd;
	uint32_t rs1 = (vect_len << 16) | w_ptr;
	uint32_t rs2 =  x_ptr - NPU_OFFSET_ADDR;

	asm volatile (
		".insn r 0x0b, 0x7, 0x10, %0, %1, %2"
		:"=r"(rd)
		:"r"(rs1), "r"(rs2)
	);

	__asm volatile("fence");

	return rd ;
}


/**
 * @brief 计算8bit成累加，且完成打包的分段反量化（乘N个dqt），比如256长度的一个乘累加，
 * 按照64进行分段，模型里每64个w后面跟一个float的dqt，就一共4段，整个的长度是256+4*4个byte
 * plen配置为64，size配置为256+4*4，size需大于等于20
 * 
 * @param w_ptr 
 * @param x_ptr 
 * @param plen 
 * @param vect_len 
 * @return int32_t 
 */
static inline int32_t npu_mac_w_8bit_x_8bit_pack_dqt(uint16_t w_ptr,
													uint16_t x_ptr,
													uint16_t plen,
													uint16_t vect_len)
{
	int32_t rd;
	uint32_t rs1 = (vect_len << 16) | w_ptr;
	uint32_t rs2 = (plen << 16) | (x_ptr - NPU_OFFSET_ADDR);

	asm volatile (
		".insn r 0x0b, 0x7, 0x13, %0, %1, %2"
		:"=r"(rd)
		:"r"(rs1), "r"(rs2)
	);

	__asm volatile("fence");

	return rd ;
}


/**
 * @brief 计算8bit成累加，且完成打包的分段反量化（乘N个dqt），比如256长度的一个乘累加，
 * 按照64进行分段，模型里每64个w后面跟一个float的dqt，就一共4段，整个的长度是256+4*4个byte
 * plen配置为64，size配置为256+4*4
 * 
 * @param w_ptr 
 * @param x_ptr 
 * @param plen 
 * @param vect_len 
 * @return int32_t 
 */
static inline int32_t npu_mac_w_8bit_x_16bit_pack_dqt(uint16_t w_ptr,
													uint16_t x_ptr,
													uint16_t plen,
													uint16_t vect_len)
{
	int32_t rd;
	uint32_t rs1 = (vect_len << 16) | w_ptr;
	uint32_t rs2 = (plen << 16) | (x_ptr - NPU_OFFSET_ADDR);

	asm volatile (
		".insn r 0x0b, 0x7, 0x15, %0, %1, %2"
		:"=r"(rd)
		:"r"(rs1), "r"(rs2)
	);

	__asm volatile("fence");

	return rd ;
}



/**
 * @brief 计算16bit乘法
 * 
 * @param w_ptr 模型偏移地址（NN内部模型buffer的偏移地址）
 * @param x_ptr 向量起始地址（NN内部向量的偏移地址）
 * @param vect_len 向量长度（这个函数表示多少个16bit数据）
 * @return int32_t 计算结果
 */
static inline int32_t npu_mac_w_16bit_x_16bit(uint16_t w_ptr,
								uint16_t x_ptr,
								uint16_t vect_len
                                )
{
	int32_t rd;
	uint32_t rs1 = (vect_len << 16) | w_ptr;
	uint32_t rs2 = (x_ptr - NPU_OFFSET_ADDR) ;

	asm volatile (
		".insn r 0x0b, 0x7, 0x11, %0, %1, %2"
		:"=r"(rd)
		:"r"(rs1), "r"(rs2)
	);

	__asm volatile("fence");

	return rd ;
}

/**
 * @brief 计算16bit * 8bit乘法，只能模型8bit，向量16bit
 * 
 * @param w_ptr 模型偏移地址（NN内部模型buffer的偏移地址）
 * @param x_ptr 向量起始地址（NN内部向量的偏移地址）
 * @param vect_len 向量长度（这个函数表示多少个16bit数据）
 * @return int32_t 计算结果
 */
static inline int32_t npu_mac_w_8bit_x_16bit(uint16_t w_ptr,
								uint16_t x_ptr,
								uint16_t vect_len
                                )
{
	int32_t rd;
	uint32_t rs1 = (vect_len << 16) | w_ptr;
	uint32_t rs2 = (x_ptr - NPU_OFFSET_ADDR) ;

	asm volatile (
		".insn r 0x0b, 0x7, 0x12, %0, %1, %2"
		:"=r"(rd)
		:"r"(rs1), "r"(rs2)
	);

	__asm volatile("fence");

	return rd ;
}


/**
 * @brief 批处理获得带符号最大值，输入为 size 个浮点数据
 * 
 * @param size 数据个数
 * @param src_addr 需要求最大值的数据的基地址
 * @return float 带符号最大值
 */
static inline float npu_batch_max(uint16_t size,
								  uint32_t src_addr)
{
	int32_t rd;
	uint32_t rs1 = (size << 16);
	uint32_t rs2 = src_addr ;

	asm volatile (
		".insn r 0x0b, 0x7, 0x20, %0, %1, %2"
		:"=r"(rd)
		:"r"(rs1), "r"(rs2)
	);
	
	__asm volatile("fence");

	return NUM_HEX_COV_TO_FLOAT_1(rd) ;
}


/**
 * @brief 批处理获得带符号最小值，输入为 size 个浮点数据
 * 
 * @param size 数据个数
 * @param src_addr 数据的基地址
 * @return float 带符号最小值
 */
static inline float npu_batch_min(uint16_t size,
							//		uint16_t dst_ptr,
									uint32_t src_addr)
{
	int32_t rd;
	uint32_t rs1 = (size << 16);
	uint32_t rs2 = src_addr ;

	asm volatile (
		".insn r 0x0b, 0x7, 0x21, %0, %1, %2"
		:"=r"(rd)
		:"r"(rs1), "r"(rs2)
	);
	float ret = NUM_HEX_COV_TO_FLOAT_1(rd) ;
	__asm volatile("fence");
	return ret ;
}


/**
 * @brief 批处理获得绝对值最大值，输入为 size 个浮点数据
 * 
 * @param size 数据个数
 * @param src_addr 数据的基地址
 * @return float 绝对值最大值
 */
static inline float npu_batch_abs_max(uint16_t size,
							//		uint16_t dst_ptr,
									uint32_t src_addr)
{
	int32_t rd;
	uint32_t rs1 = (size << 16);
	uint32_t rs2 = src_addr ;

	asm volatile (
		".insn r 0x0b, 0x7, 0x22, %0, %1, %2"
		:"=r"(rd)
		:"r"(rs1), "r"(rs2)
	);

	float ret = NUM_HEX_COV_TO_FLOAT_1(rd) ;
	__asm volatile("fence");
	return ret ;
}


/**
 * @brief 批处理获得绝对值最小值，输入为 size 个浮点数据
 * 
 * @param size 数据个数
 * @param src_addr 数据的基地址
 * @return float 绝对值最小值
 */
static inline float npu_batch_abs_min(uint16_t size,
							//		uint16_t dst_ptr,
									uint32_t src_addr)
{
	int32_t rd;
	uint32_t rs1 = (size << 16);
	uint32_t rs2 = src_addr ;

	asm volatile (
		".insn r 0x0b, 0x7, 0x23, %0, %1, %2"
		:"=r"(rd)
		:"r"(rs1), "r"(rs2)
	);

	float ret = NUM_HEX_COV_TO_FLOAT_1(rd) ;

	__asm volatile("fence");
	return ret ;
}


/**
 * @brief relu激活（浮点到定点），有量化操作，量化操作需要先将量化的dqt写入寄存器 batch_actv_dqt_value （6号寄存器）
 * 还可以通过 batch_rslt_short_en （11号寄存器）选择输出为16bit还是8bit
 * 
 * @param size 点数
 * @param dst_ptr 激活后的定点数据存放地址（TCM内部）
 * @param src_addr 待激活数据的源地址
 * @return int32_t 
 */
static inline int32_t npu_batch_relu(uint16_t size,
									uint16_t dst_ptr,
									uint32_t src_addr)
{
	int32_t rd;
	uint32_t rs1 = (size << 16) | (dst_ptr - NPU_OFFSET_ADDR);
	uint32_t rs2 = src_addr ;

	asm volatile (
		".insn r 0x0b, 0x7, 0x24, %0, %1, %2"
		:"=r"(rd)
		:"r"(rs1), "r"(rs2)
	);

	__asm volatile("fence");

	return rd ;
}


static inline int32_t npu_batch_none(uint16_t size,
									uint16_t dst_ptr,
									uint32_t src_addr)
{
	int32_t rd;
	uint32_t rs1 = (size << 16) | (dst_ptr - NPU_OFFSET_ADDR);
	uint32_t rs2 = src_addr ;

	asm volatile (
		".insn r 0x0b, 0x7, 0x25, %0, %1, %2"
		:"=r"(rd)
		:"r"(rs1), "r"(rs2)
	);

	__asm volatile("fence");

	return rd ;
}

static inline int32_t npu_batch_sgmd(uint16_t size,
									uint16_t dst_ptr,
									uint32_t src_addr)
{
	int32_t rd;
	uint32_t rs1 = (size << 16) | (dst_ptr - NPU_OFFSET_ADDR);
	uint32_t rs2 = src_addr ;

	asm volatile (
		".insn r 0x0b, 0x7, 0x26, %0, %1, %2"
		:"=r"(rd)
		:"r"(rs1), "r"(rs2)
	);

	__asm volatile("fence");

	return rd ;
}


static inline float npu_exp(float op_data)
{
	int32_t rd;

	uint32_t rs1 = *(uint32_t*)(&op_data) ;
	// .insn r opcode, {xd,xs1,xs2}, func7 , rd, rs1, rs2
	asm volatile (
		".insn r 0x0b, 0x6, 0x28, %0, %1, x0"
		:"=r"(rd)
		:"r"(rs1)
	);

	float ret = *(float*)(&rd);
	__asm volatile("fence");
	return  ret;
}


#endif
