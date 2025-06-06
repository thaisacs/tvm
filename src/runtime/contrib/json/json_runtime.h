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
 * \file src/runtime/contrib/json/json_runtime.h
 * \brief Utilities for json runtime.
 */

#ifndef TVM_RUNTIME_CONTRIB_JSON_JSON_RUNTIME_H_
#define TVM_RUNTIME_CONTRIB_JSON_JSON_RUNTIME_H_

#include <tvm/runtime/module.h>
#include <tvm/runtime/ndarray.h>
#include <tvm/runtime/profiling.h>

#include <cstddef>
#include <string>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "json_node.h"

namespace tvm {
namespace runtime {
namespace json {

/*!
 * \brief A json runtime that executes the serialized JSON format. This runtime
 * can be extended by user defined runtime for execution.
 */
class JSONRuntimeBase : public ModuleNode {
 public:
  JSONRuntimeBase(const std::string& symbol_name, const std::string& graph_json,
                  const Array<String> const_names)
      : symbol_name_(symbol_name), graph_json_(graph_json), const_names_(const_names) {
    LoadGraph(graph_json_);
  }

  ~JSONRuntimeBase() override = default;

  const char* type_key() const override { return "json"; }  // May be overridden

  /*! \brief Get the property of the runtime module .*/
  int GetPropertyMask() const override {
    return ModulePropertyMask::kBinarySerializable | ModulePropertyMask::kRunnable;
  }

  /*! \brief Initialize a specific json runtime. */
  virtual void Init(const Array<NDArray>& consts) = 0;

  /*! \brief Invoke the execution engine to inteprete a specific json runtime. */
  virtual void Run() = 0;

  /*! \brief Does the backend support debug & profiling */
  virtual bool CanDebug() { return false; }

  /*!
   * \brief Invoke the profiler
   * \param pointer to profiler
   */
  virtual void RunProfile(profiling::Profiler* prof) {
    LOG(FATAL) << "Not expected to be here : Profiling call w/o support ?";
  }

  /*!
   * \brief Invoke the debugger
   * \return External compiler specific debug blob
   */
  virtual std::string DebugDump(void) {
    LOG(FATAL) << "Not expected to be here : Debug dump w/o support ?";
  }

  /*!
   * \brief Get a packed function.
   * \param name The name/symbol of the function.
   * \param sptr_to_self The pointer to the module node.
   * \return The packed function.
   */
  ffi::Function GetFunction(const String& name, const ObjectPtr<Object>& sptr_to_self) override {
    if (name == "get_symbol") {
      return ffi::Function(
          [sptr_to_self, this](ffi::PackedArgs args, ffi::Any* rv) { *rv = this->symbol_name_; });
    } else if (name == "get_const_vars") {
      return ffi::Function(
          [sptr_to_self, this](ffi::PackedArgs args, ffi::Any* rv) { *rv = this->const_names_; });
    } else if (this->symbol_name_ == name) {
      return ffi::Function([sptr_to_self, this](ffi::PackedArgs args, ffi::Any* rv) {
        ICHECK(this->initialized_) << "The module has not been initialized";

        // Bind argument tensors to data entries.
        this->SetInputOutputBuffers(args);

        // Execute the subgraph.
        this->Run();
      });
    } else if (this->symbol_name_ + "_debug" == name) {
      // NOTE: the current debug convention is not very compatible with
      // the FFI convention, consider clean up
      if (!this->CanDebug()) {
        return ffi::Function(nullptr);
      }
      return ffi::Function([sptr_to_self, this](ffi::PackedArgs args, ffi::Any* rv) {
        ICHECK(this->initialized_) << "The module has not been initialized";

        // Bind argument tensors to data entries.
        this->SetInputOutputBuffers(args);

        if (auto opt_str = rv->try_cast<String>()) {
          String purpose = std::move(opt_str.value());
          if ("debug_dump" == purpose) {
            *rv = this->DebugDump();
          }
        } else {
          // Profile the subgraph.
          profiling::Profiler* prof = static_cast<profiling::Profiler*>(rv->cast<void*>());
          this->RunProfile(prof);
        }
        // String vendor_prof = this->RunProfile(prof);
      });
    } else if ("__init_" + this->symbol_name_ == name) {
      // The function to initialize constant tensors.
      return ffi::Function([sptr_to_self, this](ffi::PackedArgs args, ffi::Any* rv) {
        ICHECK_EQ(args.size(), 1U);
        std::lock_guard<std::mutex> guard(this->initialize_mutex_);
        if (!this->initialized_) {
          this->Init(args[0].cast<Array<NDArray>>());
          this->initialized_ = true;
        }
        *rv = 0;
      });
    } else {
      return ffi::Function(nullptr);
    }
  }

  void SaveToBinary(dmlc::Stream* stream) override {
    // Save the symbol
    stream->Write(symbol_name_);
    // Save the graph
    stream->Write(graph_json_);
    // Save the required const names
    std::vector<std::string> consts;
    for (const auto& it : const_names_) {
      consts.push_back(it);
    }
    stream->Write(consts);
  }

  template <typename T,
            typename = typename std::enable_if<std::is_base_of<JSONRuntimeBase, T>::value>::type>
  static Module LoadFromBinary(void* strm) {
    dmlc::Stream* stream = static_cast<dmlc::Stream*>(strm);
    std::string symbol;
    std::string graph_json;
    std::vector<std::string> consts;
    // Load the symbol
    ICHECK(stream->Read(&symbol)) << "Loading symbol name failed";
    ICHECK(stream->Read(&graph_json)) << "Loading graph json failed";
    ICHECK(stream->Read(&consts)) << "Loading the const name list failed";
    Array<String> const_names;
    for (const auto& it : consts) {
      const_names.push_back(it);
    }
    auto n = make_object<T>(symbol, graph_json, const_names);
    return Module(n);
  }

  /*!
   * \brief Get the JSON generated by codegen.
   *
   * \param format the format to return.
   * \return A string of JSON.
   */
  String GetSource(const String& format = "json") override { return graph_json_; }

 protected:
  /*!
   * \brief Set up the input and output buffers by binding their DLTensor pointers to the
   * corresponding data entry.
   *
   * \param args The packed args.
   */
  void SetInputOutputBuffers(const ffi::PackedArgs& args) {
    ICHECK_EQ(args.size(), input_var_eid_.size() + outputs_.size())
        << "Found mismatch in the number of provided data entryies and required.";

    for (size_t i = 0; i < static_cast<size_t>(args.size()); i++) {
      auto eid = i < input_var_eid_.size() ? input_var_eid_[i]
                                           : EntryID(outputs_[i - input_var_eid_.size()]);

      const DLTensor* arg;
      if (auto opt_nd = args[i].as<NDArray>()) {
        NDArray arr = opt_nd.value();
        arg = arr.operator->();
      } else {
        arg = args[i].cast<DLTensor*>();
      }

      // Assign input/output the NDArray pointers to data entry so that we can directly
      // read/write host buffers.
      data_entry_[eid] = arg;
    }
  }

  /*!
   * \brief Load the graph and record the entries for inputs and constants.
   *
   * \param graph_json The graph in the json format.
   */
  void LoadGraph(const std::string& graph_json) {
    std::istringstream is(graph_json);
    dmlc::JSONReader reader(&is);
    this->Load(&reader);
    std::vector<std::string> consts;
    for (size_t i = 0; i < input_nodes_.size(); i++) {
      uint32_t nid = input_nodes_[i];
      std::string name = nodes_[nid].name_;
      if (nodes_[nid].op_type_ == "input") {
        ICHECK_EQ(nodes_[nid].GetOpShape().size(), nodes_[nid].GetOpDataType().size());
        for (size_t j = 0; j < nodes_[nid].GetOpShape().size(); ++j) {
          input_var_eid_.push_back(EntryID(nid, j));
        }
        nodes_[nid].SetNumOutput(nodes_[nid].GetOpShape().size());
      } else {
        ICHECK_EQ(nodes_[nid].op_type_, "const");
        auto pos = std::find(std::begin(const_names_), std::end(const_names_), name);
        ICHECK(pos != std::end(const_names_)) << "Found non-existent constant: " << name;
        const_idx_.push_back(nid);
        consts.push_back(name);
      }
    }
    ICHECK_EQ(consts.size(), const_names_.size())
        << "Found mismatch for the number of constants in the graph and required.";

    for (size_t i = 0; i < consts.size(); i++) {
      ICHECK_EQ(consts[i], const_names_[i])
          << "The position of constant in the graph must be the same as the required.";
    }

    // Reserve data entries.
    data_entry_.resize(NumEntries());
  }

  /*!
   * \brief Set up the constants/weights for inference by binding their DLTensor pointer to
   * the corresponding data entry.
   *
   * \param consts A list of constant NDArray to be used.
   */
  void SetupConstants(const Array<NDArray>& consts) {
    for (size_t i = 0; i < consts.size(); ++i) {
      data_entry_[EntryID(const_idx_[i], 0)] = consts[i].operator->();
    }
  }

  // Load the graph.
  void Load(dmlc::JSONReader* reader) {
    reader->BeginObject();
    std::string key;
    std::string symbol_;
    while (reader->NextObjectItem(&key)) {
      if (key == "nodes") {
        reader->Read(&nodes_);
      } else if (key == "arg_nodes") {
        reader->Read(&input_nodes_);
      } else if (key == "node_row_ptr") {
        reader->Read(&node_row_ptr_);
      } else if (key == "heads") {
        reader->Read(&outputs_);
      } else if (key == "symbol") {
        reader->Read(&symbol_);
      } else {
        LOG(FATAL) << "Unknown key: " << key;
      }
    }
  }

  // Get the node entry index.
  uint32_t EntryID(uint32_t nid, uint32_t index) const { return node_row_ptr_[nid] + index; }

  // Get the node entry index.
  uint32_t EntryID(const JSONGraphNodeEntry& e) const { return EntryID(e.id_, e.index_); }

  // Number of node entries.
  uint32_t NumEntries() const { return node_row_ptr_.back(); }

 protected:
  /*! \brief The only subgraph name for this module. */
  std::string symbol_name_;
  /*! \brief The graph. */
  std::string graph_json_;
  /*! \brief The required constant names. */
  Array<String> const_names_;
  /*! \brief The json graph nodes. */
  std::vector<JSONGraphNode> nodes_;
  /*! \brief The input nodes, including variables and constants. */
  std::vector<uint32_t> input_nodes_;
  /*! \brief Used for quick entry indexing. */
  std::vector<uint32_t> node_row_ptr_;
  /*! \brief Output entries. */
  std::vector<JSONGraphNodeEntry> outputs_;
  /*! \brief Data of that entry. */
  std::vector<const DLTensor*> data_entry_;
  /*! \brief Map the input name to entry id. */
  std::vector<uint32_t> input_var_eid_;
  /*! \brief input const node index. */
  std::vector<uint32_t> const_idx_;
  /*! \brief Indicate if the engine has been initialized. */
  bool initialized_{false};
  /*! \brief Initializer mutex*/
  std::mutex initialize_mutex_;
};

}  // namespace json
}  // namespace runtime
}  // namespace tvm
#endif  // TVM_RUNTIME_CONTRIB_JSON_JSON_RUNTIME_H_
