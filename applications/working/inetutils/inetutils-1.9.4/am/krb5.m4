dnl
dnl Copyright (C) 1996, 1997, 1998, 1999, 2000, 2001, 2002, 2003, 2004,
dnl 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012, 2013, 2014, 2015
dnl Free Software Foundation, Inc.
dnl
dnl This file is part of GNU Inetutils.
dnl
dnl GNU Inetutils is free software: you can redistribute it and/or modify
dnl it under the terms of the GNU General Public License as published by
dnl the Free Software Foundation, either version 3 of the License, or (at
dnl your option) any later version.
dnl
dnl GNU Inetutils is distributed in the hope that it will be useful, but
dnl WITHOUT ANY WARRANTY; without even the implied warranty of
dnl MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
dnl General Public License for more details.
dnl
dnl You should have received a copy of the GNU General Public License
dnl along with this program.  If not, see `http://www.gnu.org/licenses/'.

dnl Written by Miles Bader.

dnl IU_CHECK_KRB5(VERSION,PREFIX)
dnl Search for a Kerberos implementation in the standard locations
dnl plus PREFIX, if it is set and is not "yes".
dnl VERSION should be either 4 or 5.
dnl
dnl Defines KRB5_CFLAGS and KRB5_LIBS if found.
dnl Defines KRB5_IMPL to "krb5-config", "Heimdal", "MIT",
dnl "OpenBSD-Heimdal", or "OldMIT", if found, and to "none"
dnl otherwise.

AC_DEFUN([IU_CHECK_KRB5],
[
 if test "x$iu_cv_lib_krb5_libs" = x; then
  cache=""
  ## Make sure we have res_query
  AC_SEARCH_LIBS([res_query], [resolv])
  KRB5_PREFIX=[$2]
  KRB5_IMPL="none"
  # First try krb5-config
  if test "$KRB5_PREFIX" != "yes"; then
    krb5_path="$KRB5_PREFIX/bin"
  else
    krb5_path="$PATH"
  fi
  AC_PATH_PROG(KRB5CFGPATH, krb5-config, none, $krb5_path)
  if test "$KRB5CFGPATH" != "none"; then
    KRB5_CFLAGS="$CPPFLAGS `$KRB5CFGPATH --cflags krb$1`"
    KRB5_LIBS="$LDFLAGS `$KRB5CFGPATH --libs krb$1`"
    KRB5_IMPL="krb5-config"
  else
    ## OK, try the old code
    saved_CPPFLAGS="$CPPFLAGS"
    saved_LDFLAGS="$LDFLAGS"
    saved_LIBS="$LIBS"
    if test "$KRB5_PREFIX" != "yes"; then
      KRB5_CFLAGS="-I$KRB5_PREFIX/include"
      KRB5_LDFLAGS="-L$KRB5_PREFIX/lib"
      LDFLAGS="$LDFLAGS $KRB5_LDFLAGS"
    else
      ## A very common location in recent times.
      KRB5_CFLAGS="-I/usr/include/krb5"
    fi
    CPPFLAGS="$CPPFLAGS $KRB5_CFLAGS"
    KRB4_LIBS="-lkrb4 -ldes425"

    ## Check for new MIT kerberos V support
    LIBS="$saved_LIBS -lkrb5 -lk5crypto -lcom_err"
    AC_TRY_LINK([], [return krb5_init_context((void *) 0); ],
      [KRB5_IMPL="MIT"
       KRB5_LIBS="$KRB5_LDFLAGS -lkrb5 -lk5crypto -lcom_err"], )

    ## Heimdal kerberos V support
    if test "$KRB5_IMPL" = "none"; then
      LIBS="$saved_LIBS -lkrb5 -ldes -lasn1 -lroken -lcrypt -lcom_err"
      AC_TRY_LINK([], [return krb5_init_context((void *) 0); ],
        [KRB5_IMPL="Heimdal"
         KRB5_LIBS="$KRB5_LDFLAGS -lkrb5 -ldes -lasn1 -lroken -lcrypt -lcom_err"]
         , )
    fi

    ### FIXME: Implement a robust distinction between
    ### Heimdal a la OpenBSD and Old MIT Kerberos V.
    ### Presently the first will catch all, since it
    ### is readily available on contemporary systems.

    ## OpenBSD variant of Heimdal
    if test "$KRB5_IMPL" = "none"; then
      LIBS="$saved_LIBS -lkrb5 -lcrypto"
      AC_TRY_LINK([], [return krb5_init_context((void *) 0); ],
        [KRB5_IMPL="OpenBSD-Heimdal"
	 KRB5_CFLAGS="-I/usr/include/kerberosV"
         KRB5_LIBS="$KRB5_LDFLAGS -lkrb5 -lcrypto -lasn1 -ldes"], )
    fi

    ## Old MIT Kerberos V
    ## Note: older krb5 distributions use -lcrypto instead of
    ## -lk5crypto. This may conflict with OpenSSL.
    if test "$KRB5_IMPL" = "none"; then
      LIBS="$saved_LIBS -lkrb5 -lcrypto -lcom_err"
      AC_TRY_LINK([], [return krb5_init_context((void *) 0); ],
        [KRB5_IMPL="OldMIT"
         KRB5_LIBS="$KRB5_LDFLAGS $KRB4_LIBS -lkrb5 -lcrypto -lcom_err"], )
    fi

    LDFLAGS="$saved_LDFLAGS"
    LIBS="$saved_LIBS"
  fi

  iu_cv_lib_krb5_cflags="$KRB5_CFLAGS"
  iu_cv_lib_krb5_libs="$KRB5_LIBS"
  iu_cv_lib_krb5_impl="$KRB5_IMPL"
 else
  cached=" (cached) "
  KRB5_CFLAGS="$iu_cv_lib_krb5_cflags"
  KRB5_LIBS="$iu_cv_lib_krb5_libs"
  KRB5_IMPL="$iu_cv_lib_krb5_impl"
 fi
 AC_MSG_CHECKING(krb5 implementation)
 AC_MSG_RESULT(${cached}$KRB5_IMPL)
])
