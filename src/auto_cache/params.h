#pragma once

#include "llvm/Support/YAMLTraits.h"

namespace tvm {
namespace auto_cache {
    struct Params {
        unsigned total_cache_size;
        std::string path;
    };
}
}

template <> struct llvm::yaml::MappingTraits<tvm::auto_cache::Params> {
  static void mapping(llvm::yaml::IO &io, tvm::auto_cache::Params &params);
};
