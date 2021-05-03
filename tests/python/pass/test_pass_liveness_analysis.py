# pylint: disable=invalid-name, no-self-use, too-many-locals, unused-variable, protected-access
# pylint: disable=too-many-arguments
import pytest
import mnm
from mnm._lib import tvm, relay
from mnm._ffi.pass_ import InferType, LivenessAnalysis, ManifestAlloc
from mnm.testing import randn

def verify_live_in_set(mod, expected):
    mod = InferType()(mod)
    ret = LivenessAnalysis(mod)
    ret = {key.name_hint: {v.name_hint for v in var_list} for key, var_list in ret.items()}

    missed = {}
    for key, vset in expected.items():
        if key not in ret:
            missed[key] = []
        else:
            for var in ret[key]:
                if var not in vset:
                    if key not in missed:
                        missed[key] = []
                    missed[key].append(var)

    if missed or not expected:
        print("Live in sets:")
        for key, var_list in ret.items():
            print("%s: %s" % (key, ",".join(var_list)))

        print("Missed items")
        for key, var_list in missed.items():
            if not var_list:
                print("Missed key %s" % key)
            else:
                print("Missed live in of %s: %s" % (key, ",".join(var_list)))
        assert False, "Expected key or live in vars are missing"

def test_basic():
    class Model(mnm.Model):
        def build(self):
            pass

        @mnm.model.trace
        def forward(self, param_0, param_1, param_2, param_3):
            t_0 = mnm.add(param_0, param_0) # a1
            t_1 = mnm.add(t_0, param_1) # a2
            t_2 = mnm.add(t_1, param_2) # a3
            t_3 = mnm.add(t_2, t_0) # a4
            t_4 = mnm.add(t_3, param_3) # a5
            return t_4 # n_1

    device = "cpu"
    shape = (5, 5)
    model = Model()
    model.infer_mode()
    m_a, _ = randn(shape, device=device)
    m_b, _ = randn(shape, device=device)
    m_c, _ = randn(shape, device=device)
    m_d, _ = randn(shape, device=device)
    args = [m_a, m_b, m_c, m_d]

    expected = {
        "n_0": {},
        "a1": {"param_0", "param_1", "param_2", "param_3"},
        "a2": {"t_0", "param_1", "param_2", "param_3"},
        "a3": {"t_0", "t_1", "param_2", "param_3"},
        "a4": {"t_0", "t_2", "param_3"},
        "a5": {"t_3", "param_3"},
        "n_1": {"t_4"}
    }

    mod = model._internal(*args).mod
    verify_live_in_set(mod, expected)


def test_multi_outs():
    class Model(mnm.Model):
        def build(self):
            pass

        @mnm.model.trace
        def forward(self, param_0, param_1, param_2, param_3, param_4):
            t_0 = mnm.relu(param_0) # a1
            res = mnm.batch_norm_train(t_0, param_3, param_4, param_1, param_2, 0.1, 1e-5) # a2
            t_1 = res[0] # a3
            t_2 = res[1]
            t_3 = res[2]
            t_4 = mnm.relu(t_1) # a4
            t_5 = mnm.relu(t_4) # a5
            return t_5 # n_1

    model = Model()
    model.infer_mode()

    device = "cpu"
    shape = (16, 3, 224, 224)
    stats_shape = [shape[1]]
    m_x, _ = randn(shape, device=device)
    m_m, _ = randn(stats_shape, device=device)
    m_v, _ = randn(stats_shape, positive=True, device=device)
    m_w, _ = randn(stats_shape, device=device)
    m_b, _ = randn(stats_shape, device=device)
    args = [m_x, m_m, m_v, m_w, m_b]

    expected = {
        "n_0": {},
        "a1": {"param_0", "param_1", "param_2", "param_3", "param_4"},
        "a2": {"t_0", "param_1", "param_2", "param_3", "param_4"},
        "a3": {"t_1"},
        "a4": {"t_1"},
        "a5": {"t_4"},
        "n_1": {"t_5"}
    }

    mod = model._internal(*args).mod
    verify_live_in_set(mod, expected)


def test_tuple_input():
    class Model(mnm.Model):
        def build(self):
            pass

        @mnm.model.trace
        def forward(self, tup):
            x = tup[0] # a1
            y = tup[1] # a2
            t_0 = mnm.add(x, y) # a3
            return t_0 # n_1

    model = Model()
    model.infer_mode()

    device = "cpu"
    shape = (5, 5)
    m_a, _ = randn(shape, device=device)
    m_b, _ = randn(shape, device=device)
    args = [(m_a, m_b)]

    expected = {
        "n_0": {},
        "a1": {"param_0", "param_1"},
        "a2": {"param_0", "param_1"},
        "a3": {"param_0", "param_1"},
        "n_1": {"t_0"},
    }

    mod = model._internal(*args).mod
    verify_live_in_set(mod, expected)


def test_manifest_alloc_compatible():
    def test_func():
        storage_op = mnm._ffi.op.GetOp("mnm.op.vm.alloc_storage")
        tensor_op = mnm._ffi.op.GetOp("mnm.op.vm.alloc_tensor")
        invoke_op = mnm._ffi.op.GetOp("mnm.op.vm.invoke_op")
        add_op = mnm._ffi.op.GetOp("mnm.op.add")
        null = mnm.ir.const(None)

        x = relay.var("x", shape=(5, 5))
        y = relay.var("y", shape=(5, 5))
        a0 = relay.var("a0")
        a1 = relay.var("a1")
        a2 = relay.var("a2")
        a3 = relay.var("a3")
        a4 = relay.var("a4")
        a5 = relay.var("a5")
        a6 = relay.var("a6")
        a7 = relay.var("a7")

        let7 = relay.Let(a7, a1, a7)
        let6 = relay.Let(a6, relay.Call(invoke_op, [a2, a4, a5]), let7)
        let5 = relay.Let(a5, relay.Tuple((a1,)), let6)
        # Test both binded and non-binded constants
        let4 = relay.Let(a4, relay.Tuple((x, y, a3, null)), let5)
        let3 = relay.Let(a3, null, let4)
        let2 = relay.Let(a2, add_op, let3)
        let1 = relay.Let(a1, relay.Call(tensor_op,
                                        [a0, mnm.ir.const([5, 5]), mnm.ir.const("float32"),
                                         mnm.ir.const([5, 5])]), let2)
        let0 = relay.Let(a0, relay.Call(storage_op,
                                        [mnm.ir.const(100), mnm.ir.const(64), mnm.ir.const(1),
                                         mnm.ir.const(0), mnm.ir.const("float32")]), let1)
        # pylint: disable=line-too-long
        # fn (%x: Tensor[(5, 5), float32], %y: Tensor[(5, 5), float32]) {
        #   let %a0 = mnm.op.vm.alloc_storage(int64(100), int64(64), int64(1), int64(0), str"float32");
        #   let %a1 = mnm.op.vm.alloc_tensor(%a0, TupleValue([int64(5), int64(5)]), str"float32", TupleValue([int64(5), int64(5)]));
        #   let %a2 = mnm.op.add;
        #   let %a3 = nullptr;
        #   let %a4 = (%x, %y, %a3, nullptr);
        #   let %a5 = (%a1,);
        #   let %a6 = mnm.op.vm.invoke_op(%a2, %a4, %a5);
        #   let %a7 = %a1;
        #   %a7
        # }
        # pylint: enable=line-too-long

        return relay.Function([x, y], let0)

    # Note that a3 will not be visited.
    expected = {
        "n_1": {},
        "a0": {"param_0", "param_1"},
        "a1": {"param_0", "param_1", "t_0"},
        "a2": {"param_0", "param_1", "t_1"},
        "a4": {"param_0", "param_1", "t_1"},
        "a5": {"param_0", "param_1", "t_1"},
        "a6": {"param_0", "param_1", "t_1"},
        "a7": {"t_1"},
        "n_2": {"t_1"},
    }

    func = test_func()
    mod = tvm.IRModule.from_expr(func)
    verify_live_in_set(mod, expected)


def test_after_manifest_alloc():
    class Model(mnm.Model):
        def build(self):
            pass

        @mnm.model.trace
        def forward(self, param_0, param_1, param_2):
            t_0 = mnm.add(param_0, param_0) # a1
            t_1 = mnm.add(t_0, param_1) # a2
            t_2 = mnm.add(t_1, param_2) # a3
            return t_2 # n_1

    device = "cpu"
    shape = (5, 5)
    model = Model()
    model.infer_mode()
    m_a, _ = randn(shape, device=device)
    m_b, _ = randn(shape, device=device)
    m_c, _ = randn(shape, device=device)
    args = [m_a, m_b, m_c]

    mod = model._internal(*args).mod
    mod = InferType()(mod)
    mod = ManifestAlloc()(mod)
    # pylint: disable=line-too-long
    # def @main(%param_0: Tensor[(5, 5), float32],
    #           %param_1: Tensor[(5, 5), float32],
    #           %param_2: Tensor[(5, 5), float32]) -> Tensor[(5, 5), float32] {
    #   let %x_0 = mnm.op.vm.alloc_storage(int64(100), int64(64), int32(1), int32(0), str"float32");
    #   let %x_1 = mnm.op.vm.alloc_tensor(%x_0, [5, 5], str"float32", [5, 5]);
    #   let %x_2 = mnm.op.add;
    #   let %x_3 = (%param_0, %param_0, nullptr /* ty=() */, nullptr /* ty=() */);
    #   let %x_4 = (%x_1,);
    #   let %x_5 = mnm.op.vm.invoke_op(%x_2, %x_3, %x_4);
    #   let %a1 = %x_1;
    #   let %x_6 = mnm.op.vm.alloc_storage(int64(100), int64(64), int32(1), int32(0), str"float32");
    #   let %x_7 = mnm.op.vm.alloc_tensor(%x_6, [5, 5], str"float32", [5, 5]);
    #   let %x_8 = mnm.op.add;
    #   let %x_9 = (%a1, %param_1, nullptr /* ty=() */, nullptr /* ty=() */);
    #   let %x_10 = (%x_7,);
    #   let %x_11 = mnm.op.vm.invoke_op(%x_8, %x_9, %x_10);
    #   let %a2 = %x_7;
    #   let %x_12 = mnm.op.vm.alloc_storage(int64(100), int64(64), int32(1), int32(0), str"float32");
    #   let %x_13 = mnm.op.vm.alloc_tensor(%x_12, [5, 5], str"float32", [5, 5]);
    #   let %x_14 = mnm.op.add;
    #   let %x_15 = (%a2, %param_2, nullptr /* ty=() */, nullptr /* ty=() */);
    #   let %x_16 = (%x_13,);
    #   let %x_17 = mnm.op.vm.invoke_op(%x_14, %x_15, %x_16);
    #   let %a3 = %x_13;
    #   %a3
    # }
    # pylint: enable=line-too-long

    expected = {
        "x_0": {"param_0", "param_1", "param_2"},
        "x_1": {"param_0", "param_1", "t_0", "param_2"},
        "x_2": {"param_0", "param_1", "t_1", "param_2"},
        "x_3": {"param_0", "param_1", "t_1", "param_2"},
        "x_4": {"param_0", "param_1", "t_1", "param_2"},
        "x_5": {"param_0", "param_1", "t_1", "param_2"},
        "a1": {"param_1", "t_1", "param_2"},
        "x_6": {"param_1", "t_1", "param_2"},
        "x_7": {"t_2", "param_1", "t_1", "param_2"},
        "x_8": {"t_3", "param_1", "t_1", "param_2"},
        "x_9": {"t_3", "param_1", "t_1", "param_2"},
        "x_10": {"t_3", "param_1", "t_1", "param_2"},
        "x_11": {"t_3", "param_1", "t_1", "param_2"},
        "a2": {"t_3", "param_2"},
        "x_12": {"t_3", "param_2"},
        "x_13": {"t_3", "t_4", "param_2"},
        "x_14": {"t_3", "t_5", "param_2"},
        "x_15": {"t_3", "t_5", "param_2"},
        "x_16": {"t_3", "t_5", "param_2"},
        "a3": {"t_5"},
        "n_4": {"t_5"},
    }

    verify_live_in_set(mod, expected)


if __name__ == "__main__":
    pytest.main([__file__])