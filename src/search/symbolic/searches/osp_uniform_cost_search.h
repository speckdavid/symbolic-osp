#ifndef SYMBOLIC_SEARCHES_OSP_UNIFORM_COST_SEARCH_H
#define SYMBOLIC_SEARCHES_OSP_UNIFORM_COST_SEARCH_H

#include "uniform_cost_search.h"

namespace symbolic {
class OspUniformCostSearch : public UniformCostSearch {
protected:
  virtual void checkFrontierCut(Bucket &bucket, int g) override;

public:
  OspUniformCostSearch(SymbolicSearch *eng, const SymParamsSearch &params)
      : UniformCostSearch(eng, params) {}
};
} // namespace symbolic

#endif