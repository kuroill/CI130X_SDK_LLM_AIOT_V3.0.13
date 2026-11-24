先make clean,
再make
将固件放到同一目录，运行：./spilt_firmware full_ci_audio_V3.0.15.bin
./spilt_firmware full_ci_audio_V3.0.16.bin
./spilt_firmware full_ci_audio_V3.0.17.bin
./spilt_firmware full_ci_audio_V3.0.18.bin
./spilt_firmware full_ci_audio_V3.0.19.bin
./spilt_firmware full_ci_audio_V3.0.20.bin
./spilt_firmware full_ci1306_V3.0.21.bin
./spilt_firmware full_ci1306_V3.0.22.bin
./spilt_firmware full_ci1306_V3.0.23.bin
./spilt_firmware full_ci1306_V3.0.24.bin
./spilt_firmware full_ci1306_V3.0.25.bin
./spilt_firmware full_ci1306_V3.0.26.bin
./spilt_firmware full_ci1306_V3.0.28.bin

生成ota_firmware.bin文件，用于主端升级

ota_partition_enable[5] 数组对应需要使能哪些ota分区，为1就打包到ota固件，0为不打包到ota固件