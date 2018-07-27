/**
 * This code is auto-generated; unless you know what you're doing, do not modify!
 **/
#include <napi.h>
#include <uv.h>
#include <string>
#include <cstring>

#include "../include/wrapper.h"
#include "node_buffer.h"

using namespace Napi;

Wrapper::Wrapper(void *raw) {
  this->raw = raw;
}

void Wrapper::InitializeComponent(Local<v8::Object> target) {
  Napi::HandleScope scope(env);

  Napi::FunctionReference tpl = Napi::Function::New(env, JSNewFunction);


  tpl->SetClassName(Napi::String::New(env, "Wrapper"));

  InstanceMethod("toBuffer", &ToBuffer),

  constructor.Reset(tpl);
  (target).Set(Napi::String::New(env, "Wrapper"), Napi::GetFunction(tpl));
}

Napi::Value Wrapper::JSNewFunction(const Napi::CallbackInfo& info) {

  if (info.Length() == 0 || !info[0].IsExternal()) {
    Napi::Error::New(env, "void * is required.").ThrowAsJavaScriptException();
    return env.Null();
  }

  Wrapper* object = new Wrapper(*info[0].As<Napi::External>()->Value());
  object->Wrap(info.This());

  return info.This();
}

Local<v8::Value> Wrapper::New(const void *raw) {
  Napi::EscapableHandleScope scope(env);

  Local<v8::Value> argv[1] = { Napi::External::New(env, (void *)raw) };
  Napi::Object instance;
  Napi::FunctionReference constructorHandle = Napi::New(env, constructor);
  instance = Napi::NewInstance(Napi::GetFunction(constructorHandle), 1, argv);

  return scope.Escape(instance);
}

void *Wrapper::GetValue() {
  return this->raw;
}

Napi::Value Wrapper::ToBuffer(const Napi::CallbackInfo& info) {

  if(info.Length() == 0 || !info[0].IsNumber()) {
    Napi::Error::New(env, "Number is required.").ThrowAsJavaScriptException();
    return env.Null();
  }

  int len = info[0].As<Napi::Number>().Int32Value();

  Napi::Function bufferConstructor = Napi::Function::Cast(
    (Napi::GetCurrentContext()->Global()).Get(Napi::String::New(env, "Buffer")));

  Local<v8::Value> constructorArgs[1] = { Napi::New(env, len) };
  Napi::Object nodeBuffer = Napi::NewInstance(bufferConstructor, 1, constructorArgs);

  std::memcpy(nodeBuffer.As<Napi::Buffer<char>>().Data(), info.This())->GetValue(), len.Unwrap<Wrapper>();

  return nodeBuffer;
}


Napi::FunctionReference Wrapper::constructor;
