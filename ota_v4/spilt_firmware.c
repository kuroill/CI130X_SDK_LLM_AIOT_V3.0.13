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


uint16_t get_partition_table_crc(partition_table_t *partition_table)
{
    int len = sizeof(partition_table_t) - 2;
    uint16_t sum = 0;
    uint8_t user_code1_status = partition_table->user_code1_status;
    uint8_t user_code2_status = partition_table->user_code2_status;
    uint8_t asr_cmd_model_status = partition_table->asr_cmd_model_status;
    uint8_t dnn_model_status = partition_table->dnn_model_status;
    uint8_t voice_status = partition_table->voice_status;
    uint8_t user_file_status = partition_table->user_file_status;

    partition_table->user_code1_status = 0xF0;
    partition_table->user_code2_status = (partition_table->FirmwareFormatVer == 1) ? 0xF0 : 0xFF;
    partition_table->asr_cmd_model_status = 0;
    partition_table->dnn_model_status = 0;
    partition_table->voice_status = 0;
    partition_table->user_file_status = 0;
    
    for(int i = 0; i< len; i++)
    {
        sum += ((uint8_t*)partition_table)[i];
    }
    partition_table->user_code1_status = user_code1_status;
    partition_table->user_code2_status = user_code2_status;
    partition_table->asr_cmd_model_status = asr_cmd_model_status;
    partition_table->dnn_model_status = dnn_model_status;
    partition_table->voice_status = voice_status;
    partition_table->user_file_status = user_file_status;
    return sum;
}

void print_partition_table(partition_table_t *partition_table)
{
    uint16_t par_crc = get_partition_table_crc(partition_table);
    printf("user_code1_offset = 0x%x\n", partition_table->user_code1_offset);
    printf("user_code1_size = 0x%x\n", partition_table->user_code1_size);
    //printf("user_code1_crc = 0x%x\n", partition_table->user_code1_crc);
    printf("user_code1_status = 0x%x\n", partition_table->user_code1_status);
    printf("user_code2_offset = 0x%x\n", partition_table->user_code2_offset);
    printf("user_code2_size = 0x%x\n", partition_table->user_code2_size);
    //printf("user_code2_crc = 0x%x\n", partition_table->user_code2_crc);
    printf("user_code2_status = 0x%x\n", partition_table->user_code2_status);
    printf("\r\n");
    printf("asr_cmd_model_offset = 0x%x\n", partition_table->asr_cmd_model_offset);
    printf("dnn_model_offset = 0x%x\n", partition_table->dnn_model_offset);
    printf("voice_offset = 0x%x\n", partition_table->voice_offset);
    printf("user_file_offset = 0x%x\n", partition_table->user_file_offset);
    printf("user_file_size = 0x%x\n", partition_table->user_file_size);
    printf("par_crc = %x %x\r\n", partition_table->patitiontablechecksum, par_crc);
}

uint32_t get_erase_area(uint32_t size)
{
    uint32_t earse_page = size/PACK_LENGTH_4K;
    if (size%PACK_LENGTH_4K)
        earse_page ++;
    return (earse_page*PACK_LENGTH_4K);
}
