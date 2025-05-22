#include "config.h"

using namespace llvm::yaml;
using namespace tvm;
using namespace tvm::auto_cache;

void MappingTraits<Config>::mapping(IO &io, Config &config) {
    io.mapRequired("size", config.size);
    io.mapRequired("files", config.files);
}