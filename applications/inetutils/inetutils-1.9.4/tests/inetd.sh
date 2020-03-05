#!/bin/sh

# Copyright (C) 2013, 2014, 2015 Free Software Foundation, Inc.
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

# Test to establish functionality of inetd.
# An important part is to run in daemon mode
# and to send SIGHUP repeatedly.
#
# Written by Mats Erik Andersson.

. ./tools.sh

if test -z "${VERBOSE+set}"; then
    silence=:
fi

# Portability fix for SVR4
PWD="${PWD:-`pwd`}"
USER=${USER:-`func_id_user`}

# Prerequisites
#
$need_mktemp || exit_no_mktemp

PASSWD=/etc/passwd
PWDDB=/etc/pwd.db
PROTOCOLS=/etc/protocols

# Keep the following two tests separate for better diagnosis!
#
if test ! -r $PROTOCOLS; then
    cat <<-EOT >&2
	This test requires the availability of "$PROTOCOLS",
	a file which can not be found in the current system.
	Therefore skipping this test.
	EOT
    exit 77
fi

if test ! -r $PASSWD && test ! -r $PWDDB; then
    cat <<-EOT >&2
	This test requires availability of either "$PASSWD"
	or "$PWDDB".  The requirement can not be met in the
	current system.  Therefore skipping this test.
	EOT
    exit 77
fi

# Execution control.  Initialise early!
#
do_cleandir=false

# Select numerical target address, only IPv4.
TARGET=${TARGET:-127.0.0.1}

# Executable under test and helper functionality.
#
INETD=${INETD:-../src/inetd$EXEEXT}
ADDRPEEK=${ADDRPEEK:-$PWD/addrpeek$EXEEXT}
TCPGET=${TCPGET:-$PWD/tcpget$EXEEXT}

if test ! -x $ADDRPEEK; then
    echo >&2 "No executable '$ADDRPEEK' present.  Skipping test."
    exit 77
fi

if [ ! -x $INETD ]; then
    echo "Missing executable '$INETD'.  Skipping test." >&2
    exit 77
fi

if test ! -x $TCPGET; then
    echo >&2 "No executable '$TCPGET' present.  Skipping test."
    exit 77
fi

if test -n "$VERBOSE"; then
    set -x
    $INETD --version | $SED '1q'
fi

# The value of USER is vital to the test configuration.
$silence echo "Running test as user $USER."

# For file creation below IU_TESTDIR.
umask 0077

# Keep any external assignment of testing directory.
# Otherwise a randomisation is included.
#
: ${IU_TESTDIR:=$PWD/iu_inetd.XXXXXX}

if [ ! -d "$IU_TESTDIR" ]; then
    do_cleandir=true
    IU_TESTDIR="`$MKTEMP -d "$IU_TESTDIR" 2>/dev/null`" ||
	{
	    echo 'Failed at creating test directory.  Aborting.' >&2
	    exit 77
	}
elif expr X"$IU_TESTDIR" : X"\.\{1,2\}/\{0,1\}$" >/dev/null; then
    # Eliminating directories: . ./ .. ../
    echo 'Dangerous input for test directory.  Aborting.' >&2
    exit 77
fi

# The INETD daemon uses two files in the present test.
#
CONF="$IU_TESTDIR"/inetd.conf
PID="$IU_TESTDIR"/inetd.pid

# Are we able to write in IU_TESTDIR?
# This could happen with preset IU_TESTDIR.
#
touch "$CONF" || {
    echo 'No write access in test directory.  Aborting.' >&2
    exit 1
}

# Erase the temporary directory.
#
clean_testdir () {
    if test -f "$PID" && kill -0 "`cat "$PID"`" >/dev/null 2>&1; then
	kill "`cat "$PID"`" || kill -9 "`cat "$PID"`"
    fi
    if test -z "${NOCLEAN+no}" && $do_cleandir; then
	rm -r -f "$IU_TESTDIR"
    fi
}

# Write a fresh configuration file.  Port is input parameter.
write_conf () {
    # First argument is port number.  Node is fixed.
    echo "$TARGET:$1 stream tcp4 nowait $USER $ADDRPEEK addrpeek addr" \
	> $CONF
}

errno=0

PORT=`expr 12347 + ${RANDOM:-$$} % 521`

write_conf $PORT

# The daemon is launched only once.
#
$INETD -p$PID $CONF

# Allow for the service to settle.
sleep 2

if test ! -f $PID; then
    echo >&2 "Inetd never started: missing the PID-file."
    errno=1
else
    # Repeated SIGHUP testing, with modified port.
    for nn in 1 2 3 4 5; do
	# Check for response at chosen port.
	$TCPGET $TARGET $PORT 2>/dev/null |
	    grep "Your address is $TARGET." >/dev/null 2>&1 || errno=1

	test $errno -eq 0 ||
	    { cat >&2 <<-EOT
		*** Repetition $nn of SIGHUP test has failed. ***
		Configuration file:
		##### $CONF
		`cat $CONF`
		###########
		EOT
	      break; }

	# Update with new port for next round.
	PORT=`expr $PORT + 1 + ${RANDOM:-$$} % 521`
	write_conf $PORT
	kill -HUP `cat $PID`

	# Allow for the service to settle.
	sleep 1
    done
    $silence echo "Passed `expr $nn - 1` SIGHUP rounds."
fi

test $errno -ne 0 || $silence echo 'Successful testing.'

clean_testdir

exit $errno
