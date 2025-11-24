#pragma once

#include <memory>
#include <string>
#include <utility>

#include <llvm/IR/Value.h>

#include "context.hpp"
#include "nodes/type.hpp"

namespace axen::ast {

class ExpressionNode {
public:
  ExpressionNode(std::shared_ptr<TypeNode> type, error::SourceSpan span) : type_(type), span_(span) {}
  virtual ~ExpressionNode() = default;
  virtual void analyze(AnalysisContext &ctx) = 0;
  virtual llvm::Value *codeGen(CodegenContext &ctx) = 0;
  virtual llvm::Value *codeGenLValue(CodegenContext &ctx) {
    throw std::runtime_error("Lvalue codegen not supported on this expression.");
  }
  virtual std::shared_ptr<TypeNode> getType() { return type_; }

  const error::SourceSpan &getSpan() const { return span_; }

public:
  std::shared_ptr<TypeNode> type_;
  error::SourceSpan span_;
};

class VariableReference : public ExpressionNode {
public:
  VariableReference(std::string name, std::shared_ptr<TypeNode> type, error::SourceSpan span)
      : name_(name), ExpressionNode(type, span) {}

  void analyze(AnalysisContext &ctx) override;
  llvm::Value *codeGen(CodegenContext &ctx) override;
  llvm::Value *codeGenLValue(CodegenContext &ctx) override;

private:
  std::string name_;
};

class StructAccess : public ExpressionNode {
public:
  StructAccess(std::unique_ptr<ExpressionNode> &&structExpr, std::string memberName, std::string structName,
               std::shared_ptr<ClassReferenceNode> structType, std::shared_ptr<TypeNode> memberType,
               error::SourceSpan span)
      : structExpr_(std::move(structExpr)), memberName_(std::move(memberName)), structName_(std::move(structName)),
        structType_(structType), ExpressionNode(memberType, span) {}

  void analyze(AnalysisContext &ctx) override;
  llvm::Value *codeGen(CodegenContext &ctx) override;
  llvm::Value *codeGenLValue(CodegenContext &ctx) override;

private:
  std::unique_ptr<ExpressionNode> structExpr_;
  std::string memberName_;
  std::string structName_;
  std::shared_ptr<ClassReferenceNode> structType_;
};

class ArrayAccess : public ExpressionNode {
public:
  ArrayAccess(std::unique_ptr<ExpressionNode> &&arrayExpr, std::unique_ptr<ExpressionNode> &&indexExpr,
              std::shared_ptr<ArrayTypeNode> type, error::SourceSpan span)
      : arrayExpr_(std::move(arrayExpr)), indexExpr_(std::move(indexExpr)), arrayType_(type),
        ExpressionNode(type, span) {}

  void analyze(AnalysisContext &ctx) override;
  llvm::Value *codeGen(CodegenContext &ctx) override;
  llvm::Value *codeGenLValue(CodegenContext &ctx) override;

private:
  std::unique_ptr<ExpressionNode> arrayExpr_;
  std::unique_ptr<ExpressionNode> indexExpr_;
  std::shared_ptr<ArrayTypeNode> arrayType_;
};

class PtrIndexAccess : public ExpressionNode {
public:
  PtrIndexAccess(std::unique_ptr<ExpressionNode> &&ptrExpr, std::unique_ptr<ExpressionNode> &&indexExpr,
                 std::shared_ptr<PointerTypeNode> type, error::SourceSpan span)
      : ptrExpr_(std::move(ptrExpr)), indexExpr_(std::move(indexExpr)), ptrType_(type), ExpressionNode(type, span) {}

  void analyze(AnalysisContext &ctx) override;
  llvm::Value *codeGen(CodegenContext &ctx) override;
  llvm::Value *codeGenLValue(CodegenContext &ctx) override;

private:
  std::unique_ptr<ExpressionNode> ptrExpr_;
  std::unique_ptr<ExpressionNode> indexExpr_;
  std::shared_ptr<PointerTypeNode> ptrType_;
};

class Dref : public ExpressionNode {
public:
  Dref(std::unique_ptr<ExpressionNode> &&target, std::shared_ptr<TypeNode> derivedType, error::SourceSpan span)
      : target_(std::move(target)), derivedType_(derivedType),
        ExpressionNode(std::make_shared<PointerTypeNode>(derivedType, span), span) {}

  void analyze(AnalysisContext &ctx) override;
  llvm::Value *codeGen(CodegenContext &ctx) override;
  llvm::Value *codeGenLValue(CodegenContext &ctx) override;

private:
  std::unique_ptr<ExpressionNode> target_;
  std::shared_ptr<TypeNode> derivedType_;
};

class AddressOf : public ExpressionNode {
public:
  AddressOf(std::unique_ptr<ExpressionNode> &&target, std::shared_ptr<TypeNode> type, error::SourceSpan span)
      : target_(std::move(target)), ExpressionNode(type, span) {}

  void analyze(AnalysisContext &ctx) override;
  llvm::Value *codeGen(CodegenContext &ctx) override;

private:
  std::unique_ptr<ExpressionNode> target_;
};

class IntLiteral : public ExpressionNode {
public:
  IntLiteral(int value, bool isSigned, error::SourceSpan span)
      : value_(value), ExpressionNode(std::make_shared<PrimitiveTypeNode>(PrimitiveType::Int, isSigned, span), span) {}

  void analyze(AnalysisContext &ctx) override;
  llvm::Value *codeGen(CodegenContext &ctx) override;

private:
  int value_;
};

class FloatLiteral : public ExpressionNode {
public:
  FloatLiteral(float value, error::SourceSpan span)
      : value_(value), ExpressionNode(std::make_shared<PrimitiveTypeNode>(PrimitiveType::Float, true, span), span) {}

  void analyze(AnalysisContext &ctx) override;
  llvm::Value *codeGen(CodegenContext &ctx) override;

private:
  float value_;
};

class StringLiteral : public ExpressionNode {
public:
  StringLiteral(std::string value, error::SourceSpan span)
      : value_(value),
        ExpressionNode(
            std::make_shared<PointerTypeNode>(std::make_shared<PrimitiveTypeNode>(PrimitiveType::Char, false, span), span), span) {}

  void analyze(AnalysisContext &ctx) override;
  llvm::Value *codeGen(CodegenContext &ctx) override;

private:
  std::string value_;
};

class NullptrLiteral : public ExpressionNode {
public:
  NullptrLiteral(error::SourceSpan span)
      : ExpressionNode(
            std::make_shared<PointerTypeNode>(std::make_shared<PrimitiveTypeNode>(PrimitiveType::Void, true, span), span), span) {}

  void analyze(AnalysisContext &ctx) override;
  llvm::Value *codeGen(CodegenContext &ctx) override;
};

class FunctionReference : public ExpressionNode {
public:
  FunctionReference(std::string name, std::shared_ptr<TypeNode> type, error::SourceSpan span)
      : name_(std::move(name)), isVariable_(false), ExpressionNode(type, span) {}

  // for function pointers
  FunctionReference(std::unique_ptr<ExpressionNode> &&varExpr, std::shared_ptr<TypeNode> type, error::SourceSpan span)
      : varExpr_(std::move(varExpr)), isVariable_(true), ExpressionNode(type, span) {}

  void analyze(AnalysisContext &ctx) override;
  llvm::Value *codeGen(CodegenContext &ctx) override;

  const std::string &getName() const { return name_; }
  bool isVariable() const { return isVariable_; }

private:
  std::string name_;
  std::unique_ptr<ExpressionNode> varExpr_;
  bool isVariable_;
};

class FunctionCall : public ExpressionNode {
public:
  FunctionCall(std::unique_ptr<FunctionReference> &&funcRef, std::vector<std::unique_ptr<ExpressionNode>> &&args,
               std::shared_ptr<TypeNode> type, error::SourceSpan span)
      : funcRef_(std::move(funcRef)), args_(std::move(args)), ExpressionNode(type, span) {}

  void analyze(AnalysisContext &ctx) override;
  llvm::Value *codeGen(CodegenContext &ctx) override;

private:
  std::unique_ptr<FunctionReference> funcRef_;
  std::vector<std::unique_ptr<ExpressionNode>> args_;
};

enum class BinaryOperationType {
  And,
  Add,
  Subtract,
  Multiply,
  Divide,
  Modulo,
  Less,
  More,
  Equal,
};

class BinaryOperation : public ExpressionNode {
public:
  BinaryOperation(BinaryOperationType opType, std::unique_ptr<ExpressionNode> &&L, std::unique_ptr<ExpressionNode> &&R,
                  std::shared_ptr<TypeNode> type, error::SourceSpan span)
      : opType_(opType), L_(std::move(L)), R_(std::move(R)), ExpressionNode(type, span) {}

  void analyze(AnalysisContext &ctx) override;
  llvm::Value *codeGen(CodegenContext &ctx) override;

private:
  BinaryOperationType opType_;
  std::unique_ptr<ExpressionNode> L_;
  std::unique_ptr<ExpressionNode> R_;
};

} // namespace axen::ast
