#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <llvm/IR/Type.h>
#include <llvm/IR/Value.h>

#include "nodes/context.hpp"
#include "nodes/expression.hpp"
#include "nodes/type.hpp"

namespace axen::ast {
class StatementNode {
public:
  StatementNode(error::SourceSpan span) : span_(span) {}
  virtual ~StatementNode() = default;
  virtual void analyze(AnalysisContext &ctx) = 0;
  virtual void codeGen(CodegenContext &ctx) = 0;

  const error::SourceSpan &getSpan() const { return span_; }

protected:
  error::SourceSpan span_;
};

class VariableDeclaration : public StatementNode {
public:
  VariableDeclaration(std::shared_ptr<TypeNode> &type, std::string name, std::unique_ptr<ExpressionNode> &&initialValue,
                      error::SourceSpan span)
      : type_(type), name_(std::move(name)), initialValue_(std::move(initialValue)), StatementNode(span) {};
  void analyze(AnalysisContext &ctx) override;
  void codeGen(CodegenContext &ctx) override;

  const std::string &getName() const { return name_; }
  std::shared_ptr<TypeNode> getType() const { return type_; }

private:
  std::shared_ptr<TypeNode> type_;
  std::string name_;
  std::unique_ptr<ExpressionNode> initialValue_;
};

class AssignmentStatement : public StatementNode {
public:
  AssignmentStatement(std::unique_ptr<ExpressionNode> &&target, std::unique_ptr<ExpressionNode> &&value,
                      error::SourceSpan span)
      : target_(std::move(target)), value_(std::move(value)), StatementNode(span) {}

  void analyze(AnalysisContext &ctx) override;
  void codeGen(CodegenContext &ctx) override;

private:
  std::unique_ptr<ExpressionNode> target_;
  std::unique_ptr<ExpressionNode> value_;
};

class Return : public StatementNode {
public:
  Return(std::unique_ptr<ExpressionNode> &&value, error::SourceSpan span)
      : value_(std::move(value)), StatementNode(span) {}
  void analyze(AnalysisContext &ctx) override;
  void codeGen(CodegenContext &ctx) override;

private:
  std::unique_ptr<ExpressionNode> value_;
};

class Break : public StatementNode {
public:
  Break(error::SourceSpan span) : StatementNode(span) {}
  void analyze(AnalysisContext &ctx) override;
  void codeGen(CodegenContext &ctx) override;
};

class Continue : public StatementNode {
public:
  Continue(error::SourceSpan span) : StatementNode(span) {}
  void analyze(AnalysisContext &ctx) override;
  void codeGen(CodegenContext &ctx) override;
};

class If : public StatementNode {
public:
  If(std::unique_ptr<ExpressionNode> &&condition, std::vector<std::unique_ptr<StatementNode>> &&trueBody,
     std::optional<std::vector<std::unique_ptr<StatementNode>>> &&falseBody, error::SourceSpan span)
      : condition_(std::move(condition)), trueBody_(std::move(trueBody)), falseBody_(std::move(falseBody)),
        StatementNode(span) {}
  void analyze(AnalysisContext &ctx) override;
  void codeGen(CodegenContext &ctx) override;

private:
  std::unique_ptr<ExpressionNode> condition_;
  std::vector<std::unique_ptr<StatementNode>> trueBody_;
  std::optional<std::vector<std::unique_ptr<StatementNode>>> falseBody_;
};

class While : public StatementNode {
public:
  While(std::unique_ptr<ExpressionNode> &&condition, std::vector<std::unique_ptr<StatementNode>> &&body,
        error::SourceSpan span)
      : condition_(std::move(condition)), body_(std::move(body)), StatementNode(span) {}
  void analyze(AnalysisContext &ctx) override;
  void codeGen(CodegenContext &ctx) override;

private:
  std::unique_ptr<ExpressionNode> condition_;
  std::vector<std::unique_ptr<StatementNode>> body_;
};

class ExpressionStatement : public StatementNode {
public:
  ExpressionStatement(std::unique_ptr<ExpressionNode> &&expression, error::SourceSpan span)
      : expression_(std::move(expression)), StatementNode(span) {}
  void analyze(AnalysisContext &ctx) override;
  void codeGen(CodegenContext &ctx) override;

private:
  std::unique_ptr<ExpressionNode> expression_;
};

} // namespace axen::ast
