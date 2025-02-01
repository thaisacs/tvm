#include "auto_cache.h"
#include "cache_file.h"
#include "config.h"
#include "util.h"

#include <tvm/auto_scheduler/measure_record.h>
#include <tvm/auto_scheduler/search_policy.h>
#include <tvm/auto_scheduler/transform_step.h>
#include <tvm/runtime/registry.h>

#include <fstream>
#include <iostream>
#include <string>
#include <filesystem>

namespace tvm {
namespace auto_cache {

AutoCache::AutoCache(std::string params_file) {
  Params params = read_params_file(params_file);
  this->path = params.path;
  this->total_cache_size = params.total_cache_size;
}

void AutoCache::LoadFromFile(tvm::auto_scheduler::SearchTask search_task) {
  std::string workload_key = search_task->workload_key;
  std::string hash = get_hash(workload_key);
  auto cache_file = this->path + "configs/"+ hash +".yml";
  if(!std::filesystem::exists(cache_file)) {
    return;
  }
  Config data = read_cache_file(cache_file);
  size_t value = this->total_cache_size/data.size;

  for(unsigned i = 0; i < data.size; i++) {
    std::string log_file = this->path + data.files[i];
    if(!std::filesystem::exists(log_file)) {
      continue;
    }
    TaskData cache_data = read_log_file(log_file);

    size_t value_ = value;
    if(cache_data.space.size() < value) {
      value_ = cache_data.space.size();
    }

    for (size_t j = 0; j < value_; j++) {
      tvm::auto_scheduler::MeasureInputNode inp;
      tvm::auto_scheduler::MeasureResultNode res;
      std::string log_version;
      std::string state_str = get_transformations(cache_data.space[j]);
      tvm::auto_scheduler::ReadMeasureRecord(state_str, &inp, &res, &log_version);
      tvm::auto_scheduler::State state = search_task->compute_dag->init_state;
      auto pstate = state.CopyOnWrite();
      pstate->transform_steps = inp.state->transform_steps;
      for (const auto& step : pstate->transform_steps) {
        StepApplyToState(step, &state, search_task->compute_dag);
      }
      this->cache.push_back(std::move(state));
    }
  }
}

Array<tvm::auto_scheduler::State> AutoCache::SampleInitPopulation() {
  Array<tvm::auto_scheduler::State> measured_states;
  for (size_t idx = 0; idx < this->cache.size(); idx++) {
    measured_states.push_back(this->cache[idx]);
  }
  return measured_states;
}

}  // namespace auto_cache
}  // namespace tvm
