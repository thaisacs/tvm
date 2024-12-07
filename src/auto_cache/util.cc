#include "util.h"

namespace tvm {
namespace auto_cache {

std::string get_hash(std::string workload_key) {
    std::string hash = workload_key.substr(2, 32);
    return hash;
}

Config read_cache_file(std::string cache_file) {
    Config data;
    auto InputPBuffer = llvm::MemoryBuffer::getFile(cache_file);
    llvm::yaml::Input yinp(InputPBuffer->get()->getBuffer());
    yinp >> data;
    return data;
}

TaskData read_log_file(std::string log_file) {
    TaskData data;
    auto InputPBuffer = llvm::MemoryBuffer::getFile(log_file);
    llvm::yaml::Input yinp(InputPBuffer->get()->getBuffer());
    yinp >> data;
    return data;
}

std::string get_transformations(std::string state_str) {
    std::string state_new;
    for(long unsigned int i = 0; i < state_str.size(); i++) {
        if(state_str[i] == '\'') {
            state_new.push_back('"');
        }else if(state_str[i] == '"') {
            state_new.push_back('\\');
            state_new.push_back('"');
        }else if(state_str[i] == '\n') {
            i = i + 1;
        }else {
            state_new.push_back(state_str[i]);
        }
    }
    return state_new;
}

Params read_params_file(std::string params_file) {
  Params params;
  auto InputPBuffer = llvm::MemoryBuffer::getFile(params_file);
  llvm::yaml::Input yinp(InputPBuffer->get()->getBuffer());
  yinp >> params;
    return params;
}

}
}