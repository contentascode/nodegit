#include <napi.h>
#include <uv.h>
#include <string>
#include <cstring>

#include "../include/str_array_converter.h"
#include "git2/strarray.h"

using namespace Napi;

git_strarray *StrArrayConverter::Convert(Local<v8::Value> val) {
  if (!val.As<Napi::Boolean>().Value()) {
    return NULL;
  }
  else if (val->IsArray()) {
    return ConvertArray(*val.As<Napi::Array>());
  }
  else if (val.IsString() || val->IsStringObject()) {
    return ConvertString(val->ToString());
  }
  else {
    return NULL;
  }
}

git_strarray * StrArrayConverter::AllocStrArray(const size_t count) {
  const size_t size = sizeof(git_strarray) + (sizeof(char*) * count);
  uint8_t* memory = reinterpret_cast<uint8_t*>(malloc(size));
  git_strarray *result = reinterpret_cast<git_strarray *>(memory);
  result->count = count;
  result->strings = reinterpret_cast<char**>(memory + sizeof(git_strarray));
  return result;
}

git_strarray *StrArrayConverter::ConvertArray(Array *val) {
  git_strarray *result = AllocStrArray(val->Length());

  for(size_t i = 0; i < result->count; i++) {
    std::string entry = val->Get(i.As<Napi::String>());
    result->strings[i] = strdup(*entry);
  }

  return result;
}

git_strarray* StrArrayConverter::ConvertString(Napi::String val) {
  char *strings[1];
  std::string utf8String = val.As<Napi::String>();

  strings[0] = *utf8String;

  return ConstructStrArray(1, strings);
}

git_strarray *StrArrayConverter::ConstructStrArray(int argc, char** argv) {
  git_strarray *result = AllocStrArray(argc);

  for(size_t i = 0; i < result->count; i++) {
    result->strings[i] = strdup(argv[i]);
  }

  return result;
}
