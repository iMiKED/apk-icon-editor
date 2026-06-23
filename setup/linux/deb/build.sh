#!/bin/bash

export DEBEMAIL="M-I-B@yandex.ru"
export DEBFULLNAME="iMiKED"

PACKAGE="apk-icon-editor-reborn"
VERSION="3.0.0~beta5"

clean() {
    rm ../apk-icon-editor-reborn_*.tar.xz ../apk-icon-editor-reborn_*.tar.gz ../apk-icon-editor-reborn_*.dsc ../apk-icon-editor-reborn_*.changes ../apk-icon-editor-reborn_*.build 2> /dev/null
    rm -rf ./debian 2> /dev/null
}

cd "$(dirname "$0")/../../.."

clean
rm ../apk-icon-editor-reborn_*.deb 2> /dev/null

if dh_make -y -s -c gpl3 --createorig --packagename "$PACKAGE"_"$VERSION"; then
    rm -rf ./debian 2> /dev/null
    cp -R ./setup/linux/deb/debian ./debian
    debuild -uc -us
    clean
fi
