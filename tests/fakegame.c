#include <conio.h>
#include <dos.h>
#include <stdio.h>

#include "aml.h"

int main(void)
{
#if AML_TEST_HOOKS
    FILE *fp;
    FILE *trace;
#endif

    puts("FAKE GAME");
    puts("");
    puts("This is the e2e payload.");
    puts("Press any key to return.");

#if AML_TEST_HOOKS
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
#endif

    getch();
    return 0;
}
