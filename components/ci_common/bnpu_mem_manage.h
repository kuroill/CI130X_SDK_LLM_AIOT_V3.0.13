#ifndef __BNPU_MEM_MANAGE_H__
#define __BNPU_MEM_MANAGE_H__

#include <stdint.h>
#include <stdio.h>
#include "romlib_runtime.h"
#include "ci_assert.h"
#include "remote_api_for_bnpu.h"
#include "ci130x_nuclear_com.h"
#include "sdk_default_config.h"

void *bnpu_remote_malloc(size_t malloc_size);
void *bnpu_remote_calloc(size_t num, size_t size);
void  bnpu_remote_free(void *free_ptr);
uint32_t get_alg_malloc_size(void);
uint32_t get_remote_calloc_size(void);
void doa_aec_share_buffer_init(void);
void doa_aec_share_buffer_memset(void);
void *doa_buffer_malloc(size_t malloc_size);
void *doa_buffer_calloc(size_t num, size_t size);
void doa_buffer_free(void *free_ptr);
void *aec_buffer_malloc(size_t malloc_size);
void *aec_buffer_calloc(size_t num, size_t size);
void aec_buffer_free(void *free_ptr);
#endif  //__BNPU_MEM_MANAGE_H__