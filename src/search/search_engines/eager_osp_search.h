#ifndef SEARCH_ENGINES_EAGER_OSP_SEARCH_H
#define SEARCH_ENGINES_EAGER_OSP_SEARCH_H

#include "../open_list.h"
#include "../symbolic/sym_variables.h"
#include "eager_search.h"

#include <memory>
#include <vector>

class Evaluator;
class PruningMethod;

namespace options {
class Options;
}

namespace eager_search {
class EagerOspSearch : public EagerSearch {

protected:
  symbolic::SymVariables vars;
  ADD add_utility_function;
  double max_utility;

  GlobalState best_state;
  double best_utility;

  virtual void initialize() override;
  virtual SearchStatus step() override;
  virtual std::pair<SearchNode, bool> fetch_next_node() override;

public:
  explicit EagerOspSearch(const options::Options &opts);
  virtual ~EagerOspSearch() = default;
};
} // namespace eager_search

#endif
