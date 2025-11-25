#pragma once

#include <string>
#include <unordered_map>

#include "json.h"

namespace axenlsp {

enum class CompletionItemKind : int {
  Text = 1,
  Method = 2,
  Function = 3,
  Constructor = 4,
  Field = 5,
  Variable = 6,
  Class = 7,
  Interface = 8,
  Module = 9,
  Property = 10,
  Unit = 11,
  Value = 12,
  Enum = 13,
  Keyword = 14,
  Snippet = 15,
  Color = 16,
  File = 17,
  Reference = 18,
  Folder = 19,
  EnumMember = 20,
  Constant = 21,
  Struct = 22,
  Event = 23,
  Operator = 24,
  TypeParameter = 25
};

class Lsp {

public:
  void run();

  void publishDiagnostics(const std::string &uri, const std::string &text);
  void handleCompletion(const std::string &id, json_object_s *params);
  void handleInitialize(const std::string &id);
  void handleDidOpen(json_object_s *params);
  void handleDidChange(json_object_s *params);
  void handleDidClose(json_object_s *params);

  void addCompletionSuggestion(std::ostringstream &completions, bool &firstItem, const std::string &label,
                               CompletionItemKind kind, const std::string &detail = "");

private:
  void sendMessage(const std::string &json_str);
  void sendResponse(const std::string &id, const std::string &result);
  void sendError(const std::string &id, int code, const std::string &message);
  json_value_s *readMessage();

  std::string escapeJsonString(const std::string &str);
  std::string getStringField(json_object_s *obj, const char *field_name);
  json_object_s *getObjectField(json_object_s *obj, const char *field_name);
  json_array_s *getArrayField(json_object_s *obj, const char *field_name);
  json_number_s *getNumberField(json_object_s *obj, const char *field_name);

  std::unordered_map<std::string, std::string> openFiles_;
};

} // namespace axenlsp
