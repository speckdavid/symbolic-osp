#include "osp_symbolic_uniform_cost_search.h"
#include "../../option_parser.h"
#include "../original_state_space.h"
#include "../plugin.h"
#include "../searches/bidirectional_search.h"
#include "../searches/uniform_cost_search.h"

namespace symbolic {

void OspSymbolicUniformCostSearch::initialize() {
  mgr = std::make_shared<OriginalStateSpace>(vars.get(), mgrParams);

  std::unique_ptr<UniformCostSearch> fw_search = nullptr;
  std::unique_ptr<UniformCostSearch> bw_search = nullptr;

  if (fw) {
    fw_search = std::unique_ptr<UniformCostSearch>(
        new UniformCostSearch(this, searchParams));
  }

  if (bw) {
    bw_search = std::unique_ptr<UniformCostSearch>(
        new UniformCostSearch(this, searchParams));
  }

  if (fw) {
    fw_search->init(mgr, true, bw_search.get());
  }

  if (bw) {
    bw_search->init(mgr, false, fw_search.get());
  }

  plan_data_base->init(vars);
  solution_registry.init(vars, fw_search.get(), bw_search.get(),
                         plan_data_base);

  if (fw && bw) {
    search = std::unique_ptr<BidirectionalSearch>(new BidirectionalSearch(
        this, searchParams, move(fw_search), move(bw_search)));
  } else {
    search.reset(fw ? fw_search.release() : bw_search.release());
  }

  initialize_utilitiy_function();
  upper_bound = task->get_plan_bound(); // We use upper bound for plan bound
}

void OspSymbolicUniformCostSearch::initialize_utilitiy_function() {
  utility_function = vars->zeroBDD().Add();
  for (auto &pair : task->get_utilities()) {
    BDD fact = vars->get_axiom_compiliation()->get_primary_representation(
        pair.first.var, pair.first.value);
    ADD value = vars->get_manager()->constant(pair.second);
    utility_function += (fact.Add() * value);
  }
}

OspSymbolicUniformCostSearch::OspSymbolicUniformCostSearch(
    const options::Options &opts)
    : SymbolicUniformCostSearch(opts, true, false) {}

void OspSymbolicUniformCostSearch::new_solution(const SymSolutionCut &sol) {
  ADD states_utilities = sol.get_cut().Add() * utility_function;
  double max_value = Cudd_V(states_utilities.FindMax().getNode());
  BDD max_states = states_utilities.BddThreshold(max_value);
  SymSolutionCut max_sol = sol;
  max_sol.set_cut(max_states);
}

void OspSymbolicUniformCostSearch::add_options_to_parser(OptionParser &parser) {
  SymbolicUniformCostSearch::add_options_to_parser(parser);
}

} // namespace symbolic

static std::shared_ptr<SearchEngine> _parse_forward_osp(OptionParser &parser) {
  parser.document_synopsis("Symbolic Forward Osp Search", "");
  symbolic::OspSymbolicUniformCostSearch::add_options_to_parser(parser);
  Options opts = parser.parse();

  std::shared_ptr<symbolic::SymbolicSearch> engine = nullptr;
  if (!parser.dry_run()) {
    engine = std::make_shared<symbolic::OspSymbolicUniformCostSearch>(opts);
  }

  return engine;
}

static Plugin<SearchEngine> _plugin_sym_fw_osp("symosp-fw", _parse_forward_osp);