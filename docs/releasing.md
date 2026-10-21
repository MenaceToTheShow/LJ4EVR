# Releasing

Tag-driven, the Anubis `Build apps` pattern (design 9.4, 9.5). Semver `vX.Y.Z`;
`v0.x` are pre-releases until wave 2 closes; `v1.0.0` at the public flip.

## Cut a release

1. Bump `VERSION` (one line, `x.y.z`, no `v`). It feeds `version.h`, CPack,
   the bundle version and the About box; nothing else carries the number.
2. Move the `Unreleased` section of `CHANGELOG.md` under the new version.
3. Commit, make sure `ci.yml` is green on `main`.
4. Tag and push:

        git tag v$(cat VERSION)
        git push origin main v$(cat VERSION)

5. Watch `release.yml`. When it finishes, the Release page has three assets.
6. Mac acceptance (ME-21, every wave tag): Joey runs the `.dmg` on his Mac and
   screenshots the Sequence window and one menu. Attach both to the release.

## What `release.yml` does

| Job | Runner | Does |
|---|---|---|
| `check` | `ubuntu-latest` | fails unless the tag equals `v$(cat VERSION)`; exports the version |
| `windows` | `windows-latest` | preset `release-win`, `ctest`, `cpack -G NSIS` -> `LJ4EVR-<ver>-win64-setup.exe` (per-user, no admin) |
| `macos` | `macos-14` | preset `release-mac` (universal, macOS 12+), `ctest`, `strip -x`, `codesign --force --deep -s -` (ad-hoc), `codesign --verify --strict`, `cpack -G DragNDrop -D CPACK_STRIP_FILES=OFF` -> `LJ4EVR-<ver>-macos.dmg` |
| `release` | `ubuntu-latest`, `contents: write` | downloads both, writes `SHA256SUMS`, `gh release create` with both installers + sums; `--prerelease` for `0.*`; notes carry the SmartScreen line, the right-click-open / `xattr -d com.apple.quarantine` instruction (until an Apple Developer account exists, R6) and the ME-21 acceptance line |

The two build jobs only upload artifacts; one final job creates the Release,
so nothing races to create it. Everything runs on GitHub-hosted runners; a
release never touches atlas-ci2.

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
