#pragma once

#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <utility>

#include <llvm/IR/Value.h>

#include "context.hpp"
#include "nodes/types.hpp"

namespace axen::ast {

class ExpressionNode {
public:
  ExpressionNode(bool isSigned) : isSigned_(isSigned) {}
  virtual ~ExpressionNode() = default;
  virtual llvm::Value *codeGen(CodegenContext &ctx) = 0;
  virtual llvm::Value *codeGenLValue(CodegenContext &ctx) {
    fprintf(stderr, "Lvalue codegen not supported on this expression.");
    exit(EXIT_FAILURE);
  }
  virtual bool isSigned() { return isSigned_; }

protected:
  bool isSigned_;
};

class VariableReference : public ExpressionNode {
public:
  VariableReference(std::string name, bool isSigned) : name_(name), ExpressionNode(isSigned) {}

  llvm::Value *codeGen(CodegenContext &ctx) override;
  llvm::Value *codeGenLValue(CodegenContext &ctx) override;

private:
  std::string name_;
};

class StructAccess : public ExpressionNode {
public:
  StructAccess(std::unique_ptr<ExpressionNode> &&structExpr, std::string memberName, std::string structName,
               bool isSigned, std::shared_ptr<ClassReferenceNode> &type)
      : structExpr_(std::move(structExpr)), memberName_(std::move(memberName)), structName_(std::move(structName)),
        ExpressionNode(isSigned), type_(type) {}

  llvm::Value *codeGen(CodegenContext &ctx) override;
  llvm::Value *codeGenLValue(CodegenContext &ctx) override;

private:
  std::unique_ptr<ExpressionNode> structExpr_;
  std::string memberName_;
  std::string structName_;
  std::shared_ptr<ClassReferenceNode> type_;
};

class ArrayAccess : public ExpressionNode {
public:
  ArrayAccess(std::unique_ptr<ExpressionNode> &&arrayExpr, std::unique_ptr<ExpressionNode> &&indexExpr, bool isSigned,
              std::shared_ptr<ArrayTypeNode> &type)
      : arrayExpr_(std::move(arrayExpr)), indexExpr_(std::move(indexExpr)), ExpressionNode(isSigned), type_(type) {}

  llvm::Value *codeGen(CodegenContext &ctx) override;
  llvm::Value *codeGenLValue(CodegenContext &ctx) override;

private:
  std::unique_ptr<ExpressionNode> arrayExpr_;
  std::unique_ptr<ExpressionNode> indexExpr_;
  std::shared_ptr<ArrayTypeNode> type_;
};

class PtrIndexAccess : public ExpressionNode {
public:
  PtrIndexAccess(std::unique_ptr<ExpressionNode> &&ptrExpr, std::unique_ptr<ExpressionNode> &&indexExpr, bool isSigned,
                 std::shared_ptr<PointerTypeNode> &type)
      : ptrExpr_(std::move(ptrExpr)), indexExpr_(std::move(indexExpr)), ExpressionNode(isSigned), type_(type) {}

  llvm::Value *codeGen(CodegenContext &ctx) override;
  llvm::Value *codeGenLValue(CodegenContext &ctx) override;

private:
  std::unique_ptr<ExpressionNode> ptrExpr_;
  std::unique_ptr<ExpressionNode> indexExpr_;
  std::shared_ptr<PointerTypeNode> type_;
};

class Dref : public ExpressionNode {
public:
  Dref(std::unique_ptr<ExpressionNode> &&target, std::shared_ptr<TypeNode> &derivedType, bool isSigned)
      : target_(std::move(target)), derivedType_(derivedType), ExpressionNode(isSigned) {}

  llvm::Value *codeGen(CodegenContext &ctx) override;
  llvm::Value *codeGenLValue(CodegenContext &ctx) override;

private:
  std::unique_ptr<ExpressionNode> target_;
  std::shared_ptr<TypeNode> derivedType_;
};

class AddressOf : public ExpressionNode {
public:
  AddressOf(std::unique_ptr<ExpressionNode> &&target, bool isSigned)
      : target_(std::move(target)), ExpressionNode(isSigned) {}

  llvm::Value *codeGen(CodegenContext &ctx) override;

private:
  std::unique_ptr<ExpressionNode> target_;
};

class IntLiteral : public ExpressionNode {
public:
  IntLiteral(int value) : value_(value), ExpressionNode(true) {}

  llvm::Value *codeGen(CodegenContext &ctx) override;

private:
  int value_;
};

class FloatLiteral : public ExpressionNode {
public:
  FloatLiteral(float value) : value_(value), ExpressionNode(true) {}

  llvm::Value *codeGen(CodegenContext &ctx) override;

private:
  float value_;
};

class StringLiteral : public ExpressionNode {
public:
  StringLiteral(std::string value) : value_(value), ExpressionNode(false) {}

  llvm::Value *codeGen(CodegenContext &ctx) override;

private:
  std::string value_;
};

class FunctionCall : public ExpressionNode {
public:
  FunctionCall(std::string name, std::vector<std::unique_ptr<ExpressionNode>> &&args, bool isSigned)
      : name_(std::move(name)), args_(std::move(args)), ExpressionNode(isSigned) {}

  llvm::Value *codeGen(CodegenContext &ctx) override;

private:
  std::string name_;
  std::vector<std::unique_ptr<ExpressionNode>> args_;
};

enum class BinaryOperationType {
  Add,
  Subtract,
  Multiply,
  Divide,
  Less,
  More,
  Equal,
};

class BinaryOperation : public ExpressionNode {
public:
  BinaryOperation(BinaryOperationType opType, std::unique_ptr<ExpressionNode> &&L, std::unique_ptr<ExpressionNode> &&R,
                  bool isSigned)
      : type_(opType), L_(std::move(L)), R_(std::move(R)), ExpressionNode(isSigned) {}

  llvm::Value *codeGen(CodegenContext &ctx) override;

private:
  BinaryOperationType type_;
  std::unique_ptr<ExpressionNode> L_;
  std::unique_ptr<ExpressionNode> R_;
};

} // namespace axen::ast
