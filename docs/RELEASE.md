# Releasing CHRONA

Tag a version and CI builds, signs, packages and publishes it.

```bash
git tag v0.1.0 && git push origin v0.1.0
```

`.github/workflows/release.yml` then, per platform:
1. builds the plug-in (Release, all formats),
2. **macOS** — `scripts/package-macos.sh`: codesigns the VST3/AU (hardened
   runtime), builds a `.pkg`, notarizes + staples,
3. **Windows** — `packaging/windows/CHRONA.iss`: builds an Inno Setup installer,
4. uploads artifacts and attaches them to a GitHub Release.

## Signing secrets (repo → Settings → Secrets → Actions)

Signing/notarization is **skipped gracefully** when these are absent (you still
get unsigned artifacts). For a distributable build, set:

| Secret | Meaning |
|---|---|
| `MACOS_CERT_ID` | `Developer ID Application: Name (TEAMID)` |
| `MACOS_INSTALLER_ID` | `Developer ID Installer: Name (TEAMID)` |
| `AC_TEAM_ID` | Apple Developer team id |
| `AC_NOTARY_PROFILE` | notarytool keychain profile (`xcrun notarytool store-credentials`) |
| *(Windows)* | sign the produced installer with your own `signtool` + EV/OV cert |

The macOS runner must import your Developer ID cert into a keychain before
`package-macos.sh` (standard `apple-actions/import-codesign-certs` step — add it
with your base64 cert secret when you have one).

## Install locations
- macOS: `/Library/Audio/Plug-Ins/VST3` and `…/Components` (via the pkg).
- Windows: `C:\Program Files\Common Files\VST3`.

## Pre-release gate
Run the checklist in [`QUALITY_BAR.md`](QUALITY_BAR.md); all 🔴 gates must PASS
with evidence (CI already runs build ×3 OS, ASan/UBSan, TSan and pluginval L10).
