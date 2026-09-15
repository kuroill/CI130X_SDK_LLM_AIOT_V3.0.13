/**
 * @file platform_config.c
 * @brief 统一初始化io、中断优先级等
 * @version 1.0
 * @date 2019-06-12
 * 
 * @copyright Copyright (c) 2019 Chipintelli Technology Co., Ltd.
 * 
 */
#include "platform_config.h"
#include "ci130x_system.h"
#include "ci130x_scu.h"
#include "ci130x_core_eclic.h"
#include "ci130x_gpio.h"
#include "sdk_default_config.h"
#include "romlib_runtime.h"
#include "board.h"
#include "ci130x_mailbox.h"
#include "status_share.h"




/**
 * @brief 配置系统中断优先级
 * 
 */
void init_irq_pri(void)
{
    for (int i = TWDG_IRQn;i <= MAILBOX_IRQn;i++ )
    {
        eclic_irq_set_priority(i, 6, 0);
    }
}


/**
 * @brief 配置总线时钟
 * 
 */
void init_clk_div()
{   
    // uint32_t main_frequency = dpmu_get_pll_frequency();

    // /* PLL 480M ip_core 240M */
    // set_ipcore_clk(main_frequency);
    
    // /* AHB 240M */
    // set_ahb_clk(main_frequency);

    // /* APB 120M */
    // void set_apb_clk(uint32_t clk);
    // set_apb_clk(main_frequency/2);

    // /* SRC 12.288M */
    // void set_src_clk(uint32_t clk);
    // set_src_clk(main_frequency);

    // /* 内核timer时钟 7.5M x 2(双边沿) */
    // set_systick_clk(main_frequency/12);
}




/**
 * @brief 初始化系统
 * 
 */
void init_platform(void)
{    
    init_clk_div();
    init_irq_pri();
    mailbox_preinit();
}


/**
 * @brief 获取ipcore时钟
 * 
 * @return uint32_t ipcore时钟
 */
uint32_t get_ipcore_clk(void)
{
    volatile uint32_t ret = (uint32_t)ciss_get(CI_SS_IPCORE_CLK);
    return ret;
}


/**
 * @brief 获取AHB时钟
 * 
 * @return uint32_t AHB时钟
 */
uint32_t get_ahb_clk(void)
{
    return (uint32_t)ciss_get(CI_SS_AHB_CLK);
}


/**
 * @brief 获取APB时钟
 * 
 * @return uint32_t APB时钟
 */
uint32_t get_apb_clk(void)
{
    return (uint32_t)ciss_get(CI_SS_APB_CLK);
}


/**
 * @brief 获取systick时钟
 * 
 * @return uint32_t systick时钟
 */
uint32_t get_systick_clk(void)
{
    volatile uint32_t ret = (uint32_t)ciss_get(CI_SS_SYSTEMTICK_CLK);
    return ret;
}



/**
 * @brief 获取osc时钟
 * 
 * @return uint32_t osc时钟
 */
uint32_t get_osc_clk(void)
{
    return (uint32_t)ciss_get(CI_SS_EXT_OSC_CLK);
}

uint32_t get_src_clk(void)
{
    volatile uint32_t ret = (uint32_t)ciss_get(CI_SS_SRC_CLK);
    return ret;
}



/**
 * @brief 初始化 maskrom lib
 * 
 */
void maskrom_lib_init(void)
{
    //mprintf("maskrom lib mark %s!\n",MASK_ROM_LIB_FUNC->ci_lib_romruntime);
    //mprintf("maskrom lib ver %d!\n",MASK_ROM_LIB_FUNC->verison);
	MASK_ROM_LIB_FUNC->init_lib_romruntime_p();
	extern char *sbrk(int incr);
	extern void __malloc_lock(struct _reent *p);
	extern void __malloc_unlock(struct _reent *p);
	MASK_ROM_LIB_FUNC->newlibcfunc.NEWLib_Set_Func_p((void *)sbrk,(void *)__malloc_lock,(void *)__malloc_unlock);
	MASK_ROM_LIB_FUNC->mp3func.MP3Lib_Set_Func_p((void *)(MASK_ROM_LIB_FUNC->newlibcfunc.malloc_p),
												  (void *)(MASK_ROM_LIB_FUNC->newlibcfunc.free_p),
												  (void *)(MASK_ROM_LIB_FUNC->newlibcfunc.memcpy_p),
												  (void *)(MASK_ROM_LIB_FUNC->newlibcfunc.memmove_p),
												  (void *)(MASK_ROM_LIB_FUNC->newlibcfunc.memset_p));
}




