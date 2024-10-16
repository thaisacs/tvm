#include "params.h"

using namespace llvm::yaml;
using namespace tvm;
using namespace tvm::auto_cache;

void MappingTraits<Params>::mapping(IO &io, Params &params) {
  io.mapRequired("TotalCacheSize", params.total_cache_size);
  io.mapRequired("CachePath", params.path);
}
