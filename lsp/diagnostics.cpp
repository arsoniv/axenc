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

    // analyze all classes
    for (const auto &_class : *parser->getClasses()) {
      _class->analyze(analysisCtx);
    }

    // analyze all functions
    for (const auto &func : *parser->getFunctions()) {
      func->analyze(analysisCtx);
    }

    // report analysis errors as diagnostics
    for (const auto &error : analysisCtx.errors) {
      if (!first_diagnostic) {
        diagnostics << ",";
      }
      first_diagnostic = false;

      diagnostics << R"({"range":{"start":{"line":)" << error.location.startRow - 1
                  << ",\"character\":" << error.location.startCol - 1 << R"(},"end":{"line":)"
                  << error.location.endRow - 1 << ",\"character\":" << error.location.endCol - 1
                  << R"(}},"severity":1,"message":")" << escapeJsonString(error.message) << "\"}";
    }

  } catch (const axen::error::CompilerException &e) {
    if (!first_diagnostic) {
      diagnostics << ",";
    }
    first_diagnostic = false;

    int line = e.getLocation().startRow > 0 ? e.getLocation().startRow - 1 : 0;
    int col = e.getLocation().startCol > 0 ? e.getLocation().startCol - 1 : 0;

    diagnostics << R"({"range":{"start":{"line":)" << line << ",\"character\":" << col << R"(},"end":{"line":)" << line
                << ",\"character\":" << (col + 1) << R"(}},"severity":1,"message":")" << escapeJsonString(e.what())
                << "\"}";
  }

  diagnostics << "]";

  std::ostringstream notification;
  notification << R"({"jsonrpc":"2.0","method":"textDocument/publishDiagnostics",)"
               << R"("params":{"uri":")" << escapeJsonString(uri) << R"(","diagnostics":)" << diagnostics.str() << "}}";

  sendMessage(notification.str());
}

} // namespace axenlsp
