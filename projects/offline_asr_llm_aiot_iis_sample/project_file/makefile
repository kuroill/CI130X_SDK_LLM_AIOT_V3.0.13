
SDK_PATH ?= ../../..

# Keep BNPU configuration/linker output complete before parsing the MCU graph.
# Recursive make preserves the jobserver and explicit IDE command-line flags.
.DEFAULT_GOAL := all
ifneq ($(LENWELL_MCU_BUILD_STAGE),1)
SDK_ROOT := $(abspath $(SDK_PATH))
ifneq ($(filter clean,$(MAKECMDGOALS)),)
ifneq ($(filter-out clean,$(MAKECMDGOALS)),)
$(error Run clean separately from build targets)
endif
endif
.PHONY: $(sort all $(MAKECMDGOALS)) lenwell-build
$(sort all $(MAKECMDGOALS)): lenwell-build
lenwell-build:
ifeq ($(MAKECMDGOALS),clean)
	+$(MAKE) -f makefile LENWELL_MCU_BUILD_STAGE=1 SDK_PATH="$(SDK_PATH)" clean
else
	+$(MAKE) -C "$(SDK_ROOT)/projects/bnpu_core_alg_pro/project_file" PROJECT_NAME=bnpu_core_alg_pro SDK_PATH=../../.. all
	+$(MAKE) -f makefile LENWELL_MCU_BUILD_STAGE=1 SDK_PATH="$(SDK_PATH)" $(if $(MAKECMDGOALS),$(MAKECMDGOALS),all)
endif
else

include $(SDK_PATH)/system/user_alg_config.mk
include $(SDK_PATH)/utils/common_head.mk

#---音频上传云端接口类型: #1-串口输出  2-hpout输出  3-iis输出 (不可修改)
AUDIO_UPLOAD_TYPE              := 3
#---单麦就近唤醒:1-开启/0-关闭
USE_PWK_ENABLE                 := 0
#---意图数量:1-单意图   >1为多意图，最大支持3意图, 除USE_NULL、USE_DENOISE_NN和USE_DEREVERB可以支持多意图外，其他算法选项都只支持单意图
MULT_INTENT                    := 1
#---播放器类型: 0-精简播放器(相比老播放器节约内存15KB,功能简单) 1-老播放器(占内存较多，功能完善)
USE_AUDIO_PLAYER_TYPE          := 0
#算法功能编译配置
ifeq ($(USE_ASR), 1)
	C_FLAGS += -DCINN_MAX_CHA_NUM=1   -DNO_ASR_FLOW=0
else
	C_FLAGS += -DNO_ASR_FLOW=1
endif
ifeq ($(USE_ANY_MIC), 1)
C_FLAGS += -DUSE_DUAL_MIC_ANY=1 -DHOST_CODEC_CHA_NUM=2  -DOFFLINE_DUAL_MIC_ALG_SUPPORT=1 -DHOST_MIC_USE_NUMBER=2
endif
ifeq ($(USE_AEC), 1)
C_FLAGS += -DOFFLINE_DUAL_MIC_ALG_SUPPORT=1 -DUSE_AEC_MODULE=1 -DMULT_INTENT=$(MULT_INTENT) -DHOST_CODEC_CHA_NUM=2
endif
ifeq ($(USE_CWSL), 1)
C_FLAGS += -DUSE_CWSL=1 -DMULT_INTENT=$(MULT_INTENT)
endif
ifeq ($(USE_NN_DENOISE), 1) 
C_FLAGS += -DUSE_NN_DENOISE=1 -DMULT_INTENT=$(MULT_INTENT)
endif
ifeq ($(USE_NN_VAD), 1) 
C_FLAGS += -DUSE_NN_VAD=1
endif
ifeq ($(USE_TRA_DENOISE), 1) 
C_FLAGS += -DUSE_TRA_DENOISE=1 -DMULT_INTENT=$(MULT_INTENT)
endif
ifeq ($(USE_NN_DOA), 1)
C_FLAGS += -DUSE_NN_DOA=1 -DOFFLINE_DUAL_MIC_ALG_SUPPORT=1 -DHOST_CODEC_CHA_NUM=2 -DHOST_MIC_USE_NUMBER=2 -DMULT_INTENT=$(MULT_INTENT)
USE_DECODE_FRM := 1
endif
ifeq ($(USE_GCC_DOA), 1)
C_FLAGS += -DUSE_GCC_DOA=1  -DOFFLINE_DUAL_MIC_ALG_SUPPORT=1 -DHOST_CODEC_CHA_NUM=2  -DHOST_MIC_USE_NUMBER=2 -DMULT_INTENT=$(MULT_INTENT)
endif
ifeq ($(USE_IIS_RECORD), 1)
C_FLAGS += -DUSE_IIS1_OUT_PRE_RSLT_AUDIO=1
endif



ifeq ($(USE_BF), 1)
	LIBS += -lasr_host_use_deepse_v2 
else
	LIBS += -lasr_host_dis_deepse_v2 
endif

ifeq ($(USE_PWK_ENABLE), 1)
	USE_DECODE_FRM := 1
	C_FLAGS += -DUSE_PWK=1
else
	C_FLAGS += -DUSE_PWK=0
endif
ifeq ($(USE_ALC_AUTO_SWITCH_MODULE), 1)
	C_FLAGS += -DUSE_ALC_AUTO_SWITCH_MODULE=1
else
	C_FLAGS += -DUSE_ALC_AUTO_SWITCH_MODULE=0
endif
ifeq ($(MULT_INTENT), 1)
	C_FLAGS += -DMULT_INTENT=1

else
	C_FLAGS += -DMULT_INTENT=$(MULT_INTENT) -DDEFAULT_STOP_SILCNT=15 -DASR_FE_REDUCE_MEM=1 -DMAX_STOP_VAD_FRM=60
#多意图需要使用帧长统计功能
	USE_DECODE_FRM := 1
endif

ifeq ($(AIOT_AUDIO_COMPRESS_TYPE), 1)
LIBS += -lspeex
C_FLAGS += -I$(SDK_PATH)/components/cias_speex/include/speex -I$(SDK_PATH)/components/cias_speex/include/ogg -I$(SDK_PATH)/components/cias_speex/include -I$(SDK_PATH)/components/cias_speex/libspeex -I$(SDK_PATH)/components/cias_speex/port	   
else ifeq ($(AIOT_AUDIO_COMPRESS_TYPE), 2)    
LIBS += -lopus
C_FLAGS += -I$(SDK_PATH)/components/cias_opus/src/ -I$(SDK_PATH)/components/cias_opus/silk/float  -I$(SDK_PATH)/components/cias_opus/celt -I$(SDK_PATH)/components/cias_opus/include -I$(SDK_PATH)/projects/libopus/src -I$(SDK_PATH)/components/cias_opus/silk -I$(SDK_PATH)/components/cias_opus/silk/fixed/ 		   
else ifeq ($(AIOT_AUDIO_COMPRESS_TYPE), 3)
C_FLAGS += -DAIOT_AUDIO_COMPRESS_BY_G722=1
else ifeq ($(AIOT_AUDIO_COMPRESS_TYPE), 0)
else
$(error "error:请配置正确的AIOT_AUDIO_COMPRESS_TYPE参数!!!")
endif

ifeq ($(USE_AUDIO_PLAYER_TYPE), 0)
	C_FLAGS += -DSIMPLE_AUDIO_PLAYER_ENABLE=1
else
	C_FLAGS += -DSIMPLE_AUDIO_PLAYER_ENABLE=0
endif

ifeq ($(AUDIO_UPLOAD_TYPE), 1)
C_FLAGS += -DUSE_AUDIO_UPLOAD_BY_UART=1 -DUSE_AUDIO_UPLOAD_BY_HPOUT=0 -DUSE_AUDIO_UPLOAD_BY_IIS=0
else ifeq ($(AUDIO_UPLOAD_TYPE), 2)
C_FLAGS += -DUSE_AUDIO_UPLOAD_BY_HPOUT=1 -DUSE_AUDIO_UPLOAD_BY_UART=0 -DUSE_AUDIO_UPLOAD_BY_IIS=0
else ifeq ($(AUDIO_UPLOAD_TYPE), 3)
C_FLAGS += -DUSE_AUDIO_UPLOAD_BY_IIS=1 -DUSE_AUDIO_UPLOAD_BY_UART=0 -DUSE_AUDIO_UPLOAD_BY_HPOUT=0
else
$(error "error:请配置正确的AUDIO_UPLOAD_TYPE参数!!!")
endif



ALG_PRO_LINK_LDS := ../lds/ci130x_asr_alg_mcu.lds
LIB_BNPU_CORE_ALG_PRO := ci130x_asr_alg_bnpu.a
MCU_FLASH := ../firmware/user_code/[0]code.bin
BNPU_FLASH := $(SDK_PATH)/$(LIBS_PATH)/$(LIB_BNPU_CORE_ALG_PRO)
SECONDARY_FLASH := ../firmware/user_code/user_code.bin
COMMON_LINK_LDS := $(SDK_PATH)/utils/common.lds
SECONDARY_LIST := $(BUILD_DIR)/$(PROJECT_NAME).lst
LTO_OPTION = -flto

LIBS += -lm

C_FLAGS += -march=rv32imafc -mabi=ilp32f -mcmodel=medlow -msmall-data-limit=8 -msave-restore -mfdiv $(O_OPTION) -fmessage-length=0 $(LTO_OPTION)
C_FLAGS += -fsigned-char -ffunction-sections -fdata-sections -fno-common -fno-delete-null-pointer-checks -fno-unroll-loops  -g -std=gnu11 -MMD -MP
C_FLAGS += -DCI_CONFIG_FILE=\"user_config.h\" -DCORE_ID=0
# enum only cost 1 byte
C_FLAGS += -fshort-enums
S_FLAGS := -march=rv32imafc -mabi=ilp32f -mcmodel=medlow -msmall-data-limit=8 -msave-restore -mfdiv $(O_OPTION) -fmessage-length=0 $(LTO_OPTION)
S_FLAGS += -fsigned-char -ffunction-sections -fdata-sections -fno-common -fno-delete-null-pointer-checks -fno-unroll-loops  -g -x assembler-with-cpp -MMD -MP
S_FLAGS += -I$(SDK_PATH)/components -I$(SDK_PATH)/driver/ci130x_chip_driver/inc
LD_FLAGS := -march=rv32imafc -mabi=ilp32f -mcmodel=medlow -msmall-data-limit=8 -msave-restore -mfdiv $(O_OPTION) -fmessage-length=0	$(LTO_OPTION)
LD_FLAGS += -fsigned-char -ffunction-sections -fdata-sections -fno-common -fno-delete-null-pointer-checks -fno-unroll-loops
LD_FLAGS += -g -T "$(ALG_PRO_LINK_LDS)"  -nostartfiles -Xlinker  --gc-sections -Wl,-Map,"$(BUILD_DIR)/$(PROJECT_NAME).map" --specs=nano.specs

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
LD_FLAGS += -Wl,--wrap,malloc
LD_FLAGS += -Wl,--wrap,free
LD_FLAGS += -Wl,--wrap,vsnprintf
LD_FLAGS += -Wl,--wrap,sprintf
LD_FLAGS += -Wl,--wrap,sscanf
LD_FLAGS += -Wl,--wrap,abs
LD_FLAGS += -Wl,--wrap,isnanf
LD_FLAGS += -Wl,--wrap,isnan
LD_FLAGS += -Wl,--wrap,isinf
LD_FLAGS += -Wl,--wrap,isinff
LD_FLAGS += -Wl,--wrap,cosf
LD_FLAGS += -Wl,--wrap,sinf
LD_FLAGS += -Wl,--wrap,tanf
LD_FLAGS += -Wl,--wrap,expf
LD_FLAGS += -Wl,--wrap,sqrtf
LD_FLAGS += -Wl,--wrap,fabsf
LD_FLAGS += -Wl,--wrap,logf
LD_FLAGS += -Wl,--wrap,log10f
LD_FLAGS += -Wl,--wrap,atoi

LD_FLAGS += -L$(SDK_PATH)/$(LIBS_PATH) -L$(SDK_PATH)/components/freertos/portable/GCC
LD_FLAGS += -L$(SDK_PATH)/components/asr/decoder -L$(SDK_PATH)/utils

all: secondary-outputs

ifneq ($(MAKECMDGOALS), clean)
-include $(BUILD_DIR)/source_file.mk

ifeq ($(USE_DECODE_FRM), 1)
LIB_FILES += $(SDK_PATH)/$(LIBS_PATH)/libcikd_pro_cwsl_use_frm.a
LIBS += -lcikd_pro_cwsl_use_frm
else
LIB_FILES += $(SDK_PATH)/$(LIBS_PATH)/libcikd_pro_cwsl_dis_frm.a
LIBS += -lcikd_pro_cwsl_dis_frm
endif

endif


# Compare effective flags on every invocation without changing the stamp on a
# no-op build. Do not attach this forced prerequisite to an included makefile.
BUILD_CONFIG := $(BUILD_DIR)/build-config.txt
BUILD_CONFIG_INPUTS := makefile source_file.prj $(SDK_PATH)/system/user_alg_config.mk \
	$(SDK_PATH)/utils/common_head.mk $(SDK_PATH)/utils/common_tail_mcu.mk \
	$(SDK_PATH)/utils/generate_makefile.lua
shell_quote = '$(subst ','"'"',$(1))'
.PHONY: force-build-config
.DELETE_ON_ERROR:
$(BUILD_CONFIG): force-build-config
	@mkdir -p "$(BUILD_DIR)"
	@printf '%s\n' $(call shell_quote,PROJECT_NAME=$(PROJECT_NAME)) \
		$(call shell_quote,SDK_PATH=$(SDK_PATH)) \
		$(call shell_quote,CC=$(CC_PREFIX)$(CC)) $(call shell_quote,AS=$(CC_PREFIX)$(AS)) \
		$(call shell_quote,LD=$(CC_PREFIX)$(LD)) $(call shell_quote,OC=$(CC_PREFIX)$(OC)) \
		$(call shell_quote,C_FLAGS=$(C_FLAGS)) $(call shell_quote,S_FLAGS=$(S_FLAGS)) \
		$(call shell_quote,LD_FLAGS=$(LD_FLAGS)) $(call shell_quote,LIBS=$(LIBS)) \
		$(call shell_quote,LIB_FILES=$(LIB_FILES)) $(call shell_quote,USER_OBJS=$(USER_OBJS)) \
		$(call shell_quote,SDK_ALG_PRO_SRAM_HOST_END_ADDR=$(SDK_ALG_PRO_SRAM_HOST_END_ADDR)) > "$@.tmp"
	@if $(CMDBOX) cmp -s "$@" "$@.tmp"; then rm -f "$@.tmp"; else mv -f "$@.tmp" "$@"; fi

$(OBJS): $(BUILD_CONFIG) $(BUILD_CONFIG_INPUTS)
build/source_file.mk: $(SDK_PATH)/utils/generate_makefile.lua

SDK_ALG_PRO_SRAM_HOST_END_ADDR = $(shell grep 'SDK_ALG_PRO_SRAM_HOST_END_ADDR' $(COMMON_LINK_LDS) | grep -o '0x[0-9a-fA-F]*')
$(ALG_PRO_LINK_LDS): ../lds/ci130x_asr_alg_mcu_temp.lds $(COMMON_LINK_LDS) $(BUILD_CONFIG) $(BUILD_CONFIG_INPUTS)
	sed 's/@@SDK_ALG_PRO_SRAM_HOST_END_ADDR@@/$(SDK_ALG_PRO_SRAM_HOST_END_ADDR)/g' $< >$@

$(BUILD_DIR)/$(PROJECT_NAME).elf: $(OBJS) $(USER_OBJS) build/source_file.mk $(LIB_FILES) $(ALG_PRO_LINK_LDS) $(BUILD_CONFIG) $(BUILD_CONFIG_INPUTS)
	@echo 'Building target: $@'
	@echo 'Invoking: GNU RISC-V Cross C++ Linker'
	$(CC_PREFIX)$(LD) $(LD_FLAGS)  -o "$(BUILD_DIR)/$(PROJECT_NAME).elf" $(OBJS) $(USER_OBJS) $(LIBS)
	@echo 'Finished building target: $@'
	@echo ' '

$(MCU_FLASH): $(BUILD_DIR)/$(PROJECT_NAME).elf
	@echo 'Invoking: GNU RISC-V Cross Create MCU Flash Image'
	@set -e; mkdir -p ../firmware/user_code; \
		trap 'rm -f "$@.tmp"' 0; \
		$(CC_PREFIX)$(OC) -O binary "$<" "$@.tmp"; \
		mv -f "$@.tmp" "$@"

# The merged image is a separate output: changing only BNPU must re-merge it.
# Retain the last successful package if objcopy/merge fails. The staging
# directory basename is significant: ci-tool-kit emits user_code.bin there.
$(SECONDARY_FLASH): $(MCU_FLASH) $(BNPU_FLASH)
	@echo 'Merging current MCU and BNPU images: $@'
	@set -e; merge_dir="$(BUILD_DIR)/merge/user_code"; \
		mkdir -p "$$merge_dir"; \
		trap 'rm -f "$$merge_dir/[0]code.bin" "$$merge_dir/[1]code.bin" "$$merge_dir/user_code.bin"' 0; \
		rm -f "$$merge_dir/user_code.bin"; \
		cp "$(MCU_FLASH)" "$$merge_dir/[0]code.bin"; \
		cp "$(BNPU_FLASH)" "$$merge_dir/[1]code.bin"; \
		"$(SDK_PATH)/tools/ci-tool-kit" merge user-file -i "$$merge_dir" > /dev/null; \
		test -s "$$merge_dir/user_code.bin"; \
		mv -f "$$merge_dir/user_code.bin" "$@"
	@echo 'Finished building: $@'

.PHONY: user_code
user_code: $(SECONDARY_FLASH)

$(BUILD_DIR)/$(PROJECT_NAME).lst: $(BUILD_DIR)/$(PROJECT_NAME).elf
	@echo 'Invoking: GNU RISC-V Cross Create Listing'
	$(CC_PREFIX)$(OD) --demangle --disassemble --reloc --wide "$(BUILD_DIR)/$(PROJECT_NAME).elf" > "$(BUILD_DIR)/$(PROJECT_NAME).lst"
	$(CC_PREFIX)$(SIZE) --format=berkeley "$(BUILD_DIR)/$(PROJECT_NAME).elf"
	@echo 'Finished building: $@'
	@echo ' '



secondary-outputs: $(ALG_PRO_LINK_LDS) $(SECONDARY_FLASH) $(SECONDARY_LIST) 


include $(SDK_PATH)/utils/common_tail_mcu.mk

endif # LENWELL_MCU_BUILD_STAGE
