/* 
*  ==================================================================
*  main.c v0.1 for smvp-csr
*  ==================================================================
*/

#include <stdio.h>
#include <stdlib.h>
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
    int mmio_rb_return, mmio_rs_return, mmIndex;
    int fInputRows, fInputCols, fInputNonZeros;

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
    else
    {
        if (mm_is_sparse(matcode) == 0)
        {
            printf(ANSI_COLOR_RED "This application only supports sparse matricies.\nSpecified input file does not appear to contain a sparse matrix.\n" ANSI_COLOR_RESET);
            exit(1);
        }
        else if (mm_is_general(matcode) == 0)
        {
            printf(ANSI_COLOR_RED "This application currently only supports general matricies.\nSpecified input file contains a symmetry declaration.\n" ANSI_COLOR_RESET);
            exit(1);
        }
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
    for (mmIndex = 0; mmIndex < fInputNonZeros; mmIndex++)
    {
        if (mm_is_pattern(matcode) != 0)
        {
            fscanf(mmInputFile, "%d %d\n", &mmImportData[mmIndex].row, &mmImportData[mmIndex].col);
            mmImportData[mmIndex].val = 1; //Not really needed, but keeps the data sane just in case it does get referenced somewhere
        }
        else
        {
            fscanf(mmInputFile, "%d %d %lg\n", &mmImportData[mmIndex].row, &mmImportData[mmIndex].col, &mmImportData[mmIndex].val);
        }
        // Convert from 1-based coordinate system to 0-based coordinate system (make sure to unpack to the original format when writing output files!)
        mmImportData[mmIndex].row--;
        mmImportData[mmIndex].col--;
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

    /*
    * [TEMP] Load/sort sanity check 

    mm_write_banner(stdout, matcode);
    mm_write_mtx_crd_size(stdout, fInputRows, fInputCols, fInputNonZeros);
    for (mmIndex = 0; mmIndex < fInputNonZeros; mmIndex++)
    {
        if (mm_is_pattern(matcode) != 0)
        {
            fprintf(stdout, "%d %d\n", mmImportData[mmIndex].row + 1, mmImportData[mmIndex].col + 1);
        }
        else
        {
            fprintf(stdout, "%d %d %g\n", mmImportData[mmIndex].row + 1, mmImportData[mmIndex].col + 1, mmImportData[mmIndex].val);
        }
    }
    *
    */

    for (mmIndex = 0; mmIndex < fInputNonZeros; mmIndex++)
    {
        workingMatrix.val[mmIndex] = mmImportData[mmIndex].val;
        workingMatrix.col_ind[mmIndex] = mmImportData[mmIndex].col;

        if (mmIndex == fInputNonZeros - 1)
        {
            workingMatrix.row_ptr[mmImportData[mmIndex].row + 1] = fInputNonZeros;
        }
        else if (mmImportData[mmIndex].row < mmImportData[(mmIndex + 1)].row)
        {
            workingMatrix.row_ptr[mmImportData[mmIndex].row + 1] = mmIndex + 1;
        }
        else if (mmIndex == 0)
        {
            workingMatrix.row_ptr[mmImportData[mmIndex].row] = 0;
        }
    }

    // [TEMP] CSR compression sanity check
    printf(ANSI_COLOR_YELLOW "row_ptr:\n" ANSI_COLOR_RESET);
    for (mmIndex = 0; mmIndex < fInputRows + 1; mmIndex++)
    {
        printf(ANSI_COLOR_YELLOW "%d  " ANSI_COLOR_RESET, workingMatrix.row_ptr[mmIndex]);
    }
    printf(ANSI_COLOR_MAGENTA "\nval:\n" ANSI_COLOR_RESET);
    for (mmIndex = 0; mmIndex < fInputNonZeros; mmIndex++)
    {
        printf(ANSI_COLOR_MAGENTA "%g  " ANSI_COLOR_RESET, workingMatrix.val[mmIndex]);
    }
    printf(ANSI_COLOR_BLUE "\ncol_ind:\n" ANSI_COLOR_RESET);
    for (mmIndex = 0; mmIndex < fInputNonZeros; mmIndex++)
    {
        printf(ANSI_COLOR_BLUE "%d  " ANSI_COLOR_RESET, workingMatrix.col_ind[mmIndex]);
    }
    printf("\n");

    // 2. Load the vector to be multiplied

    // 3*. Visual representation of loaded matrix and vector (*if time)

    // 4. DO THE THING (y=(A^n)*x using CSR)
    // a. setup logging of overall compute time
    // b. setup logging of per iteration compute time
    // c. compute the product

    // 5*. Visual representation of output product (*if time)

    // 6. Write compute and execution time results to file(s)
}
