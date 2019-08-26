import ctypes

from ..._ffi._tvm import _DLManagedTensor, _register_func
from ..._ffi.tensor import MarkNumpy
from ..._ffi.value import AssembleTensorValue
from ..base import register_mnm_node
from ..context import Context
from .value import Value

_DL_MANAGED_TENSOR_PTR = ctypes.POINTER(_DLManagedTensor)


@_register_func("mnm._numpy_array_deleter")
def numpy_array_deleter(handle):
    handle = ctypes.cast(handle, _DL_MANAGED_TENSOR_PTR)
    void_p = handle.contents.manager_ctx
    pyobj = ctypes.cast(void_p, ctypes.py_object)
    ctypes.pythonapi.Py_DecRef(pyobj)


@register_mnm_node("mnm.value.TensorValue")
class TensorValue(Value):

    @property
    def dltensor_handle(self):
        return self._tensor.handle

    @property
    def data(self):
        handle = self.dltensor_handle
        return handle.contents.data

    @property
    def ctx(self):
        handle = self.dltensor_handle
        ctx = handle.contents.ctx
        return Context.create(ctx.device_type, ctx.device_id)

    @property
    def ndim(self):
        handle = self.dltensor_handle
        return handle.contents.ndim

    @property
    def dtype(self):
        handle = self.dltensor_handle
        return str(handle.contents.dtype)

    @property
    def shape(self):
        handle = self.dltensor_handle
        ndim = self.ndim
        return tuple(handle.contents.shape[i] for i in range(ndim))

    @property
    def strides(self):
        handle = self.dltensor_handle
        ndim = self.ndim
        return tuple(handle.contents.strides[i] for i in range(ndim))

    @property
    def byte_offset(self):
        handle = self.dltensor_handle
        return handle.contents.byte_offset

    @staticmethod
    def assemble(shape, dtype, ctx, strides=None, data=None):
        if isinstance(ctx, str):
            ctx = Context(ctx)
        assert isinstance(ctx, Context), type(ctx)
        return AssembleTensorValue(ctx, dtype, shape, strides, data)

    @staticmethod
    def from_numpy(a):

        def _tensor_value(obj):
            ctx = Context("cpu")
            dtype = str(obj.dtype)
            shape = [x for x in obj.shape]
            strides = [x // obj.itemsize for x in obj.strides]
            data = obj.ctypes.data_as(ctypes.c_void_p)
            assert len(shape) == len(strides)
            return TensorValue.assemble(ctx=ctx, dtype=dtype, shape=shape, strides=strides, data=data)

        def _manager_ctx(obj):
            pyobj = ctypes.py_object(obj)
            void_p = ctypes.c_void_p.from_buffer(pyobj)
            ctypes.pythonapi.Py_IncRef(pyobj)
            return void_p

        result = _tensor_value(a)
        MarkNumpy(result._tensor, _manager_ctx(a))
        return result