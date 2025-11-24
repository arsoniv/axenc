#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <llvm/IR/CFG.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/InstrTypes.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Value.h>

#include "nodes/context.hpp"
#include "nodes/statement.hpp"
#include "nodes/type.hpp"

namespace axen::ast {

class FunctionNode {
public:
  FunctionNode(std::string name, std::shared_ptr<FunctionTypeNode> type, std::vector<std::string> paramNames,
               std::optional<std::vector<std::unique_ptr<StatementNode>>> &&body, error::SourceSpan span)
      : name_(std::move(name)), type_(type), paramNames_(paramNames), body_(std::move(body)), span_(span) {}

  void analyze(AnalysisContext &ctx);
  void analyzeBody(AnalysisContext &ctx);

  llvm::Function *codeGen(CodegenContext &ctx);
  void generateFunctionBody(CodegenContext &ctx, llvm::Function *function);

  std::string getName() { return name_; }

  const std::shared_ptr<TypeNode> &getReturnType() { return type_->getReturn(); }

  // TODO: make this not inefficent
  std::vector<std::pair<std::string, std::shared_ptr<TypeNode>>> getParams() const {
    auto params = std::vector<std::pair<std::string, std::shared_ptr<TypeNode>>>();

    const auto &paramTypes = type_->getParameters();

    for (int i = 0; i < paramTypes.size(); i++) {
      params.emplace_back(paramNames_.at(i), paramTypes.at(i));
    }

    return params;
  }
  const std::optional<std::vector<std::unique_ptr<StatementNode>>> &getBody() const { return body_; }

  const error::SourceSpan &getSpan() const { return span_; }

private:
  std::string name_;
  std::shared_ptr<FunctionTypeNode> type_;
  std::vector<std::string> paramNames_;
  std::optional<std::vector<std::unique_ptr<StatementNode>>> body_;
  error::SourceSpan span_;
};

} // namespace axen::ast
