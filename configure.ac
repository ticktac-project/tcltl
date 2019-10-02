# -*- coding: utf-8 -*-
# Copyright (C) 2019, Laboratoire de Recherche et Développement
# de l'Epita (LRDE).
#
# This file is part of TCLTL, a model checker based on Tchecker and
# Spot.
#
# TCLTL is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.
#
# TCLTL is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
# License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

AC_PREREQ([2.63])
AC_INIT([tcltl], [0.1.dev], [adl@lrde.epita.fr])
AC_CONFIG_AUX_DIR([tools])
AC_CONFIG_MACRO_DIR([m4])
AM_INIT_AUTOMAKE([1.11 gnu tar-ustar color-tests parallel-tests subdir-objects])
AC_CONFIG_HEADERS([config.h])dnl Private config, not to be used in .hh files.

AC_PROG_CC
gl_EARLY
AM_PROG_CC_C_O
AC_PROG_CXX

# Decrease verbosity when passing argument V=0
AM_SILENT_RULES([no])

# Activate C11 for gnulib tests
AX_CHECK_COMPILE_FLAG([-std=c11], [CFLAGS="$CFLAGS -std=c11"])

gl_INIT

AX_CHECK_COMPILE_FLAG([-Werror -fvisibility=hidden],
  [CFLAGS="$CFLAGS -fvisibility=hidden"])
AC_LANG(C++)
AX_CHECK_COMPILE_FLAG([-Werror -fvisibility=hidden],
  [CXXFLAGS="$CXXFLAGS -fvisibility=hidden"
  AX_CHECK_COMPILE_FLAG([-fvisibility-inlines-hidden],
    [CXXFLAGS="$CXXFLAGS -fvisibility-inlines-hidden"])])
CXXFLAGS="$CXXFLAGS -DTCLTL_BUILD"

# Turn on C++14 support
m4_define([_AX_CXX_COMPILE_STDCXX_14_testbody],
[AC_LANG_SOURCE([#include <memory>
#include <string>
#include <chrono>  // used to fail in C++11 with some installation of clang
#include <map>

    template <typename T>
    struct check
    {
      static_assert(sizeof(int) <= sizeof(T), "not big enough");
    };

    typedef check<check<bool>> right_angle_brackets;

    auto f = std::make_unique<std::string>("uniq_ptr");

    int a;
    decltype(a) b;

    typedef check<int> check_type;
    check_type c;
    check_type&& cr = static_cast<check_type&&>(c);

    auto d = a;
])])

for f in -std=c++17 '-std=c++17 -stdlib=libc++' -std=c++1z
do
  AX_CHECK_COMPILE_FLAG([$f], [CXXFLAGS="$CXXFLAGS $f" stdpass=true], [], [],
                        [_AX_CXX_COMPILE_STDCXX_14_testbody])
  ${stdpass-false} && break
done
if ! "${stdpass-false}"; then
  AC_ERROR([unable to turn on C++17 mode with this compiler])
fi

LT_INIT([win32-dll])

AC_ARG_ENABLE([python],
              [AC_HELP_STRING([--disable-python],
                              [do not compile Python bindings])],
              [], [enable_python=yes])

case $enable_shared:$enable_python in
 no:no);;
 no:*)
    enable_python=no
    AC_MSG_NOTICE([Not building Python bindings because of --disable-shared.])
    ;;
esac

AM_CONDITIONAL([USE_PYTHON], [test "x${enable_python:-yes}" = xyes])

if test "x${enable_python:-yes}" = xyes; then
  AC_MSG_NOTICE([You may configure with --disable-python ]dnl
[if you do not need Python bindings.])
  adl_CHECK_PYTHON
fi

AC_ARG_WITH([spot],
  [AS_HELP_STRING([--with-spot=PREFIXDIR],
    [assume Spot has been installed in PREFIXDIR @<:@default to --prefix@:>@])],
  [], [with_spot='${prefix}'])
AC_SUBST([SPOTPREFIX], [$with_spot])

# Debian has a binary for SWIG 3.0 named swig3.0 and they kept swig as
# an alias for swig-2.0.  Let's use the former when available.
AC_CHECK_PROGS([SWIG], [swig3.0 swig], [swig])
AC_CHECK_PROG([PERL], [perl], [perl])

AC_CONFIG_FILES([Makefile lib/Makefile tests/defs])
AC_CONFIG_FILES([tests/run], [chmod +x tests/run])
AC_OUTPUT
