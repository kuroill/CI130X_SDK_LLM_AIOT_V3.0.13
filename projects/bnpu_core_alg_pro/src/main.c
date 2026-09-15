/**
 * @file main.c
 * @brief 示例程序
 * @version 1.0.0
 * @date 2021-03-19
 *
 * @copyright Copyright (c) 2019  Chipintelli Technology Co., Ltd.
 *
 */
#include <stdio.h> 
#include <malloc.h>
#include "FreeRTOS.h" 
#include "task.h"
#include "sdk_default_config.h"
#include "ci130x_core_eclic.h"
#include "ci130x_spiflash.h"
#include "ci130x_gpio.h"
// #include "audio_play_api.h"
// #include "audio_play_decoder.h"
#include "ci_flash_data_info.h"
//#include "ci130x_audio_capture.h"
#include "board.h"
#include "asr_api.h"
// #include "dnn_inner_port_rpmsg.h"
// #include "asr_top_inner_port_rpmsg.h"
#include "ci130x_uart.h"
// #include "nn_and_flash_manage.h"
// #include "flash_manage_inner_port.h"
// #include "decoder_outside_port.h"
// #include "vad_fe_inner_port_rpmsg.h"
#include "ci130x_mailbox.h"
#include "ci130x_nuclear_com.h"
#include "ci130x_dpmu.h"
#include "status_share.h"
#include "ci130x_core_misc.h"
#include "alg_preprocess.h"
/* 音频输入任务句柄及系统监控id */
uint8_t audio_in_preprocess_mode_id;
TaskHandle_t audio_in_preprocess_mode_handle;

/**
 * @brief 硬件初始化
 *          这个函数主要用于系统上电后初始化硬件寄存器到初始值，配置中断向量表初始化芯片io配置时钟
 *          配置完成后，系统时钟配置完毕，相关获取clk的函数可以正常调用
 */
static void hardware_default_init(void)
{
    /* 配置外设复位，硬件外设初始化 */
	// extern void SystemInit(void);
    // SystemInit();

	/* 设置中断优先级分组 */
	eclic_priority_group_set(ECLIC_PRIGROUP_LEVEL3_PRIO0);
    
	/* 开启全局中断 */
	eclic_global_interrupt_enable();

	enable_mcycle_minstret();

	init_platform();
}


/**
 * @brief 用于平台初始化相关代码
 *
 * @note 在这里初始化硬件需要注意：
 *          由于部分驱动代码中使用os相关接口，在os运行前调用这些接口会导致中断被屏蔽
 *          其中涉及的驱动包括：QSPIFLASH、DMA、I2C、SPI
 *          所以这些外设的初始化需要放置在vTaskVariablesInit进行。
 *          如一定需要（非常不建议）在os运行前初始化这些驱动，请仔细确认保证：
 *              1.CONFIG_DIRVER_BUF_USED_FREEHEAP_EN  宏配置为0
 *              2.DRIVER_OS_API                     宏配置为0
 */
static int platform_init(void)
{
    #if CONFIG_CI_LOG_UART
    ci_log_init();      //初始化日志模块
    #endif

    #if (CONFIG_CI_LOG_UART == UART_PROTOCOL_NUMBER && MSG_COM_USE_UART_EN)
	CI_ASSERT(0,"Log uart and protocol uart confict!\n");
    #endif
    
    #if CONFIG_SYSTEMVIEW_EN   
    /* 初始化SysView RTT，仅用于调试 */
    SEGGER_SYSVIEW_Conf();
    /* 使用串口方式输出sysview信息 */
    vSYSVIEWUARTInit();
    ci_logdebug(CI_LOG_DEBUG, "Segger Sysview Control Block Detection Address is 0x%x\n",&_SEGGER_RTT);
    #endif

	return 0;
}

/**
 * @brief sdk上电信息打印
 *
 */
static void welcome(void)
{
    ci_loginfo(LOG_USER,"\r\n");
    ci_loginfo(LOG_USER,"\r\n");
    ci_loginfo(LOG_USER,"ci130x_sdk_%s_%d.%d.%d Built-in\r\n",
               SDK_TYPE,
               SDK_VERSION,SDK_SUBVERSION,SDK_REVISION);
    ci_loginfo(LOG_USER,"\033[1;32mWelcome to ci130x_sdk.\033[0;39m\r\n");
    
}


extern TaskHandle_t nn_and_flash_task_handle;

static void task_init(void *p_arg)
{
    //DMA通道中断开启
    scu_set_dma_mode(DMAINT_SEL_CHANNEL0);

    //复位NPU
    scu_set_device_gate(HAL_NPU_BASE,ENABLE);
    scu_set_device_reset(HAL_NPU_BASE);
    scu_set_device_reset_release(HAL_NPU_BASE);
    npu_load_w_int_en(ENABLE);

    extern void flash_init_to_xip(void);
    flash_init_to_xip();

	//TODO 是否应该放在这里
    //nuclear_com_init需在mailboxboot_sync之前
    nuclear_com_init();

    extern void ci_set_fe_is_reduce_mem(int is_8bit);
    #if ASR_FE_REDUCE_MEM
        ci_set_fe_is_reduce_mem(1);
    #else
        ci_set_fe_is_reduce_mem(0);
    #endif
    mprintf("ASR_FE_REDUCE_MEM=%d\n",ASR_FE_REDUCE_MEM);

    /*各个通信组件的初始化*/
    asr_top_port_inner_rpmsg_init();
    decoder_nuclear_com_outside_port_init();
    flash_manage_nuclear_com_inner_port_init();
    audio_in_rpmsg_init();
    /*各个通信组件的初始化 end*/
	mailboxboot_sync();  
    ciss_init();//bnpu的ciss_init需要在mailboxboot_sync之后调用
	//之后该核心正常运行
    /* flash固件信息解析并初始化固件信息结构，DEFAULT_MODEL_GROUP_ID为默认模型分组ID，开机后第一次运行的识别环境 */
    extern void nn_and_flash_manage_task(void* p);
    xTaskCreate(nn_and_flash_manage_task,"nn_and_flash_manage_task",512,NULL,4,&nn_and_flash_task_handle);

    
    //等待主核ci_flash_data_info_init初始化完成，这个组件初始化之后，ASR才能读取模型信息进行初始化
    bool flash_data_info_inited_flag;
    is_ci_flash_data_info_inited(&flash_data_info_inited_flag);
    while(!flash_data_info_inited_flag)
    {
        vTaskDelay(pdMS_TO_TICKS(2));
        is_ci_flash_data_info_inited(&flash_data_info_inited_flag);
    }
    extern void nuclear_com_bnpu_task(void* p);
    xTaskCreate(nuclear_com_bnpu_task,"nuclear_com_bnpu_task",168+64+256,NULL,4,NULL);
    //初始化npu
    npu_init();
    #if !NO_ASR_FLOW
    /* 启动DNN配置*/
    if(!dnn_module_reg_by_user_cfg())
    {
        CI_ASSERT(0, "\n");
    }
    #endif
    /* ASR系统启动任务 */
	#if !NO_ASR_FLOW
	xTaskCreate(asr_system_startup_task,"asr_system_startup_task",512,NULL,4,NULL);
    #endif
    
    /* 语音输入任务 */
    extern void audio_in_preprocess_mode_task(void* p);
	#if USE_SED
    xTaskCreate(audio_in_preprocess_mode_task,"audio_in_preprocess",1024,&audio_in_preprocess_mode_id,4,&audio_in_preprocess_mode_handle);
	extern void sed_manage_task(void* p);
    xTaskCreate(sed_manage_task,"sed_manage_task",512,0,4,0);
	#else
	xTaskCreate(audio_in_preprocess_mode_task,"audio_in_preprocess",480,&audio_in_preprocess_mode_id,4,&audio_in_preprocess_mode_handle);
	#endif
    #if !SDK_RELEASE_EN
    while(1) 
    {
        UBaseType_t ArraySize;
        TaskStatus_t *StatusArray;
        uint8_t x;
        ArraySize = 10;
        StatusArray = pvPortMalloc(ArraySize*sizeof(TaskStatus_t));
        vTaskDelay(pdMS_TO_TICKS(3000));
        while(StatusArray)
        {
            vTaskSuspendAll();
            UBaseType_t ArraySize2 = uxTaskGetSystemState(StatusArray, ArraySize, NULL);
            mprintf("TaskName\t\tPriority\tTaskNumber\tMinStk\t%d\n", ArraySize2);
            for (int i = 0;i < ArraySize2;i++)
            {
                mprintf("% -16s\t%d\t\t%d\t\t%d\r\n",
                    StatusArray[i].pcTaskName,
                    (int)StatusArray[i].uxCurrentPriority,
                    (int)StatusArray[i].xTaskNumber,
                    (int)StatusArray[i].usStackHighWaterMark);
            }
            mprintf("\n\n");
            mprintf("HEAP free:%d\n",xPortGetFreeHeapSize());
            mprintf("HEAP_644 free:%d\n",xPortGetFreeHeapSize_644());
            xTaskResumeAll();
            vTaskDelay(pdMS_TO_TICKS(10000));
        }
    }
    #endif
    vTaskDelete(NULL);
}

/**
 * @brief 主函数，进入应用程序的入口
 */
int main(void)
{
    hardware_default_init();

    /*平台相关初始化*/
    platform_init();

    /* 版本信息 */
    welcome();
    /* 创建启动任务 */
    xTaskCreate(task_init,"init task",280,NULL,4,NULL);

    /* 启动调度，开始执行任务 */
    vTaskStartScheduler();

    while(1){}
}


