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

EXTRA_DIST = tools/gitlog-to-changelog

AM_CPPFLAGS = -I$(srcdir)/src
bin_PROGRAMS = tcltl
tcltl_SOURCES = src/tcltl.cc src/tcltl.hh
tcltl_LDADD = -llibtchecker -lspot -lbddx -lpthread


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
