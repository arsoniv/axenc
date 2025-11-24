#include "nodes/context.hpp"
#include "nodes/function.hpp"

namespace axen::ast {

void AnalysisContext::registerFunction(const std::string &name, FunctionNode *func) { functions[name] = func; }

} // namespace axen::ast
