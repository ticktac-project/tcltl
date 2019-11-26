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

#pragma once

#include <string>

#include <spot/tl/apcollect.hh>
#include <spot/kripke/kripke.hh>
#include <spot/tl/formula.hh>

#ifdef TCLTL_BUILD
  #define TCLTL_API SPOT_HELPER_DLL_EXPORT
#else
  #define TCLTL_API SPOT_HELPER_DLL_IMPORT
#endif

struct tc_model_details;
typedef std::shared_ptr<tc_model_details> tc_model_details_ptr;

enum zg_zone_semantics
  {
   elapsed_no_extrapolation,
   elapsed_extraLU_global,
   elapsed_extraLU_local,
   elapsed_extraLUplus_global,
   elapsed_extraLUplus_local,
   elapsed_extraM_global,
   elapsed_extraM_local,
   elapsed_extraMplus_global,
   elapsed_extraMplus_local,
   non_elapsed_no_extrapolation,
   non_elapsed_extraLU_global,
   non_elapsed_extraLU_local,
   non_elapsed_extraLUplus_global,
   non_elapsed_extraLUplus_local,
   non_elapsed_extraM_global,
   non_elapsed_extraM_local,
   non_elapsed_extraMplus_global,
   non_elapsed_extraMplus_local,
  };


class TCLTL_API tc_model final
{
private:
  tc_model_details_ptr priv_;
  tc_model(tc_model_details*);
public:
  // Load a TChecker model.
  //
  // This will throw an exception on error.
  static tc_model load(const std::string filename);


  // Return any warnings that was output while instantiating the
  // model.  Calling this function will clear the logs.
  std::string get_logs() const;

  // Display the list of variables we can use on this model.
  void dump_info(std::ostream& out) const;

  // Create a Kripke structure from the model.
  //
  // The dead parameter is used to control the behavior of the model
  // on dead states (i.e. the final states of finite sequences).  If
  // DEAD is formula::ff(), it means we are not interested in finite
  // sequences of the system, and dead state will have no successor.
  // If DEAD is formula::tt(), we want to check finite sequences as
  // well as infinite sequences, but do not need to distinguish
  // them.  In that case dead state will have a loop labeled by
  // true.  If DEAD is any atomic proposition (formula::ap("...")),
  // this is the name a property that should be true when looping on
  // a dead state, and false otherwise.
  //
  // This function returns nullptr on error.
  //
  // \a to_observe the list of atomic propositions that should be observed
  //               in the model
  // \a dict the BDD dictionary to use
  // \a dead an atomic proposition or constant to use for looping on
  //         dead states
  // \a zone_sem the zone semantics that TChecker should use
  spot::kripke_ptr kripke(const spot::atomic_prop_set* to_observe,
                          spot::bdd_dict_ptr dict,
                          spot::formula dead = spot::formula::tt(),
                          zg_zone_semantics zone_sem =
                          elapsed_extraLUplus_local);
};
