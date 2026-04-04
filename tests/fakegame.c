#include <conio.h>
#include <direct.h>
#include <dos.h>
#include <stdio.h>

#include "aml.h"

int main(void)
{
#if AML_TEST_HOOKS
    FILE *fp;
    FILE *trace;
    char cwd[128];
    const char *auto_path = AML_AUTO_FILE;
    const char *trace_path = AML_TRACE_FILE;
#endif

    puts("FAKE GAME");
    puts("");
    puts("This is the e2e payload.");
    puts("Press any key to return.");

#if AML_TEST_HOOKS
    fp = fopen(auto_path, "r");
    if (fp == NULL) {
        auto_path = "A:\\AML2.AUT";
        trace_path = "A:\\AML2.TRC";
        fp = fopen(auto_path, "r");
    }
    if (fp != NULL) {
        fclose(fp);
        trace = fopen(trace_path, "a");
        if (trace != NULL) {
            fputs("fakegame\n", trace);
            if (getcwd(cwd, sizeof(cwd)) != NULL) {
                fputs("cwd=", trace);
                fputs(cwd, trace);
                fputc('\n', trace);
            }
            fclose(trace);
        }
        sleep(3);
        return 0;
    }
#endif

    getch();
    return 0;
}
