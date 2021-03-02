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

int main(int argc, char *argv[])
{

    FILE *mmInputFile, *reportOutputFile;
    char *outputFileName, *inputFileName;
    MM_typecode matcode;
    int mmio_rb_return, mmio_rs_return, index;
    int fInputRows, fInputCols, fInputNonZeros;
    unsigned long outputFileTime;


    printf(ANSI_COLOR_GREEN "\n[START]\tExecuting smvp-toolbox-cli v%d.%d.%d\n" ANSI_COLOR_RESET, MAJOR_VER, MINOR_VER, REVISION_VER);

    // Validate user provided arguments, handle exceptions accordingly
    if (argc < 2)
    {
        fprintf(stderr, ANSI_COLOR_YELLOW "[HELP]\tUsage: %s [martix-market-filename]\n" ANSI_COLOR_RESET, argv[0]);
        exit(1);
    }
    else
    {
        if ((mmInputFile = fopen(argv[1], "r")) == NULL)
        {
            exit(1);
        }
        else
        {
            inputFileName = argv[1];
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

    printf(ANSI_COLOR_CYAN "[DATA]\tNon-zero numbers contained in matrix:\n" ANSI_COLOR_RESET);
    printf("\t%d\n", fInputNonZeros);

    // Convert loaded data to CSR format
    printf(ANSI_COLOR_YELLOW "[INFO]\tConverting loaded content to CSR format.\n" ANSI_COLOR_RESET);
    printf(ANSI_COLOR_CYAN "[DATA]\tVector operand in use:\n" ANSI_COLOR_RESET);
    printf("\tOnes vector with dimensions [%d, %d]\n", fInputRows, 1);

    printf(ANSI_COLOR_YELLOW "[INFO]\tComputing n=1000 sparse-vector multiplication iterations.\n" ANSI_COLOR_RESET);

//##############################
// CSR WAS HERE
//##############################

    printf(ANSI_COLOR_CYAN "[DATA]\tVector product computation time:\n" ANSI_COLOR_RESET);
    printf("\t%g seconds\n", comp_time_taken);

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

    printf(ANSI_COLOR_GREEN "[STOP]\tExit smvp-csr v%d.%d.%d\n\n" ANSI_COLOR_RESET, MAJOR_VER, MINOR_VER, REVISION_VER);

    return 0;

}
