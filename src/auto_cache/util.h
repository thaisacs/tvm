#pragma once

#include <string>
#include <vector>
#include <iostream>

#include "params.h"
#include "dict.h"
#include "cache_file.h"

namespace tvm {
namespace auto_cache {

std::string GetHash(std::string task_name);

TaskData ReadLogFile(std::string log_file);

Params ReadParamsFile(std::string params_file);

std::string GetTransformations(std::string state_str);

}
}