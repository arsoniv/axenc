#include "nodes/type.hpp"
#include "nodes/context.hpp"

namespace axen::ast {

void FunctionTypeNode::analyze(AnalysisContext &ctx) {
  if (!returnType_) {
    ctx.reportError("Function type with no return type");
  }
}

void PointerTypeNode::analyze(AnalysisContext &ctx) {

  if (!target_) {
    ctx.reportError("Pointer type with no target type");
    return;
  }
  target_->analyze(ctx);
}

void ArrayTypeNode::analyze(AnalysisContext &ctx) {

  if (!target_) {
    ctx.reportError("Array type with no element type");
    return;
  }
  target_->analyze(ctx);

  if (length_ <= 0) {
    ctx.reportError("Array length must be positive (got " + std::to_string(length_) + ")");
  }
}

void PrimitiveTypeNode::analyze(AnalysisContext &ctx) {
  // primitive types do not require analysis as they are always valid
}

void ClassReferenceNode::analyze(AnalysisContext &ctx) {

  // ensure that class declaration exists
  auto decl = decl_.lock();
  if (!decl) {
    ctx.reportError("Class reference has no declaration");
    return;
  }

  const std::string &className = decl->getName();
  if (className.empty()) {
    ctx.reportError("Class reference has empty class name");
    return;
  }

  // ensure the class exists in this context
  auto classNode = ctx.lookupClass(className);
  if (!classNode) {
    ctx.reportError("Class '" + className + "' is not defined");
    return;
  }
}

} // namespace axen::ast
