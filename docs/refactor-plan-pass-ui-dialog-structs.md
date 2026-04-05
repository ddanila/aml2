## UI Dialog Struct Refactor Plan

Scope:
- reduce repeated dialog body assembly in `ui_edit.c` and `ui_core.c`
- introduce a struct-driven dialog rendering layer for repeated dialog layouts
- extend the model to editable dialogs without hiding their input behavior behind one oversized abstraction

Prerequisite:
- first rename generic dialog/layout interfaces to neutral UI names
- keep `AML_*` and `Aml*` only where the concept is launcher-specific rather than reusable UI infrastructure

Constraints:
- preserve all current dialog text, layout, key behavior, and cursor behavior
- keep the implementation plain C with fixed-size data and no dynamic allocation
- avoid a giant all-in-one modal framework that mixes rendering, focus, and event loops

Execution:
1. Rename the generic dialog/layout helpers in `src/ui_int.h` and `src/ui_core.c` so the dialog layer reads as reusable UI infrastructure.
2. Add a dialog spec and widget/item model that can describe:
   - titled dialog bounds
   - static text rows
   - centered text
   - ellipsized text
   - label/value rows
   - editable text fields
   - simple selectable menus
3. Implement one generic draw entry point for dialog specs and small focused helpers for widget-specific rendering and cursor placement.
4. Convert static dialogs first:
   - notice/message box helpers in `ui_core.c`
   - details overlay
   - delete confirmation
   - help overlay
5. Convert editable and selectable dialogs next while keeping their event loops explicit:
   - entry editor dialog
   - debug run menu
6. Run `./tools/build.sh` and `bash tests/run_all.sh`.

Non-goals:
- no user-visible redesign
- no callback-heavy dialog engine
- no generic framework that owns every dialog input loop
