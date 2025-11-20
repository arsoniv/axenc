#include <memory>
#include <sstream>
#include <string>

#include "error.hpp"
#include "lsp.hpp"
#include "nodes/context.hpp"
#include "parser.hpp"

namespace axenlsp {

void Lsp::publishDiagnostics(const std::string &uri, const std::string &text) {
  std::ostringstream diagnostics;
  diagnostics << "[";

  // convert URI to file path (remove "file://" prefix if present)
  std::string filePath = uri;
  if (filePath.starts_with("file://")) {
    filePath = filePath.substr(7);
  }

  bool first_diagnostic = true;

  try {
    auto parser = std::make_unique<axen::parser::Parser>(std::string(text), filePath);
    // parse
    parser->parse();

    // run semantic analysis
    axen::ast::AnalysisContext analysisCtx;
    analysisCtx.currentFile = filePath;
    analysisCtx.collectErrors = true; // collect all errors instead of exiting after first error

    // analyze all classes first (so that functions can register them
    for (const auto &structure : *parser->getStructs()) {
      analysisCtx.registerClass(structure->getName(), structure);
    }

    // analyze all functions
    for (const auto &func : *parser->getFunctions()) {
      try {
        func->analyze(analysisCtx);
      } catch (const std::runtime_error &) {
        // continue analyzing to collect all errors
      }
    }

    // report analysis errors as diagnostics
    for (const auto &error : analysisCtx.errors) {
      if (!first_diagnostic) {
        diagnostics << ",";
      }
      first_diagnostic = false;

      int line = error.row > 0 ? error.row - 1 : 0;
      int col = error.col > 0 ? error.col - 1 : 0;

      diagnostics << R"({"range":{"start":{"line":)" << line << ",\"character\":" << col << R"(},"end":{"line":)"
                  << line << ",\"character\":" << (col + 1) << R"(}},"severity":1,"message":")"
                  << escapeJsonString(error.message) << "\"}";
    }

  } catch (const axen::error::CompilerException &e) {
    if (!first_diagnostic) {
      diagnostics << ",";
    }
    first_diagnostic = false;

    int line = e.getRow() > 0 ? e.getRow() - 1 : 0;
    int col = e.getCol() > 0 ? e.getCol() - 1 : 0;

    diagnostics << R"({"range":{"start":{"line":)" << line << ",\"character\":" << col << R"(},"end":{"line":)" << line
                << ",\"character\":" << (col + 1) << R"(}},"severity":1,"message":")" << escapeJsonString(e.what())
                << "\"}";
  } catch (const std::exception &e) {
    if (!first_diagnostic) {
      diagnostics << ",";
    }
    first_diagnostic = false;

    diagnostics << R"({"range":{"start":{"line":0,"character":0},)"
                << R"("end":{"line":0,"character":1}},)"
                << R"("severity":1,"message":")" << escapeJsonString(e.what()) << "\"}";
  } catch (...) {
    if (!first_diagnostic) {
      diagnostics << ",";
    }
    first_diagnostic = false;

    diagnostics << R"({"range":{"start":{"line":0,"character":0},)"
                << R"("end":{"line":0,"character":1}},)"
                << R"("severity":1,"message":"Unknown compilation error"})";
  }

  diagnostics << "]";

  std::ostringstream notification;
  notification << R"({"jsonrpc":"2.0","method":"textDocument/publishDiagnostics",)"
               << R"("params":{"uri":")" << escapeJsonString(uri) << R"(","diagnostics":)" << diagnostics.str() << "}}";

  sendMessage(notification.str());
}

} // namespace axenlsp
