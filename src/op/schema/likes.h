/*!
 * Copyright (c) 2020 by Contributors
 * Auto generated. Do not touch.
 * \file src/op/schema/likes.h
 * \brief Operator schema.
 */
#pragma once
#include <vector>
#include <string>
#include "mnm/op.h"
#include "mnm/value.h"
namespace mnm {
namespace op {
namespace schema {
class CollapseLikeArgs : public ir::AttrsNode<CollapseLikeArgs> {
 public:
  value::BaseTensorValue x;
  std::vector<int64_t> shape;
  MNM_OP_SCHEMA(CollapseLikeArgs, "mnm.args.collapse_like");
};

class ReshapeArgs : public ir::AttrsNode<ReshapeArgs> {
 public:
  value::BaseTensorValue x;
  std::vector<int64_t> shape;
  bool reverse{false};
  MNM_OP_SCHEMA(ReshapeArgs, "mnm.args.reshape");
};

class SumArgs : public ir::AttrsNode<SumArgs> {
 public:
  value::BaseTensorValue x;
  std::vector<int64_t> axis{};
  std::vector<int64_t> keepdims{0};
  MNM_OP_SCHEMA(SumArgs, "mnm.args.sum");
};
}  // namespace schema
}  // namespace op
}  // namespace mnm
