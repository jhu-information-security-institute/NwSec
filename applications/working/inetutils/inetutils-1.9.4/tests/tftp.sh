#!/bin/sh

# Copyright (C) 2010, 2011, 2012, 2013, 2014, 2015 Free Software
# Foundation, Inc.
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

# Run `inetd' with `tftpd' and try to fetch a file from there using `tftp'.

# Prerequisites:
#
#  * Shell: SVR4 Bourne shell, or newer.
#
#  * dd(1), id(1), kill(1), mktemp(1), netstat(8), uname(1).
#
#  * Accessed by launched Inetd:
#      /etc/nsswitch.conf, /etc/passwd, /etc/protocols.
#
#    OpenBSD uses /etc/services directly, not via /etc/nsswitch.conf.

#
# Currently implemented tests (10 or 12 in total):
#
#  * Read three files in binary mode, from 127.0.0.1 and ::1,
#    needing one, two, and multiple data packets, respectively.
#
#  * Read one moderate size ascii file from 127.0.0.1 and ::1.
#
#  * Reload configuration and read a small binary file twice.
#
#  * (root only) Reload configuration for chrooted mode.
#    Read one binary file with a relative name, and one ascii
#    file with absolute location.
#
# The values of TARGET and TARGET6 replace the loopback addresses
# 127.0.0.1 and ::1, whenever the variables are set.  However,
# Setting the variable ADDRESSES to a list of addresses takes
# precedence over all other choices.  The particular value "sense"
# tries to find all local addresses, then go ahead with these.

. ./tools.sh

$need_dd || exit_no_dd
$need_mktemp || exit_no_mktemp
$need_netstat || exit_no_netstat

if test -z "${VERBOSE+set}"; then
    silence=:
    bucket='>/dev/null'
fi

if test -n "$VERBOSE"; then
    set -x
fi

# Portability fix for SVR4
PWD="${PWD:-`pwd`}"

TFTP="${TFTP:-../src/tftp$EXEEXT}"
TFTPD="${TFTPD:-$PWD/../src/tftpd$EXEEXT}"
INETD="${INETD:-../src/inetd$EXEEXT}"
IFCONFIG="${IFCONFIG:-../ifconfig/ifconfig$EXEEXT --format=unix}"
IFCONFIG_SIMPLE=`expr X"$IFCONFIG" : X'\([^ ]*\)'`	# Remove options

if [ ! -x $TFTP ]; then
    echo "No TFTP client '$TFTP' present.  Skipping test" >&2
    exit 77
elif [ ! -x $TFTPD ]; then
    echo "No TFTP server '$TFTPD' present.  Skipping test" >&2
    exit 77
elif [ ! -x $INETD ]; then
    echo "No inetd superserver '$INETD' present.  Skipping test" >&2
    exit 77
elif [ ! -x $IFCONFIG_SIMPLE ]; then	# Remove options
    echo "No ifconfig '$IFCONFIG_SIMPLE' present.  Skipping test" >&2
    exit 77
fi

# The superserver Inetd puts constraints on any chroot
# when running this script, since it needs to look up
# some basic facts stated in the configuration file.
NSSWITCH=/etc/nsswitch.conf
PASSWD=/etc/passwd
PROTOCOLS=/etc/protocols

# Overrides based on systems.
test `uname -s` = OpenBSD && NSSWITCH=/etc/services

if test ! -r $NSSWITCH || test ! -r $PASSWD \
      || test ! -r $PROTOCOLS; then
    cat <<-EOT >&2
	The use of the superserver Inetd in this script requires
	the availability of "$NSSWITCH", "$PASSWD", and
	"$PROTOCOLS".  At least one of these is now missing.
	Therefore skipping test.
	EOT
    exit 77
fi

AF=${AF:-inet}
PROTO=${PROTO:-udp}
USER=`func_id_user`

# Late supplimentary subtest.
do_conf_reload=true
do_secure_setting=true

# Disable chrooted mode for non-root invocation.
test `func_id_uid` -eq 0 || do_secure_setting=false

# Random base directory at testing time.
TMPDIR=`$MKTEMP -d $PWD/tmp.XXXXXXXXXX` ||
    {
	echo 'Failed at creating test directory.  Aborting.' >&2
	exit 1
    }

INETD_CONF="$TMPDIR/inetd.conf.tmp"
INETD_PID="$TMPDIR/inetd.pid.$$"

posttesting () {
    if test -n "$TMPDIR" && test -f "$INETD_PID" \
	&& test -r "$INETD_PID" \
	&& kill -0 "`cat $INETD_PID`" >/dev/null 2>&1
    then
	kill "`cat $INETD_PID`" 2>/dev/null ||
	kill -9 "`cat $INETD_PID`" 2>/dev/null
    fi
    test -n "$TMPDIR" && test -d "$TMPDIR" \
	&& rm -rf "$TMPDIR" $FILELIST
}

trap posttesting EXIT HUP INT QUIT TERM

# Use only "127.0.0.1 ::1" as default address list,
# but take account of TARGET and TARGET6.
# Other configured addresses might be set under
# strict filter policies, thus might block.
#
# Allow a setting "ADDRESSES=sense" to compute the
# available addresses and then to test them all.
if test "$ADDRESSES" = "sense"; then
    ADDRESSES=`$IFCONFIG -a | $SED -e "/$AF /!d" \
	-e "s/^.*$AF \([:.0-9]\{1,\}\) .*$/\1/g"`
fi

if test -z "$ADDRESSES"; then
    ADDRESSES="${TARGET:-127.0.0.1}"
    test "$TEST_IPV6" = "no" || ADDRESSES="$ADDRESSES ${TARGET6:-::1}"
fi

# Work around the peculiar output of netstat(1m,solaris).
#
# locate_port proto port
#
locate_port () {
    if [ "`uname -s`" = "SunOS" ]; then
	$NETSTAT -na -finet -finet6 -P$1 |
	$GREP "\.$2[^0-9]" >/dev/null 2>&1
    else
	$NETSTAT -na |
	$GREP "^$1[46]\{0,2\}.*[^0-9]$2[^0-9]" >/dev/null 2>&1
    fi
}

if [ "$VERBOSE" ]; then
    "$TFTP" --version | $SED '1q'
    "$TFTPD" --version | $SED '1q'
    "$INETD" --version | $SED '1q'
    "$IFCONFIG_SIMPLE" --version | $SED '1q'
fi

# Find an available port number.  There will be some
# room left for a race condition, but we try to be
# flexible enough for running copies of this script.
#
if test -z "$PORT"; then
    for PORT in 7777 7779 7783 7791 7807 7839 none; do
	test $PORT = none && break
	if locate_port $PROTO $PORT; then
	    continue
	else
	    break
	fi
    done
    if test "$PORT" = 'none'; then
	echo 'Our port allocation failed.  Skipping test.' >&2
	exit 77
    fi
fi

# Create `inetd.conf'.  Note: We want $TFTPD to be an absolute file
# name because `inetd' chdirs to `/' in daemon mode; ditto for
# $INETD_CONF.  Thus the dependency on file locations will be
# identical in daemon-mode and in debug-mode.
write_conf () {
    cat > "$INETD_CONF" <<-EOF
	$PORT dgram ${PROTO}4 wait $USER $TFTPD   tftpd -l $TMPDIR/tftp-test
	EOF

    test "$TEST_IPV6" = "no" ||
	cat >> "$INETD_CONF" <<-EOF
	$PORT dgram ${PROTO}6 wait $USER $TFTPD   tftpd -l $TMPDIR/tftp-test
	EOF
}

write_conf ||
    {
	echo 'Could not create configuration file for Inetd.  Aborting.' >&2
	exit 1
    }

# Launch `inetd', assuming it's reachable at all $ADDRESSES.
# Must use '-d' consistently to prevent daemonizing, but we
# would like to suppress the verbose output.  The variable
# REDIRECT is set to '2>/dev/null' in non-verbose mode.
#
test -n "$VERBOSE" || REDIRECT='2>/dev/null'

eval "$INETD -d -p'$INETD_PID' '$INETD_CONF' $REDIRECT &"

# Debug mode allows the shell to recover PID of Inetd.
spawned_pid=$!

sleep 2

inetd_pid="`cat $INETD_PID 2>/dev/null`" ||
    {
	cat <<-EOT >&2
		Inetd did not create a PID-file.  Aborting test,
		but loosing control whether an Inetd process is
		still around.
	EOT
	exit 1
    }

test -z "$VERBOSE" || echo "Launched Inetd as process $inetd_pid." >&2

# Wait somewhat for the service to settle.
sleep 1

# Did `inetd' really succeed in establishing a listener?
locate_port $PROTO $PORT
if test $? -ne 0; then
    # No it did not.
    kill -0 "$inetd_pid" >/dev/null 2>&1 && kill -9 "$inetd_pid" 2>/dev/null
    rm -f "$INETD_PID"

    echo 'First attempt at starting Inetd has failed.' >&2
    echo 'A new attempt will follow after some delay.' >&2
    echo 'Increasing verbosity for better backtrace.' >&2
    sleep 7

    # Select a new port, with offset and some randomness.
    PORT=`expr $PORT + 137 + \( ${RANDOM:-$$} % 517 \)`
    write_conf ||
	{
	    echo 'Could not create configuration file for Inetd.  Aborting.' >&2
	    exit 1
	}

    $INETD -d -p"$INETD_PID" "$INETD_CONF" &
    spawned_pid=$!
    sleep 2
    inetd_pid="`cat $INETD_PID 2>/dev/null`" ||
	{
	    cat <<-EOT >&2
		Inetd did not create a PID-file.  Aborting test,
		but loosing control whether an Inetd process is
		still around.
		EOT
	    kill -0 "$spawned_pid" >/dev/null 2>&1 && kill -9 "$spawned_pid" 2>/dev/null
	    exit 1
	}

    echo "Launched Inetd as process $inetd_pid." >&2

    if locate_port $PROTO $PORT; then
	: # Successful this time.
    else
	echo "Failed again at starting correct Inetd instance." >&2
	kill -0 "$spawned_pid" >/dev/null 2>&1 && kill -9 "$spawned_pid" 2>/dev/null
	exit 1
    fi
fi

if [ -r /dev/urandom ]; then
    input="/dev/urandom"
else
    input="/dev/zero"
fi

test -d "$TMPDIR" && rm -fr "$TMPDIR/tftp-test" tftp-test-file*
test -d "$TMPDIR" && mkdir -p "$TMPDIR/tftp-test" \
    || {
	echo 'Failed at creating directory for master files.  Aborting.' >&2
	exit 1
    }

# It is important to test data of differing sizes.
# These are binary files.
#
# Input format:
#
#  name  block-size  count

FILEDATA="file-small 320 1
file-medium 320 2
tftp-test-file 1024 170"

echo "$FILEDATA" |
while read name bsize count; do
    test -z "$name" && continue
    $DD if="$input" of="$TMPDIR/tftp-test/$name" \
	bs=$bsize count=$count 2>/dev/null
done

FILELIST="`echo "$FILEDATA" | $SED 's/ .*//' | tr "\n" ' '`"

# Add a file known to be ASCII encoded.
#
ASCIIFILE=asciifile.txt
if test -r tools.sh; then
    cp tools.sh "$TMPDIR/tftp-test/$ASCIIFILE"
    FILELIST="$FILELIST $ASCIIFILE"
fi

SUCCESSES=0
EFFORTS=0
RESULT=0

$silence echo "Looking into '`echo $ADDRESSES | tr "\n" ' '`'."

for addr in $ADDRESSES; do
    $silence echo "trying address '$addr'..." >&2

    for name in $FILELIST; do
	test -n "$name" || continue
	EFFORTS=`expr $EFFORTS + 1`
	rm -f "$name"
	test "$name" = $ASCIIFILE && type=ascii || type=binary
	echo "$type
get $name" | \
	eval "$TFTP" ${VERBOSE:+-v} "$addr" $PORT $bucket

	cmp "$TMPDIR/tftp-test/$name" "$name" 2>/dev/null
	result=$?

	if [ "$result" -ne 0 ]; then
	    # Failure.
	    test -z "$VERBOSE" || echo "Failed comparison for $addr/$name." >&2
	    RESULT=$result
	else
	    SUCCESSES=`expr $SUCCESSES + 1`
	    test -z "$VERBOSE" || echo "Successful comparison for $addr/$name." >&2
	fi
   done
done

# Test the ability of inetd to reload configuration:
#
# Assign a new port in the configuration file. Send SIGHUP
# to inetd and check whether transmission of the small
# file used previously still succeeds.
#
PORT=`expr $PORT + 1 + ${RANDOM:-$$} % 521`

locate_port $PROTO $PORT &&
    {
	# Try a second port.
	PORT=`expr $PORT + 97 + ${RANDOM:-$$} % 479`
	# Disable subtest if still no free port.
	locate_port $PROTO $PORT && do_conf_reload=false
    }

$silence echo >&2

if $do_conf_reload; then
    $silence echo >&2 'Testing altered and reloaded configuration.'
    write_conf ||
	{
	    echo >&2 'Could not rewrite configuration file for Inetd.  Failing.'
	    exit 1
	}

    kill -HUP $inetd_pid
    name=`echo "$FILELIST" | $SED 's/ .*//'`
    for addr in $ADDRESSES; do
	EFFORTS=`expr $EFFORTS + 1`
	test -f "$name" && rm "$name"
	echo "binary
get $name" | \
	eval "$TFTP" ${VERBOSE:+-v} "$addr" $PORT $bucket
	cmp "$TMPDIR/tftp-test/$name" "$name" 2>/dev/null
	result=$?
	if test $result -ne 0; then
	    test -z "$VERBOSE" || echo >&2 "Failed comparison for $addr/$name."
	    RESULT=$result
	else
	    SUCCESSES=`expr $SUCCESSES + 1`
	    test -z "$VERBOSE" || echo >&2 "Success at new port for $addr/$name."
	fi
    done
else
    $silence echo >&2 'Informational: Inhibiting config reload test.'
fi

if $do_secure_setting; then
    # Allow an underprivileged process owner to read files.
    chmod g=rx,o=rx $TMPDIR

    cat > "$INETD_CONF" <<-EOF
	$PORT dgram ${PROTO}4 wait $USER $TFTPD   tftpd -l -s $TMPDIR /tftp-test
	EOF

    test "$TEST_IPV6" = "no" ||
	cat >> "$INETD_CONF" <<-EOF
	$PORT dgram ${PROTO}6 wait $USER $TFTPD   tftpd -l -s $TMPDIR /tftp-test
	EOF

    # Let inetd reload configuration.
    kill -HUP $inetd_pid

    # Test two files: file-small and asciifile.txt
    #
    addr=`echo "$ADDRESSES" | $SED 's/ .*//'`
    name=`echo "$FILELIST" | $SED 's/ .*//'`
    rm -f "$name" "$ASCIIFILE"
    EFFORTS=`expr $EFFORTS + 2`

    echo "binary
get $name
ascii
get /tftp-test/$ASCIIFILE" | \
    eval "$TFTP" ${VERBOSE:+-v} "$addr" $PORT $bucket

    cmp "$TMPDIR/tftp-test/$name" "$name" 2>/dev/null
    result=$?
    if test $? -ne 0; then
	$silence echo >&2 "Failed chrooted access to $name."
	RESULT=$result
    else
	$silence echo >&2 "Success with chrooted access to $name."
	SUCCESSES=`expr $SUCCESSES + 1`
    fi
    cmp "$TMPDIR/tftp-test/$ASCIIFILE" "$ASCIIFILE" 2>/dev/null
    result=$?
    if test $? -ne 0; then
	$silence echo >&2 "Failed chrooted access to /tftp-test/$ASCIIFILE."
	RESULT=$result
    else
	$silence echo >&2 "Success with chrooted /tftp-test/$ASCIIFILE."
	SUCCESSES=`expr $SUCCESSES + 1`
    fi
else
    $silence echo >&2 'Informational: Inhibiting chroot test.'
fi

# Minimal clean up. Main work in posttesting().
$silence echo
test $RESULT -eq 0 && $silence false \
    || echo Test had $SUCCESSES successes out of $EFFORTS cases.

exit $RESULT
