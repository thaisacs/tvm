#pragma once

#include "util.h"

#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <map>
#include <regex>

namespace tvm {
namespace auto_cache {
    class Config {
    protected:
        std::unordered_map<std::string, std::vector<std::string>> configs;
    public:
        Config(std::string filename);
        std::vector<Item> GetCacheFiles(std::string mod_dna);
    };
}
}
