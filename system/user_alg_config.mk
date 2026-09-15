########单一算法使能定义(注意:值后更不能有空格,修改算法后需要重新编译BNPU CORE)########
#只用ASR识别功能(注意：除了事件检测功能其他功能都依赖识别，都必须默认开启)
USE_ASR           := 1
#任意mic识别
USE_ANY_MIC       := 0
#使用回声消除模块:1-开启/0-关闭 会占用28K的空间 -单/双mic可用，130x内部有2个codec;如果同时使用双mic算法+加AEC算法，需要硬件再加一个codc作为AEC的信号回采
USE_AEC           := 1
#自学习功能-请在安静环境下，用清晰洪亮的声音进行指令学习，避免环境噪音过大和学习者声音过小导致学习不成功
USE_CWSL          := 0
#使用降混响模块:1-开启/0-关闭--暂不支持
USE_DEREVERB	  := 0
#NN深度降噪    
USE_NN_DENOISE    := 1
#NN VAD
USE_NN_VAD        := 1
#传统降噪--暂不支持
USE_TRA_DENOISE   := 0
#NN DOA - 暂不支持
#USE_NN_DOA        := 0
#GCC DOA
USE_GCC_DOA       := 0

#IIS 开启IIS采音功能,可以使用采音板采音,占用PA2~PA6。会多消耗20KB SYS内存
USE_IIS_RECORD    := 1

#############各个算法内存统计###############
#只开识别算法, 只开ASR识别算法HOST END基地址
SDK_ALG_PRO_SRAM_HOST_NOT_ALG_END_ADDR = 0x1FFD0000
#AEC算法算法在BNPU端消耗的内存
AEC_USE_BNPU_SRAM_SIZE	= 27*1024
#自学习算法在BNPU端消耗的内存
CWSL_USE_BNPU_SRAM_SIZE	= 0*1024
#降混响算法在BNPU端消耗的内存
DEREVERB_USE_BNPU_SRAM_SIZE = 38*1024
#NN降噪算法在NPU端消耗的内存
NN_DENOISE_USE_BNPU_SRAM_SIZE = 43*1024
#TVAD算法在NPU端内存消耗
TVAD_USE_BNPU_SRAM_SIZE = 0*1024
#传统降噪算法在NPU端消耗的内存
TRA_DENOISE_USE_BNPU_SRAM_SIZE = 5*1024
#NN DOA算法在NPU端消耗的内存
NN_DOA_USE_BNPU_SRAM_SIZE = 60*1024
#GCC DOA算法在NPU端消耗的内存
GCC_DOA_USE_BNPU_SRAM_SIZE = 20*1024
#IIS采音在BNPU端内存消耗
IIS_RECORD_USE_BNPU_SRAM_SIZE = 0*1024


########可组合算法使能(注意:设置变量值后面不能有空格，只能是0/1/2...)########
#---是否开启动态ALC功能，1:开启，0:关闭; -单mic算法，仅单mic可用
USE_ALC_AUTO_SWITCH_MODULE     := 0
#---AIOT 音频压缩类型(仅uart_sample使用) 	#0-null  1-speex  2-opus 3-g722
AIOT_AUDIO_COMPRESS_TYPE       := 1
########################################################################

