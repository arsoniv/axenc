#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <llvm/IR/Type.h>
#include <llvm/IR/Value.h>

#include "nodes/context.hpp"
#include "nodes/expression.hpp"
#include "nodes/types.hpp"

namespace axen::ast {
class StatementNode {
public:
  virtual ~StatementNode() = default;
  virtual void codeGen(CodegenContext &ctx) = 0;
};

class VariableDeclaration : public StatementNode {
public:
  VariableDeclaration(std::shared_ptr<TypeNode> &type, std::string name, std::unique_ptr<ExpressionNode> &&initialValue)
      : type_(type), name_(std::move(name)), initialValue_(std::move(initialValue)) {};
  void codeGen(CodegenContext &ctx) override;

private:
  std::shared_ptr<TypeNode> type_;
  std::string name_;
  std::unique_ptr<ExpressionNode> initialValue_;
};

class AssignmentStatement : public StatementNode {
public:
  AssignmentStatement(std::unique_ptr<ExpressionNode> &&target, std::unique_ptr<ExpressionNode> &&value)
      : target_(std::move(target)), value_(std::move(value)) {}

  void codeGen(CodegenContext &ctx) override;

private:
  std::unique_ptr<ExpressionNode> target_;
  std::unique_ptr<ExpressionNode> value_;
};

class Return : public StatementNode {
public:
  Return(std::unique_ptr<ExpressionNode> &&value) : value_(std::move(value)) {}
  void codeGen(CodegenContext &ctx) override;

private:
  std::unique_ptr<ExpressionNode> value_;
};

class If : public StatementNode {
public:
  If(std::unique_ptr<ExpressionNode> &&condition, std::vector<std::unique_ptr<StatementNode>> &&trueBody,
     std::optional<std::vector<std::unique_ptr<StatementNode>>> &&falseBody)
      : condition_(std::move(condition)), trueBody_(std::move(trueBody)), falseBody_(std::move(falseBody)) {}
  void codeGen(CodegenContext &ctx) override;

private:
  std::unique_ptr<ExpressionNode> condition_;
  std::vector<std::unique_ptr<StatementNode>> trueBody_;
  std::optional<std::vector<std::unique_ptr<StatementNode>>> falseBody_;
};

class While : public StatementNode {
public:
  While(std::unique_ptr<ExpressionNode> &&condition, std::vector<std::unique_ptr<StatementNode>> &&body)
      : condition_(std::move(condition)), body_(std::move(body)) {}
  void codeGen(CodegenContext &ctx) override;

private:
  std::unique_ptr<ExpressionNode> condition_;
  std::vector<std::unique_ptr<StatementNode>> body_;
};

class ExpressionStatement : public StatementNode {
public:
  ExpressionStatement(std::unique_ptr<ExpressionNode> &&expression) : expression_(std::move(expression)) {}
  void codeGen(CodegenContext &ctx) override;

private:
  std::unique_ptr<ExpressionNode> expression_;
};

} // namespace axen::ast
