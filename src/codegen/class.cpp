

#include <llvm/IR/DerivedTypes.h>

#include "nodes/class.hpp"
#include "nodes/context.hpp"

namespace axen::ast {

llvm::StructType *ClassNode::codeGen(CodegenContext &ctx) {

  auto it = ctx.namedStructs.find(name_);
  if (it != ctx.namedStructs.end())
    return it->second.first;

  llvm::StructType *llvmStruct = llvm::StructType::create(ctx.llvmContext, name_);

  std::vector<std::string> memberNames;
  for (const auto &pair : members_)
    memberNames.push_back(pair.first);

  ctx.declareStruct(name_, llvmStruct, memberNames);

  std::vector<llvm::Type *> llvmMembers;
  for (const auto &pair : members_)
    llvmMembers.push_back(pair.second->codeGen(ctx));

  llvmStruct->setBody(llvmMembers);

  return llvmStruct;
}

} // namespace axen::ast
