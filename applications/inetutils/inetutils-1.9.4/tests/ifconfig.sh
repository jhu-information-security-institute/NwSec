#!/bin/sh

# Copyright (C) 2011, 2012, 2013, 2014, 2015 Free Software Foundation, Inc.
#
# This file is part of GNU Inetutils.
#
# GNU Inetutils is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or (at
# your option) any later version.
#
# GNU Inetutils is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see `http://www.gnu.org/licenses/'.

# Tests to establish functionality of ifconfig utility.
#
# Written by Mats Erik Andersson.

# Prerequisites:
#
#  * Shell: SVR4 Bourne shell, or newer.

# Is usage explanation in demand?
#
if test "$1" = "-h" || test "$1" = "--help" || test "$1" = "--usage"; then
    cat <<HERE
Test utility for ifconfig.

The following environment variables are used:

VERBOSE		Be verbose, if set.
FORMAT		Test only these output formats.  A list of
		formats is excepted.
TARGET		Loopback address; defaults to 127.0.0.1.

HERE
    exit 0
fi

# Step into `tests/', should the invokation
# have been made outside of it.
#
[ -d src ] && [ -f tests/syslogd.sh ] && cd tests/

. ./tools.sh

TARGET=${TARGET:-127.0.0.1}

# Executable under test.
#
IFCONFIG=${IFCONFIG:-../ifconfig/ifconfig$EXEEXT}

if test ! -x "$IFCONFIG"; then
    echo >&2 "Missing executable '$IFCONFIG'.  Skipping test."
    exit 77
fi

if test -z "${VERBOSE+set}"; then
    silence=:
    bucket='>/dev/null'
fi

if test -n "$VERBOSE"; then
    set -x
    $IFCONFIG --version | $SED '1q'
fi

# Locate the loopback interface.
#
# Avoid cases where `lo' is a substring
# of another interface name, like for
# `pflog0' of OpenBSD.
#
IF_LIST=`$IFCONFIG -l`
for nn in $IF_LIST; do
    LO=`expr $nn : '\(lo0\{0,\}\).*'`
    test -z "$LO" || break
done

if test -z "$LO"; then
    echo >&2 'Unable to locate loopback interface.  Failing.'
    exit 1
fi

target=`echo $TARGET | $SED -e 's,\.,\\\\&,g'`

find_lo_addr () {
   $IFCONFIG ${1+--format=$1} -i $LO | \
   eval $GREP "'inet .*$target'" $bucket 2>/dev/null
}

errno=0

# Check for loopback address in all formats displaying the
# standard address $TARGET, commonly 127.0.0.1.
#
for fmt in ${FORMAT:-gnu gnu-one-entry net-tools osf unix}; do
    $silence echo "Checking format $fmt."
    find_lo_addr $fmt || { errno=1; echo >&2 "Failed with format '$fmt'."; }
done

# Check that all listed adapters are responding affirmatively
# to all formats, but discard all output.
#
for fmt in check default gnu gnu-one-entry netstat net-tools osf unix
do
    for iface in $IF_LIST; do
	$IFCONFIG --format=$fmt -i $iface >/dev/null ||
	    { errno=1;
	      echo >&2 "Failed with format '$fmt', adapter '$iface'."
	    }
    done
done

# Check that short format and full printout succeed; discard output.
#
$IFCONFIG --short >/dev/null ||
    { errno=1; echo >&2 'Failed with short format.'; }

$IFCONFIG --all >/dev/null ||
    { errno=1; echo >&2 'Failed while listing all.'; }

# Informational check whether the legacy form use
# is implemented.  No error produced, only message.
#
if $IFCONFIG $LO >/dev/null 2>&1; then
    :
else
    cat <<-EOT
	Hint: This system does not yet support the legacy use
	      which does without switches:  ifconfig $LO
EOT
fi

test $errno -ne 0 || $silence echo "Successful testing".

exit $errno
