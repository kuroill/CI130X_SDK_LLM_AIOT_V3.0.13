/**
 * @file 
 * @brief 
 * @version 1.0.0
 * @date 2021-03-19
 *
 * @copyright Copyright (c) 2019  Chipintelli Technology Co., Ltd.
 *
 */
#include <stdio.h> 
#include "FreeRTOS.h" 
#include "task.h"
#include "queue.h"
#include "sdk_default_config.h"
#include "ci130x_core_eclic.h"
#include "ci130x_nuclear_com.h"
#include "ci_log.h"
#include "ci_assert.h"
#include "nuclear_com_task_bnpu.h"
#include "ci130x_core_misc.h"


// SemaphoreHandle_t asrtop_pause_continue_semaphore = NULL;
QueueHandle_t  nuclear_com_bnpu_xQueuemsg = NULL;


void send_msg_to_nuclear_com_task(nuclear_com_bnpu_msg_t* msg_p)
{
    BaseType_t ret;
    BaseType_t xTaskWokenByReceive = pdFALSE;
    if(check_curr_trap())
    {   
        ret = xQueueSendFromISR(nuclear_com_bnpu_xQueuemsg,msg_p,&xTaskWokenByReceive);
        portEND_SWITCHING_ISR(xTaskWokenByReceive);
    }
    else
    {
        ret = xQueueSend(nuclear_com_bnpu_xQueuemsg,msg_p,0);
    }
    if(pdPASS != ret)
    {
        mprintf("nuclear_com_bnpu_xQueuemsg send err\n");
    }
}


void nuclear_com_bnpu_task(void* p)
{
    nuclear_com_bnpu_xQueuemsg = xQueueCreate(10, sizeof(nuclear_com_bnpu_msg_t));//TODO 是否需要这么多
    if(!nuclear_com_bnpu_xQueuemsg)
    {
        CI_ASSERT(0,"\n");
    }

    nuclear_com_bnpu_msg_t msg;

    for(;;)
    {
        if(pdPASS == xQueueReceive(nuclear_com_bnpu_xQueuemsg,&msg,pdMS_TO_TICKS(portMAX_DELAY)))
        {
            asr_rpmsg_ept_num_t serve_num = msg.ept_num;
            switch(serve_num)
            {
                case asrtop_asr_system_continue_ept_num:
                {
                    //执行asr continue的操作
                    extern int asrtop_asr_system_continue(void);
                    asrtop_asr_system_continue();

                    //等待asr continue的操作完成，之后，发送消息通知另外一个核
                    void asrtop_asr_system_continue_done_isr(void);
                    asrtop_asr_system_continue_done_isr();
                    break;
                }
                case asrtop_asr_system_pause_ept_num:
                {
                    //执行asr pause的操作
                    extern int asrtop_asr_system_pause(void);
                    asrtop_asr_system_pause();

                    //等待asr pause的操作完成，之后，发送消息通知另外一个核
                    extern void asrtop_asr_system_pause_done_isr(void);
                    asrtop_asr_system_pause_done_isr();
                    break;
                }
                case asrtop_asr_system_create_model_ept_num:
                {
                    extern int asrtop_asr_system_create_model(unsigned int lg_model_addr,unsigned int lg_model_size,
                                                                unsigned int ac_model_addr,unsigned int ac_model_size,void* pdata);
                    unsigned int lg_model_addr = msg.data_p[0];
                    unsigned int lg_model_size = msg.data_p[1];
                    unsigned int ac_model_addr = msg.data_p[2];
                    unsigned int ac_model_size = msg.data_p[3];
                    void* pdata = (void*)msg.data_p[4];
                    asrtop_asr_system_create_model(lg_model_addr,lg_model_size,ac_model_addr,ac_model_size,pdata);
                    extern void asrtop_asr_system_create_model_done_isr(void);
                    asrtop_asr_system_create_model_done_isr();
                    break;
                }
                default:
                {
                    CI_ASSERT(0,"\n");
                }
            }
        }
    }
}




