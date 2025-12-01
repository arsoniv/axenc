#pragma once

#include <memory>
#include <string>
#include <vector>

#include <llvm/IR/Type.h>

#include "context.hpp"

namespace axen::ast {

class TypeNode {
public:
  TypeNode(error::SourceSpan span, std::string typeString) : span_(span), typeString_(typeString) {}
  virtual ~TypeNode() = default;
  virtual void analyze(AnalysisContext &ctx) = 0;
  virtual llvm::Type *codeGen(CodegenContext &ctx) = 0;

  virtual bool isSigned() = 0;

  const error::SourceSpan &getSpan() const { return span_; }

  const std::string &getTypeString() const { return typeString_; }

protected:
  std::string typeString_;
  error::SourceSpan span_;
};

class FunctionTypeNode : public TypeNode {
public:
  FunctionTypeNode(std::shared_ptr<TypeNode> returnType, std::vector<std::shared_ptr<TypeNode>> parameters,
                   error::SourceSpan span)
      : TypeNode(span, ""), // temporarily empty
        returnType_(returnType), parameters_(parameters) {
    if (!returnType_) {
      throw std::runtime_error("FunctionTypeNode: returnType is null");
    }

    std::string typeStr = returnType_->getTypeString() + "(";
    bool first = true;
    for (const auto &param : parameters_) {
      if (!first)
        typeStr += ", ";
      typeStr += param ? param->getTypeString() : "<null>";
      first = false;
    }
    typeStr += ")";
    typeString_ = typeStr;
  }

  void analyze(AnalysisContext &ctx) override;
  llvm::Type *codeGen(CodegenContext &ctx) override;

  bool isSigned() override { return false; }

  const std::vector<std::shared_ptr<TypeNode>> &getParameters() const { return parameters_; }
  const std::shared_ptr<TypeNode> &getReturn() const { return returnType_; }

private:
  std::shared_ptr<TypeNode> returnType_;
  std::vector<std::shared_ptr<TypeNode>> parameters_;
};

class PointerTypeNode : public TypeNode {
public:
  PointerTypeNode(std::shared_ptr<TypeNode> target, error::SourceSpan span)
      : TypeNode(span, target->getTypeString().substr(4)), target_(target) {}

  void analyze(AnalysisContext &ctx) override;
  llvm::Type *codeGen(CodegenContext &ctx) override;

  std::shared_ptr<TypeNode> target() const { return target_; }

  bool isSigned() override { return target_->isSigned(); }

private:
  std::shared_ptr<TypeNode> target_;
};

class ArrayTypeNode : public TypeNode {
public:
  ArrayTypeNode(std::shared_ptr<TypeNode> target, int length, error::SourceSpan span)
      : TypeNode(span, target->getTypeString() + "[" + std::to_string(length) + "]"), target_(target), length_(length) {
  }

  void analyze(AnalysisContext &ctx) override;
  llvm::Type *codeGen(CodegenContext &ctx) override;

  std::shared_ptr<TypeNode> target() const { return target_; }

  bool isSigned() override { return target_->isSigned(); }

private:
  std::shared_ptr<TypeNode> target_;
  int length_;
};

enum class PrimitiveType {
  Void,
  Bool,

  // int
  Char,
  Short,
  Int,
  Long,

  // fp
  Half,
  Float,
  Double,
  Quad,
};

// from type to string
inline const std::unordered_map<PrimitiveType, std::string> primitiveTypeToString = {
    {PrimitiveType::Void, "void"},   {PrimitiveType::Bool, "bool"},   {PrimitiveType::Char, "char"},
    {PrimitiveType::Short, "short"}, {PrimitiveType::Int, "int"},     {PrimitiveType::Long, "long"},
    {PrimitiveType::Half, "half"},   {PrimitiveType::Float, "float"}, {PrimitiveType::Double, "double"},
    {PrimitiveType::Quad, "quad"},
};

// from string to type
inline const std::unordered_map<std::string, PrimitiveType> stringToPrimitiveType = {
    {"void", PrimitiveType::Void},   {"bool", PrimitiveType::Bool},   {"char", PrimitiveType::Char},
    {"short", PrimitiveType::Short}, {"int", PrimitiveType::Int},     {"long", PrimitiveType::Long},
    {"half", PrimitiveType::Half},   {"float", PrimitiveType::Float}, {"double", PrimitiveType::Double},
    {"quad", PrimitiveType::Quad},
};

class PrimitiveTypeNode : public TypeNode {
public:
  PrimitiveTypeNode(PrimitiveType type, bool isSigned, error::SourceSpan span)
      : TypeNode(span, primitiveTypeToString.at(type)), type_(type), isSigned_(isSigned) {}

  void analyze(AnalysisContext &ctx) override;
  llvm::Type *codeGen(CodegenContext &ctx) override;

  bool isSigned() override { return isSigned_; }

  PrimitiveType getType() { return type_; }

private:
  PrimitiveType type_;

  // only applies to integer types. floating point types are always signed.
  bool isSigned_;
};

class ClassReferenceNode : public TypeNode {
public:
  ClassReferenceNode(std::shared_ptr<ClassNode> decl, error::SourceSpan span);

  void analyze(AnalysisContext &ctx) override;
  llvm::Type *codeGen(CodegenContext &ctx) override;

  bool isSigned() override { return false; }

  std::shared_ptr<ClassNode> getDecl() {
    auto ptr = decl_.lock();
    if (!ptr) {
      throw std::runtime_error("ClassNode has been destroyed");
    }
    return ptr;
  }

private:
  std::weak_ptr<ClassNode> decl_;
};

} // namespace axen::ast
