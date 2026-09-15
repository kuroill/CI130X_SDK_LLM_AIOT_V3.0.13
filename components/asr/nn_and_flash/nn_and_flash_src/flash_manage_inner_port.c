
#include "nn_and_flash_manage.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "croutine.h"
#include "semphr.h"
#include "ci130x_system_ept.h"
#include "ci130x_nuclear_com.h"
#include <string.h>
#include <stdlib.h>


#define FM_CI_ASSERT(x,msg)                                                                                                    \
    if( ( x ) == 0 )                                                                                                        \
    {                                                                                                                       \
        ci_logdebug(LOG_SYS_INFO, "%s",msg);                                                                                                   \
        ci_logdebug(LOG_SYS_INFO, "FM Line:%d\n",__LINE__);                                                                                   \
        while(1)  asm volatile ("ebreak");                                                                                  \
    }


#define NUCLER_COM_JUDGE_PARA_NUM_FM    (0)


static void flash_manage_ept_cal_outside_serve(char* para,uint32_t size)
{
    nuclear_com_t str;
    // MASK_ROM_LIB_FUNC->newlibcfunc.memset_p((void*)&str,0,sizeof(str));
    str.src_ept_num = 0;
    str.dst_ept_num = flash_manage_serve_outside_ept_num;
    str.data_p = (void*)para;
    str.data_len = size;
    nuclear_com_send(&str,0xfffff);
}


void post_read_flash_int_op_done(void)
{
    uint32_t para[1];
    para[0] = post_read_flash_op_done_ept_num;
    flash_manage_ept_cal_outside_serve((char*)para,sizeof(para));
}


void post_erase_flash_int_op_done(void)
{
    uint32_t para[1];
    para[0] = post_erase_flash_op_done_ept_num;
    flash_manage_ept_cal_outside_serve((char*)para,sizeof(para));
}


void post_write_flash_int_op_done(void)
{
    uint32_t para[1];
    para[0] = post_write_flash_op_done_ept_num;
    flash_manage_ept_cal_outside_serve((char*)para,sizeof(para));
}

void post_read_unique_id_int_op_done(void)
{
    uint32_t para[1];
    para[0] = post_spic_read_unique_id_op_done_ept_num;
    flash_manage_ept_cal_outside_serve((char*)para,sizeof(para));
}

#define FLASH_MANAGE_MAX_PARA_NUM    (6)
static uint32_t flash_manage_nuclear_com_buf[FLASH_MANAGE_MAX_PARA_NUM];

static int32_t flash_manage_nuclear_com_inner_serve_cb(void *payload, uint32_t payload_len, uint32_t src, void *priv)
{
    // MASK_ROM_LIB_FUNC->newlibcfunc.memcpy_p((void*)flash_manage_nuclear_com_buf,(void*)payload,payload_len);
    uint32_t* data = (uint32_t*)payload;

    switch(data[0])
    {
        case is_flash_power_off_ept_num:
        {
            #if NUCLER_COM_JUDGE_PARA_NUM_FM
            if(8 != payload_len)
            {
                // ci_logdebug(LOG_SYS_INFO, "payload_len = %d\n",payload_len);
                FM_CI_ASSERT(0,"\n");
            }
            #endif
            //模块内部实现
            bool* state = (bool*)data[1];
            is_flash_power_off(state);
            break;
        }
        case post_read_flash_ept_num:
        {
            #if NUCLER_COM_JUDGE_PARA_NUM_FM
            if(16 != payload_len)
            {
                ci_logdebug(LOG_SYS_INFO, "payload_len = %d\n",payload_len);
                FM_CI_ASSERT(0,"\n");
            }
            #endif
            //模块内部实现
            char* buf = (char*)data[1];
            uint32_t addr = data[2];
            uint32_t size = data[3];
            post_read_flash_int(buf,addr,size);
            break;
        }
        case post_erase_flash_ept_num:
        {
            #if NUCLER_COM_JUDGE_PARA_NUM_FM
            if(12 != payload_len)
            {
                ci_logdebug(LOG_SYS_INFO, "payload_len = %d\n",payload_len);
                FM_CI_ASSERT(0,"\n");
            }
            #endif
            //模块内部实现
            uint32_t addr = data[1];
            uint32_t size = data[2];
            post_erase_flash_int(addr,size);
            break;
        }
        case post_write_flash_ept_num:
        {
            #if NUCLER_COM_JUDGE_PARA_NUM_FM
            if(16 != payload_len)
            {
                ci_logdebug(LOG_SYS_INFO, "payload_len = %d\n",payload_len);
                FM_CI_ASSERT(0,"\n");
            }
            #endif
            //模块内部实现
            char* buf = (char*)data[1];
            uint32_t addr = data[2];
            uint32_t size = data[3];
            post_write_flash_int(buf,addr,size);
            break;
        }
        case post_spic_read_unique_id_ept_num:
        {
            #if NUCLER_COM_JUDGE_PARA_NUM_FM
            if(8 != payload_len)
            {
                ci_logdebug(LOG_SYS_INFO, "payload_len = %d\n",payload_len);
                FM_CI_ASSERT(0,"\n");
            }
            #endif
            //模块内部实现
            char* buf = (char*)data[1];
            post_spic_read_unique_id_int((uint8_t*)buf);
            break;
        }
        default:
            break;
    }
    return 0;
}


void flash_manage_nuclear_com_inner_port_init(void)
{
    nuclear_com_registe_serve(flash_manage_nuclear_com_inner_serve_cb,flash_manage_serve_inner_ept_num);
}



