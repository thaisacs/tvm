#include "run_task.h"

#include <tvm/ffi/reflection/registry.h>
#include "../meta_schedule/utils.h"

#include <tvm/meta_schedule/database.h>
#include <tvm/relax/transform.h>
#include <tvm/tir/transform.h>

#include <filesystem>
#include "../tir/schedule/utils.h"

#include "../meta_schedule/module_equality.h"
#include "../meta_schedule/trace_apply.h"

#include <iostream>

namespace tvm {
namespace pbtvm {

std::string get_transformations(std::string state_str) {
    std::string state_new;
    for(long unsigned int i = 0; i < state_str.size(); i++) {
        if(state_str[i] == '\'') {
            state_new.push_back('"');
        }else if(state_str[i] == '"') {
            state_new.push_back('\\');
            state_new.push_back('"');
        }else if(state_str[i] == '\n') {
            i = i + 1;
        }else {
            state_new.push_back(state_str[i]);
        }
    }
    return state_new;
}

void RunTask(ffi::Array<meta_schedule::TuneContext> ctxs,
             std::string ssch, meta_schedule::Runner runner, 
             meta_schedule::Builder builder) {
    const meta_schedule::TuneContext& ctx = ctxs[0];

    std::cout << "Running PBTVM Task: " << ctx->task_name << std::endl;

    std::string record_string = get_transformations(ssch);
    Any json = meta_schedule::JSONLoads(record_string);

    const ObjectRef& json_obj = json.cast<ObjectRef>();
    const ffi::ArrayObj* json_array = json_obj.as<ffi::ArrayObj>();
    if (!json_array || json_array->size() != 2) {
        std::cout << "Invalid JSON Array!" << std::endl;
        return;
    }

    const ObjectRef& decisions_ref = json_array->at(1).cast<ObjectRef>();;
    const ffi::ArrayObj*  decisions_array = decisions_ref.as<ffi::ArrayObj>();
    if (!decisions_array || decisions_array->size() == 0) {
        std::cout << "No decisions found!" << std::endl;
        return;
    }

    const ObjectRef& trace_json = decisions_array->at(0).cast<ObjectRef>();;

    tir::Schedule sch{nullptr};
    try {
        std::cout << "Applying Transformations..." << std::endl;
        sch = tir::Schedule::Traced(
            ctx->mod.value(), /*seed=*/-1, /*debug_mask=*/0,
            tir::ScheduleErrorRenderLevel::kNone
        );
        tir::Trace::ApplyJSONToSchedule(trace_json, sch);
    } catch (...) {
        return;
    }

    if(sch.defined()) {
        std::cout << "Transformed Schedule done... " << std::endl;

        Target target = ctx->target.value();
        ffi::Array<meta_schedule::BuilderInput> builder_inputs;
        builder_inputs.push_back(
            meta_schedule::BuilderInput(sch->mod(), target)
        );
        ffi::Array<meta_schedule::BuilderResult> builder_results = builder->Build(builder_inputs);
        const meta_schedule::MeasureCandidate& candidate = meta_schedule::MeasureCandidate(sch, meta_schedule::ArgInfo::FromEntryFunc(sch->mod(), /*remove_preproc=*/true));
        const meta_schedule::BuilderResult& builder_result = builder_results[0];
        ffi::Array<meta_schedule::RunnerInput> runner_inputs;
        runner_inputs.push_back(meta_schedule::RunnerInput(/*artifact_path=*/builder_result->artifact_path.value(),
                                    /*device_type=*/target->kind->name,
                                    /*args_info=*/candidate->args_info));
        ffi::Array<meta_schedule::RunnerFuture> futures = runner->Run(runner_inputs);
        std::cout << "Running on Runner..." << std::endl;
        auto result = futures[0]->Result();
        // `run_secs` is an Optional<Array<FloatImm>>
        if (result->run_secs.defined()) {
            auto run_secs = result->run_secs.value();  // extract the Array<FloatImm>
            if (!run_secs.empty()) {
                std::cout << "First run time: " << run_secs[0]->value << " seconds" << std::endl;
            } else {
                std::cout << "No run times recorded." << std::endl;
            }
        } else {
            std::cout << "No run_secs available in RunnerResult." << std::endl;
        }
    }
}

TVM_FFI_STATIC_INIT_BLOCK() {
    namespace refl = tvm::ffi::reflection;
    refl::GlobalDef()
        .def_method("pbtvm.RunTask", &RunTask);
}

}  // namespace pbtvm
}  // namespace tvm