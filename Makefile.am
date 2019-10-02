## -*- coding: utf-8 -*-
## Copyright (C) 2019 Laboratoire de Recherche et Développement
## de l'Epita (LRDE).
##
## This file is part of TCLTL, a model checker for timed-automata.
##
## TCLTL is free software; you can redistribute it and/or modify it
## under the terms of the GNU General Public License as published by
## the Free Software Foundation; either version 3 of the License, or
## (at your option) any later version.
##
## Spot is distributed in the hope that it will be useful, but WITHOUT
## ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
## or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
## License for more details.
##
## You should have received a copy of the GNU General Public License
## along with this program.  If not, see <http://www.gnu.org/licenses/>.

SUBDIRS = lib .

EXTRA_DIST = tools/gitlog-to-changelog m4/gnulib-cache.m4 $(TESTS) \
	tests/ipnbdoctest.py

AM_CPPFLAGS = -I$(srcdir)/src

lib_LTLIBRARIES = src/libtcltl.la
src_libtcltl_la_SOURCES = src/tcltl.cc src/tcltl.hh

bin_PROGRAMS = bin/tcltl
bin_tcltl_SOURCES = bin/main.cc
bin_tcltl_LDADD = src/libtcltl.la lib/libgnu.a \
	-L$(SPOTPREFIX)/lib -lspot -lbddx -ltchecker -lpthread
bin_tcltl_CPPFLAGS = $(AM_CPPFLAGS) -Ilib -I$(top_srcdir)/lib



# The "spot.tchecker" Python module.
EXTRA_DIST += python/spot-extra/tchecker.i

if USE_PYTHON
tcpyexecdir = $(pyexecdir)/spot-extra
tcpyexec_PYTHON = python/spot-extra/tchecker.py
tcpyexec_LTLIBRARIES = python/spot-extra/_tchecker.la
endif

MAINTAINERCLEANFILES = python/spot/tchecker.cxx

## Automake 1.16 fails to remove the __pycache__/* files.
uninstall-local:
	rm -rf $(DESTDIR)$(pyexecdir)/spot-extra/__pycache__

python_spot_extra__tchecker_la_CPPFLAGS = \
   -I$(PYTHONINC) -I$(top_srcdir)/src -DSWIG_TYPE_TABLE=spot
python_spot_extra__tchecker_la_SOURCES = python/spot-extra/tchecker_wrap.cxx
python_spot_extra__tchecker_la_LDFLAGS = -avoid-version -module $(SYMBOLIC_LDFLAGS)
python_spot_extra__tchecker_la_LIBADD = \
   src/libtcltl.la -L$(SPOTPREFIX)/lib -lspot -lbddx -ltchecker -lpthread

SWIGFLAGS = -c++ -python -py3 -O -I$(SPOTPREFIX)/include -I$(top_srcdir)/src

${srcdir}/python/spot-extra/tchecker_wrap.cxx: python/spot-extra/tchecker.i
	$(SWIG) $(SWIGFLAGS) $(srcdir)/python/spot-extra/tchecker.i
## work around https://github.com/swig/swig/issues/1553
	$(PERL) -pi -e 's/^#define SWIG_Python_CallFunctor.*/#define SWIG_Python_CallFunctor(functor, obj) PyObject_CallFunctionObjArgs(functor, obj, NULL);/' $@


# Empty TEST_EXTENSIONS it to avoid default value ".test"
TEST_EXTENSIONS =
LOG_COMPILER = tests/run
check_SCRIPTS = tests/defs tests/run
TESTS = \
  tests/basic.test \
  tests/dead.test \
  tests/errcli.test \
  tests/errclout.test

if USE_PYTHON
TESTS += \
  tests/notebook.ipynb \
  tests/python.py
endif

# Remove the test directories created in by tests/defs
distclean-local:
	find . -name '*.dir' -type d -print | xargs rm -rf

# We write the commit message so that they can be used to build a
# ChangeLog file that we distribute, following the GNU conventions.
# Distributing the list of changes along with any modification of this
# project is also a requirement of the GPL.
.PHONY: gen-ChangeLog
dist-hook: gen-ChangeLog
gen-ChangeLog:
	if test -d .git; then						\
	    $(top_srcdir)/tools/gitlog-to-changelog			\
	      --no-cluster --format='%s%n%n%b%n' > $(distdir)/cl-t;	\
	  rm -f $(distdir)/ChangeLog;					\
	  mv $(distdir)/cl-t $(distdir)/ChangeLog;			\
       fi
