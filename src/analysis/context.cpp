#include "nodes/context.hpp"
#include "nodes/function.hpp"

namespace axen::ast {

void AnalysisContext::registerFunction(const std::string &name, FunctionNode *func) {
  functions[name] = func;
  allSymbols.emplace_back(name, SymbolInfo::Kind::Function, func->getReturnType(), 0, currentFile, 0, 0);
}

} // namespace axen::ast
