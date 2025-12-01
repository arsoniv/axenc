#include "nodes/class.hpp"
#include "nodes/type.hpp"
#include <string>

namespace axen::ast {

void ClassNode::emitHeader(std::ostream &out) {

  out << "class " << name_ << " {\n\t// auto generated class header does not contain member methods\n\n";

  for (const auto &member : members_) {
    out << "\t" << member.second->getTypeString() << " " << member.first << ";\n";
  }

  out << "}\n\n";
}

} // namespace axen::ast
