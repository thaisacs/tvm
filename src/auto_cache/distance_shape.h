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
    class DistanceShape {
    protected:
        std::vector<int> shapes;
    public:
        DistanceShape(std::string mod_str);
        int ComputeDistance(std::string sequence_shapes);
    private:
        void setShapes(std::string mod_str);
        std::string RenameBlock(std::string block_name);
        std::vector<std::string> Split(const std::string& str, const std::string& delimiter);
        std::vector<std::string> Split(const std::string& str, char delimiter);
        void dumpShapes(std::vector<int> shapes);
    };
}
}
