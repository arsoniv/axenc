

#include "nodes/class.hpp"
#include "nodes/context.hpp"
#include <set>

namespace axen::ast {

void ClassNode::analyze(AnalysisContext &ctx) {
  if (name_.empty()) {
    ctx.reportError("Class with empty name");
    return;
  }

  // check for duplicate class declaration
  auto existingClass = ctx.lookupClass(name_);
  if (existingClass) {
    ctx.reportError("Class '" + name_ + "' already declared");
    return;
  }

  // set context
  auto previousClass = ctx.currentClass;
  ctx.currentClass = name_;

  // check for duplicate member names and analyze member types
  std::set<std::string> memberNames;
  for (const auto &member : members_) {
    if (member.first.empty()) {
      ctx.reportError("Class '" + name_ + "' has member with empty name");
      continue;
    }

    // check for duplicate members
    if (memberNames.count(member.first)) {
      ctx.reportError("Class '" + name_ + "' has duplicate member '" + member.first + "'");
      continue;
    }
    memberNames.insert(member.first);

    if (!member.second) {
      ctx.reportError("Member '" + member.first + "' in class '" + name_ + "' has no type");
      continue;
    }

    member.second->analyze(ctx);
  }

  ctx.registerClass(name_, std::shared_ptr<ClassNode>(this, [](ClassNode *) {}));

  // restore class context
  ctx.currentClass = previousClass;
}

} // namespace axen::ast
