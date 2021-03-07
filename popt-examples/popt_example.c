/* Processes command-line options. */
#include <stdio.h>
#include <stdlib.h>
#include <popt.h>

/* Data values for the options. */
static int intVal = 55;
static int print = 0;
static char *stringVal;
void callback(poptContext context,
              enum poptCallbackReason reason,
              const struct poptOption *option,
              const char *arg,
              const void *data)
{
    switch (reason)
    {
    case POPT_CALLBACK_REASON_PRE:
        printf("\t Callback in pre setting\n");
        break;
    case POPT_CALLBACK_REASON_POST:
        printf("\t Callback in post setting\n");
        break;
    case POPT_CALLBACK_REASON_OPTION:
        printf("\t Callback in option setting\n");
        break;
    }
}

/* Set up a table of options. */
static struct poptOption optionsTable[] = {
    {(const) "int", (char)'i', POPT_ARG_INT, (void *)&intVal, 0,
     (const) "follow with an integer value", (const) "2, 4, 8, or 16"},
    {"callback", '\0', POPT_ARG_CALLBACK | POPT_ARGFLAG_DOC_HIDDEN,
     &callback, 0, NULL, NULL},
    {(const) "file", (char)'f', POPT_ARG_STRING, (void *)&stringVal, 0,
     (const) "follow with a file name", NULL},
    {(const) "print", (char)'p', POPT_ARG_NONE, &print, 0,
     (const) "send output to the printer", NULL},
    POPT_AUTOALIAS
        POPT_AUTOHELP
            POPT_TABLEEND};
int main(int argc, char *argv[])
{
    poptContext context = poptGetContext(
        (const char *)"popt1",
        argc,
        argv,
        (const struct poptOption *)&optionsTable,
        0);
    int option = poptGetNextOpt(context);
    printf("option = %d\n", option);

    /* Print out option values. */
    printf("After processing, options have values:\n");
    printf("\t intVal holds %d\n", intVal);
    printf("\t print flag holds %d\n", print);
    printf("\t stringVal holds [%s]\n", stringVal);
    poptFreeContext(context);
    exit(0);
}