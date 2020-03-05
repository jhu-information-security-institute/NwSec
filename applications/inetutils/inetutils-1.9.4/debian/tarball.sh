#!/bin/sh
#
# Copyright © 2004, 2005, 2007, 2009 Guillem Jover <guillem@debian.org>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#

set -e
set -u

pkg=inetutils
pkg_file=libinetutils

action=$1
shift

get_version()
{
  local d=$1

  $d/configure --version | sed -ne 's/-/./;s/GNU inetutils configure *//p'
}

echo "-> getting the source."
case "$action" in
snapshot)
  echo " -> making a git snapshot."

  snapshot_dir="$pkg-snapshot"

  if [ -d $snapshot_dir ]; then
    cd $snapshot_dir
    git pull
    cd ..
  else
    git clone https://git.savannah.gnu.org/git/inetutils.git $snapshot_dir
  fi

  echo " -> bootstrapping source tree form gnulib."
  cd $snapshot_dir
  ./bootstrap --copy --no-bootstrap-sync
  cd ..

  version="$(get_version $snapshot_dir)"
  ;;
tarball)
  echo " -> unpacking upstream tarball."

  upstream_dir="$pkg-tarball"
  upstream_tarball=$1

  mkdir $upstream_dir
  cd $upstream_dir
  tar xJf ../$upstream_tarball --strip 1
  cd ..

  version=$(get_version $upstream_dir)
  ;;
esac

tarball="${pkg}_${version}.orig.tar.xz"
tree="${pkg}-${version}"

echo "-> package ${pkg} version ${version}"

echo "-> filling the working tree."
case "$action" in
snapshot)
  cp -al $snapshot_dir $tree
  ;;
tarball)
  mv $upstream_dir $tree
  ;;
esac

if ! [ -e $tree/$pkg_file ]
then
  echo "error: no $pkg tree available."
  exit 1
fi

echo "-> cleaning tree."
# Clean non-free stuff
: nothing to clean

# Clean junk from CVS to git conversion
rm -rf $tree/Attic

# Clean bootstrap and autofoo stuff
rm -rf $tree/autom4te.cache
rm -rf $tree/gnulib

# Clean vcs stuff
find $tree -name 'CVS' -o -name '.cvsignore' -o \
           -name '.git' -o -name '.gitignore' | xargs rm -rf

echo "-> creating new tarball."
tar cJf $tarball $tree

echo "-> cleaning directory tree."
rm -rf $tree
