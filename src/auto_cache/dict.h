#pragma once

#include <iostream>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <map>
#include <unordered_map>
#include <string>

namespace tvm {
namespace auto_cache {
    class Dict {
    protected:
        std::unordered_map<std::string, std::string> dict;
    public:
        Dict();
        Dict(std::string filename);
        void LoadFromFile(std::string file);
        //std::string FindToken(std::string token);
        std::string FindToken(const std::string& token);
        bool Has(std::string token);
    };
}
}
