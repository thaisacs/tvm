#ifndef TVM_AUTO_CACHE_H
#define TVM_AUTO_CACHE_H

#include <tvm/arith/analyzer.h>
#include <tvm/auto_scheduler/auto_schedule.h>
#include <tvm/auto_scheduler/measure.h>
#include <tvm/auto_scheduler/search_policy.h>
#include <tvm/runtime/registry.h>
#include <tvm/tir/builtin.h>
#include <tvm/tir/expr_functor.h>

namespace tvm {
namespace auto_cache {

/*!
 * \brief The auto cache that makes it more efficient.
 */
class AutoCache {
 public:
  /*! \brief The cache map for our auto cache. */
  std::vector<tvm::auto_scheduler::State> cache;

  /*!
   * \brief Sample the init population.
   * \return The generated states (the initial population).
   */
  void LoadFromFile(tvm::auto_scheduler::SearchTask search_task);

  /*!
   * \brief Sample the init population.
   * \return The generated states (the initial population).
   */
  Array<tvm::auto_scheduler::State> SampleInitPopulation();

};


}  // namespace auto_cache
}  // namespace tvm

#endif
