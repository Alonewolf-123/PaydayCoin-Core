#!/usr/bin/env bash

export LC_ALL=C
TOPDIR=${TOPDIR:-$(git rev-parse --show-toplevel)}
BUILDDIR=${BUILDDIR:-$TOPDIR}

BINDIR=${BINDIR:-$BUILDDIR/src}
MANDIR=${MANDIR:-$TOPDIR/doc/man}

PAYDAYCOIND=${PAYDAYCOIND:-$BINDIR/paydaycoind}
PAYDAYCOINCLI=${PAYDAYCOINCLI:-$BINDIR/paydaycoin-cli}
PAYDAYCOINTX=${PAYDAYCOINTX:-$BINDIR/paydaycoin-tx}
WALLET_TOOL=${WALLET_TOOL:-$BINDIR/paydaycoin-wallet}
PAYDAYCOINQT=${PAYDAYCOINQT:-$BINDIR/qt/paydaycoin-qt}

[ ! -x $PAYDAYCOIND ] && echo "$PAYDAYCOIND not found or not executable." && exit 1

# The autodetected version git tag can screw up manpage output a little bit
PDAYVER=($($PAYDAYCOINCLI --version | head -n1 | awk -F'[ -]' '{ print $6, $7 }'))

# Create a footer file with copyright content.
# This gets autodetected fine for paydaycoind if --version-string is not set,
# but has different outcomes for paydaycoin-qt and paydaycoin-cli.
echo "[COPYRIGHT]" > footer.h2m
$PAYDAYCOIND --version | sed -n '1!p' >> footer.h2m

for cmd in $PAYDAYCOIND $PAYDAYCOINCLI $PAYDAYCOINTX $WALLET_TOOL $PAYDAYCOINQT; do
  cmdname="${cmd##*/}"
  help2man -N --version-string=${PDAYVER[0]} --include=footer.h2m -o ${MANDIR}/${cmdname}.1 ${cmd}
  sed -i "s/\\\-${PDAYVER[1]}//g" ${MANDIR}/${cmdname}.1
done

rm -f footer.h2m
