#!/bin/sh

# Copyright (C) 2014, 2015 Free Software Foundation, Inc.
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

# Check response in libls.
# Very simple testing, aiming mostly at code coverage.

set -u

. ./tools.sh

: ${EXEEXT:=}
silence=
bucket=

# Executable under test.
#
LS=${LS:-./ls$EXEEXT}

if test ! -x "$LS"; then
    echo >&2 "Missing executable '$LS'.  Skipping test."
    exit 77
fi

if test -z "${VERBOSE+set}"; then
    silence=:
    bucket='>/dev/null'
fi

if test -n "${VERBOSE:+set}"; then
    set -x
fi

# IMPORTANT: Execute an initial call to $LS, just to get going.
# In case this is a coverage run, as NixOS does, this very first
# call will create `ls.gcda', whose creation would interfere with
# the counting after `$LS -a1' and `$LS -A1'.
#
$LS -alTt >/dev/null 2>&1

# Several runs with different switches are compared by
# a simple count of printed lines.
#
REPLY_a1=`$LS -a1`
REPLY_A1=`$LS -A1`

REPLY_C=`$LS -C`
REPLY_Cf=`$LS -Cf`
REPLY_Cr=`$LS -Cr`
REPLY_Ct=`$LS -Ct`
REPLY_x=`$LS -x`
REPLY_m=`$LS -m`

REPLY_l=`$LS -l`
REPLY_lT=`$LS -l`
REPLY_n=`$LS -n`

# In an attempt to counteract lack of subsecond accuracy,
# probe the parent directory where timing is known to be more
# varied, than in the subdirectory "tests/".
#
REPLY_Ccts=`$LS -Ccts ..`
REPLY_Cuts=`$LS -Cuts ..`

# All the following failure causes are checked and possibly
# brought to attention, independently of the other instances.
#
errno=0

diff=`{ echo "$REPLY_a1"; echo "$REPLY_A1"; } | sort | uniq -u`

test `echo "$diff" | wc -l` -eq 2 &&
test `echo "$diff" | grep -c -v -e '^\.\{1,2\}$'` -eq 0 ||
  { errno=1; echo >&2 'Failed to tell switch -a apart from -A.'
    # Attempt a diagnosis.
    if test -z "$diff"; then
      echo >&2 'Flags -a and -A produce identical lists.'
    else
      cat >&2 <<-EOT
	--- File list difference with '-a' and with '-A'. ---
	`echo "$diff" | $SED -e 's,^,    ,'`
	--- End of list ---
	EOT
    fi
  }

test x"$REPLY_C" != x"$REPLY_Cf" ||
  { errno=1; echo >&2 'Failed to disable sorting with "-f".'; }

test x"$REPLY_C" != x"$REPLY_Cr" ||
  { errno=1; echo >&2 'Failed to reverse sorting with "-r".'; }

test x"$REPLY_C" != x"$REPLY_Ct" ||
  { errno=1; echo >&2 'Failed to sort on modification with "-t".'; }

test x"$REPLY_C" != x"$REPLY_x" ||
  { errno=1; echo >&2 'Failed to distinguish "-C" from "-x".'; }

test x"$REPLY_C" != x"$REPLY_m" ||
  { errno=1; echo >&2 'Failed to distinguish "-C" from "-m".'; }

test x"$REPLY_l" != x"$REPLY_n" ||
  { errno=1; echo >&2 'Failed to distinguish "-l" from "-n".'; }

test x"$REPLY_Ccts" != x"$REPLY_Cuts" ||
  { errno=1; echo >&2 'Failed to distinguish "-u" from "-c".'; }

test $errno -ne 0 || $silence echo "Successful testing".

exit $errno
