/* 
*  ==================================================================
*  main.c v0.3 for smvp-csr
*  ==================================================================
*/

#define _POSIX_C_SOURCE 200809L // Required to utilize HPET for execution time calculations (via CLOCK_MONOTONIC)

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "mmio.h"

// ANSI terminal color escape codes for making output BEAUTIFUL
#define ANSI_COLOR_RED "\x1b[31m"
#define ANSI_COLOR_GREEN "\x1b[32m"
#define ANSI_COLOR_YELLOW "\x1b[33m"
#define ANSI_COLOR_BLUE "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN "\x1b[36m"
#define ANSI_COLOR_RESET "\x1b[0m"

// Struct: _mm_raw_data_
// Provides a convenient structure for importing/exporting Matrix Market file contents
typedef struct _mm_raw_data_
{
    int row;
    int col;
    double val;
} MMRawData;

// Struct: _csr_data_
// Provides a convenient structure for storing/manipulating CSR compressed data
typedef struct _csr_data_
{
    int *row_ptr;
    int *col_ind;
    double *val;
} CSRData;

// Function: ov_Init
// Reinitializes vectors between calculation iterations
void vectorInit(int vectorLen, double *outputVector, double val)
{
    for (int index = 0; index < vectorLen; index++)
    {
        outputVector[index] = val;
    }
}

// Function: mmrd_comparitor
// Provides a comparitor function that matches the format expected by stdlib qsort()
// Sorts data by row, then by column
int mmrd_comparator(const void *v1, const void *v2)
{
    const MMRawData *p1 = (MMRawData *)v1;
    const MMRawData *p2 = (MMRawData *)v2;
    if (p1->row < p2->row)
        return -1;
    else if (p1->row > p2->row)
        return +1;
    else if (p1->col < p2->col)
        return -1;
    else if (p1->col > p2->col)
        return +1;
    else
        return 0;
}

// Function: mmioErrorHandler
// Provides a simple error handler for known error types (mmio.h)
void mmioErrorHandler(int retcode)
{
    if (retcode == MM_PREMATURE_EOF)
    {
        printf(ANSI_COLOR_RED "[ERROR] Could not process specified Matrix Market input file. Required parameters not present on first line of file.\n" ANSI_COLOR_RESET);
        exit(1);
    }
    else if (retcode == MM_NO_HEADER)
    {
        printf(ANSI_COLOR_RED "[ERROR] Could not process specified Matrix Market input file. Required header is missing or file contents may not be Matrix Market formatted.\n" ANSI_COLOR_RESET);
        exit(1);
    }
    else if (retcode == MM_UNSUPPORTED_TYPE)
    {
        printf(ANSI_COLOR_RED "[ERROR] Could not process specified Matrix Market input file. Matrix content description not parseable or is absent.\n" ANSI_COLOR_RESET);
        exit(1);
    }
    else
    {
        printf(ANSI_COLOR_RED "[ERROR] Could not process specified Matrix Market input file. Unhandled exception occured during file loading .\n" ANSI_COLOR_RESET);
        exit(1);
    }
}

int main(int argc, char *argv[])
{

    FILE *mmInputFile, *mmOutputFile;
    MM_typecode matcode;
    int mmio_rb_return, mmio_rs_return, index, j, compIter;
    int fInputRows, fInputCols, fInputNonZeros;
    double *onesVector, *outputVector;
    struct timespec tsCompStart, tsCompEnd;
    double comp_time_taken;

    // Validate user provided arguments, handle exceptions accordingly
    if (argc < 2)
    {
        fprintf(stderr, ANSI_COLOR_YELLOW "[HELP] Usage: %s [martix-market-filename]\n" ANSI_COLOR_RESET, argv[0]);
        exit(1);
    }
    else
    {
        if ((mmInputFile = fopen(argv[1], "r")) == NULL)
            exit(1);
    }

    // Ensure input file is of proper format and contains an appropriate matrix type
    mmio_rb_return = mm_read_banner(mmInputFile, &matcode);
    if (mmio_rb_return != 0)
    {
        mmioErrorHandler(mmio_rb_return);
    }
    else if (mm_is_sparse(matcode) == 0)
    {
        printf(ANSI_COLOR_RED "[ERROR] This application only supports sparse matricies. Specified input file does not appear to contain a sparse matrix.\n" ANSI_COLOR_RESET);
        exit(1);
    }

    // Load sparse matrix properties from input file
    printf(ANSI_COLOR_YELLOW "[INFO] Loading matrix content from source file...\n" ANSI_COLOR_RESET);
    mmio_rs_return = mm_read_mtx_crd_size(mmInputFile, &fInputRows, &fInputCols, &fInputNonZeros);
    if (mmio_rs_return != 0)
    {
        mmioErrorHandler(mmio_rs_return);
    }

    // Stage matrix content from the input file into working memory (not yet CSR compressed)
    MMRawData mmImportData[fInputNonZeros];
    for (index = 0; index < fInputNonZeros; index++)
    {
        if (mm_is_pattern(matcode) != 0)
        {
            fscanf(mmInputFile, "%d %d\n", &mmImportData[index].row, &mmImportData[index].col);
            mmImportData[index].val = 1; //Not really needed, but keeps the data sane just in case it does get referenced somewhere
        }
        else
        {
            fscanf(mmInputFile, "%d %d %lg\n", &mmImportData[index].row, &mmImportData[index].col, &mmImportData[index].val);
        }
        // Convert from 1-based coordinate system to 0-based coordinate system (make sure to unpack to the original format when writing output files!)
        mmImportData[index].row--;
        mmImportData[index].col--;
    }

    // The Matrix Market library sample runs this check, I assume its for closing the file only if it isn't somehow mapped as keyboard input
    if (mmInputFile != stdin)
    {
        fclose(mmInputFile);
    }

    // Convert loaded data to CSR format
    printf(ANSI_COLOR_YELLOW "[INFO] Converting loaded content to CSR format...\n" ANSI_COLOR_RESET);
    CSRData workingMatrix;
    workingMatrix.row_ptr = malloc(sizeof(int) * (fInputRows + 1));
    workingMatrix.col_ind = malloc(sizeof(int) * fInputNonZeros);
    workingMatrix.val = malloc(sizeof(double) * fInputNonZeros);

    qsort(mmImportData, fInputNonZeros, sizeof(MMRawData), mmrd_comparator); //MM format specs don't guarantee data is sorted for easy conversion to CSR...

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

    //[DEBUG] CSR compression sanity check
    printf(ANSI_COLOR_CYAN "[DEBUG] row_ptr:\n" ANSI_COLOR_RESET);
    for (index = 0; index < fInputRows + 1; index++)
    {
        printf("%d  ", workingMatrix.row_ptr[index]);
    }
    printf(ANSI_COLOR_CYAN "\n[DEBUG] val:\n" ANSI_COLOR_RESET);
    for (index = 0; index < fInputNonZeros; index++)
    {
        printf("%g  ", workingMatrix.val[index]);
    }
    printf(ANSI_COLOR_CYAN "\n[DEBUG] col_ind:\n" ANSI_COLOR_RESET);
    for (index = 0; index < fInputNonZeros; index++)
    {
        printf("%d  ", workingMatrix.col_ind[index]);
    }
    printf("\n");

    // Prepare the (unit) vector to be multiplied and output vector
    onesVector = (double *)malloc(sizeof(double) * fInputRows);
    vectorInit(fInputRows, onesVector, 1);
    outputVector = (double *)malloc(sizeof(double) * fInputRows);

    printf(ANSI_COLOR_MAGENTA "[DATA] Vector operand in use:\n" ANSI_COLOR_RESET);
    printf("Ones vector with dimensions [%d, %d]\n", fInputRows, 1);

    printf(ANSI_COLOR_YELLOW "[INFO] Computing n=1000 sparse-vector multiplication iterations...\n" ANSI_COLOR_RESET);

    // Set up logging of compute time
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
                printf(ANSI_COLOR_CYAN "[DEBUG] Line Calc:\n" ANSI_COLOR_RESET);
                printf("\tindex = %d, j = %d\n", index, j);
                printf("\tOpV_precalc(%g) += WM.val(%g) * 1V(%g)\n", outputVector[index], workingMatrix.val[j], onesVector[workingMatrix.col_ind[j]]);

                outputVector[index] += workingMatrix.val[j] * onesVector[workingMatrix.col_ind[j]];
            }
        }
    }

    // Write compute time results to terminal and/or output file
    clock_gettime(CLOCK_MONOTONIC, &tsCompEnd);

    comp_time_taken = (tsCompEnd.tv_sec - tsCompStart.tv_sec) * 1e9;
    comp_time_taken = (comp_time_taken + (tsCompEnd.tv_nsec - tsCompStart.tv_nsec)) * 1e-9;

    printf(ANSI_COLOR_GREEN "[DONE] Computations completed in %g seconds.\n" ANSI_COLOR_RESET, comp_time_taken);

    // [DEBUG] Output vector sanity check
    printf(ANSI_COLOR_CYAN "[DEBUG] Output vector:\n" ANSI_COLOR_RESET);
    printf("[");
    for (index = 0; index < fInputRows; index++)
    {
        printf("%g", outputVector[index]);
        if (index < fInputRows - 1)
        {
            printf(", ");
        }
        else
        {
            printf("]\n");
        }
    }

    return 0;
}
