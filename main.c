/* 
*  ==================================================================
*  main.c v0.1 for smvp-csr
*  ==================================================================
*/

#include <stdio.h>
#include <stdlib.h>
#include "mmio.h"

// ANSI terminal color escape codes for making printf BEAUTIFUL
#define ANSI_COLOR_RED "\x1b[31m"
#define ANSI_COLOR_GREEN "\x1b[32m"
#define ANSI_COLOR_YELLOW "\x1b[33m"
#define ANSI_COLOR_BLUE "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN "\x1b[36m"
#define ANSI_COLOR_RESET "\x1b[0m"

int main(int argc, char *argv[])
{

    FILE *mmInputFile, *mmOutputFile;
    MM_typecode matcode;
    int mmio_return;

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

    // Ensure file is of proper format and content
    mmio_return = mm_read_banner(mmInputFile, &matcode);
    if (mmio_return != 0)
    {
        if (mmio_return == MM_PREMATURE_EOF)
        {
            printf(ANSI_COLOR_RED "Could not process specified Matrix Market input file.\nRequired items not present on first line of file.\n" ANSI_COLOR_RESET);
            exit(1);
        }
        else if (mmio_return == MM_NO_HEADER)
        {
            printf(ANSI_COLOR_RED "Could not process specified Matrix Market input file.\nRequired header is missing or file contents may not be Matrix Market formatted.\n" ANSI_COLOR_RESET);
            exit(1);
        }
        else if (mmio_return == MM_UNSUPPORTED_TYPE)
        {
            printf(ANSI_COLOR_RED "Could not process specified Matrix Market input file.\nMatrix content description not parseable or is absent.\n" ANSI_COLOR_RESET);
            exit(1);
        }
        else{
            printf(ANSI_COLOR_RED "Could not process specified Matrix Market input file.\nUnhandled exception occured during file loading .\n" ANSI_COLOR_RESET);
            exit(1);
        }
    }
    else
    {
        if (mm_is_sparse(matcode) == 0)
        {
            printf(ANSI_COLOR_RED "This application only supports sparse matricies.\nSpecified input file does not appear to contain a sparse matrix.\n" ANSI_COLOR_RESET);
            exit(1);
        }
    }

    // 1. Load the sparse matrix file

    // 2. Load the vector to be multiplied

    // 3*. Visual representation of loaded matrix and vector (*if time)

    // 4. DO THE THING (y=(A^n)*x using CSR)
    // a. setup logging of overall compute time
    // b. setup logging of per iteration compute time
    // c. compute the product

    // 5*. Visual representation of output product (*if time)

    // 6. Write compute and execution time results to file(s)
}
