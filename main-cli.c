/* 
*  ==================================================================
*  main-cli.c v0.6.4 for smvp-toolbox
*  ==================================================================
*/

#define MAJOR_VER 0
#define MINOR_VER 6
#define REVISION_VER 4
#define SMVP_CSR_DEBUG 1
#define SMVP_TJDS_DEBUG 0

#include <math.h>
#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <stdint.h>
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

#define ALG_ALL 256
#define ALG_NONE 0
#define ALG_CSR (1 << 1)
#define ALG_TJDS (1 << 2)
#define ALG_CISR (1 << 3)

// Struct: _mm_raw_data_
// Provides a convenient structure for importing/exporting Matrix Market file contents
typedef struct _mm_raw_data_
{
    int row;
    int col;
    double val;
} MMRawData;

// Struct: _mm_data_plus_
// Provides a sligktly larger structure than _mm_raw_data_for retaining row data for TJDS
typedef struct _mm_data_plus_
{
    int row;
    int row_orig;
    int col;
    double val;
} MMDataPlus;

// Struct: _csr_data_
// Provides a convenient structure for storing/manipulating CSR compressed data
typedef struct _csr_data_
{
    int *row_ptr;
    int *col_ind;
    double *val;
} CSRData;

// Struct: _tjds_data_
// Provides a convenient structure for storing/manipulating TJDS compressed data
typedef struct _tjds_data_
{
    double *val;
    int *row_ind;
    int *start_pos;
} TJDSData;

// Struct: _transpose_table_
// Provides a convenient structure for sorting TJDS compressed data
typedef struct _transpose_table_
{
    int originCol;
    int colLength;
} TXTable;

// Struct: _results_data_
// Provides a convenient structure for storing/manipulating algorithm run results
struct _time_data_
{
    double time_total;
    double time_avg;
    double time_stdev;
    double time_min;
    double time_max;
    double time_each[];
};

// Function: newResultsData
// Initializes and returns a _time_data_ struct
struct _time_data_ *newResultsData(struct _time_data_ *t, int num_runs)
{
    t = (struct _time_data_ *)malloc(sizeof(*t) + (sizeof(double) * num_runs));

    t->time_total = 0;
    t->time_avg = 0;
    t->time_stdev = 0;
    t->time_min = 0;
    t->time_max = 0;

    return t;
}

// Function: calcStDevDouble
// Calculates standard deviation for an array of doubles
double calcStDevDouble(double data[], int len)
{
    int i;
    double sum, mean, stdev;

    for (i = 0; i < len; ++i)
    {
        sum += data[i];
    }
    mean = sum / len;
    for (i = 0; i < len; ++i)
    {
        stdev += pow(data[i] - mean, 2);
    }

    return sqrt(stdev / len);
}

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

// Function: mmrd_comparator_row_col
// Provides a comparitor function for MMRawData structs that matches the format expected by stdlib qsort()
// Sorts data by row (lowest = leftmost), then by column (lowest = leftmost)
int mmrd_comparator_row_col(const void *v1, const void *v2)
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

// Function: mmrd_comparator_col_row
// Provides a comparitor function for MMRawData structs that matches the format expected by stdlib qsort()
// Sorts data by column (lowest = leftmost), then by row (lowest = leftmost)
int mmrd_comparator_col_row(const void *v1, const void *v2)
{
    const MMRawData *p1 = (MMRawData *)v1;
    const MMRawData *p2 = (MMRawData *)v2;
    if (p1->col < p2->col)
        return -1;
    else if (p1->col > p2->col)
        return +1;
    else if (p1->row < p2->row)
        return -1;
    else if (p1->row > p2->row)
        return +1;
    else
        return 0;
}

// Function: txtable_comparator_len
// Provides a comparitor function for TXTable structs that matches the format expected by stdlib qsort()
// Sorts data by colLength (highest = leftmost), then by originCol (lowest = leftmost)
int txtable_comparator_len(const void *v1, const void *v2)
{
    const TXTable *p1 = (TXTable *)v1;
    const TXTable *p2 = (TXTable *)v2;
    if (p1->colLength > p2->colLength)
        return -1;
    else if (p1->colLength < p2->colLength)
        return +1;
    else if (p1->originCol < p2->originCol)
        return -1;
    else if (p1->originCol > p2->originCol)
        return +1;
    else
        return 0;
}

// Function: mmdp_comparator_row_col
// Provides a comparitor function for MMDataPlus structs that matches the format expected by stdlib qsort()
// Sorts data by row (lowest = leftmost), then by column (lowest = leftmost)
int mmdp_comparator_row_col(const void *v1, const void *v2)
{
    const MMDataPlus *p1 = (MMDataPlus *)v1;
    const MMDataPlus *p2 = (MMDataPlus *)v2;
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

// Function: generateReportText
// Generates a report file from calculation results
void generateReportText(const char *inputFileName, char *reportPath, int alg_mode, int fInputNonZeros, int fInputRows, int iter, double *outputVector, struct _time_data_ *timeData)
{

    int index, pathLen, filenameLen;
    char *alg_name, *outputFileName, *outputFullPath, *dirDelimiter;
    unsigned long outputFileTime;
    FILE *reportOutputFile;

    if (alg_mode & ALG_CSR)
    {
        alg_name = "CSR";
    }
    else if (alg_mode & ALG_TJDS)
    {
        alg_name = "TJDS";
    }

    dirDelimiter = "/";

    // Dynamically generate report file name
    filenameLen = 100;
    outputFileName = (char *)malloc(sizeof(char) * filenameLen); // Length is arbitrary, adjust as needed
    outputFileTime = ((unsigned long)time(NULL));
    snprintf(outputFileName, filenameLen, "smvp-toolbox_report_%s_%lu.txt", alg_name, outputFileTime);

    // Concatenate report folder path and filename if required
    if (reportPath && reportPath[0] == '\0')
    {
        outputFullPath = (char *)malloc(sizeof(char) * filenameLen);
        snprintf(outputFullPath, filenameLen, "%s", outputFileName);
    }
    else
    {
        pathLen = strlen(reportPath) + filenameLen;
        outputFullPath = (char *)malloc(sizeof(char) * pathLen);
        snprintf(outputFullPath, pathLen, "%s", reportPath);
        if (reportPath[strlen(reportPath) - 1] != '/')
        {
            strncat(outputFullPath, dirDelimiter, pathLen);
        }
        strncat(outputFullPath, outputFileName, pathLen);
    }

    //  Write compute time results and output vector to file
    printf(ANSI_COLOR_MAGENTA "[FILE]\tExecution report file saved as:\n" ANSI_COLOR_RESET);
    printf("\t%s\n", outputFullPath);

    reportOutputFile = fopen(outputFullPath, "a+");
    fprintf(reportOutputFile, "Execution results for smvp-toolbox v.%d.%d.%d, %s algorithm\n", MAJOR_VER, MINOR_VER, REVISION_VER, alg_name);
    fprintf(reportOutputFile, "Generated on %lu (Unix time)\n\n", outputFileTime);
    fprintf(reportOutputFile, "Sparse matrix file in use:\n%s\n\n", inputFileName);
    fprintf(reportOutputFile, "Non-zero numbers contained in matrix: %d\n\n", fInputNonZeros);
    fprintf(reportOutputFile, "Compute times for %d iterations:\n\n", iter);
    fprintf(reportOutputFile, "Total Time: %g ms\n", timeData->time_total);
    fprintf(reportOutputFile, "Average Time: %g ms\n", timeData->time_avg);
    fprintf(reportOutputFile, "Fastest Time: %g ms\n", timeData->time_min);
    fprintf(reportOutputFile, "Slowest Time: %g ms\n", timeData->time_max);
    fprintf(reportOutputFile, "Time StDev: %g ms\n\n", timeData->time_stdev);
    fprintf(reportOutputFile, "Output vector (one cell per line):\n");
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

// Function: smvp_csr_compute
// Calculates SMVP using CSR algorithm
// Returns results vector directly, time data via pointer
double *smvp_csr_compute(MMRawData *mmImportData, int fInputRows, int fInputNonZeros, int compiter, struct _time_data_ *csr_time)
{

    CSRData workingMatrix;
    double *onesVector, *outputVector;
    double comp_time_taken;
    int i, j, index;
    double *time_run = (double *)malloc(compiter * sizeof(double));
    struct timespec *time_run_start = (struct timespec *)malloc(sizeof(struct timespec) * compiter);
    struct timespec *time_run_end = (struct timespec *)malloc(sizeof(struct timespec) * compiter);

    // Convert loaded data to CSR format
    printf(ANSI_COLOR_YELLOW "[INFO]\tConverting loaded content to CSR format.\n" ANSI_COLOR_RESET);

    // Sort imported data now to simplify future data processing
    qsort(mmImportData, (size_t)fInputNonZeros, sizeof(MMRawData), mmrd_comparator_row_col);

    // Allocate memory for CSR storage
    workingMatrix.row_ptr = (int *)malloc(sizeof(int) * (long unsigned int)(fInputRows + 1));
    workingMatrix.col_ind = (int *)malloc(sizeof(int) * (long unsigned int)fInputNonZeros);
    workingMatrix.val = (double *)malloc(sizeof(double) * (long unsigned int)fInputNonZeros);

    // Convert MatrixMarket format into CSR format
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

    // Prepare the "ones" vector and output vector
    onesVector = (double *)malloc(sizeof(double) * (long unsigned int)fInputRows);
    vectorInit(fInputRows, onesVector, 1);
    outputVector = (double *)malloc(sizeof(double) * (long unsigned int)fInputRows);

    printf(ANSI_COLOR_YELLOW "[INFO]\tCalculating %d iterations of SMVP CSR.\n" ANSI_COLOR_RESET, compiter);

    if (SMVP_CSR_DEBUG)
    {
        printf("[DEBUG]\tCSR JIT row_ptr:\n\t[");
        for (i = 0; i < fInputRows + 1; i++)
        {
            printf("%d, ", workingMatrix.row_ptr[i]);
        }
        printf("]\n");
        printf("[DEBUG]\tCSR JIT val:\n\t[");
        for (i = 0; i < fInputNonZeros; i++)
        {
            printf("%g, ", workingMatrix.val[i]);
        }
        printf("]\n");
        printf("[DEBUG]\tCSR JIT col_ind:\n\t[");
        for (i = 0; i < fInputNonZeros; i++)
        {
            printf("%d, ", workingMatrix.col_ind[i]);
        }
        printf("]\n\n");
    }

    //
    // ATOMIC SECTION START
    // PERFORM NO ACTIONS OTHER THAN SMVP BETWEEN START AND END TIME CAPTURES
    //

    // Compute CSR SMVP (technically y=Axn, not y=x(A^n) as indicated in reqs doc, but is an approved deviation)
    for (i = 0; i < compiter; i++)
    {
        //Reset output vector contents between iterations
        vectorInit(fInputRows, outputVector, 0);

        // Capture compute run start time
        clock_gettime(CLOCK_MONOTONIC_RAW, &time_run_start[i]);

        for (index = 0; index < fInputRows; index++)
        {
            for (j = workingMatrix.row_ptr[index]; j < workingMatrix.row_ptr[index + 1]; j++)
            {
                outputVector[index] += workingMatrix.val[j] * onesVector[workingMatrix.col_ind[j]];
            }
        }

        // Capture compute run end time
        clock_gettime(CLOCK_MONOTONIC_RAW, &time_run_end[i]);
    }

    //
    // ATOMIC SECTION END
    // PERFORM NO ACTIONS OTHER THAN SMVP BETWEEN START AND END TIME CAPTURES
    //

    // Convert all per-run timespec structs to time in milliseconds & populate time structure
    for (i = 0; i < compiter; i++)
    {
        // Calculate runtime in ns, then convert to ms
        time_run[i] = (double)((time_run_end[i].tv_sec * 1e9 + time_run_end[i].tv_nsec) - (time_run_start[i].tv_sec * 1e9 + time_run_start[i].tv_nsec));
        time_run[i] /= 1e6;

        csr_time->time_each[i] = time_run[i];
        csr_time->time_total += time_run[i];
        csr_time->time_avg += time_run[i];

        if (i == 0)
        {
            csr_time->time_min = time_run[i];
            csr_time->time_max = time_run[i];
        }
        else
        {
            if (csr_time->time_min > time_run[i])
            {
                csr_time->time_min = time_run[i];
            }
            if (csr_time->time_max < time_run[i])
            {
                csr_time->time_max = time_run[i];
            }
        }
    }
    csr_time->time_avg /= compiter;
    csr_time->time_stdev = calcStDevDouble(csr_time->time_each, compiter);

    if (SMVP_CSR_DEBUG)
    {
        printf("[DEBUG]\tCSR JIT Vector Out:\n\t[");
        for (i = 0; i < fInputRows; i++)
        {
            printf("%g, ", outputVector[i]);
        }
        printf("]\n\n");
    }

    return outputVector;
}

// Function: smvp_cisr_coegen
// Generates CISR COE data file
void smvp_cisr_coegen(MMRawData *mmImportData, int fInputRows, int fInputNonZeros, int slotCount) //, const char *inputFileName, char *reportPath)
{

    typedef struct _cisr_value_data_
    {
        double val;
        int col_ind;
        int slot;
    } CISRValData;

    CSRData workingMatrix;
    int i, j, index;

    int *cisr_rowLengths = (int *)malloc(sizeof(int) * (long unsigned int)(fInputRows + 1));

    // Convert loaded data to CSR format
    printf(ANSI_COLOR_YELLOW "[INFO]\tConverting loaded content to CISR format.\n" ANSI_COLOR_RESET);

    // Sort imported data now to simplify future data processing
    qsort(mmImportData, (size_t)fInputNonZeros, sizeof(MMRawData), mmrd_comparator_row_col);

    // Allocate memory for CSR storage
    workingMatrix.row_ptr = (int *)malloc(sizeof(int) * (long unsigned int)(fInputRows + 1));
    workingMatrix.col_ind = (int *)malloc(sizeof(int) * (long unsigned int)fInputNonZeros);
    workingMatrix.val = (double *)malloc(sizeof(double) * (long unsigned int)fInputNonZeros);

    // Allocate memory for CISR storage
    // cisr_valData.val = (double *)malloc(sizeof(int) * (long unsigned int)(fInputNonZeros));
    // cisr_valData.col_ind = (int *)malloc(sizeof(int) * (long unsigned int)fInputNonZeros);
    // cisr_valData.slot = (int *)malloc(sizeof(int) * (long unsigned int)fInputNonZeros);

    // Convert MatrixMarket format into CSR format
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

    //
    // Convert CSR format into CISR format
    //

    int(*slotgrp)[slotCount];

    int slot_rowend[slotCount];
    for (int clean_0 = 0; clean_0 < slotCount; clean_0++)
    {
        slot_rowend[clean_0] = 0;
    }

    slotgrp = malloc(sizeof(*slotgrp) * fInputNonZeros + 1); //absolute worst case size

    int slot_grp_iter = 0;
    int csr_rowptr_iter = 0;
    int csr_eof = 0;

    printf("\n[DEBUG]\tslot count: %d\n", slotCount);
    printf("[DEBUG]\tfInputRows: %d\n", fInputRows);

    // For each slot group...
    // while ( (csr_rowptr_iter < fInputRows) && (csr_eof == 0) ) // continue until the EOF nzn index is reached, but DON'T increment it up here
    while (csr_eof == 0) // continue until the EOF nzn index is reached, but DON'T increment it up here
    {

        //... initially, pick the first nzn index of each row until all slots are filled.
        if (slot_grp_iter == 0)
        {
            for (int slot_num_iter = 0; slot_num_iter < slotCount; slot_num_iter++)
            {
                // First, make sure there are more new rows available to choose from
                if (csr_rowptr_iter < fInputRows)
                {
                    slotgrp[slot_grp_iter][slot_num_iter] = workingMatrix.row_ptr[csr_rowptr_iter];
                    slot_rowend[slot_num_iter] = workingMatrix.row_ptr[csr_rowptr_iter + 1];
                    // printf("[INFO]\tcsr_rowptr_iter++ on 558\n");
                    csr_rowptr_iter++;
                    // printf("[INFO]\tcsr_rowptr_iter: %d\n\n", csr_rowptr_iter);
                }
                else
                {
                    // If there aren't any more rows available, assign an invalid index (overflowing seems safer than NULL or negatives)
                    slotgrp[slot_grp_iter][slot_num_iter] = workingMatrix.row_ptr[fInputRows] + 1;
                }
            }
        }
        else
        {
            // Continue adding the next available nzn for each row
            for (int slot_num_iter = 0; slot_num_iter < slotCount; slot_num_iter++)
            {
                // ... unless the current slot runs out of row values to retreive (runs into the next row ptr)
                if ((slotgrp[slot_grp_iter - 1][slot_num_iter]) >= slot_rowend[slot_num_iter] - 1)
                {
                    // First, make sure there are more new rows available to choose from
                    if (csr_rowptr_iter >= fInputRows)
                    {
                        // If there aren't any more rows available, assign an invalid index (overflowing seems safer than NULL or negatives)
                        slotgrp[slot_grp_iter][slot_num_iter] = workingMatrix.row_ptr[fInputRows] + 1;
                    }
                    else
                    {
                        // if more rows are available, pick up a new row for the current slot
                        slotgrp[slot_grp_iter][slot_num_iter] = workingMatrix.row_ptr[csr_rowptr_iter];
                        slot_rowend[slot_num_iter] = workingMatrix.row_ptr[csr_rowptr_iter + 1];
                        // printf("[DEBUG]\t\tslot %d: %d\n", slot_num_iter, slotgrp[slot_grp_iter][slot_num_iter]);
                        // printf("[DEBUG]\t\tcsr_rowptr_iter: %d\n", csr_rowptr_iter);
                        // printf("[INFO]\tcsr_rowptr_iter++ on 587\n");
                        csr_rowptr_iter++;
                        // printf("[INFO]\tcsr_rowptr_iter: %d\n\n", csr_rowptr_iter);
                    }
                }
                else
                {
                    // If the row still has values, just add the next available nzn
                    slotgrp[slot_grp_iter][slot_num_iter] = slotgrp[slot_grp_iter - 1][slot_num_iter] + 1;
                }
            }
        }

        // Make sure at least one of the stored row values is valid (EOF detection)
        csr_eof = 1;
        for (int wer = 0; wer < slotCount; wer++)
        {
            if (slotgrp[slot_grp_iter][wer] < fInputNonZeros)
                csr_eof = 0;
        }

        // Finally, move to the next slot group
        slot_grp_iter++;

        if (slot_grp_iter >= fInputNonZeros)
        {
            printf("\n[ERROR]\tslot_group_iter overran fInputNonZeros!\n");
            exit(EXIT_FAILURE);
        }
    }

    // save the total number of slot groups for later use
    int slot_grp_total = slot_grp_iter;

    // Readback of slotgrp 2d array
    printf("[DEBUG]\tslot group total: %d\n", slot_grp_total);
    for (int qaz = 0; qaz < slot_grp_total; qaz++)
    {
        printf("\n[DEBUG]\tslot group: %d\n", qaz);
        for (int xyz = 0; xyz < slotCount; xyz++)
        {
            printf("[DEBUG]\t\tslot %d: %d\n", xyz, slotgrp[qaz][xyz]);
        }
    }

    CISRValData cisr_valData[slot_grp_total * slotCount + 1];
    int cisrdata_iter_1 = 0;

    // After determining the slot group assignments, expand the associated values into a usable data structure
    for (int slotgrp_iter_1 = 0; slotgrp_iter_1 < slot_grp_total; slotgrp_iter_1++)
    {
        for (int slot_iter_1 = 0; slot_iter_1 < slotCount; slot_iter_1++)
        {
            printf("\n[DEBUG] slotgrp[slotgrp_iter_1][slot_iter_1] = %d\n", slotgrp[slotgrp_iter_1][slot_iter_1]);
            if (slotgrp[slotgrp_iter_1][slot_iter_1] >= fInputNonZeros)
            {
                printf("val[%d] = %g\n", cisrdata_iter_1, (double)4095);
                cisr_valData[cisrdata_iter_1].val = 4095;
                printf("col_ind[%d] = %g\n", cisrdata_iter_1, (double)4095);
                cisr_valData[cisrdata_iter_1].col_ind = 4095;
            }
            else
            {
                printf("val[%d] = %g\n", cisrdata_iter_1, workingMatrix.val[slotgrp[slotgrp_iter_1][slot_iter_1]]);
                cisr_valData[cisrdata_iter_1].val = workingMatrix.val[slotgrp[slotgrp_iter_1][slot_iter_1]];
                printf("col_ind[%d] = %g\n", cisrdata_iter_1, workingMatrix.col_ind[slotgrp[slotgrp_iter_1][slot_iter_1]]);
                cisr_valData[cisrdata_iter_1].col_ind = workingMatrix.col_ind[slotgrp[slotgrp_iter_1][slot_iter_1]];
            }
            cisr_valData[cisrdata_iter_1].slot = slot_iter_1;
            cisrdata_iter_1++;
        }
    }

    // Readback of cisr_valdata structure
    for (int cisrdata_iter_2 = 0; cisrdata_iter_2 < (slot_grp_total * slotCount); cisrdata_iter_2++)
    {
        printf("\nCISR Index: %d", cisrdata_iter_2);
        printf("\nval: %g, col_ind: %d, slot: %d\n", cisr_valData[cisrdata_iter_2].val, cisr_valData[cisrdata_iter_2].col_ind, cisr_valData[cisrdata_iter_2].slot);
    }

    // Pack cisr_valdata as structures as 36-bit memory structures
    //CISR Packed Memory Format

    // Hex	0x	C VVVV VVVV
    // C = Control Code		V = Payload
    //
    // 0	Start of Data		(0x1234 ABCD)
    // 1	Value				0xVVVI IINN
    // 					        	V = Value
    // 					        	I = Index
    // 					        	N = Slot Number
    // 2	Row Length			0xVAAA VBBB
    // 					        	V = Valid Data Here: 0x1 = valid, 0x0 = invalid/empty
    // 					        	A = Row length 1
    // 					        	B = Row Length 2
    // 3	End of Data			(0xDEAD BEEF)

    printf("\n*****************");
    printf("\n* CISR COE File *");
    printf("\n*****************\n");

    uint64_t result = 0;

    for (int cdv_iter = 0; cdv_iter < (slot_grp_total * slotCount); cdv_iter++)
    {
        // printf("\nval: %g, col_ind: %d, slot: %d\n", cisr_valData[cdv_iter].val, cisr_valData[cdv_iter].col_ind, cisr_valData[cdv_iter].slot);
        result = ((int)cisr_valData[cdv_iter].val << 20) | (cisr_valData[cdv_iter].col_ind << 8) | (cisr_valData[cdv_iter].slot << 0);
        printf("01%08x,\n", result);
    }
}

// Function: smvp_tjds_compute
// Calculates SMVP using TJDS algorithm
// Returns results vector directly, time data via pointer
double *smvp_tjds_compute(MMRawData *mmImportData, int fInputRows, int fInputColumns, int fInputNonZeros, int compiter, struct _time_data_ *tjds_time)
{

    TJDSData workingMatrix;
    MMDataPlus *mmCloneData;
    TXTable *txList = (struct _transpose_table_ *)malloc(sizeof(struct _transpose_table_) * fInputColumns);
    int index, txIter, i, num_tjdiag, k, p, j, sp_index;
    double *onesVector, *outputVector, *onesVectorTemp;
    double comp_time_taken;
    double *time_run = (double *)malloc(compiter * sizeof(double));
    struct timespec *time_run_start = (struct timespec *)malloc(sizeof(struct timespec) * compiter);
    struct timespec *time_run_end = (struct timespec *)malloc(sizeof(struct timespec) * compiter);

    if (SMVP_TJDS_DEBUG)
    {
        printf(ANSI_COLOR_RED "[DEBUG]\tIF YOU CAN READ THIS MESSAGE, TJDS IMPLEMENTATION MAY BE INCOMPLETE AND/OR BROKEN\n" ANSI_COLOR_RESET);
    }

    // Convert loaded data to TJDS format
    printf(ANSI_COLOR_YELLOW "[INFO]\tConverting loaded content to TJDS format.\n" ANSI_COLOR_RESET);

    // Allocate memory for TJDS storage
    workingMatrix.val = (double *)malloc(sizeof(double) * (long unsigned int)fInputNonZeros);
    workingMatrix.row_ind = (int *)malloc(sizeof(int) * (long unsigned int)fInputNonZeros);
    workingMatrix.start_pos = (int *)malloc(sizeof(int) * (long unsigned int)(fInputRows));

    // Prepare the "ones" vector and output vector
    onesVector = (double *)malloc(sizeof(double) * (long unsigned int)fInputRows);
    vectorInit(fInputRows, onesVector, 1);
    outputVector = (double *)malloc(sizeof(double) * (long unsigned int)fInputRows);

    // Sort imported data by columns to simplify future data processing
    qsort(mmImportData, (size_t)fInputNonZeros, sizeof(MMRawData), mmrd_comparator_col_row);

    /* Convert MatrixMarket format into TJDS format */

    if (SMVP_TJDS_DEBUG)
    {
        printf("[DEBUG]\tTJDS PHASE 0: Original Data (shown sideways):\n");
        printf("\t[");
        for (index = 0; index < fInputNonZeros; index++)
        {
            printf("%g (%d), ", mmImportData[index].val, mmImportData[index].row);
            if (mmImportData[index].col < mmImportData[index + 1].col)
            {
                printf("\n\t");
            }
        }
        printf("]\n\n");
    }

    // 1. Allocate a "clone" import data object so we don't clobber the original data during compression (in case other algs will be called later)
    mmCloneData = (MMDataPlus *)malloc(sizeof(MMDataPlus) * (long unsigned int)fInputNonZeros);

    // 2. Compress import data vertically
    for (index = 0; index < fInputNonZeros; index++)
    {
        // Capture the original row of the cell
        mmCloneData[index].row_orig = mmImportData[index].row;

        // The column and value don't change.
        mmCloneData[index].col = mmImportData[index].col;
        mmCloneData[index].val = mmImportData[index].val;

        // If we're on the first NNZ...
        if (index == 0)
        {
            // ...relocate the NNZ to the top of the column (row translate).
            mmCloneData[index].row = 0;
        }
        // If we're past the first NNZ...
        else
        {
            // ...and the column has changed between the current and previous NNZ...
            if (mmImportData[index].col > mmImportData[index - 1].col)
            {
                // ...relocate the NNZ to the top of the column (row translate).
                mmCloneData[index].row = 0;
            }
            // ...and there's a row gap between the current and previous NNZ...
            else if ((mmImportData[index].row - mmImportData[index - 1].row) > 0)
            {
                // ...relocate the NNZ to just below thw previous NNZ (row translate).
                mmCloneData[index].row = (mmCloneData[index - 1].row + 1);
            }
            // ...and there's no row gap between the current and previous NNZ (DEFAULT CASE ASSUMPTION)...
            else
            {
                // ...do nothing to the row of the NNZ.
                mmCloneData[index].row = mmCloneData[index].row;
            }
        }
    }

    if (SMVP_TJDS_DEBUG)
    {
        printf("[DEBUG]\tTJDS PHASE 2: Compress Matrix Vertically (shown sideways):\n");
        printf("\t[");
        for (index = 0; index < fInputNonZeros; index++)
        {
            printf("%g (%d), ", mmCloneData[index].val, mmCloneData[index].row);
            if (mmCloneData[index].col < mmCloneData[index + 1].col)
            {
                printf("\n\t");
            }
        }
        printf("]\n\n");
    }

    // Generate reordering table
    // Iterate through the entire set of NNZs...
    for (index = 0; index < fInputNonZeros; index++)
    {
        // ...if a column change is imminent...
        if (mmCloneData[index].col < mmCloneData[index + 1].col)
        {
            // ...document the column length and position of the column.
            txList[mmCloneData[index].col].colLength = mmCloneData[index].row;
            txList[mmCloneData[index].col].originCol = mmCloneData[index].col;
        }

        // If this is the last NNZ in the set...
        if (index == fInputNonZeros - 1)
        {
            // ...document the column length and position of the the column.
            txList[mmCloneData[index].col].colLength = mmCloneData[index].row;
            txList[mmCloneData[index].col].originCol = mmCloneData[index].col;
        }
    }

    // Derive number of transpose jagged diagonals for later use in TJDS computation
    num_tjdiag = txList[0].colLength + 1;

    // 3. Sort reordering table
    qsort(txList, (size_t)fInputColumns, sizeof(TXTable), txtable_comparator_len);

    if (SMVP_TJDS_DEBUG)
    {
        printf("[DEBUG]\tTJDS PHASE 3: Reordering Table:\n");
        printf("key\t[");
        for (index = 0; index < fInputColumns; index++)
        {
            printf("%d, ", index);
        }
        printf("]\n");
        printf("origCol\t[");
        for (index = 0; index < fInputColumns; index++)
        {
            printf("%d, ", txList[index].originCol);
        }
        printf("]\n");
        printf("colLen\t[");
        for (index = 0; index < fInputColumns; index++)
        {
            printf("%d, ", txList[index].colLength);
        }
        printf("]\n\n");
    }

    // 4. Reassign import data column assignments and recorded row indicies using reordering table
    for (index = 0; index < fInputNonZeros; index++)
    {
        for (txIter = 0; txIter < fInputColumns; txIter++)
        {
            if (txList[txIter].originCol == mmCloneData[index].col)
            {
                mmCloneData[index].col = txIter;
                break;
            }
        }
    }

    // 5. Reassign multiplication vector rows by using reordering table
    onesVectorTemp = (double *)malloc(sizeof(double) * (long unsigned int)fInputRows);
    for (index = 0; index < fInputRows; index++)
    {
        for (txIter = 0; txIter < fInputColumns; txIter++)
        {
            if (txList[txIter].originCol == index)
            {
                onesVectorTemp[txIter] = onesVector[index];
                break;
            }
        }
    }
    for (index = 0; index < fInputRows; index++)
    {
        onesVector[index] = onesVectorTemp[index];
    }
    free(onesVectorTemp);

    // 6. Import data into appropriate TJDS structures
    qsort(mmCloneData, (size_t)fInputNonZeros, sizeof(MMDataPlus), mmdp_comparator_row_col);

    if (SMVP_TJDS_DEBUG)
    {
        printf("[DEBUG]\tTJDS PHASE 6: Reorder Columns (shown normal):\n");
        printf("\t[");
        for (index = 0; index < fInputNonZeros; index++)
        {
            printf("%g, ", mmCloneData[index].val);
            if (mmCloneData[index].row < mmCloneData[index + 1].row)
            {
                printf("\n\t");
            }
        }
        printf("]\n\n");
    }

    // Copy values and col_orig as-ordered since they're already sorted correctly
    sp_index = 0;
    for (index = 0; index < fInputNonZeros; index++)
    {
        workingMatrix.val[index] = mmCloneData[index].val;
        workingMatrix.row_ind[index] = mmCloneData[index].row_orig;

        // Start position of first NNZ is always zero
        if (index == 0)
        {
            workingMatrix.start_pos[sp_index] = index;
            sp_index++;
        }
        // If the row has incremented, capture the row as a start position
        else if (mmCloneData[index].row > mmCloneData[index - 1].row)
        {
            workingMatrix.start_pos[sp_index] = index;
            sp_index++;
        }
        // If we're at the end of the value list, record the "next" value as the last start position
        else if (index == fInputNonZeros - 1)
        {
            workingMatrix.start_pos[sp_index] = index + 1;
        }
    }

    if (SMVP_TJDS_DEBUG)
    {
        printf("[DEBUG]\tTJDS PHASE 7: Pre-Calc Fields:\n");
        printf("\tval:\t\t[");
        for (index = 0; index < fInputNonZeros; index++)
        {
            printf("%g, ", workingMatrix.val[index]);
        }
        printf("]\n");
        printf("\trow_ind:\t[");
        for (index = 0; index < fInputNonZeros; index++)
        {
            printf("%d, ", workingMatrix.row_ind[index]);
        }
        printf("]\n");
        printf("\tstart_pos:\t[");
        for (index = 0; index < num_tjdiag + 1; index++)
        {
            printf("%d, ", workingMatrix.start_pos[index]);
        }
        printf("]\n\n");
        printf("\tnum_tjdiag (count, not 0-index):\t%d", num_tjdiag);
        printf("\n\n");
    }

    free(mmCloneData);

    printf(ANSI_COLOR_YELLOW "[INFO]\tCalculating %d iterations of SMVP TJDS.\n" ANSI_COLOR_RESET, compiter);

    //
    // ATOMIC SECTION START
    // PERFORM NO ACTIONS OTHER THAN SMVP BETWEEN START AND END TIME CAPTURES
    //

    // Compute TJDS SMVP (technically y=Axn, not y=x(A^n) as indicated in reqs doc, but is an approved deviation)
    for (i = 0; i < compiter; i++)
    {

        //Reset output vector contents between iterations
        vectorInit(fInputRows, outputVector, 0);

        // Capture compute run start time
        clock_gettime(CLOCK_MONOTONIC_RAW, &time_run_start[i]);

        for (index = 0; index < num_tjdiag + 1; index++)
        {
            for (j = workingMatrix.start_pos[index]; j < workingMatrix.start_pos[index + 1]; j++)
            {
                p = workingMatrix.row_ind[j];
                outputVector[p] += workingMatrix.val[j] * onesVector[p];
            }
        }

        // Capture compute run end time
        clock_gettime(CLOCK_MONOTONIC_RAW, &time_run_end[i]);
    }

    //
    // ATOMIC SECTION END
    // PERFORM NO ACTIONS OTHER THAN SMVP BETWEEN START AND END TIME CAPTURES
    //

    // Inline Vivado LUT builder

    for (int i = 0; i < 9 + 1; i++)
    {
        for (int j = 0; j < 36519 + 1; j++)
        {
            if (j < workingMatrix.start_pos[i + 1] - workingMatrix.start_pos[i] + i && j >= i)
            {
                printf("a_ij[%d][%d] = 1'b1;\n", i, j);
            }
            else
            {
                printf("a_ij[%d][%d] = 1'b0;\n", i, j);
            }
        }
    }

    int temp = 0;

    for (int i = 0; i < 9 + 1; i++)
    {
        for (int j = 0; j < 36519 + 1; j++)
        {
            if (j < workingMatrix.start_pos[i + 1] - workingMatrix.start_pos[i] + i && j >= i)
            {
                printf("i[%d][%d] = %d;\n", i, j, workingMatrix.row_ind[temp]);
                temp++;
            }
            else
            {
                printf("i[%d][%d] = 1'b0;\n", i, j);
            }
        }
    }

    // // Inline Vivado COE Builder (WIP)
    // temp = 0;
    // uint64_t  packed_val_temp = 0;
    // int coe_memory_depth_min = fInputNonZeros * 2;
    // int coe_memory_initialization_radix = 64; // this is a constant for the ROM structure we're using
    //                                           // Given a radix of 48:
    //                                           // 16U = TJDS_Row_Num (ex. i[X][]), 16M1 = TJDS_Col_Num (ex. i[][X]), 16M2 = TJDS_i_Val, 16L = TLDS_a_ij_val
    //                                           // !!! RESIZE STRUCTURE IF ANY NUMBER EXCEEDS 4095 !!!

    // printf("******************************************************************\n");
    // printf("*************  Single Port Block Memory .COE file   **************\n");
    // printf("******************************************************************\n");
    // printf("memory_initialization_radix=%d\n", coe_memory_initialization_radix);
    // printf("memory_initialization_vector=\n");
    // for (uint16_t i = 0; i < 7; i++)
    // {
    //     for (uint16_t j = 0; j < 33; j++)
    //     {
    //         packed_val_temp = 0;

    //         packed_val_temp |= ( i << 48);
    //         packed_val_temp |= ( j << 32);

    //         if (j < workingMatrix.start_pos[i + 1] - workingMatrix.start_pos[i] + i && j >= i)
    //         {
    //             packed_val_temp |= (1<<0);
    //         }
    //         else
    //         {
    //             packed_val_temp |= (0<<0);
    //         }

    //         if (j < workingMatrix.start_pos[i + 1] - workingMatrix.start_pos[i] + i && j >= i)
    //         {
    //             packed_val_temp |= ((uint16_t)(workingMatrix.row_ind[temp]) << 16);
    //             temp++;
    //         }
    //         else
    //         {
    //             packed_val_temp |= (0 << 12);
    //         }

    //         if ((i == num_tjdiag - 1) && (j == fInputRows - 1))
    //         {
    //             printf("[%d][%d]\t%x;\n", i, j, packed_val_temp);
    //         }
    //         else
    //         {
    //             printf("[%d][%d]\t%x,\n", i, j, packed_val_temp);
    //         }
    //     }
    // }

    // Convert all per-run timespec structs to time in milliseconds & populate time structure
    for (i = 0; i < compiter; i++)
    {
        // Calculate runtime in ns, then convert to ms
        time_run[i] = (double)((time_run_end[i].tv_sec * 1e9 + time_run_end[i].tv_nsec) - (time_run_start[i].tv_sec * 1e9 + time_run_start[i].tv_nsec));
        time_run[i] /= 1e6;

        tjds_time->time_each[i] = time_run[i];
        tjds_time->time_total += time_run[i];
        tjds_time->time_avg += time_run[i];

        if (i == 0)
        {
            tjds_time->time_min = time_run[i];
            tjds_time->time_max = time_run[i];
        }
        else
        {
            if (tjds_time->time_min > time_run[i])
            {
                tjds_time->time_min = time_run[i];
            }
            if (tjds_time->time_max < time_run[i])
            {
                tjds_time->time_max = time_run[i];
            }
        }
    }
    tjds_time->time_avg /= compiter;
    tjds_time->time_stdev = calcStDevDouble(tjds_time->time_each, compiter);

    if (SMVP_TJDS_DEBUG)
    {
        printf("[DEBUG]\tTJDS PHASE 8: Output Vector:\n");
        printf("\t[");
        for (index = 0; index < fInputRows; index++)
        {
            printf("%g, ", outputVector[index]);
        }
        printf("]\n\n");
    }

    return outputVector;
}

// Function: smvp_csr_debug
// Because sometimes things just don't go the way you hoped they would
void smvp_csr_debug(double *output_vector, struct _time_data_ *csr_time, int fInputRows, int fInputNonZeros, int iter)
{

    int index;

    printf("[DEBUG]\tCSR Iterations: %d\n", iter);
    printf("[DEBUG]\tCSR fInputRows: %d\n", fInputRows);
    printf("[DEBUG]\tCSR fInputNonZeros: %d\n", fInputNonZeros);
    printf("[DEBUG]\tCSR Total Time: %g\n", csr_time->time_total);
    printf("[DEBUG]\tCSR Avg Time: %g\n", csr_time->time_avg);
    printf("[DEBUG]\tCSR StDev Time: %g\n", csr_time->time_avg);
    printf("[DEBUG]\tCSR Times:\n");
    printf("\t[");
    for (index = 0; index < iter; index++)
    {
        printf("%g, ", csr_time->time_each[index]);
    }
    printf("]\n");
    printf("[DEBUG]\tCSR Output Vector:\n");
    printf("\t[");
    for (index = 0; index < fInputRows; index++)
    {
        printf("%g, ", output_vector[index]);
    }
    printf("]\n\n");
}

// Function: popt_usage
// Displays syntax recommendations to the user when incorrrect arguments are passed to popt
void popt_usage(poptContext optCon, int exitcode, char *error, char *addl)
{
    poptPrintUsage(optCon, stderr, 0);
    if (error)
        fprintf(stderr, "%s: %s", error, addl);
    exit(exitcode);
}

// Function: checkFolderExists
// Checks if a folder exists at the specified location
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
    int mmio_rb_return, mmio_rs_return, index, alg_mode, calc_iter, cisr_slots;
    int fInputRows, fInputCols, fInputNonZeros;
    int *iteration_time;
    double *output_vector;

    // Ust POPT library to handle command line arguments robustly
    // POPT library and documentation available at https://github.com/devzero2000/POPT
    struct _popt_field
    {
        int iter;
        int slots;
        char *outputFolder;

    } popt_field;

    struct poptOption optionsTable[] = {
        {"all-algs", 'a', POPT_ARG_NONE, NULL, 'a', "Enable all SMVP algorithms.", NULL},
        {"csr", 'c', POPT_ARG_NONE, NULL, 'c', "Enable CSR SMVP algorithm.", NULL},
        {"cisr-gen", 'g', POPT_ARG_NONE, NULL, 'g', "Generate CISR COE file.", NULL},
        {"tjds", 't', POPT_ARG_NONE, NULL, 't', "Enable TJDS SMVP algorithm.", NULL},
        {"number", 'n', POPT_ARG_INT, &popt_field.iter, 'n', "Number of computation iterations per-algorithm.", "1000"},
        {"slots", 's', POPT_ARG_INT, &popt_field.slots, 's', "Number of slots for CISR.", "16"},
        {"dir", 'd', POPT_ARG_STRING, &popt_field.outputFolder, 'd', "Output folder for reports.", "./"},
        POPT_AUTOHELP
            POPT_TABLEEND};

    optCon = poptGetContext(NULL, argc, argv, optionsTable, POPT_CONTEXT_NO_EXEC | POPT_CONTEXT_POSIXMEHARDER);
    poptSetOtherOptionHelp(optCon, "[OPTIONS] <file>");

    // Start with no algorithms selected, enable specific algs as options are processed.
    alg_mode = ALG_NONE;

    // Define default algorithm iteration count
    calc_iter = 1000;

    // Define default CISR slot count
    cisr_slots = 16;

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
        case 'g':
            if (alg_mode == ALG_ALL)
            {
                printf(ANSI_COLOR_RED "[ERROR]\tCombining [-a|--all] with other algorithm flags is not supported.\n" ANSI_COLOR_RESET);
                exit(1);
            }
            else
            {
                alg_mode += ALG_CISR;
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
        case 's':
            if (popt_field.slots >= 1)
            {
                cisr_slots = popt_field.slots;
            }
            else
            {
                printf(ANSI_COLOR_RED "[ERROR]\tInvalid number of CISR slots specified.\n" ANSI_COLOR_RESET);
                exit(1);
            }
            break;
        case 'd':
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

    // Done parsing options/args, release popt instance
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
    printf(ANSI_COLOR_MAGENTA "[FILE]\tInput matrix file name: " ANSI_COLOR_RESET "%s\n", inputFileName);
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

    // Close input file only if it isn't somehow mapped as keyboard input
    if (mmInputFile != stdin)
    {
        fclose(mmInputFile);
    }

    printf(ANSI_COLOR_CYAN "[DATA]\tNon-zero numbers contained in matrix: " ANSI_COLOR_RESET "%d\n", fInputNonZeros);
    printf(ANSI_COLOR_CYAN "[DATA]\tVector operand in use: " ANSI_COLOR_RESET "Ones vector with dimensions [%d, %d]\n", fInputRows, 1);

    // Run every SMVP algorithm selected by user
    if (alg_mode & ALG_CSR)
    {
        // DO CSR
        struct _time_data_ *csr_time = newResultsData(csr_time, calc_iter);
        double *output_vector_csr = smvp_csr_compute(mmImportData, fInputRows, fInputNonZeros, calc_iter, csr_time);
        generateReportText(inputFileName, reportPath, ALG_CSR, fInputNonZeros, fInputRows, calc_iter, output_vector_csr, csr_time);

        if (SMVP_CSR_DEBUG)
        {
            smvp_csr_debug(output_vector_csr, csr_time, fInputRows, fInputNonZeros, calc_iter);
        }
    }
    if (alg_mode & ALG_TJDS)
    {
        // DO TJDS
        struct _time_data_ *tjds_time = newResultsData(tjds_time, calc_iter);
        double *output_vector_tjds = smvp_tjds_compute(mmImportData, fInputRows, fInputCols, fInputNonZeros, calc_iter, tjds_time);
        generateReportText(inputFileName, reportPath, ALG_TJDS, fInputNonZeros, fInputRows, calc_iter, output_vector_tjds, tjds_time);
    }
    if (alg_mode & ALG_CISR)
    {
        // DO CISR COE
        smvp_cisr_coegen(mmImportData, fInputRows, fInputNonZeros, cisr_slots); //, const char *inputFileName, char *reportPath)
    }

    printf(ANSI_COLOR_GREEN "[STOP]\tExit smvp-toolbox v%d.%d.%d\n\n" ANSI_COLOR_RESET, MAJOR_VER, MINOR_VER, REVISION_VER);

    return 0;
}
