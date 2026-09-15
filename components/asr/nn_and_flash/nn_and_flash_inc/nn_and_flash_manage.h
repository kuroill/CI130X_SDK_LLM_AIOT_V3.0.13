#ifndef __NN_AND_FLASH_MANAGE_H
#define __NN_AND_FLASH_MANAGE_H

#include <stdint.h>
#include <stdbool.h>
#include "flash_manage.h"
#include "ci_nn_manage.h"

#ifdef __cplusplus
extern "C" {
#endif


typedef enum
{
    NN_AND_FLASH_CHOSE_NN = 0,
    NN_AND_FLASH_CHOSE_FLASH = 1,
    NN_AND_FLASH_REQ_NN_CLEAR,
    NN_AND_FLASH_REQ_NN_INIT,
}nn_and_flash_chose_t;



typedef struct 
{
    nn_and_flash_chose_t nn_or_flash;
    union 
    {
        flash_ctr_msg_t flash_ctr_info;
        nn_ctr_msg_t nn_ctr_info;
    };
}nn_and_flash_manage_msg_t;


void req_denoise_nn_cmpt(ci_denoise_nn_cmpt_info_t* cmpt_p);
void req_doa_tdnn_cmpt(ci_nn_doa_tdnn_cmpt_info_t* cmpt_p);
void req_vp_nn_cmpt(ci_nn_ecapa_tdnn_info_t* cmpt_p);

void denoise_nn_cmpt_done(void);
void doa_tdnn_cmpt_done(void);
void nn_and_flash_manage_task(void* p);

void req_flash_init(void);

int32_t post_read_flash(char *buf, uint32_t addr, uint32_t size);
int32_t post_read_flash_int(char *buf, uint32_t addr, uint32_t size);

int32_t post_write_flash(char *buf, uint32_t addr, uint32_t size);
int32_t post_write_flash_int(char *buf, uint32_t addr, uint32_t size);

int32_t post_erase_flash(uint32_t addr, uint32_t size);
int32_t post_erase_flash_int(uint32_t addr, uint32_t size);

int32_t post_spic_read_unique_id(uint8_t* buf);
int32_t post_spic_read_unique_id_int(char* buf);

bool get_nn_and_flash_task_state(void);

void req_cinn_clear(void);
void cinn_wait_clear_done(void);

extern volatile int sys_err_flag;


#ifdef __cplusplus
}
#endif

#endif 