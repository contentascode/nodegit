#include <napi.h>
#include <uv.h>
#include <string>

#include "../include/git_buf_converter.h"
#include "git2/buffer.h"

using namespace Napi;

git_buf *GitBufConverter::Convert(Local<v8::Value> val) {
  if (val.IsString() || val->IsStringObject()) {
    Napi::String param1(env, val->ToString());
    std::string v8String = std::string(*param1);

    const size_t size = sizeof(git_buf);
    uint8_t* memory = reinterpret_cast<uint8_t *>(malloc(size));
    git_buf *result = reinterpret_cast<git_buf *>(memory);
    size_t stringLength = v8String.Length();

    memory = reinterpret_cast<uint8_t *>(malloc(stringLength));

    memcpy(memory, v8String.c_str(), stringLength);

    result->asize = stringLength;
    result->size = stringLength;
    result->ptr = reinterpret_cast<char *>(memory);
    return result;
  } else {
    return NULL;
  }
}
