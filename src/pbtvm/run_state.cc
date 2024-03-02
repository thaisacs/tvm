#include "run_state.h"

#include <iostream>

#include "../auto_scheduler/search_policy/sketch_policy.h"

namespace tvm {
namespace pbtvm {

Array<tvm::auto_scheduler::MeasureResult> Run(
    const tvm::auto_scheduler::SearchTask& search_task, const tvm::auto_scheduler::State& state,
    const int timeout, const int number, const int repeat, const int min_repeat_ms,
    const double cooldown_interval, const bool enable_cpu_cache_flush, const int device,
    const int n_parallel, const tvm::runtime::String& build_func) {
  std::cout << "running..." << std::endl;

  auto runner = tvm::auto_scheduler::LocalRunner(timeout, number, repeat, min_repeat_ms,
                                                 cooldown_interval, enable_cpu_cache_flush, device);
  auto builder = tvm::auto_scheduler::LocalBuilder(timeout, n_parallel, build_func);
  Array<tvm::auto_scheduler::MeasureCallback> measure_callbacks;
  auto verbose = 0;

  tvm::auto_scheduler::ProgramMeasurer measurer =
      tvm::auto_scheduler::ProgramMeasurer(builder, runner, measure_callbacks, verbose);

  Array<tvm::auto_scheduler::MeasureInput> inputs;
  Array<tvm::auto_scheduler::MeasureResult> results;

  auto node = tvm::auto_scheduler::SketchPolicyNode();

  measurer->Reset();
  inputs.push_back(tvm::auto_scheduler::MeasureInput(search_task, state));
  results =
      measurer->Measure(search_task, GetRef<tvm::auto_scheduler::SearchPolicy>(&node), inputs);
  return results;
}

TVM_REGISTER_GLOBAL("pbtvm.Run").set_body_typed(Run);

}  // namespace pbtvm
}  // namespace tvm