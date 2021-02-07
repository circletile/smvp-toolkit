/* 
*  ==================================================================
*  main.c v0.2 for smvp-csr
*  ==================================================================
*/

#define _POSIX_C_SOURCE 200809L

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
        printf(ANSI_COLOR_RED "Could not process specified Matrix Market input file.\nRequired items not present on first line of file.\n" ANSI_COLOR_RESET);
        exit(1);
    }
    else if (retcode == MM_NO_HEADER)
    {
        printf(ANSI_COLOR_RED "Could not process specified Matrix Market input file.\nRequired header is missing or file contents may not be Matrix Market formatted.\n" ANSI_COLOR_RESET);
        exit(1);
    }
    else if (retcode == MM_UNSUPPORTED_TYPE)
    {
        printf(ANSI_COLOR_RED "Could not process specified Matrix Market input file.\nMatrix content description not parseable or is absent.\n" ANSI_COLOR_RESET);
        exit(1);
    }
    else
    {
        printf(ANSI_COLOR_RED "Could not process specified Matrix Market input file.\nUnhandled exception occured during file loading .\n" ANSI_COLOR_RESET);
        exit(1);
    }
}

int main(int argc, char *argv[])
{

    FILE *mmInputFile, *mmOutputFile;
    MM_typecode matcode;
    int mmio_rb_return, mmio_rs_return, index, j, compIter;
    int fInputRows, fInputCols, fInputNonZeros;
    int *unitVector, *outputVector;
    struct timespec start, end;

    // Validate user provided arguments, handle exceptions accordingly
    if (argc < 2)
    {
        fprintf(stderr, ANSI_COLOR_YELLOW "Usage: %s [martix-market-filename]\n" ANSI_COLOR_RESET, argv[0]);
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
        printf(ANSI_COLOR_RED "This application only supports sparse matricies.\nSpecified input file does not appear to contain a sparse matrix.\n" ANSI_COLOR_RESET);
        exit(1);
    }

    // Load sparse matrix properties from input file
    printf(ANSI_COLOR_YELLOW "Loading matrix content from source file...\n" ANSI_COLOR_RESET);
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

    /* [DEBUG] CSR compression sanity check
    printf(ANSI_COLOR_YELLOW "row_ptr:\n" ANSI_COLOR_RESET);
    for (index = 0; index < fInputRows + 1; index++)
    {
        printf(ANSI_COLOR_YELLOW "%d  " ANSI_COLOR_RESET, workingMatrix.row_ptr[index]);
    }
    printf(ANSI_COLOR_MAGENTA "\nval:\n" ANSI_COLOR_RESET);
    for (index = 0; index < fInputNonZeros; index++)
    {
        printf(ANSI_COLOR_MAGENTA "%g  " ANSI_COLOR_RESET, workingMatrix.val[index]);
    }
    printf(ANSI_COLOR_BLUE "\ncol_ind:\n" ANSI_COLOR_RESET);
    for (index = 0; index < fInputNonZeros; index++)
    {
        printf(ANSI_COLOR_BLUE "%d  " ANSI_COLOR_RESET, workingMatrix.col_ind[index]);
    }
    printf("\n");
    */

    // Load the vector to be multiplied and prepare an output vector
    unitVector = malloc(sizeof(int) * fInputRows);
    outputVector = malloc(sizeof(int) * fInputRows);
    for (index = 0; index < fInputRows; index++)
    {
        unitVector[index] = 1;
        outputVector[index] = 0;
    }

    // Set up logging of compute time
    clock_gettime(CLOCK_MONOTONIC, &start);

    // Compute the sparse vector multiplication
    for (compIter = 0; compIter < 1000; compIter++)
    {
        for (index = 0; index < fInputRows; index++)
        {
            for (j = workingMatrix.row_ptr[index]; j < workingMatrix.row_ptr[index + 1]; j++)
            {
                outputVector[index] += workingMatrix.val[j] * unitVector[workingMatrix.col_ind[j]];
            }
        }
    }

    // Write compute time results to terminal and/or output file
    clock_gettime(CLOCK_MONOTONIC, &end);

    double time_taken;
    time_taken = (end.tv_sec - start.tv_sec) * 1e9;
    time_taken = (time_taken + (end.tv_nsec - start.tv_nsec)) * 1e-9;

    printf(ANSI_COLOR_GREEN "\nTime taken by computations is : %g seconds.\n" ANSI_COLOR_RESET, time_taken);

    /*
    * [DEBUG] Output vector sanity check
    printf(ANSI_COLOR_YELLOW "\nOutput vector:\n" ANSI_COLOR_RESET);
    for (index = 0; index < fInputRows; index++)
    {
        printf("%g\n", outputVector[index]);
    }
    *
    */

    return 0;
}
