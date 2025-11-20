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
  ExpressionNode(std::shared_ptr<TypeNode> type) : type_(type) {}
  virtual ~ExpressionNode() = default;
  virtual void analyze(AnalysisContext &ctx) = 0;
  virtual llvm::Value *codeGen(CodegenContext &ctx) = 0;
  virtual llvm::Value *codeGenLValue(CodegenContext &ctx) {
    throw std::runtime_error("Lvalue codegen not supported on this expression.");
  }
  virtual std::shared_ptr<TypeNode> getType() { return type_; }

  void setLocation(int row, int col) {
    row_ = row;
    col_ = col;
  }
  int getRow() const { return row_; }
  int getCol() const { return col_; }

public:
  std::shared_ptr<TypeNode> type_;
  int row_ = 0;
  int col_ = 0;
};

class VariableReference : public ExpressionNode {
public:
  VariableReference(std::string name, std::shared_ptr<TypeNode> type) : name_(name), ExpressionNode(type) {}

  void analyze(AnalysisContext &ctx) override;
  llvm::Value *codeGen(CodegenContext &ctx) override;
  llvm::Value *codeGenLValue(CodegenContext &ctx) override;

private:
  std::string name_;
};

class StructAccess : public ExpressionNode {
public:
  StructAccess(std::unique_ptr<ExpressionNode> &&structExpr, std::string memberName, std::string structName,
               std::shared_ptr<ClassReferenceNode> structType, std::shared_ptr<TypeNode> memberType)
      : structExpr_(std::move(structExpr)), memberName_(std::move(memberName)), structName_(std::move(structName)),
        structType_(structType), ExpressionNode(memberType) {}

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
              std::shared_ptr<ArrayTypeNode> type)
      : arrayExpr_(std::move(arrayExpr)), indexExpr_(std::move(indexExpr)), arrayType_(type), ExpressionNode(type) {}

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
                 std::shared_ptr<PointerTypeNode> type)
      : ptrExpr_(std::move(ptrExpr)), indexExpr_(std::move(indexExpr)), ptrType_(type), ExpressionNode(type) {}

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
  Dref(std::unique_ptr<ExpressionNode> &&target, std::shared_ptr<TypeNode> derivedType)
      : target_(std::move(target)), derivedType_(derivedType),
        ExpressionNode(std::make_shared<PointerTypeNode>(derivedType)) {}

  void analyze(AnalysisContext &ctx) override;
  llvm::Value *codeGen(CodegenContext &ctx) override;
  llvm::Value *codeGenLValue(CodegenContext &ctx) override;

private:
  std::unique_ptr<ExpressionNode> target_;
  std::shared_ptr<TypeNode> derivedType_;
};

class AddressOf : public ExpressionNode {
public:
  AddressOf(std::unique_ptr<ExpressionNode> &&target, std::shared_ptr<TypeNode> type)
      : target_(std::move(target)), ExpressionNode(type) {}

  void analyze(AnalysisContext &ctx) override;
  llvm::Value *codeGen(CodegenContext &ctx) override;

private:
  std::unique_ptr<ExpressionNode> target_;
};

class IntLiteral : public ExpressionNode {
public:
  IntLiteral(int value, bool isSigned = true)
      : value_(value), ExpressionNode(std::make_shared<PrimitiveTypeNode>(PrimitiveType::Int, isSigned)) {}

  void analyze(AnalysisContext &ctx) override;
  llvm::Value *codeGen(CodegenContext &ctx) override;

private:
  int value_;
};

class FloatLiteral : public ExpressionNode {
public:
  FloatLiteral(float value)
      : value_(value), ExpressionNode(std::make_shared<PrimitiveTypeNode>(PrimitiveType::Float, true)) {}

  void analyze(AnalysisContext &ctx) override;
  llvm::Value *codeGen(CodegenContext &ctx) override;

private:
  float value_;
};

class StringLiteral : public ExpressionNode {
public:
  StringLiteral(std::string value)
      : value_(value), ExpressionNode(std::make_shared<PointerTypeNode>(
                           std::make_shared<PrimitiveTypeNode>(PrimitiveType::Char, false))) {}

  void analyze(AnalysisContext &ctx) override;
  llvm::Value *codeGen(CodegenContext &ctx) override;

private:
  std::string value_;
};

class NullptrLiteral : public ExpressionNode {
public:
  NullptrLiteral()
      : ExpressionNode(
            std::make_shared<PointerTypeNode>(std::make_shared<PrimitiveTypeNode>(PrimitiveType::Void, true))) {}

  void analyze(AnalysisContext &ctx) override;
  llvm::Value *codeGen(CodegenContext &ctx) override;
};

class FunctionReference : public ExpressionNode {
public:
  FunctionReference(std::string name, std::shared_ptr<TypeNode> type)
      : name_(std::move(name)), isVariable_(false), ExpressionNode(type) {}

  // for function pointers
  FunctionReference(std::unique_ptr<ExpressionNode> &&varExpr, std::shared_ptr<TypeNode> type)
      : varExpr_(std::move(varExpr)), isVariable_(true), ExpressionNode(type) {}

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
               std::shared_ptr<TypeNode> type)
      : funcRef_(std::move(funcRef)), args_(std::move(args)), ExpressionNode(type) {}

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
                  std::shared_ptr<TypeNode> type)
      : opType_(opType), L_(std::move(L)), R_(std::move(R)), ExpressionNode(type) {}

  void analyze(AnalysisContext &ctx) override;
  llvm::Value *codeGen(CodegenContext &ctx) override;

private:
  BinaryOperationType opType_;
  std::unique_ptr<ExpressionNode> L_;
  std::unique_ptr<ExpressionNode> R_;
};

} // namespace axen::ast
