#ifndef __CIAS_OTA_CONFIG_H__
#define __CIAS_OTA_CONFIG_H__
#include <stdint.h>
#include <stdbool.h>
#define DEBUG_LOG 1
#define OUT_DEBUG_INFO 1

#define HEAD_LENGTH                        0x6000                 //ota时只需要从0x6000开始取数据，前面是boot等无效信息
#define PACK_LENGTH_4K                     4096                 

#pragma pack(1)
typedef struct
{
    uint32_t manu_facturer_id;    //32Bit
    uint32_t product_id[2];      //64Bit (MAC Address)
//    
    uint32_t hard_ware_name[16];        //String
    uint32_t hard_ware_version;         //Vm.n.x.y
    uint32_t soft_ware_name[16];        //String,Exporting to Packet file name
    uint32_t soft_ware_version;         //Vm.n.x.y
//    
    uint32_t bootloader_version; //Vm.n.x.y
    char ChipName[9];
    uint8_t FirmwareFormatVer;
    uint8_t reserve[4];
//    
    uint32_t user_code1_version;
    uint32_t user_code1_offset;
    uint32_t user_code1_size;
    uint32_t user_code1_crc;
    uint8_t user_code1_status;
//    
    uint32_t user_code2_version;
    uint32_t user_code2_offset;
    uint32_t user_code2_size;
    uint32_t user_code2_crc;
    uint8_t user_code2_status;
//    
    uint32_t asr_cmd_model_version;
    uint32_t asr_cmd_model_offset;
    uint32_t asr_cmd_model_size;
    uint32_t asr_cmd_model_crc;
    uint8_t asr_cmd_model_status;
//    
    uint32_t dnn_model_version;
    uint32_t dnn_model_offset;
    uint32_t dnn_model_size;
    uint32_t dnn_model_crc;
    uint8_t dnn_model_status;
//    
    uint32_t voice_version;
    uint32_t voice_offset;
    uint32_t voice_size;
    uint32_t voice_crc;
    uint8_t voice_status;
//
    uint32_t user_file_version;
    uint32_t user_file_offset;
    uint32_t user_file_size;
    uint32_t user_file_crc;
    uint8_t user_file_status;
//
    uint32_t nv_data_offset;
    uint32_t nv_data_size;

    uint16_t patitiontablechecksum;
    //uint32_t partition_table_crc32;
}partition_table_t; 


#pragma pack()
uint32_t get_erase_area(uint32_t size);
void print_partition_table(partition_table_t *partition_table);
uint16_t get_partition_table_crc(partition_table_t *partition_table);
bool spilt_firmware(char* output_file_name, FILE* file_input, partition_table_t *partition_table, uint32_t offset, uint32_t size);
#endif


