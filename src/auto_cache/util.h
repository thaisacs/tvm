#pragma once

#include "config.h"
#include "cache_file.h"
#include "params.h"

#include <string>

namespace tvm {
namespace auto_cache {

std::string get_hash(std::string workload_key);

Config read_cache_file(std::string cache_file);

TaskData read_log_file(std::string log_file);

Params read_params_file(std::string params_file);

std::string get_transformations(std::string state_str);

}
}

