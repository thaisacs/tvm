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
        DNA(std::string mod_str, std::shared_ptr<Dict> dict, std::string task_name);
        std::string DumpGene();
    private:
        std::string ConvertMod2DNA(std::string mod_str, std::shared_ptr<Dict> dict, std::string task_name);
        std::string RenameBlock(std::string block_name);
        std::string DNAGenerator(std::vector<std::string> program_tokens, std::shared_ptr<Dict> dict);
    };
}
}
