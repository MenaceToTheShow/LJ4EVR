# Releasing

Tag-driven, the Anubis `Build apps` pattern (design 9.4, 9.5). Semver `vX.Y.Z`;
`v0.x` are pre-releases; `v1.0.0` at the public flip.

## Cut a release

1. Bump `VERSION` (one line, `x.y.z`, no `v`). It feeds `version.h`, CPack,
   the bundle version and the About box; nothing else carries the number.
2. Move the `Unreleased` section of `CHANGELOG.md` under the new version.
3. Commit, make sure `ci.yml` is green on `main`.
4. Tag and push:

        git tag v$(cat VERSION)
        git push origin main v$(cat VERSION)

5. Watch `release.yml`. When it finishes, the Release page has three assets.
6. Mac acceptance (ME-21): the maintainer runs the `.dmg` on a Mac and
   screenshots the Sequence window and one menu when he can (a W5 entry gate
   since the W4 verdict, ME-158); until then `[CHROME] Native=1` is the
   documented macOS default and the release notes say so.

## What `release.yml` does

| Job | Runner | Does |
|---|---|---|
| `check` | `ubuntu-latest` | fails unless the tag equals `v$(cat VERSION)`; exports the version |
| `windows` | `windows-latest` | preset `release-win`, `ctest`, `cpack -G NSIS` -> `LJ4EVR-<ver>-win64-setup.exe` (per-user, no admin); `dumpbin /dependents` must list no `vcruntime` / `msvcp` / `ucrtbase` (ME-19); a silent install (`/S /D=<temp>`) checked by `tools/help_check.py --install-tree` (ME-36, W4-HELP); the installer's VERSIONINFO equals `VERSION` |
| `macos` | `macos-14` | preset `release-mac` (universal, macOS 12+), `ctest`, `strip -x`, `codesign --force --deep -s -` (ad-hoc), `codesign --verify --strict`; the log then shows `lipo -info` (arm64 and x86_64 required), `codesign -dv` (`Signature=adhoc` required), `plutil -p Info.plist` (`CFBundleShortVersionString` = `VERSION`, `LSMinimumSystemVersion` = 12.0) and the `LC_BUILD_VERSION` minos; `cpack -G DragNDrop -D CPACK_STRIP_FILES=OFF` -> `LJ4EVR-<ver>-macos.dmg`; the `.dmg` is mounted, listed, its `Applications` link and `LJ4EVR.app` checked with `help_check.py --install-tree` and `codesign --verify` (W4-MAC) |
| `release` | `ubuntu-latest`, `contents: write` | downloads both, writes `SHA256SUMS`, `gh release create` with both installers + sums; `--prerelease` for `0.*`, `--latest` otherwise; notes carry the SmartScreen line, the drag-to-Applications and right-click-open / `xattr -d com.apple.quarantine /Applications/LJ4EVR.app` instruction (until an Apple Developer account exists, R6), the notices and trademark lines, then the version's `CHANGELOG.md` section up to its `Developer detail:` line (the sacrifice list and the known gaps live there; the job fails if the section is missing) |

The two build jobs only upload artifacts; one final job creates the Release,
so nothing races to create it. Everything runs on GitHub-hosted runners; a
release never touches the self-hosted runners.

Why the Mac job strips by hand: `CPACK_STRIP_FILES ON` in `CMakeLists.txt`
would strip the installed copy of the binary after it was signed, which breaks
the ad-hoc signature. The workflow strips first, signs, then tells cpack not to.

## If it goes wrong

- `check` red: `VERSION` and the tag disagree. Fix `VERSION`, delete the tag
  (`git push origin :refs/tags/vX.Y.Z`), retag.
- A build job red: fix on `main`, delete the tag, retag. Releases are
  immutable once published; do not re-run a `release` job onto an existing
  Release with changed binaries.
- Re-running `release` alone after a transient failure is fine: the artifacts
  of the same run are reused.

## v1.0.0 and the public flip: the rows that need the maintainer

`v1.0.0` is tagged only after every RELEASE row of `docs/w4-gates.md` is ticked,
and the repository goes public only with the maintainer's explicit go. The
evidence for the four rows that need him is recorded here (and in the W4
integration log) when it exists; until then each slot stays open.

| Row | What is needed | Evidence slot |
|---|---|---|
| Licence ruling (design 13 item 1) | the maintainer picks the repository licence. MIT (ME-155, Joey's ruling of 2026-10-22); `LICENSE`, `NOTICE`, the About line (`LJ_ABOUT_LICENCE` in `src/windows/lj_about_form.c`, the one constant), `MACOSX_BUNDLE_COPYRIGHT`, the NSIS `LegalCopyright` and the CPack licence page (`CPACK_RESOURCE_FILE_LICENSE` = `LICENSE`) all say MIT today and change together | ruling: MIT, 2026-10-22 (Q2); `gh repo view --json licenseInfo`: MIT |
| ME-45 non-admin install | one install of the release-candidate NSIS installer on a standard-user account (not in Administrators): `whoami /groups` without `S-1-5-32-544`, the tree under `%LOCALAPPDATA%\Programs\LJ4EVR`, the HKCU uninstall key, About showing the version | `docs/screenshots/w4/install-nonadmin.png`: _open_ |
| ME-21 Mac run | the maintainer runs the release-candidate `.dmg` on a Mac, screenshots the Sequence window and one menu, and latches a cue from a MIDI device | `docs/screenshots/w4/mac-*.png`: _open_ (until then `[CHROME] Native=1` is the documented macOS default) |
| REL-FLIP | `gh repo edit UnbannableGT/LJ4EVR --visibility public` after REL-HISTORY, REL-PRIVATE, REL-CI, ME-36, ME-37, the licence ruling and ME-45 are ticked. Right after it (a private repository cannot have it): `gh api -X PUT repos/UnbannableGT/LJ4EVR/private-vulnerability-reporting`, then read it back with `gh api repos/UnbannableGT/LJ4EVR/private-vulnerability-reporting` (`enabled: true`); `SECURITY.md` points at it and gives the fallback until then. Also right after it: fork pull-request approval `all_external_contributors` (`docs/ci.md` rule 2), read back | the go: _open_; `gh repo view --json visibility`: _open_ |
