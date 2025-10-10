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
        std::string mod2dna(std::string mod_str, std::unique_ptr<Dict> dict);
        std::string rename_block(std::string block_name);
        std::vector<std::string> split(const std::string& str, const std::string& delimiter);
        std::vector<std::string> split(const std::string& str, char delimiter);
        std::string dna_generator(std::vector<std::string> program_tokens, std::unique_ptr<Dict> dict);
    };
}
}
