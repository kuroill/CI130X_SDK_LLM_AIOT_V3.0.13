/**
  ******************************************************************************
  * @文件    asr_top_config.h
  * @版本    V1.0.1
  * @日期    2019-3-15
  * @概要  asr 系统宏配置，调试用
  ******************************************************************************
  * @注意
  *
  * 版权归chipintelli公司所有，未经允许不得使用或修改
  *
  ******************************************************************************
  */ 

#ifndef __ASR_TOP_CONFIG_H
#define __ASR_TOP_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif
  
#include "ci_log.h"
#include <stdbool.h>
#include "sdk_default_config.h"
#define CI_NN_V2_EN  1              //是否使用重构的NN代码
#define FE_FAKE_DATA_TEST_EN    0    //使用标准智能管家模型以及FE输入数据buf zngj_utt_int16_t[]

#define NN_IS_RIGHT_DEBUG   0//直接吃FE，算NN
#define NN_WAIT_DECODER_DOWN  1//NN吐出结果需要等到decoder完成,当这个功能打开的时候，只有一个NN buffer在实际使用

#define NUCLER_COM_JUDGE_PARA_NUM (1)//通信的时候判断参数数量 

#if SDK_RELEASE_EN
#define ASR_RPMSG_ALL_PRINT   (0) //必须关闭
#else
#define ASR_RPMSG_ALL_PRINT   (0)//打印全部打开
#endif
#define NN_PRUNB_SIZE (128)//查找表的大小，n 个word

#define DEBUG_ASRTOP_FEDNN_CMPT_TEST_MODE (0)
#define DEBUG_ASRTOP_SAVE_FE_PCM (0)

#define FBANK_VEC_NUM 60

extern int8_t ASR_FE_QUANT_TO_8BIT;  

 
#define FE_USE_16BIT 1




#define DNN_FE_BUF_ADDR 0X20100000
#define DNN_FE_BUF_ROWS 128
  
#define ASRVAD_DEBUG 0

#if ASRVAD_DEBUG
  #define ASRVAD_DEBUG_VADSTART_FRM   41//ASRVAD_OFFSET
  #define ASRVAD_DEBUG_VADEND_FRM   380
  #define DEBUG_VAD_NOTUSE 0
#endif   

#define USE_I2SDMA_VAD_BYPASS 0

#define USE_PCM_FROM_EX_SD 0

#define DEBUG_ASRTOP (0)

#define NN_OUT_MAX_ULTRAL_NUM (300)

#if DEBUG_ASRTOP
#define ASR_DEBUG_ERROR_LOG(format,...) {ci_logassert(LOG_ASR_DECODER,"F:,L: %05d: "format"\n",__LINE__, ##__VA_ARGS__)  ;__BKPT(0);}
#define ASR_DEBUG_BKPT() {ci_logassert(LOG_ASR_DECODER,"F:,L: %d \n",__LINE__);__BKPT(0);}
#define ASR_AUTOAL_PRINT(format,...) //mprintf(format,##__VA_ARGS__)
#define ASR_DEBUG_LOG(format,...) //mprintf(format,##__VA_ARGS__)
#else
#define ASR_DEBUG_ERROR_LOG(format,...) //{mprintf("File: "__FILE__",Line: %05d: "format"\n", __LINE__, ##__VA_ARGS__)  ;}
#define DNN_DEBUG_BKPT()  {mprintf("dnn,L:%d \n",__LINE__);}
#define DECODE_DEBUG_BKPT()  {mprintf("decode,L:%d \n",__LINE__);}
#define VAD_DEBUG_BKPT()  {mprintf("vad,L:%d \n",__LINE__);}
#define ASRTOP_DEBUG_BKPT()  {mprintf("asrtop,L:%d \n",__LINE__);}

extern volatile int sys_err_flag;

#define ASR_AUTOAL_PRINT(format,...) //mprintf(format,##__VA_ARGS__)
#define ASR_DEBUG_LOG(format,...) //mprintf(format,##__VA_ARGS__)

#endif
#define DNN_CPU_COMPUTE 0


typedef enum
{
    ASR_RST_FLAG_NUM_1 = 1,
    ASR_RST_FLAG_NUM_2 = 2,
    ASR_RST_FLAG_NUM_3 = 3,
    ASR_RST_FLAG_NUM_4 = 4,
    ASR_RST_FLAG_NUM_5 = 5,
    ASR_RST_FLAG_NUM_6 = 6,
    ASR_RST_FLAG_NUM_7 = 7,
    ASR_RST_FLAG_NUM_8 = 8,
    ASR_RST_FLAG_NUM_9 = 9,
    ASR_RST_FLAG_NUM_10 = 10,
    ASR_RST_FLAG_NUM_11 = 11,
    ASR_RST_FLAG_NUM_12 = 12,
    ASR_RST_FLAG_NUM_13 = 13,
    ASR_RST_FLAG_NUM_14 = 14,
    ASR_RST_FLAG_NUM_15 = 15,
    ASR_RST_FLAG_NUM_16 = 16,
    ASR_RST_FLAG_NUM_17 = 17,
    ASR_RST_FLAG_NUM_18 = 18,
    ASR_RST_FLAG_NUM_19 = 19,
    ASR_RST_FLAG_NUM_20 = 20,
    ASR_RST_FLAG_NUM_21 = 21,
    ASR_RST_FLAG_NUM_22 = 22,
    ASR_RST_FLAG_NUM_23 = 23,
    ASR_RST_FLAG_NUM_24 = 24,
    ASR_RST_FLAG_NUM_25 = 25,
    ASR_RST_FLAG_NUM_26 = 26,
    ASR_RST_FLAG_NUM_27 = 27,
    ASR_RST_FLAG_NUM_28 = 28,
    ASR_RST_FLAG_NUM_29 = 29,
    ASR_RST_FLAG_NUM_30 = 30,
    ASR_RST_FLAG_NUM_31 = 31,
    ASR_RST_FLAG_NUM_32 = 32,
    ASR_RST_FLAG_NUM_33 = 33,
    ASR_RST_FLAG_NUM_34 = 34,
    ASR_RST_FLAG_NUM_35 = 35,
    ASR_RST_FLAG_NUM_36 = 36,
    ASR_RST_FLAG_NUM_37 = 37,
    ASR_RST_FLAG_NUM_38 = 38,
    ASR_RST_FLAG_NUM_39 = 39,
    ASR_RST_FLAG_NUM_40 = 40,
    ASR_RST_FLAG_NUM_41 = 41,
    ASR_RST_FLAG_NUM_42 = 42,
    ASR_RST_FLAG_NUM_43 = 43,
    ASR_RST_FLAG_NUM_44 = 44,
    ASR_RST_FLAG_NUM_45 = 45,
    ASR_RST_FLAG_NUM_46 = 46,
    ASR_RST_FLAG_NUM_47 = 47,
    ASR_RST_FLAG_NUM_48 = 48,
    ASR_RST_FLAG_NUM_49 = 49,
    ASR_RST_FLAG_NUM_50 = 50,
    ASR_RST_FLAG_NUM_51 = 51,
    ASR_RST_FLAG_NUM_52 = 52,
    ASR_RST_FLAG_NUM_53 = 53,
    ASR_RST_FLAG_NUM_54 = 54,
    ASR_RST_FLAG_NUM_55 = 55,
    ASR_RST_FLAG_NUM_56 = 56,
    ASR_RST_FLAG_NUM_57 = 57,
    ASR_RST_FLAG_NUM_58 = 58,
    ASR_RST_FLAG_NUM_59 = 59,
    ASR_RST_FLAG_NUM_60 = 60,
    ASR_RST_FLAG_NUM_61 = 61,
    ASR_RST_FLAG_NUM_62 = 62,
    ASR_RST_FLAG_NUM_63 = 63,
    ASR_RST_FLAG_NUM_64 = 64,
    ASR_RST_FLAG_NUM_65 = 65,
    ASR_RST_FLAG_NUM_66 = 66,
    ASR_RST_FLAG_NUM_67 = 67,
    ASR_RST_FLAG_NUM_68 = 68,
    ASR_RST_FLAG_NUM_69 = 69,
    ASR_RST_FLAG_NUM_70 = 70,
    ASR_RST_FLAG_NUM_71 = 71,
    ASR_RST_FLAG_NUM_72 = 72,
    ASR_RST_FLAG_NUM_73 = 73,
    ASR_RST_FLAG_NUM_74 = 74,
    ASR_RST_FLAG_NUM_75 = 75,
    ASR_RST_FLAG_NUM_76 = 76,
    ASR_RST_FLAG_NUM_77 = 77,
    ASR_RST_FLAG_NUM_78 = 78,
    ASR_RST_FLAG_NUM_79 = 79,
    ASR_RST_FLAG_NUM_80 = 80,
    ASR_RST_FLAG_NUM_81 = 81,
    ASR_RST_FLAG_NUM_82 = 82,
    ASR_RST_FLAG_NUM_83 = 83,
    ASR_RST_FLAG_NUM_84 = 84,
    ASR_RST_FLAG_NUM_85 = 85,
    ASR_RST_FLAG_NUM_86 = 86,
    ASR_RST_FLAG_NUM_87 = 87,
    ASR_RST_FLAG_NUM_88 = 88,
    ASR_RST_FLAG_NUM_89 = 89,
    ASR_RST_FLAG_NUM_90 = 90,
    ASR_RST_FLAG_NUM_91 = 91,
    ASR_RST_FLAG_NUM_92 = 92,
    ASR_RST_FLAG_NUM_93 = 93,
    ASR_RST_FLAG_NUM_94 = 94,
    ASR_RST_FLAG_NUM_95 = 95,
    ASR_RST_FLAG_NUM_96 = 96,
    ASR_RST_FLAG_NUM_97 = 97,
    ASR_RST_FLAG_NUM_98 = 98,
    ASR_RST_FLAG_NUM_99 = 99,
    ASR_RST_FLAG_NUM_100 = 100,
    ASR_RST_FLAG_NUM_101 = 101,
    ASR_RST_FLAG_NUM_102 = 102,
    ASR_RST_FLAG_NUM_103 = 103,
    ASR_RST_FLAG_NUM_104 = 104,
    ASR_RST_FLAG_NUM_105 = 105,
    ASR_RST_FLAG_NUM_106 = 106,
    ASR_RST_FLAG_NUM_107 = 107,
    ASR_RST_FLAG_NUM_108 = 108,
    ASR_RST_FLAG_NUM_109 = 109,
    ASR_RST_FLAG_NUM_110 = 110,
    ASR_RST_FLAG_NUM_111 = 111,
    ASR_RST_FLAG_NUM_112 = 112,
    ASR_RST_FLAG_NUM_113 = 113,
    ASR_RST_FLAG_NUM_114 = 114,
    ASR_RST_FLAG_NUM_115 = 115,
    ASR_RST_FLAG_NUM_116 = 116,
    ASR_RST_FLAG_NUM_117 = 117,
    ASR_RST_FLAG_NUM_118 = 118,
    ASR_RST_FLAG_NUM_119 = 119,
    ASR_RST_FLAG_NUM_120 = 120,
    ASR_RST_FLAG_NUM_121 = 121,
    ASR_RST_FLAG_NUM_122 = 122,
    ASR_RST_FLAG_NUM_123 = 123,
    ASR_RST_FLAG_NUM_124 = 124,
    ASR_RST_FLAG_NUM_125 = 125,
    ASR_RST_FLAG_NUM_126 = 126,
    ASR_RST_FLAG_NUM_127 = 127,
    ASR_RST_FLAG_NUM_128 = 128,
    ASR_RST_FLAG_NUM_129 = 129,
    ASR_RST_FLAG_NUM_130 = 130,
    ASR_RST_FLAG_NUM_131 = 131,
    ASR_RST_FLAG_NUM_132 = 132,
    ASR_RST_FLAG_NUM_133 = 133,
    ASR_RST_FLAG_NUM_134 = 134,
    ASR_RST_FLAG_NUM_135 = 135,
    ASR_RST_FLAG_NUM_136 = 136,
    ASR_RST_FLAG_NUM_137 = 137,
    ASR_RST_FLAG_NUM_138 = 138,
    ASR_RST_FLAG_NUM_139 = 139,
    ASR_RST_FLAG_NUM_140 = 140,
    ASR_RST_FLAG_NUM_141 = 141,
    ASR_RST_FLAG_NUM_142 = 142,
    ASR_RST_FLAG_NUM_143 = 143,
    ASR_RST_FLAG_NUM_144 = 144,
    ASR_RST_FLAG_NUM_145 = 145,
    ASR_RST_FLAG_NUM_146 = 146,
    ASR_RST_FLAG_NUM_147 = 147,
    ASR_RST_FLAG_NUM_148 = 148,
    ASR_RST_FLAG_NUM_149 = 149,
    ASR_RST_FLAG_NUM_150 = 150,
    ASR_RST_FLAG_NUM_151 = 151,
    ASR_RST_FLAG_NUM_152 = 152,
    ASR_RST_FLAG_NUM_153 = 153,
    ASR_RST_FLAG_NUM_154 = 154,
    ASR_RST_FLAG_NUM_155 = 155,
    ASR_RST_FLAG_NUM_156 = 156,
    ASR_RST_FLAG_NUM_157 = 157,
    ASR_RST_FLAG_NUM_158 = 158,
    ASR_RST_FLAG_NUM_159 = 159,
    ASR_RST_FLAG_NUM_160 = 160,
    ASR_RST_FLAG_NUM_161 = 161,
    ASR_RST_FLAG_NUM_162 = 162,
    ASR_RST_FLAG_NUM_163 = 163,
    ASR_RST_FLAG_NUM_164 = 164,
    ASR_RST_FLAG_NUM_165 = 165,
    ASR_RST_FLAG_NUM_166 = 166,
    ASR_RST_FLAG_NUM_167 = 167,
    ASR_RST_FLAG_NUM_168 = 168,
    ASR_RST_FLAG_NUM_169 = 169,
    ASR_RST_FLAG_NUM_170 = 170,
    ASR_RST_FLAG_NUM_171 = 171,
    ASR_RST_FLAG_NUM_172 = 172,
    ASR_RST_FLAG_NUM_173 = 173,
    ASR_RST_FLAG_NUM_174 = 174,
    ASR_RST_FLAG_NUM_175 = 175,
    ASR_RST_FLAG_NUM_176 = 176,
    ASR_RST_FLAG_NUM_177 = 177,
    ASR_RST_FLAG_NUM_178 = 178,
    ASR_RST_FLAG_NUM_179 = 179,
    ASR_RST_FLAG_NUM_180 = 180,
    ASR_RST_FLAG_NUM_181 = 181,
    ASR_RST_FLAG_NUM_182 = 182,
    ASR_RST_FLAG_NUM_183 = 183,
    ASR_RST_FLAG_NUM_184 = 184,
    ASR_RST_FLAG_NUM_185 = 185,
    ASR_RST_FLAG_NUM_186 = 186,
    ASR_RST_FLAG_NUM_187 = 187,
    ASR_RST_FLAG_NUM_188 = 188,
    ASR_RST_FLAG_NUM_189 = 189,
    ASR_RST_FLAG_NUM_190 = 190,
    ASR_RST_FLAG_NUM_191 = 191,
    ASR_RST_FLAG_NUM_192 = 192,
    ASR_RST_FLAG_NUM_193 = 193,
    ASR_RST_FLAG_NUM_194 = 194,
    ASR_RST_FLAG_NUM_195 = 195,
    ASR_RST_FLAG_NUM_196 = 196,
    ASR_RST_FLAG_NUM_197 = 197,
    ASR_RST_FLAG_NUM_198 = 198,
    ASR_RST_FLAG_NUM_199 = 199,

    ASR_WARNING_FLAG_300 = 300,
    ASR_WARNING_FLAG_301 = 301,
    ASR_WARNING_FLAG_302 = 302,

    ASR_WARNING_FLAG_400 = 400,
}asr_rst_flag_num_t;


#ifdef __cplusplus
}
#endif

#endif //__ASR_TOP_CONFIG_H

/***************** (C) COPYRIGHT Chipintelli Technology Co., Ltd. *****END OF FILE****/

