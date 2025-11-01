#include "cache_file.h"

using namespace llvm::yaml;
using namespace tvm;
using namespace tvm::auto_cache;

void MappingTraits<TaskData>::mapping(IO &io, TaskData &data) {
    io.mapRequired("id", data.id);
    io.mapRequired("dna", data.dna);
    io.mapRequired("space", data.space);
}