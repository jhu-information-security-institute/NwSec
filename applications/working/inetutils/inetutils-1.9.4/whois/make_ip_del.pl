#!/usr/bin/perl -w

# Copyright (C) 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005, 2006,
# 2007, 2008, 2009, 2010, 2011, 2012, 2013, 2014, 2015 Free Software
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

use strict;

while (<>) {
	chomp;
	s/^\s*(.*)\s*$/$1/;
	s/\s*#.*$//;
	next if /^$/;
	die "format error: $_" unless (/^([\d\.]+)\/(\d+)\s+([\w\.]+)$/);
	my $m=$2; my $s=$3;
	my ($i1, $i2, $i3, $i4)=split(/\./, $1);
	print "{ ".(($i1<<24)+($i2<<16)+($i3<<8)+$i4)."UL, ".
		(0xffffffff^(0xffffffff>>$m))."UL, \"";
	if ($s =~ /\./) {
		print "$s";
	} else {
		print "whois.$s.net";
	}
	print "\" },\n";
}
