#include "auto_cache.h"
#include "util.h"

#include <tvm/meta_schedule/database.h>
#include <tvm/relax/transform.h>
#include <tvm/relax/tuning_api.h>
#include <tvm/tir/transform.h>

#include "../tir/schedule/utils.h"

#include "../meta_schedule/module_equality.h"
#include "../meta_schedule/trace_apply.h"
#include "../meta_schedule/utils.h"

#include <iostream>
#include <sstream>
#include <vector>
#include <string>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <map>

extern "C" int nw_cmdline(const char*, const char*);

namespace tvm {
namespace auto_cache {

TaskGraphCachingAlgorithm::TaskGraphCachingAlgorithm(std::string params_file) {
    Params params = read_params_file(params_file);
    this->path = params.path;
    this->total_cache_size = params.total_cache_size;
}

void TaskGraphCachingAlgorithm::LoadFromFile(Optional<IRModule> mod, std::string task_name) {
    // Load dict
    std::string filename = this->path + "dict.csv";
    if(!std::filesystem::exists(filename)) {
        std::cout << "File " << filename << " does not exist." << std::endl;
        return;
    }
    std::unique_ptr<Dict> dict = std::make_unique<Dict>(filename);

    std::ostringstream oss;
    oss << mod.value();  // or oss << mod; if not Optional
    std::string mod_string = oss.str();
    std::unique_ptr<DNA> dna_obj = std::make_unique<DNA>(mod_string, move(dict));
    std::string test_dna = dna_obj->DumpGene();
    std::cout << "DNA: " << test_dna << std::endl;

    int best_similarity = -100000;
    std::string best_filename;
    // Load cache
    for (const auto& entry : std::filesystem::directory_iterator(this->path)) {
        if (entry.is_regular_file()) {
            std::string file_type = entry.path().filename().string().substr(0, 6);
            if(file_type == "cachex") {
                std::string file_path = this->path + entry.path().filename().string();
                TaskData cache_data = read_log_file(file_path);

                int similarity_value = nw_cmdline(test_dna.c_str(), cache_data.dna.c_str());
                if(similarity_value > best_similarity) {
                    best_similarity = similarity_value;
                    best_filename = file_path;
                }
            }
        }
    }

    TaskData cache_data = read_log_file(best_filename);
    tvm::relax::Trace trace{nullptr};
    Optional<Array<FloatImm>> run_secs{nullptr};

    std::string record_string = get_transformations(cache_data.space[0]);
    Any json = JSONLoads(record_string);

    const ObjectRef& json_obj = json.cast<ObjectRef>();
    const ffi::ArrayObj* json_array = json_obj.as<ffi::ArrayObj>();
    if (!json_array || json_array->size() != 2) {
        //continue;
        return;
    }

    const ObjectRef& decisions_ref = json_array->at(1).cast<ObjectRef>();
    const ffi::ArrayObj* decisions_array = decisions_ref.as<ffi::ArrayObj>();
    if (!decisions_array || decisions_array->size() == 0) {
        //continue;
        return;
    }

    const ObjectRef& trace_json = decisions_array->at(0).cast<ObjectRef>();
    tir::Schedule sch{nullptr};

    try {
        sch = tir::Schedule::Traced(
            mod.value(), /*seed=*/-1, /*debug_mask=*/0,
            tir::ScheduleErrorRenderLevel::kNone
        );
        tir::Trace::ApplyJSONToSchedule(trace_json, sch);
        std::cout << "success..." << std::endl;
    } catch (...) {
        std::cout << "error..." << std::endl;
        //continue;  // Skip invalid traces
        return;
    }

    if(sch.defined()) {
        this->cache.push_back(sch);
    }

    std::cout << "===================" << std::endl;
    std::cout << "Best similarity: " << best_similarity << std::endl;
    std::cout << "Test DNA: " << test_dna.size() << std::endl;
    std::cout << "cache size: " << this->cache.size() << std::endl;
    std::cout << "===================" << std::endl;

    //std::cout << "==================\n";
    //std::cout << task_name << std::endl;
    //std::cout << hash << std::endl;
    //std::cout << cache_file << std::endl;
    //std::cout << "files size: " << data.size << std::endl;
    //std::cout << "==================\n";
}

std::vector<tir::Schedule> TaskGraphCachingAlgorithm::SampleInitPopulation(int num) {
    std::vector<tir::Schedule> candidates;
    if(num > static_cast<int>(this->cache.size()))
        num = static_cast<int>(this->cache.size());
    for (int idx = 0; idx < num; idx++) {
        candidates.push_back(this->cache[idx]);
    }
    std::cout << "candidates size: " << candidates.size() << std::endl;
    return std::move(candidates);
}

void Run(IRModule mod) {
//    Target target = Target("llvm");
//    std::string work_dir = "tuning_logs6";
//
//    meta_schedule::Database database{nullptr};
//    if (meta_schedule::Database::Current().defined()) {
//      database = meta_schedule::Database::Current().value();
//    } else {
//      //ICHECK(work_dir.defined());
//      String path_workload = work_dir + "/database_workload.json";
//      String path_tuning_record = work_dir + "/database_tuning_record.json";
//      //LOG(WARNING) << "Creating JSONDatabase. Workload at: " << path_workload
//      //             << ", Tuning records at: " << path_tuning_record;
//      database = meta_schedule::Database::JSONDatabase(path_workload, path_tuning_record, true);
//    }
//
//    auto mod_eq_structural = meta_schedule::ModuleEquality::Create("ignore-ndarray");
//
//    for (const auto& iter : mod->functions) {
//        GlobalVar gv = iter.first;
//        std::cout << gv->name_hint << std::endl;
//        if(
//            gv->name_hint == "fused_conv2d2_add3_relu2"
//        ) {
//            BaseFunc base_func = iter.second;
//            //const runtime::PackedFunc* normalize_mod_func_;
//            //normalize_mod_func_ = runtime::Registry::Get("tvm.meta_schedule.normalize_mod");
//            const std::optional<tvm::ffi::Function> normalize_mod_func_ =
//                tvm::ffi::Function::GetGlobalRequired("tvm.meta_schedule.normalize_mod");
//
//            if (const auto* prim_func_node = base_func.as<tir::PrimFuncNode>()) {
//                tir::PrimFunc prim_func = GetRef<tir::PrimFunc>(prim_func_node);
//
//                IRModule tir_mod = (*normalize_mod_func_)(prim_func).cast<IRModule>();
//                if (Optional<meta_schedule::TuningRecord> opt_record =
//                        database->QueryTuningRecord(tir_mod, target, gv->name_hint)) {
//                    meta_schedule::TuningRecord record = opt_record.value();
//                    //std::cout << record->workload->mod << std::endl;
//                    tir::Schedule sch{nullptr};
//                    if (!mod_eq_structural->Equal(tir_mod, record->workload->mod)) {
//                        // When the database lookup succeeds while structural equality check fails,
//                        // it implies that the anchor block based equality has been used during tuning.
//                        // The trace in the record cannot directly be applied to this query module.
//                        sch = tir::Schedule::Traced(
//                            tir_mod, /*seed=*/-1, /*debug_mask=*/0,
//                            /*error_render_level=*/tir::ScheduleErrorRenderLevel::kDetail);
//                        meta_schedule::ScheduleUsingAnchorTrace(sch, record->trace, target);
//                    } else {
//                        sch = tir::Schedule::Traced(
//                            record->workload->mod, /*seed=*/-1, /*debug_mask=*/0,
//                            /*error_render_level=*/tir::ScheduleErrorRenderLevel::kDetail);
//                        record->trace->ApplyToSchedule(sch, /*remove_postproc=*/false);
//                    }
//
//                    //std::cout << sch << std::endl;
//
//                    Array<meta_schedule::MeasureCandidate> candidates;
//                    candidates.push_back(
//                        meta_schedule::MeasureCandidate(sch, meta_schedule::ArgInfo::FromEntryFunc(sch->mod(), /*remove_preproc=*/true)));
//
//                    //static const auto* f_get_local_builder = runtime::Registry::Get("meta_schedule.builder.get_local_builder");
//                    //meta_schedule::Builder builder = (*f_get_local_builder)();
//                    // fetch a local builder
//                    static const auto f_get_local_builder =
//                        tvm::ffi::Function::GetGlobalRequired("meta_schedule.builder.get_local_builder");
//                    meta_schedule::Builder builder = f_get_local_builder().cast<meta_schedule::Builder>();
//
//                    meta_schedule::Runner runner{nullptr};
//                    //static const auto* f_get_local_runner =
//                    //    runtime::Registry::Get("meta_schedule.runner.get_local_runner");
//                    //runner = (*f_get_local_runner)();
//                    static const auto f_get_local_runner =
//                        tvm::ffi::Function::GetGlobalRequired("meta_schedule.runner.get_local_runner");
//                    runner = f_get_local_runner().cast<meta_schedule::Runner>();
//
//                    Array<meta_schedule::BuilderInput> builder_inputs;
//                    builder_inputs.push_back(meta_schedule::BuilderInput(sch->mod(), target));
//                    Array<meta_schedule::BuilderResult> builder_results = builder->Build(builder_inputs);
//                    Array<meta_schedule::RunnerInput> runner_inputs;
//
//                    for (const meta_schedule::BuilderResult& builder_result : builder_results) {
//                        if (!builder_result->error_msg.defined()) {
//                            std::cout << target << std::endl;
//                            runner_inputs.push_back(meta_schedule::RunnerInput(
//                                            /*artifact_path=*/builder_result->artifact_path.value(),
//                                            /*device_type=*/target->kind->name,
//                                            /*args_info=*/candidates[0]->args_info));
//                        }
//                    }
//
//                    Array<meta_schedule::RunnerFuture> futures = runner->Run(runner_inputs);
//
//                    std::vector<double> costs;
//                    for (const meta_schedule::RunnerFuture& runner_future : futures) {
//                        meta_schedule::RunnerResult runner_result = runner_future->Result();
//
//                        if (runner_result->error_msg.defined()) {
//                            costs.push_back(1e10);
//                        } else {
//                            double sum = 0;
//                            for (const FloatImm& cost : runner_result->run_secs.value()) {
//                                sum += cost->value;
//                                std::cout << cost->value << std::endl;
//                            }
//                            costs.push_back(sum / runner_result->run_secs.value().size());
//                        }
//                    }
//
//                }else {
//                    std::cout << "Tuning record is not found for primfunc: " << gv->name_hint << std::endl;
//                }
//            }
//        }
//    }
}

TVM_REGISTER_GLOBAL("auto_cache.Run").set_body_typed(Run);

} // namespace tgc_algorithm
} // namespace tvm
