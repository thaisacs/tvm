# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.
import numpy as np

import tvm
import tvm.testing
from tvm import te
from tvm.script import tir as T
from tvm import tir
from tvm.tir.tensor_intrin.hexagon import VRMPY_u8u8i32_INTRIN


def _check(original, transformed):
    func = original
    mod = tvm.IRModule.from_expr(func.with_attr("global_symbol", "main"))
    mod = tvm.tir.transform.PlanAndUpdateBufferAllocationLocation()(mod)
    tvm.ir.assert_structural_equal(mod["main"], transformed.with_attr("global_symbol", "main"))


@T.prim_func
def element_func(a: T.handle, c: T.handle) -> None:
    A = T.match_buffer(a, (16, 16))
    C = T.match_buffer(c, (16, 16))
    B = T.alloc_buffer((16, 16))
    for i0 in range(0, 16):
        for j0 in range(0, 16):
            with T.block():
                i, j = T.axis.remap("SS", [i0, j0])
                B[i, j] = A[i, j] + 1.0
        for j0 in range(0, 16):
            with T.block():
                i, j = T.axis.remap("SS", [i0, j0])
                C[i, j] = B[i, j] * 2.0


@T.prim_func
def transformed_element_func(a: T.handle, c: T.handle) -> None:
    A = T.match_buffer(a, [16, 16])
    C = T.match_buffer(c, [16, 16])

    for i_0 in range(0, 16):
        with T.block():
            T.reads([A[i_0, 0:16]])
            T.writes([C[i_0, 0:16]])
            B = T.alloc_buffer([16, 16])
            for j_0 in T.serial(0, 16):
                with T.block():
                    i, j = T.axis.remap("SS", [i_0, j_0])
                    B[i, j] = A[i, j] + 1.0
            for j_0 in T.serial(0, 16):
                with T.block():
                    i, j = T.axis.remap("SS", [i_0, j_0])
                    C[i, j] = B[i, j] * 2.0


@T.prim_func
def original_func() -> None:
    A = T.alloc_buffer((128, 128), "float32")
    for i0, j0 in T.grid(128, 128):
        with T.block():
            i, j = T.axis.remap("SS", [i0, j0])
            A[i, j] = T.float32(0)
    for i0, j0, k0 in T.grid(32, 32, 32):
        with T.block():
            i, j, k = T.axis.remap("SSR", [i0, j0, k0])
            B = T.alloc_buffer((128, 128), "float32")
            C = T.alloc_buffer((128, 128), "float32")
            D = T.alloc_buffer((128, 128), "float32")
            if k == 0:
                for ii, jj in T.grid(4, 4):
                    B[i * 4 + ii, j * 4 + jj] = A[i * 4 + ii, j * 4 + jj]
            for ii, jj in T.grid(4, 4):
                for kk in range(0, 4):
                    B[i * 4 + ii, j * 4 + jj] += C[i * 4 + ii, k * 4 + kk]
                for kk in range(0, 4):
                    B[i * 4 + ii, j * 4 + jj] += (
                        D[j * 4 + jj, k * 4 + kk] * C[i * 4 + ii, k * 4 + kk]
                    )


@T.prim_func
def transformed_func() -> None:
    A = T.alloc_buffer([128, 128])
    for i0, j0 in T.grid(128, 128):
        with T.block():
            i, j = T.axis.remap("SS", [i0, j0])
            A[i, j] = T.float32(0)
    for i0, j0, k0 in T.grid(32, 32, 32):
        with T.block():
            i, j, k = T.axis.remap("SSR", [i0, j0, k0])
            B = T.alloc_buffer([128, 128])
            if k == 0:
                for ii, jj in T.grid(4, 4):
                    B[i * 4 + ii, j * 4 + jj] = A[i * 4 + ii, j * 4 + jj]
            for ii, jj in T.grid(4, 4):
                with T.block(""):
                    T.reads([B[((i * 4) + ii), ((j * 4) + jj)]])
                    T.writes([B[((i * 4) + ii), ((j * 4) + jj)]])
                    C = T.alloc_buffer([128, 128])
                    for kk in T.serial(0, 4):
                        B[((i * 4) + ii), ((j * 4) + jj)] = (
                            B[((i * 4) + ii), ((j * 4) + jj)] + C[((i * 4) + ii), ((k * 4) + kk)]
                        )
                    for kk in T.serial(0, 4):
                        with T.block(""):
                            T.reads(
                                [
                                    B[((i * 4) + ii), ((j * 4) + jj)],
                                    C[((i * 4) + ii), ((k * 4) + kk)],
                                ]
                            )
                            T.writes([B[((i * 4) + ii), ((j * 4) + jj)]])
                            D = T.alloc_buffer([128, 128])
                            B[((i * 4) + ii), ((j * 4) + jj)] = B[
                                ((i * 4) + ii), ((j * 4) + jj)
                            ] + (
                                D[((j * 4) + jj), ((k * 4) + kk)]
                                * C[((i * 4) + ii), ((k * 4) + kk)]
                            )


@T.prim_func
def match_buffer_func() -> None:
    C = T.alloc_buffer((128, 128))
    for i in range(128):
        with T.block():
            vi = T.axis.S(128, i)
            C0 = T.match_buffer(C[vi, 0:128], (128))
            for j in range(128):
                with T.block():
                    jj = T.axis.S(128, j)
                    C1 = T.match_buffer(C0[jj], ())
                    C1[()] = 0


@T.prim_func
def transformed_match_buffer_func() -> None:
    for i in range(0, 128):
        with T.block():
            vi = T.axis.S(128, i)
            C = T.alloc_buffer((128, 128))
            C0 = T.match_buffer(C[vi, 0:128], (128))
            for j in range(128):
                with T.block():
                    jj = T.axis.S(128, j)
                    C1 = T.match_buffer(C0[jj], ())
                    C1[()] = 0


@T.prim_func
def opaque_access(a: T.handle, b: T.handle) -> None:
    A = T.match_buffer(a, [1024])
    B = T.match_buffer(b, [1024])
    A_cache = T.alloc_buffer([1024])
    for i in T.serial(0, 8):
        with T.block():
            vi = T.axis.S(8, i)
            with T.block():
                v = T.axis.S(8, vi)
                T.reads([A[(v * 128) : ((v * 128) + 128)]])
                T.writes([A_cache[(v * 128) : ((v * 128) + 128)]])
                T.evaluate(
                    T.call_extern(
                        "test",
                        A_cache.data,
                        (v * 128),
                        128,
                        A.data,
                        (v * 128),
                        128,
                        dtype="float32",
                    )
                )
            for j in T.serial(0, 128):
                with T.block():
                    v = T.axis.S(1024, vi * 128 + j)
                    T.reads([A_cache[v]])
                    T.writes([B[v]])
                    B[v] = A_cache[v]


@T.prim_func
def transformed_opaque_access(a: T.handle, b: T.handle) -> None:
    A = T.match_buffer(a, [1024])
    B = T.match_buffer(b, [1024])
    for i in T.serial(0, 8):
        with T.block():
            vi = T.axis.S(8, i)
            T.reads(A[vi * 128 : vi * 128 + 128])
            T.writes(B[vi * 128 : vi * 128 + 128])
            A_cache = T.alloc_buffer([1024])
            with T.block():
                v = T.axis.S(8, vi)
                T.reads([A[v * 128 : v * 128 + 128]])
                T.writes([A_cache[v * 128 : v * 128 + 128]])
                T.evaluate(
                    T.call_extern(
                        "test", A_cache.data, v * 128, 128, A.data, v * 128, 128, dtype="float32"
                    )
                )
            for j in T.serial(0, 128):
                with T.block():
                    v = T.axis.S(1024, vi * 128 + j)
                    T.reads([A_cache[v]])
                    T.writes([B[v]])
                    B[v] = A_cache[v]


def test_elementwise():
    _check(element_func, transformed_element_func)


def test_locate_buffer_allocation():
    _check(original_func, transformed_func)


def test_match_buffer_allocation():
    _check(match_buffer_func, transformed_match_buffer_func)


def test_opaque_access():
    _check(opaque_access, transformed_opaque_access)


def test_loop_carried_dependency():
    """The buffer allocation should be above opaque iter var's loop scopes
    such that buffer accesses with loop carried dependencies are covered,
    and the allocate buffer should keep the order."""

    @T.prim_func
    def before(A: T.Buffer((8, 8, 8), "int32"), B: T.Buffer((8, 8, 8), "int32")):
        C = T.alloc_buffer([8, 8, 8], dtype="int32")
        D = T.alloc_buffer([8, 8, 8], dtype="int32")
        for i in T.serial(8):
            for j in T.serial(8):
                for k in T.serial(8):
                    with T.block("b0"):
                        vi, vj, vk = T.axis.remap("SSS", [i, j, k])
                        C[vi, vj, vk] = A[vi, vj, vk] + 1
                for k in T.serial(8):
                    with T.block("b1"):
                        vi, vj, vk = T.axis.remap("SSS", [i, j, k])
                        D[vi, vj, vk] = A[vi, vj, vk] + 2
                for k in T.serial(8):
                    with T.block("b2"):
                        vi, vk = T.axis.remap("SS", [i, k])
                        vj = T.axis.opaque(8, j)
                        B[vi, vj, vk] = (
                            C[vi, vj, vk]
                            + T.if_then_else(0 < vj, C[vi, vj - 1, vk], 0, dtype="int32")
                            + D[vi, vj, vk]
                        )

    @T.prim_func
    def after(A: T.Buffer((8, 8, 8), "int32"), B: T.Buffer((8, 8, 8), "int32")) -> None:
        for i in T.serial(8):
            with T.block():
                T.reads(A[i, 0:8, 0:8])
                T.writes(B[i, 0:8, 0:8])
                C = T.alloc_buffer([8, 8, 8], dtype="int32")
                D = T.alloc_buffer([8, 8, 8], dtype="int32")
                for j in T.serial(8):
                    for k in T.serial(8):
                        with T.block("b0"):
                            vi, vj, vk = T.axis.remap("SSS", [i, j, k])
                            C[vi, vj, vk] = A[vi, vj, vk] + 1
                    for k in T.serial(8):
                        with T.block("b1"):
                            vi, vj, vk = T.axis.remap("SSS", [i, j, k])
                            D[vi, vj, vk] = A[vi, vj, vk] + 2
                    for k in T.serial(8):
                        with T.block("b2"):
                            vi, vk = T.axis.remap("SS", [i, k])
                            vj = T.axis.opaque(8, j)
                            B[vi, vj, vk] = (
                                C[vi, vj, vk]
                                + T.if_then_else(0 < vj, C[vi, vj - 1, vk], 0, dtype="int32")
                                + D[vi, vj, vk]
                            )

    _check(before, after)


def test_1D_cascade_op_rolling_buffer():
    """The intermediate buffer must be allocated above rolling buffer's rolling loop,
    which is marked as opaque in consumer block's iter mappings."""

    @T.prim_func
    def before(A: T.Buffer((4, 16), "int32"), C: T.Buffer((4, 8), "int32")):
        B = T.alloc_buffer((4, 6), "int32")
        for c in T.serial(4):
            for i in T.serial(0, 2):
                for j in T.serial(0, 6):
                    for k in T.serial(3):
                        with T.block("P1"):
                            T.where(i < 1 or j >= 2)
                            cc, vi, vj, vk = T.axis.remap("SSSR", [c, i, j, k])
                            if vk == 0:
                                B[cc, T.floormod(vi * 4 + vj, 6)] = 0
                            B[cc, T.floormod(vi * 4 + vj, 6)] = (
                                B[cc, T.floormod(vi * 4 + vj, 6)] + A[cc, vi * 4 + vj + vk]
                            )
                for j in T.serial(0, 4):
                    for k in T.serial(3):
                        with T.block("P2"):
                            vi = T.axis.opaque(2, i)
                            cc, vj, vk = T.axis.remap("SSR", [c, j, k])
                            if vk == 0:
                                C[cc, vi * 4 + vj] = 0
                            C[cc, vi * 4 + vj] = (
                                C[cc, vi * 4 + vj] + B[cc, T.floormod(vi * 4 + vj + vk, 6)]
                            )

    @T.prim_func
    def after(A: T.Buffer((4, 16), "int32"), C: T.Buffer((4, 8), "int32")):
        for c in T.serial(4):
            with T.block():
                T.reads(A[c, 0:12], C[c, 0:8])
                T.writes(C[c, 0:8])
                B = T.alloc_buffer([4, 6], dtype="int32")
                for i in T.serial(2):
                    for j, k in T.grid(6, 3):
                        with T.block("P1"):
                            T.where(i < 1 or j >= 2)
                            cc, vi, vj, vk = T.axis.remap("SSSR", [c, i, j, k])
                            if vk == 0:
                                B[cc, (vi * 4 + vj) % 6] = 0
                            B[cc, (vi * 4 + vj) % 6] = (
                                B[cc, (vi * 4 + vj) % 6] + A[cc, vi * 4 + vj + vk]
                            )
                    for j, k in T.grid(4, 3):
                        with T.block("P2"):
                            vi = T.axis.opaque(2, i)
                            cc, vj, vk = T.axis.remap("SSR", [c, j, k])
                            if vk == 0:
                                C[cc, vi * 4 + vj] = 0
                            C[cc, vi * 4 + vj] = C[cc, vi * 4 + vj] + B[cc, (vi * 4 + vj + vk) % 6]

    _check(before, after)


def test_buffer_conditional_lowering():
    """Buffers passed as pointer arguments are unmodified

    Confirm that the `tir.PlanAndUpdateBufferAllocationLocation` pass
    leaves (Buffer nodes corresponding to pointer-typed PrimFunc arguments)
    unchanged, rather than lowering them to `reads`, `writes`, and `alloc_buffer` nodes.
    """

    @T.prim_func
    def before(A: T.handle("float32")):
        T.func_attr({"global_symbol": "main", "tir.noalias": True})
        for i in range(1):
            A_1 = T.Buffer((1,), data=A)
            A_1[i] = 0

    after = before
    _check(before, after)


def test_dltensor_buffer_is_unlowered():
    """Buffers allocated with a LetStmt are unmodified

    Confirm that the `tir.PlanAndUpdateBufferAllocationLocation` pass
    leaves (Buffer nodes corresponding to PrimFunc DLTensor arguments)
    unchanged, rather than lowering them to `reads`, `writes`, and
    `alloc_buffer` nodes.
    """

    @T.prim_func
    def before(dlpack_handle: T.handle, axis: T.int64) -> T.int64:
        ndim: T.int32 = T.tvm_struct_get(dlpack_handle, 0, 5, "int32")
        stride_ptr: T.handle("int64") = T.tvm_struct_get(dlpack_handle, 0, 4, "handle")
        if T.isnullptr(stride_ptr):
            shape_ptr: T.handle("int64") = T.tvm_struct_get(dlpack_handle, 0, 3, "handle")
            shape = T.decl_buffer(ndim, "int64", data=shape_ptr)
            product = T.decl_buffer([], "int64")
            product[()] = 1
            for dim in range(axis + 1, ndim):
                product[()] = product[()] * shape[dim]
            return product[()]
        else:
            strides = T.decl_buffer(ndim, "int64", data=stride_ptr)
            stride: T.int64 = strides[axis]
            return stride

    after = before
    _check(before, after)


def test_reduce_buffer_dominate_reduce_loops():
    """Reduction write buffer allocation should dominate all reduce loops"""

    @T.prim_func
    def before(x: T.Buffer((256, 256, 256), "float32"), x_red: T.Buffer((256, 256), "float32")):
        x_red_ = T.alloc_buffer((256, 256))
        for ax0_0, k1_0, ax1_0 in T.grid(4, 4, 4):
            for ax0_1, k1_1, ax1_1 in T.grid(64, 64, 64):
                with T.block("x_red"):
                    v_ax0 = T.axis.spatial(256, ax0_0 * 64 + ax0_1)
                    v_ax1 = T.axis.spatial(256, ax1_0 * 64 + ax1_1)
                    v_k1 = T.axis.reduce(256, k1_0 * 64 + k1_1)
                    if v_k1 == 0:
                        x_red_[v_ax0, v_ax1] = T.float32(0.0)
                    x_red_[v_ax0, v_ax1] = x_red_[v_ax0, v_ax1] + x[v_ax0, v_k1, v_ax1]
            for ax0, ax1 in T.grid(64, 64):
                with T.block("x_red_"):
                    v0 = T.axis.spatial(256, ax0_0 * 64 + ax0)
                    v1 = T.axis.spatial(256, ax1_0 * 64 + ax1)
                    x_red[v0, v1] = x_red_[v0, v1]

    @T.prim_func
    def after(x: T.Buffer((256, 256, 256), "float32"), x_red: T.Buffer((256, 256), "float32")):
        for ax0_0 in range(4):
            with T.block(""):
                T.reads(x[ax0_0 * 64 : ax0_0 * 64 + 64, 0:256, 0:256])
                T.writes(x_red[ax0_0 * 64 : ax0_0 * 64 + 64, 0:256])
                x_red_ = T.alloc_buffer((256, 256))
                for k1_0, ax1_0 in T.grid(4, 4):
                    for ax0_1, k1_1, ax1_1 in T.grid(64, 64, 64):
                        with T.block("x_red"):
                            v_ax0 = T.axis.spatial(256, ax0_0 * 64 + ax0_1)
                            v_ax1 = T.axis.spatial(256, ax1_0 * 64 + ax1_1)
                            v_k1 = T.axis.reduce(256, k1_0 * 64 + k1_1)
                            T.reads(x_red_[v_ax0, v_ax1], x[v_ax0, v_k1, v_ax1])
                            T.writes(x_red_[v_ax0, v_ax1])
                            if v_k1 == 0:
                                x_red_[v_ax0, v_ax1] = T.float32(0.0)
                            x_red_[v_ax0, v_ax1] = x_red_[v_ax0, v_ax1] + x[v_ax0, v_k1, v_ax1]
                    for ax0, ax1 in T.grid(64, 64):
                        with T.block("x_red_"):
                            v0 = T.axis.spatial(256, ax0_0 * 64 + ax0)
                            v1 = T.axis.spatial(256, ax1_0 * 64 + ax1)
                            T.reads(x_red_[v0, v1])
                            T.writes(x_red[v0, v1])
                            x_red[v0, v1] = x_red_[v0, v1]

    _check(before, after)


if __name__ == "__main__":
    tvm.testing.main()
