#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>

#include "json.h"
#include "lsp.hpp"

namespace axenlsp {

void Lsp::sendMessage(const std::string &json_str) {
  std::cout << "Content-Length: " << json_str.size() << "\r\n\r\n" << json_str;
  std::cout.flush();
}

void Lsp::sendResponse(const std::string &id, const std::string &result) {
  std::string response = R"({"jsonrpc":"2.0","id":)" + id + ",\"result\":" + result + "}";
  sendMessage(response);
}

void Lsp::sendError(const std::string &id, int code, const std::string &message) {
  std::ostringstream oss;
  oss << R"({"jsonrpc":"2.0","id":)" << id << R"(,"error":{"code":)" << code << R"(,"message":")"
      << escapeJsonString(message) << "\"}}";
  sendMessage(oss.str());
}

void Lsp::handleInitialize(const std::string &id) {
  std::string result = "{\"capabilities\":{\"textDocumentSync\":{\"openClose\":true,\"change\":1},"
                       "\"completionProvider\":{\"resolveProvider\":false},"
                       "\"hoverProvider\":false,\"definitionProvider\":false},"
                       "\"serverInfo\":{\"name\":\"axenlsp\","
                       "\"version\":\"0.0.1\"}}";
  sendResponse(id, result);
}

void Lsp::handleDidOpen(json_object_s *params) {
  if (params == nullptr) {
    return;
  }

  json_object_s *textDoc = getObjectField(params, "textDocument");
  if (textDoc == nullptr) {
    return;
  }

  std::string uri = getStringField(textDoc, "uri");
  std::string text = getStringField(textDoc, "text");

  if (uri.empty()) {
    return;
  }

  openFiles_[uri] = text;
  publishDiagnostics(uri, text);
}

void Lsp::handleDidChange(json_object_s *params) {
  if (params == nullptr) {
    return;
  }

  json_object_s *textDoc = getObjectField(params, "textDocument");
  if (textDoc == nullptr) {
    return;
  }

  json_array_s *changes = getArrayField(params, "contentChanges");
  if ((changes == nullptr) || (changes->start == nullptr)) {
    return;
  }

  json_object_s *change = nullptr;
  if (changes->start->value->type == json_type_object) {
    change = (json_object_s *)changes->start->value->payload;
  }
  if (change == nullptr) {
    return;
  }

  std::string uri = getStringField(textDoc, "uri");
  std::string text = getStringField(change, "text");

  if (uri.empty()) {
    return;
  }

  openFiles_[uri] = text;
  publishDiagnostics(uri, text);
}

void Lsp::handleDidClose(json_object_s *params) {
  if (params == nullptr) {
    return;
  }

  json_object_s *textDoc = getObjectField(params, "textDocument");
  if (textDoc == nullptr) {
    return;
  }

  std::string uri = getStringField(textDoc, "uri");
  if (uri.empty()) {
    return;
  }

  openFiles_.erase(uri);
}

json_value_s *Lsp::readMessage() {
  std::string line;
  size_t contentLength = 0;

  while (std::getline(std::cin, line)) {
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
    if (line.empty()) {
      break;
    }
    if (line.starts_with("Content-Length:")) {
      contentLength = static_cast<size_t>(std::stoi(line.substr(15)));
    }
  }

  if (std::cin.eof() || contentLength == 0) {
    return nullptr;
  }

  std::string body(contentLength, ' ');
  std::cin.read(body.data(), static_cast<std::streamsize>(contentLength));

  if (!std::cin) {
    return nullptr;
  }

  json_value_s *value = json_parse(body.c_str(), body.size());
  if (value == nullptr) {
    std::cerr << "Failed to parse JSON" << '\n';
  }
  return value;
}

void Lsp::run() {
  while (true) {
    json_value_s *request = readMessage();
    if (request == nullptr) {
      break;
    }

    if (request->type != json_type_object) {
      free(request);
      continue;
    }

    auto *obj = (json_object_s *)request->payload;
    std::string method = getStringField(obj, "method");

    if (method.empty()) {
      free(request);
      continue;
    }

    std::string id;
    json_object_element_s *elem = obj->start;
    while (elem != nullptr) {
      if (strcmp(elem->name->string, "id") == 0) {
        if (elem->value->type == json_type_number) {
          auto *num = (json_number_s *)elem->value->payload;
          id = std::string(num->number, num->number_size);
        } else if (elem->value->type == json_type_string) {
          auto *str = (json_string_s *)elem->value->payload;
          id = "\"" + std::string(str->string, str->string_size) + "\"";
        }
        break;
      }
      elem = elem->next;
    }

    json_object_s *params = getObjectField(obj, "params");

    if (method == "initialize") {
      handleInitialize(id);
    } else if (method == "initialized") {

    } else if (method == "shutdown") {
      sendResponse(id, "{}");
    } else if (method == "exit") {
      free(request);
      break;
    } else if (method == "textDocument/didOpen") {
      handleDidOpen(params);
    } else if (method == "textDocument/didChange") {
      handleDidChange(params);
    } else if (method == "textDocument/didClose") {
      handleDidClose(params);
    } else if (method == "textDocument/completion") {
      handleCompletion(id, params);
    }

    free(request);
  }
}

} // namespace axenlsp

int main() {
  axenlsp::Lsp lsp;
  lsp.run();
  return 0;
}
