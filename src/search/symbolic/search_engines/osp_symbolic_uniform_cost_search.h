#ifndef SYMBOLIC_SEARCH_ENGINES_OSP_SYMBOLIC_UNIFORM_COST_SEARCH_H
#define SYMBOLIC_SEARCH_ENGINES_OSP_SYMBOLIC_UNIFORM_COST_SEARCH_H

#include "symbolic_uniform_cost_search.h"

namespace symbolic {
class OspSymbolicUniformCostSearch : public SymbolicUniformCostSearch {

protected:
  virtual void initialize() override;

public:
  OspSymbolicUniformCostSearch(const options::Options &opts);
  virtual ~OspSymbolicUniformCostSearch() = default;

  virtual void new_solution(const SymSolutionCut &sol) override;

  static void add_options_to_parser(OptionParser &parser);
};

} // namespace symbolic

#endif