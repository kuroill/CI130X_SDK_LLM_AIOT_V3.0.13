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
#include "ci130x_system_ept.h"


typedef struct 
{
    uint32_t data_p[6];
    uint8_t data_num;
    asr_rpmsg_ept_num_t ept_num;
}nuclear_com_bnpu_msg_t;

void send_msg_to_nuclear_com_task(nuclear_com_bnpu_msg_t* msg_p);

