# KEYNOTES

Internal notes for ongoing work on `aml2`.

## Direction

- plain C
- Open Watcom
- small DOS launcher
- simple custom TUI, not a framework
- `AML.COM` remains the supervisor entrypoint

## Practical Constraints

- keep runtime behavior simple before chasing more size
- avoid dynamic allocation in the main launcher path
- treat `.COM` pressure as a stub concern first, not a launcher concern
- `AML.COM` owns the default viewer-mode entrypoint; direct `AMLUI.EXE` runs require explicit `/V` or `/E`

## Toolchain

- pinned Open Watcom snapshot: `Current-build` from `2026-04-11`
- vendor root: `vendor/openwatcom-v2/current-build-2026-04-11`
- current tested stub size: `1337 bytes`
- current launcher size is roughly `32 KB`

## Testing Notes

- QEMU + real DOS is the authoritative launcher test path
- host tests cover fast non-TUI configuration and launch-plan checks
- DOS-side automation files are more reliable than QMP key injection
- keep 8.3 naming in mind in the floppy harness

Current real-DOS coverage includes:

- launcher/stub/game end-to-end loop
- long-list navigation
- search and empty-config messaging
- hotkeys and help dialog
- viewer-mode blocking and editor-mode save persistence
- failure cases for missing launcher and invalid working directory

Detailed bring-up findings live in `docs/e2e-findings.md`.

## Release Notes

Principles:
- release screenshots come from a release zip, not a local tree
- keep the release screenshot and README screenshot in sync
- keep release descriptions short and concrete

Checklist after pushing a `vX.Y.Z` tag:

1. Wait for the `Build` workflow on the tag to finish — it creates a *draft* release with `aml2-vX.Y.Z.zip` attached.
2. Publish (un-draft) the release first — the screenshot workflow uses `gh release download`, which does not reliably see drafts in CI:
   ```sh
   gh release edit vX.Y.Z --draft=false
   ```
3. Trigger the screenshot workflow against the tag and download the artifact:
   ```sh
   gh workflow run capture-release-screenshot.yml -f release_tag=vX.Y.Z
   # wait for it, then:
   gh run download <run-id> -D /tmp/aml2-shot
   ```
   The 1x png is `aml2-vX.Y.Z-games-f1-help.png` at 640×400.
4. Update the README screenshot from the same artifact and push to master:
   ```sh
   cp /tmp/aml2-shot/release-screenshot-vX.Y.Z/aml2-vX.Y.Z-games-f1-help.png assets/aml2-screenshot.png
   git commit -am "Update README screenshot from vX.Y.Z release" && git push
   ```
5. Attach the 1x png to the release as an asset:
   ```sh
   gh release upload vX.Y.Z /tmp/aml2-shot/release-screenshot-vX.Y.Z/aml2-vX.Y.Z-games-f1-help.png
   ```
6. Write the release body, embedding the asset via its public download URL:
   ```sh
   gh release edit vX.Y.Z --notes "$(cat <<EOF
   <short user-facing summary>

   - bullet for each notable change
   - omit refactors / CI churn / internal cleanups

   ![aml2 screenshot](https://github.com/ddanila/aml2/releases/download/vX.Y.Z/aml2-vX.Y.Z-games-f1-help.png)

   **Full Changelog**: https://github.com/ddanila/aml2/compare/vPREV...vX.Y.Z
   EOF
   )"
   ```
   The embedded image URL resolves once the asset (step 5) exists on the published release (step 2).

Helper: `./tools/capture_release_help_screenshot.sh <tag>` (the workflow above just runs this on a fresh runner against the published zip).

## Future Ideas

- optional per-game statistics, especially launch count and accumulated play time
