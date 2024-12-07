#pragma once

#include "llvm/Support/YAMLTraits.h"

namespace tvm {
namespace auto_cache {
    struct Config {
        unsigned size;
        std::vector<std::string> files;
    };
}
}

template <> struct llvm::yaml::MappingTraits<tvm::auto_cache::Config> {
  static void mapping(llvm::yaml::IO &io, tvm::auto_cache::Config &config);
};
