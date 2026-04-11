#include <conio.h>
#include <ctype.h>
#include <dos.h>
#include <i86.h>

#include "ui_int.h"
#include "ui_ops.h"
#include "ui_state.h"

static void update_search_selection(AmlState *state, const char *query)
{
    int match = ui_find_match(state, query);

    if (match >= 0) {
        ui_select_index(state, match);
    }
}

static void prompt_search(AmlState *state)
{
    char query[UI_SEARCH_MAX + 1];
    int len = 0;

    query[0] = '\0';

    for (;;) {
        int key;

        ui_render(state);
        ui_fill_rect(3, 23, 76, 23, ' ', UI_ATTR_STATUS);
        ui_write_at(3, 23, "Find:", UI_ATTR_MUTED);
        ui_write_padded(9, 23, query, UI_SEARCH_MAX, UI_ATTR_STATUS);
        ui_flush();

        key = getch();
        if (key == UI_KEY_ESC) {
            return;
        }
        if (key == UI_KEY_ENTER) {
            int match = ui_find_match(state, query);
            if (match >= 0) {
                ui_select_index(state, match);
            }
            return;
        }
        if (key == UI_KEY_BACKSPACE) {
            if (len > 0) {
                query[--len] = '\0';
            }
        } else if (isprint(key) && len < UI_SEARCH_MAX) {
            query[len++] = (char)key;
            query[len] = '\0';
        } else {
            continue;
        }

        update_search_selection(state, query);
    }
}

static int bios_kbhit(void);
#pragma aux bios_kbhit = \
    "mov ah, 1"   \
    "int 0x16"    \
    "jnz have"    \
    "xor ax, ax"  \
    "jmp done"    \
    "have:"       \
    "mov ax, 1"   \
    "done:"       \
    value [ax]    \
    modify [ax];

static void wait_for_input_redraw(AmlState *state, unsigned *last_tick)
{
    unsigned short far *tick = (unsigned short far *)MK_FP(0x0040, 0x006C);

    while (!bios_kbhit()) {
        unsigned now_tick = *tick;

        if ((unsigned)(now_tick - *last_tick) >= 18) {
            ui_update_clock(state);
            *last_tick = now_tick;
        }
    }
}

static AmlUiAction apply_test_automation(AmlState *state)
{
#if AML_TEST_HOOKS
    sleep(1);
    return ui_apply_automation(state);
#else
    (void)state;
    return (AmlUiAction)UI_AUTO_NONE;
#endif
}

static AmlUiAction handle_extended_key(AmlState *state, int key)
{
    if (key == UI_KEY_F1) {
        ui_show_help_overlay(state);
    } else if (key == UI_KEY_F2) {
        return AML_UI_SAVE;
    } else if (key == UI_KEY_F3) {
        ui_show_details_overlay(state);
    } else if (key == UI_KEY_F4) {
        ui_edit_entry(state);
    } else if (key == UI_KEY_F5) {
        ui_move_entry_up(state);
    } else if (key == UI_KEY_F6) {
        ui_move_entry_down(state);
    } else if (key == UI_KEY_INS) {
        ui_insert_entry(state);
    } else if (key == UI_KEY_F8) {
        ui_delete_entry_with_confirm(state);
    } else if (key == UI_KEY_F9) {
        if (ui_has_entries(state)) {
            return ui_show_debug_run_menu(state);
        }
    } else if (key == UI_KEY_F10) {
        return AML_UI_QUIT;
    } else if (!ui_has_entries(state)) {
        return (AmlUiAction)UI_AUTO_NONE;
    } else if (key == UI_KEY_HOME) {
        ui_select_first(state);
    } else if (key == UI_KEY_END) {
        ui_select_last(state);
    } else if (key == UI_KEY_PGUP) {
        ui_select_page_up(state);
    } else if (key == UI_KEY_PGDN) {
        ui_select_page_down(state);
    } else if (key == UI_KEY_UP) {
        ui_select_prev_wrap(state);
    } else if (key == UI_KEY_DOWN) {
        ui_select_next_wrap(state);
    }

    return (AmlUiAction)UI_AUTO_REDRAW;
}

static AmlUiAction handle_hotkey(AmlState *state, int key)
{
    int hotkey_index = ui_hotkey_index(key);

    if (hotkey_index >= 0 && hotkey_index < state->entry_count) {
        ui_select_index(state, hotkey_index);
        ui_sync_view_top(state);
        return AML_UI_LAUNCH;
    }

    return (AmlUiAction)UI_AUTO_NONE;
}

AmlUiAction ui_run(AmlState *state)
{
    unsigned short far *tick = (unsigned short far *)MK_FP(0x0040, 0x006C);
    unsigned last_tick = *tick;
    int redraw_pending = 1;

    ui_sync_view_top(state);

    for (;;) {
        AmlUiAction action;
        int key;

        if (redraw_pending) {
            ui_draw(state);
            last_tick = *tick;
            redraw_pending = 0;
        }

        action = apply_test_automation(state);
        if (action == UI_AUTO_REDRAW) {
            ui_sync_view_top(state);
            redraw_pending = 1;
            continue;
        }
        if (action >= 0) {
            return action;
        }

        wait_for_input_redraw(state, &last_tick);
        key = getch();

        if (key == UI_KEY_ENTER) {
            if (ui_has_entries(state)) {
                return AML_UI_LAUNCH;
            }
            redraw_pending = 1;
            continue;
        }
        if (key == UI_KEY_SLASH && ui_has_entries(state)) {
            prompt_search(state);
            ui_sync_view_top(state);
            redraw_pending = 1;
            continue;
        }
        if (key == UI_KEY_EXTENDED || key == UI_KEY_EXTENDED_2) {
            int ext_key = getch();
            int old_selected = state->selected;
            int old_view_top = state->view_top;

            action = handle_extended_key(state, ext_key);
            if (action >= 0) {
                return action;
            }
            ui_sync_view_top(state);
            /* Write directly to VRAM to show key processed — no backbuffer draw */
            { unsigned short far *v = (unsigned short far *)MK_FP(0xB800, 0);
              v[24*80] = (unsigned short)('0' + (state->selected & 0xF)) | 0x4E00;
            }
            if ((ext_key == UI_KEY_UP || ext_key == UI_KEY_DOWN) &&
                state->view_top == old_view_top) {
                ui_draw_selection_change(state, old_selected);
                last_tick = *tick;
            } else {
                redraw_pending = 1;
            }
            continue;
        }
        if (key == UI_KEY_QUESTION) {
            ui_show_help_overlay(state);
            redraw_pending = 1;
            continue;
        }

        action = handle_hotkey(state, key);
        if (action >= 0) {
            return action;
        }
        redraw_pending = 1;
    }
}
