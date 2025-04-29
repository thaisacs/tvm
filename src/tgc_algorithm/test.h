#ifndef TGC_TEST_H
#define TGC_TEST_H

#include <iostream>
#include <tvm/runtime/registry.h>

#include <tvm/ir/function.h>
#include <tvm/relax/expr_functor.h>

namespace tvm {
namespace tgc_algorithm {

void Run(IRModule mod);

} // namespace tgc_algorithm
} // namespace tvm

#endif