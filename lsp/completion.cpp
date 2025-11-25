#include <memory>
#include <sstream>
#include <string>

#include "lsp.hpp"
#include "nodes/context.hpp"
#include "nodes/type.hpp"
#include "parser.hpp"

namespace axenlsp {

void Lsp::addCompletionSuggestion(std::ostringstream &completions, bool &firstItem, const std::string &label,
                                  CompletionItemKind kind, const std::string &detail) {
  if (!firstItem) {
    completions << ",";
  }
  firstItem = false;

  completions << R"({"label":")" << escapeJsonString(label) << R"(","kind":)" << static_cast<int>(kind);

  // add detail if provided
  if (!detail.empty()) {
    completions << R"(,"detail":")" << escapeJsonString(detail) << "\"";
  }

  completions << "}";
}

void Lsp::handleCompletion(const std::string &id, json_object_s *params) {
  if (params == nullptr) {
    sendResponse(id, "{\"isIncomplete\":false,\"items\":[]}");
    return;
  }

  json_object_s *textDoc = getObjectField(params, "textDocument");
  if (textDoc == nullptr) {
    sendResponse(id, "{\"isIncomplete\":false,\"items\":[]}");
    return;
  }

  std::string uri = getStringField(textDoc, "uri");
  if (uri.empty()) {
    sendResponse(id, "{\"isIncomplete\":false,\"items\":[]}");
    return;
  }

  // get position for completion
  json_object_s *position = getObjectField(params, "position");
  int row = 0;
  int col = 0;
  if (position != nullptr) {
    json_number_s *lineNum = getNumberField(position, "line");
    json_number_s *charNum = getNumberField(position, "character");
    if (lineNum != nullptr) {
      row = static_cast<int>(std::strtol(lineNum->number, nullptr, 10));
    }
    if (charNum != nullptr) {
      col = static_cast<int>(std::strtol(charNum->number, nullptr, 10));
    }
  }

  // get the text for this file
  auto it = openFiles_.find(uri);
  if (it == openFiles_.end()) {
    sendResponse(id, "{\"isIncomplete\":false,\"items\":[]}");
    return;
  }

  std::string text = it->second;

  // convert URI to file path
  std::string filePath = uri;
  if (filePath.starts_with("file://")) {
    filePath = filePath.substr(7);
  }

  std::ostringstream completions;
  completions << "[";
  bool firstItem = true;

  auto parser = std::make_unique<axen::parser::Parser>(std::string(text), filePath);
  try {
    parser->parse();

  } catch (...) {
    // if parsing fails, return empty completions
  }

  axen::ast::AnalysisContext analysisCtx;
  analysisCtx.currentFile = filePath;
  analysisCtx.cRow = row + 1;
  analysisCtx.cCol = col + 1;

  // analyze classes (declared before current position)
  for (const auto &_class : *parser->getClasses()) {
    _class->analyze(analysisCtx);
  }

  // analyze functions (declared before current position)
  for (const auto &func : *parser->getFunctions()) {
    func->analyze(analysisCtx);
  }

  for (auto contextCompletion : analysisCtx.contextSymbols) {
    CompletionItemKind kind;

    switch (contextCompletion.second) {
    case axen::ast::SymbolType::Function:
      kind = CompletionItemKind::Function;
      break;
    case axen::ast::SymbolType::Variable:
      kind = CompletionItemKind::Variable;
      break;
    case axen::ast::SymbolType::Class:
      kind = CompletionItemKind::Class;
      break;
    default:
      kind = CompletionItemKind::Text;
      break;
    }

    if (contextCompletion.first.find("-") != std::string::npos) {
      continue; // skip member functions
    }

    addCompletionSuggestion(completions, firstItem, contextCompletion.first, kind);
  }

  for (auto typeDef : parser->getTypeDefs()) {
    std::string detail = "type; ";

    std::shared_ptr<axen::ast::TypeNode> type = typeDef.second;

    while (true) {
      auto _classRef = std::dynamic_pointer_cast<axen::ast::ClassReferenceNode>(type);
      if (_classRef) {
        auto classDecl = _classRef->getDecl();
        detail += "class " + classDecl->getName();
        break;
      }

      auto pointerType = std::dynamic_pointer_cast<axen::ast::PointerTypeNode>(type);
      if (pointerType) {
        detail += "ptr -> ";
        type = pointerType->target();
        continue;
      }

      auto primitiveType = std::dynamic_pointer_cast<axen::ast::PrimitiveTypeNode>(type);
      if (primitiveType) {
        if (!primitiveType->isSigned() && primitiveType->getType() != axen::ast::PrimitiveType::Bool) {
          detail += "u" + axen::ast::primitiveTypeToString.at(primitiveType->getType());
        } else {
          detail += axen::ast::primitiveTypeToString.at(primitiveType->getType());
        }
        break;
      }
    }

    addCompletionSuggestion(completions, firstItem, typeDef.first, CompletionItemKind::Class, detail);
  }

  for (auto intDef : parser->getIntDefs()) {
    addCompletionSuggestion(completions, firstItem, intDef.first, CompletionItemKind::Constant);
  }

  completions << "]";

  std::ostringstream response;
  response << "{\"isIncomplete\":false,\"items\":" << completions.str() << "}";
  sendResponse(id, response.str());
}

} // namespace axenlsp
