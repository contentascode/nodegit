/**
 * This code is auto-generated; unless you know what you're doing, do not modify!
 **/

#ifndef WRAPPER_H
#define WRAPPER_H

#include <v8.h>
#include <napi.h>
#include <uv.h>


using namespace Napi;

class Wrapper : public Napi::ObjectWrap<Wrapper> {
  public:

    static Napi::FunctionReference constructor;
    static void InitializeComponent (Napi::Object target);

    void *GetValue();
    static Napi::Value New(const void *raw);

  private:
    Wrapper(void *raw);

    static Napi::Value JSNewFunction(const Napi::CallbackInfo& info);
    static Napi::Value ToBuffer(const Napi::CallbackInfo& info);

    void *raw;
};

#endif
