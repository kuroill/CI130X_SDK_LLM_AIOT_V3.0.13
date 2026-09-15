/**
 * @file asr_malloc_port.c
 * @brief 
 * @version 0.1
 * @date 2019-06-19
 * 
 * @copyright Copyright (c) 2019  Chipintelli Technology Co., Ltd.
 * 
 */

#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "ci_log.h"
#include "asr_malloc_port.h"
#include "ci_system_info.h"
#include "FreeRTOS.h"

static int asr_malloc_times = 0;
static int asr_free_times = 0;

void free_insram_bnpu(void* p)
{
    if(p)
    {
        vPortFree(p);
    }
}

void* malloc_insram_bnpu(int size)
{
    if(size > 0)
    {
        char * ptr = NULL;
        ptr = (char*)pvPortMalloc(size);
        return ptr;
    }
    return NULL;
}


/**
 * @brief decoder lib used malloc function, can add some info for debug
 * 
 * @param size : malloc size
 * @return void* : malloc address,NULL is malloc failed
 */
void *decoder_port_malloc(int size)
{
#if 1
    //asr_malloc_times++;
    if(size > 0)
    {
        char *ptr ;
        // ptr = (char*)pvPortMalloc(size);
        ptr = malloc(size);
        return ptr;
    }
    return NULL;
#else
  
#endif
}
/**
 * @brief decoder lib used malloc function, can add some info for debug
 * 
 * @param pp : free address pointer
 */
void decoder_port_free(void *pp)
{
#if 1
    //asr_free_times++;
    if(pp)
    {
        // vPortFree(pp);
        free(pp);
    }
#else
#endif
}



