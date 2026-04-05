## Goal

Reduce noisy `aml_` prefixes where the project context already provides the namespace, and make exported symbols describe their owning subsystem more clearly.

This pass is planning-only. It does not rename any public API yet.

## Naming Rule

- Keep a project-level prefix only where a globally shared type or constant really belongs to the whole application.
- Prefer module prefixes for exported functions:
  - `cfg_*`
  - `ui_*`
  - `launch_*`
- Prefer no project prefix for `static` file-local helpers.

## Current Exported Surface

### `include/cfg.h`

- `AmlCfgStatus`
- `aml_load_config`
- `aml_save_config`

### `include/ui.h`

- `AmlUiAction`
- `aml_ui_init`
- `aml_ui_shutdown`
- `aml_ui_draw`
- `aml_ui_show_message`
- `aml_ui_show_notice`
- `aml_ui_wait_for_ack`
- `aml_ui_run`

### `include/launch.h`

- `AmlLaunchCheck`
- `aml_write_run_request`
- `aml_clear_run_request`
- `aml_check_launch_entry`
- `aml_check_direct_launch_entry`
- `aml_run_entry_child`

### `include/aml.h`

- macros such as `AML_CONFIG_FILE`, `AML_RUN_FILE`, `AML_MAX_*`
- shared types `AmlEntry`, `AmlState`

## Proposed Public Function Renames

### Config

- `aml_load_config` -> `cfg_load`
- `aml_save_config` -> `cfg_save`

### UI

- `aml_ui_init` -> `ui_init`
- `aml_ui_shutdown` -> `ui_shutdown`
- `aml_ui_draw` -> `ui_draw`
- `aml_ui_show_message` -> `ui_show_message`
- `aml_ui_show_notice` -> `ui_show_notice`
- `aml_ui_wait_for_ack` -> `ui_wait_for_ack`
- `aml_ui_run` -> `ui_run`

### Launch

- `aml_write_run_request` -> `launch_write_run_request`
- `aml_clear_run_request` -> `launch_clear_run_request`
- `aml_check_launch_entry` -> `launch_check_entry`
- `aml_check_direct_launch_entry` -> `launch_check_direct_entry`
- `aml_run_entry_child` -> `launch_run_child`

## Proposed Type And Enum Direction

For the first API cleanup pass, keep these as-is:

- `AmlEntry`
- `AmlState`
- `AmlCfgStatus`
- `AmlUiAction`
- `AmlLaunchCheck`

Reason:

- function renames give most of the readability gain with much lower churn
- type renames would touch nearly every translation unit and make the diff much noisier

Possible later follow-up if desired:

- `AmlCfgStatus` -> `CfgStatus`
- `AmlUiAction` -> `UiAction`
- `AmlLaunchCheck` -> `LaunchCheck`

I would only do that after the function namespace settles.

## Constants And Macros

Keep the `AML_*` constants for now.

Reason:

- they are clearly application-global
- they collide less easily than short module-prefixed macros
- changing them has lower readability payoff than function renames

If they are ever revisited, prefer a deliberate app-level namespace rather than bare module macros.

## Internal Helper Rule

For `static` file-local helpers:

- remove `aml_`
- keep only the intent-bearing part of the name

Examples:

- `aml_validate_entry` -> `validate_entry`
- `aml_switch_to_entry_directory` -> `switch_to_entry_directory`
- `aml_execute_child_command` -> `execute_child_command`

For shared internal-only helpers across multiple `src/ui_*.c` files:

- use `ui_` rather than `aml_ui_`

Examples:

- `aml_ui_has_entries` -> `ui_has_entries`
- `aml_ui_select_next_wrap` -> `ui_select_next_wrap`

## Migration Options

### Option A: Hard Rename

- rename exported declarations and all call sites in one pass
- no compatibility macros

Pros:

- cleanest end state
- no duplicate naming layer

Cons:

- larger single diff
- breaks any external downstream code immediately

### Option B: Transitional Compatibility Macros

- rename implementations and canonical declarations
- keep temporary compatibility aliases in headers

Example:

```c
#define aml_load_config cfg_load
#define aml_save_config cfg_save
```

Pros:

- smoother migration
- easy to update call sites incrementally

Cons:

- temporary header noise
- can obscure which name is canonical unless documented clearly

### Option C: Leave Public API, Clean Internals Only

- keep exported `aml_*`
- remove `aml_` only from private and shared-internal helpers

Pros:

- lowest risk
- most churn reduction inside implementation files

Cons:

- exported names stay noisy and vague

## Recommended Sequence

1. Rename `static` file-local helpers to drop `aml_`.
2. Rename shared internal UI helpers from `aml_ui_*` to `ui_*`.
3. Rename exported functions to module prefixes:
   - `cfg_*`
   - `ui_*`
   - `launch_*`
4. Decide later whether shared types should also drop the `Aml` prefix.

## Recommended Public API Strategy

Use Option B first:

- rename exported functions to the module-prefixed forms
- keep temporary compatibility macros in public headers for one pass
- update all in-repo call sites immediately
- remove compatibility macros in a later cleanup once the tree is stable

This gives a cleaner public shape without forcing a single all-or-nothing cutover.
