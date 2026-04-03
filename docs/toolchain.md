# Toolchain

`aml2` vendors a pinned Open Watcom snapshot from the `open-watcom-v2` GitHub releases.

Current pin:

- release tag: `Current-build`
- published: `2026-04-03`
- vendor path: `vendor/openwatcom-v2/current-build-2026-04-03`

## Why vendor it

- the Ubuntu snap is stale
- host installs are inconvenient to keep aligned
- other local repos already use version-pinned Open Watcom bundles

## Expected Layout

After setup, the toolchain root should contain directories like:

- `binl64/`
- `binl/`
- `h/`
- `lib286/`
- `lib386/`

For Linux x86_64 hosts, `aml2` uses:

- `binl64/wcc`
- `binl64/wlink`

## Setup

```bash
./tools/setup_openwatcom.sh
```

This downloads `ow-snapshot.tar.xz` from the `Current-build` release and extracts it into the pinned vendor directory.

## Build

```bash
./tools/build.sh
```

The build script exports the required Open Watcom environment variables and runs `make`.

## Updating the Pin

When we move to a newer daily build:

1. change the pinned directory date in `tools/setup_openwatcom.sh`
2. change `WATCOM_ROOT` in `Makefile`
3. update this document

Keep the pin explicit so builds stay reproducible.
