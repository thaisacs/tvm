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
  Map<String, String> cache;

  Array<tvm::auto_scheduler::State> LoadFromFile(tvm::auto_scheduler::SearchTask search_task);

//  std::pair<Array<MeasureInput>, Array<MeasureResult>> ContinueSearchOneRound(
//      int num_measure, ProgramMeasurer measurer) final;
//
//  /*!
//   * \brief Generate sketches.
//   * \return The generated sketches(states).
//   */
//  Array<State> GenerateSketches();
    void print();
//
//  /*!
//   * \brief Sample the init population.
//   * \param sketches The initial sketches for the sampled population
//   * \return The generated states (the initial population).
//   */
//  Array<State> SampleInitPopulation(const Array<State>& sketches);
//
//  /*!
//   * \brief Perform evolutionary search.
//   * \param init_populations The states generated from init population.
//   * \param out_size The number of expected output states.
//   * \return The generated states after evolutionary search.
//   */
//  Array<State> EvolutionarySearch(const Array<State>& init_populations, int out_size);
//
//  static constexpr const char* _type_key = "auto_scheduler.SketchPolicy";
//
//  TVM_DECLARE_FINAL_OBJECT_INFO(SketchPolicyNode, SearchPolicyNode);
//
// private:
//  /*!
//   * \brief Run one round of the search pipeline.
//   * \param num_random_states Number of states that are picked randomly, this is used for
//   * eps-greedy policy.
//   * \param random_states The picked random states, used as one of the output of this function.
//   * \return The best several states generated in this search round.
//   */
//  Array<State> SearchOneRound(int num_random_states, Array<State>* random_states = nullptr);
//
//  /*!
//   * \brief Pick states from best states and random states with eps-greedy policy.
//   * \param best_states States picked by cost model.
//   * \param random_states States picked randomly.
//   * \param remaining_n_trials The remaining number of states need to be generated.
//   * \return The generated states to be measured, wrapped in MeasureInput.
//   */
//  Array<MeasureInput> PickStatesWithEpsGreedy(const Array<State>& best_states,
//                                              const Array<State>& random_states,
//                                              int remaining_n_trials);
//
//  /*! \brief The number of states to measure per iteration. */
//  int num_measure_per_iter_;
//
//  /*! \brief The cached sketches */
//  Array<State> sketch_cache_;
//
//  /*! \brief The minimul output population of SampleInitPopulation */
//  int sample_init_min_pop_;
//
//  friend class SketchPolicy;
};


}  // namespace auto_cache
}  // namespace tvm

#endif
