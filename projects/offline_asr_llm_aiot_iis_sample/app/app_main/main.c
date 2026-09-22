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
#include "ci130x_dma.h"
#include "ci_flash_data_info.h"
// #include "ci130x_audio_capture.h"
#include "board.h"
#include "ci130x_uart.h"
#include "flash_manage_outside_port.h"
#include "system_msg_deal.h"
#include "ci130x_dpmu.h"
#include "ci130x_mailbox.h"
#include "ci130x_nuclear_com.h"
#include "flash_control_inner_port.h"
#include "romlib_runtime.h"
#include "audio_in_manage_inner.h"
#include "ci_log.h"
#include "status_share.h"
#include "platform_config.h"
#include "asr_api.h"
#include "alg_preprocess.h"
#include "ci130x_iwdg.h"
#include "voice_print_recognition.h"
#include "cwsl_manage.h"
#include "codec_manager.h"
#include "cias_audio_data_handle.h"
#include "ota_partition_verify.h"
#include "doa_app_handle.h"
#include "codec_manage_outside_port.h"
#include "code_switch.h"
#include "ci_nlp_user.h"
#include "ci_nlp.h"
#include "cias_network_msg_protocol.h"
#include "timers.h"
#include "ci_agc.h"
#include "user_config.h"
#include "ai_uart_i2s_protocol.h"
#if !SIMPLE_AUDIO_PLAYER_ENABLE
#include "audio_play_api.h"
#include "audio_play_decoder.h"
#endif
/**
 * @brief 硬件初始化
 *          这个函数主要用于系统上电后初始化硬件寄存器到初始值，配置中断向量表初始化芯片io配置时钟
 *          配置完成后，系统时钟配置完毕，相关获取clk的函数可以正常调用
 */
static void hardware_default_init(void)
{
    /* 配置外设复位，硬件外设初始化 */
	extern void SystemInit(void);
    SystemInit();

	/* 设置中断优先级分组 */
	eclic_priority_group_set(ECLIC_PRIGROUP_LEVEL3_PRIO0);

	/* 开启全局中断 */
	eclic_global_interrupt_enable();

	enable_mcycle_minstret();

	init_platform();

    /* 初始化maskrom lib */
    maskrom_lib_init();

    #if !(USE_INNER_LDO3)
    dpmu_ldo3_en(false);
    dpmu_config_update_en(DPMU_UPDATE_EN_NUM_LDO3);
    #endif

    //DMA通道中断开启
    scu_set_dma_mode(DMAINT_SEL_CHANNEL1);
    scu_set_device_reset(HAL_GDMA_BASE);
    scu_set_device_reset_release(HAL_GDMA_BASE);
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
    #if COMMAND_LINE_CONSOLE_EN         
    vUARTCommandConsoleStart( 256, 4);
    #endif
    #endif
    #if (CONFIG_CI_LOG_UART == UART_PROTOCOL_NUMBER && MSG_COM_USE_UART_EN)
	CI_ASSERT(0,"Log uart and protocol uart confict!\r\n");
    #endif
    
    #if CONFIG_SYSTEMVIEW_EN   
    /* 初始化SysView RTT，仅用于调试 */
	SEGGER_SYSVIEW_Conf();
	/* 使用串口方式输出sysview信息 */
	vSYSVIEWUARTInit();
	ci_logdebug(CI_LOG_DEBUG, "Segger Sysview Control Block Detection Address is 0x%x\r\n",&_SEGGER_RTT);
#endif
#if 1   //开启看门狗
    iwdg_init_t init;
    init.irq = iwdg_irqen_enable;
    init.res = iwdg_resen_enable;
    init.count = ((get_src_clk()/0x10)*3);/* IWDG时钟从src_clk经过16分频得到, 当前配置为2秒*/
    scu_set_device_gate(IWDG, ENABLE);
    dpmu_iwdg_reset_system_config();
    iwdg_init(IWDG,init);
    iwdg_open(IWDG);
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
    ci_loginfo(LOG_USER,"ci130x_sdk_%s_%s_%d.%d.%d Built-in\r\n",
               SDK_TYPE,SDK_PUB_TYPE,
               SDK_VERSION,SDK_SUBVERSION,SDK_REVISION);
    ci_loginfo(LOG_USER,"\033[1;32mWelcome to CI130x_SDK.\033[0;39m\r\n");
    extern char heap_start;
    extern char heap_end;
    ci_loginfo(LOG_USER,"Heap size:%dKB\r\n", (((uint32_t)&heap_end) - ((uint32_t)&heap_start))/1024);
    ci_loginfo(LOG_USER,"Freq factor %d\r\n", (int)(get_freq_factor()*1000));
    ci_loginfo(LOG_USER,"Freq %d\r\n", (int)(get_ipcore_clk()));

    // 实际主频检查
    if (abs(((int)get_ipcore_clk()) - ((int)MAIN_FREQUENCY)) > 10000000)
    {
        mprintf("PLL config err!\r\n");
        while(1);
    }
}
//算法模型初始化
static int alg_model_init(void)
{
    #if USE_VPR
    vpr_init(vpr_callback);
    #endif
    #if USE_WMAN_VPR
    vpr_init(NULL); 
    #endif
    #if USE_NN_DENOISE
    get_ci_nn_denoise_model_addr();
    #endif
    #if USE_NN_DOA
    get_ci_nn_doa_model_addr();
    #if !USE_AEC_MODULE
    REMOTE_CALL(set_doa_out_type(NN_DOA_OUT_TYPE));
    #endif
    #endif
    #if USE_SED  
    REMOTE_CALL(sed_nn_cmpt_cfg(200,0));     //配置哭声检测一次的帧数，10ms一帧
    extern void get_ci_sed_model_addr(void);
    get_ci_sed_model_addr();
    #endif
    return 0;
}
#if CLOUD_UART_PROTOCOL_EN
#include "chipintelli_cloud_protocol.h"
//云端协串口议初始化
static int alg_cloud_protocol_init(void)
{
    #if USE_VPR
        vpr_cloud_cmd_init_call();
    #elif USE_WMAN_VPR
        vgr_cloud_cmd_init_call();
    #elif USE_SED_CRY
        cry_cloud_cmd_init_call();
    #elif USE_SED_SNORE
        snore_cloud_cmd_init_call();
    #elif USE_NN_DOA     
        doa_cloud_cmd_init_call();
    #endif
    return 0;
}
#endif
//双核交互参数初始化
static void ciss_param_init(void)
{
    ciss_set(CI_SS_ASR_ROLL_FRM, VAD_ROLL_FRM);      //ASR回退帧数
    #if NO_ASR_FLOW
    ciss_set(CI_SS_ASR_EN, 0);
    #else
    ciss_set(CI_SS_ASR_EN, 1);
    #endif
    #if USE_CWSL   //开启自学习
    ciss_set(CI_SS_CWSL_EN, 1);  
    #else
    ciss_set(CI_SS_CWSL_EN, 0);  
    #endif
    #if USE_NN_DENOISE  //开启NN降噪
    ciss_set(CI_SS_NN_DENOISE_EN, 1);
    #else
    ciss_set(CI_SS_NN_DENOISE_EN, 0);
    #endif
    #if USE_TRA_DENOISE  //开启传统降噪
    ciss_set(CI_SS_TRA_DENOISE_EN, 1);
    #else
    ciss_set(CI_SS_TRA_DENOISE_EN, 0);
    #endif
    #if USE_NN_VAD  //开启USE_NN_VAD
    ciss_set(CI_SS_NN_VAD_EN, 1);
    #else
    ciss_set(CI_SS_NN_VAD_EN, 0);
    #endif
    
    #if USE_NN_DOA
    ciss_set(CI_SS_NN_DOA_EN, 1);
    #else
    ciss_set(CI_SS_NN_DOA_EN, 0);
    #endif
    #if USE_VPR
    ciss_set(CI_SS_VPR_EN, 1);
    #else
    ciss_set(CI_SS_VPR_EN, 0);
    #endif
    #if USE_WMAN_VPR
    ciss_set(CI_SS_WMAN_VPR_EN, 1);
    #else
    ciss_set(CI_SS_WMAN_VPR_EN, 0);
    #endif
    #if USE_SED
    ciss_set(CI_SS_SED_EN, 1);
    ciss_set(CI_SS_ASR_EN, 0);   //事件检测内存不够，不开识别
    #else
    ciss_set(CI_SS_SED_EN, 0);
    #endif
    
    // #if USE_AEC_MODULE&&USE_NN_DOA
    // ciss_set(CI_SS_AEC_WORK_STATE, 0);
    // ciss_set(CI_SS_DOA_WORK_STATE, 1);
    // ciss_set(CI_SS_DOA_INIT_STATE, 1);
    // ciss_set(CI_SS_CUR_DOA_AEC_STATE, 1);
    // #elif USE_AEC_MODULE
    // ciss_set(CI_SS_DOA_WORK_STATE, 0);
    // ciss_set(CI_SS_AEC_WORK_STATE, 1);
    // ciss_set(CI_SS_AEC_INIT_STATE, 1);
    // #elif USE_NN_DOA
    // ciss_set(CI_SS_AEC_WORK_STATE, 0);
    // ciss_set(CI_SS_DOA_WORK_STATE, 1);
    // ciss_set(CI_SS_DOA_INIT_STATE, 1);
    // #endif
	ciss_set(CI_SS_VOX_WORK_STATE, 1);
//	ciss_set(CI_SS_VOX_SET_END_CONFIDENCE, VOX_VAD_END_CONFIDENCE_DEFAULT);
    ciss_set(CI_SS_ALG_DEBUG_STATE, 0);           //开启前端算法调试
    //注册语音前段信号处理模块
    #if USE_DUAL_MIC_ANY
    ciss_set(CI_SS_DUALMIC_IS_ANY,true);
    ciss_set(CI_SS_ANY_MIC_AEC_PCM_THR_VAL, 100);
    cm_set_codec_alc(HOST_MIC_RECORD_CODEC_ID, CM_CHA_LEFT,  DISABLE);   //禁用alc，设置固定增益为20
    cm_set_codec_adc_gain(HOST_MIC_RECORD_CODEC_ID, CM_CHA_LEFT, 20);
    cm_set_codec_alc(HOST_MIC_RECORD_CODEC_ID, CM_CHA_RIGHT,  DISABLE);
    cm_set_codec_adc_gain(HOST_MIC_RECORD_CODEC_ID, CM_CHA_RIGHT, 20);
    #else
    ciss_set(CI_SS_DUALMIC_IS_ANY,false);
    #endif
     /*DNN配置*/
    ciss_set(CI_SS_DNN_TASK_ASR_REG_EN, 1);
    bnpu_dnn_param_cfg();
    ciss_set(CI_SS_DNN_HOST_CFG_READY, 1);      //需要在asr,降噪等dnn功能配置完后再ready
    ciss_set(CI_SS_AUDIO_IN_BUFFER_NUM, AUDIO_IN_BUFFER_NUM);  //2M模型，buffer至少设置为20
    ciss_set(CI_SS_DECODER_MIN_ACTIVE, DECODER_MIN_ACTIVE);
    #if DEEP_SEPARATE_ENABLE
    ciss_set(CI_SS_CHA_NUM, 2);
    #else 
    ciss_set(CI_SS_CHA_NUM, 1);
    #endif
    
    float beam = DECODER_BEAM;
    ciss_set(CI_SS_DECODER_BEAM,*(uint32_t*)&beam);
	
}
static void task_init(void *p_arg)
{

    extern char SDK_ALG_PRO_SRAM_HOST_END_ADDR;
    dsu_init((uint32_t)&SDK_ALG_PRO_SRAM_HOST_END_ADDR);
    vTaskDelay(pdMS_TO_TICKS(5));       //必须延时5ms，否则识别慢
    #if CLOUD_UART_PROTOCOL_EN
    UARTPollingConfig((UART_TypeDef*)CLOUD_CFG_UART_PORT, CLOUD_CFG_UART_BAUND_RATE);
    #endif

    cm_init();
    /* 注册录音codec */
    audio_in_codec_registe();

    nuclear_com_init();

    /*各个通信组件的初始化*/
    decoder_port_inner_rpmsg_init();
    flash_control_inner_port_init();
    asr_top_nuclear_com_outside_port_init();
    flash_manage_nuclear_com_outside_port_init();
    codec_manage_inner_port_init();
    ciss_init();
    ciss_param_init();
	mailboxboot_sync();   //需要放在ciss_set双核参数同步设置之后

    ci_flash_data_info_init(DEFAULT_MODEL_GROUP_ID);
    extern ci_ssp_config_t ci_ssp;
    extern audio_capture_t audio_capture;
    REMOTE_CALL(set_ssp_registe(&audio_capture, (ci_ssp_st*)&ci_ssp, sizeof(ci_ssp)/sizeof(ci_ssp_st)));
    REMOTE_CALL(set_freqvad_start_para_gain(VAD_SENSITIVITY));
    alg_model_init();                                               //初始化算法模型
    ciss_set(CI_SS_HOST_PARAM_SET_OK, BNPU_HOST_PARAM_SYNC_MAGIC);  //参数设置完成
    #if USER_CODE_SWITCH_ENABLE  //支持两份code动态切换
    check_code2_param();
    #endif
    #if USE_CWSL 
    cwsl_set_vad_alc_config(1);
    cwsl_init();
    #endif

    #if USE_PWK    //就近唤醒
	cm_set_codec_alc(HOST_MIC_RECORD_CODEC_ID,CM_CHA_LEFT,DISABLE);
	cm_set_codec_adc_gain(HOST_MIC_RECORD_CODEC_ID,CM_CHA_LEFT,25);
    #endif
    #if !NO_ASR_FLOW          //事件检测不带asr
    extern void decoder_task_init_port(void);
    decoder_task_init_port();
    #endif
    xTaskCreate(audio_in_manage_inner_task, "audio_in_manage_inner_task", 300, NULL, 4, NULL);
    /* 播放器任务 */
    #if SIMPLE_AUDIO_PLAYER_ENABLE
    #if AUDIO_PLAYER_ENABLE || NET_AUDIO_PLAY_BY_MP3 || NET_AUDIO_PLAY_BY_PCM || NET_AUDIO_PLAY_BY_G722
    sap_init();
    #endif
    #else
    #if AUDIO_PLAYER_ENABLE
    audio_play_init();
    #endif
    #endif
    #if USE_NN_DOA || USE_GCC_DOA
    xTaskCreate(doa_out_result_hand_task, "doa_out_result_hand_task", 100, NULL, 4, NULL); 
    #endif
    #if USE_SED
    sed_set_vol_init();
    sed_play_welcome_prompt();
    #endif
    #if CLOUD_UART_PROTOCOL_EN
    alg_cloud_protocol_init();
    #endif
    #if !AI_UART_CONTROL_EN
    if(!cias_online_func_init())
    {
        CI_ASSERT(0, "cias_online_func_init error\r\n");
    }
    #endif
    /*user app初始化*/
    userapp_initial();
    /* 用户任务 */
    sys_msg_task_initial();
    #if AI_UART_CONTROL_EN
    ai_uart_i2s_protocol_init();
    #endif
    xTaskCreate(UserTaskManageProcess,"UserTaskManageProcess",480,NULL,4,NULL);
    
#if !NO_ASR_FLOW
    extern void config_adpt_cnt(int enable);
    config_adpt_cnt(ADAPTIVE_CNT_ENABLE);
    extern void config_max_stop_cfd(int enable,int nocnt_max_stop_cfd,int cnt_max_stop_cfd);
    config_max_stop_cfd(MAX_STOP_CFD_ENABLE,MAX_STOP_CFD_NOCNT,MAX_STOP_CFD_CNT);
    extern void config_max_vad_end_frm(int max_vad_end_frm);
    config_max_vad_end_frm(MAX_STOP_VAD_FRM);
    extern int config_base_confidence_count(short base_confidence,unsigned char valid_count);
    config_base_confidence_count(DEFAULT_CONFIDENCE,DEFAULT_CNT);
    extern void config_recover_result(int enable,int mode,int max_frm);
    config_recover_result(RECOVER_RESULT_ENABLE,RECOVER_RESULT_MODE,RECOVER_RESULT_MAX_FRM);
    extern void config_silprob_cnt(float base_silprob,int base_silcnt );
    config_silprob_cnt(DEFAULT_STOP_SILPROB, DEFAULT_STOP_SILCNT);

    REMOTE_CALL(vad_rollback_frm_cfg(VAD_START_BACK_FRAMES));
    REMOTE_CALL(vad_rollback_fast_frm_cfg(VAD_START_BACK_FRAMES_FASTER,VAD_START_BACK_FRAMES_ADAPTIVE_EN));
#endif
    #if USER_CODE_SWITCH_ENABLE
    xTaskCreate(uart_data_handle_task,"uart_data_handle_task", 480, NULL, 4, NULL);
    #endif
    #if (!COMMAND_LINE_CONSOLE_EN)
    //语音系统准备OK
    cias_send_cmd(CIAS_AUDIO_SYS_READY, DEF_FILL);
    #if AI_UART_CONTROL_EN
    ai_uart_i2s_on_audio_ready();
    #endif
    #if 0
    while(1)
    {
        #if !DEBUG_AUDIO_UART_UPLOAD_EN
        UBaseType_t ArraySize = 20;
        TaskStatus_t *StatusArray;
        //ArraySize = uxTaskGetNumberOfTasks();
        StatusArray = pvPortMalloc(ArraySize*sizeof(TaskStatus_t));
        if (StatusArray && ArraySize)
        {
            uint32_t ulTotalRunTime;
            volatile UBaseType_t ArraySize2 = uxTaskGetSystemState(StatusArray, ArraySize, &ulTotalRunTime);
            mprintf("TaskName\t\tPriority\tTaskNumber\tMinStk\t%d\r\n", ArraySize2);
            for (int i = 0;i < ArraySize2;i++)
            {
                mprintf("% -16s\t%d\t\t%d\t\t%d\r\n",
                    StatusArray[i].pcTaskName,
                    (int)StatusArray[i].uxCurrentPriority,
                    (int)StatusArray[i].xTaskNumber,
                    (int)StatusArray[i].usStackHighWaterMark
                );
            }
            mprintf("\r\n");
            extern int get_heap_bytes_remaining_size(void);
            mprintf("asr heap min free:%dKB\r\n", get_heap_bytes_remaining_size()/1024);
            mprintf("system heap min free:%dKB\r\n", xPortGetMinimumEverFreeHeapSize()/1024);
            mprintf("system heap free:%dKB\r\n", xPortGetFreeHeapSize()/1024);
        }
        vPortFree(StatusArray);
        #endif
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
    #endif
    #endif
    vTaskDelete(NULL);
}

/**
 * @brief 
 * 
 */
int main(void)
{
    hardware_default_init();

    /*平台相关初始化*/
    platform_init(); 

    /* 版本信息 */
    welcome();

    /* 创建启动任务 */
    xTaskCreate(task_init, "init task", 280, NULL, 4, NULL);

    /* 启动调度，开始执行任务 */
    vTaskStartScheduler();

    while(1){}
}


