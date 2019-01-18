#!/bin/bash

export LC_ALL=C
TOPDIR=${TOPDIR:-$(git rev-parse --show-toplevel)}
BUILDDIR=${BUILDDIR:-$TOPDIR}

BINDIR=${BINDIR:-$BUILDDIR/src}
MANDIR=${MANDIR:-$TOPDIR/doc/man}

VERGED=${VERGED:-$BINDIR/verged}
VERGECLI=${VERGECLI:-$BINDIR/verge-cli}
VERGETX=${VERGETX:-$BINDIR/verge-tx}
VERGEQT=${VERGEQT:-$BINDIR/qt/verge-qt}

[ ! -x $VERGED ] && echo "$verged not found or not executable." && exit 1

# The autodetected version git tag can screw up manpage output a little bit
XSHVER=($($VERGECLI --version | head -n1 | awk -F'[ -]' '{ print $6, $7 }'))

# Create a footer file with copyright content.
# This gets autodetected fine for verged if --version-string is not set,
# but has different outcomes for verge-qt and verge-cli.
echo "[COPYRIGHT]" > footer.h2m
$verged --version | sed -n '1!p' >> footer.h2m

for cmd in $verged $VERGECLI $VERGETX $VERGEQT; do
  cmdname="${cmd##*/}"
  help2man -N --version-string=${XSHVER[0]} --include=footer.h2m -o ${MANDIR}/${cmdname}.1 ${cmd}
  sed -i "s/\\\-${XSHVER[1]}//g" ${MANDIR}/${cmdname}.1
done

rm -f footer.h2m
