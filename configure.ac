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

AC_CONFIG_FILES([Makefile lib/Makefile])
AC_OUTPUT
