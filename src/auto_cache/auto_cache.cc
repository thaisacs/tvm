#include "auto_cache.h"

#include <tvm/meta_schedule/database.h>
#include <tvm/relax/transform.h>
#include <tvm/relax/tuning_api.h>
#include <tvm/tir/transform.h>
#include "../tir/schedule/utils.h"
#include "../meta_schedule/module_equality.h"
#include "../meta_schedule/trace_apply.h"
#include "../meta_schedule/utils.h"
#include <filesystem>
#include <optional>
#include <tvm/ir/expr.h>
#include <tvm/arith/analyzer.h>
#include <tvm/tir/stmt_functor.h>

namespace tvm {
namespace auto_cache {

TaskGraphCachingAlgorithm::TaskGraphCachingAlgorithm(std::string params_file) {
    Params params = read_params_file(params_file);
    this->path = params.path;
    this->total_cache_size = params.total_cache_size;
    this->target = params.target;
}

bool HasInvalidThreadIdxX(const tir::Schedule& sch) {
    std::optional<tvm::PrimExpr> ref_extent;
    tvm::arith::Analyzer ana;

    for (const auto& kv : sch->mod()->functions) {
        if (const auto* f = kv.second.as<tir::PrimFuncNode>()) {
            tir::PostOrderVisit(f->body, [&](const tvm::ObjectRef& obj) {
                if (const auto* for_node = obj.as<tir::ForNode>()) {
                    if (!for_node->thread_binding.defined()) return;

                    const auto* iv =
                        for_node->thread_binding.as<tir::IterVarNode>();
                    if (!iv) return;

                    if (iv->thread_tag == "threadIdx.x") {
                        if (!ref_extent.has_value()) {
                            ref_extent = for_node->extent;
                        } else if (
                            !ana.CanProveEqual(
                                *ref_extent, for_node->extent)) {
                            throw 1;  // conflito detectado
                        }
                    }
                }
            });
        }
    }
    return false;
}

void TaskGraphCachingAlgorithm::SortDistance(std::vector<TaskData> cache, std::vector<int> distances) {
    std::vector<std::pair<int, TaskData>> tmp;

    for (size_t i = 0; i < cache.size(); ++i) {
        tmp.emplace_back(distances[i], cache[i]);
    }

    std::sort(tmp.begin(), tmp.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });

    for (size_t i = 0; i < tmp.size(); ++i) {
        distances[i] = tmp[i].first;
        cache[i] = tmp[i].second;
    }
}

void TaskGraphCachingAlgorithm::LoadFromFile(Optional<IRModule> mod, std::string task_name) {
    bool isCuda = false;
    if(this->target.find("cuda") != std::string::npos) {
        isCuda = true;
    }

    Target target = Target(this->target);
    std::string hash = get_hash(task_name);

    auto cache_file = this->path + "configs/"+ hash +".yml";
    if(!std::filesystem::exists(cache_file)) {
        return;
    }
    Config data = read_cache_file(cache_file);

    // Convert mod to dna
    std::ostringstream oss;
    oss << mod.value();  // or oss << mod; if not Optional
    std::string mod_string = oss.str();
    std::unique_ptr<DistanceShape> distance_obj = std::make_unique<DistanceShape>(mod_string);

    //std::vector<TaskData> cache;
    //std::vector<int> distances;
    std::vector<std::pair<int, TaskData>> cache;
    for (unsigned i = 0; i < data.size; ++i) {
        std::string log_file = this->path + data.files[i];
        if (!std::filesystem::exists(log_file)) {
            continue;
        }
        TaskData cache_data = read_log_file(log_file);
        //cache.push_back(cache_data);
        ////size_t limit = std::min(value, cache_data.space.size());
        int distance = distance_obj->ComputeDistance(cache_data.shapes);
        //distances.push_back(distance);
        cache.emplace_back(distance, cache_data);
    }

    std::sort(cache.begin(), cache.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });

    //std::cout << "sorted:" << std::endl;
    //for(int p = 0; p < cache.size(); p++) {
    //    std::cout << cache[p].first << std::endl;
    //}
    std::cout << "first distance: " << task_name << " " << cache[0].first << std::endl;

    for (unsigned i = 0; i < cache.size(); ++i) {
        for (size_t j = 0; j < cache[i].second.space.size(); ++j) {
            std::string record_string = get_transformations(cache[i].second.space[j]);
            Any json = JSONLoads(record_string);

            const ObjectRef& json_obj = json.cast<ObjectRef>();
            const ffi::ArrayObj* json_array = json_obj.as<ffi::ArrayObj>();
            if (!json_array || json_array->size() != 2) {
                continue;
            }

            const ObjectRef& decisions_ref = json_array->at(1).cast<ObjectRef>();;
            const ffi::ArrayObj*  decisions_array = decisions_ref.as<ffi::ArrayObj>();
            if (!decisions_array || decisions_array->size() == 0) {
                continue;
            }

            const ObjectRef& trace_json = decisions_array->at(0).cast<ObjectRef>();;
            tir::Schedule sch{nullptr};

            try {
                sch = tir::Schedule::Traced(
                    mod.value(), /*seed=*/-1, /*debug_mask=*/0,
                    //tir::ScheduleErrorRenderLevel::kNone
                    //tir::ScheduleDebugMask::kVerifyGPUCode,
                    tir::ScheduleErrorRenderLevel::kDetail
                );
                tir::Trace::ApplyJSONToSchedule(trace_json, sch);

                if (isCuda and HasInvalidThreadIdxX(sch)) {
                    continue;  // reject trace early
                }
            } catch (...) {
                continue;  // Skip invalid traces
            }

            if(sch.defined()) {
                this->cache.push_back(sch);
            }

            if(this->cache.size() >= 500) {
                break;
            }
        }
        if(this->cache.size() >= 500) {
            break;
        }
    }

    std::cout << "==================\n";
    std::cout << "task name: " << task_name << std::endl;
    std::cout << "hash: " << hash << std::endl;
    std::cout << "cache file: " << cache_file << std::endl;
    std::cout << "files size: " << data.size << std::endl;
    std::cout << "cache size: " << this->cache.size() << std::endl;
    std::cout << "cache log: " << task_name << " " << this->cache.size() << std::endl;
    std::cout << "==================\n";
}

//void TaskGraphCachingAlgorithm::LoadFromFile(Optional<IRModule> mod, std::string task_name) {
//    bool isCuda = false;
//    if(this->target.find("cuda") != std::string::npos) {
//        isCuda = true;
//    }
//
//    Target target = Target(this->target);
//    std::string hash = get_hash(task_name);
//
//    auto cache_file = this->path + "configs/"+ hash +".yml";
//    if(!std::filesystem::exists(cache_file)) {
//        return;
//    }
//    Config data = read_cache_file(cache_file);
//    size_t value = this->total_cache_size/data.size;
//
//    for (unsigned i = 0; i < data.size; ++i) {
//        std::string log_file = this->path + data.files[i];
//        if (!std::filesystem::exists(log_file)) {
//            continue;
//        }
//
//        TaskData cache_data = read_log_file(log_file);
//        size_t limit = std::min(value, cache_data.space.size());
//
//        for (size_t j = 0; j < limit; ++j) {
//            std::string record_string = get_transformations(cache_data.space[j]);
//            Any json = JSONLoads(record_string);
//
//            const ObjectRef& json_obj = json.cast<ObjectRef>();
//            const ffi::ArrayObj* json_array = json_obj.as<ffi::ArrayObj>();
//            if (!json_array || json_array->size() != 2) {
//                continue;
//            }
//
//            const ObjectRef& decisions_ref = json_array->at(1).cast<ObjectRef>();;
//            const ffi::ArrayObj*  decisions_array = decisions_ref.as<ffi::ArrayObj>();
//            if (!decisions_array || decisions_array->size() == 0) {
//                continue;
//            }
//
//            const ObjectRef& trace_json = decisions_array->at(0).cast<ObjectRef>();;
//            tir::Schedule sch{nullptr};
//
//            try {
//                sch = tir::Schedule::Traced(
//                    mod.value(), /*seed=*/-1, /*debug_mask=*/0,
//                    //tir::ScheduleErrorRenderLevel::kNone
//                    //tir::ScheduleDebugMask::kVerifyGPUCode,
//                    tir::ScheduleErrorRenderLevel::kDetail
//                );
//                tir::Trace::ApplyJSONToSchedule(trace_json, sch);
//
//                if (isCuda and HasInvalidThreadIdxX(sch)) {
//                    continue;  // reject trace early
//                }
//            } catch (...) {
//                continue;  // Skip invalid traces
//            }
//
//            if(sch.defined()) {
//                this->cache.push_back(sch);
//            }
//        }
//    }
//
//    std::cout << "==================\n";
//    std::cout << task_name << std::endl;
//    std::cout << hash << std::endl;
//    std::cout << cache_file << std::endl;
//    std::cout << "files size: " << data.size << std::endl;
//    std::cout << "cache size: " << this->cache.size() << std::endl;
//    std::cout << "cache log: " << task_name << " " << this->cache.size() << std::endl;
//    std::cout << "==================\n";
//}

std::vector<tir::Schedule> TaskGraphCachingAlgorithm::SampleInitPopulation(int num) {
    std::vector<tir::Schedule> candidates;
    std::cout << "==================\n";
    std::cout << "cache hit or miss: " << this->cache.size() << std::endl;
    std::cout << "num: " << num << std::endl;
    std::cout << "==================\n";

    if(num > static_cast<int>(this->cache.size()))
        num = static_cast<int>(this->cache.size());
    for (int idx = 0; idx < num; idx++) {
        candidates.push_back(this->cache[idx]);
    }
    return std::move(candidates);
}

void Run(IRModule mod) {

}

TVM_REGISTER_GLOBAL("auto_cache.Run").set_body_typed(Run);

} // namespace tgc_algorithm
} // namespace tvm
