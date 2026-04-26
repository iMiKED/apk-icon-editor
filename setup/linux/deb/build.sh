#!/bin/bash

export DEBEMAIL="imiked@users.noreply.github.com"
export DEBFULLNAME="iMiKED"

PACKAGE="apk-icon-editor"
VERSION="3.0.0~beta1"

clean() {
    rm ../apk-icon-editor_*.tar.xz ../apk-icon-editor_*.tar.gz ../apk-icon-editor_*.dsc ../apk-icon-editor_*.changes ../apk-icon-editor_*.build 2> /dev/null
    rm -rf ./debian 2> /dev/null
}

cd "$(dirname "$0")/../../.."

clean
rm ../apk-icon-editor_*.deb 2> /dev/null

if dh_make -y -s -c gpl3 --createorig --packagename "$PACKAGE"_"$VERSION"; then
    rm -rf ./debian 2> /dev/null
    cp -R ./setup/linux/deb/debian ./debian
    debuild -uc -us
    clean
fi
