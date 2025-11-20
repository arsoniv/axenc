#include "nodes/statement.hpp"
#include "nodes/context.hpp"
#include "nodes/expression.hpp"
#include "nodes/function.hpp"

namespace axen::ast {

void VariableDeclaration::analyze(AnalysisContext &ctx) {
  ctx.currentRow = row_;
  ctx.currentCol = col_;

  if (name_.empty()) {
    ctx.reportError("Variable declaration with empty name");
    return;
  }

  // ensure variable does not already exist in current scope
  if (ctx.existsInCurrentScope(name_)) {
    ctx.reportError("Variable '" + name_ + "' already declared in this scope");
    return;
  }

  if (!type_) {
    ctx.reportError("Variable '" + name_ + "' has no type");
    return;
  }
  type_->analyze(ctx);

  if (initialValue_) {
    initialValue_->analyze(ctx);

    auto initType = initialValue_->getType();
    if (!initType) {
      ctx.reportError("Cannot determine type of initializer for variable '" + name_ + "'");
    }
  }

  ctx.declareVariable(name_, type_);
}

void AssignmentStatement::analyze(AnalysisContext &ctx) {
  ctx.currentRow = row_;
  ctx.currentCol = col_;

  if (!target_) {
    ctx.reportError("Assignment with null target");
    return;
  }
  target_->analyze(ctx);

  auto targetType = target_->getType();
  if (!targetType) {
    ctx.reportError("Cannot determine type of assignment target");
    return;
  }

  if (!value_) {
    ctx.reportError("Assignment with null value");
    return;
  }
  value_->analyze(ctx);

  auto valueType = value_->getType();
  if (!valueType) {
    ctx.reportError("Cannot determine type of assignment value");
    return;
  }
}

void Return::analyze(AnalysisContext &ctx) {
  ctx.currentRow = row_;
  ctx.currentCol = col_;

  // ensure return statement is in a function
  if (!ctx.currentFunction) {
    ctx.reportError("Return statement outside of function");
    return;
  }

  auto expectedReturnType = ctx.currentFunction->getReturnType();
  if (!expectedReturnType) {
    ctx.reportError("Current function has no return type");
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
      ctx.reportError("Cannot determine type of return value");
      return;
    }

    if (isVoidReturn) {
      ctx.reportError("Cannot return a value from void function");
      return;
    }

  } else {
    // should be a void function return
    if (!isVoidReturn) {
      ctx.reportError("Non-void function must return a value");
    }
  }
}

void If::analyze(AnalysisContext &ctx) {
  ctx.currentRow = row_;
  ctx.currentCol = col_;

  if (!condition_) {
    ctx.reportError("If statement with null condition");
    return;
  }
  condition_->analyze(ctx);

  auto condType = condition_->getType();
  if (!condType) {
    ctx.reportError("Cannot determine type of if condition");
    return;
  }

  // ensure condition is a primitive type
  auto condPrimType = std::dynamic_pointer_cast<PrimitiveTypeNode>(condType);
  if (!condPrimType) {
    ctx.reportError("If condition must be an integer or boolean type");
    return;
  }

  // analyze true body (with new scope)
  ctx.pushScope();
  for (auto &stmt : trueBody_) {
    if (!stmt) {
      ctx.reportError("Null statement in if true body");
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
        ctx.reportError("Null statement in if false body");
        continue;
      }
      stmt->analyze(ctx);
    }
    ctx.popScope();
  }
}

void While::analyze(AnalysisContext &ctx) {
  ctx.currentRow = row_;
  ctx.currentCol = col_;

  if (!condition_) {
    ctx.reportError("While statement with null condition");
    return;
  }
  condition_->analyze(ctx);

  auto condType = condition_->getType();
  if (!condType) {
    ctx.reportError("Cannot determine type of while condition");
    return;
  }

  // ensure condition is a primitive type
  auto condPrimType = std::dynamic_pointer_cast<PrimitiveTypeNode>(condType);
  if (!condPrimType) {
    ctx.reportError("While condition must be an integer or boolean type");
    return;
  }

  // analyze body (with new scope)
  ctx.pushScope();
  for (auto &stmt : body_) {
    if (!stmt) {
      ctx.reportError("Null statement in while body");
      continue;
    }
    stmt->analyze(ctx);
  }
  ctx.popScope();
}

void ExpressionStatement::analyze(AnalysisContext &ctx) {
  ctx.currentRow = row_;
  ctx.currentCol = col_;

  if (!expression_) {
    ctx.reportError("Expression statement with null expression");
    return;
  }
  expression_->analyze(ctx);

  if (!expression_->getType()) {
    ctx.reportError("Expression has no type");
  }
}

} // namespace axen::ast
