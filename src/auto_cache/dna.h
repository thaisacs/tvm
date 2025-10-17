#pragma once

#include "util.h"

#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <map>

namespace tvm {
namespace auto_cache {
    class DNA {
    protected:
        std::string gene;
    public:
        DNA(std::string mod_str, std::unique_ptr<Dict> dict);
        std::string DumpGene();
    private:
        std::string ConvertMod2DNA(std::string mod_str, std::unique_ptr<Dict> dict);
        std::string RenameBlock(std::string block_name);
        std::vector<std::string> Split(const std::string& str, const std::string& delimiter);
        std::vector<std::string> Split(const std::string& str, char delimiter);
        std::string DNAGenerator(std::vector<std::string> program_tokens, std::unique_ptr<Dict> dict);
    };
}
}
