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


// A lot of code in this file is inspired from Spot's interface
// with LTSmin, as seen in Spot's spot/ltsmin/ltsmin.cc file.

#include <iostream>
#include <cassert>

#include <tchecker/parsing/parsing.hh>
#include <tchecker/utils/log.hh>
#include <tchecker/zg/zg_ta.hh>
#include <tchecker/ts/allocators.hh>
#include <tchecker/ts/builder.hh>

#include <spot/misc/fixpool.hh>

#include "tcltl.hh"


// prop_list encodes the list of atomic propositions we have to
// valuate on each state.  Since TChecker uses numbers to denote each
// variable and location, we store those numbers (var_num) to make
// the query fast.  The triplet var_num,op,val therefore encode a
// comparison between a variable and a value, or (if op=OP_AT) a check
// that process var_num is at location val.
//
// The bddvar is the BDD variable used to represent this atomic
// proposition in Spot's API.
//
// The actual evaluation of the prop_list is done in
// tcltl_kripke::state_condition().  The conversion (or
// "byte-compiling" if you prefer) from text to one_prop is
// done by convert_aps().
typedef enum { OP_EQ, OP_NE, OP_LT, OP_GT, OP_LE, OP_GE, OP_AT } relop;
struct one_prop
{
  int var_num;
  relop op;
  int val;
  int bddvar;           // if "var_num op val" is true, output bddvar,
                        // else its negation
};
typedef std::vector<one_prop> prop_list;

// Private member for the tc_model.  This is also shared with
// tcltl_kripke, in case the user decide to destroy tc_model before
// the tcltl_kripke generated by tc_model::kripke().
struct tc_model_details final
{
public:
  const tchecker::parsing::system_declaration_t* sysdecl;
  tchecker::zg::ta::model_t* model;

  tc_model_details(tchecker::parsing::system_declaration_t const * sys,
                  tchecker::zg::ta::model_t* m)
    : sysdecl(sys), model(m)
  {
  }

  ~tc_model_details()
  {
    delete model;
    delete sysdecl;
  }
};

// Spot wrapper around a TChercker shared_state_ptr_t.
//
// FIXME: The Spot wrapper is itself reference counted, so it makes
// little sense to store a shared_state_ptr_t (with a refcount of 1
// always) inside.  We should try to store a state_t, or inherit from
// state_t and see if we can reuse TChecker's allocators instead of
// the one of Spot.
template <typename STATE_PTR>
struct tcltl_state final: public spot::state
{
  tcltl_state(spot::fixed_size_pool* p, STATE_PTR zg)
    : pool_(p), hash_val_(hash_value(*zg)), count_(1), zg_state_(zg)
  {
  }

  tcltl_state* clone() const override
  {
    ++count_;
    return const_cast<tcltl_state*>(this);
  }

  void destroy() const override
  {
    if (--count_)
      return;
    this->~tcltl_state(); // this destroys zg_state_
    pool_->deallocate(const_cast<tcltl_state*>(this));
  }

  size_t hash() const override
  {
    return hash_val_;
  }

  int compare(const state* other) const override
  {
    if (this == other)
      return 0;
    const tcltl_state* o = spot::down_cast<const tcltl_state*>(other);
    if (hash_val_ < o->hash_val_)
      return -1;
    if (hash_val_ > o->hash_val_)
      return 1;
    // FIXME: We really want <, but tchecker does not have it.
    // https://github.com/ticktac-project/tchecker/issues/23
    return *zg_state() != *o->zg_state();
  }

  // It's important not to return a copy, because some TChecker
  // constructs (like its outgoing_iterators) will hold a reference on
  // this state.  If we return a temporary copy, the outgoing
  // iterators we build on this will have stale pointers.
  STATE_PTR& zg_state() const
  {
    return zg_state_;
  }
private:

  ~tcltl_state()
  {
  }

  spot::fixed_size_pool* pool_;
  unsigned hash_val_;
  mutable unsigned count_;
  mutable STATE_PTR zg_state_;
};


// This wrap the TChecker outgoing iterator (the ITERATOR type), but
// also provide the option to add a self-loop to states when the
// selfloop argument is given (in this case, the iterator is ignored).
//
// We could have separated these behavior into two classes that
// inherit from spot::kripke_succ_iterator (one for the normal
// wrapping of TChecker's iterator, another one for the looping case)
// but that makes recycling harder (by requiring a slow dynamic_cast
// for type checking).
template <typename ITERATOR>
class tcltl_succ_iterator final: public spot::kripke_succ_iterator
{
public:

  tcltl_succ_iterator(spot::fixed_size_pool& pool, ITERATOR start, bdd cond,
                      const spot::state* selfloop)
    : kripke_succ_iterator(cond), pool_(pool), start_(start), pos_(start),
      selfloop_(selfloop), done_(false)
  {
  }

  void recycle(ITERATOR start, bdd cond, const spot::state* selfloop)
  {
    kripke_succ_iterator::recycle(cond);
    start_ = start;
    pos_ = start;
    selfloop_ = selfloop;
    done_ = false;
  }

  ~tcltl_succ_iterator()
  {
    if (selfloop_)
      selfloop_->destroy();
  }

private:
  bool is_done() const
  {
    return selfloop_ ? done_ : pos_.at_end();
  }

public:
  virtual bool first() override
  {
    pos_ = start_;
    done_ = false;
    return !is_done();
  }

  virtual bool next() override
  {
    if (selfloop_)
      done_ = true;
    else
      ++pos_;
    return !is_done();
  }

  virtual bool done() const override
  {
    return is_done();
  }

  virtual spot::state* dst() const override
  {
    if (selfloop_)
      return selfloop_->clone();
    auto [st, trans] = *pos_;
    return new(pool_.allocate()) tcltl_state<decltype(st)>(&pool_, st);
  }

private:
  spot::fixed_size_pool& pool_;
  ITERATOR start_;
  ITERATOR pos_;
  const spot::state* selfloop_;
  bool done_;
};


template <typename ZONE>
class tcltl_kripke final: public spot::kripke
{
  using zg_t = ZONE;
  using state_t = typename zg_t::shared_state_t;
  using state_ptr_t = typename zg_t::shared_state_ptr_t;
  using state_allocator_t =
    typename zg_t::template state_pool_allocator_t<state_t>;
  using transition_allocator_t =
    typename zg_t::template transition_singleton_allocator_t
    <typename zg_t::transition_t>;
  using allocator_t =
    tchecker::ts::allocator_t<state_allocator_t, transition_allocator_t>;
  using builder_t =
    tchecker::ts::builder_ok_t<typename zg_t::ts_t, allocator_t>;
  using tcltl_succiter_t =
    tcltl_succ_iterator<typename builder_t::outgoing_iterator_t>;

  // Keep a shared pointer to the model and system so that they are
  // not deallocated before this Kripke structure.
  tc_model_details_ptr tcmd_;
  typename zg_t::ts_t ts_;
  mutable allocator_t allocator_;
  mutable builder_t builder_;
  const prop_list* ps_;
  bdd alive_prop;
  bdd dead_prop;
  mutable spot::fixed_size_pool statepool_;
public:

  tcltl_kripke(tchecker::gc_t& gc,
               tc_model_details_ptr tcmd,
               const spot::bdd_dict_ptr& dict,
               const prop_list* ps, spot::formula dead)
    : kripke(dict),
      tcmd_(tcmd),
      ts_(*tcmd->model),
      allocator_(gc, std::make_tuple(*tcmd->model, 100000), std::tuple<>()),
      builder_(ts_, allocator_),
      ps_(ps),
      statepool_(sizeof(tcltl_state<state_ptr_t>))
  {
    // Register the "dead" proposition.  There are three cases to
    // consider:
    //  * If DEAD is "false", it means we are not interested in finite
    //    sequences of the system.
    //  * If DEAD is "true", we want to check finite sequences as well
    //    as infinite sequences, but do not need to distinguish them.
    //  * If DEAD is any other string, this is the name a property
    //    that should be true when looping on a dead state, and false
    //    otherwise.
    // We handle these three cases by setting ALIVE_PROP and DEAD_PROP
    // appropriately.  ALIVE_PROP is the bdd that should be ANDed
    // to all transitions leaving a live state, while DEAD_PROP should
    // be ANDed to all transitions leaving a dead state.
    if (dead.is_ff())
      {
        alive_prop = bddtrue;
        dead_prop = bddfalse;
      }
    else if (dead.is_tt())
      {
        alive_prop = bddtrue;
        dead_prop = bddtrue;
      }
    else
      {
        int var = dict->register_proposition(dead, this);
        dead_prop = bdd_ithvar(var);
        alive_prop = bdd_nithvar(var);
      }
  }

  ~tcltl_kripke()
  {
    if (iter_cache_)
      {
        delete iter_cache_;
        iter_cache_ = nullptr;
      }
    dict_->unregister_all_my_variables(ps_);
    delete ps_;
  }

  virtual tcltl_state<state_ptr_t>* get_init_state() const override
  {
    tcltl_state<state_ptr_t>* res = nullptr;
    bool first = true;
    auto initial_range = builder_.initial();
    for (auto it = initial_range.begin(); ! it.at_end(); ++it)
      if (first)
        {
          state_ptr_t st;
          typename builder_t::transition_ptr_t trans;
          std::tie(st, trans) = *it;
          first = false;
          res = new(statepool_.allocate())
            tcltl_state<state_ptr_t>(&statepool_, st);
        }
      else
        {
          throw std::runtime_error("Multiple initial states not supported.");
        }
    return res;
  }

  virtual
  tcltl_succiter_t* succ_iter(const spot::state* st) const override
  {
    auto zs = spot::down_cast<const tcltl_state<state_ptr_t>*>(st);
    state_ptr_t& z = zs->zg_state();
    auto beg = builder_.outgoing(z).begin();
    bdd scond = state_condition(st);
    bool want_loop = false;
    if (!beg.at_end())
      {
        scond &= alive_prop;
      }
    else
      {
        scond &= dead_prop;
        want_loop = scond != bddfalse;
      }

    if (iter_cache_)
      {
        tcltl_succiter_t* it =
          spot::down_cast<tcltl_succiter_t*>(iter_cache_);
        it->recycle(beg, scond, want_loop ? st->clone() : nullptr);
        iter_cache_ = nullptr;
        return it;
      }
    return new tcltl_succiter_t(statepool_, beg, scond,
                                want_loop ? st->clone() : nullptr);
  }

  virtual
  bdd state_condition(const spot::state* st) const override
  {
    bdd cond = bddtrue;
    auto zs = spot::down_cast<const tcltl_state<state_ptr_t>*>(st)->zg_state();
    auto& vals = zs->intvars_valuation();
    auto& vloc = zs->vloc();
    for (const one_prop& prop: *ps_)
      {
        bool res = false;
        if (prop.op == OP_AT)
          {
            res = vloc[prop.var_num]->id() == unsigned(prop.val);
          }
        else
          {
            int val = vals[prop.var_num];
            int ref = prop.val;
            switch (prop.op)
              {
              case OP_EQ:
                res = val == ref;
                break;
              case OP_NE:
                res = val != ref;
                break;
              case OP_LT:
                res = val < ref;
                break;
              case OP_GT:
                res = val > ref;
                break;
              case OP_LE:
                res = val <= ref;
                break;
              case OP_GE:
                res = val >= ref;
                break;
              case OP_AT:
                // unreachable
                break;
              }
          }
        cond &= (res ? bdd_ithvar : bdd_nithvar)(prop.bddvar);
      }
    return cond;
  }

  virtual
  std::string format_state(const spot::state *st) const override
  {
    auto& model = ts_.model();
    auto zs = spot::down_cast<const tcltl_state<state_ptr_t>*>(st)->zg_state();
    tchecker::zg::ta::state_outputter_t
      so(model.system_integer_variables().index(),
         model.system_clock_variables().index());
    std::stringstream s;
    so.output(s, *zs);
    return s.str();
  }

};

// Convert a set of atomic propositions (seen as strings) into a kind
// of byte-code (prop_list) that encode the associated query.  At some
// point this service should be offered by TChecker, so that we do not
// have to depend on the way variables are stored, and so that we do
// not even have to decide on the syntax to use for those
// propositions.
// https://github.com/ticktac-project/tchecker/issues/21
void convert_aps(const spot::atomic_prop_set* aps,
                 const tchecker::zg::ta::model_t& model,
                 spot::bdd_dict_ptr dict, spot::formula dead,
                 prop_list& out)
{
  int errors = 0;
  std::ostringstream err;

  const auto& sys = model.system();
  const auto& procidx = sys.processes();
  const auto& locations = sys.locations();
  auto const& intvars = model.system_integer_variables();
  auto const& varsidx = intvars.index();

  for (spot::atomic_prop_set::const_iterator ap = aps->begin();
       ap != aps->end(); ++ap)
    {
      if (*ap == dead)
        continue;

      const std::string& str = ap->ap_name();
      const char* s = str.c_str();

      // Skip any leading blank.
      while (*s && (*s == ' ' || *s == '\t'))
        ++s;
      if (!*s)
        {
          err << "Proposition `" << str << "' cannot be parsed.\n";
          ++errors;
          continue;
        }


      char* name = (char*) malloc(str.size() + 1);
      char* name_p = name;
      char* lastdot = nullptr;
      while (*s && (*s != '=') && *s != '<' && *s != '!'  && *s != '>')
        {

          if (*s == ' ' || *s == '\t')
            ++s;
          else
            {
              if (*s == '.')
                lastdot = name_p;
              *name_p++ = *s++;
            }
        }
      *name_p = 0;

      if (name == name_p)
        {
          err << "Proposition `" << str << "' cannot be parsed.\n";
          free(name);
          ++errors;
          continue;
        }

      // Lookup the name
      int varid;
      try
        {
          varid = varsidx.key(name);
        }
      catch (const std::invalid_argument&)
        {
          varid = -1;
        }

      if (varid < 0)
        {
          // We may have a name such as X.Y.Z
          // If it is not a known variable, it might mean
          // an enumerated variable X.Y with value Z.
          int procid = -1;
          if (lastdot)
            {
              *lastdot++ = 0;
              try
                {
                  procid = procidx.key(name);
                }
              catch (const std::invalid_argument&)
                {
                  procid = -1;
                }
            }

          if (procid < 0)
            {
              err << "No variable or process `" << name
                  << "' found in model (for proposition `"
                  << str << "').\n";
              free(name);
              ++errors;
              continue;
            }

          // We have found a process name, lastdot is
          // pointing to its location.
          try
            {
              varid = sys.location(name, lastdot)->id();
            }
          catch (const std::invalid_argument&)
            {
              err << "No location `" << lastdot << "' known for process `"
                  << name << "'.\n";
              // FIXME: list possible locations.
              free(name);
              ++errors;
              continue;
            }

          // At this point, *s should be 0.
          if (*s)
            {
              err << "Trailing garbage `" << s
                  << "' at end of proposition `"
                  << str << "'.\n";
              free(name);
              ++errors;
              continue;
            }

          // Record that X.Y must be equal to Z.
          int v = dict->register_proposition(*ap, &out);
          one_prop p = { procid, OP_AT, varid, v };
          out.emplace_back(p);
          free(name);
          continue;
        }

      if (!*s)                // No operator?  Assume "!= 0".
        {
          int v = dict->register_proposition(*ap, &out);
          one_prop p = { varid, OP_NE, 0, v };
          out.emplace_back(p);
          free(name);
          continue;
        }

      relop op;

      switch (*s)
        {
        case '!':
          if (s[1] != '=')
            goto report_error;
          op = OP_NE;
          s += 2;
          break;
        case '=':
          if (s[1] != '=')
            goto report_error;
          op = OP_EQ;
          s += 2;
          break;
        case '<':
          if (s[1] == '=')
            {
              op = OP_LE;
              s += 2;
            }
          else
            {
              op = OP_LT;
              ++s;
            }
          break;
        case '>':
          if (s[1] == '=')
            {
              op = OP_GE;
              s += 2;
            }
          else
            {
              op = OP_GT;
              ++s;
            }
          break;
        default:
        report_error:
          err << "Unexpected `" << s
              << "' while parsing atomic proposition `" << str
              << "'.\n";
          ++errors;
          free(name);
          continue;
        }

      while (*s && (*s == ' ' || *s == '\t'))
        ++s;

      char* s_end;
      int val = strtol(s, &s_end, 10);
      if (s == s_end)
        {
          err << "Failed to parse `" << s << "' as an integer.\n";
          ++errors;
          free(name);
          continue;
        }
      s = s_end;
      free(name);

      while (*s && (*s == ' ' || *s == '\t'))
        ++s;
      if (*s)
        {
          err << "Unexpected `" << s
              << "' while parsing atomic proposition `" << str
              << "'.\n";
          ++errors;
          continue;
        }

      int v = dict->register_proposition(*ap, &out);
      one_prop p = { varid, op, val, v };
      out.emplace_back(p);
    }

  if (errors)
    throw std::runtime_error(err.str());
}

tc_model::tc_model(tc_model_details* tcm)
  : priv_(tcm)
{
}

tc_model tc_model::load(const std::string filename)
{
  tchecker::log_t log(std::cerr);
  auto* sysdecl = tchecker::parsing::parse_system_declaration(filename, log);

  if (sysdecl == nullptr)
    throw std::runtime_error("System declaration could not be built.");
  auto tcm = new tc_model_details(sysdecl,
                                  new tchecker::zg::ta::model_t(*sysdecl, log));
  return tc_model(tcm);
}

void tc_model::dump_info(std::ostream& out) const
{
  auto& s = priv_->model->system();
  // auto const& events = s.events();
  // for (const auto& e: events)
  //   out << "evt " << events.key(e) << '=' << events.value(e) << '\n';
  const auto& process_index = s.processes();
  bool first = true;
  for (const auto* loc: s.locations())
    {
      if (first)
        {
          std::cout
            << "The following location(s) may be used in the formula:\n";
          first = false;
        }
      std::string const & process_name = process_index.value(loc->pid());
      out << "- " // << loc->id() << '='
          << process_name << "." << loc->name() << '\n';
    }
  auto const& intvars = priv_->model->system_integer_variables();
  auto const& idx = intvars.index();
  first = true;
  for (const auto v: idx)
    {
      if (first)
        {
          std::cout
            << "The following variable(s) may be used in the formula:\n";
          first = false;
        }
      auto id = idx.key(v);
      tchecker::intvar_info_t const & info = intvars.info(id);
      out << "- " << /* id << '=' << */ idx.value(v)
          << " (" << info.min() << ".." << info.max() << ")\n";
    }
}


static spot::kripke_ptr
instantiate_kripke(tchecker::gc_t& gc, tc_model_details_ptr tcmd,
                   const spot::bdd_dict_ptr& dict, const prop_list* ps,
                   spot::formula dead, zg_zone_semantics zone_sem)
{
#define inst(ZONE) \
  case ZONE: \
    return std::make_shared<tcltl_kripke<tchecker::zg::ta::ZONE ## _t>>\
      (gc, tcmd, dict, ps, dead);
  switch (zone_sem)
    {
      inst(elapsed_no_extrapolation);
      inst(elapsed_extraLU_global);
      inst(elapsed_extraLU_local);
      inst(elapsed_extraLUplus_global);
      inst(elapsed_extraLUplus_local);
      inst(elapsed_extraM_global);
      inst(elapsed_extraM_local);
      inst(elapsed_extraMplus_global);
      inst(elapsed_extraMplus_local);
      inst(non_elapsed_no_extrapolation);
      inst(non_elapsed_extraLU_global);
      inst(non_elapsed_extraLU_local);
      inst(non_elapsed_extraLUplus_global);
      inst(non_elapsed_extraLUplus_local);
      inst(non_elapsed_extraM_global);
      inst(non_elapsed_extraM_local);
      inst(non_elapsed_extraMplus_global);
      inst(non_elapsed_extraMplus_local);
    }
#undef inst
  // unreachable
  assert(0);
  return nullptr;
}

spot::kripke_ptr tc_model::kripke(tchecker::gc_t& gc,
                                  const spot::atomic_prop_set* to_observe,
                                  spot::bdd_dict_ptr dict,
                                  spot::formula dead,
                                  zg_zone_semantics zone_sem)
{
  prop_list* ps = new prop_list;
  try
    {
      convert_aps(to_observe, *priv_->model, dict, dead, *ps);
    }
  catch (const std::runtime_error&)
    {
      dict->unregister_all_my_variables(ps);
      delete ps;
      throw;
    }

  spot::kripke_ptr res =
    instantiate_kripke(gc, priv_, dict, ps, dead, zone_sem);

  // All atomic propositions have been registered to the bdd_dict
  // for iface, but we also need to add them to the automaton so
  // twa::ap() works.
  for (auto ap: *to_observe)
    res->register_ap(ap);
  if (dead.is(spot::op::ap))
    res->register_ap(dead);
  return res;
}
