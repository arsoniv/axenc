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
    ctx.emitCodegenError("Undefined variable '" + name_ + "'");
  }

  return ctx.builder.CreateLoad(alloca->getAllocatedType(), alloca, "varValRef");
}

llvm::Value *VariableReference::codeGenLValue(CodegenContext &ctx) {
  llvm::AllocaInst *alloca = ctx.lookupVariable(name_);

  if (!alloca) {
    ctx.emitCodegenError("Undefined variable '" + name_ + "'");
  }

  return alloca;
}

llvm::Value *Dref::codeGen(CodegenContext &ctx) {
  llvm::Value *ptr = target_->codeGen(ctx);

  if (!ptr) {
    ctx.emitInternalError("Failed to generate target expression for dereference");
  }

  if (!ptr->getType()->isPointerTy()) {
    ctx.emitInternalError("Cannot dereference non-pointer type (should have been caught in analysis)");
  }

  return ctx.builder.CreateLoad(derivedType_->codeGen(ctx), ptr, "varValRef");
}

llvm::Value *Dref::codeGenLValue(CodegenContext &ctx) {
  llvm::Value *ptr = target_->codeGenLValue(ctx);

  if (!ptr) {
    ctx.emitCodegenError("Failed to generate lvalue for dereference target");
  }

  if (!ptr->getType()->isPointerTy()) {
    ctx.emitCodegenError("Cannot dereference non-pointer type");
  }

  return ctx.builder.CreateLoad(ptr->getType(), ptr, "varValRef");
}

llvm::Value *AddressOf::codeGen(CodegenContext &ctx) {
  llvm::Value *lvalue = target_->codeGenLValue(ctx);

  if (!lvalue) {
    ctx.emitCodegenError("Failed to generate lvalue for address-of operator");
  }

  return lvalue;
}

llvm::Value *StructAccess::codeGen(CodegenContext &ctx) {
  llvm::Value *fieldPtr = codeGenLValue(ctx);
  if (!fieldPtr) {
    ctx.emitCodegenError("Failed to generate lvalue for struct member '" + memberName_ + "'");
  }

  std::shared_ptr<ast::TypeNode> memberType = structType_->getDecl()->lookupMemberType(memberName_);
  if (!memberType) {
    ctx.emitCodegenError("Struct '" + structName_ + "' has no member named '" + memberName_ + "'");
  }

  llvm::Type *fieldType = memberType->codeGen(ctx);

  return ctx.builder.CreateLoad(fieldType, fieldPtr, structName_ + "_member");
}

llvm::Value *StructAccess::codeGenLValue(CodegenContext &ctx) {
  llvm::Value *structPtr = structExpr_->codeGenLValue(ctx);

  if (!structPtr) {
    ctx.emitCodegenError("Failed to generate lvalue for struct expression");
  }

  llvm::Type *ty = structType_->codeGen(ctx);
  llvm::StructType *llvmStructType = llvm::dyn_cast<llvm::StructType>(ty);

  if (!llvmStructType) {
    ctx.emitCodegenError("Expected struct type but got different type");
  }

  int memberIndex = structType_->getDecl()->lookupMemberIndex(memberName_);

  return ctx.builder.CreateStructGEP(llvmStructType, structPtr, memberIndex);
}

llvm::Value *ArrayAccess::codeGen(CodegenContext &ctx) {
  llvm::Value *elemPtr = codeGenLValue(ctx);

  if (!elemPtr) {
    ctx.emitCodegenError("Failed to generate lvalue for array access");
  }

  llvm::Type *ty = arrayType_->codeGen(ctx);
  llvm::ArrayType *arrayType = llvm::dyn_cast<llvm::ArrayType>(ty);

  if (!arrayType) {
    ctx.emitCodegenError("Expected array type but got different type");
  }

  llvm::Type *elemType = arrayType->getElementType();

  return ctx.builder.CreateLoad(elemType, elemPtr, "arrayval");
}

llvm::Value *ArrayAccess::codeGenLValue(CodegenContext &ctx) {
  llvm::Value *arrayPtr = arrayExpr_->codeGenLValue(ctx);

  if (!arrayPtr) {
    ctx.emitCodegenError("Failed to generate lvalue for array expression");
  }

  llvm::Value *indexVal = indexExpr_->codeGen(ctx);

  if (!indexVal) {
    ctx.emitCodegenError("Failed to generate index expression for array access");
  }

  if (!indexVal->getType()->isIntegerTy()) {
    ctx.emitCodegenError("Array index must be an integer type");
  }

  if (!arrayType_) {
    ctx.emitCodegenError("Array access has null type");
  }

  llvm::Type *ty = arrayType_->codeGen(ctx);
  llvm::ArrayType *arrayType = llvm::dyn_cast<llvm::ArrayType>(ty);

  if (!arrayType) {
    ctx.emitCodegenError("Expected array type but got different type");
  }

  llvm::Value *zero = llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx.llvmContext), 0);
  std::vector<llvm::Value *> indices = {zero, indexVal};

  return ctx.builder.CreateGEP(arrayType, arrayPtr, indices, "arrayidx");
}

llvm::Value *PtrIndexAccess::codeGen(CodegenContext &ctx) {
  llvm::Value *elemPtr = codeGenLValue(ctx);

  if (!elemPtr) {
    ctx.emitCodegenError("Failed to generate lvalue for pointer index access");
  }

  llvm::Type *ty = ptrType_->codeGen(ctx);
  llvm::Type *dirType = ptrType_->target()->codeGen(ctx);

  return ctx.builder.CreateLoad(dirType, elemPtr, "ptrval");
}

llvm::Value *PtrIndexAccess::codeGenLValue(CodegenContext &ctx) {
  llvm::Value *ptrVal = ptrExpr_->codeGen(ctx);

  if (!ptrVal) {
    ctx.emitCodegenError("Failed to generate pointer expression for indexing");
  }

  if (!ptrVal->getType()->isPointerTy()) {
    ctx.emitCodegenError("Cannot index into non-pointer type");
  }

  llvm::Value *indexVal = indexExpr_->codeGen(ctx);

  if (!indexVal) {
    ctx.emitCodegenError("Failed to generate index expression for pointer access");
  }

  if (!indexVal->getType()->isIntegerTy()) {
    ctx.emitCodegenError("Pointer index must be an integer type");
  }

  if (!ptrType_) {
    ctx.emitCodegenError("Pointer access has null type");
  }

  llvm::Type *ty = ptrType_->target()->codeGen(ctx);

  std::vector<llvm::Value *> indices = {indexVal};

  return ctx.builder.CreateGEP(ty, ptrVal, indices, "ptridx");
}

llvm::Value *IntLiteral::codeGen(CodegenContext &ctx) {
  return llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx.llvmContext), value_, getType()->isSigned());
}

llvm::Value *FloatLiteral::codeGen(CodegenContext &ctx) {
  return llvm::ConstantFP::get(llvm::Type::getFloatTy(ctx.llvmContext), value_);
}

llvm::Value *StringLiteral::codeGen(CodegenContext &ctx) {

  llvm::GlobalVariable *gvar = ctx.builder.CreateGlobalString(value_);

  llvm::Value *zero = ctx.builder.getInt32(0);
  return ctx.builder.CreateInBoundsGEP(gvar->getValueType(), gvar, {zero, zero});
}

llvm::Value *NullptrLiteral::codeGen(CodegenContext &ctx) {
  return llvm::ConstantPointerNull::get(llvm::PointerType::get(ctx.llvmContext, 0));
}

llvm::Value *FunctionReference::codeGen(CodegenContext &ctx) {
  if (isVariable_) {
    // function pointer variable
    if (!varExpr_) {
      ctx.emitCodegenError("Function reference with null variable expression");
    }

    // will be loaded by variable reference codeGen
    return varExpr_->codeGen(ctx);
  } else {
    // direct function reference
    llvm::Function *func = ctx.module->getFunction(name_);

    if (!func) {
      ctx.emitCodegenError("Unknown function '" + name_ + "'");
    }

    return func;
  }
}

llvm::Value *FunctionCall::codeGen(CodegenContext &ctx) {
  llvm::Value *funcValue = funcRef_->codeGen(ctx);

  if (!funcValue) {
    ctx.emitCodegenError("Failed to generate function reference");
  }

  llvm::FunctionType *funcType = nullptr;
  auto astType = funcRef_->getType();

  // get function type from either direct function or function pointer
  if (auto astFuncType = std::dynamic_pointer_cast<ast::FunctionTypeNode>(astType)) {
    funcType = llvm::dyn_cast<llvm::FunctionType>(astFuncType->codeGen(ctx));
  } else if (auto astFuncPtrType = std::dynamic_pointer_cast<ast::PointerTypeNode>(astType)) {
    auto astFuncType = std::dynamic_pointer_cast<ast::FunctionTypeNode>(astFuncPtrType->target());
    if (astFuncType) {
      funcType = llvm::dyn_cast<llvm::FunctionType>(astFuncType->codeGen(ctx));
    }
  }

  if (!funcType) {
    ctx.emitCodegenError("Could not determine function type");
  }

  if (funcType->getNumParams() != args_.size()) {
    ctx.emitCodegenError("Function expects " + std::to_string(funcType->getNumParams()) + " arguments, got " +
                         std::to_string(args_.size()));
  }

  std::vector<llvm::Value *> args;
  args.reserve(args_.size());

  for (size_t i = 0; i < args_.size(); ++i) {
    llvm::Value *argValue = args_.at(i)->codeGen(ctx);

    if (!argValue) {
      ctx.emitCodegenError("Failed to generate argument " + std::to_string(i) + " for function call");
    }

    llvm::Type *expectedType = funcType->getParamType(i);
    bool argIsSigned = args_.at(i)->getType()->isSigned();
    llvm::Value *convertedArg = ctx.convertIfNeeded(argValue, expectedType, argIsSigned);

    args.push_back(convertedArg);
  }

  llvm::Value *result =
      ctx.builder.CreateCall(funcType, funcValue, args, funcType->getReturnType()->isVoidTy() ? "" : "calltmp");

  if (!result) {
    ctx.emitCodegenError("Failed to create function call");
  }

  return result;
}

llvm::Value *BinaryOperation::codeGen(CodegenContext &ctx) {

  llvm::Value *L = L_->codeGen(ctx);
  llvm::Value *R = R_->codeGen(ctx);

  if (!L || !R) {
    ctx.emitCodegenError("Failed to generate operands for binary operation");
  }

  // use left operand signedness conversion target
  bool operandIsSigned = L_->getType()->isSigned();
  R = ctx.convertIfNeeded(R, L->getType(), operandIsSigned);

  switch (opType_) {
  case BinaryOperationType::Add:

    if (L->getType()->isPointerTy()) {
      if (!R->getType()->isIntegerTy()) {
        ctx.emitCodegenError("Cannot add non-integer to pointer");
      }
      return ctx.builder.CreateGEP(L->getType(), L, R);
    } else if (R->getType()->isPointerTy()) {
      if (!L->getType()->isIntegerTy()) {
        ctx.emitCodegenError("Cannot add non-integer to pointer");
      }
      return ctx.builder.CreateGEP(R->getType(), R, L);
    } else if (L->getType()->isFloatingPointTy() && R->getType()->isFloatingPointTy()) {
      return ctx.builder.CreateFAdd(L, R, "faddtmp");
    } else if (L->getType()->isIntegerTy() && R->getType()->isIntegerTy()) {
      return ctx.builder.CreateAdd(L, R, "addtmp");
    } else {
      ctx.emitCodegenError("Addition requires numeric operands of the same type");
    }

  case BinaryOperationType::Subtract:

    if (L->getType()->isPointerTy()) {
      if (!R->getType()->isIntegerTy()) {
        ctx.emitCodegenError("Cannot subtract non-integer from pointer");
      }
      return ctx.builder.CreateGEP(L->getType(), L, ctx.builder.CreateNeg(R));
    } else if (L->getType()->isFloatingPointTy() && R->getType()->isFloatingPointTy()) {
      return ctx.builder.CreateFSub(L, R, "fsubtmp");
    } else if (L->getType()->isIntegerTy() && R->getType()->isIntegerTy()) {
      return ctx.builder.CreateSub(L, R, "subtmp");
    } else {
      ctx.emitCodegenError("Subtraction requires numeric operands of the same type");
    }

  case BinaryOperationType::Multiply:
    if (L->getType()->isFloatingPointTy() && R->getType()->isFloatingPointTy()) {
      return ctx.builder.CreateFMul(L, R, "fmultmp");
    } else if (L->getType()->isIntegerTy() && R->getType()->isIntegerTy()) {
      return ctx.builder.CreateMul(L, R, "multmp");
    } else {
      ctx.emitCodegenError("Multiplication requires numeric operands of the same type");
    }

  case BinaryOperationType::Divide:
    if (L->getType()->isFloatingPointTy() && R->getType()->isFloatingPointTy()) {
      return ctx.builder.CreateFDiv(L, R, "fdivtmp");
    } else if (L->getType()->isIntegerTy() && R->getType()->isIntegerTy()) {
      if (operandIsSigned) {
        return ctx.builder.CreateSDiv(L, R, "sdivtmp");
      } else {
        return ctx.builder.CreateUDiv(L, R, "udivtmp");
      }
    } else {
      ctx.emitCodegenError("Division requires numeric operands of the same type");
    }

  case BinaryOperationType::Modulo:
    if (L->getType()->isFloatingPointTy() && R->getType()->isFloatingPointTy()) {
      return ctx.builder.CreateFRem(L, R, "fremtmp");
    } else if (L->getType()->isIntegerTy() && R->getType()->isIntegerTy()) {
      if (operandIsSigned) {
        return ctx.builder.CreateSRem(L, R, "sremtmp");
      } else {
        return ctx.builder.CreateURem(L, R, "uremtmp");
      }
    } else {
      ctx.emitCodegenError("Modulo requires numeric operands of the same type");
    }

  case BinaryOperationType::Less:
    if (L->getType()->isPointerTy() && R->getType()->isPointerTy()) {
      return ctx.builder.CreateICmpULT(L, R);
    } else if (L->getType()->isFloatingPointTy() && R->getType()->isFloatingPointTy()) {
      return ctx.builder.CreateFCmpOLT(L, R, "fcmpolt");
    } else if (L->getType()->isIntegerTy() && R->getType()->isIntegerTy()) {
      if (operandIsSigned) {
        return ctx.builder.CreateICmpSLT(L, R, "cmpslt");
      } else {
        return ctx.builder.CreateICmpULT(L, R, "cmpult");
      }
    } else {
      ctx.emitCodegenError("Comparison requires operands of the same type");
    }

  case BinaryOperationType::More:
    if (L->getType()->isPointerTy() && R->getType()->isPointerTy()) {
      return ctx.builder.CreateICmpUGT(L, R);
    } else if (L->getType()->isFloatingPointTy() && R->getType()->isFloatingPointTy()) {
      return ctx.builder.CreateFCmpOGT(L, R, "fcmpogt");
    } else if (L->getType()->isIntegerTy() && R->getType()->isIntegerTy()) {
      if (operandIsSigned) {
        return ctx.builder.CreateICmpSGT(L, R, "cmpsgt");
      } else {
        return ctx.builder.CreateICmpUGT(L, R, "cmpugt");
      }
    } else {
      ctx.emitCodegenError("Comparison requires operands of the same type");
    }

  case BinaryOperationType::LessEqual:
    if (L->getType()->isPointerTy() && R->getType()->isPointerTy()) {
      return ctx.builder.CreateICmpULE(L, R);
    } else if (L->getType()->isFloatingPointTy() && R->getType()->isFloatingPointTy()) {
      return ctx.builder.CreateFCmpOLE(L, R, "fcmpole");
    } else if (L->getType()->isIntegerTy() && R->getType()->isIntegerTy()) {
      if (operandIsSigned) {
        return ctx.builder.CreateICmpSLE(L, R, "cmpsle");
      } else {
        return ctx.builder.CreateICmpULE(L, R, "cmpule");
      }
    } else {
      ctx.emitCodegenError("Comparison requires operands of the same type");
    }

  case BinaryOperationType::MoreEqual:
    if (L->getType()->isPointerTy() && R->getType()->isPointerTy()) {
      return ctx.builder.CreateICmpUGE(L, R);
    } else if (L->getType()->isFloatingPointTy() && R->getType()->isFloatingPointTy()) {
      return ctx.builder.CreateFCmpOGE(L, R, "fcmpoge");
    } else if (L->getType()->isIntegerTy() && R->getType()->isIntegerTy()) {
      if (operandIsSigned) {
        return ctx.builder.CreateICmpSGE(L, R, "cmpsge");
      } else {
        return ctx.builder.CreateICmpUGE(L, R, "cmpuge");
      }
    } else {
      ctx.emitCodegenError("Comparison requires operands of the same type");
    }

  case BinaryOperationType::Equal:
    if (L->getType()->isPointerTy() && R->getType()->isPointerTy()) {
      return ctx.builder.CreateICmpEQ(L, R);
    } else if (L->getType()->isFloatingPointTy() && R->getType()->isFloatingPointTy()) {
      return ctx.builder.CreateFCmpOEQ(L, R, "fcmpoeq");
    } else if (L->getType()->isIntegerTy() && R->getType()->isIntegerTy()) {
      return ctx.builder.CreateICmpEQ(L, R);
    } else {
      ctx.emitCodegenError("Equality comparison requires operands of the same type");
    }

  case BinaryOperationType::NotEqual:
    if (L->getType()->isPointerTy() && R->getType()->isPointerTy()) {
      return ctx.builder.CreateICmpNE(L, R);
    } else if (L->getType()->isFloatingPointTy() && R->getType()->isFloatingPointTy()) {
      return ctx.builder.CreateFCmpONE(L, R, "fcmpone");
    } else if (L->getType()->isIntegerTy() && R->getType()->isIntegerTy()) {
      return ctx.builder.CreateICmpNE(L, R);
    } else {
      ctx.emitCodegenError("Inequality comparison requires operands of the same type");
    }

  case BinaryOperationType::And:
    if (L->getType()->isIntegerTy() && R->getType()->isIntegerTy()) {
      return ctx.builder.CreateAnd(L, R, "andtmp");
    } else {
      ctx.emitCodegenError("Bitwise AND requires integer operands");
    }

  case BinaryOperationType::Or:
    if (L->getType()->isIntegerTy() && R->getType()->isIntegerTy()) {
      return ctx.builder.CreateOr(L, R, "ortmp");
    } else {
      ctx.emitCodegenError("Bitwise OR requires integer operands");
    }

  case BinaryOperationType::Not:
    if (L->getType()->isIntegerTy()) {
      return ctx.builder.CreateNot(L, "nottmp");
    } else {
      ctx.emitCodegenError("Bitwise NOT requires integer operand");
    }
    break;
  }

  return nullptr;
}

llvm::Value *SizeOf::codeGen(CodegenContext &ctx) {
  llvm::Type *ty = targetType_->codeGen(ctx);

  if (!ty) {
    ctx.emitCodegenError("Failed to generate type for sizeof operator");
  }

  llvm::DataLayout dataLayout = ctx.module->getDataLayout();
  uint64_t sizeInBytes = dataLayout.getTypeAllocSize(ty);

  return llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx.llvmContext), sizeInBytes);
}
} // namespace axen::ast
