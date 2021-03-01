#include "eager_osp_search.h"

#include "../evaluation_context.h"
#include "../evaluator.h"
#include "../open_list_factory.h"
#include "../option_parser.h"
#include "../pruning_method.h"

#include "../algorithms/ordered_set.h"
#include "../task_utils/successor_generator.h"

#include <cassert>
#include <cstdlib>
#include <memory>
#include <set>

using namespace std;

namespace eager_search {

void EagerOspSearch::initialize() {
  EagerSearch::initialize();
  vars.init();

  add_utility_function = vars.zeroBDD().Add();
  for (auto &pair : task->get_utilities()) {
    BDD fact = vars.get_axiom_compiliation()->get_primary_representation(
        pair.first.var, pair.first.value);
    ADD value = vars.get_manager()->constant(pair.second);
    add_utility_function += (fact.Add() * value);
  }
  max_utility = Cudd_V(add_utility_function.FindMax().getNode());
  bound = task->get_plan_bound();
}

SearchStatus EagerOspSearch::step() {
  pair<SearchNode, bool> n = fetch_next_node();
  if (!n.second) {
    check_goal_and_set_plan(best_state);
    std::cout << "Plan utility: " << best_utility << std::endl;
    save_plan_if_necessary();
    return SOLVED;
  }
  SearchNode node = n.first;

  GlobalState s = node.get_state();

  BDD bdd_state = vars.getStateBDD(s);
  ADD add_state_util = add_utility_function * bdd_state.Add();
  double state_util = Cudd_V(add_state_util.FindMax().getNode());
  if (std::abs(max_utility - state_util) < 0.001) {
    best_state = s;
    best_utility = state_util;
    check_goal_and_set_plan(best_state);
    std::cout << "Plan utility: " << best_utility << std::endl;
    save_plan_if_necessary();
    return SOLVED;
  }

  if (state_util > best_utility) {
    best_state = s;
    best_utility = state_util;
  }

  vector<OperatorID> applicable_ops;
  successor_generator.generate_applicable_ops(s, applicable_ops);

  /*
    TODO: When preferred operators are in use, a preferred operator will be
    considered by the preferred operator queues even when it is pruned.
  */
  pruning_method->prune_operators(s, applicable_ops);

  // This evaluates the expanded state (again) to get preferred ops
  EvaluationContext eval_context(s, node.get_g(), false, &statistics, true);
  ordered_set::OrderedSet<OperatorID> preferred_operators;
  for (const shared_ptr<Evaluator> &preferred_operator_evaluator :
       preferred_operator_evaluators) {
    collect_preferred_operators(
        eval_context, preferred_operator_evaluator.get(), preferred_operators);
  }

  for (OperatorID op_id : applicable_ops) {
    OperatorProxy op = task_proxy.get_operators()[op_id];
    if ((node.get_real_g() + op.get_cost()) >= bound)
      continue;

    GlobalState succ_state = state_registry.get_successor_state(s, op);
    statistics.inc_generated();
    bool is_preferred = preferred_operators.contains(op_id);

    SearchNode succ_node = search_space.get_node(succ_state);

    for (Evaluator *evaluator : path_dependent_evaluators) {
      evaluator->notify_state_transition(s, op_id, succ_state);
    }

    // Previously encountered dead end. Don't re-evaluate.
    if (succ_node.is_dead_end())
      continue;

    if (succ_node.is_new()) {
      // We have not seen this state before.
      // Evaluate and create a new node.

      // Careful: succ_node.get_g() is not available here yet,
      // hence the stupid computation of succ_g.
      // TODO: Make this less fragile.
      int succ_g = node.get_g() + get_adjusted_cost(op);

      EvaluationContext succ_eval_context(succ_state, succ_g, is_preferred,
                                          &statistics);
      statistics.inc_evaluated_states();

      if (open_list->is_dead_end(succ_eval_context)) {
        succ_node.mark_as_dead_end();
        statistics.inc_dead_ends();
        continue;
      }
      succ_node.open(node, op, get_adjusted_cost(op));

      open_list->insert(succ_eval_context, succ_state.get_id());
      if (search_progress.check_progress(succ_eval_context)) {
        print_checkpoint_line(succ_node.get_g());
        reward_progress();
      }
    } else if (succ_node.get_g() > node.get_g() + get_adjusted_cost(op)) {
      // We found a new cheapest path to an open or closed state.
      if (reopen_closed_nodes) {
        if (succ_node.is_closed()) {
          /*
            TODO: It would be nice if we had a way to test
            that reopening is expected behaviour, i.e., exit
            with an error when this is something where
            reopening should not occur (e.g. A* with a
            consistent heuristic).
          */
          statistics.inc_reopened();
        }
        succ_node.reopen(node, op, get_adjusted_cost(op));

        EvaluationContext succ_eval_context(succ_state, succ_node.get_g(),
                                            is_preferred, &statistics);

        /*
          Note: our old code used to retrieve the h value from
          the search node here. Our new code recomputes it as
          necessary, thus avoiding the incredible ugliness of
          the old "set_evaluator_value" approach, which also
          did not generalize properly to settings with more
          than one evaluator.

          Reopening should not happen all that frequently, so
          the performance impact of this is hopefully not that
          large. In the medium term, we want the evaluators to
          remember evaluator values for states themselves if
          desired by the user, so that such recomputations
          will just involve a look-up by the Evaluator object
          rather than a recomputation of the evaluator value
          from scratch.
        */
        open_list->insert(succ_eval_context, succ_state.get_id());
      } else {
        // If we do not reopen closed nodes, we just update the parent pointers.
        // Note that this could cause an incompatibility between
        // the g-value and the actual path that is traced back.
        succ_node.update_parent(node, op, get_adjusted_cost(op));
      }
    }
  }

  return IN_PROGRESS;
}

pair<SearchNode, bool> EagerOspSearch::fetch_next_node() {
    /* TODO: The bulk of this code deals with multi-path dependence,
       which is a bit unfortunate since that is a special case that
       makes the common case look more complicated than it would need
       to be. We could refactor this by implementing multi-path
       dependence as a separate search algorithm that wraps the "usual"
       search algorithm and adds the extra processing in the desired
       places. I think this would lead to much cleaner code. */

    while (true) {
        if (open_list->empty()) {
            // cout << "Completely explored state space -- no solution!" << endl;
            // HACK! HACK! we do this because SearchNode has no default/copy constructor
            const GlobalState &initial_state = state_registry.get_initial_state();
            SearchNode dummy_node = search_space.get_node(initial_state);
            return make_pair(dummy_node, false);
        }
        StateID id = open_list->remove_min();
        // TODO is there a way we can avoid creating the state here and then
        //      recreate it outside of this function with node.get_state()?
        //      One way would be to store GlobalState objects inside SearchNodes
        //      instead of StateIDs
        GlobalState s = state_registry.lookup_state(id);
        SearchNode node = search_space.get_node(s);

        if (node.is_closed())
            continue;

        if (!lazy_evaluator)
            assert(!node.is_dead_end());

        if (lazy_evaluator) {
            /*
              With lazy evaluators (and only with these) we can have dead nodes
              in the open list.

              For example, consider a state s that is reached twice before it is expanded.
              The first time we insert it into the open list, we compute a finite
              heuristic value. The second time we insert it, the cached value is reused.

              During first expansion, the heuristic value is recomputed and might become
              infinite, for example because the reevaluation uses a stronger heuristic or
              because the heuristic is path-dependent and we have accumulated more
              information in the meantime. Then upon second expansion we have a dead-end
              node which we must ignore.
            */
            if (node.is_dead_end())
                continue;

            if (lazy_evaluator->is_estimate_cached(s)) {
                int old_h = lazy_evaluator->get_cached_estimate(s);
                /*
                  We can pass calculate_preferred=false here
                  since preferred operators are computed when the state is expanded.
                */
                EvaluationContext eval_context(s, node.get_g(), false, &statistics);
                int new_h = eval_context.get_evaluator_value_or_infinity(lazy_evaluator.get());
                if (open_list->is_dead_end(eval_context)) {
                    node.mark_as_dead_end();
                    statistics.inc_dead_ends();
                    continue;
                }
                if (new_h != old_h) {
                    open_list->insert(eval_context, id);
                    continue;
                }
            }
        }

        node.close();
        assert(!node.is_dead_end());
        update_f_value_statistics(node);
        statistics.inc_expanded();
        return make_pair(node, true);
    }
}

EagerOspSearch::EagerOspSearch(const Options &opts)
    : EagerSearch(opts), vars(true),
      max_utility(-std::numeric_limits<double>::infinity()),
      best_state(state_registry.get_initial_state()),
      best_utility(-std::numeric_limits<double>::infinity()) {}

} // namespace eager_search
