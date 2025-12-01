#include "nodes/function.hpp"
#include <string>

namespace axen::ast {

void FunctionNode::emitHeader(std::ostream &out) {

  std::string paramsString = "(";

  for (int i = 0; i < paramNames_.size(); ++i) {
    if (i > 0) {
      paramsString += ", ";
    }
    paramsString += type_->getParameters().at(i)->getTypeString();
    paramsString += " " + paramNames_.at(i);
  }
  paramsString += ")";

  out << "export " << getReturnType()->getTypeString() << " " << name_ << paramsString << ";\n";
}

} // namespace axen::ast
