#!/bin/sh

# Copyright (C) 2009, 2010, 2011, 2012, 2013, 2014, 2015 Free Software
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

# Prerequisites:
#
#  * Shell: SVR3 Bourne shell, or newer.
#
#  * id(1).

. ./tools.sh

PING=${PING:-../ping/ping$EXEEXT}
TARGET=${TARGET:-127.0.0.1}

PING6=${PING6:-../ping/ping6$EXEEXT}
TARGET6=${TARGET6:-::1}

if [ ! -x $PING ]; then
    echo 'No executable "'$PING'" available.  Skipping test.' >&2
    exit 77
fi

if [ $VERBOSE ]; then
    set -x
    $PING --version
fi

if [ `func_id_uid` != 0 ]; then
    echo "ping needs to run as root"
    exit 77
fi

errno=0
errno2=0

$PING -n -c 1 $TARGET || errno=$?
test $errno -eq 0 || echo "Failed at pinging $TARGET." >&2

# Host might not have been built with IPv6 support.
test "$TEST_IPV6" != "no" && test -x $PING6 &&
    { $PING6 -n -c 1 $TARGET6 || errno2=$?; }

test $errno2 -eq 0 || echo "Failed at pinging $TARGET6." >&2

test $errno -eq 0 || exit $errno

exit $errno2
