#ifndef LOAD_STATES_H
#define LOAD_STATES_H

#include <tvm/arith/analyzer.h>
#include <tvm/auto_scheduler/auto_schedule.h>
#include <tvm/auto_scheduler/measure.h>
#include <tvm/auto_scheduler/search_policy.h>
#include <tvm/runtime/registry.h>
#include <tvm/tir/builtin.h>
#include <tvm/tir/expr_functor.h>

namespace tvm {
namespace autokcache {

void load_file();

}  // namespace autokcache
}  // namespace tvm

#endif
