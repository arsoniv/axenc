#include "nodes/statement.hpp"
#include "nodes/context.hpp"
#include "nodes/expression.hpp"
#include "nodes/function.hpp"

namespace axen::ast {

void VariableDeclaration::analyze(AnalysisContext &ctx) {
  ctx.currentRow = span_.startRow;
  ctx.currentCol = span_.startCol;

  if (name_.empty()) {
    ctx.emitAnalysisError("Variable declaration with empty name", span_);
    return;
  }

  // ensure variable does not already exist in current scope
  if (ctx.existsInCurrentScope(name_)) {
    ctx.emitAnalysisError("Variable '" + name_ + "' already declared in this scope", span_);
    return;
  }

  if (!type_) {
    ctx.emitAnalysisError("Variable '" + name_ + "' has no type", span_);
    return;
  }
  type_->analyze(ctx);

  if (initialValue_) {
    initialValue_->analyze(ctx);

    auto initType = initialValue_->getType();
    if (!initType) {
      ctx.emitAnalysisError("Cannot determine type of initializer for variable '" + name_ + "'", span_);
    }
  }

  ctx.declareVariable(name_, type_);
}

void AssignmentStatement::analyze(AnalysisContext &ctx) {
  ctx.currentRow = span_.startRow;
  ctx.currentCol = span_.startCol;

  if (!target_) {
    ctx.emitAnalysisError("Assignment with null target", span_);
    return;
  }
  target_->analyze(ctx);

  auto targetType = target_->getType();
  if (!targetType) {
    ctx.emitAnalysisError("Cannot determine type of assignment target", span_);
    return;
  }

  if (!value_) {
    ctx.emitAnalysisError("Assignment with null value", span_);
    return;
  }
  value_->analyze(ctx);

  auto valueType = value_->getType();
  if (!valueType) {
    ctx.emitAnalysisError("Cannot determine type of assignment value", span_);
    return;
  }
}

void Return::analyze(AnalysisContext &ctx) {
  ctx.currentRow = span_.startRow;
  ctx.currentCol = span_.startCol;

  // ensure return statement is in a function
  if (!ctx.currentFunction) {
    ctx.emitAnalysisError("Return statement outside of function", span_);
    return;
  }

  auto expectedReturnType = ctx.currentFunction->getReturnType();
  if (!expectedReturnType) {
    ctx.emitAnalysisError("Current function has no return type", span_);
    return;
  }

  // check for void return
  auto primitiveType = std::dynamic_pointer_cast<PrimitiveTypeNode>(expectedReturnType);
  bool isVoidReturn = primitiveType && primitiveType->isSigned() == false;

  // analyze the return value if present
  if (value_) {
    value_->analyze(ctx);

    auto returnType = value_->getType();
    if (!returnType) {
      ctx.emitAnalysisError("Cannot determine type of return value", span_);
      return;
    }

    if (isVoidReturn) {
      ctx.emitAnalysisError("Cannot return a value from void function", span_);
      return;
    }

  } else {
    // should be a void function return
    if (!isVoidReturn) {
      ctx.emitAnalysisError("Non-void function must return a value", span_);
    }
  }
}

void If::analyze(AnalysisContext &ctx) {
  ctx.currentRow = span_.startRow;
  ctx.currentCol = span_.startCol;

  if (!condition_) {
    ctx.emitAnalysisError("If statement with null condition", span_);
    return;
  }
  condition_->analyze(ctx);

  auto condType = condition_->getType();
  if (!condType) {
    ctx.emitAnalysisError("Cannot determine type of if condition", span_);
    return;
  }

  // ensure condition is a primitive type
  auto condPrimType = std::dynamic_pointer_cast<PrimitiveTypeNode>(condType);
  if (!condPrimType) {
    ctx.emitAnalysisError("If condition must be an integer or boolean type", span_);
    return;
  }

  // analyze true body (with new scope)
  ctx.pushScope();
  for (auto &stmt : trueBody_) {
    if (!stmt) {
      ctx.emitAnalysisError("Null statement in if true body", span_);
      continue;
    }
    stmt->analyze(ctx);
  }
  ctx.popScope();

  // analyze false body (if present, with new scope)
  if (falseBody_) {
    ctx.pushScope();
    for (auto &stmt : *falseBody_) {
      if (!stmt) {
        ctx.emitAnalysisError("Null statement in if false body", span_);
        continue;
      }
      stmt->analyze(ctx);
    }
    ctx.popScope();
  }
}

void While::analyze(AnalysisContext &ctx) {
  ctx.currentRow = span_.startRow;
  ctx.currentCol = span_.startCol;

  if (!condition_) {
    ctx.emitAnalysisError("While statement with null condition", span_);
    return;
  }
  condition_->analyze(ctx);

  auto condType = condition_->getType();
  if (!condType) {
    ctx.emitAnalysisError("Cannot determine type of while condition", span_);
    return;
  }

  // ensure condition is a primitive type
  auto condPrimType = std::dynamic_pointer_cast<PrimitiveTypeNode>(condType);
  if (!condPrimType) {
    ctx.emitAnalysisError("While condition must be an integer or boolean type", span_);
    return;
  }

  // analyze body (with new scope)
  ctx.pushScope();
  for (auto &stmt : body_) {
    if (!stmt) {
      ctx.emitAnalysisError("Null statement in while body", span_);
      continue;
    }
    stmt->analyze(ctx);
  }
  ctx.popScope();
}

void ExpressionStatement::analyze(AnalysisContext &ctx) {
  ctx.currentRow = span_.startRow;
  ctx.currentCol = span_.startCol;

  if (!expression_) {
    ctx.emitAnalysisError("Expression statement with null expression", span_);
    return;
  }
  expression_->analyze(ctx);

  if (!expression_->getType()) {
    ctx.emitAnalysisError("Expression has no type", span_);
  }
}

} // namespace axen::ast
