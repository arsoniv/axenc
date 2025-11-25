#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
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
    } else {
      if (strcmp(argv[i], "-o") == 0 && i < argc) {
        outputFile = argv[i + 1];
        i++;
      } else {
        fprintf(stderr, "Invalid argument: '%s'\n", argv[i]);
        return 1;
      }
    }
  }

  if (srcFile.empty()) {
    fprintf(stderr, "Missing required argument: -f <source file>\n");
    return 1;
  }

  axen::ast::CodegenContext ctx(srcFile);

  std::filesystem::path srcPath = std::filesystem::path(srcFile);

  std::ifstream in(srcFile);
  if (!in) {
    fprintf(stderr, "Could not open file: '%s'\n", srcPath.string().c_str());
    return 1;
  }

  std::ostringstream ss;
  ss << in.rdbuf();
  std::string sourceCode = ss.str();

  std::string className = srcPath.stem().string();

  if (className.empty()) {
    fprintf(stderr, "Invalid class name derived from file path\n");
    return 1;
  }

  std::unique_ptr<axen::parser::Parser> parser = std::make_unique<axen::parser::Parser>(std::move(sourceCode), srcPath);

  // parse
  parser->parse();

  // run semantic analysis
  axen::ast::AnalysisContext analysisCtx;
  analysisCtx.currentFile = srcPath.string();

  // analyze all classes first (so that functions can use them)
  for (const auto &classNode : *parser->getClasses()) {
    classNode->analyze(analysisCtx);
  }

  // analyze all functions
  for (const auto &func : *parser->getFunctions()) {
    func->analyze(analysisCtx);
  }

  // report any errors
  if (!parser->getErrors().empty()) {
    fprintf(stderr, "Parsing found %zu error(s):\n", parser->getErrors().size());
    for (const auto &error : parser->getErrors()) {
      fprintf(stderr, "  %s at %s [%d:%d]\n", error.message.c_str(), error.span.file.c_str(), error.span.startRow,
              error.span.startCol);
    }
    return 1;
  }
  if (!analysisCtx.errors.empty()) {
    fprintf(stderr, "Semantic analysis found %zu error(s):\n", analysisCtx.errors.size());
    for (const auto &error : analysisCtx.errors) {
      fprintf(stderr, "  %s at %s [%d:%d]\n", error.message.c_str(), error.span.file.c_str(), error.span.startRow,
              error.span.startCol);
    }
    return 1;
  }

  // generate code (wrapped in try-catch for codegen exceptions)
  try {
    for (const auto &structure : *parser->getClasses()) {
      structure->codeGen(ctx);
    }
    for (const auto &func : *parser->getFunctions()) {
      func->codeGen(ctx);
    }
  } catch (const std::runtime_error &e) {
    std::cerr << e.what() << std::endl;
    return 1;
  }

  std::string errorStr;
  llvm::raw_string_ostream errorStream(errorStr);
  if (llvm::verifyModule(*ctx.module, &errorStream)) {
    llvm::errs() << "Module verification failed:\n" << errorStr << "\n";
    return 1;
  }

  if (outputFile.empty()) {
    // print llvm IR
    ctx.module->print(llvm::outs(), nullptr);
  } else {
    // generate object file
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
    std::unique_ptr<llvm::TargetMachine> targetMachine(target->createTargetMachine(triple, CPU, features, opt, RM));

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
