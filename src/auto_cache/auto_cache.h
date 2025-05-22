#ifndef TGC_TEST_H
#define TGC_TEST_H

#include <tvm/meta_schedule/database.h>
#include <tvm/runtime/registry.h>
#include <tvm/ir/function.h>
#include <tvm/relax/expr_functor.h>

#include <iostream>

using namespace tvm;
using namespace tvm::meta_schedule;

namespace tvm {
namespace auto_cache {

class TaskGraphCachingAlgorithm {
    /*!
     * \brief The path of cache.
     */
    std::string path;

    /*! \brief Total cache size. */
    size_t total_cache_size;

    /*! \brief The cache map for our auto cache. */
    std::vector<tir::Schedule> cache;
public:
    TaskGraphCachingAlgorithm(std::string params_file);
    void LoadFromFile(Optional<IRModule> mod, std::string task_name);
    std::vector<tir::Schedule> SampleInitPopulation(int num);
};

void Run(IRModule mod);

} // namespace tgc_algorithm
} // namespace tvm

#endif