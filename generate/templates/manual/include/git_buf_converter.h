#ifndef STR_ARRAY_H
#define STR_ARRAY_H

#include <v8.h>

#include "napi.h"
#include "uv.h"
#include "git2/buffer.h"

using namespace Napi;

class GitBufConverter {
  public:
    static git_buf *Convert(Napi::Value val);
};

#endif
