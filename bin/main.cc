// -*- coding: utf-8 -*-
// Copyright (C) 2019 Laboratoire de Recherche et Développement
// de l'Epita (LRDE).
//
// This file is part of TCLTL, a model checker for timed-automata.
//
// TCLTL is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 3 of the License, or
// (at your option) any later version.
//
// TCLTL is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
// or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
// License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include <spot/twaalgos/dot.hh>
#include <spot/tl/parse.hh>
#include <spot/tl/print.hh>
#include <spot/twaalgos/translate.hh>
#include <spot/twaalgos/emptiness.hh>

#include "tcltl.hh"


int main(int argc, char * argv[])
{
  if (argc < 2 || argc > 4) {
    std::cerr << "Usage: " << argv[0] << " filename [formula] [-D]\n";
    return 1;
  }

  auto dict = spot::make_bdd_dict();

  spot::formula f;

  bool dot = false;
  if (argc > 1 && strncmp(argv[argc - 1], "-D", 3) == 0)
    {
      dot = 1;
      --argc;
    }

  std::string orig_formula;
  if (argc >= 3)
  {
    // Parse the input formula.
    orig_formula = argv[2];
    spot::parsed_formula pf = spot::parse_infix_psl(argv[2]);
    if (pf.format_errors(std::cerr))
      return 1;
    // Translate its negation.
    f = spot::formula::Not(pf.f);
  }

  int exit_code = 0;
  try {
    tchecker::gc_t gc;
    tc_model m = tc_model::load(std::string(argv[1]));
    if (f)
      {
        spot::twa_graph_ptr af = spot::translator(dict).run(f);
        spot::atomic_prop_set ap;
        spot::atomic_prop_collect(f, &ap);
        spot::twa_ptr k = m.kripke(gc, &ap, dict);
        if (dot)
          k = spot::make_twa_graph(k, spot::twa::prop_set::all(), true);
        gc.start();
        if (auto run = k->intersecting_run(af))
          {
            exit_code = 1;
            if (!dot)
              {
                std::cout
                  << "formula is violated by the following run:\n" << *run;
              }
            else
              {
                run->highlight(5);
                k->set_named_prop("automaton-name",
                                  new std::string(std::string(argv[1]) +
                                                  "\ncounterexample for "
                                                  + orig_formula));
                spot::print_dot(std::cout, k, ".kvAn");
              }
          }
        else
          {
            std::cout << "formula is verified\n";
          }
        gc.stop();
      }
    else
      {
        if (dot)
          {
            spot::atomic_prop_set ap;
            auto k = m.kripke(gc, &ap, dict);
            gc.start();
            k->set_named_prop("automaton-name", new std::string(argv[1]));
            spot::print_dot(std::cout, k, ".kvA");
            gc.stop();
          }
        else
          {
            m.dump_info(std::cout);
          }
      }
  }
  catch (const std::exception& e) {
    std::cerr << e.what();
    return 2;
  }
  return exit_code;
}
