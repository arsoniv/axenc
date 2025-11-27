#include "nodes/expression.hpp"
#include "nodes/context.hpp"
#include "nodes/function.hpp"
#include "nodes/type.hpp"
#include <string>

namespace axen::ast {

void VariableReference::analyze(AnalysisContext &ctx) {
  ctx.currentRow = span_.startRow;
  ctx.currentCol = span_.startCol;

  if (name_.empty()) {
    ctx.emitAnalysisError("Variable reference with empty name", span_);
    return;
  }

  auto varType = ctx.lookupVariable(name_);
  if (!varType) {
    ctx.emitAnalysisError("Undefined variable '" + name_ + "'", span_);
    return;
  }

  type_ = varType;
}

void StructAccess::analyze(AnalysisContext &ctx) {
  ctx.currentRow = span_.startRow;
  ctx.currentCol = span_.startCol;

  if (!structExpr_) {
    ctx.emitAnalysisError("Struct access with null struct expression", span_);
    return;
  }
  structExpr_->analyze(ctx);

  if (memberName_.empty()) {
    ctx.emitAnalysisError("Struct access with empty member name", span_);
    return;
  }

  auto structExprType = structExpr_->getType();
  if (!structExprType) {
    ctx.emitAnalysisError("Cannot determine type of struct expression for member access", span_);
    return;
  }

  // validate member existance
  if (structType_ && structType_->getDecl()) {
    auto memberType = structType_->getDecl()->lookupMemberType(memberName_);
    if (!memberType) {
      ctx.emitAnalysisError("Struct '" + structName_ + "' has no member named '" + memberName_ + "'", span_);
      return;
    }

    type_ = memberType;
  }
}

void ArrayAccess::analyze(AnalysisContext &ctx) {
  ctx.currentRow = span_.startRow;
  ctx.currentCol = span_.startCol;

  if (!arrayExpr_) {
    ctx.emitAnalysisError("Array access with null array expression", span_);
    return;
  }
  arrayExpr_->analyze(ctx);

  auto arrayExprType = arrayExpr_->getType();
  if (!arrayExprType) {
    ctx.emitAnalysisError("Cannot determine type of array expression", span_);
    return;
  }

  // ensure this is an array type
  auto arrType = std::dynamic_pointer_cast<ArrayTypeNode>(arrayExprType);
  if (!arrType) {
    ctx.emitAnalysisError("Subscript operator used on non-array type", span_);
    return;
  }

  if (!indexExpr_) {
    ctx.emitAnalysisError("Array access with null index expression", span_);
    return;
  }
  indexExpr_->analyze(ctx);

  auto indexType = indexExpr_->getType();
  if (!indexType) {
    ctx.emitAnalysisError("Cannot determine type of index expression", span_);
    return;
  }

  // ensure index is a primitive type
  auto indexPrimType = std::dynamic_pointer_cast<PrimitiveTypeNode>(indexType);
  if (!indexPrimType) {
    ctx.emitAnalysisError("Array index must be an integer type", span_);
    return;
  }

  // update type_ to the element's type
  if (arrType) {
    type_ = arrType->target();
  }
}

void PtrIndexAccess::analyze(AnalysisContext &ctx) {
  ctx.currentRow = span_.startRow;
  ctx.currentCol = span_.startCol;

  if (!ptrExpr_) {
    ctx.emitAnalysisError("Pointer index access with null pointer expression", span_);
    return;
  }
  ptrExpr_->analyze(ctx);

  auto ptrExprType = ptrExpr_->getType();
  if (!ptrExprType) {
    ctx.emitAnalysisError("Cannot determine type of pointer expression", span_);
    return;
  }

  // ensure this is a pointer type
  auto pType = std::dynamic_pointer_cast<PointerTypeNode>(ptrExprType);
  if (!pType) {
    ctx.emitAnalysisError("Subscript operator used on non-pointer type", span_);
    return;
  }

  if (!indexExpr_) {
    ctx.emitAnalysisError("Pointer index access with null index expression", span_);
    return;
  }
  indexExpr_->analyze(ctx);

  auto indexType = indexExpr_->getType();
  if (!indexType) {
    ctx.emitAnalysisError("Cannot determine type of index expression", span_);
    return;
  }

  // ensure index is a primitive type
  auto indexPrimType = std::dynamic_pointer_cast<PrimitiveTypeNode>(indexType);
  if (!indexPrimType) {
    ctx.emitAnalysisError("Pointer index must be an integer type", span_);
    return;
  }

  // update type_ to the derived type
  if (pType) {
    type_ = pType->target();
  }
}

void Dref::analyze(AnalysisContext &ctx) {
  ctx.currentRow = span_.startRow;
  ctx.currentCol = span_.startCol;

  if (!target_) {
    ctx.emitAnalysisError("Dereference with null target expression", span_);
    return;
  }
  target_->analyze(ctx);

  auto targetType = target_->getType();
  if (!targetType) {
    ctx.emitAnalysisError("Cannot determine type of dereference target", span_);
    return;
  }

  // ensure pointer type
  auto ptrType = std::dynamic_pointer_cast<PointerTypeNode>(targetType);
  if (!ptrType) {
    ctx.emitAnalysisError("Cannot dereference non-pointer type", span_);
    return;
  }

  // update type_ to the derived type
  derivedType_ = ptrType->target();
  type_ = std::make_shared<PointerTypeNode>(derivedType_, span_);
}

void AddressOf::analyze(AnalysisContext &ctx) {
  ctx.currentRow = span_.startRow;
  ctx.currentCol = span_.startCol;

  if (!target_) {
    ctx.emitAnalysisError("Address-of with null target expression", span_);
    return;
  }
  target_->analyze(ctx);

  auto targetType = target_->getType();
  if (!targetType) {
    ctx.emitAnalysisError("Cannot determine type of address-of target", span_);
    return;
  }

  // update type_ to be a pointer to the target type
  type_ = std::make_shared<PointerTypeNode>(targetType, span_);
}

void IntLiteral::analyze(AnalysisContext &ctx) {
  ctx.currentRow = span_.startRow;
  ctx.currentCol = span_.startCol;

  if (!type_) {
    ctx.emitAnalysisError("Integer literal has no type", span_);
  }
}

void FloatLiteral::analyze(AnalysisContext &ctx) {
  ctx.currentRow = span_.startRow;
  ctx.currentCol = span_.startCol;

  if (!type_) {
    ctx.emitAnalysisError("Float literal has no type", span_);
  }
}

void StringLiteral::analyze(AnalysisContext &ctx) {
  ctx.currentRow = span_.startRow;
  ctx.currentCol = span_.startCol;

  if (!type_) {
    ctx.emitAnalysisError("String literal has no type", span_);
  }
}

void NullptrLiteral::analyze(AnalysisContext &ctx) {
  ctx.currentRow = span_.startRow;
  ctx.currentCol = span_.startCol;

  if (!type_) {
    ctx.emitAnalysisError("Nullptr literal has no type", span_);
  }
}

void FunctionReference::analyze(AnalysisContext &ctx) {
  ctx.currentRow = span_.startRow;
  ctx.currentCol = span_.startCol;

  if (isVariable_) {
    // function pointer
    if (!varExpr_) {
      ctx.emitAnalysisError("Function reference with null variable expression", span_);
      return;
    }

    varExpr_->analyze(ctx);
    type_ = varExpr_->getType();

    if (!type_) {
      ctx.emitAnalysisError("Function pointer variable has no type", span_);
    }
  } else {
    // direct function reference
    if (name_.empty()) {
      ctx.emitAnalysisError("Function reference with empty function name", span_);
      return;
    }

    auto func = ctx.lookupFunction(name_);
    if (!func) {
      ctx.emitAnalysisError("Reference to undefined function '" + name_ + "'", span_);
      return;
    }

    if (!type_) {
      ctx.emitAnalysisError("Function reference has no type", span_);
    }
  }
}

void FunctionCall::analyze(AnalysisContext &ctx) {
  ctx.currentRow = span_.startRow;
  ctx.currentCol = span_.startCol;

  if (!funcRef_) {
    ctx.emitAnalysisError("Function call with null function reference", span_);
    return;
  }

  funcRef_->analyze(ctx);

  // analyze all arguments first
  for (size_t i = 0; i < args_.size(); ++i) {
    if (!args_[i]) {
      ctx.emitAnalysisError("Null argument at position " + std::to_string(i) + " in function call", span_);
      continue;
    }
    args_[i]->analyze(ctx);

    if (!args_[i]->getType()) {
      ctx.emitAnalysisError("Argument at position " + std::to_string(i) + " has no type in function call", span_);
    }
  }

  if (funcRef_->isVariable()) {
    // function pointer call
    auto funcPtrType = funcRef_->getType();
    if (!funcPtrType) {
      ctx.emitAnalysisError("Function pointer has no type", span_);
      return;
    }

    // function pointer should be ptr to function type
    auto ptrType = std::dynamic_pointer_cast<PointerTypeNode>(funcPtrType);
    if (!ptrType) {
      ctx.emitAnalysisError("Cannot call non-function-pointer type", span_);
      return;
    }

    auto funcType = std::dynamic_pointer_cast<FunctionTypeNode>(ptrType->target());
    if (!funcType) {
      ctx.emitAnalysisError("Pointer does not point to a function type", span_);
      return;
    }

    // ensure argument count matches function parameter count
    const auto &paramTypes = funcType->getParameters();
    if (args_.size() != paramTypes.size()) {
      ctx.emitAnalysisError("Function pointer expects " + std::to_string(paramTypes.size()) + " argument(s) but " +
                                std::to_string(args_.size()) + " provided",
                            span_);
    }

    // update type_ to the function's return type
    type_ = funcType->getReturn();
  } else {
    // direct function call
    std::string funcName = funcRef_->getName();

    auto func = ctx.lookupFunction(funcName);
    if (!func) {
      ctx.emitAnalysisError("Call to undefined function '" + funcName + "'", span_);
      return;
    }

    // ensure argument count matches function parameter count
    const auto &params = func->getParams();
    size_t expectedArgCount = params.size();

    if (args_.size() != expectedArgCount) {
      ctx.emitAnalysisError("Function '" + funcName + "' expects " + std::to_string(expectedArgCount) +
                                " argument(s) but " + std::to_string(args_.size()) + " provided",
                            span_);
    } else {
      // ensure argument types match parameter types
      for (size_t i = 0; i < args_.size(); ++i) {
        auto argType = args_[i]->getType();
        auto paramType = (params)[i].second;

        if (!argType) {
          ctx.emitAnalysisError("Type of argument " + std::to_string(i + 1) + " in call to '" + funcName +
                                    "' could not be determined",
                                span_);
        }
        if (!paramType) {
          ctx.emitAnalysisError("Type of parameter '" + (params)[i].first + "' could not be determined", span_);
        }

        if (argType && paramType) {
          // this is messy:

          auto argPtrType = std::dynamic_pointer_cast<PointerTypeNode>(argType);
          auto paramPtrType = std::dynamic_pointer_cast<PointerTypeNode>(paramType);

          if ((argPtrType && !paramPtrType) || (!argPtrType && paramPtrType)) {
            ctx.emitAnalysisError("Argument " + std::to_string(i + 1) + " type mismatch in call to '" + funcName +
                                      "' (parameter '" + (params)[i].first + "')",
                                  span_);
          }

          auto argArrType = std::dynamic_pointer_cast<ArrayTypeNode>(argType);
          auto paramArrType = std::dynamic_pointer_cast<ArrayTypeNode>(paramType);

          if ((argArrType && !paramArrType) || (!argArrType && paramArrType)) {
            ctx.emitAnalysisError("Argument " + std::to_string(i + 1) + " type mismatch in call to '" + funcName +
                                      "' (parameter '" + (params)[i].first + "')",
                                  span_);
          }
        }
      }
    }

    // update type_ to the function's return type
    type_ = func->getReturnType();
    if (!type_) {
      ctx.emitAnalysisError("Function '" + funcName + "' has no return type", span_);
    }
  }
}

void BinaryOperation::analyze(AnalysisContext &ctx) {
  ctx.currentRow = span_.startRow;
  ctx.currentCol = span_.startCol;

  // left operand
  if (!L_) {
    ctx.emitAnalysisError("Binary operation with null left operand", span_);
    return;
  }
  L_->analyze(ctx);

  auto leftType = L_->getType();
  if (!leftType) {
    ctx.emitAnalysisError("Cannot determine type of left operand in binary operation", span_);
    return;
  }

  // right operand
  if (!R_) {
    ctx.emitAnalysisError("Binary operation with null right operand", span_);
    return;
  }
  R_->analyze(ctx);

  auto rightType = R_->getType();
  if (!rightType) {
    ctx.emitAnalysisError("Cannot determine type of right operand in binary operation", span_);
    return;
  }

  // for comparison operations the result type is bool
  if (opType_ == BinaryOperationType::Less || opType_ == BinaryOperationType::More ||
      opType_ == BinaryOperationType::Equal) {
    type_ = std::make_shared<PrimitiveTypeNode>(PrimitiveType::Bool, false, span_);
  } else {
    // for arithmetic operations the result type is the same as operand's
    type_ = leftType;
  }

  if (!type_) {
    ctx.emitAnalysisError("Cannot determine result type of binary operation", span_);
  }
}

void SizeOf::analyze(AnalysisContext &ctx) {
  ctx.currentRow = span_.startRow;
  ctx.currentCol = span_.startCol;

  if (!targetType_) {
    ctx.emitAnalysisError("SizeOf with null target type", span_);
    return;
  }

  // sizeof always results in an unsigned long
  type_ = std::make_shared<PrimitiveTypeNode>(PrimitiveType::Long, false, span_);
}

} // namespace axen::ast
