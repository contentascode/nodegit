#include <napi.h>
#include <uv.h>
#include <string.h>

extern "C" {
  #include <git2.h>
}

#include "../include/nodegit.h"
#include "../include/lock_master.h"
#include "../include/functions/copy.h"
#include "../include/filter_registry.h"
#include "nodegit_wrapper.cc"
#include "../include/async_libgit2_queue_worker.h"

#include "../include/filter.h"

using namespace std;
using namespace Napi;

Napi::Persistent<v8::Object> GitFilterRegistry::persistentHandle;

// #pragma unmanaged
void GitFilterRegistry::InitializeComponent(Napi::Object target) {
  Napi::HandleScope scope(env);

  v8::Napi::Object object = Napi::Object::New(env);

  Napi::SetMethod(object, "register", GitFilterRegister);
  Napi::SetMethod(object, "unregister", GitFilterUnregister);

  (target).Set(Napi::String::New(env, "FilterRegistry"), object);
  GitFilterRegistry::persistentHandle.Reset(object);
}

Napi::Value GitFilterRegistry::GitFilterRegister(const Napi::CallbackInfo& info) {
  Napi::EscapableHandleScope scope(env);

  if (info.Length() == 0 || !info[0].IsString()) {
    Napi::Error::New(env, "String name is required.").ThrowAsJavaScriptException();
    return env.Null();
  }

  if (info.Length() == 1 || !info[1].IsObject()) {
    Napi::Error::New(env, "Filter filter is required.").ThrowAsJavaScriptException();
    return env.Null();
  }

  if (info.Length() == 2 || !info[2].IsNumber()) {
    Napi::Error::New(env, "Number priority is required.").ThrowAsJavaScriptException();
    return env.Null();
  }

  if (info.Length() == 3 || !info[3].IsFunction()) {
    Napi::Error::New(env, "Callback is required and must be a Function.").ThrowAsJavaScriptException();
    return env.Null();
  }

  FilterRegisterBaton *baton = new FilterRegisterBaton;

  baton->filter = info[1].ToObject())->GetValue(.Unwrap<GitFilter>();
  Napi::String name(env, info[0].ToString());

  baton->filter_name = (char *)malloc(name.Length() + 1);
  memcpy((void *)baton->filter_name, *name, name.Length());
  memset((void *)(((char *)baton->filter_name) + name.Length()), 0, 1);

  baton->error_code = GIT_OK;
  baton->filter_priority = info[2].As<Napi::Number>().Int32Value();

  Napi::New(env, GitFilterRegistry::persistentHandle).Set(info[0].ToString(), info[1].ToObject());

  Napi::FunctionReference *callback = new Napi::FunctionReference(info[3].As<Napi::Function>());
  RegisterWorker *worker = new RegisterWorker(baton, callback);

  worker->SaveToPersistent("filter_name", info[0].ToObject());
  worker->SaveToPersistent("filter_priority", info[2].ToObject());

  AsyncLibgit2QueueWorker(worker);
  return;
}

void GitFilterRegistry::RegisterWorker::Execute() {
  giterr_clear();

  {
    LockMaster lockMaster(/*asyncAction: */true, baton->filter_name, baton->filter);
    int result = git_filter_register(baton->filter_name, baton->filter, baton->filter_priority);
    baton->error_code = result;

    if (result != GIT_OK && giterr_last() != NULL) {
      baton->error = git_error_dup(giterr_last());
    }
  }
}

void GitFilterRegistry::RegisterWorker::OnOK() {
  if (baton->error_code == GIT_OK) {
    Napi::Value result = Napi::New(env, baton->error_code);
    Napi::Value argv[2] = {
      env.Null(),
      result
    };
    callback->Call(2, argv, async_resource);
  }
  else if (baton->error) {
    Napi::Object err;
    if (baton->error->message) {
      err = Napi::Error::New(env, baton->error->message)->ToObject();
    } else {
      err = Napi::Error::New(env, "Method register has thrown an error.")->ToObject();
    }
    err.Set(Napi::String::New(env, "errno"), Napi::New(env, baton->error_code));
    err.Set(Napi::String::New(env, "errorFunction"), Napi::String::New(env, "FilterRegistry.register"));
    Napi::Value argv[1] = {
      err
    };
    callback->Call(1, argv, async_resource);
    if (baton->error->message)
      free((void *)baton->error->message);
    free((void *)baton->error);
  }
  else if (baton->error_code < 0) {
    Napi::Object err = Napi::Error::New(env, "Method register has thrown an error.")->ToObject();
    err.Set(Napi::String::New(env, "errno"), Napi::New(env, baton->error_code));
    err.Set(Napi::String::New(env, "errorFunction"), Napi::String::New(env, "FilterRegistry.register"));
    Napi::Value argv[1] = {
      err
    };
    callback->Call(1, argv, async_resource);
  }
  else {
    callback->Call(0, NULL, async_resource);
  }
  delete baton;
  return;
}

Napi::Value GitFilterRegistry::GitFilterUnregister(const Napi::CallbackInfo& info) {
  Napi::EscapableHandleScope scope(env);

  if (info.Length() == 0 || !info[0].IsString()) {
    Napi::Error::New(env, "String name is required.").ThrowAsJavaScriptException();
    return env.Null();
  }

  if (info.Length() == 1 || !info[1].IsFunction()) {
    Napi::Error::New(env, "Callback is required and must be a Function.").ThrowAsJavaScriptException();
    return env.Null();
  }

  FilterUnregisterBaton *baton = new FilterUnregisterBaton;
  Napi::String name(env, info[0].ToString());

  baton->filter_name = (char *)malloc(name.Length() + 1);
  memcpy((void *)baton->filter_name, *name, name.Length());
  memset((void *)(((char *)baton->filter_name) + name.Length()), 0, 1);

  baton->error_code = GIT_OK;

  /* Setting up Async Worker */
  Napi::FunctionReference *callback = new Napi::FunctionReference(info[1].As<Napi::Function>());
  UnregisterWorker *worker = new UnregisterWorker(baton, callback);

  worker->SaveToPersistent("filter_name", info[0]);

  AsyncLibgit2QueueWorker(worker);
  return;
}

void GitFilterRegistry::UnregisterWorker::Execute() {
  giterr_clear();

  {
    LockMaster lockMaster(/*asyncAction: */true, baton->filter_name);
    int result = git_filter_unregister(baton->filter_name);
    baton->error_code = result;

    if (result != GIT_OK && giterr_last() != NULL) {
      baton->error = git_error_dup(giterr_last());
    }
  }
}

void GitFilterRegistry::UnregisterWorker::OnOK() {
  if (baton->error_code == GIT_OK) {
    Napi::Value result = Napi::New(env, baton->error_code);
    Napi::Value argv[2] = {
      env.Null(),
      result
    };
    callback->Call(2, argv, async_resource);
  }
  else if (baton->error) {
    Napi::Object err;
    if (baton->error->message) {
      err = Napi::Error::New(env, baton->error->message)->ToObject();
    } else {
      err = Napi::Error::New(env, "Method register has thrown an error.")->ToObject();
    }
    err.Set(Napi::String::New(env, "errno"), Napi::New(env, baton->error_code));
    err.Set(Napi::String::New(env, "errorFunction"), Napi::String::New(env, "FilterRegistry.unregister"));
    Napi::Value argv[1] = {
      err
    };
    callback->Call(1, argv, async_resource);
    if (baton->error->message)
      free((void *)baton->error->message);
    free((void *)baton->error);
  }
  else if (baton->error_code < 0) {
    Napi::Object err = Napi::Error::New(env, "Method unregister has thrown an error.")->ToObject();
    err.Set(Napi::String::New(env, "errno"), Napi::New(env, baton->error_code));
    err.Set(Napi::String::New(env, "errorFunction"), Napi::String::New(env, "FilterRegistry.unregister"));
    Napi::Value argv[1] = {
      err
    };
    callback->Call(1, argv, async_resource);
  }
  else {
    callback->Call(0, NULL, async_resource);
  }
  delete baton;
  return;
}
