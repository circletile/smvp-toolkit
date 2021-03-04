/* 
*  ==================================================================
*  main-cli.c v0.4 for smvp-toolbox
*  ==================================================================
*/

#define MAJOR_VER 0
#define MINOR_VER 4
#define REVISION_VER 0
#define SMVP_CSR_DEBUG 0

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <popt.h>
#include "mmio/mmio.h"
#include "smvp-algs/smvp-algs.h"

// ANSI terminal color escape codes for making output BEAUTIFUL
#define ANSI_COLOR_RED "\x1b[31m"
#define ANSI_COLOR_GREEN "\x1b[32m"
#define ANSI_COLOR_YELLOW "\x1b[33m"
#define ANSI_COLOR_BLUE "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN "\x1b[36m"
#define ANSI_COLOR_RESET "\x1b[0m"

#define ALG_ALL 255
#define ALG_NONE 0
#define ALG_CSR 1
#define ALG_TJDS 2

// Struct: _mm_raw_data_
// Provides a convenient structure for importing/exporting Matrix Market file contents
typedef struct _mm_raw_data_
{
    int row;
    int col;
    double val;
} MMRawData;

// Function: mmioErrorHandler
// Provides a simple error handler for known error types (mmio.h)
void mmioErrorHandler(int retcode)
{
    if (retcode == MM_PREMATURE_EOF)
    {
        printf(ANSI_COLOR_RED "[ERROR]\tCould not process specified Matrix Market input file. Required parameters not present on first line of file.\n" ANSI_COLOR_RESET);
        exit(1);
    }
    else if (retcode == MM_NO_HEADER)
    {
        printf(ANSI_COLOR_RED "[ERROR]\tCould not process specified Matrix Market input file. Required header is missing or file contents may not be Matrix Market formatted.\n" ANSI_COLOR_RESET);
        exit(1);
    }
    else if (retcode == MM_UNSUPPORTED_TYPE)
    {
        printf(ANSI_COLOR_RED "[ERROR]\tCould not process specified Matrix Market input file. Matrix content description not parseable or is absent.\n" ANSI_COLOR_RESET);
        exit(1);
    }
    else
    {
        printf(ANSI_COLOR_RED "[ERROR]\tCould not process specified Matrix Market input file. Unhandled exception occured during file loading .\n" ANSI_COLOR_RESET);
        exit(1);
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

/*
// Function: generateReportFile
// Generates a report file from calculation results
void generateReportFile()
{

    char *outputFileName;
    unsigned long outputFileTime;
    FILE *reportOutputFile;

    //  Write compute time results and output vector to file
    outputFileName = (char *)malloc(sizeof(char) * 50); // Length is arbitrary, adjust as needed
    outputFileTime = ((unsigned long)time(NULL));
    snprintf(outputFileName, 50, "smvp-csr_output_%lu.txt", outputFileTime);

    printf(ANSI_COLOR_MAGENTA "[FILE]\tCompute time and output vector saved as:\n" ANSI_COLOR_RESET);
    printf("\t%s\n", outputFileName);

    reportOutputFile = fopen(outputFileName, "a+");
    fprintf(reportOutputFile, "Execution results for smvp-csr v.%d.%d.%d\n", MAJOR_VER, MINOR_VER, REVISION_VER);
    fprintf(reportOutputFile, "Generated on %lu (Unix time)\n\n", outputFileTime);
    fprintf(reportOutputFile, "Sparse matrix file in use:\n%s\n\n", inputFileName);
    fprintf(reportOutputFile, "Non-zero numbers contained in matrix:\n");
    fprintf(reportOutputFile, "%d\n\n", fInputNonZeros);
    fprintf(reportOutputFile, "Compute time for 1000 iterations:\n");
    fprintf(reportOutputFile, "%g seconds\n", comp_time_taken);
    fprintf(reportOutputFile, "\nOutput vector (one cell per line):\n");
    fprintf(reportOutputFile, "[\n");
    for (index = 0; index < fInputRows; index++)
    {
        fprintf(reportOutputFile, "%g", outputVector[index]);
        if (index < fInputRows - 1)
        {
            fprintf(reportOutputFile, "\n");
        }
        else
        {
            fprintf(reportOutputFile, "\n]\n\n");
        }
    }

    fclose(reportOutputFile);
}
*/

int main(int argc, const char *argv[])
{

    char *inputFileName;
    char c;
    FILE *mmInputFile;
    MM_typecode matcode;
    int mmio_rb_return, mmio_rs_return, index, alg_mode, calc_iter;
    int fInputRows, fInputCols, fInputNonZeros;
    int *iteration_time;
    double *output_vector;

    printf(ANSI_COLOR_GREEN "\n[START]\tExecuting smvp-toolbox-cli v%d.%d.%d\n" ANSI_COLOR_RESET, MAJOR_VER, MINOR_VER, REVISION_VER);

    // Ust POPT library to handle command line arguments robustly
    // POPT library and documentation available at https://github.com/devzero2000/POPT
    struct _popt_field
    {
        int iter;
        char *file;
        /* etc */
    } popt_field;

    struct poptOption optionsTable[] = {
        {"all", 'a', POPT_ARG_NONE, NULL, 'a', "Enable all SMVP algorithms."},
        {"csr", 'c', POPT_ARG_NONE, NULL, 'c', "Enable CSR SMVP algorithm."},
        {"tjds", 't', POPT_ARG_NONE, NULL, 't', "Enable TJDS SMVP algorithm."},
        {"iter", 'i', POPT_ARG_INT, &popt_field.iter, 'i', "Specify computation iterations."},
        {"file", 'f', POPT_ARG_STRING, &popt_field.file, 'f', "Specify path to input matrix file."},
        POPT_AUTOHELP
            POPT_TABLEEND};

    poptContext optCon = poptGetContext(NULL, argc, argv, optionsTable, 0);
    poptSetOtherOptionHelp(optCon, "[OPTIONS]");

    // Start with no algorithms selected, enable specific algs as oprions are processed.
    alg_mode = ALG_NONE;

    while ((c = poptGetNextOpt(optCon)) >= 0)
    {
        switch (c)
        {
        case 'a':
            alg_mode = ALG_ALL;
            break;
        case 'c':
            alg_mode += ALG_CSR;
            break;
        case 't':
            alg_mode += ALG_TJDS;
            break;
        case 'i':
            calc_iter = popt_field.iter;
            break;
        case 'f':
            if ((mmInputFile = fopen(popt_field.file, "r")) == NULL)
            {
                printf(ANSI_COLOR_RED "[ERROR]\tSpecified input file not found." ANSI_COLOR_RESET);
                exit(1);
            }
            else
            {
                inputFileName = popt_field.file;
            }
            break;
        default:
            poptPrintUsage(optCon, stderr, 0);
            exit(1);
        }
    }

    // Ensure input file is of proper format and contains an appropriate matrix type
    mmio_rb_return = mm_read_banner(mmInputFile, &matcode);
    if (mmio_rb_return != 0)
    {
        mmioErrorHandler(mmio_rb_return);
    }
    else if (mm_is_sparse(matcode) == 0)
    {
        printf(ANSI_COLOR_RED "[ERROR]\tThis application only supports sparse matricies. Specified input file does not appear to contain a sparse matrix.\n" ANSI_COLOR_RESET);
        exit(1);
    }

    // Load sparse matrix properties from input file
    printf(ANSI_COLOR_MAGENTA "[FILE]\tInput matrix file name:\n" ANSI_COLOR_RESET);
    printf("\t%s\n", inputFileName);
    printf(ANSI_COLOR_YELLOW "[INFO]\tLoading matrix content from source file.\n" ANSI_COLOR_RESET);
    mmio_rs_return = mm_read_mtx_crd_size(mmInputFile, &fInputRows, &fInputCols, &fInputNonZeros);
    if (mmio_rs_return != 0)
    {
        mmioErrorHandler(mmio_rs_return);
    }

    // Stage matrix content from the input file into working memory
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

    // Sort imported data now to simplify future data processing
    qsort(mmImportData, (size_t)fInputNonZeros, sizeof(MMRawData), mmrd_comparator);

    // Close input file only if it isn't somehow mapped as keyboard input
    if (mmInputFile != stdin)
    {
        fclose(mmInputFile);
    }

    printf(ANSI_COLOR_CYAN "[DATA]\tNon-zero numbers contained in matrix:\n" ANSI_COLOR_RESET);
    printf("\t%d\n", fInputNonZeros);

    // Convert loaded data to CSR format
    printf(ANSI_COLOR_YELLOW "[INFO]\tConverting loaded content to CSR format.\n" ANSI_COLOR_RESET);
    printf(ANSI_COLOR_CYAN "[DATA]\tVector operand in use:\n" ANSI_COLOR_RESET);
    printf("\tOnes vector with dimensions [%d, %d]\n", fInputRows, 1);

    printf(ANSI_COLOR_YELLOW "[INFO]\tComputing n=1000 sparse-vector multiplication iterations.\n" ANSI_COLOR_RESET);

    if (alg_mode == ALG_CSR)
    {
        //DO CSR
    }
    else if (alg_mode == ALG_TJDS)
    {
        //DO TJDS
    }
    else if (alg_mode == ALG_ALL)
    {
        //DO ALL
    }

    /*
    printf(ANSI_COLOR_CYAN "[DATA]\tVector product computation time:\n" ANSI_COLOR_RESET);
    printf("\t%g seconds\n", comp_time_taken);
    */

    printf(ANSI_COLOR_GREEN "[STOP]\tExit smvp-csr v%d.%d.%d\n\n" ANSI_COLOR_RESET, MAJOR_VER, MINOR_VER, REVISION_VER);

    return 0;
}
