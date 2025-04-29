import tvm
from . import _ffi_api

def my_run_test(mod):
    _ffi_api.Run(mod)
