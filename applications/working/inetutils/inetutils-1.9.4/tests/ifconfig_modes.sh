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

# Check invocation modes of ifconfig.
#
# Written by Mats Erik Andersson.

usage () {
  cat <<HERE
Checking of invocation modes for ifconfig.
Never use this script on production systems,
or at least, use a system where \$IFACE names
an interface without all kind of connectivity.

The following environment variables are used:

IFACE		Interface to examine. (mandatory)
IFCONFIG	Location of executable.  Override
		manually whenever needed.
VERBOSE		Verbose commenting, whenever set.

HERE
}

# Is a usage explanation asked for?
#
if test "$1" = "-h" || test "$1" = "--help" || test "$1" = "--usage"; then
  usage
  exit 0
fi

set -u

: ${EXEEXT:=}

silence=
if test -z "${VERBOSE+set}"; then
  silence=:
fi

# Step into `tests/', should the call have
# been made in source or build root.
#
##test -d tests && test -f "tests/${0##*/}" && cd tests/
test -d tests/ && cd tests/

# Executable under test.
#
IFCONFIG=${IFCONFIG:-../ifconfig/ifconfig$EXEEXT}

test -x $IFCONFIG ||
  { echo >&2 'No executable available for "ifconfig".'; exit 1; }

test -n "${IFACE:+set}" ||
  { echo >&2 'No interface is assigned to IFACE.';
    echo >&2; usage >&2; exit 1;
  }

# Exclude all access to the loopback device.
if test `expr "$IFACE" : 'lo'` -gt 0; then
  { echo >&2 'This interface, IFACE="'$IFACE'", might be a loopback device.'
    echo >&2 'Execution denied!'
    exit 1
  }
fi

# Make sure the interface exists.
$IFCONFIG --interface=$IFACE --format=check-existence || exit 1

# Two alternate setups.
#
# Observe that the broadcast addresses violate the netmask.
# Some systems will amend them at repeated calls to ifconfig:
#
#  1.2.2.255/23 --> 1.2.3.255
#
#  2.3.6.255/22 --> 2.3.7.255
#
ADDRESS1=1.2.3.4
ADDRESS1_PATTERN='1\.2\.3\.4'
ADDRESS2=2.3.4.5
ADDRESS2_PATTERN='2\.3\.4\.5'
BRDADDR1=1.2.2.255
BRDADDR1_PATTERN='1\.2\.2\.255'
BRDADDR2=2.3.4.255
BRDADDR2_PATTERN='2\.3\.4\.255'
NETMASK1=255.255.254.0
NETMASK1_HEX=0xfffffe00
NETMASK1_PATTERN='255\.255\.254\.0'
NETMASK2=255.255.252.0
NETMASK2_HEX=0xfffffc00
NETMASK2_PATTERN='255\.255\.252\.0'
METRIC=7
MTU=1324

get_gnu_output () {
  $IFCONFIG --interface=$IFACE --format=gnu
} # get_gnu_output

make_setup () {
  # First alternative.
  ADDRESS=$ADDRESS1
  ADDRESS_PATTERN=$ADDRESS1_PATTERN
  BRDADDR=$BRDADDR1
  BRDADDR_PATTERN=$BRDADDR1_PATTERN
  NETMASK=$NETMASK1
  NETMASK_HEX=$NETMASK1_HEX
  NETMASK_PATTERN=$NETMASK1_PATTERN
  METRIC=`expr $METRIC + 1`
  MTU=`expr $MTU + 10`

  # Change setup whenever the address is in use.
  get_gnu_output |
    grep "inet address  $ADDRESS1_PATTERN" >/dev/null 2>&1 &&
      ADDRESS=$ADDRESS2 ADDRESS_PATTERN=$ADDRESS2_PATTERN \
      BRDADDR=$BRDADDR2 BRDADDR_PATTERN=$BRDADDR2_PATTERN \
      NETMASK=$NETMASK2 NETMASK_HEX=$NETMASK2_HEX \
      NETMASK_PATTERN=$NETMASK2_PATTERN \
      METRIC=`expr $METRIC + 1` MTU=`expr $MTU + 10`
} # make_setup

make_setup

#
# Counter of failures.
#
STATUS=0

increase_status () {
  STATUS=`expr $STATUS + 1`
}

check_output () {
  echo "$OUTPUT" |
  grep "$1" > /dev/null || { echo >&2 "$2"; increase_status; }
}

#
# For consistency and repeatability, first bring the interface down.
#
$silence cat <<-HERE
	** Bring interface down with a lone switch:
	**   ifconfig -i $IFACE --down
	HERE

$IFCONFIG -i $IFACE --down >/dev/null
OUTPUT=`get_gnu_output`

echo "$OUTPUT" | grep flags |
  grep -v "flags         UP" >/dev/null ||
    { echo >&2 'Failed to bring interface down with switch.'
      increase_status
    }

#
# Check setting of address, netmask, and broadcast
# address using switches.
#
$silence cat <<-HERE
	** Set address, netmask, and broadcast using switches:
	**	--address=$ADDRESS --netmask=$NETMASK --broadcast=$BRDADDR
	HERE

$IFCONFIG --interface=$IFACE --address=$ADDRESS \
	  --netmask=$NETMASK --broadcast=$BRDADDR
OUTPUT=`get_gnu_output`

check_output	"inet address  $ADDRESS_PATTERN" \
		'Failed to set address with "--address".'

check_output	"broadcast     $BRDADDR_PATTERN" \
		'Failed to set broadcast address with "--broadcast".'

check_output	"netmask       $NETMASK_PATTERN" \
		'Failed to set netmask with "--netmask".'

check_output	"flags         UP" \
		'Failed to bring interface up with address/netmask/broadcast.'

#
# Diagnose a failure to implicitly bring the interface up.
#
echo "$OUTPUT" |
  grep "flags         UP" >/dev/null 2>&1 ||
    {
      $silence cat <<-HERE
	** Set only address and explicit 'up' switches:
	**	--interface=$IFACE --address=$ADDRESS --up
	HERE

      $IFCONFIG --interface=$IFACE --address=$ADDRESS --up >/dev/null
      OUTPUT=`get_gnu_output`

      check_output	"flags         UP" \
			'Failed to bring interface up with only address.'

      # As a last resort, try a single imperative 'up'.
      echo "$OUTPUT" |
	grep "flags         UP" >/dev/null 2>&1 ||
	  {
	    $silence cat <<-HERE
		** Apply a single switch:
		**	--interface=$IFACE --up
		HERE

	    $IFCONFIG --interface=$IFACE --up >/dev/null
	    OUTPUT=`get_gnu_output`

	    if echo "$OUTPUT" | grep "flags         UP" >/dev/null 2>&1
	    then
	      echo >&2 '!!! This platform needs "--up" in splendid isolation.'
	    else
	      echo >&2 'Failed to bring interface up with a single "--up".'
	      STATUS=`expr $STATUS + 1`
	    fi
	  }
    }

#
# Assign metric and MTU using switches.
#
$silence cat <<-HERE
	** Set metric and MTU using switches:
	**	--metric=$METRIC --mtu=$MTU
	HERE

$IFCONFIG --interface=$IFACE --metric=$METRIC --mtu=$MTU
OUTPUT=`get_gnu_output`

check_output "metric        $METRIC" \
	     'Failed to set metric with "--metric".'

check_output "mtu           $MTU" \
	     'Failed to set MTU with "--mtu".'

#
# Bring the adapter down, and then up again.
# The previous actions contain an implicit
# switch '--up', as do standard implementations.
#
$silence cat <<-HERE
	** Bring interface down, then up again.
	**	--down, --up
	HERE

$IFCONFIG --interface=$IFACE --down >/dev/null
OUTPUT=`get_gnu_output`

# The absence of the flag UP indicates success.
echo "$OUTPUT" | grep flags |
  grep -v "flags         UP" >/dev/null ||
    { echo >&2 'Failed to bring interface down with "--down".'
      increase_status
    }

#
# And up again!
#
$IFCONFIG --interface=$IFACE --up >/dev/null
OUTPUT=`get_gnu_output`

check_output "flags         UP" \
	     'Failed to bring interface up with "--up".'

# Choose a second address collection.

make_setup

#
# Bring interface down on a parsed commandline.
#
$silence cat <<-HERE
	** Bring interface down on a parsed command line:
	**   ifconfig $IFACE down
	HERE

$IFCONFIG $IFACE down >/dev/null
OUTPUT=`get_gnu_output`

echo "$OUTPUT" | grep flags |
  grep -v "flags         UP" >/dev/null ||
    { echo >&2 'Failed to bring interface down with directive.'
      increase_status
    }

#
# Check setting of address, netmask, and broadcast
# address on a parsed command line.
#
$silence cat <<-HERE
	** Set address, netmask, and broadcast as arguments.
	**   ifconfig $IFACE $ADDRESS broadcast $BRDADDR netmask $NETMASK
	HERE

$IFCONFIG $IFACE $ADDRESS broadcast $BRDADDR netmask $NETMASK
OUTPUT=`get_gnu_output`

check_output	"inet address  $ADDRESS_PATTERN" \
		'Failed to set address as argument.'

check_output	"broadcast     $BRDADDR_PATTERN" \
		'Failed to set broadcast as argument.'

check_output	"netmask       $NETMASK_PATTERN" \
		'Failed to set netmask as argument.'

check_output	"flags         UP" \
		'Failed to bring interface up implicitly.'

#
# Diagnose this last failure.
#
echo "$OUTPUT" |
  grep "flags         UP" >/dev/null 2>&1 ||
    {
      $silence cat <<-HERE
	** Set only address and explicit 'up' switches:
	**   ifconfig $IFACE $ADDRESS up
	HERE

      $IFCONFIG $IFACE $ADDRESS up >/dev/null
      OUTPUT=`get_gnu_output`

      check_output	"flags         UP" \
			'Failed to bring interface up with only address.'

      # As a last resort, try a single imperative 'up'.
      echo "$OUTPUT" |
	grep "flags         UP" >/dev/null 2>&1 ||
	  {
	    $silence cat <<-HERE
		** Apply a single switch:
		**   ifconfig $IFACE up
		HERE

	    $IFCONFIG $IFACE up >/dev/null
	    OUTPUT=`get_gnu_output`

	    if echo "$OUTPUT" | grep "flags         UP" >/dev/null 2>&1
	    then
	      echo >&2 '!!! This platform needs specific "ifconfig '$IFACE' up".'
	    else
	      echo >&2 'Failed to bring interface up with a "ifconfig '$IFACE' up".'
	      STATUS=`expr $STATUS + 1`
	    fi
	  }
    }

#
# Assign metric and MTU on command line.
#
$silence cat <<-HERE
	** Set metric and MTU as parsed arguments:
	**   ifconfig $IFACE metric $METRIC mtu $MTU
	HERE

$IFCONFIG $IFACE metric $METRIC mtu $MTU
OUTPUT=`get_gnu_output`

check_output "metric        $METRIC" \
	     'Failed to set metric as argument.'

check_output "mtu           $MTU" \
	     'Failed to set MTU as argument.'

#
# Status summary
#
if test $STATUS -eq 0; then
  $silence echo 'Successful testing: '`uname -m -r -s`
else
  PLURAL=
  test $STATUS -le 1 || PLURAL=s
  echo 'There were '$STATUS' failure'$PLURAL': '`uname -m -r -s`
fi

exit $STATUS
