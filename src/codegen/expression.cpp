#include <cstdio>
#include <cstdlib>
#include <llvm/ADT/StringRef.h>
#include <llvm/IR/Constants.h>
#include <string>
#include <vector>

#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Value.h>
#include <llvm/Support/Casting.h>

#include "nodes/context.hpp"
#include "nodes/expression.hpp"

namespace axen::ast {

llvm::Value *VariableReference::codeGen(CodegenContext &ctx) {

  llvm::AllocaInst *alloca = ctx.lookupVariable(name_);

  if (!alloca) {
    error::reportError(error::ErrorType::Codegen, "Undefined variable '" + name_ + "'");
  }

  return ctx.builder.CreateLoad(alloca->getAllocatedType(), alloca, "varValRef");
}

llvm::Value *VariableReference::codeGenLValue(CodegenContext &ctx) {

  llvm::AllocaInst *alloca = ctx.lookupVariable(name_);

  if (!alloca) {
    error::reportError(error::ErrorType::Codegen, "Undefined variable '" + name_ + "'");
  }

  return alloca;
}

llvm::Value *Dref::codeGen(CodegenContext &ctx) {

  llvm::Value *ptr = target_->codeGen(ctx);

  if (!ptr) {
    error::reportError(error::ErrorType::Codegen, "Failed to generate target expression for dereference");
  }

  if (!ptr->getType()->isPointerTy()) {
    error::reportError(error::ErrorType::Codegen, "Cannot dereference non-pointer type");
  }

  return ctx.builder.CreateLoad(derivedType_->codeGen(ctx), ptr, "varValRef");
}

llvm::Value *Dref::codeGenLValue(CodegenContext &ctx) {

  llvm::Value *ptr = target_->codeGenLValue(ctx);

  if (!ptr) {
    error::reportError(error::ErrorType::Codegen, "Failed to generate lvalue for dereference target");
  }

  if (!ptr->getType()->isPointerTy()) {
    error::reportError(error::ErrorType::Codegen, "Cannot dereference non-pointer type");
  }

  return ctx.builder.CreateLoad(ptr->getType(), ptr, "varValRef");
}

llvm::Value *AddressOf::codeGen(CodegenContext &ctx) {
  llvm::Value *lvalue = target_->codeGenLValue(ctx);

  if (!lvalue) {
    error::reportError(error::ErrorType::Codegen, "Failed to generate lvalue for address-of operator");
  }

  return lvalue;
}

llvm::Value *StructAccess::codeGen(CodegenContext &ctx) {
  llvm::Value *fieldPtr = codeGenLValue(ctx);
  if (!fieldPtr) {
    error::reportError(error::ErrorType::Codegen, "Failed to generate lvalue for struct member '" + memberName_ + "'");
  }

  std::shared_ptr<ast::TypeNode> memberType = type_->getDecl()->lookupMemberType(memberName_);
  if (!memberType) {
    error::reportError(error::ErrorType::Codegen,
                       "Struct '" + structName_ + "' has no member named '" + memberName_ + "'");
  }

  llvm::Type *fieldType = memberType->codeGen(ctx);

  return ctx.builder.CreateLoad(fieldType, fieldPtr, structName_ + "_member");
}

llvm::Value *StructAccess::codeGenLValue(CodegenContext &ctx) {
  llvm::Value *structPtr = structExpr_->codeGenLValue(ctx);

  if (!structPtr) {
    error::reportError(error::ErrorType::Codegen, "Failed to generate lvalue for struct expression");
  }

  llvm::Type *ty = type_->codeGen(ctx);
  llvm::StructType *llvmStructType = llvm::dyn_cast<llvm::StructType>(ty);

  if (!llvmStructType) {
    error::reportError(error::ErrorType::Codegen, "Expected struct type but got different type");
  }

  int memberIndex = type_->getDecl()->lookupMemberIndex(memberName_);

  return ctx.builder.CreateStructGEP(llvmStructType, structPtr, memberIndex);
}

llvm::Value *ArrayAccess::codeGen(CodegenContext &ctx) {
  llvm::Value *elemPtr = codeGenLValue(ctx);

  if (!elemPtr) {
    error::reportError(error::ErrorType::Codegen, "Failed to generate lvalue for array access");
  }

  llvm::Type *ty = type_->codeGen(ctx);
  llvm::ArrayType *arrayType = llvm::dyn_cast<llvm::ArrayType>(ty);

  if (!arrayType) {
    error::reportError(error::ErrorType::Codegen, "Expected array type but got different type");
  }

  llvm::Type *elemType = arrayType->getElementType();

  return ctx.builder.CreateLoad(elemType, elemPtr, "arrayval");
}

llvm::Value *ArrayAccess::codeGenLValue(CodegenContext &ctx) {
  llvm::Value *arrayPtr = arrayExpr_->codeGenLValue(ctx);

  if (!arrayPtr) {
    error::reportError(error::ErrorType::Codegen, "Failed to generate lvalue for array expression");
  }

  llvm::Value *indexVal = indexExpr_->codeGen(ctx);

  if (!indexVal) {
    error::reportError(error::ErrorType::Codegen, "Failed to generate index expression for array access");
  }

  if (!indexVal->getType()->isIntegerTy()) {
    error::reportError(error::ErrorType::Codegen, "Array index must be an integer type");
  }

  if (!type_) {
    error::reportError(error::ErrorType::Codegen, "Array access has null type");
  }

  llvm::Type *ty = type_->codeGen(ctx);
  llvm::ArrayType *arrayType = llvm::dyn_cast<llvm::ArrayType>(ty);

  if (!arrayType) {
    error::reportError(error::ErrorType::Codegen, "Expected array type but got different type");
  }

  llvm::Value *zero = llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx.llvmContext), 0);
  std::vector<llvm::Value *> indices = {zero, indexVal};

  return ctx.builder.CreateGEP(arrayType, arrayPtr, indices, "arrayidx");
}

llvm::Value *PtrIndexAccess::codeGen(CodegenContext &ctx) {
  llvm::Value *elemPtr = codeGenLValue(ctx);

  if (!elemPtr) {
    error::reportError(error::ErrorType::Codegen, "Failed to generate lvalue for pointer index access");
  }

  llvm::Type *ty = type_->codeGen(ctx);
  llvm::Type *dirType = type_->target()->codeGen(ctx);

  return ctx.builder.CreateLoad(dirType, elemPtr, "ptrval");
}

llvm::Value *PtrIndexAccess::codeGenLValue(CodegenContext &ctx) {
  llvm::Value *ptrVal = ptrExpr_->codeGen(ctx);

  if (!ptrVal) {
    error::reportError(error::ErrorType::Codegen, "Failed to generate pointer expression for indexing");
  }

  if (!ptrVal->getType()->isPointerTy()) {
    error::reportError(error::ErrorType::Codegen, "Cannot index into non-pointer type");
  }

  llvm::Value *indexVal = indexExpr_->codeGen(ctx);

  if (!indexVal) {
    error::reportError(error::ErrorType::Codegen, "Failed to generate index expression for pointer access");
  }

  if (!indexVal->getType()->isIntegerTy()) {
    error::reportError(error::ErrorType::Codegen, "Pointer index must be an integer type");
  }

  if (!type_) {
    error::reportError(error::ErrorType::Codegen, "Pointer access has null type");
  }

  llvm::Type *ty = type_->target()->codeGen(ctx);

  std::vector<llvm::Value *> indices = {indexVal};

  return ctx.builder.CreateGEP(ty, ptrVal, indices, "ptridx");
}

llvm::Value *IntLiteral::codeGen(CodegenContext &ctx) {
  return llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx.llvmContext), value_, true);
}

llvm::Value *FloatLiteral::codeGen(CodegenContext &ctx) {
  return llvm::ConstantFP::get(llvm::Type::getFloatTy(ctx.llvmContext), value_);
}

llvm::Value *StringLiteral::codeGen(CodegenContext &ctx) {

  llvm::GlobalVariable *gvar = ctx.builder.CreateGlobalString(value_);

  llvm::Value *zero = ctx.builder.getInt32(0);
  return ctx.builder.CreateInBoundsGEP(gvar->getValueType(), gvar, {zero, zero});
}

llvm::Value *FunctionCall::codeGen(CodegenContext &ctx) {
  llvm::Function *calleeFunc = ctx.module->getFunction(name_);

  if (!calleeFunc) {
    error::reportError(error::ErrorType::Codegen, "Unknown function '" + name_ + "'");
  }

  if (calleeFunc->arg_size() != args_.size()) {
    error::reportError(error::ErrorType::Codegen, "Function '" + name_ + "' expects " +
                                                      std::to_string(calleeFunc->arg_size()) + " arguments, got " +
                                                      std::to_string(args_.size()));
  }

  std::vector<llvm::Value *> args;
  args.reserve(args_.size());

  for (size_t i = 0; i < args_.size(); ++i) {
    llvm::Value *argValue = args_.at(i)->codeGen(ctx);

    if (!argValue) {
      error::reportError(error::ErrorType::Codegen,
                         "Failed to generate argument " + std::to_string(i) + " for function '" + name_ + "'");
      return nullptr;
    }

    args.push_back(argValue);
  }

  llvm::Value *result =
      ctx.builder.CreateCall(calleeFunc, args, calleeFunc->getReturnType()->isVoidTy() ? "" : "calltmp");

  if (!result) {
    error::reportError(error::ErrorType::Codegen, "Failed to create call to function '" + name_ + "'");
    return nullptr;
  }

  return result;
}

llvm::Value *BinaryOperation::codeGen(CodegenContext &ctx) {

  llvm::Value *L = L_->codeGen(ctx);
  llvm::Value *R = R_->codeGen(ctx);

  R = ctx.convertIfNeeded(R, L->getType(), isSigned_);

  if (!L || !R) {
    error::reportError(error::ErrorType::Codegen, "Failed to generate operands for binary operation");
  }

  switch (type_) {
  case BinaryOperationType::Add:

    if (L->getType()->isPointerTy()) {
      if (!R->getType()->isIntegerTy()) {
        error::reportError(error::ErrorType::Codegen, "Cannot add non-integer to pointer");
      }
      return ctx.builder.CreateGEP(L->getType(), L, R);
    } else if (R->getType()->isPointerTy()) {
      if (!L->getType()->isIntegerTy()) {
        error::reportError(error::ErrorType::Codegen, "Cannot add non-integer to pointer");
      }
      return ctx.builder.CreateGEP(R->getType(), R, L);
    } else {
      if (!L->getType()->isIntegerTy() || !R->getType()->isIntegerTy()) {
        error::reportError(error::ErrorType::Codegen, "Addition requires integer operands");
      }
      return ctx.builder.CreateAdd(L, R, "addtmp");
    }

  case BinaryOperationType::Subtract:

    if (L->getType()->isPointerTy()) {
      if (!R->getType()->isIntegerTy()) {
        error::reportError(error::ErrorType::Codegen, "Cannot subtract non-integer from pointer");
      }
      return ctx.builder.CreateGEP(L->getType(), L, ctx.builder.CreateNeg(R));
    } else {
      if (!L->getType()->isIntegerTy() || !R->getType()->isIntegerTy()) {
        error::reportError(error::ErrorType::Codegen, "Subtraction requires integer operands");
      }
      return ctx.builder.CreateSub(L, R, "subtmp");
    }

  case BinaryOperationType::Multiply:
    if (!L->getType()->isIntegerTy() || !R->getType()->isIntegerTy()) {
      error::reportError(error::ErrorType::Codegen, "Multiplication requires integer operands");
    }
    return ctx.builder.CreateMul(L, R, "multmp");

  case BinaryOperationType::Divide:
    if (!L->getType()->isIntegerTy() || !R->getType()->isIntegerTy()) {
      error::reportError(error::ErrorType::Codegen, "Division requires integer operands");
    }
    return ctx.builder.CreateUDiv(L, R, "udivtmp");

  case BinaryOperationType::Less:
    if (!L->getType()->isIntegerTy() || !R->getType()->isIntegerTy()) {
      error::reportError(error::ErrorType::Codegen, "Comparison requires integer operands");
    }
    return ctx.builder.CreateICmpULT(L, R);

  case BinaryOperationType::More:
    if (!L->getType()->isIntegerTy() || !R->getType()->isIntegerTy()) {
      error::reportError(error::ErrorType::Codegen, "Comparison requires integer operands");
    }
    return ctx.builder.CreateICmpUGT(L, R);

  case BinaryOperationType::Equal:
    if (!L->getType()->isIntegerTy() || !R->getType()->isIntegerTy()) {
      error::reportError(error::ErrorType::Codegen, "Equality comparison requires integer operands");
    }
    return ctx.builder.CreateICmpEQ(L, R);

  default:
    error::reportError(error::ErrorType::Codegen, "Unexpected binary operation type");
    return nullptr; // unreachable
  }
}
} // namespace axen::ast
