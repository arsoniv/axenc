#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

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

#include "cpptoml.h"
#include "error.hpp"
#include "nodes/context.hpp"
#include "parser.hpp"

static bool ensureDirectory(const std::filesystem::path &p) {
  if (std::filesystem::exists(p))
    return true;
  std::error_code ec;
  if (!std::filesystem::create_directories(p, ec)) {
    fprintf(stderr, "Failed to create directory %s: %s\n", p.c_str(), ec.message().c_str());
    return false;
  }
  return true;
}

static bool loadFile(const std::filesystem::path &p, std::string &out) {
  std::ifstream in(p);
  if (!in)
    return false;
  std::ostringstream ss;
  ss << in.rdbuf();
  out = ss.str();
  return true;
}

int main(int argc, char **argv) {
  const std::filesystem::path projectFile = "axenproject.toml";
  const std::filesystem::path buildDir = "build";
  const std::filesystem::path outputFile = buildDir / "root.ax.o";
  const std::filesystem::path srcFile = "src/main.ax";

  if (!ensureDirectory(buildDir))
    return EXIT_FAILURE;

  std::shared_ptr<cpptoml::table> cfg;
  // read project TOML file using cpptoml header
  try {
    cfg = cpptoml::parse_file(projectFile.string());
  } catch (const cpptoml::parse_exception &e) {
    fprintf(stderr, "Failed to parse TOML: %s\n", e.what());
    return EXIT_FAILURE;
  }

  // get project name
  std::string projectName;
  if (auto name = cfg->get_as<std::string>("name"))
    projectName = *name;
  else {
    fprintf(stderr, "Project name not found in %s\n", projectFile.string().c_str());
    return EXIT_FAILURE;
  }

  fprintf(stdout, "Compiling project: %s\n", projectName.c_str());

  std::string sourceCode;
  if (!loadFile(srcFile, sourceCode)) {
    fprintf(stderr, "Could not open file %s\n", srcFile.string().c_str());
    return EXIT_FAILURE;
  }

  axen::ast::CodegenContext ctx(srcFile.string());
  std::vector<std::string> includePaths{"/usr/include/axen"};

  std::unique_ptr<axen::parser::Parser> parser =
      std::make_unique<axen::parser::Parser>(std::move(sourceCode), srcFile, std::move(includePaths));

  // parse
  parser->parse();

  // run semantic analysis
  axen::ast::AnalysisContext analysisCtx;
  analysisCtx.currentFile = srcFile.string();

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
    return EXIT_FAILURE;
  }
  if (!analysisCtx.errors.empty()) {
    fprintf(stderr, "Semantic analysis found %zu error(s):\n", analysisCtx.errors.size());
    for (const auto &error : analysisCtx.errors) {
      fprintf(stderr, "  %s at %s [%d:%d]\n", error.message.c_str(), error.span.file.c_str(), error.span.startRow,
              error.span.startCol);
    }
    return EXIT_FAILURE;
  }

  try {
    for (const auto &structure : *parser->getClasses())
      structure->codeGen(ctx);
    for (const auto &func : *parser->getFunctions())
      func->codeGen(ctx);
  } catch (const std::runtime_error &e) {
    fprintf(stderr, "Error during code generation: %s\n", e.what());
    return EXIT_FAILURE;
  }

  std::string verifyError;
  llvm::raw_string_ostream verifyStream(verifyError);
  if (llvm::verifyModule(*ctx.module, &verifyStream)) {
    llvm::errs() << "Module verification failed:\n" << verifyError << "\n";
    return EXIT_FAILURE;
  }

  llvm::InitializeAllTargetInfos();
  llvm::InitializeAllTargets();
  llvm::InitializeAllTargetMCs();
  llvm::InitializeAllAsmParsers();
  llvm::InitializeAllAsmPrinters();

  const std::string targetTriple = llvm::sys::getDefaultTargetTriple();
  llvm::Triple triple(targetTriple);
  ctx.module->setTargetTriple(triple);

  std::string lookupError;
  const llvm::Target *target = llvm::TargetRegistry::lookupTarget(targetTriple, lookupError);
  if (!target) {
    llvm::errs() << lookupError << "\n";
    return EXIT_FAILURE;
  }

  llvm::TargetOptions opt;
  auto relocModel = std::optional<llvm::Reloc::Model>(llvm::Reloc::PIC_);
  std::unique_ptr<llvm::TargetMachine> targetMachine(
      target->createTargetMachine(triple, "generic", "", opt, relocModel));

  ctx.module->setDataLayout(targetMachine->createDataLayout());

  std::error_code ec;
  llvm::raw_fd_ostream dest(outputFile.string(), ec, llvm::sys::fs::OF_None);
  if (ec) {
    llvm::errs() << "Could not open output file: " << ec.message() << "\n";
    return EXIT_FAILURE;
  }

  llvm::legacy::PassManager pass;
  if (targetMachine->addPassesToEmitFile(pass, dest, nullptr, llvm::CodeGenFileType::ObjectFile)) {
    llvm::errs() << "Target machine cannot emit this file type.\n";
    return EXIT_FAILURE;
  }

  pass.run(*ctx.module);
  dest.flush();

  // get linker args from toml
  std::vector<std::string> linkTargets;
  if (auto build = cfg->get_table("build")) {
    if (auto linker = build->get_array_of<std::string>("linker_args")) {
      linkTargets = *linker;
    }
  }

  if (linkTargets.empty()) {
    linkTargets = {};
  }

  std::vector<std::string> objectFiles = {outputFile.string()};
  std::string exeFile = buildDir / "root";

  // build ld command
  std::ostringstream cmd;
  cmd << "ld -static -o " << exeFile << " /usr/lib/axen/axenruntime.o ";
  for (const auto &obj : objectFiles)
    cmd << obj << " ";
  for (const auto &lib : linkTargets)
    cmd << lib << " ";

  std::string finalCmd = cmd.str();
  fprintf(stdout, "Running linker command:\n%s\n", finalCmd.c_str());

  int ret = std::system(finalCmd.c_str());
  if (ret != 0) {
    fprintf(stderr, "Linker command failed with code %d\n", ret);
    return EXIT_FAILURE;
  }

  fprintf(stdout, "Build succeeded.\n");
  return EXIT_SUCCESS;
}
