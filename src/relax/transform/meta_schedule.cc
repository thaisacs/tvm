/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

/*!
 * \file tvm/relax/transform/meta_schedule.cc
 * \brief Pass for meta_schedule tuning
 */
#include <tvm/ffi/reflection/registry.h>
#include <tvm/meta_schedule/database.h>
#include <tvm/relax/transform.h>
#include <tvm/tir/transform.h>

#include "../src/meta_schedule/module_equality.h"
#include "../src/meta_schedule/trace_apply.h"
#include "../src/meta_schedule/utils.h"

#include "auto_cache/dna.h"
#include "auto_cache/util.h"
#include "auto_cache/dict.h"
#include "auto_cache/config.h"

namespace tvm {
namespace relax {
namespace transform {

class MetaScheduleTuner {
 public:
  explicit MetaScheduleTuner(Target target, ffi::String work_dir, Integer max_trials_global,
                             Integer max_trials_per_task,
                             ffi::Optional<ffi::Array<ffi::String>> op_names,
                             ffi::Map<ffi::String, runtime::Tensor> params = {})
      : target_(target),
        work_dir_(work_dir),
        max_trials_global_(max_trials_global),
        max_trials_per_task_(max_trials_per_task),
        op_names_(op_names),
        params_(params) {
    normalize_mod_func_ = tvm::ffi::Function::GetGlobalRequired("tvm.meta_schedule.normalize_mod");
  }

  IRModule TuneIRMod(IRModule mod, transform::PassContext ctx) {
    static ffi::Function tune_relax_func =
        tvm::ffi::Function::GetGlobalRequired("tvm.meta_schedule.tune_relax");
    tune_relax_func(mod, params_, target_, work_dir_, max_trials_global_, max_trials_per_task_,
                    op_names_);
    return mod;
  }

  tir::PrimFunc TuneTIR(tir::PrimFunc f, transform::PassContext ctx) {
    static ffi::Function tune_tir_func =
        tvm::ffi::Function::GetGlobalRequired("tvm.meta_schedule.tune_tir");
    tune_tir_func(normalize_mod_func_(f), target_, work_dir_, max_trials_global_);
    return f;
  }

 private:
  Target target_;
  ffi::String work_dir_;
  Integer max_trials_global_;
  Integer max_trials_per_task_;
  ffi::Optional<ffi::Array<ffi::String>> op_names_;
  ffi::Map<ffi::String, runtime::Tensor> params_;
  tvm::ffi::Function normalize_mod_func_;
};

Pass MetaScheduleApplyTGC(std::string subgraph_cache, bool enable_warning = false) {
  Target target = Target::Current(false);
  const std::optional<tvm::ffi::Function> normalize_mod_func_ =
      tvm::ffi::Function::GetGlobalRequired("tvm.meta_schedule.normalize_mod");

  auto pass_func = [=](IRModule mod, PassContext ctx) {
    auto_cache::Params params = auto_cache::ReadParamsFile(subgraph_cache);

    // Load configs
    std::string config_filename = params.path + "configs.json";
    std::unique_ptr<auto_cache::Config> config = std::make_unique<auto_cache::Config>(config_filename);

    // Load dict
    std::string dict_filename = params.path + "dict.csv";
    if(!std::filesystem::exists(dict_filename)) {
        std::cout << "File " << dict_filename << " does not exist." << std::endl;
        // return; TODO
    }
    std::shared_ptr<auto_cache::Dict> dict = std::make_shared<auto_cache::Dict>(dict_filename);

    std::cout << "Applying MetaSchedule TGC..." << std::endl;
    ffi::Map<GlobalVar, BaseFunc> result;
    auto mod_eq_structural = meta_schedule::ModuleEquality::Create("ignore-tensor");
    for (const auto& iter : mod->functions) {
      GlobalVar gv = iter.first;
      BaseFunc base_func = iter.second;
      if (const auto* prim_func_node = base_func.as<tir::PrimFuncNode>()) {
        tir::PrimFunc prim_func = ffi::GetRef<tir::PrimFunc>(prim_func_node);

        ffi::Optional<IRModule> tir_mod = (*normalize_mod_func_)(prim_func).cast<ffi::Optional<IRModule>>();

        // Convert mod to dna
        std::ostringstream oss;
        oss << tir_mod.value();  // or oss << mod; if not Optional
        std::string mod_string = oss.str();

        std::unique_ptr<auto_cache::DNA> dna_obj = std::make_unique<auto_cache::DNA>(mod_string, dict, gv->name_hint);
        std::string mod_dna = dna_obj->DumpGene();

        std::vector<auto_cache::Item> files = config->GetCacheFiles(mod_dna);

        if(files.size() == 0) {
            std::cout << "No similar cache files found for DNA: " << mod_dna << std::endl;
            //return;
        }

        bool tuned = false;
        for(const auto_cache::Item& item : files) {
          if(!tuned) {
            for(const std::string& file : item.files) {
              auto_cache::TaskData cache_data = auto_cache::ReadLogFile(params.path + file);
              if(cache_data.space.size()) {
                std::string record_string = auto_cache::GetTransformations(cache_data.space[0]);
                Any json = tvm::meta_schedule::JSONLoads(record_string);

                const ObjectRef& json_obj = json.cast<ObjectRef>();
                const ffi::ArrayObj* json_array = json_obj.as<ffi::ArrayObj>();
                if (!json_array || json_array->size() != 2) {
                    continue;
                }

                const ObjectRef& decisions_ref = json_array->at(1).cast<ObjectRef>();
                const ffi::ArrayObj* decisions_array = decisions_ref.as<ffi::ArrayObj>();
                if (!decisions_array || decisions_array->size() == 0) {
                    continue;
                }

                const ObjectRef& trace_json = decisions_array->at(0).cast<ObjectRef>();
                tir::Schedule sch{nullptr};

                try {
                    sch = tir::Schedule::Traced(
                        tir_mod.value(), /*seed=*/-1, /*debug_mask=*/0,
                        tir::ScheduleErrorRenderLevel::kNone
                    );
                    tir::Trace::ApplyJSONToSchedule(trace_json, sch);
                } catch (...) {
                    continue;  // Skip invalid traces
                }

                if(sch.defined()) {
                  std::cout << "done\n" << std::endl;
                  IRModule new_mod = sch->mod();
                  ICHECK_EQ(new_mod->functions.size(), 1);
                  BaseFunc new_base_func = (*new_mod->functions.begin()).second;
                  ICHECK(new_base_func->IsInstance<tir::PrimFuncNode>());
                  tir::PrimFunc tuned_prim_func = Downcast<tir::PrimFunc>(new_base_func);
                  // maintain the original attributes
                  tir::PrimFunc new_prim_func = tir::PrimFunc(/*params=*/tuned_prim_func->params,
                                                              /*body=*/tuned_prim_func->body,
                                                              /*ret_type=*/tuned_prim_func->ret_type,
                                                              /*buffer_map=*/tuned_prim_func->buffer_map,
                                                              /*attrs=*/prim_func->attrs);
                  new_prim_func = WithAttr(std::move(new_prim_func), tir::attr::kIsScheduled, true);
                  result.Set(gv, new_prim_func);
                  tuned = true;
                  break;
                }
              }
            }
          }
        }
        if(tuned) {
            continue;
        }
      }
      result.Set(gv, base_func);
    }
    return IRModule(result,       // functions
                    {},           // map
                    mod->attrs);  // attrs);
    };
  return CreateModulePass(pass_func, 0, "MetaScheduleApplyTGC", {});
}

Pass MetaScheduleApplyDatabase(ffi::Optional<ffi::String> work_dir, bool enable_warning = false) {
  using tvm::meta_schedule::Database;
  Target target = Target::Current(false);
  const std::optional<tvm::ffi::Function> normalize_mod_func_ =
      tvm::ffi::Function::GetGlobalRequired("tvm.meta_schedule.normalize_mod");
  ICHECK(normalize_mod_func_.has_value()) << "Normalization function is not found.";

  auto pass_func = [=](IRModule mod, PassContext ctx) {
    Database database{ffi::UnsafeInit()};
    if (Database::Current().defined()) {
      database = Database::Current().value();
    } else {
      ICHECK(work_dir.has_value());
      std::filesystem::create_directories(work_dir.value().c_str());
      ffi::String path_workload = work_dir.value() + "/database_workload.json";
      ffi::String path_tuning_record = work_dir.value() + "/database_tuning_record.json";
      LOG(WARNING) << "Creating JSONDatabase. Workload at: " << path_workload
                   << ", Tuning records at: " << path_tuning_record;
      database = meta_schedule::Database::JSONDatabase(path_workload, path_tuning_record, true);
    }

    ffi::Map<GlobalVar, BaseFunc> result;
    auto mod_eq_structural = meta_schedule::ModuleEquality::Create("ignore-tensor");
    for (const auto& iter : mod->functions) {
      GlobalVar gv = iter.first;
      BaseFunc base_func = iter.second;
      if (const auto* prim_func_node = base_func.as<tir::PrimFuncNode>()) {
        tir::PrimFunc prim_func = ffi::GetRef<tir::PrimFunc>(prim_func_node);

        IRModule tir_mod = (*normalize_mod_func_)(prim_func).cast<IRModule>();
        if (ffi::Optional<meta_schedule::TuningRecord> opt_record =
                database->QueryTuningRecord(tir_mod, target, gv->name_hint)) {
          meta_schedule::TuningRecord record = opt_record.value();
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
          IRModule new_mod = sch->mod();
          ICHECK_EQ(new_mod->functions.size(), 1);
          BaseFunc new_base_func = (*new_mod->functions.begin()).second;
          ICHECK(new_base_func->IsInstance<tir::PrimFuncNode>());
          tir::PrimFunc tuned_prim_func = Downcast<tir::PrimFunc>(new_base_func);
          // maintain the original attributes
          tir::PrimFunc new_prim_func = tir::PrimFunc(/*params=*/tuned_prim_func->params,
                                                      /*body=*/tuned_prim_func->body,
                                                      /*ret_type=*/tuned_prim_func->ret_type,
                                                      /*buffer_map=*/tuned_prim_func->buffer_map,
                                                      /*attrs=*/prim_func->attrs);
          new_prim_func = WithAttr(std::move(new_prim_func), tir::attr::kIsScheduled, true);
          result.Set(gv, new_prim_func);
          continue;
        } else if (enable_warning) {
          LOG(WARNING) << "Tuning record is not found for primfunc: " << gv->name_hint;
        }
      }
      result.Set(gv, base_func);
    }
    return IRModule(result,       // functions
                    {},           // map
                    mod->attrs);  // attrs);
  };
  return CreateModulePass(pass_func, 0, "MetaScheduleApplyDatabase", {});
}

Pass MetaScheduleTuneIRMod(ffi::Map<ffi::String, runtime::Tensor> params, ffi::String work_dir,
                           Integer max_trials_global,
                           ffi::Optional<Integer> max_trials_per_task = std::nullopt,
                           ffi::Optional<ffi::Array<ffi::String>> op_names = std::nullopt) {
  Target target = Target::Current(false);
  auto pass_func = [=](IRModule m, PassContext ctx) {
    auto max_trials_task = max_trials_per_task.value_or(max_trials_global);
    return MetaScheduleTuner(target, work_dir, max_trials_global, max_trials_task, op_names, params)
        .TuneIRMod(m, ctx);
  };
  return CreateModulePass(/*pass function*/ pass_func, /*opt level*/ 0,
                          /*pass name*/ "MetaScheduleTuneIRModule",
                          /*required*/ {},
                          /*traceable*/ true);
}

Pass MetaScheduleTuneTIR(ffi::String work_dir, Integer max_trials_global) {
  Target target = Target::Current(false);
  ffi::TypedFunction<tir::PrimFunc(tir::PrimFunc, IRModule, PassContext)> pass_func =
      [=](tir::PrimFunc f, IRModule mod, PassContext ctx) {
        return MetaScheduleTuner(target, work_dir, max_trials_global, max_trials_global,
                                 std::nullopt)
            .TuneTIR(f, ctx);
      };
  return tir::transform::CreatePrimFuncPass(/*pass function*/ pass_func, /*opt level*/ 0,
                                            /*pass name*/ "MetaScheduleTuneTIR",
                                            /*required*/ {},
                                            /*traceable*/ true);
}

TVM_FFI_STATIC_INIT_BLOCK() {
  namespace refl = tvm::ffi::reflection;
  refl::GlobalDef()
      .def("relax.transform.MetaScheduleApplyDatabase", MetaScheduleApplyDatabase)
      .def("relax.transform.MetaScheduleApplyTGC", MetaScheduleApplyTGC)
      .def("relax.transform.MetaScheduleTuneIRMod", MetaScheduleTuneIRMod)
      .def("relax.transform.MetaScheduleTuneTIR", MetaScheduleTuneTIR);
}
}  // namespace transform
}  // namespace relax
}  // namespace tvm
