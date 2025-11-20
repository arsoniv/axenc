#include "nodes/function.hpp"
#include "nodes/context.hpp"
#include "nodes/statement.hpp"
#include "nodes/type.hpp"
#include <set>

namespace axen::ast {

void FunctionNode::analyze(AnalysisContext &ctx) {
  if (name_.empty()) {
    ctx.reportError("Function with empty name");
    return;
  }

  // check for duplicate function declaration
  auto existingFunc = ctx.lookupFunction(name_);
  if (existingFunc) {
    ctx.reportError("Function '" + name_ + "' already declared");
    return;
  }

  // set context
  auto previousFunction = ctx.currentFunction;
  ctx.currentFunction = this;

  // ensure valid return type
  if (!type_) {
    ctx.reportError("Function '" + name_ + "' has no return type");
    ctx.currentFunction = previousFunction;
    return;
  }
  type_->analyze(ctx);

  const auto &params = getParams();

  // analyze parameter types
  std::set<std::string> paramNames;
  for (const auto &param : params) {
    if (param.first.empty()) {
      ctx.reportError("Function '" + name_ + "' has parameter with empty name");
      continue;
    }

    // check for duplicate parameters
    if (paramNames.count(param.first)) {
      ctx.reportError("Function '" + name_ + "' has duplicate parameter '" + param.first + "'");
      continue;
    }
    paramNames.insert(param.first);

    if (!param.second) {
      ctx.reportError("Parameter '" + param.first + "' in function '" + name_ + "' has no type");
      continue;
    }

    param.second->analyze(ctx);

    // add parameter symbols to index (for lsp)
    ctx.allSymbols.emplace_back(param.first, SymbolInfo::Kind::Parameter, param.second, 0, ctx.currentFile, 0, 0);
  }

  ctx.registerFunction(name_, this);

  if (body_)
    analyzeBody(ctx);

  // restore function context
  ctx.currentFunction = previousFunction;
}

void FunctionNode::analyzeBody(AnalysisContext &ctx) {
  if (!body_) {
    ctx.reportError("Function '" + name_ + "' body is null");
    return;
  }

  ctx.pushScope();

  // add params to new scope
  const auto &params = getParams();
  for (auto &param : params) {
    if (ctx.existsInCurrentScope(param.first)) {
      ctx.reportError("Parameter '" + param.first + "' already declared in function scope");
      continue;
    }
    ctx.declareVariable(param.first, param.second);
  }

  // for return analysis
  bool hasReturnStatement = false;

  // analyze each statement
  for (auto &stmt : *body_) {
    if (!stmt) {
      ctx.reportError("Null statement in function '" + name_ + "' body");
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
        ctx.reportError("Non-void function '" + name_ + "' missing return statement");
      }
    }
  }

  ctx.popScope();
}

} // namespace axen::ast
