#pragma once

#include <memory>
#include <vector>

#include <llvm/IR/Type.h>

#include "context.hpp"
#include "nodes/class.hpp"

namespace axen::ast {

class TypeNode {
public:
  virtual ~TypeNode() = default;
  virtual void analyze(AnalysisContext &ctx) = 0;
  virtual llvm::Type *codeGen(CodegenContext &ctx) = 0;

  virtual bool isSigned() = 0;
};

class FunctionTypeNode : public TypeNode {
public:
  FunctionTypeNode(std::shared_ptr<TypeNode> returnType, std::vector<std::shared_ptr<TypeNode>> parameters)
      : returnType_(returnType), parameters_(parameters) {}

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
  PointerTypeNode(std::shared_ptr<TypeNode> target) : target_(target) {}

  void analyze(AnalysisContext &ctx) override;
  llvm::Type *codeGen(CodegenContext &ctx) override;

  std::shared_ptr<TypeNode> target() const { return target_; }

  bool isSigned() override { return target_->isSigned(); }

private:
  std::shared_ptr<TypeNode> target_;
};

class ArrayTypeNode : public TypeNode {
public:
  ArrayTypeNode(std::shared_ptr<TypeNode> target, int length) : target_(target), length_(length) {}

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

class PrimitiveTypeNode : public TypeNode {
public:
  PrimitiveTypeNode(PrimitiveType type, bool isSigned) : type_(type), isSigned_(isSigned) {}

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
  ClassReferenceNode(std::shared_ptr<ClassNode> decl) : decl_(decl) {}

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
