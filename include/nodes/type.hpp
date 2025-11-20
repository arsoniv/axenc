#pragma once

#include <memory>
#include <string>

#include <llvm/IR/Type.h>
#include <utility>
#include <vector>

#include "context.hpp"
#include "error.hpp"

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

/// class decl node
class ClassNode {
  // TODO: add analyze function
public:
  ClassNode(std::string name, std::map<std::string, std::shared_ptr<TypeNode>> &&members)
      : name_(name), members_(std::move(members)) {}

  llvm::StructType *codeGen(CodegenContext &ctx) {
    // TODO: move this to a source file lol

    auto it = ctx.namedStructs.find(name_);
    if (it != ctx.namedStructs.end())
      return it->second.first;

    llvm::StructType *llvmStruct = llvm::StructType::create(ctx.llvmContext, name_);

    std::vector<std::string> memberNames;
    for (const auto &pair : members_)
      memberNames.push_back(pair.first);

    ctx.declareStruct(name_, llvmStruct, memberNames);

    std::vector<llvm::Type *> llvmMembers;
    for (const auto &pair : members_)
      llvmMembers.push_back(pair.second->codeGen(ctx));

    llvmStruct->setBody(llvmMembers);

    return llvmStruct;
  }

  std::shared_ptr<ast::TypeNode> lookupMemberType(const std::string &name) const {
    auto it = members_.find(name);
    return it != members_.end() ? it->second : nullptr;
  }

  int lookupMemberIndex(const std::string &name) const {
    auto it = members_.find(name);
    if (it != members_.end()) {
      return static_cast<int>(std::distance(members_.begin(), it));
    }
    error::reportError(error::ErrorType::Internal,
                       "Could not find index of member '" + name + "' in struct '" + name_ + "'");
    return -1; // unreachable
  }

  const std::string &getName() const { return name_; }

  void addMembers(const std::map<std::string, std::shared_ptr<TypeNode>> &newMembers) {
    members_.insert(newMembers.begin(), newMembers.end());
  }

  /// returns all data members
  const std::map<std::string, std::shared_ptr<TypeNode>> &getMembers() const { return members_; }

private:
  std::string name_;
  std::map<std::string, std::shared_ptr<TypeNode>> members_;
};

class ClassReferenceNode : public TypeNode {
public:
  ClassReferenceNode(std::shared_ptr<ClassNode> decl) : decl_(decl) {}

  void analyze(AnalysisContext &ctx) override;
  llvm::Type *codeGen(CodegenContext &ctx) override;

  const std::string &name() const { return decl_->getName(); }

  bool isSigned() override { return false; }

  std::shared_ptr<ClassNode> getDecl() { return decl_; }

private:
  std::shared_ptr<ClassNode> decl_;
};

} // namespace axen::ast
