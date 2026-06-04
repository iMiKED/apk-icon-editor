#!/bin/bash

cd "$(dirname "$0")/../.." || { echo "Invalid initial directory."; exit 1; }

rm -rf "setup/macosx/build"
rm -rf "bin/macosx/apk-icon-editor-reborn.app"
rm -rf "build"
find . -name "Makefile*" -type f -delete

QT_BIN="${QT_BIN:-$HOME/Qt/6.8.3/macos/bin}"
ARTIFACT_SUFFIX="${MACOS_ARTIFACT_SUFFIX:-}"

"$QT_BIN/qmake" "DEFINES+=CI" && make || { echo "Could not build the project."; exit 2; }
"$QT_BIN/macdeployqt" bin/macosx/apk-icon-editor-reborn.app || { echo "Could not deploy the project."; exit 3; }
ZIPALIGN_BIN="${MACOS_ZIPALIGN:-$(command -v zipalign || true)}"
if [ -n "$ZIPALIGN_BIN" ] && [ -x "$ZIPALIGN_BIN" ]; then
    cp "$ZIPALIGN_BIN" "bin/macosx/apk-icon-editor-reborn.app/Contents/MacOS/zipalign"
fi
find "bin/macosx/apk-icon-editor-reborn.app" -name ".DS_Store" -type f -delete

mkdir "setup/macosx/build"
cp -R bin/macosx/apk-icon-editor-reborn.app "setup/macosx/build/APK Icon Editor Reborn.app"
ditto -c -k --sequesterRsrc --keepParent "setup/macosx/build/APK Icon Editor Reborn.app" "setup/macosx/build/apk-icon-editor-reborn_3.0.0-beta4${ARTIFACT_SUFFIX}.app.zip"
appdmg setup/macosx/appdmg.json "setup/macosx/build/apk-icon-editor-reborn_3.0.0-beta4${ARTIFACT_SUFFIX}.dmg" || { echo "Could not create installer."; exit 4; }
