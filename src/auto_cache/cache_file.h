#pragma once

#include "llvm/Support/YAMLTraits.h"

namespace tvm {
namespace auto_cache {
    struct TaskData {
        unsigned id;
        std::string hash;
        std::vector<std::string> space;
    };
}
}

template <> struct llvm::yaml::MappingTraits<tvm::auto_cache::TaskData> {
    static void mapping(llvm::yaml::IO &io, tvm::auto_cache::TaskData &data);
};