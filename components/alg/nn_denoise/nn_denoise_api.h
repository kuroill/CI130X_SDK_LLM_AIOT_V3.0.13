#ifndef NN_DENOISE_API_H
#define NN_DENOISE_API_H 


#include <stdio.h>
#include <stdbool.h>
#ifdef __cplusplus
extern "C"{
#endif

typedef struct
{
	bool alg_enable;
	float alpha_forget;
	int denoise_mode;    
	bool denoise_beta_adaptive_mode; 
	float denoise_beta;
	float denoise_ratio_upper_limit;
	float denoise_ratio_lower_limit;
	float denoise_energy_highest_thr;
	float denoise_energy_lowest_thr;
}denoise_nn_config_t;

int ci_nn_denoise_version( void );
void* ci_nn_denoise_create(void* module_config);

void ci_nn_denoise_npu_init(void);

#ifdef __cplusplus
}
#endif
#endif