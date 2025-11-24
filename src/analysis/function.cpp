#include "nodes/function.hpp"
#include "nodes/context.hpp"
#include "nodes/statement.hpp"
#include "nodes/type.hpp"
#include <set>

namespace axen::ast {

void FunctionNode::analyze(AnalysisContext &ctx) {
  if (name_.empty()) {
    ctx.emitAnalysisError("Function with empty name", span_);
    return;
  }

  // check for duplicate function declaration
  auto existingFunc = ctx.lookupFunction(name_);
  if (existingFunc) {
    ctx.emitAnalysisError("Function '" + name_ + "' already declared", span_);
    return;
  }

  // set context
  auto previousFunction = ctx.currentFunction;
  ctx.currentFunction = this;

  // ensure valid return type
  if (!type_) {
    ctx.emitAnalysisError("Function '" + name_ + "' has no return type", span_);
    ctx.currentFunction = previousFunction;
    return;
  }
  type_->analyze(ctx);

  const auto &params = getParams();

  // analyze parameter types
  std::set<std::string> paramNames;
  for (const auto &param : params) {
    if (param.first.empty()) {
      ctx.emitAnalysisError("Function '" + name_ + "' has parameter with empty name", span_);
      continue;
    }

    // check for duplicate parameters
    if (paramNames.count(param.first)) {
      ctx.emitAnalysisError("Function '" + name_ + "' has duplicate parameter '" + param.first + "'", span_);
      continue;
    }
    paramNames.insert(param.first);

    if (!param.second) {
      ctx.emitAnalysisError("Parameter '" + param.first + "' in function '" + name_ + "' has no type", span_);
      continue;
    }

    param.second->analyze(ctx);
  }

  ctx.registerFunction(name_, this);

  if (body_)
    analyzeBody(ctx);

  // restore function context
  ctx.currentFunction = previousFunction;
}

void FunctionNode::analyzeBody(AnalysisContext &ctx) {
  if (!body_) {
    ctx.emitAnalysisError("Function '" + name_ + "' body is null", span_);
    return;
  }

  ctx.pushScope();

  // add params to new scope
  const auto &params = getParams();
  for (auto &param : params) {
    if (ctx.existsInCurrentScope(param.first)) {
      ctx.emitAnalysisError("Parameter '" + param.first + "' already declared in function scope", span_);
      continue;
    }
    ctx.declareVariable(param.first, param.second);
  }

  // for return analysis
  bool hasReturnStatement = false;

  // analyze each statement
  for (auto &stmt : *body_) {
    if (!stmt) {
      ctx.emitAnalysisError("Null statement in function '" + name_ + "' body", span_);
      continue;
    }

    stmt->analyze(ctx);

    // check for return statement
    if (dynamic_cast<Return *>(stmt.get())) {
      hasReturnStatement = true;
    }
  }

  // validate that non-void functions have at least one return statement
  // TODO: ensure all control flow routes have return statements
  if (type_) {
    auto primitiveType = std::dynamic_pointer_cast<PrimitiveTypeNode>(type_);
    if (primitiveType) {
      bool isVoid = primitiveType->getType() == PrimitiveType::Void;

      if (!isVoid && !hasReturnStatement) {
        ctx.emitAnalysisError("Non-void function '" + name_ + "' missing return statement", span_);
      }
    }
  }

  ctx.popScope();
}

} // namespace axen::ast
