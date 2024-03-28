from tvm.driver import tvmc
import tvm
from copy import deepcopy
from tvm.driver.tvmc.autotuner import autoscheduler_get_tuning_tasks
from tvm import auto_scheduler
from tvm.auto_scheduler import MeasureInput, MeasureResult
from tvm.ir.container import Array

import tvm._ffi
from . import _ffi_api

import json
import io

def get_state(task, inp):
    return task.compute_dag.infer_bound_from_state(inp.state)

def get_search_space(task, inputs, results):
    new_inputs = []
    new_results = []
    task_wk = task.workload_key
    task_wk = json.loads(task_wk)[0]
    for i in range(len(inputs)):
        input_wk = inputs[i].task.workload_key
        input_wk = json.loads(input_wk)[0]
        if(task_wk == input_wk):
            new_inputs.append(inputs[i])
            new_results.append(results[i])
    return new_inputs

def get_search_space_(inputs):
    new_inputs = []
    for i in range(len(inputs)):
        new_inputs.append(inputs[i])
    return new_inputs

def load_model(model_filename):
    return tvmc.load(model_filename)

def run_task(task, state):
    device = 0
    res = _ffi_api.Run(task, state, 1000, 100, 10, 100, 1, False, device, 1, "default")
    return res

def pbtvm_main(model_filename, log_filename):
    tvmc_model = load_model(model_filename)

    mod = deepcopy(tvmc_model.mod)
    params = tvmc_model.params
    target = tvm.target.Target("llvm")
    
    tasks, weights = autoscheduler_get_tuning_tasks(
        mod=mod,
        params=params,
        target=target,
    )


    for task in tasks:
        auto_scheduler.workload_registry.register_workload_tensors(task.workload_key, task.compute_dag.tensors)

    id = 0
    inputs, results = auto_scheduler.RecordReader(log_filename).read_lines()
    #ss_inputs = get_search_space_(inputs)
    for task in tasks:
        #ss_inputs = get_search_space(task, inputs, results)
        states = []
        for i in range(len(inputs)):
            try:
                states.append(get_state(task, inputs[i]))
            except:
                ...
        for state in states:
            inp = [MeasureInput(task, state)]
            res = _ffi_api.Run(task, state.state_object)
            _ffi_api.SaveRecords("test.log", inp, res);

        id = id + 1

def run_task_search_space(task, log_filename):
    id = 0
    inputs, results = auto_scheduler.RecordReader(log_filename).read_lines()
    states = []
    for i in range(len(inputs)):
        try:
            states.append(get_state(task, inputs[i]))
        except:
            ...

    device = 0
    for state in states:
        inp = [MeasureInput(task, state)]
        res = _ffi_api.Run(task, state.state_object, 1000, 100, 10, 100, 1, False, device, 1, "default")
        _ffi_api.SaveRecords("test.log", inp, res);

    id = id + 1

def save_record(inp, res, filename):
    _ffi_api.SaveRecords(filename, inp, res);
