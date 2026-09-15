/**/
#ifndef __USER_CONFIG_H__
#define __USER_CONFIG_H__

#include "sdk_default_config.h"

#define NO_RC_CLOCK_CONFIG
#define VAD_TIME_OUT_FRAME                 760                 //vad超时帧数        

#define CONFIG_SYSTEMVIEW_EN               0                   //不使能systemview

#if USE_V7
#define CONFIG_CI_LOG_UART                  HAL_UART1_BASE
#endif

#if USE_V5
#define CONFIG_CI_LOG_UART                  HAL_UART1_BASE
#endif
#if SDK_RELEASE_EN
#define CONFIG_CI_LOG_UART                  0//HAL_UART0_BASE    
#else
#define CONFIG_CI_LOG_UART                  0 
#endif
#define USE_PWK                             1

#define REF_IN_FROM_INNER_CODEC             0//双麦算法时，使用模拟右麦作参考信号

#if (USE_BEAMFORMING_MODULE  || USE_NN_DOA || USE_GCC_DOA || USE_DEREVERB_MODULE || USE_DUAL_MIC_ANY) && !REF_IN_FROM_INNER_CODEC
#define HOST_CODEC_CHA_NUM              2
#define OFFLINE_DUAL_MIC_ALG_SUPPORT    1     //双MIC功能使能:1-开启/0-关闭
#define DOA_DEBUG_MODE                  0     //doa算法是否通过协议串口，输出micl的采音信息
#else
#define HOST_CODEC_CHA_NUM              1
#define OFFLINE_DUAL_MIC_ALG_SUPPORT    0     //双MIC功能使能:1-开启/0-关闭
#endif
#endif /* _USER_CONFIG_H_ */ 
