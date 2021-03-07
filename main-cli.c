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
#include <sys/stat.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <popt.h>
#include "mmio/mmio.h"

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

// Function: popt_usage
// Displays syntax recommendations to the user when incorrrect arguments are passed to popt
void popt_usage(poptContext optCon, int exitcode, char *error, char *addl)
{
    poptPrintUsage(optCon, stderr, 0);
    if (error)
        fprintf(stderr, "%s: %s", error, addl);
    exit(exitcode);
}

int checkFolderExists(char *path)
{
    // Use POSIX stat() to retreive output folder properties
    struct stat folderStats;
    stat(path, &folderStats);

    if (S_ISDIR(folderStats.st_mode))
    {
        return 1;
    }

    return 0;
}

int main(int argc, const char *argv[])
{

    const char *inputFileName;
    char *reportPath;
    char c;
    FILE *mmInputFile;
    MM_typecode matcode;
    poptContext optCon;
    int mmio_rb_return, mmio_rs_return, index, alg_mode, calc_iter;
    int fInputRows, fInputCols, fInputNonZeros;
    int *iteration_time;
    double *output_vector;

    // Ust POPT library to handle command line arguments robustly
    // POPT library and documentation available at https://github.com/devzero2000/POPT
    struct _popt_field
    {
        int iter;
        char *outputFolder;

    } popt_field;

    struct poptOption optionsTable[] = {
        {"all-algs", 'a', POPT_ARG_NONE, NULL, 'a', "Enable all SMVP algorithms.", NULL},
        {"csr", 'c', POPT_ARG_NONE, NULL, 'c', "Enable CSR SMVP algorithm.", NULL},
        {"tjds", 't', POPT_ARG_NONE, NULL, 't', "Enable TJDS SMVP algorithm.", NULL},
        {"number", 'n', POPT_ARG_INT, &popt_field.iter, 'n', "Number of computation iterations per-algorithm.", "1000"},
        {"output", 'o', POPT_ARG_STRING, &popt_field.outputFolder, 'o', "Output folder for reports.", "./"},
        POPT_AUTOHELP
            POPT_TABLEEND};

    optCon = poptGetContext(NULL, argc, argv, optionsTable, POPT_CONTEXT_NO_EXEC | POPT_CONTEXT_POSIXMEHARDER);
    poptSetOtherOptionHelp(optCon, "[OPTIONS] <file>");

    // Start with no algorithms selected, enable specific algs as options are processed.
    alg_mode = ALG_NONE;

    // Define default algorithm iteration count
    calc_iter = 1000;

    // Display usage if no arguments are specified
    if (argc < 2)
    {
        poptPrintUsage(optCon, stderr, 0);
        exit(1);
    }

    // Parse non-mandatory options
    while ((c = poptGetNextOpt(optCon)) >= 0)
    {
        switch (c)
        {
        case 'a':
            if (alg_mode != ALG_NONE)
            {
                printf(ANSI_COLOR_RED "[ERROR]\tCombining [-a|--all] with other algorithm flags is not supported.\n" ANSI_COLOR_RESET);
                exit(1);
            }
            else
            {
                alg_mode = ALG_ALL;
                break;
            }
        case 'c':
            if (alg_mode == ALG_ALL)
            {
                printf(ANSI_COLOR_RED "[ERROR]\tCombining [-a|--all] with other algorithm flags is not supported.\n" ANSI_COLOR_RESET);
                exit(1);
            }
            else
            {
                alg_mode += ALG_CSR;
                break;
            }
        case 't':
            if (alg_mode == ALG_ALL)
            {
                printf(ANSI_COLOR_RED "[ERROR]\tCombining [-a|--all] with other algorithm flags is not supported.\n" ANSI_COLOR_RESET);
                exit(1);
            }
            else
            {
                alg_mode += ALG_TJDS;
                break;
            }
        case 'n':
            if (popt_field.iter >= 1)
            {
                calc_iter = popt_field.iter;
            }
            else
            {
                printf(ANSI_COLOR_RED "[ERROR]\tInvalid number of algorithm iterations specified.\n" ANSI_COLOR_RESET);
                exit(1);
            }
            break;
        case 'o':
            // Determine folder existance and act accordingly
            if (checkFolderExists(popt_field.outputFolder))
            {
                reportPath = popt_field.outputFolder;
                break;
            }
            else
            {
                printf(ANSI_COLOR_RED "[ERROR]\tReport output folder not found. Check path and/or create folder if it does not exist.\n" ANSI_COLOR_RESET);
                exit(1);
            }
        default:
            poptPrintUsage(optCon, stderr, 0);
            exit(1);
        }
    }

    // Handle popt option exceptions where appropriate
    if (c < -1)
    {
        switch (c)
        {
        case POPT_ERROR_NOARG:
            printf(ANSI_COLOR_RED "[ERROR]\tOne or more options missing a required argument.\n" ANSI_COLOR_RESET);
            exit(1);
        case POPT_ERROR_NULLARG:
            printf(ANSI_COLOR_RED "[ERROR]\tOne or more options missing a required argument.\n" ANSI_COLOR_RESET);
            exit(1);
        case POPT_ERROR_BADQUOTE:
            printf(ANSI_COLOR_RED "[ERROR]\tQuotation mismatch in path. Ensure path is enclosed by only one pair of quotation marks.\n" ANSI_COLOR_RESET);
            exit(1);
        case POPT_ERROR_BADNUMBER:
            printf(ANSI_COLOR_RED "[ERROR]\tArgument for iteration count contains non-number characters.\n" ANSI_COLOR_RESET);
            exit(1);
        case POPT_ERROR_OVERFLOW:
            printf(ANSI_COLOR_RED "[ERROR]\tArgument for iteration count must be between 0 and approximately 1.8E19 (64-bit integer).\n" ANSI_COLOR_RESET);
            exit(1);
        default:
            fprintf(stderr, "%s: %s\n", poptBadOption(optCon, POPT_BADOPTION_NOALIAS), poptStrerror(c));
            exit(1);
        }
    }

    // Parse mandatory arguments
    inputFileName = poptGetArg(optCon);
    if ((inputFileName == NULL) || !(poptPeekArg(optCon) == NULL))
    {
        popt_usage(optCon, 1, ANSI_COLOR_RED "[ERROR]\tMust specify a single input file", "ex., /path/to/file.mtx\n" ANSI_COLOR_RESET);
    }
    else if ((mmInputFile = fopen(inputFileName, "r")) == NULL)
    {
        printf(ANSI_COLOR_RED "[ERROR]\tSpecified input file not found.\n" ANSI_COLOR_RESET);
        exit(1);
    }

    // Done parsing options/args, release popt context instance
    poptFreeContext(optCon);
    printf(ANSI_COLOR_GREEN "\n[START]\tExecuting smvp-toolbox-cli v%d.%d.%d\n" ANSI_COLOR_RESET, MAJOR_VER, MINOR_VER, REVISION_VER);

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
