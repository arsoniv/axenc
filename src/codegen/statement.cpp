#include <llvm/IR/Instructions.h>
#include <llvm/IR/Type.h>
#include <llvm/Support/Casting.h>

#include "nodes/context.hpp"
#include "nodes/statement.hpp"

namespace axen::ast {

void VariableDeclaration::codeGen(CodegenContext &ctx) {

  llvm::Type *type = type_->codeGen(ctx);

  if (!type) {
    error::reportError(error::ErrorType::Codegen, "Failed to create type for variable '" + name_ + "'");
  }

  llvm::AllocaInst *variable = ctx.builder.CreateAlloca(type, nullptr, name_);
  if (!variable) {
    error::reportError(error::ErrorType::Codegen, "Failed to allocate variable '" + name_ + "'");
  }

  if (initialValue_) {
    llvm::Value *init_val = initialValue_->codeGen(ctx);

    if (!init_val) {
      error::reportError(error::ErrorType::Codegen, "Failed to generate initial value for variable '" + name_ + "'");
    }

    llvm::Value *converted = ctx.convertIfNeeded(init_val, type, initialValue_->isSigned());

    if (ctx.checkTypeCompatible(type, converted->getType())) {
      ctx.builder.CreateStore(converted, variable);
    } else {
      error::reportError(error::ErrorType::Codegen,
                         "Cannot initialize variable '" + name_ + "' with incompatible type");
    }
  }

  ctx.declareVariable(name_, variable);
}

void AssignmentStatement::codeGen(CodegenContext &ctx) {
  llvm::Value *ptr = target_->codeGenLValue(ctx);

  if (!ptr) {
    error::reportError(error::ErrorType::Codegen, "Failed to generate lvalue for assignment target");
  }

  llvm::Value *val = value_->codeGen(ctx);

  if (!val) {
    error::reportError(error::ErrorType::Codegen, "Failed to generate value for assignment");
  }

  llvm::Type *targetType = nullptr;
  if (auto *alloca = llvm::dyn_cast<llvm::AllocaInst>(ptr)) {
    targetType = alloca->getAllocatedType();
  } else if (auto *gep = llvm::dyn_cast<llvm::GetElementPtrInst>(ptr)) {
    targetType = gep->getResultElementType();
  } else {
    targetType = val->getType();
  }

  llvm::Value *converted = ctx.convertIfNeeded(val, targetType, value_->isSigned());

  ctx.builder.CreateStore(converted, ptr);
}

void Return::codeGen(CodegenContext &ctx) {

  if (value_) {
    llvm::Value *value = value_->codeGen(ctx);

    if (!value) {
      error::reportError(error::ErrorType::Codegen, "Failed to generate return value");
    }

    llvm::Type *returnType = ctx.builder.getCurrentFunctionReturnType();

    llvm::Value *converted = ctx.convertIfNeeded(value, returnType, value_->isSigned());

    if (!ctx.checkTypeCompatible(returnType, converted->getType())) {
      error::reportError(error::ErrorType::Codegen, "Return value type does not match function return type");
    }

    ctx.builder.CreateRet(converted);
  } else {
    llvm::Type *returnType = ctx.builder.getCurrentFunctionReturnType();

    if (!returnType->isVoidTy()) {
      error::reportError(error::ErrorType::Codegen, "Non-void function must return a value");
    }

    ctx.builder.CreateRetVoid();
  }
}

void If::codeGen(CodegenContext &ctx) {

  llvm::Function *parentFunction = ctx.builder.GetInsertBlock()->getParent();
  if (!parentFunction) {
    error::reportError(error::ErrorType::Codegen, "If statement has no parent function");
  }

  llvm::BasicBlock *thenBB = llvm::BasicBlock::Create(ctx.llvmContext, "then", parentFunction);
  llvm::BasicBlock *elseBB = falseBody_ ? llvm::BasicBlock::Create(ctx.llvmContext, "else", parentFunction) : nullptr;
  llvm::BasicBlock *mergeBB = llvm::BasicBlock::Create(ctx.llvmContext, "ifcont", parentFunction);

  llvm::Value *condValue = condition_->codeGen(ctx);
  if (!condValue) {
    error::reportError(error::ErrorType::Codegen, "Failed to generate condition for if statement");
  }

  if (!condValue->getType()->isIntegerTy()) {
    error::reportError(error::ErrorType::Codegen, "If statement condition must be integer type");
  }

  if (falseBody_) {
    ctx.builder.CreateCondBr(condValue, thenBB, elseBB);
  } else {
    ctx.builder.CreateCondBr(condValue, thenBB, mergeBB);
  }

  ctx.builder.SetInsertPoint(thenBB);
  for (auto &stmt : trueBody_) {
    stmt->codeGen(ctx);
  }

  // create terminator if none exists
  if (!ctx.builder.GetInsertBlock()->getTerminator()) {
    ctx.builder.CreateBr(mergeBB);
  }

  if (falseBody_) {
    ctx.builder.SetInsertPoint(elseBB);
    for (auto &stmt : *falseBody_) {
      stmt->codeGen(ctx);
    }

    // create terminator if none exists
    if (!ctx.builder.GetInsertBlock()->getTerminator()) {
      ctx.builder.CreateBr(mergeBB);
    }
  }

  ctx.builder.SetInsertPoint(mergeBB);
}

void While::codeGen(CodegenContext &ctx) {

  llvm::Function *parentFunction = ctx.builder.GetInsertBlock()->getParent();
  if (!parentFunction) {
    error::reportError(error::ErrorType::Codegen, "While statement has no parent function");
  }

  llvm::BasicBlock *condBB = llvm::BasicBlock::Create(ctx.llvmContext, "cond", parentFunction);
  llvm::BasicBlock *bodyBB = llvm::BasicBlock::Create(ctx.llvmContext, "body", parentFunction);
  llvm::BasicBlock *exitBB = llvm::BasicBlock::Create(ctx.llvmContext, "exit", parentFunction);

  ctx.builder.CreateBr(condBB);

  ctx.builder.SetInsertPoint(condBB);

  llvm::Value *condValue = condition_->codeGen(ctx);
  if (!condValue) {
    error::reportError(error::ErrorType::Codegen, "Failed to generate condition for while statement");
  }

  if (!condValue->getType()->isIntegerTy()) {
    error::reportError(error::ErrorType::Codegen, "While statement condition must be integer type");
  }

  ctx.builder.CreateCondBr(condValue, bodyBB, exitBB);

  ctx.builder.SetInsertPoint(bodyBB);
  for (auto &stmt : body_) {
    stmt->codeGen(ctx);
  }

  ctx.builder.CreateBr(condBB);

  ctx.builder.SetInsertPoint(exitBB);
}

void ExpressionStatement::codeGen(CodegenContext &ctx) {
  llvm::Value *result = expression_->codeGen(ctx);

  if (!result) {
    error::reportError(error::ErrorType::Codegen, "Failed to generate expression statement");
  }
}

} // namespace axen::ast
