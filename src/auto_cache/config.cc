#include "config.h"

using namespace llvm::yaml;
using namespace tvm;
using namespace tvm::auto_cache;

void MappingTraits<Config>::mapping(IO &io, Config &config) {
  io.mapRequired("Size", config.size);
  io.mapRequired("Files", config.files);
}
