#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include "ci_gcc_doa.h"
#include "ci_doa_apply.h"
#include "bnpu_mem_manage.h"
//#include "ci_nn_doa.h"
#include "ci_nn_doa_init_param.h"
#include "status_share.h"
#include "ci_log.h"
#include "remote_api_for_bnpu.h"
#include "ci_nn_manage.h"


int ci_doa_deal_for_application(void *doa, float **fft_in, short *mic_l)
{
#if USE_NN_DOA
    ci_nn_doa_tdnn_cmpt_info_t cmpt_str;
   
	memset((void*)&cmpt_str,0,sizeof(cmpt_str));
	cmpt_str.handle = doa;
	cmpt_str.fft_in = fft_in;
    cmpt_str.mic_l = mic_l;
	req_doa_tdnn_cmpt(&cmpt_str);
#elif USE_GCC_DOA
    if(!doa)
    {
        CI_ASSERT(0, "\n");
    }
    if(ciss_get(CI_SS_AEC_WORK_STATE))
    {
       return 0;
    }
    ci_gcc_doa_deal(doa, fft_in);

#endif 
    return 0;   
}