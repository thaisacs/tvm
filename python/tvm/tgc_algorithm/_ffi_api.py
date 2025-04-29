"""FFI API for TGC."""
import tvm._ffi

tvm._ffi._init_api("tgc_algorithm", __name__)