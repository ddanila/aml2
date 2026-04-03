#include <conio.h>
#include <dos.h>
#include <stdio.h>

#include "aml.h"

int main(void)
{
    FILE *fp;
    FILE *trace;

    puts("FAKE GAME");
    puts("");
    puts("This is the e2e payload.");
    puts("Press any key to return.");

    fp = fopen(AML_AUTO_FILE, "r");
    if (fp != NULL) {
        fclose(fp);
        trace = fopen(AML_TRACE_FILE, "a");
        if (trace != NULL) {
            fputs("fakegame\n", trace);
            fclose(trace);
        }
        sleep(3);
        return 0;
    }

    getch();
    return 0;
}
