
// #include "nn_and_flash_manage.h"
#include "flash_manage.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "croutine.h"
#include "semphr.h"
#include "event_groups.h"
#include "ci_log.h"
#include "ci130x_dma.h"
#include "ci130x_core_eclic.h"
#include "ci130x_scu.h"
#include "ci130x_spiflash.h"
#include "ci130x_dtrflash.h"
#include "ci130x_system_ept.h"
#include "debug_time_consuming.h"
#include "flash_manage_inner_port.h"
#include "status_share.h"
#include "romlib_runtime.h"


#define FMC_CI_ASSERT(x,msg)                                                                                                    \
    if( ( x ) == 0 )                                                                                                        \
    {                                                                                                                       \
        mprintf("%s",msg);                                                                                                   \
        mprintf("FMC Line:%d\n",__LINE__);                                                                                   \
        while(1)  asm volatile ("ebreak");                                                                                  \
    }


typedef enum
{
    FLASH_STATE_POWER_OFF = 0,
    FLASH_STATE_IDLE,
    FLASH_STATE_WRITE_BUSY,
    FLASH_STATE_READ_BUSY,
    FLASH_STATE_ERASE_BUSY,
    FLASH_STATE_READ_UNIQUE_ID_BUSY,
}flash_state_t;

flash_state_t flash_state = FLASH_STATE_POWER_OFF;

void set_flash_state_to_idle(void)
{
    flash_state = FLASH_STATE_IDLE;
}

SemaphoreHandle_t flash_op_semaphore = NULL;
// SemaphoreHandle_t flash_op_done_sem[FLASH_OPERATE_MAX_NUM] = {NULL}; 
static EventGroupHandle_t flash_op_done_event_group = NULL;


void flash_manage_wait_op_done_sem(flash_operate_type_t op_type)
{
    // BaseType_t ret = xSemaphoreTake(flash_op_done_sem[op_type],pdMS_TO_TICKS(3000));
    // if(ret != pdPASS)
    EventBits_t ret = xEventGroupWaitBits(flash_op_done_event_group, op_type, pdTRUE, pdFALSE, pdMS_TO_TICKS(3000));
    if (ret != op_type)
    {
        //等待flash信号量超时
        // mprintf("op_type = %d\n",op_type);
        FMC_CI_ASSERT(0,"\n");
    }
}


/**
 * @brief flash各种操作完成，释放信号量
 * 
 * @param op_type 
 */
static void flash_manage_give_op_done_sem(flash_operate_type_t op_type)
{
    // xSemaphoreGive(flash_op_done_sem[op_type]);
    xEventGroupSetBits(flash_op_done_event_group, op_type);
}


static void flash_manage_take_sem(void)
{
    BaseType_t ret = xSemaphoreTake(flash_op_semaphore,pdMS_TO_TICKS(1000));
    if(ret != pdPASS)
    {
        //等待flash信号量超时
        FMC_CI_ASSERT(0,"\n");
    }
}


static void flash_manage_give_sem(void)
{
    ciss_set(CI_SS_FLASH_BNPU_STATE,CI_SS_FLASH_IDLE);
    xSemaphoreGive(flash_op_semaphore);
}


volatile uint8_t flash_dma_read_done = 0;

static void flash_xip_dma_read_irq_callback(void)
{
    flash_dma_read_done = 1;
}

void is_flash_power_off(bool* state)
{
    if(FLASH_STATE_POWER_OFF == flash_state)
    {
        *state = true;
    }
    else
    {
        *state = false;
    }
    
}


/**
 * @brief flash读函数，自动选择DMA读还是CPU读
 * 
 * @param dst_addr 
 * @param src_addr 
 * @param size 
 */
static void flash_read_intelli(uint32_t dst_addr,uint32_t src_addr,uint32_t size)
{


    if(src_addr >= FLASH_CPU_READ_BASE_ADDR)
    {
        src_addr -= FLASH_CPU_READ_BASE_ADDR;
    }
    if(size > FLASH_SIZE_DMA_TH)
    {
        extern void set_dma_int_callback(DMACChannelx dmachannel,dma_callback_func_ptr_t func);
        set_dma_int_callback(DMACChannel0,flash_xip_dma_read_irq_callback);
        uint16_t times = size/FLASH_SIZE_DMA_TH;
        uint32_t dst_addr_cur = dst_addr;
        uint32_t src_addr_cur = src_addr + FLASH_CPU_READ_BASE_ADDR;
        for(int i=0;i<times;i++)
        {
            //速度待测试
            DMAC_M2MConfig(DMACChannel0,src_addr_cur,dst_addr_cur,FLASH_SIZE_DMA_TH,DMAC_AHBMaster1);
            while(!flash_dma_read_done);
            flash_dma_read_done = 0;

            dst_addr_cur += FLASH_SIZE_DMA_TH;
            src_addr_cur += FLASH_SIZE_DMA_TH;
        }

        uint16_t size_left = size%FLASH_SIZE_DMA_TH;
        if(0 != size_left)
        {
            MASK_ROM_LIB_FUNC->newlibcfunc.memcpy_p((void*)dst_addr_cur,(void*)src_addr_cur,size_left);
        }
    }
    else
    {
        //速度待测试
        MASK_ROM_LIB_FUNC->newlibcfunc.memcpy_p((void*)dst_addr,(void*)(FLASH_CPU_READ_BASE_ADDR + src_addr),size);
    }
}


void flash_manage_init(void)
{
    flash_op_semaphore = xSemaphoreCreateBinary();
    if(NULL == flash_op_semaphore)
    {
        FMC_CI_ASSERT(0,"\n");
    }
    xSemaphoreGive(flash_op_semaphore); 

    // for(int i=0;i<FLASH_OPERATE_MAX_NUM;i++)
    // {
    //     flash_op_done_sem[i] = xSemaphoreCreateBinary();
    //     if(NULL == flash_op_semaphore)
    //     {
    //         FMC_CI_ASSERT(0,"\n");
    //     }
    //     xSemaphoreTake(flash_op_done_sem[i],0);
    // }

    flash_op_done_event_group = xEventGroupCreate();
    if (!flash_op_done_event_group)
    {
        FMC_CI_ASSERT(0,"\n");
    }

    // scu_set_device_reset(HAL_GDMA_BASE);
    // scu_set_device_reset_release(HAL_GDMA_BASE);
    scu_set_device_gate(HAL_GDMA_BASE,ENABLE);
    eclic_irq_enable(DMA_IRQn);
    clear_dma_translate_flag(DMACChannel0);
}


void flash_manage_read(uint32_t dst_addr,uint32_t src_addr,uint32_t size)
{
    flash_manage_take_sem();
    ciss_set(CI_SS_FLASH_BNPU_STATE,CI_SS_FLASH_READ);
    if(FLASH_STATE_IDLE != flash_state)
    {
        FMC_CI_ASSERT(0,"\n");
    }

    //
    flash_state = FLASH_STATE_READ_BUSY;
    
    flash_read_intelli(dst_addr,src_addr,size);

    flash_state = FLASH_STATE_IDLE;

    flash_manage_give_sem();
}


void flash_init_to_xip(void)
{
    scu_run_in_flash();
    flash_init(QSPI0,DISABLE);
    spic_xipconfig(QSPI0);
    // vTaskDelay(pdMS_TO_TICKS(10));
}


void flash_config_to_normal(void)
{
    // scu_run_not_in_flash();
    // flash_init(QSPI0);
    spic_prefetch_en(QSPI0,false);
}


void flash_config_to_xip(void)
{
    scu_run_in_flash();
    // flash_init(QSPI0);
    spic_xipconfig(QSPI0);
}


uint32_t max_read_time = 0;
uint32_t max_write_time = 0;
uint32_t max_erase_time = 0;



static void flash_power_off_deal_with(flash_ctr_msg_t* msg_p)
{
    flash_operate_type_t op_type = msg_p->op_type;
    uint32_t dst_addr = msg_p->dst_addr;
    uint32_t src_addr = msg_p->src_addr;
    uint32_t size = msg_p->op_size;
    switch(op_type)
    {
        case FLASH_INIT_OPERTATE:
        {
            flash_manage_take_sem();
            //初始化flash为XIP模式
            flash_init_to_xip();
            flash_state = FLASH_STATE_IDLE;
            flash_manage_give_sem();

            flash_manage_give_op_done_sem(FLASH_INIT_OPERTATE);
            break;
        }
        default:
        {
            FMC_CI_ASSERT(0,"\n");
            break;
        }
    }
}


static void flash_idle_deal_with(flash_ctr_msg_t* msg_p)
{
    flash_operate_type_t op_type = msg_p->op_type;
    uint32_t dst_addr = msg_p->dst_addr;
    uint32_t src_addr = msg_p->src_addr;
    uint32_t size = msg_p->op_size;

    // extern int32_t spic_check_busy(spic_base_t spic,int32_t timeout);
    // int ret = spic_check_busy(QSPI0,20*100000);
    switch(op_type)
    {
        case FLASH_READ_OPERTATE:
        case FLASH_READ_OPERTATE_INT:
        {
            //读flash的流程
            // timer0_start_debug_time(true);
            flash_manage_read(dst_addr,src_addr,size);
            if(FLASH_READ_OPERTATE == op_type)
            {
                flash_manage_give_op_done_sem(FLASH_READ_OPERTATE);
            }
            else
            {
                post_read_flash_int_op_done();
            }
            
            // uint32_t time = timer0_end_debug_time(true,&max_read_time);
            break;
        }
        case FLASH_WRITE_OPERTATE:
        case FLASH_WRITE_OPERTATE_INT:
        {
            // mprintf("%s:%d\n",__FUNCTION__,__LINE__);
            // mprintf("write:");
            // timer0_start_debug_time(true);
            flash_manage_take_sem();
            flash_state = FLASH_STATE_WRITE_BUSY;
            ciss_set(CI_SS_FLASH_BNPU_STATE,CI_SS_FLASH_WRITE);
            //写操作流程
            flash_config_to_normal();
            int32_t ret = flash_write(QSPI0,dst_addr,src_addr, size);
            if(RETURN_OK != ret)
            {
                FMC_CI_ASSERT(0,"\n");
            }
            flash_config_to_xip();
            flash_state = FLASH_STATE_IDLE;
            flash_manage_give_sem();
            if(FLASH_WRITE_OPERTATE == op_type)
            {
                flash_manage_give_op_done_sem(FLASH_WRITE_OPERTATE);
            }
            else
            {
                post_write_flash_int_op_done();
            }
            
            // uint32_t time = timer0_end_debug_time(true,&max_write_time);
            break;
        }
        case FLASH_READ_UNIQUE_ID_OPERTATE:
        case FLASH_READ_UNIQUE_ID_OPERTATE_INT:
        {
            // mprintf("%s:%d\n",__FUNCTION__,__LINE__);
            // mprintf("write:");
            // timer0_start_debug_time(true);
            flash_manage_take_sem();
            flash_state = FLASH_STATE_READ_UNIQUE_ID_BUSY;
            ciss_set(CI_SS_FLASH_BNPU_STATE,CI_SS_FLASH_READ_UNIQUE_ID);
            //写操作流程
            flash_config_to_normal();
            int32_t ret = spic_read_unique_id(QSPI0,(uint8_t*)dst_addr);
            if(RETURN_OK != ret)
            {
                FMC_CI_ASSERT(0,"\n");
            }
            flash_config_to_xip();
            flash_state = FLASH_STATE_IDLE;
            flash_manage_give_sem();
            if(FLASH_READ_UNIQUE_ID_OPERTATE == op_type)
            {
                flash_manage_give_op_done_sem(FLASH_READ_UNIQUE_ID_OPERTATE);
            }
            else
            {
                post_read_unique_id_int_op_done();
            }
            
            // uint32_t time = timer0_end_debug_time(true,&max_write_time);
            break;
        }
        case FLASH_ERASE_OPERTATE:
        case FLASH_ERASE_OPERTATE_INT:
        {
            // mprintf("%s:%d\n",__FUNCTION__,__LINE__);
            // mprintf("erase:");
            // timer0_start_debug_time(true);
            flash_manage_take_sem();
            flash_state = FLASH_STATE_ERASE_BUSY;
            ciss_set(CI_SS_FLASH_BNPU_STATE,CI_SS_FLASH_ERASE);
            //擦除操作流程
            flash_config_to_normal();
            int32_t ret = flash_erase(QSPI0,dst_addr,size);
            if(RETURN_OK != ret)
            {
                FMC_CI_ASSERT(0,"\n");
            }
            flash_config_to_xip();
            flash_state = FLASH_STATE_IDLE;
            flash_manage_give_sem();
            if(FLASH_ERASE_OPERTATE == op_type)
            {
                flash_manage_give_op_done_sem(FLASH_ERASE_OPERTATE);
            }
            else
            {
                post_erase_flash_int_op_done();
            }
            
            // uint32_t time = timer0_end_debug_time(true,&max_erase_time);
            break;
        }
        case FLASH_POWER_OFF_OPERTATE:
        {
            flash_manage_take_sem();
            flash_state = FLASH_STATE_POWER_OFF;
            //power off操作
            flash_state = FLASH_STATE_POWER_OFF;
            flash_manage_give_sem();
            ciss_set(CI_SS_FLASH_BNPU_STATE,CI_SS_FLASH_POWER_OFF);
            flash_manage_give_op_done_sem(FLASH_POWER_OFF_OPERTATE);
            break;
        }
        default:
        {
            FMC_CI_ASSERT(0,"\n");
            break;
        }
    }
}


//read busy时，只相应read请求
static void flash_read_busy_deal_with(flash_ctr_msg_t* msg_p)
{
    flash_operate_type_t op_type = msg_p->op_type;
    uint32_t dst_addr = msg_p->dst_addr;
    uint32_t src_addr = msg_p->src_addr;
    uint32_t size = msg_p->op_size;

    // extern int32_t spic_check_busy(spic_base_t spic,int32_t timeout);
    // int ret = spic_check_busy(QSPI0,20*100000);
    switch(op_type)
    {
        case FLASH_READ_OPERTATE:
        case FLASH_READ_OPERTATE_INT:
        {
            //读flash的流程
            // printf("read:");
            // timer0_start_debug_time(true);
            flash_manage_read(dst_addr,src_addr,size);
            if(FLASH_READ_OPERTATE == op_type)
            {
                flash_manage_give_op_done_sem(FLASH_READ_OPERTATE);
            }
            else
            {
                post_read_flash_int_op_done();
            }
            
            // uint32_t time = timer0_end_debug_time(true,&max_read_time);
            break;
        }
        default:
        {
            FMC_CI_ASSERT(0,"\n");
            break;
        }
    }
}



void flash_manage_flow(flash_ctr_msg_t* msg_p)
{
    switch(flash_state)
    {
        case FLASH_STATE_POWER_OFF:
        {
            flash_power_off_deal_with(msg_p);
            break;
        }
        case FLASH_STATE_IDLE:
        {
            flash_idle_deal_with(msg_p);
            break;
        }
        case FLASH_STATE_READ_BUSY:
        {
            flash_read_busy_deal_with(msg_p);
            break;
        }
        default:
        {
            FMC_CI_ASSERT(0,"\n");
            break;
        }
    }
}

