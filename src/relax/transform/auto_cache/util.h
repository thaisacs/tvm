#pragma once

#include <string>
#include <vector>
#include <iostream>

#include "params.h"
#include "dict.h"
#include "cache_file.h"

namespace tvm {
namespace auto_cache {

struct Item {
    int id;
    std::vector<std::string> files;

    // Comparison operator for sorting by id
    bool operator<(const Item& other) const {
        return id > other.id;
    }
};

std::string GetHash(std::string task_name);

TaskData ReadLogFile(std::string log_file);

Params ReadParamsFile(std::string params_file);

std::string GetTransformations(std::string state_str);

std::string FixParams(std::string task_name, std::string mod_str, std::string record_string);

std::vector<std::string> Split(const std::string& str, const std::string& delimiter);

std::vector<std::string> Split(const std::string& str, char delimiter);

}
}