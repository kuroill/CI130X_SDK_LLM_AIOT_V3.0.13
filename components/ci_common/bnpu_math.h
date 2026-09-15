#ifndef __BNPU_MATH_H_
#define __BNPU_MATH_H_
#include <stdio.h> 
#include <malloc.h>
#include "FreeRTOS.h" 

#define FFT_SHIFT        (256)
#define FREQ_SIZE        (FFT_SHIFT + 1)
#define FFT_SIZE		 (512)

//浮点四舍五入到int32
#define __RV_FLOAT_COV_TO_INT_RNE(f)		\
		({	\
			int32_t result;		\
			float __f = (float)f;	\
			asm volatile ("fcvt.w.s %0,%1,rne" :"=r"(result) :"f"(f)	);	\
			result;	\
		})
typedef struct
{
    float real;
    float image;
}Complex;

float ci_sqrt_f32(float x);
float hard_fsqrt(float x);
float tansig_approx(float x);
float ci_fmin_f32(float x1,float x2);
float ci_fmax_f32(float x1,float x2);
float ci_fsgnj_f32(float x,float sign_f);
float ci_sigmoid(float x);
float one_float_saturate(float f_src,float min,float max);
#endif   //__BNPU_MATH_H_