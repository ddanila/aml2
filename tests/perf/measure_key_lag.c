/*
 * measure_key_lag.c: Measure instruction count between consecutive
 * arrow key presses in the aml2 launcher, using the kvikdos test harness.
 *
 * Usage: ./measure_key_lag <path-to-AMLUI.EXE> <path-to-dir-with-LAUNCHER.CFG>
 *
 * The directory must contain LAUNCHER.CFG with at least 3 entries.
 * AMLUI.EXE is run with /V (viewer mode).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../../vendor/kvikdos/test_harness.h"

/* Detect screen change by snapshotting a video RAM region and waiting
 * for any byte to differ. Works regardless of text layout or attributes. */
#define SNAP_SIZE 4000  /* Full screen: 80 cols × 25 rows × 2 bytes. */

static unsigned char snap_buf[SNAP_SIZE];

static void snapshot_screen(void)
{
    int r, c;

    for (r = 0; r < 25; r++) {
        for (c = 0; c < 80; c++) {
            unsigned idx = (unsigned)((r * 80 + c) * 2);

            snap_buf[idx] = (unsigned char)kviktest_read_char(r, c);
            snap_buf[idx + 1] = kviktest_read_attr(r, c);
        }
    }
}

static int wait_for_screen_change(unsigned timeout_ms)
{
    unsigned elapsed = 0;
    int r, c;

    while (elapsed < timeout_ms) {
        for (r = 0; r < 25; r++) {
            for (c = 0; c < 80; c++) {
                unsigned idx = (unsigned)((r * 80 + c) * 2);

                if ((unsigned char)kviktest_read_char(r, c) != snap_buf[idx] ||
                    kviktest_read_attr(r, c) != snap_buf[idx + 1]) {
                    return 1;
                }
            }
        }
        usleep(1000);
        elapsed += 1;
    }
    return 0;
}

int main(int argc, char **argv)
{
    const char *exe_path;
    const char *mount_dir;
    unsigned long insn_first, insn_second;

    if (argc < 3) {
        fprintf(stderr, "Usage: %s <AMLUI.EXE> <dir-with-LAUNCHER.CFG>\n", argv[0]);
        return 1;
    }

    exe_path = argv[1];
    mount_dir = argv[2];

    printf("=== aml2 input lag measurement ===\n");
    printf("EXE:  %s\n", exe_path);
    printf("Dir:  %s\n", mount_dir);

    /* Copy AMLUI.EXE to the mount directory so kvikdos finds it. */
    {
        char cmd[2048];
        snprintf(cmd, sizeof(cmd), "cp '%s' '%s/AMLUI.EXE' 2>/dev/null", exe_path, mount_dir);
        system(cmd);
    }

    /* The test copies AMLUI.EXE into mount_dir via the exe_path arg. */
    printf("\nStarting emulator...\n");
    if (kviktest_start("AMLUI.EXE /V", mount_dir) != 0) {
        fprintf(stderr, "Failed to start kvikdos\n");
        return 1;
    }

    /* Wait for the launcher UI to appear. Look for the frame character
     * or the title text on row 0. */
    printf("Waiting for UI to load (is_running=%d)...\n", kviktest_is_running());
    if (!kviktest_wait_for_text_anywhere("Launcher", 120000, NULL, NULL)) {
        fprintf(stderr, "Timeout waiting for launcher UI\n");
        /* Dump screen for debugging. */
        { int r; char buf[81];
          printf("\n--- Screen dump ---\n");
          for (r = 0; r < 25; r++) {
            kviktest_read_text(r, 0, buf, 81);
            printf("  %02d: [%s]\n", r, buf);
          }
        }
        kviktest_stop();
        return 1;
    }

    /* Let the UI settle (initial draw complete). */
    usleep(500000);

    /* --- Measurement 1: First DOWN keypress --- */
    printf("\n--- First DOWN keypress ---\n");
    snapshot_screen();
    kviktest_insn_count_reset();
    kviktest_send_key(KEY_DOWN);

    if (!wait_for_screen_change(30000)) {
        fprintf(stderr, "Timeout waiting for screen change after first DOWN\n");
        kviktest_stop();
        return 1;
    }

    insn_first = kviktest_insn_count();
    printf("Instructions for first DOWN: %lu\n", insn_first);

    /* Brief pause to simulate rapid (but not instant) double-press. */
    usleep(50000);

    /* --- Measurement 2: Second DOWN keypress --- */
    printf("\n--- Second DOWN keypress ---\n");
    snapshot_screen();
    kviktest_insn_count_reset();
    kviktest_send_key(KEY_DOWN);

    if (!wait_for_screen_change(30000)) {
        fprintf(stderr, "Timeout waiting for screen change after second DOWN\n");
        kviktest_stop();
        return 1;
    }

    insn_second = kviktest_insn_count();
    printf("Instructions for second DOWN: %lu\n", insn_second);

    /* --- Results --- */
    printf("\n=== Results ===\n");
    printf("First DOWN:  %lu instructions\n", insn_first);
    printf("Second DOWN: %lu instructions\n", insn_second);
    if (insn_first > 0) {
        printf("Ratio:       %.2fx\n", (double)insn_second / (double)insn_first);
    }

    /* Clean exit. */
    kviktest_send_key(KEY_F10);
    kviktest_wait_exit(5000);

    printf("\nDone.\n");
    return 0;
}
