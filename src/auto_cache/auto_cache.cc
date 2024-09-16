#include "auto_cache.h"

#include <tvm/auto_scheduler/measure_record.h>
#include <tvm/auto_scheduler/search_policy.h>
#include <tvm/runtime/registry.h>

#include <fstream>
#include <iostream>
#include <string>
#include <filesystem>

namespace tvm {
namespace auto_cache {

//Array<tvm::auto_scheduler::State> AutoCache::LoadFromFile(tvm::auto_scheduler::SearchTask search_task) {
//  std::string file_path = __FILE__;
//  std::string dir_path = file_path.substr(0, file_path.rfind("\\"));
//  std::cout << dir_path << std::endl;
//  std::string path = "/home/thais/Dev/tvm/src/auto_cache/cache/";
//  Array<tvm::auto_scheduler::State> measured_states;
//  for (const auto & entry : std::filesystem::directory_iterator(path)) {
//    std::cout << entry.path() << std::endl;
//    std::string log_file = entry.path();
//    tvm::auto_scheduler::RecordReader reader = tvm::auto_scheduler::RecordReader(log_file);
//    const auto& res = reader->ReadLines(-1);
//    size_t log_size = res.first.size();
//    std::cout << log_size << std::endl;
//    std::vector<float> measured_throughputs;
//    std::cout << "search info:" << search_task->workload_key << std::endl;
//    for (size_t i = 0; i < log_size; i++) {
//        const auto& inp = res.first[i];
//        tvm::auto_scheduler::State state = search_task->compute_dag->init_state;
//        auto pstate = state.CopyOnWrite();
//        pstate->transform_steps = inp->state->transform_steps;
//        std::cout << "   log_info: " << inp->task->workload_key << std::endl;
//        for (const auto& step : pstate->transform_steps) {
//          StepApplyToState(step, &state, search_task->compute_dag);
//        }
//        measured_states.push_back(std::move(state));
//        //measured_throughputs.push_back(
//        //    res.second[i]->error_no == 0 ? (1.0 / tvm::auto_scheduler::FloatArrayMean(res.second[i]->costs)) : 0.0);
//    }
//  }
//  return measured_states;
//}

void AutoCache::LoadFromFile(tvm::auto_scheduler::SearchTask search_task) {
  std::string file_path = __FILE__;
  std::string dir_path = file_path.substr(0, file_path.rfind("\\"));
  std::string path = "/home/thais/Dev/tvm/src/auto_cache/cache/";
  std::string workload_key = search_task->workload_key;
  std::string hash = workload_key.substr(2, 32);
  for (const auto & entry : std::filesystem::directory_iterator(path)) {
    std::string log_file = entry.path();
    tvm::auto_scheduler::RecordReader reader = tvm::auto_scheduler::RecordReader(log_file);
    const auto& res = reader->ReadLines(-1);
    size_t log_size = res.first.size();
    for (size_t i = 0; i < log_size; i++) {
      const auto& inp = res.first[i];
      std::string inp_workload_key = inp->task->workload_key;
      std::string inp_hash = inp_workload_key.substr(2, 32);
      if(hash == inp_hash) {
        tvm::auto_scheduler::State state = search_task->compute_dag->init_state;
        auto pstate = state.CopyOnWrite();
        pstate->transform_steps = inp->state->transform_steps;
        for (const auto& step : pstate->transform_steps) {
          StepApplyToState(step, &state, search_task->compute_dag);
        }
        this->cache.push_back(std::move(state));
      }
    }
  }
}

Array<tvm::auto_scheduler::State> AutoCache::SampleInitPopulation() {
  Array<tvm::auto_scheduler::State> measured_states;
  size_t cache_size = this->cache.size();
  size_t value = 2050;
  if(cache_size < value)
    value = 0;
  for (size_t i = cache_size-value; i < cache_size; i++) {
    measured_states.push_back(this->cache[i]);
  }
  return measured_states;
}

}  // namespace auto_cache
}  // namespace tvm
