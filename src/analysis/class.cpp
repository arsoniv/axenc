

#include "nodes/class.hpp"
#include "nodes/context.hpp"
#include <set>

namespace axen::ast {

void ClassNode::analyze(AnalysisContext &ctx) {
  if (name_.empty()) {
    ctx.emitAnalysisError("Class with empty name", span_);
    return;
  }

  // check for duplicate class declaration
  auto existingClass = ctx.lookupClass(name_);
  if (existingClass) {
    ctx.emitAnalysisError("Class '" + name_ + "' already declared", span_);
    return;
  }

  // set context
  auto previousClass = ctx.currentClass;
  ctx.currentClass = name_;

  // check for duplicate member names and analyze member types
  std::set<std::string> memberNames;
  for (const auto &member : members_) {
    if (member.first.empty()) {
      ctx.emitAnalysisError("Class '" + name_ + "' has member with empty name", span_);
      continue;
    }

    // check for duplicate members
    if (memberNames.count(member.first)) {
      ctx.emitAnalysisError("Class '" + name_ + "' has duplicate member '" + member.first + "'", span_);
      continue;
    }
    memberNames.insert(member.first);

    if (!member.second) {
      ctx.emitAnalysisError("Member '" + member.first + "' in class '" + name_ + "' has no type", span_);
      continue;
    }

    member.second->analyze(ctx);
  }

  if (ctx.isSymbolAvailableAtCursor(this->getSpan())) {
    ctx.addContextSymbol(name_, SymbolType::Class);
  }
  ctx.registerClass(name_, std::shared_ptr<ClassNode>(this, [](ClassNode *) {}));

  // restore class context
  ctx.currentClass = previousClass;
}

} // namespace axen::ast
