

%.o : %.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

%.o : %.S
	$(CC_PREFIX)$(AS) $(S_FLAGS) -c -o "$@" "$<"

build/source_file.mk: source_file.prj
	# sh $(SDK_PATH)/tools/generate_makefile.sh
	$(LUA) $(SDK_PATH)/utils/generate_makefile.lua source_file.prj $(SDK_PATH)

clean:
	@echo "CURDIR: $(CURDIR)"
	-$(RM) -rf build/*
	-$(RM) -rf *.a
	-$(RM) -rf ../../../utils/common.lds
	-$(RM) -rf ../lds/ci130x_asr_alg_bnpu.lds
	-@echo ' '

.PHONY: all clean dependents
