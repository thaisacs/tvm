#include "auto_cache.h"
#include "cache_file.h"

#include <tvm/auto_scheduler/measure_record.h>
#include <tvm/auto_scheduler/search_policy.h>
#include <tvm/runtime/registry.h>

#include <fstream>
#include <iostream>
#include <string>
#include <filesystem>

namespace tvm {
namespace auto_cache {

AutoCache::AutoCache(std::string log_file) {
  Params params;
  auto InputPBuffer = llvm::MemoryBuffer::getFile(log_file);
  llvm::yaml::Input yinp(InputPBuffer->get()->getBuffer());
  yinp >> params;
  this->path = params.path;
  this->total_cache_size = params.total_cache_size;
}

void AutoCache::LoadFromFile(tvm::auto_scheduler::SearchTask search_task) {
  std::string file_path = __FILE__;
  std::string dir_path = file_path.substr(0, file_path.rfind("\\"));
  std::string workload_key = search_task->workload_key;
  std::string hash = workload_key.substr(2, 32);

  for (const auto & entry : std::filesystem::directory_iterator(this->path)) {
    TaskData data;
    std::string log_file = entry.path();
    auto InputPBuffer = llvm::MemoryBuffer::getFile(log_file);
    llvm::yaml::Input yinp(InputPBuffer->get()->getBuffer());
    yinp >> data;

    std::vector<tvm::auto_scheduler::State> task_cache;
    for (size_t i = 0; i < data.space.size(); i++) {
      auto& str = data.space[i];
      tvm::auto_scheduler::MeasureInputNode inp;
      tvm::auto_scheduler::MeasureResultNode res;
      std::string log_version;
      std::string mynew;

      for(long unsigned int i = 0; i < str.size(); i++) {
        if(str[i] == '\'') {
          mynew.push_back('"');
        }else if(str[i] == '"') {
          mynew.push_back('\\');
          mynew.push_back('"');
        }else if(str[i] == '\n') {
          i = i + 1;
        }else {
          mynew.push_back(str[i]);
        }
      }

      tvm::auto_scheduler::ReadMeasureRecord(mynew, &inp, &res, &log_version);
      std::string inp_workload_key = inp.task->workload_key;
      std::string inp_hash = inp_workload_key.substr(2, 32);
      if(hash == inp_hash) {
        tvm::auto_scheduler::State state = search_task->compute_dag->init_state;
        auto pstate = state.CopyOnWrite();
        pstate->transform_steps = inp.state->transform_steps;
        for (const auto& step : pstate->transform_steps) {
          StepApplyToState(step, &state, search_task->compute_dag);
        }
        task_cache.push_back(std::move(state));
      }
    }
    if(task_cache.size() > 0) {
      this->cache.push_back(std::move(task_cache));
    }
  }
  std::cout << "=======================" << std::endl;
  std::cout << "myhash: ";
  std::cout << hash << std::endl;
  std::cout << "cache size: ";
  std::cout << this->cache.size() << std::endl;
  for(unsigned i = 0; i < this->cache.size(); i++) {
    std::cout << this->cache[i].size() << std::endl;
  }
  std::cout << "=======================" << std::endl;
}

Array<tvm::auto_scheduler::State> AutoCache::SampleInitPopulation() {
  Array<tvm::auto_scheduler::State> measured_states;
  for (size_t idx = 0; idx < this->cache.size(); idx++) {
    size_t cache_size = this->cache[idx].size();
    size_t value = this->total_cache_size/this->cache.size();
    if(cache_size < value)
      value = cache_size;
    for (size_t i = 0; i < value; i++) {
      measured_states.push_back(this->cache[idx][i]);
    }
  }
  std::cout << "=======================" << std::endl;
  std::cout << "real cache size: ";
  std::cout << measured_states.size() << std::endl;
  std::cout << "=======================" << std::endl;
  return measured_states;
}

}  // namespace auto_cache
}  // namespace tvm
