// -*- coding: utf-8 -*-
// Copyright (C) 2019 Laboratoire de Recherche et Développement de
// l'Epita (LRDE).
//
// This file is part of Spot, a model checking library.
//
// Spot is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 3 of the License, or
// (at your option) any later version.
//
// Spot is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
// or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
// License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

%module(package="spot", director="1") tchecker

%include "std_string.i"
%include "exception.i"
%import(module="spot.impl") "std_set.i"
%include "std_shared_ptr.i"

%shared_ptr(spot::bdd_dict)
%shared_ptr(spot::twa)
%shared_ptr(spot::kripke)
%shared_ptr(spot::fair_kripke)

%{
#include <tcltl.hh>
%}

%import(module="spot.impl") <spot/misc/common.hh>
%import(module="spot.impl") <spot/twa/bdddict.hh>
%import(module="spot.impl") <spot/twa/twa.hh>
%import(module="spot.impl") <spot/tl/formula.hh>
%import(module="spot.impl") <spot/tl/apcollect.hh>
%import(module="spot.impl") <spot/kripke/fairkripke.hh>
%import(module="spot.impl") <spot/kripke/kripke.hh>

%exception {
  try {
    $action
  }
  catch (const spot::parse_error& e)
  {
    std::string er("\n");
    er += e.what();
    SWIG_Error(SWIG_SyntaxError, er.c_str());
  }
  catch (const std::runtime_error& e)
  {
    SWIG_exception(SWIG_RuntimeError, e.what());
  }
}

%rename(model) tc_model;
%rename(kripke_raw) tc_model::kripke;
%include <tcltl.hh>

%pythoncode %{
import spot
import spot.aux
import sys
import subprocess
import tempfile

def load(filename):
  """Load a TChecker model.

The argument is assumed to be a filename if it contains no newline.
Otherwise it is assumed to be the actual text of the model.
"""
  if '\n' in filename:
    # Assume it's an actual model.
    with spot.aux.tmpdir():
       with tempfile.NamedTemporaryFile(dir='.', suffix='.tc') as t:
           # See ticktac-project/tchecker#35
           if filename[-1] != '\n':
               filename += '\n'
           t.write(filename.encode('utf-8'))
           t.flush()
           return load(t.name)
  try:
    m = model.load(filename)
    logs = m.get_logs()
    if logs:
      print(logs, end='', file=sys.stderr)
  except RuntimeError as err:
    print(str(err), end='', file=sys.stderr)
    m = None
  return m

@spot._extend(model)
class model:
  def kripke(self, ap_set, dict=spot._bdd_dict,
             dead=spot.formula_ap('dead'),
             zone_sem=elapsed_extraLUplus_local):
    s = spot.atomic_prop_set()
    for ap in ap_set:
      s.insert(spot.formula_ap(ap))
    return self.kripke_raw(s, dict, dead, zone_sem)

  def __repr__(self):
    res = "tchecker model\n";
    ostr = spot.ostringstream()
    self.dump_info(ostr)
    return res + ostr.str()

# Load IPython specific support if we can.
try:
    # Load only if we are running IPython.
    __IPYTHON__

    from IPython.core.magic import Magics, magics_class, cell_magic

    @magics_class
    class EditTC(Magics):

        @cell_magic
        def tchecker(self, line, cell):
            if not line:
               raise ValueError("missing variable name for %%tchecker")
            # Make sure we have at least one newline so that
            # the cell is not interpreted as a filename
            if cell[-1] != '\n':
               cell += '\n'
            self.shell.user_ns[line] = load(cell)

    ip = get_ipython()
    ip.register_magics(EditTC)

except (ImportError, NameError):
    pass
%}
