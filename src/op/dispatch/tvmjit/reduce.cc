/*!
 * Copyright (c) 2019 by Contributors
 * \file ./src/op/dispatch/tvmjit/binary.cc
 * \brief Binary operators bridged from TVM.
 */
#include <array>
#include <numeric>
#include <mnm/tvmjit/reduce.h>
#include "./tvm_attrs.h"
#include "./tvmjit_utils.h"
#include "../../schema/reduce.h"
#include "../../../common/shape_utils.h"
#include "../../schema/likes.h"

namespace mnm {
namespace op {
namespace tvmjit {

using namespace mnm::ir;
using namespace mnm::op::schema;
using common::shape_utils::GetNumel;

// use tvm::relay::ReduceAttrs here

std::vector<Value> ReduceSchema2Args(const ReduceArgs* args) {
  return {args->x};
}

std::vector<std::string> ReduceSchemaArgNames(const op::CallValues& call) {
  return {"x"};
}

Attrs ReduceSchema2Attrs(const ReduceArgs* args) {
  auto attrs = make_object<tvm_attrs::ReduceAttrs>();
  // expand the empty axis
  DLTensor* x = args->x;
  auto ndim = x->ndim;
  std::vector<int64_t> axis;
  if (args->axis.empty()) {
    axis.resize(ndim);
    std::iota(axis.begin(), axis.end(), 0);
  } else {
    axis = args->axis;
  }
  for (int i = 0, n = axis.size(); i < n; ++i) {
    attrs->axis.push_back(axis[i]);
  }
  attrs->keepdims = args->keepdims;
  attrs->exclude = args->exclude;
  return Attrs(attrs);
}

HashKey ReduceHasher(const std::vector<Type>& param_types, const Type& ret_type,
                     const ReduceArgs* args) {
  HashKey key = GenericHasher<nullptr_t>(param_types, ret_type, nullptr);
  for (int i = 0, n = args->axis.size(); i < n; ++i) {
    key << args->axis[i];
  }
  key << args->keepdims;
  key << args->exclude;
  return key;
}

MNM_TVMJIT(Argmax, "mnm.op.argmax", ReduceArgs, ReduceSchema2Args, ReduceSchemaArgNames,
           ReduceSchema2Attrs, ReduceHasher);
MNM_TVMJIT(Argmin, "mnm.op.argmin", ReduceArgs, ReduceSchema2Args, ReduceSchemaArgNames,
           ReduceSchema2Attrs, ReduceHasher);
MNM_TVMJIT(Max, "mnm.op.max", ReduceArgs, ReduceSchema2Args, ReduceSchemaArgNames,
           ReduceSchema2Attrs, ReduceHasher);
MNM_TVMJIT(Min, "mnm.op.min", ReduceArgs, ReduceSchema2Args, ReduceSchemaArgNames,
           ReduceSchema2Attrs, ReduceHasher);
MNM_TVMJIT(All, "mnm.op.all", ReduceArgs, ReduceSchema2Args, ReduceSchemaArgNames,
           ReduceSchema2Attrs, ReduceHasher);
MNM_TVMJIT(Any, "mnm.op.any", ReduceArgs, ReduceSchema2Args, ReduceSchemaArgNames,
           ReduceSchema2Attrs, ReduceHasher);
MNM_TVMJIT(Mean, "mnm.op.mean", ReduceArgs, ReduceSchema2Args, ReduceSchemaArgNames,
           ReduceSchema2Attrs, ReduceHasher);
MNM_TVMJIT(Prod, "mnm.op.prod", ReduceArgs, ReduceSchema2Args, ReduceSchemaArgNames,
           ReduceSchema2Attrs, ReduceHasher);

std::vector<Value> ReduceDxSchema2Args(const ReduceDxArgs* args) {
  return {args->x, args->y, args->dy};
}

std::vector<std::string> ReduceDxSchemaArgNames(const op::CallValues& call) {
  return {"x", "y", "dy"};
}

Attrs ReduceDxSchema2Attrs(const ReduceDxArgs* args) {
  auto attrs = make_object<tvm_attrs::ReduceAttrs>();
  // expand the empty axis
  DLTensor* x = args->x;
  auto ndim = x->ndim;
  std::vector<int64_t> axis;
  if (args->axis.empty()) {
    axis.resize(ndim);
    std::iota(axis.begin(), axis.end(), 0);
  } else {
    axis = args->axis;
  }
  for (int i = 0, n = axis.size(); i < n; ++i) {
    attrs->axis.push_back(axis[i]);
  }
  attrs->keepdims = args->keepdims;
  attrs->exclude = args->exclude;
  return Attrs(attrs);
}

HashKey ReduceDxHasher(const std::vector<Type>& param_types, const Type& ret_type,
                       const ReduceDxArgs* args) {
  HashKey key = GenericHasher<nullptr_t>(param_types, ret_type, nullptr);
  for (int i = 0, n = args->axis.size(); i < n; ++i) {
    key << args->axis[i];
  }
  key << args->keepdims;
  key << args->exclude;
  return key;
}

TVM_REGISTER_NODE_TYPE(SumAttrs);

std::vector<Value> SumSchema2Args(const SumArgs* args) {
  return {args->x};
}

std::vector<std::string> SumSchemaArgNames(const op::CallValues& call) {
  return {"x"};
}

Attrs SumSchema2Attrs(const SumArgs* args) {
  auto attrs = make_object<SumAttrs>();
  for (int i = 0, n = args->axis.size(); i < n; ++i) {
    attrs->axis.push_back(args->axis[i]);
  }
  for (int i = 0, n = args->keepdims.size(); i < n; ++i) {
    attrs->keepdims.push_back(args->keepdims[i]);
  }
  attrs->exclude = args->exclude;
  return Attrs(attrs);
}

HashKey SumHasher(const std::vector<Type>& param_types, const Type& ret_type, const SumArgs* args) {
  HashKey key = GenericHasher<std::nullptr_t>(param_types, ret_type, nullptr);
  for (int i = 0, n = args->axis.size(); i < n; ++i) {
    key << args->axis[i];
    key << args->keepdims[i];
  }
  key << args->exclude;
  return key;
}

std::vector<Value> SumDxSchema2Args(const SumDxArgs* args) {
  return {args->x, args->y, args->dy};
}

std::vector<std::string> SumDxSchemaArgNames(const op::CallValues& call) {
  return {"x", "y", "dy"};
}

Attrs SumDxSchema2Attrs(const SumDxArgs* args) {
  auto attrs = make_object<SumAttrs>();
  // expand the empty axis
  DLTensor* x = args->x;
  std::vector<int64_t> axis;
  for (int i = 0, n = args->axis.size(); i < n; ++i) {
    attrs->axis.push_back(args->axis[i]);
  }
  for (int i = 0, n = args->keepdims.size(); i < n; ++i) {
    attrs->keepdims.push_back(args->keepdims[i]);
  }
  attrs->exclude = args->exclude;
  return Attrs(attrs);
}

HashKey SumDxHasher(const std::vector<Type>& param_types, const Type& ret_type,
                    const SumDxArgs* args) {
  HashKey key = GenericHasher<nullptr_t>(param_types, ret_type, nullptr);
  for (int i = 0, n = args->axis.size(); i < n; ++i) {
    key << args->axis[i];
    key << args->keepdims[i];
  }
  key << args->exclude;
  return key;
}

MNM_TVMJIT(ProdDx, "mnm.op.prod_dx", ReduceDxArgs, ReduceDxSchema2Args, ReduceDxSchemaArgNames,
           ReduceDxSchema2Attrs, ReduceDxHasher);  // NOLINT

std::vector<Value> MeanDxSchema2Args(const MeanDxArgs* args) {
  return {args->dy};
}

std::vector<std::string> MeanDxSchemaArgNames(const op::CallValues& call) {
  return {"dy"};
}

TVM_REGISTER_NODE_TYPE(MeanDxAttrs);

Attrs MeanDxSchema2Attrs(const MeanDxArgs* args) {
  auto attrs = make_object<MeanDxAttrs>();
  std::vector<int64_t> shape = args->x_shape;
  auto ndim = shape.size();
  for (int64_t s : shape) {
    attrs->shape.push_back(s);
  }
  std::vector<int64_t> axis;
  if (args->axis.empty()) {
    axis.resize(ndim);
    std::iota(axis.begin(), axis.end(), 0);
  } else {
    axis = args->axis;
  }
  for (int i = 0, n = axis.size(); i < n; ++i) {
    attrs->axis.push_back(axis[i]);
  }
  attrs->keepdims = args->keepdims;
  attrs->exclude = args->exclude;
  return Attrs(attrs);
}

HashKey MeanDxHasher(const std::vector<Type>& param_types, const Type& ret_type,
                     const MeanDxArgs* args) {
  HashKey key = GenericHasher<nullptr_t>(param_types, ret_type, nullptr);
  key << args->x_shape;
  key << args->axis;
  key << args->keepdims;
  key << args->exclude;
  return key;
}

MNM_TVMJIT(MeanDx, "mnm.op.mean_dx", MeanDxArgs, MeanDxSchema2Args, MeanDxSchemaArgNames,
           MeanDxSchema2Attrs, MeanDxHasher);  // NOLINT

MNM_TVMJIT(Sum, "mnm.op.sum", SumArgs, SumSchema2Args, SumSchemaArgNames, SumSchema2Attrs,
           SumHasher);

MNM_TVMJIT(SumDx, "mnm.op.sum_dx", SumDxArgs, SumDxSchema2Args, SumDxSchemaArgNames,
           SumDxSchema2Attrs, SumDxHasher);

}  // namespace tvmjit
}  // namespace op
}  // namespace mnm
