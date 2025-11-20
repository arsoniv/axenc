#pragma once

#include <map>
#include <memory>
#include <string>
#include <unordered_map>

#include "json.h"
#include "nodes/type.hpp"

namespace axenlsp {

class Lsp {

public:
  void run();

  void publishDiagnostics(const std::string &uri, const std::string &text);
  void handleInitialize(const std::string &id);
  void handleDidOpen(json_object_s *params);
  void handleDidChange(json_object_s *params);
  void handleDidClose(json_object_s *params);

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
