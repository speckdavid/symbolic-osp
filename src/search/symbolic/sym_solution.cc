#include "sym_solution.h"

#include <vector>
#include "../state_registry.h"
#include "closed_list.h"
#include "debug_macros.h"

#include "unidirectional_search.h"

using namespace std;

namespace symbolic {

void SymSolution::merge(const SymSolution &other) {
  for (const auto &entry : other.get_cuts()) {
    if (cuts.count(entry.first)) {
      cuts[entry.first] += entry.second;
    } else {
      cuts[entry.first] = entry.second;
    }
  }
}

void SymSolution::getPlan(vector<OperatorID> &path) const {
  std::cout << "Plan reconstruction [" << cuts.size() << "]"
            << "..." << std::endl;
  for (auto &entry : cuts) {
    if (exp_fw) {
      extract_multiply_paths(entry.second, entry.first.first, true, path);
    } else {
      extract_multiply_paths(entry.second, entry.first.second, false, path);
    }
  }
  auto existing_search = exp_fw ? exp_fw : exp_bw;  // Only for output
  std::cout << "done!" << std::endl;
  std::cout << "#Plans: "
            << existing_search->getStateSpaceShared()->get_num_plans()
            << std::endl;
  /*assert(path.empty());  // This code should be modified to allow appending
                         // things to paths
  cout << "Extract path forward: " << g << endl;
  if (exp_fw) {
    exp_fw->getPlan(cut, g, path);
  }
  DEBUG_MSG(cout << "Extract path backward: " << h << endl;);
  if (exp_bw) {
    Bdd newCut;
    if (!path.empty()) {
      TaskProxy task_proxy(*tasks::g_root_task);
      State s = task_proxy.get_initial_state();
      // Get state
      for (auto op_id : path) {
        OperatorProxy op = task_proxy.get_operators()[op_id];
        s = s.get_successor(op);
      }
      newCut = exp_bw->getStateSpace()->getVars()->getStateBDD(s.get_values());
    } else {
      newCut = cut;
    }

    exp_bw->getPlan(newCut, h, path);
  }*/
}

void SymSolution::extract_multiply_paths(const Bdd &c, int h, bool fw,
                                         vector<OperatorID> path) const {
  auto cur_search = fw ? exp_fw : exp_bw;
  auto cur_closed = cur_search->getClosedShared();
  const map<int, vector<TransitionRelation>> &trs =
      cur_search->getStateSpaceShared()->getIndividualTRs();
  Bdd cut = c;

  // First we need to resolve 0-cost actions...
  cut = bdd_for_zero_reconstruction(cut, h, fw);
  size_t steps0 = cur_closed->get_zero_cut(h, cut);
  bool found_zero = false;

  if (steps0 > 0) {
    for (const TransitionRelation &tr : trs.at(0)) {
      Bdd succ;
      if (fw) {
        succ = tr.preimage(cut);
      } else {
        succ = tr.image(cut);
      }
      if (succ.IsZero()) {
        continue;
      }
      for (size_t newSteps0 = 0; newSteps0 < steps0; newSteps0++) {
        Bdd intersection = succ * cur_closed->get_zero_closed_at(h, newSteps0);
        if (!intersection.IsZero()) {
          found_zero = true;
          vector<OperatorID> new_path = path;
          new_path.push_back(*(tr.getOpsIds().begin()));
          if (h == 0 && newSteps0 == 0) {
            cur_search->getStateSpaceShared()->save_plan(new_path, fw);
          } else {
            extract_multiply_paths(intersection, h, fw, new_path);
          }
          if (cur_search->getStateSpaceShared()->found_enough_plans()) {
            return;
          }
        }
      }
    }
  }
  if (steps0 == 0 || !found_zero) {
    // Perform cost reconstruction
    for (auto key : trs) {
      if (key.first == 0) {
        continue;
      }
      int newH = h - key.first;
      if (newH < 0) {
        continue;
      }
      for (TransitionRelation &tr : key.second) {
        Bdd succ;
        if (fw) {
          succ = tr.preimage(cut);
        } else {
          succ = tr.image(cut);
        }

        Bdd intersection =
            succ * cur_search->getClosedShared()->get_closed_at(newH);
        if (!intersection.IsZero()) {
          vector<OperatorID> new_path = path;
          new_path.push_back(*(tr.getOpsIds().begin()));
          if (newH == 0 && cur_closed->get_zero_cut(newH, intersection) == 0) {
            cur_search->getStateSpaceShared()->save_plan(new_path, fw);
          } else {
            extract_multiply_paths(intersection, newH, fw, new_path);
          }
          if (cur_search->getStateSpaceShared()->found_enough_plans()) {
            return;
          }
        }
      }
    }
  }
}

Bdd SymSolution::bdd_for_zero_reconstruction(const Bdd &c, int h,
                                             bool fw) const {
  auto cur_search = fw ? exp_fw : exp_bw;
  auto cur_closed = cur_search->getClosedShared();

  // Contains 0 buckets
  if (cur_closed->get_num_zero_closed_layers(h)) {
    size_t steps0 = cur_closed->get_zero_cut(h, c);
    if (steps0 < cur_closed->get_num_zero_closed_layers(h)) {
      std::cout << "new cur returned!" << std::endl;
      return c * cur_closed->get_zero_closed_at(h, steps0);
    }
  }

  // There exist no 0-cost buckets or we haven't found it => search it
  return c;
}

}  // namespace symbolic
