/**
  ******************************************************************************
  * @文件    NN_memmap.h
  * @版本    V1.0.1
  * @日期    2019-3-15
  * @概要
  ******************************************************************************
  * @注意
  *
  * 版权归chipintelli公司所有，未经允许不得使用或修改
  *
  ******************************************************************************
  */ 

#ifndef _NN_MEMMAP_H
#define _NN_MEMMAP_H

#include <stdbool.h>
#include <stdint.h>


#define CINN_CINN2_USE_SAME_TMP_BUFFER  0

//修改了这个还要改lds
#define TCM_MAX_USE  0


#define W_A_BUFFER_ADDR     (0x30000000)
#define W_B_BUFFER_ADDR     (0x30004000)

#if 0
#if USE_VPR || USE_SED || USE_NN_DENOISE
//这两个模块会用到NPU map起始的起始地址，需要单独处理
#define NN_MAP_OFFSET_SIZE  (0*1024U)
#else
#define NN_MAP_OFFSET_SIZE  (0*1024U)
#endif
#endif

#define NN_MAP_OFFSET_SIZE  (0*1024U)

#define NN_MAP_START_ADDR   (0x1fff8000 + NN_MAP_OFFSET_SIZE)   //NPU的malloc的起始边界
#define NN_MEM_MAP_SIZE     (32*1024U - NN_MAP_OFFSET_SIZE)     //NPU malloc的大小
#define NN_MAP_END_ADDR     (0x20000000)

#define NN_TCM_START_ADDR   (0x1fff8000)                        //NPU的TCM从哪个地址开始
#define NPU_OFFSET_ADDR	(0x8000)

#define NPU_MODEL_BUFFER_SIZE (16*1024)

#define MAX_NPU_FLOAT_TO_INT16_NUM  (256)                       //每次float到int只能最多256个

#define NPU_MIN_SEG_NUM_8x8   (28)


void check_npu_malloc_is_right(int malloc_size,bool is_clear);

#endif //_NN_MEMMAP_H

/***************** (C) COPYRIGHT Chipintelli Technology Co., Ltd. *****END OF FILE****/  
