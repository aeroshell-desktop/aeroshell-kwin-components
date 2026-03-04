#! /bin/sh

# SPDX-License-Identifier: CC0-1.0
# SPDX-FileCopyrightText: none

DOMAIN=aeroshell-kwin-components
podir=./po
EXTRACTRC=extractrc
mkdir -p $podir

PKGNAME=aeroshell-kwin-components
BUG_ADDR=https://gitgud.io/aeroshell/aeroshell-kwin-components/-/issues

kde_xgettext() {
    xgettext --package-name=$PKGNAME \
        --msgid-bugs-address=$BUG_ADDR \
        --from-code=UTF-8 \
        -C --kde \
        -ci18n \
        -ki18n:1 -ki18nc:1c,2 -ki18np:1,2 -ki18ncp:1c,2,3 \
        -ki18nd:2 -ki18ndc:2c,3 -ki18ndp:2,3 -ki18ndcp:2c,3,4 \
        -kki18n:1 -kki18nc:1c,2 -kki18np:1,2 -kki18ncp:1c,2,3 \
        -kki18nd:2 -kki18ndc:2c,3 -kki18ndp:2,3 -kki18ndcp:2c,3,4 \
        -kxi18n:1 -kxi18nc:1c,2 -kxi18np:1,2 -kxi18ncp:1c,2,3 \
        -kxi18nd:2 -kxi18ndc:2c,3 -kxi18ndp:2,3 -kxi18ndcp:2c,3,4 \
        -kkxi18n:1 -kkxi18nc:1c,2 -kkxi18np:1,2 -kxi18ncp:1c,2,3 \
        -kkxi18nd:2 -kkxi18ndc:2c,3 -kxi18ndp:2,3 -kxi18ndcp:2c,3,4 \
        -kkli18n:1 -kkli18nc:1c,2 -kki18np:1,2 -kki18ncp:1c,2,3 \
        -kklxi18n:1 -kklxi18nc:1c,2 -kklxi18np:1,2 -kklxi18ncp:1c,2,3 \
        -kI18N_NOOP:1 -kI18NC_NOOP:1c,2 \
        -kI18N_NOOP2:1c,2 -kI18N_NOOP2_NOSTRIP:1c,2 \
        -ktr2i18n:1 -ktr2xi18n:1 \
        "$@"
}

# Export the function so it's available to child scripts
export -f kde_xgettext
export XGETTEXT="kde_xgettext"

$EXTRACTRC `find -name \*.ui -o -name \*.rc -o -name \*.kcfg` >> rc.cpp || exit 11
$XGETTEXT $(find -name *.cpp -o -name *.h  -o -name *.qml) -o $podir/$DOMAIN.pot
rm -f rc.cpp
