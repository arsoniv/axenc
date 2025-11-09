#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <sstream>

#include <llvm/Bitcode/BitcodeWriter.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/Verifier.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/TargetParser/Host.h>
#include <llvm/Transforms/Utils/Cloning.h>

#include "error.hpp"
#include "nodes/context.hpp"
#include "parser.hpp"

int main(int argc, char **argv) {

  std::string srcFile = "";
  std::string outputFile = "";

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-f") == 0 && i < argc) {
      srcFile = argv[i + 1];
      i++;
    } else if (strcmp(argv[i], "-o") == 0 && i < argc) {
      outputFile = argv[i + 1];
      i++;
    } else {
      axen::error::reportError(axen::error::ErrorType::Syntax, "Invalid argument: '" + std::string(argv[i]) + "'");
    }
  }

  if (srcFile.empty()) {
    axen::error::reportError(axen::error::ErrorType::Syntax, "Missing required argument: -f <source file>");
  }

  axen::ast::CodegenContext ctx(srcFile);

  std::filesystem::path srcPath = std::filesystem::path(srcFile);

  std::ifstream in(srcFile);
  if (!in) {
    axen::error::reportError(axen::error::ErrorType::Syntax, "Could not open file: '" + srcPath.string() + "'");
  }

  std::ostringstream ss;
  ss << in.rdbuf();
  std::string sourceCode = ss.str();

  std::string className = srcPath.stem().string();

  if (className.empty()) {
    axen::error::reportError(axen::error::ErrorType::Internal, "Invalid class name derived from file path");
  }

  std::unique_ptr<axen::parser::Parser> parser = std::make_unique<axen::parser::Parser>(std::move(sourceCode), srcPath);

  parser->parse();

  for (const auto &structure : *parser->getStructs()) {
    structure->codeGen(ctx);
  }

  for (const auto &func : *parser->getFunctions()) {
    func->codeGen(ctx);
  }

  std::string errorStr;
  llvm::raw_string_ostream errorStream(errorStr);
  if (llvm::verifyModule(*ctx.module, &errorStream)) {
    llvm::errs() << "Module verification failed:\n" << errorStr << "\n";
    return 1;
  }

  if (outputFile.empty()) {
    ctx.module->print(llvm::outs(), nullptr);
  } else {
    llvm::InitializeAllTargetInfos();
    llvm::InitializeAllTargets();
    llvm::InitializeAllTargetMCs();
    llvm::InitializeAllAsmParsers();
    llvm::InitializeAllAsmPrinters();

    auto targetTriple = llvm::sys::getDefaultTargetTriple();
    llvm::Triple triple(llvm::sys::getDefaultTargetTriple());
    ctx.module->setTargetTriple(triple);

    std::string error;
    auto target = llvm::TargetRegistry::lookupTarget(targetTriple, error);

    if (!target) {
      llvm::errs() << error;
      return 1;
    }

    auto CPU = "generic";
    auto features = "";

    llvm::TargetOptions opt;
    auto RM = std::optional<llvm::Reloc::Model>(llvm::Reloc::PIC_);
    auto targetMachine = target->createTargetMachine(triple, CPU, features, opt, RM);

    ctx.module->setDataLayout(targetMachine->createDataLayout());

    std::error_code EC;
    llvm::raw_fd_ostream dest(outputFile, EC, llvm::sys::fs::OF_None);

    if (EC) {
      llvm::errs() << "Could not open file: " << EC.message();
      return 1;
    }

    llvm::legacy::PassManager pass;
    auto fileType = llvm::CodeGenFileType::ObjectFile;

    if (targetMachine->addPassesToEmitFile(pass, dest, nullptr, fileType)) {
      llvm::errs() << "TargetMachine can't emit a file of this type";
      return 1;
    }

    pass.run(*ctx.module);
    dest.flush();
  }

  return 0;
}
