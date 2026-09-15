

SDK_PATH ?= ../../..
BUILD_DIR := build

include $(SDK_PATH)/system/user_alg_config.mk
include $(SDK_PATH)/utils/common_head.mk


ALG_BPU_SRAM_SIZE_TOTAL_CNT := 0
ifeq ($(USE_ASR), 1)
	C_FLAGS += -DCINN_MAX_CHA_NUM=1 -DNO_ASR_FLOW=0
else
	C_FLAGS += -DNO_ASR_FLOW=1
endif
ifeq ($(USE_ANY_MIC), 1)
	C_FLAGS += -DUSE_DUAL_MIC_ANY=1 -DHOST_CODEC_CHA_NUM=2 -DOFFLINE_DUAL_MIC_ALG_SUPPORT=1
endif
ifeq ($(USE_AEC), 1)
	C_FLAGS += -DUSE_AEC_MODULE=1 -DCINN_MAX_CHA_NUM=1   
	ALG_BPU_SRAM_SIZE_TOTAL_CNT := 0x$(shell let add=$(ALG_BPU_SRAM_SIZE_TOTAL_CNT)+$(AEC_USE_BNPU_SRAM_SIZE); printf '%x'  $$add )
endif
ifeq ($(USE_CWSL), 1)
	C_FLAGS += -DUSE_CWSL=1   -DASR_FE_REDUCE_MEM=1 -DCINN_MAX_CHA_NUM=1 
	ALG_BPU_SRAM_SIZE_TOTAL_CNT := 0x$(shell let add=$(ALG_BPU_SRAM_SIZE_TOTAL_CNT)+$(CWSL_USE_BNPU_SRAM_SIZE); printf '%x'  $$add )
endif
ifeq ($(USE_NN_DOA), 1)
	C_FLAGS += -DUSE_NN_DOA=1 -DASR_FE_REDUCE_MEM=1 -DOFFLINE_DUAL_MIC_ALG_SUPPORT=1 -DHOST_CODEC_CHA_NUM=2  -DCINN_MAX_CHA_NUM=1 
	ALG_BPU_SRAM_SIZE_TOTAL_CNT := 0x$(shell let add=$(ALG_BPU_SRAM_SIZE_TOTAL_CNT)+$(NN_DOA_USE_BNPU_SRAM_SIZE); printf '%x'  $$add )
endif
ifeq ($(USE_GCC_DOA), 1)
	C_FLAGS += -DUSE_GCC_DOA=1   -DASR_FE_REDUCE_MEM=1 -DOFFLINE_DUAL_MIC_ALG_SUPPORT=1 -DHOST_CODEC_CHA_NUM=2 -DCINN_MAX_CHA_NUM=1 
	ALG_BPU_SRAM_SIZE_TOTAL_CNT := 0x$(shell let add=$(ALG_BPU_SRAM_SIZE_TOTAL_CNT)+$(GCC_DOA_USE_BNPU_SRAM_SIZE); printf '%x'  $$add )
endif
ifeq ($(USE_DEREVERB), 1)
	C_FLAGS += -DOFFLINE_DUAL_MIC_ALG_SUPPORT=1 -DUSE_DEREVERB_MODULE=1 -DASR_FE_REDUCE_MEM=1 -DHOST_CODEC_CHA_NUM=2 -DCINN_MAX_CHA_NUM=1 
	ALG_BPU_SRAM_SIZE_TOTAL_CNT := 0x$(shell let add=$(ALG_BPU_SRAM_SIZE_TOTAL_CNT)+$(DEREVERB_USE_BNPU_SRAM_SIZE); printf '%x'  $$add )
endif

ifeq ($(USE_NN_DENOISE), 1)
	C_FLAGS += -DUSE_NN_DENOISE=1   -DCINN_MAX_CHA_NUM=1 
	ALG_BPU_SRAM_SIZE_TOTAL_CNT := 0x$(shell let add=$(ALG_BPU_SRAM_SIZE_TOTAL_CNT)+$(NN_DENOISE_USE_BNPU_SRAM_SIZE); printf '%x'  $$add )
endif

ifeq ($(USE_NN_VAD), 1)
	C_FLAGS += -DUSE_NN_VAD=1   
	ALG_BPU_SRAM_SIZE_TOTAL_CNT := 0x$(shell let add=$(ALG_BPU_SRAM_SIZE_TOTAL_CNT)+$(TVAD_USE_BNPU_SRAM_SIZE); printf '%x'  $$add )
endif


ifeq ($(USE_TRA_DENOISE), 1)
	C_FLAGS += -DUSE_TRA_DENOISE=1   -DCINN_MAX_CHA_NUM=1 
	ALG_BPU_SRAM_SIZE_TOTAL_CNT := 0x$(shell let add=$(ALG_BPU_SRAM_SIZE_TOTAL_CNT)+$(TRA_DENOISE_USE_BNPU_SRAM_SIZE); printf '%x'  $$add )
endif

ifeq ($(USE_VPR), 1)  
	C_FLAGS += -DUSE_VPR=1 -DCINN_MAX_CHA_NUM=1 -DASR_FE_REDUCE_MEM=0
	ALG_BPU_SRAM_SIZE_TOTAL_CNT := 0x$(shell let add=$(ALG_BPU_SRAM_SIZE_TOTAL_CNT)+$(NN_VPR_USE_BNPU_SRAM_SIZE); printf '%x'  $$add )
endif
ifeq ($(USE_WMAN_VPR), 1)  
	C_FLAGS += -DUSE_WMAN_VPR=1 -DCINN_MAX_CHA_NUM=1 -DASR_FE_REDUCE_MEM=0
	ALG_BPU_SRAM_SIZE_TOTAL_CNT := 0x$(shell let add=$(ALG_BPU_SRAM_SIZE_TOTAL_CNT)+$(NN_VPR_USE_BNPU_SRAM_SIZE); printf '%x'  $$add )
endif
ifeq ($(USE_BF), 1) 	
	C_FLAGS += -DDEEP_SEPARATE_ENABLE=1 -DCINN_MAX_CHA_NUM=2  -DASR_FE_REDUCE_MEM=1 -DUSE_BEAMFORMING_MODULE=1 -DOFFLINE_DUAL_MIC_ALG_SUPPORT=1 -DHOST_CODEC_CHA_NUM=2
	ALG_BPU_SRAM_SIZE_TOTAL_CNT := 0x$(shell let add=$(ALG_BPU_SRAM_SIZE_TOTAL_CNT)+$(BF_USE_BNPU_SRAM_SIZE); printf '%x'  $$add )
endif
ifeq ($(USE_SED_CRY), 1) 
	C_FLAGS += -DUSE_SED=1 -DNO_ASR_FLOW=1 -DCINN_MAX_CHA_NUM=1 -DASR_FE_REDUCE_MEM=0
	ALG_BPU_SRAM_SIZE_TOTAL_CNT := 0x$(shell let add=$(ALG_BPU_SRAM_SIZE_TOTAL_CNT)+$(NN_SED_USE_BNPU_SRAM_SIZE); printf '%x'  $$add )
endif
ifeq ($(USE_SED_SNORE), 1) 
	C_FLAGS += -DUSE_SED=1 -DNO_ASR_FLOW=1 -DCINN_MAX_CHA_NUM=1 -DASR_FE_REDUCE_MEM=0	
	ALG_BPU_SRAM_SIZE_TOTAL_CNT := 0x$(shell let add=$(ALG_BPU_SRAM_SIZE_TOTAL_CNT)+$(NN_SED_USE_BNPU_SRAM_SIZE); printf '%x'  $$add )
endif
ifeq ($(USE_IIS_RECORD), 1)
C_FLAGS += -DUSE_IIS1_OUT_PRE_RSLT_AUDIO=1
	ALG_BPU_SRAM_SIZE_TOTAL_CNT := 0x$(shell let add=$(ALG_BPU_SRAM_SIZE_TOTAL_CNT)+$(IIS_RECORD_USE_BNPU_SRAM_SIZE); printf '%x'  $$add )
endif

ifeq ($(USE_BF), 1)
	LIBS := -lasr_bnpu_use_deepse_v2
else
	LIBS := -lasr_bnpu_dis_deepse_v2
endif

# #事件检测内存消耗大，不能和识别同时开启,
# ifeq ($(USE_ASR), 1) 
# ifneq ($(filter 1, $(USE_SED_CRY) $(USE_SED_SNORE)),)
# 	$(error cannot support both ASR and sed)
# endif
# endif
# #DOA和AEC共享内存
# ifeq ($(USE_AEC), 1)
# ifeq ($(USE_NN_DOA), 1)
# AEC_NN_DOA_MAX_MEM_SIZE := $(shell if [ $(NN_DOA_USE_BNPU_SRAM_SIZE) -gt $(AEC_USE_BNPU_SRAM_SIZE) ]; then echo $(AEC_USE_BNPU_SRAM_SIZE); else echo $(NN_DOA_USE_BNPU_SRAM_SIZE); fi)
# ALG_BPU_SRAM_SIZE_TOTAL_CNT := 0x$(shell let sub=$(ALG_BPU_SRAM_SIZE_TOTAL_CNT)-$(NN_DOA_USE_BNPU_SRAM_SIZE); printf '%x'  $$sub )
# ALG_BPU_SRAM_SIZE_TOTAL_CNT := 0x$(shell let sub=$(ALG_BPU_SRAM_SIZE_TOTAL_CNT)-$(AEC_USE_BNPU_SRAM_SIZE); printf '%x'  $$sub )
# ALG_BPU_SRAM_SIZE_TOTAL_CNT := 0x$(shell let add=$(ALG_BPU_SRAM_SIZE_TOTAL_CNT)+$(AEC_NN_DOA_MAX_MEM_SIZE); printf '%x'  $$add )
# endif
# endif

SDK_ALG_PRO_SRAM_HOST_END_ADDR := 0x$(shell let sub=$(SDK_ALG_PRO_SRAM_HOST_NOT_ALG_END_ADDR)-$(ALG_BPU_SRAM_SIZE_TOTAL_CNT); printf '%x'  $$sub )
COMMON_LINK_LDS := ../../../utils/common.lds
ALG_PRO_LINK_LDS := ../lds/ci130x_asr_alg_bnpu.lds
SECONDARY_FLASH := $(SDK_PATH)/$(LIBS_PATH)/ci130x_asr_alg_bnpu.a
SECONDARY_LIST := $(BUILD_DIR)/$(PROJECT_NAME).lst

LIBS += -lm

LTO_OPTION := -flto
O_OPTION += -Os

include $(SDK_PATH)/utils/common_flags.mk

C_FLAGS += $(LTO_OPTION) $(O_OPTION) -DCI_CONFIG_FILE=\"user_config.h\"  -DCORE_ID=1
S_FLAGS += $(LTO_OPTION) $(O_OPTION)
S_FLAGS += -I$(SDK_PATH)/components -I$(SDK_PATH)/driver/ci130x_chip_driver/inc


LD_FLAGS := -march=rv32imafc -mabi=ilp32f -mcmodel=medlow -msmall-data-limit=8 -msave-restore -mfdiv $(O_OPTION) -fmessage-length=0	$(LTO_OPTION)
LD_FLAGS += -fsigned-char -ffunction-sections -fdata-sections -fno-common -fno-delete-null-pointer-checks -fno-unroll-loops
LD_FLAGS += -g -T "$(ALG_PRO_LINK_LDS)"  -nostartfiles -Xlinker --gc-sections -Wl,-Map,"$(BUILD_DIR)/$(PROJECT_NAME).map" --specs=nano.specs

LD_FLAGS += -Wl,--wrap,memcmp
LD_FLAGS += -Wl,--wrap,memcpy
LD_FLAGS += -Wl,--wrap,memmove
LD_FLAGS += -Wl,--wrap,memset
LD_FLAGS += -Wl,--wrap,strcat
LD_FLAGS += -Wl,--wrap,strcmp
LD_FLAGS += -Wl,--wrap,strcpy
LD_FLAGS += -Wl,--wrap,strlen
LD_FLAGS += -Wl,--wrap,strncat
LD_FLAGS += -Wl,--wrap,strncmp
LD_FLAGS += -Wl,--wrap,strncpy
LD_FLAGS += -Wl,--wrap,strstr
LD_FLAGS += -Wl,--wrap,stpcpy
LD_FLAGS += -Wl,--wrap,stpncpy
LD_FLAGS += -Wl,--wrap,strnlen
LD_FLAGS += -Wl,--wrap,strtoul
LD_FLAGS += -Wl,--wrap,atol
LD_FLAGS += -Wl,--wrap,qsort
# LD_FLAGS += -Wl,--wrap,malloc
# LD_FLAGS += -Wl,--wrap,free
LD_FLAGS += -Wl,--wrap,vsnprintf
LD_FLAGS += -Wl,--wrap,sprintf
LD_FLAGS += -Wl,--wrap,sscanf
LD_FLAGS += -Wl,--wrap,abs
LD_FLAGS += -Wl,--wrap,isnanf
LD_FLAGS += -Wl,--wrap,isnan
LD_FLAGS += -Wl,--wrap,isinff
LD_FLAGS += -Wl,--wrap,isinf
LD_FLAGS += -Wl,--wrap,cosf
LD_FLAGS += -Wl,--wrap,sinf
LD_FLAGS += -Wl,--wrap,tanf
LD_FLAGS += -Wl,--wrap,expf
LD_FLAGS += -Wl,--wrap,sqrtf
LD_FLAGS += -Wl,--wrap,fabsf
LD_FLAGS += -Wl,--wrap,logf
LD_FLAGS += -Wl,--wrap,log10f
LD_FLAGS += -Wl,--wrap,atoi

LD_FLAGS += -L$(SDK_PATH)/$(LIBS_PATH) -L$(SDK_PATH)/components/freertos/portable/GCC -L$(SDK_PATH)/utils

all: secondary-outputs 

ifneq ($(MAKECMDGOALS), clean)
-include $(BUILD_DIR)/source_file.mk
endif
$(ALG_PRO_LINK_LDS):../lds/ci130x_asr_alg_bnpu_temp.lds
	@echo 'mk ci130x_asr_alg_bnpu.lds'
	@echo "ALG_BPU_SRAM_SIZE_TOTAL_CNT = $(ALG_BPU_SRAM_SIZE_TOTAL_CNT)"
	@echo "AEC_NN_DOA_MAX_MEM_SIZE = $(AEC_NN_DOA_MAX_MEM_SIZE)"
	sed 's/@@SDK_ALG_PRO_SRAM_HOST_END_ADDR@@/$(SDK_ALG_PRO_SRAM_HOST_END_ADDR)/g' $< >$@
$(COMMON_LINK_LDS):../../../utils/common_temp.lds
	@echo 'mk common.lds'
	sed 's/@@SDK_ALG_PRO_SRAM_HOST_END_ADDR@@/$(SDK_ALG_PRO_SRAM_HOST_END_ADDR)/g' $< >$@



$(BUILD_DIR)/$(PROJECT_NAME).elf: $(OBJS) $(USER_OBJS) build/source_file.mk $(LIB_FILES) $(ALG_PRO_LINK_LDS) makefile
	@echo 'Building target: $@'
	@echo 'Invoking: GNU RISC-V Cross C++ Linker'
	$(CC_PREFIX)$(LD) $(LD_FLAGS) -o "$(BUILD_DIR)/$(PROJECT_NAME).elf" $(OBJS) $(USER_OBJS) $(LIBS) 
	@echo 'Finished building target: $@'
	@echo ' '

$(SECONDARY_FLASH): $(BUILD_DIR)/$(PROJECT_NAME).elf
	@echo 'Invoking: GNU RISC-V Cross Create Flash Image'
	mkdir -p ../firmware/user_code && $(CC_PREFIX)$(OC) -O binary "$(BUILD_DIR)/$(PROJECT_NAME).elf"  $(SECONDARY_FLASH)
	@echo 'Finished building: $@'
	@echo ' '

$(BUILD_DIR)/$(PROJECT_NAME).lst: $(BUILD_DIR)/$(PROJECT_NAME).elf
	@echo 'Invoking: GNU RISC-V Cross Create Listing'
	$(CC_PREFIX)$(OD) --demangle --disassemble --reloc --wide "$(BUILD_DIR)/$(PROJECT_NAME).elf" > "$(BUILD_DIR)/$(PROJECT_NAME).lst"
	$(CC_PREFIX)$(SIZE) --format=berkeley "$(BUILD_DIR)/$(PROJECT_NAME).elf"
	@echo 'Finished building: $@'
	@echo ' '


secondary-outputs: $(ALG_PRO_LINK_LDS) $(COMMON_LINK_LDS) $(SECONDARY_FLASH) $(SECONDARY_LIST) 

include $(SDK_PATH)/utils/common_tail_bnpu.mk

