#pragma once

#include <map>
#include <memory>
#include <stdexcept>
#include <string>

#include <llvm/IR/DerivedTypes.h>

#include "context.hpp"
#include "error.hpp"
#include "type.hpp"

namespace axen::ast {

class ClassNode {
public:
  ClassNode(std::string name, std::map<std::string, std::shared_ptr<TypeNode>> &&members, error::SourceSpan span)
      : name_(name), members_(std::move(members)), span_(span) {}

  llvm::StructType *codeGen(CodegenContext &ctx);
  void analyze(AnalysisContext &ctx);

  std::shared_ptr<ast::TypeNode> lookupMemberType(const std::string &name) const {
    auto it = members_.find(name);
    return it != members_.end() ? it->second : nullptr;
  }

  int lookupMemberIndex(const std::string &name) const {
    auto it = members_.find(name);
    if (it != members_.end()) {
      return static_cast<int>(std::distance(members_.begin(), it));
    }
    throw std::runtime_error("Could not find index of member '" + name + "' in struct '" + name_ + "'");
  }

  const std::string &getName() const { return name_; }

  void addMembers(const std::map<std::string, std::shared_ptr<TypeNode>> &newMembers) {
    members_.insert(newMembers.begin(), newMembers.end());
  }

  /// returns all data members
  const std::map<std::string, std::shared_ptr<TypeNode>> &getMembers() const { return members_; }

  const error::SourceSpan &getSpan() const { return span_; }

private:
  std::string name_;
  std::map<std::string, std::shared_ptr<TypeNode>> members_;
  error::SourceSpan span_;
};

} // namespace axen::ast
