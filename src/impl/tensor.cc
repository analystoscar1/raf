/*!
 * Copyright (c) 2019 by Contributors
 * \file src/impl/tensor.cc
 * \brief MNM Tensor underlying implementation
 */
#include <vector>
#include "mnm/registry.h"
#include "mnm/tensor.h"
#include "../common/shape_utils.h"

namespace mnm {
namespace tensor {

using common::shape_utils::GetShape;
using common::shape_utils::IsCompact;
using common::shape_utils::Shape2Strides;

class Tensor::TensorContainer : public tvm::runtime::NDArray::Container {
 public:
  // DLTensor dl_tensor;
  using Container::dl_tensor;
  // void* manager_ctx = nullptr;
  using Container::manager_ctx;
  // void (*deleter)(Container* self) = nullptr;
  using Container::deleter_;
  // std::atomic<int> ref_counter_ = 0;
  using Container::ref_counter_;
  // std::vector<int64_t> shape_;
  using Container::shape_;
  // An extra field
  std::vector<int64_t> strides_;

  TensorContainer() : tvm::runtime::NDArray::Container() {
    type_index_ = TensorContainer::RuntimeTypeIndex();
  }

  static constexpr const uint32_t _type_index = tvm::TypeIndex::kDynamic;
  static constexpr const char* _type_key = "mnm.tensor.Tensor";
  TVM_DECLARE_FINAL_OBJECT_INFO(TensorContainer, tvm::runtime::NDArray::Container);
};

void* Tensor::get_manager_ctx() const {
  ContainerType * self = static_cast<ContainerType*>(get_mutable());
  CHECK(self != nullptr);
  return self->manager_ctx;
}

class Tensor::Impl {
 public:
  static void DefaultDeleter(ir::Object* super_ptr) {
    TensorContainer* ptr = static_cast<TensorContainer*>(super_ptr);
    if (ptr->manager_ctx != nullptr) {
      // View of other tensors
      static_cast<TSuper::Container*>(ptr->manager_ctx)->DecRef();
    } else {
      // Memory is not owned by MNM tensor, so do nothing
    }
    delete ptr;
  }

  static void NumpyArrayDeleter(ir::Object* super_ptr) {
    static const auto& deleter = registry::GetPackedFunc("mnm._numpy_array_deleter");
    TensorContainer* ptr = static_cast<TensorContainer*>(super_ptr);
    CHECK(ptr->manager_ctx != nullptr);
    deleter(ptr->manager_ctx);
    delete ptr;
  }

  static void ToDLPackDeleter(DLManagedTensor* tensor) {
    static_cast<NDArray::Container*>(tensor->manager_ctx)->DecRef();
    delete tensor;
  }

  static void FromDLPackDeleter(ir::Object* super_ptr) {
    auto* ptr = static_cast<NDArray::Container*>(super_ptr);
    DLManagedTensor* tensor = static_cast<DLManagedTensor*>(ptr->manager_ctx);
    if (tensor->deleter != nullptr) {
      (*tensor->deleter)(tensor);
    }
    delete super_ptr;
  }

  static Tensor Make(const Context& ctx, const DType& dtype, const std::vector<int64_t>& shape,
                     const std::vector<int64_t>& strides, void* data) {
    if (!strides.empty()) {
      CHECK_EQ(shape.size(), strides.size());
    }
    TensorContainer* container = new TensorContainer();
    container->SetDeleter(DefaultDeleter);
    Tensor ret(ir::GetObjectPtr<ir::Object>(container));
    container->shape_ = shape;
    container->strides_ = !strides.empty() ? strides : Shape2Strides<int64_t>(container->shape_);
    container->dl_tensor.data = data;
    container->dl_tensor.ctx = ctx;
    container->dl_tensor.ndim = shape.size();
    container->dl_tensor.dtype = dtype;
    container->dl_tensor.shape = dmlc::BeginPtr(container->shape_);
    container->dl_tensor.strides = dmlc::BeginPtr(container->strides_);
    container->dl_tensor.byte_offset = 0;
    return ret;
  }

  static Tensor FromDLPack(DLManagedTensor* tensor) {
    TensorContainer* container = new TensorContainer();
    container->SetDeleter(FromDLPackDeleter);
    Tensor ret(ir::GetObjectPtr<ir::Object>(container));
    container->manager_ctx = tensor;
    container->dl_tensor = tensor->dl_tensor;
    std::vector<int64_t> shape(tensor->dl_tensor.shape,
                               tensor->dl_tensor.shape + tensor->dl_tensor.ndim);
    container->strides_ = Shape2Strides<int64_t>(shape);
    container->shape_ = std::move(shape);
    container->dl_tensor.shape = dmlc::BeginPtr(container->shape_);
    container->dl_tensor.strides = dmlc::BeginPtr(container->strides_);
    return ret;
  }

  static void MarkNumpy(Tensor tensor, void* manager_ctx) {
    tensor.get_mutable()->manager_ctx = manager_ctx;
    tensor.get_mutable()->SetDeleter(NumpyArrayDeleter);
  }

  static Tensor CreateView(const Tensor& self,
                           const std::vector<int64_t>& shape,    //
                           const std::vector<int64_t>& strides,  //
                           void* data) {
    if (!shape.empty() && !strides.empty()) {
      CHECK_EQ(shape.size(), strides.size());
    }
    TensorContainer* container = new TensorContainer();
    container->SetDeleter(DefaultDeleter);
    Tensor ret(ir::GetObjectPtr<ir::Object>(container));
    container->shape_ = !shape.empty() ? shape : GetShape<int64_t>(*self.operator->());
    container->strides_ = !strides.empty() ? strides : Shape2Strides<int64_t>(container->shape_);
    container->dl_tensor.ctx = self->ctx;
    container->dl_tensor.ndim = container->shape_.size();
    container->dl_tensor.dtype = self->dtype;
    container->dl_tensor.shape = dmlc::BeginPtr(container->shape_);
    container->dl_tensor.strides = dmlc::BeginPtr(container->strides_);
    container->dl_tensor.byte_offset = 0;
    self.get_mutable()->IncRef();
    container->manager_ctx = self.get_mutable()->manager_ctx;
    container->dl_tensor.data = data ? data : self->data;
    return ret;
  }
};

Tensor::Tensor(tvm::runtime::ObjectPtr<tvm::runtime::Object> data) : TSuper(data) {
}

Tensor::Tensor(const tvm::runtime::NDArray& other) : TSuper(other) {
}

Tensor Tensor::CreateView(const std::vector<int64_t>& shape, const std::vector<int64_t>& strides,
                          void* data) const {
  return Tensor::Impl::CreateView(*this, shape, strides, data);
}

Tensor Tensor::make(const Context& ctx, const DType& dtype, const std::vector<int64_t>& shape,
                    const std::vector<int64_t>& strides, void* data) {
  return Tensor::Impl::Make(ctx, dtype, shape, strides, data);
}

Tensor Tensor::FromDLPack(DLManagedTensor* tensor) {
  return Tensor::Impl::FromDLPack(tensor);
}

DLManagedTensor* Tensor::ToDLPack() const {
  DLManagedTensor* ret = new DLManagedTensor();
  ret->deleter = Tensor::Impl::ToDLPackDeleter;
  ret->manager_ctx = get_mutable();
  ret->dl_tensor = get_mutable()->dl_tensor;
  if (IsCompact(get_mutable()->dl_tensor)) {
    ret->dl_tensor.strides = nullptr;
  }
  get_mutable()->IncRef();
  return ret;
}

TVM_REGISTER_OBJECT_TYPE(Tensor::TensorContainer);

MNM_REGISTER_GLOBAL("mnm.tensor.MarkNumpy").set_body_typed(Tensor::Impl::MarkNumpy);

}  // namespace tensor
}  // namespace mnm

namespace mnm {
namespace tensor {

TVM_REGISTER_GLOBAL("mnm.tensor.nd_get_manager_ctx")
.set_body([](tvm::TVMArgs args, tvm::TVMRetValue *rv) {
    mnm::tensor::Tensor a = args[0];
    *rv = a.get_manager_ctx();
  });

}  // namespace tensor
}  // namespace mnm
