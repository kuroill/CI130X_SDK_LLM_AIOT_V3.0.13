#ifndef __FLASH_MANAGE_H
#define __FLASH_MANAGE_H

#include <stdint.h>
#include <stdbool.h>


#define FLASH_SIZE_DMA_TH   (15*1024)//超过这个值，读取flash使用DMA M2M;这个不可大于16K，DMA一次搬运不可大于等于16K


#ifdef __cplusplus
extern "C" {
#endif


typedef enum
{
    FLASH_INIT_OPERTATE = 1<<0,
    FLASH_POWER_OFF_OPERTATE = 1<<1,
    FLASH_READ_OPERTATE = 1<<2,
    FLASH_WRITE_OPERTATE = 1<<3,
    FLASH_ERASE_OPERTATE = 1<<4,
    FLASH_READ_UNIQUE_ID_OPERTATE = 1<<5,
    // FLASH_OPERATE_MAX_NUM = 6,

    //中断里面请求的操作（主要是mailbox中断）
    FLASH_READ_OPERTATE_INT,
    FLASH_ERASE_OPERTATE_INT,
    FLASH_WRITE_OPERTATE_INT,
    FLASH_READ_UNIQUE_ID_OPERTATE_INT,
}flash_operate_type_t;


typedef struct 
{
    flash_operate_type_t op_type;//flash操作选择：init、power off、read、write、erase
    uint32_t dst_addr;//地址1：dst地址，erase地址
    uint32_t src_addr;//地址2：src地址
    uint32_t op_size;//大小：read、write、erase的大小
}flash_ctr_msg_t;//flash控制消息的结构体;


void flash_manage_flow(flash_ctr_msg_t* msg_p);
void flash_manage_wait_op_done_sem(flash_operate_type_t op_type);
void flash_manage_init(void);
void is_flash_power_off(bool* state);
void flash_manage_read(uint32_t dst_addr,uint32_t src_addr,uint32_t size);


#ifdef __cplusplus
}
#endif

#endif 