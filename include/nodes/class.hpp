#pragma once

#include <memory>
#include <stdexcept>
#include <string>

#include <llvm/IR/DerivedTypes.h>

#include "context.hpp"
#include "error.hpp"

namespace axen::ast {

class ClassNode {
public:
  ClassNode(std::string name, std::vector<std::pair<std::string, std::shared_ptr<ast::TypeNode>>> &&members,
            error::SourceSpan span)
      : name_(name), members_(std::move(members)), span_(span) {}

  llvm::StructType *codeGen(CodegenContext &ctx);
  void analyze(AnalysisContext &ctx);

  void emitHeader(std::ostream &out);

  std::shared_ptr<ast::TypeNode> lookupMemberType(const std::string &name) const {
    for (const auto &member : members_) {
      if (member.first == name) {
        return member.second;
      }
    }
    return nullptr;
  }

  int lookupMemberIndex(const std::string &name) const {
    for (int i = 0; i < members_.size(); ++i) {
      if (members_[i].first == name) {
        return i;
      }
    }
    throw std::runtime_error("Could not find index of member '" + name + "' in struct '" + name_ + "'");
  }

  const std::string &getName() const { return name_; }

  void addMembers(const std::vector<std::pair<std::string, std::shared_ptr<ast::TypeNode>>> &members) {
    for (const auto &member : members) {
      members_.push_back(member);
    }
  }

  /// returns all data members
  const std::vector<std::pair<std::string, std::shared_ptr<ast::TypeNode>>> &getMembers() const { return members_; }

  const error::SourceSpan &getSpan() const { return span_; }

private:
  std::string name_;
  std::vector<std::pair<std::string, std::shared_ptr<ast::TypeNode>>> members_;
  error::SourceSpan span_;
};

} // namespace axen::ast
