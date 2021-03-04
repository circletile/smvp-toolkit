/* 
*  ==================================================================
*  smvp-algs.c v0.4 for smvp-toolbox
*  ==================================================================
*/

#include <stdlib.h>
#include <time.h>

// Struct: _csr_data_
// Provides a convenient structure for storing/manipulating CSR compressed data
typedef struct _csr_data_
{
    int *row_ptr;
    int *col_ind;
    double *val;
} CSRData;

// Function: vectorInit
// Reinitializes vectors between calculation iterations
void vectorInit(int vectorLen, double *outputVector, double val)
{
    for (int index = 0; index < vectorLen; index++)
    {
        outputVector[index] = val;
    }
}

// Function: smvp_csr_compute
// Calculates SMVP using CSR algorithm
void smvp_csr_compute(MMRawData *mmImportData, int fInputRows, int fInputNonZeros)
{

    CSRData workingMatrix;
    double *onesVector, *outputVector;
    struct timespec tsCompStart, tsCompEnd;
    double comp_time_taken;
    int j, compIter, index;

    workingMatrix.row_ptr = (int *)malloc(sizeof(int) * (long unsigned int)(fInputRows + 1));
    workingMatrix.col_ind = (int *)malloc(sizeof(int) * (long unsigned int)fInputNonZeros);
    workingMatrix.val = (double *)malloc(sizeof(double) * (long unsigned int)fInputNonZeros);

    for (index = 0; index < fInputNonZeros; index++)
    {
        workingMatrix.val[index] = mmImportData[index].val;
        workingMatrix.col_ind[index] = mmImportData[index].col;

        if (index == fInputNonZeros - 1)
        {
            workingMatrix.row_ptr[mmImportData[index].row + 1] = fInputNonZeros;
        }
        else if (mmImportData[index].row < mmImportData[(index + 1)].row)
        {
            workingMatrix.row_ptr[mmImportData[index].row + 1] = index + 1;
        }
        else if (index == 0)
        {
            workingMatrix.row_ptr[mmImportData[index].row] = 0;
        }
    }

    // Prepare the "ones "vector and output vector
    onesVector = (double *)malloc(sizeof(double) * (long unsigned int)fInputRows);
    vectorInit(fInputRows, onesVector, 1);
    outputVector = (double *)malloc(sizeof(double) * (long unsigned int)fInputRows);

    // Capture compute start time
    clock_gettime(CLOCK_MONOTONIC, &tsCompStart);

    // Compute the sparse vector multiplication (technically y=Axn, not y=x(A^n) as indicated in reqs doc, but is an approved deviation)
    for (compIter = 0; compIter < 1000; compIter++)
    {
        //Reset output vector contents between iterations
        vectorInit(fInputRows, outputVector, 0);

        for (index = 0; index < fInputRows; index++)
        {
            for (j = workingMatrix.row_ptr[index]; j < workingMatrix.row_ptr[index + 1]; j++)
            {
                outputVector[index] += workingMatrix.val[j] * onesVector[workingMatrix.col_ind[j]];
            }
        }
    }

    // Capture compute end time & derive elapsed time
    clock_gettime(CLOCK_MONOTONIC, &tsCompEnd);
    comp_time_taken = (double)(tsCompEnd.tv_sec - tsCompStart.tv_sec) * 1e9;
    comp_time_taken = (comp_time_taken + (double)(tsCompEnd.tv_nsec - tsCompStart.tv_nsec)) * 1e-9;

    // RETURN SOMETHING
}

/*
void smvp_csr_debug(FILE *reportOutputFile, int fInputRows)
{
    // Append debug info in output file if required
    fprintf(reportOutputFile, "[DEBUG]\tCSR row_ptr:\n[");
    for (index = 0; index < fInputRows + 1; index++)
    {
        fprintf(reportOutputFile, "%d\n", workingMatrix.row_ptr[index]);
    }
    fprintf(reportOutputFile, "]\n[DEBUG]\tCSR val:\n[");
    for (index = 0; index < fInputNonZeros; index++)
    {
        fprintf(reportOutputFile, "%g\n", workingMatrix.val[index]);
    }
    fprintf(reportOutputFile, "]\n[DEBUG]\tCSR col_ind:\n[");
    for (index = 0; index < fInputNonZeros; index++)
    {
        fprintf(reportOutputFile, "%d\n", workingMatrix.col_ind[index]);
    }
    fprintf(reportOutputFile, "]\n");
}
*/