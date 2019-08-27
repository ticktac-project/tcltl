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

#include "config.h"
#include "progname.h"
#include "argp.h"
#include "closeout.h"
#include "error.h"

#include <spot/twaalgos/dot.hh>
#include <spot/tl/parse.hh>
#include <spot/tl/print.hh>
#include <spot/twaalgos/translate.hh>
#include <spot/twaalgos/emptiness.hh>

#include "tcltl.hh"

static const char argp_program_doc[] ="\
Check a timed-automaton against an LTL formula.\v\
Exit status:\n\
  0  on success, or if the formula was verified\n\
  1  if the formula was violated (counter example found)\n\
  2  if any error has been reported";

// argp's default behavior of offering -? for --help is just too silly.
// We disable this option as well as -V (because --version doesn't need
// a short version).
enum {
  OPT_HELP = 1,
  OPT_VARS,
  OPT_VERSION,
};

static const argp_option options[] =
  {
    { nullptr, 0, nullptr, 0, "Input:", 1 },
    { "model", 'm', "FILENAME", 0,
      "read the timed-automaton model in FILENAME (TChecker's syntax)", 0 },
    { "formula", 'f', "FORMULA", 0,
      "check the LTL on the model (Spot's syntax)", 0 },
    { nullptr, 0, nullptr, 0, "Output:", 2 },
    { "quiet", 'q', nullptr, 0,
      "suppress standard output (check exit code for result)", 0 },
    { "dot", 'd', nullptr, 0,
      "output the result in GraphViz format" },
    { "vars", OPT_VARS, nullptr, 0,
      "list variables in the model and exit", 0 },
    { nullptr, 0, nullptr, 0, "Miscellaneous options:", -1 },
    { "version", OPT_VERSION, nullptr, 0, "print program version", 0 },
    { "help", OPT_HELP, nullptr, 0, "print this help", 0 },
    // We support --usage as a synonym for --help because argp's
    // hardcoded error message for unknown options mentions it.
    { "usage", OPT_HELP, nullptr, OPTION_HIDDEN, nullptr, 0 },
    { nullptr, 0, nullptr, 0, nullptr, 0 }
  };

static void
display_version(FILE *stream, struct argp_state*)
{
  fputs(program_name, stream);
  fputs(" " PACKAGE_VERSION "\n\
\n\
Copyright (C) 2019  Laboratoire de Recherche et Développement de l'Epita.\n\
License GPLv3+: \
GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>.\n\
This is free software: you are free to change and redistribute it.\n\
There is NO WARRANTY, to the extent permitted by law.\n", stream);
}


enum output_type_t { OUTPUT_STD, OUTPUT_DOT, OUTPUT_QUIET, OUTPUT_VARS };
static output_type_t output_type = OUTPUT_STD;
static std::string input_formula;
static spot::formula formula_neg;
static std::string model_filename;

static void parse_formula(std::string f)
{
  input_formula = f;
  spot::parsed_formula pf = spot::parse_infix_psl(f);
  if (pf.format_errors(std::cerr))
    error(1, 0, "error parsing formula");
  formula_neg = spot::formula::Not(pf.f);
}

static int
parse_opt(int key, char* arg, struct argp_state* state)
{
  // This switch is alphabetically-ordered.
  switch (key)
    {
    case 'd':
      output_type = OUTPUT_DOT;
      break;
    case 'f':
      parse_formula(arg);
      break;
    case 'm':
      model_filename = arg;
      break;
    case 'q':
      output_type = OUTPUT_QUIET;
      break;
    case OPT_HELP:
      argp_state_help(state, state->out_stream,
                      // Do not let argp exit: we want to diagnose a
                      // failure to print --help by closing stdout
                      // properly.
                      ARGP_HELP_STD_HELP & ~ARGP_HELP_EXIT_OK);
      close_stdout();
      exit(0);
      break;
    case OPT_VARS:
      output_type = OUTPUT_VARS;
      break;
    case OPT_VERSION:
      display_version(state->out_stream, state);
      close_stdout();
      exit(0);
      break;
    case ARGP_KEY_ARG:
      if (model_filename.empty())
        model_filename = arg;
      else if (input_formula.empty())
        parse_formula(arg);
      else
        error(2, 0, "too many arguments: %s", arg);
      break;
    default:
      return ARGP_ERR_UNKNOWN;
    }
  return 0;
}

static int run()
{
  auto dict = spot::make_bdd_dict();
  tchecker::gc_t gc;
  tc_model m = tc_model::load(model_filename);

  if (!formula_neg
      && output_type != OUTPUT_VARS
      && output_type != OUTPUT_DOT)
    {
      std::cout << "No LTL formula specified.\n";
      output_type = OUTPUT_VARS;
    }

  if (output_type == OUTPUT_VARS)
    {
      m.dump_info(std::cout);
      return 0;
    }

  if (!formula_neg && output_type == OUTPUT_DOT)
    {
      spot::atomic_prop_set ap;
      auto k = m.kripke(gc, &ap, dict);
      gc.start();
      k->set_named_prop("automaton-name", new std::string(model_filename));
      spot::print_dot(std::cout, k, ".kvA");
      gc.stop();
      return 0;
    }

  spot::twa_graph_ptr af = spot::translator(dict).run(formula_neg);
  spot::atomic_prop_set ap;
  spot::atomic_prop_collect(formula_neg, &ap);
  spot::twa_ptr k = m.kripke(gc, &ap, dict);
  gc.start();
  if (output_type == OUTPUT_DOT)
    {
      auto oldk = k;
      k = spot::make_twa_graph(k, spot::twa::prop_set::all(), true);
      // Make sure we stop gc before oldk is destroyed.
      gc.stop();
    }
  int exit_code = 0;
  auto run = k->intersecting_run(af);
  exit_code = !!run;
  switch (output_type)
    {
    case OUTPUT_STD:
      if (run)
        std::cout
          << "formula is violated by the following run:\n" << *run;
      else
        std::cout << "formula is satisfied\n";
      break;
    case OUTPUT_QUIET:
      break;
    case OUTPUT_DOT:
      if (run)
        {
          run->highlight(5);
          k->set_named_prop("automaton-name",
                            new std::string(model_filename +
                                            "\ncounterexample for "
                                            + input_formula));
        }
      else
        {
          k->set_named_prop("automaton-name",
                            new std::string(model_filename +
                                            "\nsatisfies "
                                            + input_formula));
        }
      spot::print_dot(std::cout, k, ".kvAn");
      break;
    case OUTPUT_VARS:
      /* unreachable */
      break;
    }
  if (output_type != OUTPUT_DOT)
    gc.stop();
  return exit_code;
}



int main(int argc, char * argv[])
{
  argp_program_bug_address = "<" PACKAGE_BUGREPORT ">";
  // Simplify the program name, because argp() uses it to report
  // errors and display help text.
  set_program_name(argv[0]);
  argv[0] = const_cast<char*>(program_name);
  argp_program_version_hook = display_version;
  argp_err_exit_status = 2;

  const argp ap = { options, parse_opt, "[FILENAME] [FORMULA]",
                    argp_program_doc, nullptr, nullptr, nullptr };
  if (int err = argp_parse(&ap, argc, argv, ARGP_NO_HELP, nullptr, nullptr))
    exit(err);

  int exit_code = 0;
  try {
    exit_code = run();
  }
  catch (const std::exception& e) {
    error(2, 0, e.what());
  }

  // Make sure we abort if we can't write to std::cout anymore
  // (like disk full or broken pipe with SIGPIPE ignored).
  std::cout.flush();
  if (!std::cout)
    error(2, 0, "error writing to standard output");

  return exit_code;
}
