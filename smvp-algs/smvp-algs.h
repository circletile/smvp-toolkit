/* 
*  ==================================================================
*  smvp-algs.h v0.4 for smvp-toolbox
*  ==================================================================
*/


#ifndef SMVP_CSR
#define SMVP_CSR

#define _POSIX_C_SOURCE 200809L // Required to utilize HPET for execution time calculations (via CLOCK_MONOTONIC)

typedef struct _csr_data_ *_csr_data_pointer;

void vectorInit(int vectorLen, double *outputVector, double val);
int mmrd_comparator(const void *v1, const void *v2);
void smvp_csr_compute();
void smvp_csr_debug();


#endif
