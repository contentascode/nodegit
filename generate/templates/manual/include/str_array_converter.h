#ifndef STR_ARRAY_H
#define STR_ARRAY_H

#include <v8.h>

#include "napi.h"
#include "uv.h"
#include "git2/strarray.h"

using namespace Napi;

class StrArrayConverter {
  public:

    static git_strarray *Convert (Napi::Value val);

  private:
    static git_strarray *ConvertArray(Array *val);
    static git_strarray *ConvertString(v8::Napi::String val);
    static git_strarray *AllocStrArray(const size_t count);
    static git_strarray *ConstructStrArray(int argc, char** argv);
};

#endif
