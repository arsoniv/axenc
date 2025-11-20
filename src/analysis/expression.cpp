#include "nodes/expression.hpp"
#include "nodes/context.hpp"
#include "nodes/function.hpp"
#include "nodes/type.hpp"
#include <string>

namespace axen::ast {

void VariableReference::analyze(AnalysisContext &ctx) {
  ctx.currentRow = row_;
  ctx.currentCol = col_;

  if (name_.empty()) {
    ctx.reportError("Variable reference with empty name");
    return;
  }

  auto varType = ctx.lookupVariable(name_);
  if (!varType) {
    ctx.reportError("Undefined variable '" + name_ + "'");
    return;
  }

  type_ = varType;
}

void StructAccess::analyze(AnalysisContext &ctx) {
  ctx.currentRow = row_;
  ctx.currentCol = col_;

  if (!structExpr_) {
    ctx.reportError("Struct access with null struct expression");
    return;
  }
  structExpr_->analyze(ctx);

  if (memberName_.empty()) {
    ctx.reportError("Struct access with empty member name");
    return;
  }

  auto structExprType = structExpr_->getType();
  if (!structExprType) {
    ctx.reportError("Cannot determine type of struct expression for member access");
    return;
  }

  // validate member existance
  if (structType_ && structType_->getDecl()) {
    auto memberType = structType_->getDecl()->lookupMemberType(memberName_);
    if (!memberType) {
      ctx.reportError("Struct '" + structName_ + "' has no member named '" + memberName_ + "'");
      return;
    }

    type_ = memberType;
  }
}

void ArrayAccess::analyze(AnalysisContext &ctx) {
  ctx.currentRow = row_;
  ctx.currentCol = col_;

  if (!arrayExpr_) {
    ctx.reportError("Array access with null array expression");
    return;
  }
  arrayExpr_->analyze(ctx);

  auto arrayExprType = arrayExpr_->getType();
  if (!arrayExprType) {
    ctx.reportError("Cannot determine type of array expression");
    return;
  }

  // ensure this is an array type
  auto arrType = std::dynamic_pointer_cast<ArrayTypeNode>(arrayExprType);
  if (!arrType) {
    ctx.reportError("Subscript operator used on non-array type");
    return;
  }

  if (!indexExpr_) {
    ctx.reportError("Array access with null index expression");
    return;
  }
  indexExpr_->analyze(ctx);

  auto indexType = indexExpr_->getType();
  if (!indexType) {
    ctx.reportError("Cannot determine type of index expression");
    return;
  }

  // ensure index is a primitive type
  auto indexPrimType = std::dynamic_pointer_cast<PrimitiveTypeNode>(indexType);
  if (!indexPrimType) {
    ctx.reportError("Array index must be an integer type");
    return;
  }

  // update type_ to the element's type
  if (arrType) {
    type_ = arrType->target();
  }
}

void PtrIndexAccess::analyze(AnalysisContext &ctx) {
  ctx.currentRow = row_;
  ctx.currentCol = col_;

  if (!ptrExpr_) {
    ctx.reportError("Pointer index access with null pointer expression");
    return;
  }
  ptrExpr_->analyze(ctx);

  auto ptrExprType = ptrExpr_->getType();
  if (!ptrExprType) {
    ctx.reportError("Cannot determine type of pointer expression");
    return;
  }

  // ensure this is a pointer type
  auto pType = std::dynamic_pointer_cast<PointerTypeNode>(ptrExprType);
  if (!pType) {
    ctx.reportError("Subscript operator used on non-pointer type");
    return;
  }

  if (!indexExpr_) {
    ctx.reportError("Pointer index access with null index expression");
    return;
  }
  indexExpr_->analyze(ctx);

  auto indexType = indexExpr_->getType();
  if (!indexType) {
    ctx.reportError("Cannot determine type of index expression");
    return;
  }

  // ensure index is a primitive type
  auto indexPrimType = std::dynamic_pointer_cast<PrimitiveTypeNode>(indexType);
  if (!indexPrimType) {
    ctx.reportError("Pointer index must be an integer type");
    return;
  }

  // update type_ to the derived type
  if (pType) {
    type_ = pType->target();
  }
}

void Dref::analyze(AnalysisContext &ctx) {
  ctx.currentRow = row_;
  ctx.currentCol = col_;

  if (!target_) {
    ctx.reportError("Dereference with null target expression");
    return;
  }
  target_->analyze(ctx);

  auto targetType = target_->getType();
  if (!targetType) {
    ctx.reportError("Cannot determine type of dereference target");
    return;
  }

  // ensure pointer type
  auto ptrType = std::dynamic_pointer_cast<PointerTypeNode>(targetType);
  if (!ptrType) {
    ctx.reportError("Cannot dereference non-pointer type");
    return;
  }

  // update type_ to the derived type
  derivedType_ = ptrType->target();
  type_ = std::make_shared<PointerTypeNode>(derivedType_);
}

void AddressOf::analyze(AnalysisContext &ctx) {
  ctx.currentRow = row_;
  ctx.currentCol = col_;

  if (!target_) {
    ctx.reportError("Address-of with null target expression");
    return;
  }
  target_->analyze(ctx);

  auto targetType = target_->getType();
  if (!targetType) {
    ctx.reportError("Cannot determine type of address-of target");
    return;
  }

  // update type_ to be a pointer to the target type
  type_ = std::make_shared<PointerTypeNode>(targetType);
}

void IntLiteral::analyze(AnalysisContext &ctx) {
  ctx.currentRow = row_;
  ctx.currentCol = col_;

  if (!type_) {
    ctx.reportError("Integer literal has no type");
  }
}

void FloatLiteral::analyze(AnalysisContext &ctx) {
  ctx.currentRow = row_;
  ctx.currentCol = col_;

  if (!type_) {
    ctx.reportError("Float literal has no type");
  }
}

void StringLiteral::analyze(AnalysisContext &ctx) {
  ctx.currentRow = row_;
  ctx.currentCol = col_;

  if (!type_) {
    ctx.reportError("String literal has no type");
  }
}

void NullptrLiteral::analyze(AnalysisContext &ctx) {
  ctx.currentRow = row_;
  ctx.currentCol = col_;

  if (!type_) {
    ctx.reportError("Nullptr literal has no type");
  }
}

void FunctionReference::analyze(AnalysisContext &ctx) {
  ctx.currentRow = row_;
  ctx.currentCol = col_;

  if (isVariable_) {
    // function pointer
    if (!varExpr_) {
      ctx.reportError("Function reference with null variable expression");
      return;
    }

    varExpr_->analyze(ctx);
    type_ = varExpr_->getType();

    if (!type_) {
      ctx.reportError("Function pointer variable has no type");
    }
  } else {
    // direct function reference
    if (name_.empty()) {
      ctx.reportError("Function reference with empty function name");
      return;
    }

    auto func = ctx.lookupFunction(name_);
    if (!func) {
      ctx.reportError("Reference to undefined function '" + name_ + "'");
      return;
    }

    if (!type_) {
      ctx.reportError("Function reference has no type");
    }
  }
}

void FunctionCall::analyze(AnalysisContext &ctx) {
  ctx.currentRow = row_;
  ctx.currentCol = col_;

  if (!funcRef_) {
    ctx.reportError("Function call with null function reference");
    return;
  }

  funcRef_->analyze(ctx);

  // analyze all arguments first
  for (size_t i = 0; i < args_.size(); ++i) {
    if (!args_[i]) {
      ctx.reportError("Null argument at position " + std::to_string(i) + " in function call");
      continue;
    }
    args_[i]->analyze(ctx);

    if (!args_[i]->getType()) {
      ctx.reportError("Argument at position " + std::to_string(i) + " has no type in function call");
    }
  }

  if (funcRef_->isVariable()) {
    // function pointer call
    auto funcPtrType = funcRef_->getType();
    if (!funcPtrType) {
      ctx.reportError("Function pointer has no type");
      return;
    }

    // function pointer should be ptr to function type
    auto ptrType = std::dynamic_pointer_cast<PointerTypeNode>(funcPtrType);
    if (!ptrType) {
      ctx.reportError("Cannot call non-function-pointer type");
      return;
    }

    auto funcType = std::dynamic_pointer_cast<FunctionTypeNode>(ptrType->target());
    if (!funcType) {
      ctx.reportError("Pointer does not point to a function type");
      return;
    }

    // ensure argument count matches function parameter count
    const auto &paramTypes = funcType->getParameters();
    if (args_.size() != paramTypes.size()) {
      ctx.reportError("Function pointer expects " + std::to_string(paramTypes.size()) + " argument(s) but " +
                      std::to_string(args_.size()) + " provided");
    }

    // update type_ to the function's return type
    type_ = funcType->getReturn();
  } else {
    // direct function call
    std::string funcName = funcRef_->getName();

    auto func = ctx.lookupFunction(funcName);
    if (!func) {
      ctx.reportError("Call to undefined function '" + funcName + "'");
      return;
    }

    // ensure argument count matches function parameter count
    const auto &params = func->getParams();
    size_t expectedArgCount = params.size();

    if (args_.size() != expectedArgCount) {
      ctx.reportError("Function '" + funcName + "' expects " + std::to_string(expectedArgCount) + " argument(s) but " +
                      std::to_string(args_.size()) + " provided");
    } else {
      // ensure argument types match parameter types
      for (size_t i = 0; i < args_.size(); ++i) {
        auto argType = args_[i]->getType();
        auto paramType = (params)[i].second;

        if (!argType) {
          ctx.reportError("Type of argument " + std::to_string(i + 1) + " in call to '" + funcName +
                          "' could not be determined");
        }
        if (!paramType) {
          ctx.reportError("Type of parameter '" + (params)[i].first + "' could not be determined");
        }

        if (argType && paramType) {
          // this is messy:

          auto argPrimType = std::dynamic_pointer_cast<PrimitiveTypeNode>(argType);
          auto paramPrimType = std::dynamic_pointer_cast<PrimitiveTypeNode>(paramType);

          if (argPrimType && paramPrimType) {
            if (argPrimType->isSigned() != paramPrimType->isSigned()) {
              ctx.reportError("Argument " + std::to_string(i + 1) + " in call to '" + funcName +
                              "' has incompatible signedness (parameter '" + (params)[i].first + "')");
            }
          }

          auto argPtrType = std::dynamic_pointer_cast<PointerTypeNode>(argType);
          auto paramPtrType = std::dynamic_pointer_cast<PointerTypeNode>(paramType);

          if ((argPtrType && !paramPtrType) || (!argPtrType && paramPtrType)) {
            ctx.reportError("Argument " + std::to_string(i + 1) + " type mismatch in call to '" + funcName +
                            "' (parameter '" + (params)[i].first + "')");
          }

          auto argArrType = std::dynamic_pointer_cast<ArrayTypeNode>(argType);
          auto paramArrType = std::dynamic_pointer_cast<ArrayTypeNode>(paramType);

          if ((argArrType && !paramArrType) || (!argArrType && paramArrType)) {
            ctx.reportError("Argument " + std::to_string(i + 1) + " type mismatch in call to '" + funcName +
                            "' (parameter '" + (params)[i].first + "')");
          }
        }
      }
    }

    // update type_ to the function's return type
    type_ = func->getReturnType();
    if (!type_) {
      ctx.reportError("Function '" + funcName + "' has no return type");
    }
  }
}

void BinaryOperation::analyze(AnalysisContext &ctx) {
  ctx.currentRow = row_;
  ctx.currentCol = col_;

  // left operand
  if (!L_) {
    ctx.reportError("Binary operation with null left operand");
    return;
  }
  L_->analyze(ctx);

  auto leftType = L_->getType();
  if (!leftType) {
    ctx.reportError("Cannot determine type of left operand in binary operation");
    return;
  }

  // right operand
  if (!R_) {
    ctx.reportError("Binary operation with null right operand");
    return;
  }
  R_->analyze(ctx);

  auto rightType = R_->getType();
  if (!rightType) {
    ctx.reportError("Cannot determine type of right operand in binary operation");
    return;
  }

  // ensure same signedness
  if (leftType->isSigned() != rightType->isSigned()) {
    ctx.reportError("Binary operation with operands of different signedness");
    return;
  }

  // for comparison operations the result type is bool
  if (opType_ == BinaryOperationType::Less || opType_ == BinaryOperationType::More ||
      opType_ == BinaryOperationType::Equal) {
    type_ = std::make_shared<PrimitiveTypeNode>(PrimitiveType::Bool, false);
  } else {
    // for arithmetic operations the result type is the same as operand's
    type_ = leftType;
  }

  if (!type_) {
    ctx.reportError("Cannot determine result type of binary operation");
  }
}

} // namespace axen::ast
