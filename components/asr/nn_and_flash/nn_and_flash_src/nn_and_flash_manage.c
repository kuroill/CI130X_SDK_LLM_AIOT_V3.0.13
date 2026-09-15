
#include "nn_and_flash_manage.h"
#include "flash_manage.h"
#include "ci_nn_manage.h"

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "croutine.h"
#include "semphr.h"
#include "event_groups.h"
#include <string.h>
#include "status_share.h"
#include "romlib_runtime.h"
#include "ci130x_core_misc.h"
#include "asr_top_config.h"
#if ASR_CODE_VERSION == 2
#endif
//flash spic_read_unique_id


#define NNFM_CI_ASSERT(x,msg)                                                                                                    \
    if( ( x ) == 0 )                                                                                                        \
    {                                                                                                                       \
        ci_logdebug(LOG_SYS_INFO, "%s",msg);                                                                                                   \
        ci_logdebug(LOG_SYS_INFO, "NNFM Line:%d\n",__LINE__);                                                                                   \
        while(1)  asm volatile ("ebreak");                                                                                  \
    }


QueueHandle_t  nn_and_flash_manage_xQueuemsg = NULL;

static EventGroupHandle_t flash_port_protect_event_group = NULL;

uint32_t cnt = 0;

static void flash_manage_wait_protect_sem(flash_operate_type_t op_type)
{
    EventBits_t ret = xEventGroupWaitBits(flash_port_protect_event_group, op_type, pdTRUE, pdFALSE, pdMS_TO_TICKS(3000));
    if(ret != (op_type))
    {
        //等待flash事件超时
        // ci_logdebug(LOG_SYS_INFO, "op_type = %d  cnt = %d\n",op_type,cnt);
        ci_logdebug(LOG_SYS_INFO, "NNFM Line:%d\n",__LINE__);    
        sys_err_flag = ASR_RST_FLAG_NUM_76;
    }
}



// bool nn_and_flash_task_init_done = false;
// bool get_nn_and_flash_task_state(void)
// {
//     return nn_and_flash_task_init_done;
// }


static void nn_and_flash_manage_send_queue(nn_and_flash_manage_msg_t* p)
{
    BaseType_t ret;
    BaseType_t xTaskWokenByReceive = pdFALSE;
    if(check_curr_trap())
    {   
        ret = xQueueSendFromISR(nn_and_flash_manage_xQueuemsg,p,&xTaskWokenByReceive);
        portEND_SWITCHING_ISR(xTaskWokenByReceive);
    }
    else
    {
        ret = xQueueSend(nn_and_flash_manage_xQueuemsg,p,0);
    }
    
    if(pdPASS != ret)
    {
        ci_logdebug(LOG_SYS_INFO, "NNFM Line:%d\n",__LINE__);    
        sys_err_flag = ASR_RST_FLAG_NUM_77;
    }
}


static void nn_and_flash_manage_send_queue_from_isr(nn_and_flash_manage_msg_t* p)
{
    BaseType_t ret;
    BaseType_t xTaskWokenByReceive = pdFALSE;
 
    ret = xQueueSendFromISR(nn_and_flash_manage_xQueuemsg,p,&xTaskWokenByReceive);
    portEND_SWITCHING_ISR(xTaskWokenByReceive);

    if(pdPASS != ret)
    {
        NNFM_CI_ASSERT(0,"\n");
    }
}


/**
 * @brief 请求初始化flash（只能是XIP模式），并等待初始化完成
 * 
 */
void req_flash_init(void)
{
    flash_manage_wait_protect_sem(FLASH_INIT_OPERTATE);

    nn_and_flash_manage_msg_t msg;
    MASK_ROM_LIB_FUNC->newlibcfunc.memset_p((void*)&msg,0,sizeof(nn_and_flash_manage_msg_t));
    msg.nn_or_flash = NN_AND_FLASH_CHOSE_FLASH;
    msg.flash_ctr_info.op_type = FLASH_INIT_OPERTATE;
    nn_and_flash_manage_send_queue(&msg);

    flash_manage_wait_op_done_sem(FLASH_INIT_OPERTATE);
    ciss_set(CI_SS_FLASH_BNPU_STATE,CI_SS_FLASH_IDLE);

    xEventGroupSetBits(flash_port_protect_event_group, (FLASH_INIT_OPERTATE));
}


/**
 * @brief 请求读flash，并等待读取完成
 * 
 * @param buf 
 * @param addr 
 * @param size 
 * @return int32_t 
 */
int32_t post_read_flash(char *buf, uint32_t addr, uint32_t size)
{
    // ci_logdebug(LOG_SYS_INFO, " = %08x\n",size);
    flash_manage_wait_protect_sem(FLASH_READ_OPERTATE);

    nn_and_flash_manage_msg_t msg;
    MASK_ROM_LIB_FUNC->newlibcfunc.memset_p((void*)&msg,0,sizeof(nn_and_flash_manage_msg_t));
    msg.nn_or_flash = NN_AND_FLASH_CHOSE_FLASH;
    msg.flash_ctr_info.op_type = FLASH_READ_OPERTATE;
    msg.flash_ctr_info.dst_addr = (uint32_t)buf;
    msg.flash_ctr_info.src_addr = addr;
    msg.flash_ctr_info.op_size = size;
    nn_and_flash_manage_send_queue(&msg);

    flash_manage_wait_op_done_sem(FLASH_READ_OPERTATE);
    // ci_logdebug(LOG_SYS_INFO, "1\n");
    xEventGroupSetBits(flash_port_protect_event_group, (FLASH_READ_OPERTATE));
    
    // ci_logdebug(LOG_SYS_INFO, "read done\n");
}


/**
 * @brief 中断中请求读flash，不等待读取完成
 * 
 * @param buf 
 * @param addr 
 * @param size 
 * @return int32_t 
 */
int32_t post_read_flash_int(char *buf, uint32_t addr, uint32_t size)
{
    // ci_logdebug(LOG_SYS_INFO, "req read int\n");
    nn_and_flash_manage_msg_t msg;
    MASK_ROM_LIB_FUNC->newlibcfunc.memset_p((void*)&msg,0,sizeof(nn_and_flash_manage_msg_t));
    msg.nn_or_flash = NN_AND_FLASH_CHOSE_FLASH;
    msg.flash_ctr_info.op_type = FLASH_READ_OPERTATE_INT;
    msg.flash_ctr_info.dst_addr = (uint32_t)buf;
    msg.flash_ctr_info.src_addr = addr;
    msg.flash_ctr_info.op_size = size;
    nn_and_flash_manage_send_queue_from_isr(&msg);
}


/**
 * @brief 请求写flash，并等待读完成
 * 
 * @param buf 
 * @param addr 
 * @param size 
 * @return int32_t 
 */
int32_t post_write_flash(char *buf, uint32_t addr, uint32_t size)
{
    flash_manage_wait_protect_sem(FLASH_WRITE_OPERTATE);

    // ci_logdebug(LOG_SYS_INFO, "req write = %08x\n",size);
    nn_and_flash_manage_msg_t msg;
    MASK_ROM_LIB_FUNC->newlibcfunc.memset_p((void*)&msg,0,sizeof(nn_and_flash_manage_msg_t));
    msg.nn_or_flash = NN_AND_FLASH_CHOSE_FLASH;
    msg.flash_ctr_info.op_type = FLASH_WRITE_OPERTATE;
    msg.flash_ctr_info.dst_addr = addr;
    msg.flash_ctr_info.src_addr = (uint32_t)buf;
    msg.flash_ctr_info.op_size = size;
    nn_and_flash_manage_send_queue(&msg);

    flash_manage_wait_op_done_sem(FLASH_WRITE_OPERTATE);
    // ci_logdebug(LOG_SYS_INFO, "write done\n");

    xEventGroupSetBits(flash_port_protect_event_group, (FLASH_WRITE_OPERTATE));
}


int32_t post_write_flash_int(char *buf, uint32_t addr, uint32_t size)
{
    // ci_logdebug(LOG_SYS_INFO, "req write int\n");
    nn_and_flash_manage_msg_t msg;
    MASK_ROM_LIB_FUNC->newlibcfunc.memset_p((void*)&msg,0,sizeof(nn_and_flash_manage_msg_t));
    msg.nn_or_flash = NN_AND_FLASH_CHOSE_FLASH;
    msg.flash_ctr_info.op_type = FLASH_WRITE_OPERTATE_INT;
    msg.flash_ctr_info.dst_addr = addr;
    msg.flash_ctr_info.src_addr = (uint32_t)buf;
    msg.flash_ctr_info.op_size = size;
    nn_and_flash_manage_send_queue_from_isr(&msg);
}


int32_t post_spic_read_unique_id(uint8_t* buf)
{
    flash_manage_wait_protect_sem(FLASH_READ_UNIQUE_ID_OPERTATE);

    nn_and_flash_manage_msg_t msg;
    MASK_ROM_LIB_FUNC->newlibcfunc.memset_p((void*)&msg,0,sizeof(nn_and_flash_manage_msg_t));
    msg.nn_or_flash = NN_AND_FLASH_CHOSE_FLASH;
    msg.flash_ctr_info.op_type = FLASH_READ_UNIQUE_ID_OPERTATE;
    msg.flash_ctr_info.dst_addr = (uint32_t)buf;
    nn_and_flash_manage_send_queue(&msg);

    flash_manage_wait_op_done_sem(FLASH_READ_UNIQUE_ID_OPERTATE);
    // ci_logdebug(LOG_SYS_INFO, "read unique ID done\n");

    xEventGroupSetBits(flash_port_protect_event_group, (FLASH_READ_UNIQUE_ID_OPERTATE));
}


int32_t post_spic_read_unique_id_int(char *buf)
{
    // ci_logdebug(LOG_SYS_INFO, "req write int\n");
    nn_and_flash_manage_msg_t msg;
    MASK_ROM_LIB_FUNC->newlibcfunc.memset_p((void*)&msg,0,sizeof(nn_and_flash_manage_msg_t));
    msg.nn_or_flash = NN_AND_FLASH_CHOSE_FLASH;
    msg.flash_ctr_info.op_type = FLASH_READ_UNIQUE_ID_OPERTATE_INT;
    msg.flash_ctr_info.dst_addr = (uint32_t)buf;
    nn_and_flash_manage_send_queue_from_isr(&msg);
}



/**
 * @brief 请求擦除flash，并等待擦除完成
 * 
 * @param addr 
 * @param size 
 * @return int32_t 
 */
int32_t post_erase_flash(uint32_t addr, uint32_t size)
{
    // ci_logdebug(LOG_SYS_INFO, "req erase 1 = %d = %08x\n",cnt,size);
    flash_manage_wait_protect_sem(FLASH_ERASE_OPERTATE);

    nn_and_flash_manage_msg_t msg;
    MASK_ROM_LIB_FUNC->newlibcfunc.memset_p((void*)&msg,0,sizeof(nn_and_flash_manage_msg_t));
    msg.nn_or_flash = NN_AND_FLASH_CHOSE_FLASH;
    msg.flash_ctr_info.op_type = FLASH_ERASE_OPERTATE;
    msg.flash_ctr_info.dst_addr = addr;
    msg.flash_ctr_info.src_addr = cnt;
    msg.flash_ctr_info.op_size = size;
    nn_and_flash_manage_send_queue(&msg);

    flash_manage_wait_op_done_sem(FLASH_ERASE_OPERTATE);

    xEventGroupSetBits(flash_port_protect_event_group, (FLASH_ERASE_OPERTATE));
    // ci_logdebug(LOG_SYS_INFO, "erase done= %d\n",cnt);
    cnt++;
}


int32_t post_erase_flash_int(uint32_t addr, uint32_t size)
{
    nn_and_flash_manage_msg_t msg;
    MASK_ROM_LIB_FUNC->newlibcfunc.memset_p((void*)&msg,0,sizeof(nn_and_flash_manage_msg_t));
    msg.nn_or_flash = NN_AND_FLASH_CHOSE_FLASH;
    msg.flash_ctr_info.op_type = FLASH_ERASE_OPERTATE_INT;
    msg.flash_ctr_info.dst_addr = addr;
    msg.flash_ctr_info.src_addr = 0;
    msg.flash_ctr_info.op_size = size;
    nn_and_flash_manage_send_queue_from_isr(&msg);
}






static SemaphoreHandle_t nn_clear_xSemaphore = NULL;
void cinn_wait_sem_creat(void)
{
    if(NULL == nn_clear_xSemaphore)
    {
        nn_clear_xSemaphore = xSemaphoreCreateBinary();
        NNFM_CI_ASSERT(nn_clear_xSemaphore,"\n");
        xSemaphoreTake(nn_clear_xSemaphore,0);
    }
}


void req_cinn_clear(void)
{
    cinn_wait_sem_creat();
    nn_and_flash_manage_msg_t msg;
    memset((void*)&msg,0,sizeof(msg));
    msg.nn_or_flash = NN_AND_FLASH_REQ_NN_CLEAR;
    nn_and_flash_manage_send_queue(&msg);
}


void cinn_wait_clear_done(void)
{
    cinn_wait_sem_creat();
    if(pdFAIL == xSemaphoreTake(nn_clear_xSemaphore,pdMS_TO_TICKS(2000)))
    {
        NNFM_CI_ASSERT(0,"\n");
    }
}

static SemaphoreHandle_t nn_init_xSemaphore = NULL;
void cinn_init_sem_creat(void)
{
    if(NULL == nn_init_xSemaphore)
    {
        nn_init_xSemaphore = xSemaphoreCreateBinary();
        NNFM_CI_ASSERT(nn_init_xSemaphore,"\n");
        xSemaphoreTake(nn_init_xSemaphore,0);
    }
}


void req_cinn_init(int cha_num)
{
    cinn_init_sem_creat();
    nn_and_flash_manage_msg_t msg;
    memset((void*)&msg,0,sizeof(msg));
    msg.nn_or_flash = NN_AND_FLASH_REQ_NN_INIT;
    msg.nn_ctr_info.cinn_cmp_info.cha_num = cha_num;
    nn_and_flash_manage_send_queue(&msg);
}


void cinn_wait_init_done(void)
{
    cinn_init_sem_creat();
    if(pdFAIL == xSemaphoreTake(nn_init_xSemaphore,pdMS_TO_TICKS(2000)))
    {
        NNFM_CI_ASSERT(0,"\n");
    }
}


SemaphoreHandle_t denoise_nn_cmpt_done_semaphore = NULL;

void denoise_nn_cmpt_done(void)
{
    xSemaphoreGive(denoise_nn_cmpt_done_semaphore);
    portEND_SWITCHING_ISR(pdTRUE);
}

SemaphoreHandle_t doa_tdnn_cmpt_done_semaphore = NULL;
void doa_tdnn_cmpt_done(void)
{
    xSemaphoreGive(doa_tdnn_cmpt_done_semaphore);
    portEND_SWITCHING_ISR(pdTRUE);
}

SemaphoreHandle_t ecapa_tdnn_cmpt_done_semaphore = NULL;
void ecapa_nn_cmpt_done(void)
{
    xSemaphoreGive(ecapa_tdnn_cmpt_done_semaphore);
    portEND_SWITCHING_ISR(pdTRUE);
}

void req_denoise_nn_cmpt(ci_denoise_nn_cmpt_info_t* cmpt_p)
{
    #if USE_NN_DENOISE
    // timer0_end_count_only_print_time_us();
    // init_timer0();
	// timer0_start_count();
    nn_and_flash_manage_msg_t msg;
    MASK_ROM_LIB_FUNC->newlibcfunc.memset_p((void*)&msg,0,sizeof(nn_and_flash_manage_msg_t));
    msg.nn_or_flash = NN_AND_FLASH_CHOSE_NN;
    msg.nn_ctr_info.nn_type = NN_TYPE_NAME_DENOISE_NN;
    MASK_ROM_LIB_FUNC->newlibcfunc.memcpy_p((void*)(&msg.nn_ctr_info.denoise_nn_cmpt_info.denoise_nn_cmpt),(void*)cmpt_p,sizeof(ci_denoise_nn_cmpt_info_t));
    nn_and_flash_manage_send_queue(&msg);

    if(pdPASS != xSemaphoreTake(denoise_nn_cmpt_done_semaphore,pdMS_TO_TICKS(3000)))
    {
        ci_logdebug(LOG_SYS_INFO, "denoise_nn cmpt timeout\n");
    }
    #endif
    // timer0_end_count_only_print_time_us();
}


void req_doa_tdnn_cmpt(ci_nn_doa_tdnn_cmpt_info_t* cmpt_p)
{
    #if USE_NN_DOA
    nn_and_flash_manage_msg_t msg;
    MASK_ROM_LIB_FUNC->newlibcfunc.memset_p((void*)&msg,0,sizeof(nn_and_flash_manage_msg_t));
    msg.nn_or_flash = NN_AND_FLASH_CHOSE_NN;
    msg.nn_ctr_info.nn_type = NN_TYPE_NAME_DOA_TDNN;
    MASK_ROM_LIB_FUNC->newlibcfunc.memcpy_p((void*)(&msg.nn_ctr_info.doa_tdnn_cmpt_info),(void*)cmpt_p, sizeof(ci_nn_doa_tdnn_cmpt_info_t));
    nn_and_flash_manage_send_queue(&msg);
    if(pdPASS != xSemaphoreTake(doa_tdnn_cmpt_done_semaphore, pdMS_TO_TICKS(3000)))
    {
        ci_logdebug(LOG_SYS_INFO, "doa_tdnn cmpt timeout\n");
    }
    #endif
}


void req_vp_nn_cmpt(ci_nn_ecapa_tdnn_info_t* cmpt_p)
{
    #if USE_VPR || USE_WMAN_VPR || USE_SED
    nn_and_flash_manage_msg_t msg;
    MASK_ROM_LIB_FUNC->newlibcfunc.memset_p((void*)&msg,0,sizeof(nn_and_flash_manage_msg_t));
    msg.nn_or_flash = NN_AND_FLASH_CHOSE_NN;
    msg.nn_ctr_info.nn_type = NN_TYPE_NAME_ECAPA_TDNN;
    MASK_ROM_LIB_FUNC->newlibcfunc.memcpy_p((void*)(&msg.nn_ctr_info.ecapa_tdnn_cmpt_info),(void*)cmpt_p,sizeof(ci_nn_ecapa_tdnn_info_t));
    nn_and_flash_manage_send_queue(&msg);
    if(pdPASS != xSemaphoreTake(ecapa_tdnn_cmpt_done_semaphore,pdMS_TO_TICKS(3000)))
    {
        ci_logdebug(LOG_SYS_INFO, "ecapa_tdnn cmpt timeout\n");
    }
    #endif
}


void nn_and_flash_manage_task(void* p)
{
    #if USE_NN_DENOISE
    denoise_nn_cmpt_done_semaphore = xSemaphoreCreateBinary();
    CI_ASSERT(denoise_nn_cmpt_done_semaphore,"\n");
    xSemaphoreTake(denoise_nn_cmpt_done_semaphore,0);
    #endif

    #if USE_NN_DOA
    doa_tdnn_cmpt_done_semaphore = xSemaphoreCreateBinary();
    CI_ASSERT(doa_tdnn_cmpt_done_semaphore,"\n");
    xSemaphoreTake(doa_tdnn_cmpt_done_semaphore,0);
    #endif

    #if USE_VPR || USE_WMAN_VPR || USE_SED
    ecapa_tdnn_cmpt_done_semaphore = xSemaphoreCreateBinary();
    CI_ASSERT(ecapa_tdnn_cmpt_done_semaphore,"\n");
    xSemaphoreTake(ecapa_tdnn_cmpt_done_semaphore,0);
    #endif
    

    nn_and_flash_manage_xQueuemsg = xQueueCreate(10, sizeof(nn_and_flash_manage_msg_t));
    if(NULL == nn_and_flash_manage_xQueuemsg)
    {
        NNFM_CI_ASSERT(0,"\n");
    }

    flash_port_protect_event_group = xEventGroupCreate();
    if(NULL == flash_port_protect_event_group)
    {
        NNFM_CI_ASSERT(0,"\n");
    }

    // ci_logdebug(LOG_SYS_INFO, "nn_and_flash_manage_msg_t size = %d\n",sizeof(nn_and_flash_manage_msg_t));

    flash_manage_init();

    // req_flash_init();
    // nn_and_flash_task_init_done = true;
    extern void set_flash_state_to_idle(void);
    set_flash_state_to_idle();

    nn_and_flash_manage_msg_t msg;
    for(;;)
    {
        if(pdPASS == xQueueReceive(nn_and_flash_manage_xQueuemsg, &msg, pdMS_TO_TICKS(portMAX_DELAY)))
        {
            nn_and_flash_chose_t nn_or_flash = msg.nn_or_flash;
            switch(nn_or_flash)
            {
                case NN_AND_FLASH_CHOSE_NN:
                {
                   
                    break;
                }
                case NN_AND_FLASH_CHOSE_FLASH:
                {
                    flash_manage_flow(&(msg.flash_ctr_info));
                    break;
                }
                #if CI_NN_V2_EN
                case NN_AND_FLASH_REQ_NN_CLEAR:
                {
                    cinn_wait_sem_creat();
                    xSemaphoreGive(nn_clear_xSemaphore);
                    break;
                }
                case NN_AND_FLASH_REQ_NN_INIT:
                {
                    cinn_init_sem_creat();
                    int cha_num = msg.nn_ctr_info.cinn_cmp_info.cha_num;
                    extern void cinn_model_init(int cha_num);
                    xSemaphoreGive(nn_init_xSemaphore);
                    break;
                }
                #endif
                default:
                {
                    NNFM_CI_ASSERT(0,"\n");
                    break;
                }
            }
            
        }
    }
    
}


volatile uint8_t flash_test_buf[16*1024];
#define FLASH_TEST_ADDR (0x400000 - 32*1024)


/*测试任务*/
void flash_test_task(void* p)
{

    MASK_ROM_LIB_FUNC->newlibcfunc.memset_p((void*)flash_test_buf,0,16*1024);

    bool is_power_off;
    is_flash_power_off(&is_power_off);
    while(is_power_off)
    {
        is_flash_power_off(&is_power_off);
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    for(;;)
    {
        post_read_flash((char*)flash_test_buf,FLASH_TEST_ADDR,16*1024);
        vTaskDelay(pdMS_TO_TICKS(2));
    }
}


