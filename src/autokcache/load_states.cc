#include "load_states.h"

#include <tvm/auto_scheduler/measure_record.h>
#include <tvm/auto_scheduler/search_policy.h>
#include <tvm/runtime/registry.h>

#include "../auto_scheduler/search_policy/utils.h"


#include <fstream>
#include <iostream>
#include <string>

namespace tvm {
namespace autokcache {

Array<tvm::auto_scheduler::State> load_file(tvm::auto_scheduler::SearchTask search_task) {
  std::string log_file = "/home/thais/Dev/tvm-scripts/operator-dataset-30/matmul_add/512-512-512-0.log";
  tvm::auto_scheduler::RecordReader reader = tvm::auto_scheduler::RecordReader(log_file);
  const auto& res = reader->ReadLines(-1);
  size_t log_size = res.first.size();
  std::cout << log_size << std::endl;
  Array<tvm::auto_scheduler::State> measured_states;
  std::vector<float> measured_throughputs;
  for (size_t i = log_size-64+1; i < log_size; i++) {
      const auto& inp = res.first[i];
      tvm::auto_scheduler::State state = search_task->compute_dag->init_state;
      auto pstate = state.CopyOnWrite();
      pstate->transform_steps = inp->state->transform_steps;
      std::cout << "   log_info: " << inp->task->workload_key << std::endl;
      std::cout << "search info:" << search_task->workload_key << std::endl;
      for (const auto& step : pstate->transform_steps) {
        StepApplyToState(step, &state, search_task->compute_dag);
      }
      measured_states.push_back(std::move(state));
      measured_throughputs.push_back(
          res.second[i]->error_no == 0 ? (1.0 / tvm::auto_scheduler::FloatArrayMean(res.second[i]->costs)) : 0.0);
  }
  return measured_states;
}

}  // namespace autokcache
}  // namespace tvm
