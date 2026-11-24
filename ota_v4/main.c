#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <semaphore.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <sys/types.h>   
#include <sys/ipc.h>   
#include <sys/msg.h> 
#include "cias_config.h"
#include "cias_crc.h"

FILE* file_input;
FILE* file_output;
char ota_partation_name[5][12] = {
    "user_code2",
    "asr",
	"dnn",
    "voice",
    "user_file"
};

char ota_partition_enable[5] = {1,1,1,1,1};

int main(int argc, char *argv[])
{
    char *file_name = argv[1];
	char *firmware = NULL;
	int read_len;
	uint32_t firmware_size;
    file_input = fopen(file_name, "r");
	if (!file_input)
	{
		printf("固件打开失败!!!\r\n");
		return false;
	}

	//读固件分区表
	partition_table_t *partition_table = malloc(sizeof(partition_table_t));  
	fseek(file_input, HEAD_LENGTH, SEEK_SET); 
	read_len = fread(partition_table, sizeof(uint8_t), sizeof(partition_table_t), file_input);
	if (read_len != sizeof(partition_table_t))
	return false; 
	print_partition_table(partition_table);
	//重新计算校验和
	partition_table->patitiontablechecksum = get_partition_table_crc(partition_table);
	//设置分区状态为需要升级
	partition_table->user_code1_status = 0xF0;
	partition_table->user_code2_status = ota_partition_enable[0] ? 0xFF : 0xF0	;
	partition_table->asr_cmd_model_status = ota_partition_enable[1] ? 0xFF : 0xF0	;
	partition_table->dnn_model_status = ota_partition_enable[2] ? 0xFF : 0xF0	;
	partition_table->voice_status = ota_partition_enable[3] ? 0xFF : 0xF0	;
	partition_table->user_file_status = ota_partition_enable[4] ? 0xFF : 0xF0	;

	// char output[100] = {0};
    // // sprintf(output, "ota_firmware.bin");
    // remove(output); 
	// FILE* file_output = fopen(output, "w+");
    // if (!file_output) {
    //     printf("%s 文件创建失败", output);
    //     return false;
    // }
	char output[100] = {0};
	const char *input_name = file_name;
	const char *filename = strrchr(input_name, '/');
	
	if (filename != NULL)
	{
	    filename++;  // 跳过 '/'
	}
	else
	{
	    filename = input_name;
	}
	
	/* 去掉开头的 full_ */
	if (strncmp(filename, "full_", 5) == 0)
	{
	    filename += 5;
	}
	
	snprintf(output, sizeof(output), "ota_%s", filename);
	
	printf("输入文件: %s\n", file_name);
	printf("输出文件: %s\n", output);
	
	remove(output);
	
	file_output = fopen(output, "wb+");
	if (!file_output)
	{
	    printf("%s 文件创建失败\n", output);
	    return false;
	}
	
	//获取需要ota的分区数量和名字
	char *head = malloc(4096);
    head[3] = 0;
	uint32_t head_len = 2;
	uint32_t ota_output_offset = 0;
	for (size_t i = 0; i < 5; i++)
	{
		if (ota_partition_enable[i] == 1)
		{
			head[3] ++;
			memcpy(&head[head_len+2],ota_partation_name[i], strlen(ota_partation_name[i]));
			head_len += strlen(ota_partation_name[i]);
		}
		
	}
	uint16_t head_crc = crc16_ccitt(0, &head[2], head_len);
	head[0] = head_len >> 8;
	head[1] = head_len;
	head[head_len + 2] = head_crc >> 8;
	head[head_len + 3] = head_crc;
	//ota固件头填入需要ota的分区信息
	fseek(file_output, ota_output_offset, SEEK_SET); 
	fwrite(head, sizeof(uint8_t), head_len + 4, file_output);
	ota_output_offset += PACK_LENGTH_4K;

	//ota固件填入分区信息
	fseek(file_output, ota_output_offset, SEEK_SET); 
    memset(head, 0xff, PACK_LENGTH_4K);
    memcpy(head, partition_table, sizeof(partition_table_t));
    fwrite(head, sizeof(uint8_t), sizeof(partition_table_t), file_output);
	ota_output_offset += PACK_LENGTH_4K;

	uint32_t partition_firmware_size = 0;
	for (size_t i = 0; i < 5; i++)
	{
		if (ota_partition_enable[i] == 1)
		{
			fseek(file_output, ota_output_offset, SEEK_SET); 
			if (i == 0) //需要升级 user_code2
			{
				fseek(file_input, partition_table->user_code2_offset, SEEK_SET); 
				firmware_size = partition_table->asr_cmd_model_offset - partition_table->user_code2_offset;
			}
			else if (i == 1) //需要升级 asr
			{
				fseek(file_input, partition_table->asr_cmd_model_offset, SEEK_SET); 
				firmware_size = partition_table->dnn_model_offset - partition_table->asr_cmd_model_offset;
			}
			else if (i == 2) //需要升级 dnn
			{
				fseek(file_input, partition_table->dnn_model_offset, SEEK_SET); 
				firmware_size = partition_table->voice_offset - partition_table->dnn_model_offset;
			}
			else if (i == 3)//需要升级voice
			{
				fseek(file_input, partition_table->voice_offset, SEEK_SET); 
				firmware_size = partition_table->user_file_offset - partition_table->voice_offset;
			}
			else if (i == 4)//需要升级user_file
			{
				fseek(file_input, partition_table->user_file_offset, SEEK_SET); 
				firmware_size = partition_table->user_file_size;
			}
			firmware = malloc(firmware_size);
			int len = fread(firmware, sizeof(uint8_t), firmware_size, file_input);
			if (len != firmware_size) {
				printf("%s 文件读取失败");
				return false;
			}
			len = fwrite(firmware, sizeof(uint8_t), firmware_size, file_output);
			if (len != firmware_size) {
				printf("%s 文件写入失败");
				return false;
			}
			ota_output_offset += firmware_size;
			free(firmware);
		}
	}
	
	fclose(file_output);
	fclose(file_input);

}

