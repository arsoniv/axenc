#include "nodes/type.hpp"
#include "nodes/class.hpp"
#include "nodes/context.hpp"

namespace axen::ast {

void FunctionTypeNode::analyze(AnalysisContext &ctx) {
  if (!returnType_) {
    ctx.emitAnalysisError("Function type with no return type", span_);
  }
}

void PointerTypeNode::analyze(AnalysisContext &ctx) {

  if (!target_) {
    ctx.emitAnalysisError("Pointer type with no target type", span_);
    return;
  }
  target_->analyze(ctx);
}

void ArrayTypeNode::analyze(AnalysisContext &ctx) {

  if (!target_) {
    ctx.emitAnalysisError("Array type with no element type", span_);
    return;
  }
  target_->analyze(ctx);

  if (length_ <= 0) {
    ctx.emitAnalysisError("Array length must be positive (got " + std::to_string(length_) + ")", span_);
  }
}

void PrimitiveTypeNode::analyze(AnalysisContext &ctx) {
  // primitive types do not require analysis as they are always valid
}

void ClassReferenceNode::analyze(AnalysisContext &ctx) {

  // ensure that class declaration exists
  auto decl = decl_.lock();
  if (!decl) {
    ctx.emitAnalysisError("Class reference has no declaration", span_);
    return;
  }

  const std::string &className = decl->getName();
  if (className.empty()) {
    ctx.emitAnalysisError("Class reference has empty class name", span_);
    return;
  }

  // ensure the class exists in this context
  auto classNode = ctx.lookupClass(className);
  if (!classNode) {
    ctx.emitAnalysisError("Class '" + className + "' is not defined", span_);
    return;
  }
}

} // namespace axen::ast
