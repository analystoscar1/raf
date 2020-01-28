/*!
 * Copyright (c) 2019 by Contributors
 * \file src/op/declare/gemm.cc
 * \brief Declaration of genmm-related operators
 */
#include "mnm/op.h"
#include "mnm/tensor.h"
#include "../schema/ufunc.h"

namespace mnm {
namespace op {
namespace declare {

using namespace mnm::op::schema;
using namespace mnm::value;

template<bool transpose_a, bool transpose_b>
void MatmulDecl(const CallValues& call) {
  // TODO(@junrushao1994): sanity check
  const auto* args = call->args.as<schema::BinaryArgs>();
  CHECK(args != nullptr);
  const DLTensor* a = args->x1;
  const DLTensor* b = args->x2;
  // a is of shape [n1, m1]
  // b is of shape [n2, m2]
  CHECK_EQ(a->ndim, 2);
  CHECK_EQ(b->ndim, 2);
  int64_t n1 = a->shape[0];
  int64_t m1 = a->shape[1];
  int64_t n2 = b->shape[0];
  int64_t m2 = b->shape[1];
  if (transpose_a) {
    std::swap(n1, m1);
  }
  if (transpose_b) {
    std::swap(n2, m2);
  }
  CHECK_EQ(m1, n2);
  CHECK(a->dtype.code == kDLFloat && (a->dtype.bits == 32 || a->dtype.bits == 64))
      << "Only float and double are supported!";
  call->out = TensorValue::Assemble(/*ctx=*/a->ctx, /*dtype=*/a->dtype, /*shape=*/{n1, m2});
  call->ctx = a->ctx;
  if (!n1 || !n2 || !m1 || !m2) {
    call->callee = ir::NullValue<OpValue>();
  }
}

auto MatmulNN = MatmulDecl<false, false>;
auto MatmulNT = MatmulDecl<false, true>;
auto MatmulTN = MatmulDecl<true, false>;
auto MatmulTT = MatmulDecl<true, true>;

MNM_OP_DECLARE("mnm.op.matmul", MatmulNN);
MNM_OP_DECLARE("mnm.op.matmul_nt", MatmulNT);
MNM_OP_DECLARE("mnm.op.matmul_tn", MatmulTN);
MNM_OP_DECLARE("mnm.op.matmul_tt", MatmulTT);

}  // namespace declare
}  // namespace op
}  // namespace mnm