#include <cstring>
#include <iomanip>
#include <sstream>
#include <string>

#include "json.h"
#include "lsp.hpp"

namespace axenlsp {

std::string Lsp::escapeJsonString(const std::string &str) {
  std::ostringstream oss;
  for (char c : str) {
    switch (c) {
    case '"':
      oss << "\\\"";
      break;
    case '\\':
      oss << "\\\\";
      break;
    case '\b':
      oss << "\\b";
      break;
    case '\f':
      oss << "\\f";
      break;
    case '\n':
      oss << "\\n";
      break;
    case '\r':
      oss << "\\r";
      break;
    case '\t':
      oss << "\\t";
      break;
    default:
      if (c < 0x20) {
        oss << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c);
      } else {
        oss << c;
      }
      break;
    }
  }
  return oss.str();
}

std::string Lsp::getStringField(json_object_s *obj, const char *field_name) {
  if (obj == nullptr) {
    return "";
  }

  json_object_element_s *elem = obj->start;
  while (elem != nullptr) {
    if (strcmp(elem->name->string, field_name) == 0 && elem->value->type == json_type_string) {
      auto *str = (json_string_s *)elem->value->payload;
      return std::string(str->string, str->string_size);
    }
    elem = elem->next;
  }
  return "";
}

json_object_s *Lsp::getObjectField(json_object_s *obj, const char *field_name) {
  if (obj == nullptr) {
    return nullptr;
  }

  json_object_element_s *elem = obj->start;
  while (elem != nullptr) {
    if (strcmp(elem->name->string, field_name) == 0 && elem->value->type == json_type_object) {
      return (json_object_s *)elem->value->payload;
    }
    elem = elem->next;
  }
  return nullptr;
}

json_array_s *Lsp::getArrayField(json_object_s *obj, const char *field_name) {
  if (obj == nullptr) {
    return nullptr;
  }

  json_object_element_s *elem = obj->start;
  while (elem != nullptr) {
    if (strcmp(elem->name->string, field_name) == 0 && elem->value->type == json_type_array) {
      return (json_array_s *)elem->value->payload;
    }
    elem = elem->next;
  }
  return nullptr;
}

json_number_s *Lsp::getNumberField(json_object_s *obj, const char *field_name) {
  if (obj == nullptr) {
    return nullptr;
  }

  json_object_element_s *elem = obj->start;
  while (elem != nullptr) {
    if (strcmp(elem->name->string, field_name) == 0 && elem->value->type == json_type_number) {
      return (json_number_s *)elem->value->payload;
    }
    elem = elem->next;
  }
  return nullptr;
}

} // namespace axenlsp
