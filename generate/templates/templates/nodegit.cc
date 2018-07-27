#include <napi.h>
#include <uv.h>
#include <v8.h>

#include <git2.h>
#include <map>
#include <algorithm>
#include <set>

#include <openssl/crypto.h>

#include "../include/init_ssh2.h"
#include "../include/lock_master.h"
#include "../include/nodegit.h"
#include "../include/wrapper.h"
#include "../include/promise_completion.h"
#include "../include/functions/copy.h"
{% each %}
  {% if type != "enum" %}
    #include "../include/{{ filename }}.h"
  {% endif %}
{% endeach %}
#include "../include/convenient_patch.h"
#include "../include/convenient_hunk.h"
#include "../include/filter_registry.h"

#if (NODE_API_MODULE_VERSION > 48)
  Napi::Value GetPrivate(Napi::Object object,
                                      Napi::String key) {
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();
    v8::Local<v8::Private> privateKey = v8::Private::ForApi(isolate, key);
    Napi::Value value;
    v8::Maybe<bool> result = object->HasPrivate(context, privateKey);
    if (!(result.IsJust() && result))
      return Napi::Value();
    if (object->GetPrivate(context, privateKey).ToLocal(&value))
      return value;
    return Napi::Value();
  }

  void SetPrivate(Napi::Object object,
                      Napi::String key,
                      Napi::Value value) {
    if (value.IsEmpty())
      return;
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();
    v8::Local<v8::Private> privateKey = v8::Private::ForApi(isolate, key);
    object->SetPrivate(context, privateKey, value);
  }
#else
  Napi::Value GetPrivate(Napi::Object object,
                                      Napi::String key) {
    return object->GetHiddenValue(key);
  }

  void SetPrivate(Napi::Object object,
                      Napi::String key,
                      Napi::Value value) {
    object->SetHiddenValue(key, value);
  }
#endif

void LockMasterEnable(const FunctionCallbackInfo<Value>& info) {
  LockMaster::Enable();
}

void LockMasterSetStatus(const FunctionCallbackInfo<Value>& info) {
  Napi::HandleScope scope(env);

  // convert the first argument to Status
  if(info.Length() >= 0 && info[0].IsNumber()) {
    v8::Local<v8::Int32> value = info[0].ToInt32();
    LockMaster::Status status = static_cast<LockMaster::Status>(value->Value());
    if(status >= LockMaster::Disabled && status <= LockMaster::Enabled) {
      LockMaster::SetStatus(status);
      return;
    }
  }

  // argument error
  Napi::Error::New(env, "Argument must be one 0, 1 or 2").ThrowAsJavaScriptException();

}

void LockMasterGetStatus(const FunctionCallbackInfo<Value>& info) {
  return Napi::New(env, LockMaster::GetStatus());
}

void LockMasterGetDiagnostics(const FunctionCallbackInfo<Value>& info) {
  LockMaster::Diagnostics diagnostics(LockMaster::GetDiagnostics());

  // return a plain JS object with properties
  Napi::Object result = Napi::Object::New(env);
  result.Set(Napi::String::New(env, "storedMutexesCount"), Napi::New(env, diagnostics.storedMutexesCount));
  return result;
}

static uv_mutex_t *opensslMutexes;

void OpenSSL_LockingCallback(int mode, int type, const char *, int) {
  if (mode & CRYPTO_LOCK) {
    uv_mutex_lock(&opensslMutexes[type]);
  } else {
    uv_mutex_unlock(&opensslMutexes[type]);
  }
}

unsigned long OpenSSL_IDCallback() {
  return (unsigned long)uv_thread_self();
}

void OpenSSL_ThreadSetup() {
  opensslMutexes=(uv_mutex_t *)malloc(CRYPTO_num_locks() * sizeof(uv_mutex_t));

  for (int i=0; i<CRYPTO_num_locks(); i++) {
    uv_mutex_init(&opensslMutexes[i]);
  }

  CRYPTO_set_locking_callback(OpenSSL_LockingCallback);
  CRYPTO_set_id_callback(OpenSSL_IDCallback);
}

ThreadPool libgit2ThreadPool(10, uv_default_loop());

extern "C" void init(Napi::Object target) {
  // Initialize thread safety in openssl and libssh2
  OpenSSL_ThreadSetup();
  init_ssh2();
  // Initialize libgit2.
  git_libgit2_init();

  Napi::HandleScope scope(env);

  Wrapper::InitializeComponent(target);
  PromiseCompletion::InitializeComponent();
  {% each %}
    {% if type != "enum" %}
      {{ cppClassName }}::InitializeComponent(target);
    {% endif %}
  {% endeach %}

  ConvenientHunk::InitializeComponent(target);
  ConvenientPatch::InitializeComponent(target);
  GitFilterRegistry::InitializeComponent(target);

  NODE_SET_METHOD(target, "enableThreadSafety", LockMasterEnable);
  NODE_SET_METHOD(target, "setThreadSafetyStatus", LockMasterSetStatus);
  NODE_SET_METHOD(target, "getThreadSafetyStatus", LockMasterGetStatus);
  NODE_SET_METHOD(target, "getThreadSafetyDiagnostics", LockMasterGetDiagnostics);

  Napi::Object threadSafety = Napi::Object::New(env);
  threadSafety.Set(Napi::String::New(env, "DISABLED"), Napi::New(env, (int)LockMaster::Disabled));
  threadSafety.Set(Napi::String::New(env, "ENABLED_FOR_ASYNC_ONLY"), Napi::New(env, (int)LockMaster::EnabledForAsyncOnly));
  threadSafety.Set(Napi::String::New(env, "ENABLED"), Napi::New(env, (int)LockMaster::Enabled));

  target.Set(Napi::String::New(env, "THREAD_SAFETY"), threadSafety);

  LockMaster::Initialize();
}

NODE_API_MODULE(nodegit, init)
