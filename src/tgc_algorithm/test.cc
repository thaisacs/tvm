#include "test.h"

#include <tvm/meta_schedule/database.h>
#include <tvm/relax/transform.h>
#include <tvm/relax/tuning_api.h>
#include <tvm/tir/transform.h>

#include "../src/meta_schedule/module_equality.h"
#include "../src/meta_schedule/trace_apply.h"
#include "../src/meta_schedule/utils.h"

namespace tvm {
namespace tgc_algorithm {

void Run(IRModule mod) {
    //Target target = Target::Current();
    Target target = Target("llvm");
    std::string work_dir = "tuning_logs6";

    meta_schedule::Database database{nullptr};
    if (meta_schedule::Database::Current().defined()) {
      database = meta_schedule::Database::Current().value();
    } else {
      //ICHECK(work_dir.defined());
      String path_workload = work_dir + "/database_workload.json";
      String path_tuning_record = work_dir + "/database_tuning_record.json";
      //LOG(WARNING) << "Creating JSONDatabase. Workload at: " << path_workload
      //             << ", Tuning records at: " << path_tuning_record;
      database = meta_schedule::Database::JSONDatabase(path_workload, path_tuning_record, true);
    }

    auto mod_eq_structural = meta_schedule::ModuleEquality::Create("ignore-ndarray");

    for (const auto& iter : mod->functions) {
        GlobalVar gv = iter.first;
        std::cout << gv->name_hint << std::endl;
        if(
            gv->name_hint == "fused_conv2d10_add7" ||
            gv->name_hint == "fused_conv2d4_add3"  ||
            gv->name_hint == "fused_conv2d7_add5"  ||
            gv->name_hint == "fused_conv2d3_add3_relu2"      ||
            gv->name_hint == "fused_conv2d1_add1_add2_relu1" ||
            gv->name_hint == "fused_conv2d1_add1_relu1" ||
            gv->name_hint == "fused_conv2d5_add5_relu3" ||
            gv->name_hint == "fused_conv2d9_add7_add8_relu4" ||
            gv->name_hint == "fused_conv2d6_add5_add6_relu3" ||
            gv->name_hint == "fused_conv2d9_add7_relu4" ||
            gv->name_hint == "fused_conv2d_add_relu" ||
            gv->name_hint == "fused_conv2d6_add5_relu3" ||
            gv->name_hint == "fused_conv2d3_add3_add4_relu2" ||
            gv->name_hint == "fused_conv2d8_add7_relu4" ||
            gv->name_hint == "fused_conv2d2_add3_relu2"
        ) {
            BaseFunc base_func = iter.second;
            const runtime::PackedFunc* normalize_mod_func_;
            normalize_mod_func_ = runtime::Registry::Get("tvm.meta_schedule.normalize_mod");

            if (const auto* prim_func_node = base_func.as<tir::PrimFuncNode>()) {
                tir::PrimFunc prim_func = GetRef<tir::PrimFunc>(prim_func_node);

                IRModule tir_mod = (*normalize_mod_func_)(prim_func);
                if (Optional<meta_schedule::TuningRecord> opt_record =
                        database->QueryTuningRecord(tir_mod, target, gv->name_hint)) {
                    meta_schedule::TuningRecord record = opt_record.value();
                    //std::cout << record->workload->mod << std::endl;
                    tir::Schedule sch{nullptr};
                    if (!mod_eq_structural->Equal(tir_mod, record->workload->mod)) {
                        // When the database lookup succeeds while structural equality check fails,
                        // it implies that the anchor block based equality has been used during tuning.
                        // The trace in the record cannot directly be applied to this query module.
                        sch = tir::Schedule::Traced(
                            tir_mod, /*seed=*/-1, /*debug_mask=*/0,
                            /*error_render_level=*/tir::ScheduleErrorRenderLevel::kDetail);
                        meta_schedule::ScheduleUsingAnchorTrace(sch, record->trace, target);
                    } else {
                        sch = tir::Schedule::Traced(
                            record->workload->mod, /*seed=*/-1, /*debug_mask=*/0,
                            /*error_render_level=*/tir::ScheduleErrorRenderLevel::kDetail);
                        record->trace->ApplyToSchedule(sch, /*remove_postproc=*/false);
                    }

                    //std::cout << sch << std::endl;

                    Array<meta_schedule::MeasureCandidate> candidates;
                    candidates.push_back(
                        meta_schedule::MeasureCandidate(sch, meta_schedule::ArgInfo::FromEntryFunc(sch->mod(), /*remove_preproc=*/true)));

                    static const auto* f_get_local_builder = runtime::Registry::Get("meta_schedule.builder.get_local_builder");
                    meta_schedule::Builder builder = (*f_get_local_builder)();

                    meta_schedule::Runner runner{nullptr};
                    static const auto* f_get_local_runner =
                        runtime::Registry::Get("meta_schedule.runner.get_local_runner");
                    runner = (*f_get_local_runner)();

                    Array<meta_schedule::BuilderInput> builder_inputs;
                    builder_inputs.push_back(meta_schedule::BuilderInput(sch->mod(), target));
                    Array<meta_schedule::BuilderResult> builder_results = builder->Build(builder_inputs);
                    Array<meta_schedule::RunnerInput> runner_inputs;

                    for (const meta_schedule::BuilderResult& builder_result : builder_results) {
                        if (!builder_result->error_msg.defined()) {
                            std::cout << target << std::endl;
                            runner_inputs.push_back(meta_schedule::RunnerInput(
                                            /*artifact_path=*/builder_result->artifact_path.value(),
                                            /*device_type=*/target->kind->name,
                                            /*args_info=*/candidates[0]->args_info));
                        }
                    }

                    Array<meta_schedule::RunnerFuture> futures = runner->Run(runner_inputs);

                    //Array<meta_schedule::RunnerFuture> results;
                    std::vector<double> costs;
                    for (const meta_schedule::RunnerFuture& runner_future : futures) {
                        meta_schedule::RunnerResult runner_result = runner_future->Result();

                        if (runner_result->error_msg.defined()) {
                            costs.push_back(1e10);
                        } else {
                            double sum = 0;
                            for (const FloatImm& cost : runner_result->run_secs.value()) {
                                sum += cost->value;
                                std::cout << cost->value << std::endl;
                            }
                            costs.push_back(sum / runner_result->run_secs.value().size());
                        }
                    }

                }else {
                    std::cout << "Tuning record is not found for primfunc: " << gv->name_hint << std::endl;
                }
            }
        }
    }
}

TVM_REGISTER_GLOBAL("tgc_algorithm.Run").set_body_typed(Run);

} // namespace tgc_algorithm
} // namespace tvm