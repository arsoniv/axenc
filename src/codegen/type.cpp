#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Type.h>

#include "nodes/context.hpp"
#include "nodes/type.hpp"

namespace axen::ast {

llvm::Type *FunctionTypeNode::codeGen(CodegenContext &ctx) {
  llvm::Type *returnType = returnType_->codeGen(ctx);

  auto parameterTypes = std::vector<llvm::Type *>();

  for (auto &param : parameters_) {
    parameterTypes.emplace_back(param->codeGen(ctx));
  }

  return llvm::FunctionType::get(returnType, parameterTypes, false);
}

llvm::Type *PrimitiveTypeNode::codeGen(CodegenContext &ctx) {
  switch (type_) {

  case PrimitiveType::Void:
    return llvm::Type::getVoidTy(ctx.llvmContext);
  case PrimitiveType::Bool:
    return llvm::Type::getInt1Ty(ctx.llvmContext);
  case PrimitiveType::Char:
    return llvm::Type::getInt8Ty(ctx.llvmContext);
  case PrimitiveType::Short:
    return llvm::Type::getInt16Ty(ctx.llvmContext);
  case PrimitiveType::Int:
    return llvm::Type::getInt32Ty(ctx.llvmContext);
  case PrimitiveType::Long:
    return llvm::Type::getInt64Ty(ctx.llvmContext);
  case PrimitiveType::Half:
    return llvm::Type::getHalfTy(ctx.llvmContext);
  case PrimitiveType::Float:
    return llvm::Type::getFloatTy(ctx.llvmContext);
  case PrimitiveType::Double:
    return llvm::Type::getDoubleTy(ctx.llvmContext);
  case PrimitiveType::Quad:
    return llvm::Type::getFP128Ty(ctx.llvmContext);
  default:
    ctx.emitInternalError("Could not find type, how did we get here?");
  }
}

llvm::Type *ClassReferenceNode::codeGen(CodegenContext &ctx) {
  auto decl = decl_.lock();
  if (!decl) {
    ctx.emitInternalError("ClassNode has been destroyed during codegen");
  }
  return decl->codeGen(ctx);
}

llvm::Type *PointerTypeNode::codeGen(CodegenContext &ctx) {
  llvm::Type *targetType = target_->codeGen(ctx);
  return llvm::PointerType::getUnqual(ctx.llvmContext);
}

llvm::Type *ArrayTypeNode::codeGen(CodegenContext &ctx) {
  llvm::Type *targetType = target_->codeGen(ctx);
  return llvm::ArrayType::get(targetType, length_);
}

} // namespace axen::ast
