
#include "bnpu_mem_manage.h"
#include "alg_preprocess.h"

uint32_t g_bnpu_remote_malloc_size = 0;
uint32_t g_bnpu_remote_calloc_size = 0;
uint32_t g_tcm_total_size = 0;
extern ci_ssp_config_t g_ci_ssp_config;

void *bnpu_malloc_in_tcm( size_t xWantedSize ) 
{
    void* ret = pvPortMalloc_644( xWantedSize );
    if(0 != ret)
    {
        g_tcm_total_size += xWantedSize;
    }
    return ret;
}

void bnpu_free_in_tcm( void *pv ) 
{
    if(!pv)
    {
        return;
    }
    vPortFree_644( pv );
}

void *bnpu_remote_malloc(size_t malloc_size)
{
#if !SDK_RELEASE_EN
    CI_ASSERT(malloc_size, "\n");
#endif
    void *addr = (void *)REMOTE_CALL(malloc_in_host)(malloc_size);
#if !SDK_RELEASE_EN
    CI_ASSERT(addr, "\n");
#endif
    g_bnpu_remote_malloc_size += malloc_size;
    return addr;
}

void *bnpu_remote_calloc(size_t num, size_t size)
{
    size_t alg_calloc_size  = num * size;
#if !SDK_RELEASE_EN
    CI_ASSERT(alg_calloc_size, "\n");
#endif
    void *addr = (void *)REMOTE_CALL(malloc_in_host)(alg_calloc_size);

#if !SDK_RELEASE_EN
    CI_ASSERT(addr, "\n");
#endif
    if(addr)
    {
        MASK_ROM_LIB_FUNC->newlibcfunc.memset_p(addr, 0, alg_calloc_size);
    }
    g_bnpu_remote_calloc_size += alg_calloc_size;    
    return addr;
}
void bnpu_remote_free(void *free_ptr)
{
#if !SDK_RELEASE_EN
    CI_ASSERT(free_ptr, "\n");
#endif
    if (free_ptr)
    {
        REMOTE_CALL(free_in_host)(free_ptr);
    }
}


//-----------------
void *bnpu_mem_malloc(size_t malloc_size)
{
#if !SDK_RELEASE_EN
    CI_ASSERT(malloc_size, "\n");
#endif
    void *addr = (void *)pvPortMalloc(malloc_size);
#if !SDK_RELEASE_EN
    CI_ASSERT(addr, "\n");
#endif
    g_bnpu_remote_malloc_size += malloc_size;
    return addr;
}

void *bnpu_mem_calloc(size_t num, size_t size)
{
    size_t alg_calloc_size  = num * size;
#if !SDK_RELEASE_EN
    CI_ASSERT(alg_calloc_size, "\n");
#endif
    void *addr = (void *)pvPortMalloc(alg_calloc_size);

#if !SDK_RELEASE_EN
    CI_ASSERT(addr, "\n");
#endif
    if(addr)
    {
        memset(addr, 0, alg_calloc_size);
    }
    g_bnpu_remote_calloc_size += alg_calloc_size;    
    return addr;
}
void bnpu_mem_free(void *free_ptr)
{
#if !SDK_RELEASE_EN
    CI_ASSERT(free_ptr, "\n");
#endif
    if (free_ptr)
    {
        vPortFree(free_ptr);
    }
}
//------------

uint32_t get_alg_malloc_size(void)
{
    return g_bnpu_remote_malloc_size;
}

uint32_t get_remote_calloc_size(void)
{
    return g_bnpu_remote_calloc_size;
}


#define DOA_AEC_SHARE_BUF_SIZE 27 * 1024
static void *doa_aec_buffer_basic = NULL;
static void *doa_buffer_temp = NULL;
static void *aec_buffer_temp = NULL;

//DOA+AEC共享内
void doa_aec_share_buffer_init(void)
{
    if(g_ci_ssp_config.doa.alg_en && g_ci_ssp_config.aec.alg_en)
    {
        if (!doa_aec_buffer_basic)
        {
            doa_aec_buffer_basic = bnpu_remote_calloc(DOA_AEC_SHARE_BUF_SIZE, 1);
        }
        doa_buffer_temp = doa_aec_buffer_basic;
        aec_buffer_temp = doa_aec_buffer_basic;
    }
}

void doa_aec_share_buffer_memset(void)
{
    if(g_ci_ssp_config.doa.alg_en && g_ci_ssp_config.aec.alg_en)
    {
#if !SDK_RELEASE_EN
        CI_ASSERT(doa_aec_buffer_basic, "\n");
#endif
        MASK_ROM_LIB_FUNC->newlibcfunc.memset_p(doa_aec_buffer_basic, 0, DOA_AEC_SHARE_BUF_SIZE);
    }

}

void *doa_buffer_malloc(size_t malloc_size)
{
#if !SDK_RELEASE_EN
    CI_ASSERT(malloc_size, "\n");
#endif
    void *pv = NULL;
    if (malloc_size > 0)
    {
        if (malloc_size % 4)
        {
            malloc_size = (malloc_size / 4 + 1) * 4;
        }
        if(g_ci_ssp_config.doa.alg_en && g_ci_ssp_config.aec.alg_en)
        {
            pv = doa_buffer_temp;
            doa_buffer_temp = (void *)((uint32_t)doa_buffer_temp + (uint32_t)malloc_size);
            uint32_t buffer_end_addr = doa_aec_buffer_basic + DOA_AEC_SHARE_BUF_SIZE;
#if !SDK_RELEASE_EN
        CI_ASSERT((doa_buffer_temp < buffer_end_addr), "\n");
#endif
        }
        else
        {
            pv = bnpu_mem_malloc(malloc_size);
        }
    }
    return pv;
}

void *doa_buffer_calloc(size_t num, size_t size)
{
    size_t calloc_size = num * size;
#if !SDK_RELEASE_EN
    CI_ASSERT(calloc_size, "\n");
#endif
    if (calloc_size % 4)
    {
        calloc_size = (calloc_size / 4 + 1) * 4;
    }
    void *pv = NULL;
    if(g_ci_ssp_config.doa.alg_en && g_ci_ssp_config.aec.alg_en)
    {
        pv = doa_buffer_temp;
        doa_buffer_temp = (void *)((uint32_t)doa_buffer_temp + (uint32_t)calloc_size);
        uint32_t buffer_end_addr = doa_aec_buffer_basic + DOA_AEC_SHARE_BUF_SIZE;
#if !SDK_RELEASE_EN
        CI_ASSERT((doa_buffer_temp < buffer_end_addr), "\n");
#endif
        MASK_ROM_LIB_FUNC->newlibcfunc.memset_p(pv, 0, calloc_size);
    }
    else
    {
        pv = bnpu_mem_calloc(calloc_size, 1);
    }
    return pv;
}

void doa_buffer_free(void *free_ptr)
{
    if(g_ci_ssp_config.doa.alg_en && g_ci_ssp_config.aec.alg_en)
    {
        doa_buffer_temp = doa_aec_buffer_basic;
    }
    else
    {
        if (free_ptr)
        {
            bnpu_mem_free(free_ptr);
            free_ptr = NULL;
        }
    }
}
//aec
void *aec_buffer_malloc(size_t malloc_size)
{
    void *pv = aec_buffer_temp;
#if !SDK_RELEASE_EN
    CI_ASSERT(malloc_size, "\n");
#endif
    if (malloc_size % 4)
    {
        malloc_size = (malloc_size / 4 + 1) * 4;
    }
    if(g_ci_ssp_config.doa.alg_en && g_ci_ssp_config.aec.alg_en)
    {
        aec_buffer_temp = (void *)((uint32_t)aec_buffer_temp + (uint32_t)malloc_size);
        uint32_t buffer_end_addr = doa_aec_buffer_basic + DOA_AEC_SHARE_BUF_SIZE;
#if !SDK_RELEASE_EN
        CI_ASSERT((aec_buffer_temp < buffer_end_addr), "\n");
#endif
    }
    else
    {
        pv = bnpu_mem_malloc(malloc_size);
    }
    return pv;
}
void *aec_buffer_calloc(size_t num, size_t size)
{
    int calloc_size = num*size;
#if !SDK_RELEASE_EN
    CI_ASSERT(calloc_size, "\n");
#endif
    if (calloc_size % 4)
    {
        calloc_size = (calloc_size / 4 + 1) * 4;
    }
    void *pv = NULL;
    if(g_ci_ssp_config.doa.alg_en && g_ci_ssp_config.aec.alg_en)
    {
        pv = aec_buffer_temp;
        aec_buffer_temp = (void *)((uint32_t)aec_buffer_temp + (uint32_t)calloc_size);
        uint32_t buffer_end_addr = doa_aec_buffer_basic + DOA_AEC_SHARE_BUF_SIZE;
#if !SDK_RELEASE_EN
        CI_ASSERT((aec_buffer_temp < buffer_end_addr), "\n");
#endif
    memset(pv, 0, calloc_size);
    }
    else
    {
        pv = bnpu_mem_calloc(calloc_size, 1);
    }
    return pv;
}

void aec_buffer_free(void *free_ptr)
{
    if(g_ci_ssp_config.doa.alg_en && g_ci_ssp_config.aec.alg_en)
    {
        aec_buffer_temp = doa_aec_buffer_basic;
    }
    else
    {
         if (free_ptr)
        {
            bnpu_mem_free(free_ptr);
            free_ptr = NULL;
        }
    }
}