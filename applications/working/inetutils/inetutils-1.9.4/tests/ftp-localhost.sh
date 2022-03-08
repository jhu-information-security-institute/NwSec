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

# Written by Simon Josefsson

# Prerequisites:
#
#  * Shell: SVR4 Bourne shell, or newer.
#
#  * chmod(1), id(1), kill(1), mktemp(1), netstat(8), uname(1).
#
#  * Detection of sysctl(8) is made.  Availability will
#    lead to better test coverage.
#
#  * Accessed by launched Inetd:
#      /etc/nsswitch.conf, /etc/passwd, /etc/protocols.
#
#    OpenBSD uses /etc/services directly, not via /etc/nsswitch.conf.

# FIXME: Better test coverage!
#
# Implemented: anonymous-only in inetd-mode.
#
# Wanted:  * standalone-mode
#          * underprivileged mode.
#
# FIXME: Resolve the hard coded dependency on anonymous mode.

# Address mapping IPv4-to-IPv6 is not uniform an all platforms,
# thus separately using `tcp4' and `tcp6' for streams in `inetd.conf'.
# FIXME: Once functionality attains, reduce code duplication when
# evaluating partial tests.

set -e

. ./tools.sh

FTP=${FTP:-../ftp/ftp$EXEEXT}
FTPD=${FTPD:-../ftpd/ftpd$EXEEXT}
INETD=${INETD:-../src/inetd$EXEEXT}
TARGET=${TARGET:-127.0.0.1}
TARGET6=${TARGET6:-::1}
TARGET46=${TARGET46:-::ffff:127.0.0.1}

# Extended transmission testing.
# This puts contents into $FTPHOME,
# and is therefore not active by default.
do_transfer=false
test "${TRANSFERTEST+yes}" = "yes" && do_transfer=true

# Portability fix for SVR4
PWD="${PWD:-`pwd`}"

# Acting user and target user
#
USER=`func_id_user`
FTPUSER=${FTPUSER:-ftp}

if [ ! -x $FTP ]; then
    echo "No FTP client '$FTP' present.  Skipping test" >&2
    exit 77
elif [ ! -x $FTPD ]; then
    echo "No FTP server '$FTPD' present.  Skipping test" >&2
    exit 77
elif [ ! -x $INETD ]; then
    echo "No inetd superserver '$INETD' present.  Skipping test" >&2
    exit 77
fi

$need_mktemp || exit_no_mktemp
$need_netstat || exit_no_netstat

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

if [ $VERBOSE ]; then
    set -x
    $FTP --version | $SED '1q'
    $FTPD --version | $SED '1q'
    $INETD --version | $SED '1q'
fi

if [ `func_id_uid` != 0 ]; then
    echo "ftpd needs to run as root" >&2
    exit 77
fi

if id "$FTPUSER" > /dev/null; then
    :
else
    echo "anonymous ftpd needs a '$FTPUSER' user" >&2
    exit 77
fi

FTPHOME="`eval echo ~"$FTPUSER"`"
if test ! -d "$FTPHOME"; then
    save_IFS="$IFS"
    IFS=:
    set -- `$GREP "^$FTPUSER:" /etc/passwd`	# Existence is known as above.
    IFS="$save_IFS"
    if test -d "$6"; then
	FTPHOME="$6"
    elif test -d "$5"; then	# In cases where GECOS is empty
	FTPHOME="$5"
    else
	echo "The user '$FTPUSER' must have a home directory." >&2
	exit 77
    fi
fi

if test -d "$FTPHOME" && test -r "$FTPHOME" && test -x "$FTPHOME"; then
    :	# We have full access to anonymous' home directory.
else
    echo "Insufficient access for $FTPUSER's home directory." >&2
    exit 77
fi

# Try common subdirectories for writability.
# Result is in DLDIR, usable in chrooted setting.
# Assigns an empty value when no writable candidate
# was found.

if test -z "$DLDIR"; then
    # We depend on detection of a usable subdirectory,
    # so reactivate this sub-test only with DLDIR known
    # to be functional.

    do_transfer=false

    for DLDIR in /pub /download /downloads /dl /tmp / ; do
	test -d $FTPHOME$DLDIR || continue
	set -- `ls -ld $FTPHOME$DLDIR`
	# Check owner.
	test "$3" = $FTPUSER || continue
	# Check for write access.
	test `expr $1 : 'drwx'` -eq 4 && do_transfer=true && break
	DLDIR=	# Reset failed value
    done

    test x"$DLDIR" = x"/" && DLDIR=
fi

# Exit with a hard error, should transfer test be requested,
# but a suitable subdirectory be missing.

if test "${TRANSFERTEST+yes}" = "yes" && \
    test $do_transfer = false
then
    cat >&2 <<-END
	There is no writable subdirectory for transfer test.
	Aborting FTP test completely.
	END
    exit 99
fi

# Note that inetd changes directory to / when --debug is not given so
# all paths must be absolute for things to work.

TMPDIR=`$MKTEMP -d $PWD/tmp.XXXXXXXXXX` ||
    {
	echo 'Failed at creating test directory.  Aborting.' >&2
	exit 1
    }

posttesting () {
    test -n "$TMPDIR" && test -f "$TMPDIR/inetd.pid" \
	&& test -r "$TMPDIR/inetd.pid" \
	&& { kill "`cat $TMPDIR/inetd.pid`" \
	     || kill -9 "`cat $TMPDIR/inetd.pid`"; }
    test -n "$TMPDIR" && test -d "$TMPDIR" && rm -rf "$TMPDIR"
    $do_transfer && test -n "$FTPHOME" \
	&& test -f "$FTPHOME$DLDIR/$PUTME" && rm -f "$FTPHOME$DLDIR/$PUTME" \
	|| true
}

trap posttesting 0 1 2 3 15

# locate_port  port
#
# Test for IPv4 as well as for IPv6.
locate_port () {
    if [ "`uname -s`" = "SunOS" ]; then
	$NETSTAT -na -finet -finet6 -Ptcp |
	$GREP "\.$1[^0-9]" >/dev/null 2>&1
    else
	$NETSTAT -na |
	$GREP "^$2[46]\{0,2\}.*[^0-9]$1[^0-9]" >/dev/null 2>&1
    fi
}

# Files used in transmission tests.
GETME=`$MKTEMP $TMPDIR/file.XXXXXXXX` || do_transfer=false

test -n "$GETME" && GETME=`expr "$GETME" : "$TMPDIR/\(.*\)"`

PUTME=putme.$GETME

# Find an available port number.  There will be some
# room left for a race condition, but we try to be
# flexible enough for running copies of this script.
#
if test -z "$PORT"; then
    for PORT in 4711 4713 4717 4725 4741 4773 none; do
	test $PORT = none && break
	if locate_port $PORT; then
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

cat <<EOT > "$TMPDIR/inetd.conf"
$PORT stream tcp4 nowait $USER $PWD/$FTPD ftpd -A -l
EOT

test "$TEST_IPV6" = "no" ||
    cat <<EOT >> "$TMPDIR/inetd.conf"
$PORT stream tcp6 nowait $USER $PWD/$FTPD ftpd -A -l
EOT

if test $? -ne 0; then
    echo 'Failed at writing configuration for Inetd.  Skipping test.' >&2
    exit 77
fi

cat <<EOT > "$TMPDIR/.netrc"
machine $TARGET login $FTPUSER password foobar
EOT

if test $? -ne 0; then
    echo 'Failed at writing access file ".netrc".  Skipping test.' >&2
    exit 77
fi

if test "$TEST_IPV6" != "no"; then
    cat <<-EOT >> "$TMPDIR/.netrc"
	machine $TARGET6 login $FTPUSER password foobar
	machine $TARGET46 login $FTPUSER password foobar
	EOT
fi

chmod 600 "$TMPDIR/.netrc"

# Some simple, but variable content.
ls -l > "$TMPDIR/$GETME"

$INETD --pidfile="$TMPDIR/inetd.pid" "$TMPDIR/inetd.conf" ||
    {
	echo 'Not able to start Inetd.  Skipping test.' >&2
	exit 1
    }

# Wait for inetd to write pid and open socket
sleep 2


test -r "$TMPDIR/inetd.pid" ||
    {
	cat <<-EOT >&2
		Inetd could not write a PID-file, but did claim a start.
		This is a serious problem.  Doing an emergency abort,
		without possibility of killing the Inetd-process.
	EOT
	exit 1
    }

# Test evaluation helper
#
# test_report  errno output_file hint_msg
#
test_report () {
    test -z "${VERBOSE}" || cat "$2"

    if [ $1 != 0 ]; then
	echo "Running '$FTP' failed with errno $1." >&2
	exit 1
    fi

    # Did we get access?
    if $GREP 'Login failed' "$2" >/dev/null 2>&1; then
	echo "Failed login for access using '$3' FTP client." >&2
	exit 1
    fi

    # Standing control connection?
    if $GREP 'FTP server status' "$2" >/dev/null 2>&1; then
	:
    else
	echo "Cannot find server status for '$3' FTP client?" >&2
	exit 1
    fi

    # Was data transfer successful?
    if $GREP '226 Transfer complete.' "$2" >/dev/null 2>&1; then
	:
    else
	echo "Cannot find transfer result for '$3' FTP client?" >&2
	exit 1
    fi
}

# Test a passive connection: PASV and IPv4.
#
echo "PASV to $TARGET (IPv4) using inetd."
cat <<STOP |
rstatus
dir
`$do_transfer && test -n "$DLDIR" && echo "\
cd $DLDIR"`
`$do_transfer && echo "\
lcd $TMPDIR
image
put $GETME $PUTME"`
STOP
HOME=$TMPDIR $FTP "$TARGET" $PORT -4 -v -p -t >$TMPDIR/ftp.stdout 2>&1

test_report $? "$TMPDIR/ftp.stdout" "PASV/$TARGET"

$do_transfer && \
    if cmp -s "$TMPDIR/$GETME" "$FTPHOME$DLDIR/$PUTME"; then
	test "${VERBOSE+yes}" && echo >&2 'Binary transfer succeeded.'
	date "+%s" >> "$TMPDIR/$GETME"
    else
	echo >&2 'Binary transfer failed.'
	exit 1
    fi

# Test an active connection: PORT and IPv4.
#
echo "PORT to $TARGET (IPv4) using inetd."
cat <<STOP |
rstatus
dir
`$do_transfer && test -n "$DLDIR" && echo "\
cd $DLDIR"`
`$do_transfer && echo "\
lcd $TMPDIR
image
put $GETME $PUTME"`
STOP
HOME=$TMPDIR $FTP "$TARGET" $PORT -4 -v -t >$TMPDIR/ftp.stdout 2>&1

test_report $? "$TMPDIR/ftp.stdout" "PORT/$TARGET"

$do_transfer && \
    if cmp -s "$TMPDIR/$GETME" "$FTPHOME$DLDIR/$PUTME"; then
	test "${VERBOSE+yes}" && echo >&2 'Binary transfer succeeded.'
	date "+%s" >> "$TMPDIR/$GETME"
    else
	echo >&2 'Binary transfer failed.'
	exit 1
    fi

# Test a passive connection: EPSV and IPv4.
#
echo "EPSV to $TARGET (IPv4) using inetd."
cat <<STOP |
rstatus
epsv4
dir
STOP
HOME=$TMPDIR $FTP "$TARGET" $PORT -4 -v -p -t >$TMPDIR/ftp.stdout 2>&1

test_report $? "$TMPDIR/ftp.stdout" "EPSV/$TARGET"

# Test a passive connection: EPSV and IPv4.
#
# Set NETRC in environment to regulate login.
#
echo "EPSV to $TARGET (IPv4) using inetd, setting NETRC."
cat <<STOP |
rstatus
epsv4
dir
`$do_transfer && test -n "$DLDIR" && echo "\
cd $DLDIR"`
`$do_transfer && echo "\
lcd $TMPDIR
image
put $GETME $PUTME"`
STOP

NETRC=$TMPDIR/.netrc \
  $FTP "$TARGET" $PORT -4 -v -p -t >$TMPDIR/ftp.stdout 2>&1

test_report $? "$TMPDIR/ftp.stdout" "EPSV/$TARGET with NETRC"

$do_transfer && \
    if cmp -s "$TMPDIR/$GETME" "$FTPHOME$DLDIR/$PUTME"; then
	test "${VERBOSE+yes}" && echo >&2 'Binary transfer succeeded.'
	date "+%s" >> "$TMPDIR/$GETME"
    else
	echo >&2 'Binary transfer failed.'
	exit 1
    fi

# Test an active connection: EPRT and IPv4.
#
echo "EPRT to $TARGET (IPv4) using inetd."
cat <<STOP |
rstatus
epsv4
dir
`$do_transfer && test -n "$DLDIR" && echo "\
cd $DLDIR"`
`$do_transfer && echo "\
lcd $TMPDIR
image
put $GETME $PUTME"`
STOP
HOME=$TMPDIR $FTP "$TARGET" $PORT -4 -v -t >$TMPDIR/ftp.stdout 2>&1

test_report $? "$TMPDIR/ftp.stdout" "EPRT/$TARGET"

$do_transfer && \
    if cmp -s "$TMPDIR/$GETME" "$FTPHOME$DLDIR/$PUTME"; then
	test "${VERBOSE+yes}" && echo >&2 'Binary transfer succeeded.'
	date "+%s" >> "$TMPDIR/$GETME"
    else
	echo >&2 'Binary transfer failed.'
	exit 1
    fi

# Test an active connection: EPRT and IPv4.
#
# Use `-N' to set location of .netrc file.
#
echo "EPRT to $TARGET (IPv4) using inetd, apply the switch -N."
cat <<STOP |
rstatus
epsv4
dir
STOP

$FTP "$TARGET" $PORT -N"$TMPDIR/.netrc" -4 -v -t >$TMPDIR/ftp.stdout 2>&1

test_report $? "$TMPDIR/ftp.stdout" "EPRT/$TARGET"

if test "$TEST_IPV6" != "no" && test -n "$TARGET6"; then
    # Test a passive connection: EPSV and IPv6.
    #
    echo "EPSV to $TARGET6 (IPv6) using inetd."
    cat <<-STOP |
	rstatus
	dir
	STOP
    HOME=$TMPDIR $FTP "$TARGET6" $PORT -6 -v -p -t >$TMPDIR/ftp.stdout 2>&1

    test_report $? "$TMPDIR/ftp.stdout" "EPSV/$TARGET6"

    # Test an active connection: EPRT and IPv6.
    #
    echo "EPRT to $TARGET6 (IPv6) using inetd."
    cat <<-STOP |
	rstatus
	dir
	`$do_transfer && test -n "$DLDIR" && echo "\
cd $DLDIR"`
	`$do_transfer && echo "\
lcd $TMPDIR
image
put $GETME $PUTME"`
	STOP
    HOME=$TMPDIR $FTP "$TARGET6" $PORT -6 -v -t >$TMPDIR/ftp.stdout 2>&1

    test_report $? "$TMPDIR/ftp.stdout" "EPRT/$TARGET6"

    $do_transfer && \
    if cmp -s "$TMPDIR/$GETME" "$FTPHOME$DLDIR/$PUTME"; then
	test "${VERBOSE+yes}" && echo >&2 'Binary transfer succeeded.'
	date "+%s" >> "$TMPDIR/$GETME"
    else
	echo >&2 'Binary transfer failed.'
	exit 1
    fi
fi # TEST_IPV6

# Availability of IPv4-mapped IPv6 addresses.
#
# These are impossible on OpenBSD, so a flexible test
# is implemented using sysctl(1) as tool.

# Helpers tp catch relevant execution path.
have_sysctl=false
have_address_mapping=false

# Known exceptions:
#
# OpenSolaris is known to allow address mapping
test `uname -s` = 'SunOS' && have_address_mapping=true

if $have_address_mapping; then
    :
else
    # Do we have sysctl(1) available?
    if sysctl -a >/dev/null 2>&1; then
	have_sysctl=true
    else
	echo "Warning: Not testing IPv4-mapped addresses." >&2
    fi
fi

if $have_address_mapping; then
    :
elif $have_sysctl; then
    # Extract the present setting of
    #
    #    net.ipv6.bindv6only (Linux)
    # or
    #    net.inet6.ip6.v6only (BSD).
    #
    value_v6only=`sysctl -a 2>/dev/null | $GREP v6only`
    if test -n "$value_v6only"; then
	value_v6only=`echo $value_v6only | $SED 's/^.*[=:] *//'`
	if test "$value_v6only" -eq 0; then
	    # This is the good value.  Keep it.
	    have_address_mapping=true
	else
	    echo "Warning: Address mapping IPv4-to-Ipv6 is disabled." >&2
	    # Set a non-zero value for later testing.
	    value_v6only=2
	fi
    else
	# Simulate a non-mapping answer in cases where "v6only" missed.
	value_v6only=2
    fi
fi

# Test functionality of IPv4-mapped IPv6 addresses.
#
if $have_address_mapping && test -n "$TARGET46" &&
   test "$TEST_IPV6" != "no"; then
    # Test a passive connection: EPSV and IPv4-mapped-IPv6.
    #
    echo "EPSV to $TARGET46 (IPv4-as-IPv6) using inetd."
    cat <<-STOP |
	rstatus
	dir
	`$do_transfer && test -n "$DLDIR" && echo "\
cd $DLDIR"`
	`$do_transfer && echo "\
lcd $TMPDIR
image
put $GETME $PUTME"`
	STOP
    HOME=$TMPDIR $FTP "$TARGET46" $PORT -6 -v -p -t >$TMPDIR/ftp.stdout 2>&1

    test_report $? "$TMPDIR/ftp.stdout" "EPSV/$TARGET46"

    $do_transfer && \
	if cmp -s "$TMPDIR/$GETME" "$FTPHOME$DLDIR/$PUTME"; then
	    test "${VERBOSE+yes}" && echo >&2 'Binary transfer succeeded.'
	    date "+%s" >> "$TMPDIR/$GETME"
	else
	    echo >&2 'Binary transfer failed.'
	    exit 1
	fi

    # Test an active connection: EPRT and IPvIPv6.
    #
    echo "EPRT to $TARGET46 (IPv4-as-IPv6) using inetd."
    cat <<-STOP |
	rstatus
	dir
	`$do_transfer && test -n "$DLDIR" && echo "\
cd $DLDIR"`
	`$do_transfer && echo "\
lcd $TMPDIR
image
put $GETME $PUTME"`
	STOP
    HOME=$TMPDIR $FTP "$TARGET46" $PORT -6 -v -t >$TMPDIR/ftp.stdout 2>&1

    test_report $? "$TMPDIR/ftp.stdout" "EPRT/$TARGET46"

    $do_transfer && \
	if cmp -s "$TMPDIR/$GETME" "$FTPHOME$DLDIR/$PUTME"; then
	    test "${VERBOSE+yes}" && echo >&2 'Binary transfer succeeded.'
	else
	    echo >&2 'Binary transfer failed.'
	    exit 1
	fi
else
    # The IPv4-as-IPv6 tests were not performed.
    echo 'Skipping two tests of IPv4 mapped as IPv6.'
fi # have_address_mapping && TEST_IPV6

# Test name mapping with PASV and IPv4.
# Needs a writable destination!
#
if $do_transfer; then
    echo "Name mapping test at $TARGET (IPv4) using inetd."

    cat <<-STOP |
	`test -z "$DLDIR" || echo "cd $DLDIR"`
	lcd $TMPDIR
	image
	nmap \$1.\$2 \$2.\$1
	put $GETME
	nmap \$1.\$2.\$3 [\$3,copy].\$1.\$2
	put $GETME
	STOP
    HOME=$TMPDIR $FTP "$TARGET" $PORT -4 -v -p -t >$TMPDIR/ftp.stdout 2>&1

    sIFS=$IFS
    IFS=.
    set -- $GETME
    IFS=$sIFS

    # Are the expected file copies present?

    if test -s $FTPHOME$DLDIR/$2.$1 && \
	test -s $FTPHOME$DLDIR/copy.$GETME
    then
	test "${VERBOSE+yes}" && echo >&2 'Name mapping succeeded.'
	rm -f $FTPHOME$DLDIR/$2.$1 $FTPHOME$DLDIR/copy.$GETME
    else
	echo >&2 'Binary transfer failed.'
	test -s $FTPHOME$DLDIR/$2.$1 || \
	    echo >&2 'Mapping "nmap $1.$2 $2.$1" failed.'
	test -s $FTPHOME$DLDIR/copy.$GETME || \
	    echo >&2 'Mapping "nmap $1.$2.$3 [$3,copy].$1.$2" failed.'
	rm -f $FTPHOME$DLDIR/$2.$1 $FTPHOME$DLDIR/copy.$GETME
	exit 1
    fi
fi

exit 0
