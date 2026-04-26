#!/bin/bash

set -euo pipefail

APP_VERSION="3.0.0-beta1"

cd "$(dirname "$0")/../../.."

TOPDIR="$PWD/setup/linux/rpm/build"
SPEC="$PWD/setup/linux/rpm/apk-icon-editor.spec"
ARCHIVE="$TOPDIR/SOURCES/apk-icon-editor-$APP_VERSION.tar.gz"

rm -rf "$TOPDIR"
mkdir -p "$TOPDIR/BUILD" "$TOPDIR/BUILDROOT" "$TOPDIR/RPMS" "$TOPDIR/SOURCES" "$TOPDIR/SPECS" "$TOPDIR/SRPMS"

git archive --format=tar --prefix="apk-icon-editor-$APP_VERSION/" HEAD | gzip -n > "$ARCHIVE"
cp "$SPEC" "$TOPDIR/SPECS/"

rpmbuild --define "_topdir $TOPDIR" -ba "$TOPDIR/SPECS/apk-icon-editor.spec"

echo "RPM packages:"
find "$TOPDIR/RPMS" "$TOPDIR/SRPMS" -type f -name "*.rpm" -print
