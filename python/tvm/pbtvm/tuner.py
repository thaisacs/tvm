
from . import _ffi_api

def run_task(task, ssch, runner, builder):
    """Run PBTuner on a single task."""
    return _ffi_api.RunTask(task, ssch, runner, builder)
