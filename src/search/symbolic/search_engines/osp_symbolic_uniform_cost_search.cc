#include "osp_symbolic_uniform_cost_search.h"
#include "../../option_parser.h"
#include "../original_state_space.h"
#include "../../plugin.h"
#include "../searches/bidirectional_search.h"
#include "../searches/osp_uniform_cost_search.h"

namespace symbolic
{

  void OspSymbolicUniformCostSearch::initialize()
  {
    mgr = std::make_shared<OriginalStateSpace>(vars.get(), mgrParams);

    std::unique_ptr<OspUniformCostSearch> fw_search = nullptr;
    std::unique_ptr<OspUniformCostSearch> bw_search = nullptr;

    if (fw)
    {
      fw_search = std::unique_ptr<OspUniformCostSearch>(
          new OspUniformCostSearch(this, searchParams));
    }

    if (bw)
    {
      bw_search = std::unique_ptr<OspUniformCostSearch>(
          new OspUniformCostSearch(this, searchParams));
    }

    if (fw)
    {
      fw_search->init(mgr, true, bw_search.get());
    }

    if (bw)
    {
      bw_search->init(mgr, false, fw_search.get());
    }

    plan_data_base->init(vars);
    solution_registry.init(vars, fw_search.get(), bw_search.get(), plan_data_base,
                           true);

    if (fw && bw)
    {
      search = std::unique_ptr<BidirectionalSearch>(new BidirectionalSearch(
          this, searchParams, move(fw_search), move(bw_search)));
    }
    else
    {
      search.reset(fw ? fw_search.release() : bw_search.release());
    }

    initialize_utilitiy_function();
    upper_bound = task->get_plan_bound(); // We use upper bound for plan bound
  }

  void OspSymbolicUniformCostSearch::initialize_utilitiy_function()
  {
    std::cout << "Utility function type: " << (use_add ? "ADD" : "BDD")
              << std::endl;
    std::cout << "Creating utility function..." << std::flush;
    utils::Timer timer;

    add_utility_function = vars->zeroBDD().Add();
    for (auto &pair : task->get_utilities())
    {
      BDD fact = vars->get_axiom_compiliation()->get_primary_representation(
          pair.first.var, pair.first.value);
      ADD value = vars->get_manager()->constant(pair.second);
      add_utility_function += (fact.Add() * value);
    }
    max_utility = Cudd_V(add_utility_function.FindMax().getNode());
    timer.stop();
    std::cout << "done." << std::endl;
    std::cout << "Utility function creation: " << timer << std::endl;
    std::cout << "Utility ADD Nodes: " << add_utility_function.nodeCount() << std::endl;

    if (!use_add)
    {
      std::cout << "Disassembling utility function..." << std::flush;
      timer.reset();
      timer.resume();
      ADD cur_add = add_utility_function;
      double min_value = Cudd_V(cur_add.FindMin().getNode());

      while (cur_add != vars->get_manager()->constant(
                            std::numeric_limits<double>::infinity()))
      {
        // std::cout << min_value << std::endl;
        bdd_utility_functions[min_value] =
            cur_add.BddInterval(min_value, min_value);
        cur_add = cur_add.Maximum(bdd_utility_functions[min_value].Add() *
                                  vars->get_manager()->constant(
                                      std::numeric_limits<double>::infinity()));
        min_value = Cudd_V(cur_add.FindMin().getNode());
      }
      std::cout << "done." << std::endl;
      std::cout << "Utility function disassembling: " << timer << std::endl;
      std::cout << "Number of utility values: " << bdd_utility_functions.size() << std::endl;
    }
  }

  SearchStatus OspSymbolicUniformCostSearch::step()
  {
    step_num++;
    // Handling empty plan
    if (step_num == 0)
    {
      BDD cut = mgr->getInitialState() * mgr->getGoal();
      if (!cut.IsZero())
      {
        new_solution(SymSolutionCut(0, 0, cut));
      }
    }

    SearchStatus cur_status;

    // Search finished!
    if (lower_bound >= upper_bound)
    {
      solution_registry.construct_cheaper_solutions(
          std::numeric_limits<int>::max());
      solution_found = plan_data_base->get_num_reported_plan() > 0;
      cur_status = solution_found ? SOLVED : FAILED;
    }
    else
    {
      // All plans found
      if (std::abs(max_utility - plan_utility) < 0.001)
      {
        solution_registry.construct_cheaper_solutions(
            std::numeric_limits<int>::max());
        solution_found = true;
        cur_status = SOLVED;
      }
      else
      {
        cur_status = IN_PROGRESS;
      }
    }

    if (lower_bound_increased)
    {
      std::cout << "BOUND: " << lower_bound << " < " << upper_bound << std::flush;

      std::cout << " [" << solution_registry.get_num_found_plans() << "/"
                << plan_data_base->get_num_desired_plans() << " plans]"
                << std::flush;
      std::cout << ", total time: " << utils::g_timer << std::endl;
    }
    lower_bound_increased = false;

    if (cur_status == SOLVED)
    {
      std::cout << "Best plan:" << std::endl;
      plan_data_base->dump_first_accepted_plan();
      std::cout << "Plan utility: " << plan_utility << std::endl;
      return cur_status;
    }
    if (cur_status == FAILED)
    {
      return cur_status;
    }

    // Actuall step
    search->step();

    return cur_status;
  }

  OspSymbolicUniformCostSearch::OspSymbolicUniformCostSearch(
      const options::Options &opts)
      : SymbolicUniformCostSearch(opts, true, false),
        use_add(opts.get<bool>("use_add")),
        plan_utility(-std::numeric_limits<double>::infinity()),
        max_utility(-std::numeric_limits<double>::infinity()) {}

  void OspSymbolicUniformCostSearch::new_solution(const SymSolutionCut &sol)
  {
    if (sol.get_f() != -1 && sol.get_f() < upper_bound)
    {

      if (use_add)
      {
        ADD states_utilities = sol.get_cut().Add() * add_utility_function;
        double max_value = Cudd_V(states_utilities.FindMax().getNode());
        if (max_value > plan_utility ||
            (max_value >= plan_utility &&
             sol.get_f() < solution_registry.cheapest_solution_cost_found()))
        {
          BDD max_states = states_utilities.BddThreshold(max_value);
          // If the max_value is 0, we have to cut again with the
          // original BDD to avoid states that are not part of the cut.
          // This is safe because we have no negative utility values.
          if (std::abs(max_value) < 0.001)
          {
            max_states *= sol.get_cut();
          }

          SymSolutionCut max_sol = sol;
          max_sol.set_cut(max_states);
          solution_registry.register_solution(max_sol);
          plan_utility = max_value;
          std::cout << "[INFO] Best utility: " << plan_utility << std::endl;
        }
      }
      else
      {
        double max_value = -1;
        BDD max_states = vars->zeroBDD();
        for (auto iter = bdd_utility_functions.rbegin();
             iter != bdd_utility_functions.rend(); ++iter)
        {
          max_states = iter->second * sol.get_cut();
          if (!max_states.IsZero())
          {
            max_value = iter->first;
            break;
          }
        }
        if (max_value > plan_utility ||
            (max_value >= plan_utility &&
             sol.get_f() < solution_registry.cheapest_solution_cost_found()))
        {
          SymSolutionCut max_sol = sol;
          max_sol.set_cut(max_states);
          solution_registry.register_solution(max_sol);
          plan_utility = max_value;
          std::cout << "[INFO] Best utility: " << plan_utility << std::endl;
        }
      }
    }
  }

  void OspSymbolicUniformCostSearch::add_options_to_parser(OptionParser &parser)
  {
    SymbolicUniformCostSearch::add_options_to_parser(parser);
    parser.add_option<bool>("use_add", "use utility function decomposition",
                            "true");
  }

} // namespace symbolic

static std::shared_ptr<SearchEngine> _parse_forward_osp(OptionParser &parser)
{
  parser.document_synopsis("Symbolic Forward Osp Search", "");
  symbolic::OspSymbolicUniformCostSearch::add_options_to_parser(parser);
  Options opts = parser.parse();

  std::shared_ptr<symbolic::SymbolicSearch> engine = nullptr;
  if (!parser.dry_run())
  {
    engine = std::make_shared<symbolic::OspSymbolicUniformCostSearch>(opts);
  }

  return engine;
}

static Plugin<SearchEngine> _plugin_sym_fw_osp("symosp-fw", _parse_forward_osp);