#ifndef RUN_TASK_H
#define RUN_TASK_H

#include "../meta_schedule/utils.h"

namespace tvm {
namespace pbtvm {

void RunTask(ffi::Array<meta_schedule::TuneContext> ctxs, 
             std::string ssch, meta_schedule::Runner runner,
             meta_schedule::Builder builder);

}  // namespace pbtvm
}  // namespace tvm

#endif