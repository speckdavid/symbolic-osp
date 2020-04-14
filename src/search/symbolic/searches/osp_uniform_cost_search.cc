#include "osp_uniform_cost_search.h"
#include "../closed_list.h"
#include "../search_engines/symbolic_search.h"

namespace symbolic {

void OspUniformCostSearch::checkFrontierCut(Bucket &bucket, int g) {
  if (p.get_non_stop()) {
    return;
  }

  for (BDD &bucketBDD : bucket) {
    auto sol = perfectHeuristic->getCheapestCut(bucketBDD, g, fw);
    if (sol.get_f() >= 0) {
      engine->new_solution(sol);
    }
  }
}

} // namespace symbolic