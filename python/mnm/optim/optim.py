"""Traced Optimizers"""
from .._core.ndarray import Symbol, get_symbol_handle
from .._core.value import NoGradValue, Value
from .._core.ir_ext import ExtendedVar
from ..model.trace import _get_func_inputs
from ..model import Model, trace
from .._ffi.pass_ import AutoDiff, InlineBackward, Substitute, InferType
from .._ffi.ir.variable import SetMayShare
from .._ffi.ir._make import Module
from .._ffi.binding import BindSymbol
from .._lib import tvm


def calc_dy(dy, record):
    """ relay function returns output + mutation. In backward, mutation needs empty gradient. """
    # pylint: disable=protected-access
    if record.mutations is None:
        return dy
    dout = [get_symbol_handle(dy)]
    for (obj, attr) in record.mutations:
        x = getattr(obj, attr, None)
        assert x is not None
        grad = NoGradValue()
        dout.append(Value.as_const_expr(grad))
    return Symbol.make_tuple(dout) if len(dout) > 1 else dy


def inline(func, inputs):
    """ inline execution func(*inputs) """
    assert len(func.params) == len(inputs)
    vmap = dict(zip(func.params, inputs))
    def evaluate(body):
        if isinstance(body, tvm.relay.Var):
            return vmap[body]
        if isinstance(body, tvm.relay.Let):
            var = BindSymbol(Substitute(body.value, vmap), "", None)
            assert body.var not in vmap
            vmap[body.var] = var
            may_share = ExtendedVar(body.var).may_share
            if may_share is not None:
                assert may_share in vmap
                may_share = vmap[may_share]
                SetMayShare(var, may_share)
            return evaluate(body.body)
        raise NotImplementedError("Not supported type: ", type(body))
    return Symbol.from_expr(evaluate(func.body))


def with_autodiff(model):
    """create a new model by apply autodiff to the input"""
    class AutoDiffWrapper(Model):
        """AutoDiff model

        Parameters
        ----------
        model: the forward model
        """
        def build(self, model):
            # pylint: disable=attribute-defined-outside-init, missing-function-docstring
            self.model = model

        @trace
        def forward(self, dy, *args):
            # pylint: disable=protected-access, missing-function-docstring
            record = self.model._internal(*args)
            dy = calc_dy(dy, record)
            main = tvm.relay.GlobalVar("main")
            mod = Module({main: record.func})
            mod = InferType(mod)
            func = AutoDiff(mod[main], record.requires_grads)
            func = InlineBackward(func)
            inputs = _get_func_inputs(record, args, {})
            inputs = inputs + [get_symbol_handle(dy)]
            out = inline(func, inputs)
            y = out[0]
            dxs = out[1]
            return y, dxs

    return AutoDiffWrapper(model)