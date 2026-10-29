# Icon Variant Tests

Build this Qt Test project with the same Qt kit as the application:

```sh
qmake6
make
QT_QPA_PLATFORM=offscreen ./test_iconvariants
```

On Windows, use a VS 2022 x64 developer prompt with Qt 6.8.3 in `PATH`, then run `qmake`, `nmake`, and `release\test_iconvariants.exe`. Ensure the Qt WebP plugin is available through `QT_PLUGIN_PATH`.

Synthetic fixtures cover adaptive-first selection, tree model validity, branch thumbnails and their live updates, fallback labels, unchanged bitmap preservation, explicit legacy updates, shared foreground detachment, read-only split resources, and replacement dialog defaults/cancellation. All six launcher entry roles have independent header previews; header decorations stay at 16x16 while individual icon rows retain 32x32 decorations.

Optional local integration tests use these environment variables:

- `FUTUBA_DECODED`: an isolated Apktool-decoded Futuba 0.1.15b directory, used read-only for preview checks.
- `FUTUBA_APK` and `APKTOOL_JAR`: enable decode/replace/build/reopen checks in a temporary directory. Requires Java in `PATH`; the supplied APK is never changed. The test output APKs are unsigned and not installed.
- `ICON_VARIANTS_SCREENSHOTS`: save offscreen widget snapshots here. `ICON_VARIANTS_RUSSIAN` may point to a Russian QM file, and `ICON_VARIANTS_FONT` to an installed font file for the offscreen renderer.

Without sample paths, only the optional integration tests are skipped. Do not commit APK samples or generated output.
