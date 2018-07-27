#include <iostream>
#include "../include/promise_completion.h"

Napi::FunctionReference PromiseCompletion::newFn;
Napi::FunctionReference PromiseCompletion::promiseFulfilled;
Napi::FunctionReference PromiseCompletion::promiseRejected;

// initializes the persistent handles for NAN_METHODs
void PromiseCompletion::InitializeComponent() {
  Napi::FunctionReference newTemplate = Napi::Function::New(env, New);

  newFn.Reset(newTemplate->GetFunction());

  promiseFulfilled.Reset(Napi::Function::New(env, PromiseFulfilled));
  promiseRejected.Reset(Napi::Function::New(env, PromiseRejected));
}

bool PromiseCompletion::ForwardIfPromise(Napi::Value result, AsyncBaton *baton, Callback callback) {
Napi::Env env = result.Env();

  Napi::HandleScope scope(env);

  // check if the result is a promise
  if (!result.IsEmpty() && result.IsObject()) {
    Napi::MaybeLocal<v8::Value> maybeThenProp = (result->ToObject()).Get(Napi::String::New(env, "then"));
    if (!maybeThenProp.IsEmpty()) {
      Napi::Value thenProp = maybeThenProp;
      if(thenProp->IsFunction()) {
        // we can be reasonably certain that the result is a promise

        // create a new v8 instance of PromiseCompletion
        Napi::Object object = Napi::NewInstance(Napi::New(env, newFn));

        // set up the native PromiseCompletion object
        PromiseCompletion *promiseCompletion = ObjectWrap::Unwrap<PromiseCompletion>(object);
        promiseCompletion->Setup(thenProp.As<Napi::Function>(), result, baton, callback);

        return true;
      }
    }
  }

  return false;
}

// creates a new instance of PromiseCompletion, wrapped in a v8 object
Napi::Value PromiseCompletion::New(const Napi::CallbackInfo& info) {
  PromiseCompletion *promiseCompletion = new PromiseCompletion();
  promiseCompletion->Wrap(info.This());
  return info.This();
}

// sets up a Promise to forward the promise result via the baton and callback
void PromiseCompletion::Setup(Napi::Function thenFn, Napi::Value result, AsyncBaton *baton, Callback callback) {
  this->callback = callback;
  this->baton = baton;

  Napi::Object promise = result->ToObject();

  Napi::Object thisHandle = handle();

  Napi::Value argv[2] = {
    Bind(promiseFulfilled, thisHandle),
    Bind(promiseRejected, thisHandle)
  };

  // call the promise's .then method with resolve and reject callbacks
  Napi::FunctionReference(thenFn).Call(promise, 2, argv);
}

// binds an object to be the context of the function.
// there might be a better way to do this than calling Function.bind...
Napi::Value PromiseCompletion::Bind(Napi::FunctionReference &function, Napi::Object object) {
  Napi::EscapableHandleScope scope(env);

  Napi::Function bind =
    (Napi::New(env).Get(function), Napi::String::New(env, "bind"))
      .As<Napi::Function>();

  Napi::Value argv[1] = { object };

  return scope.Escape(bind->Call(Napi::New(env, function), 1, argv));
}

// calls the callback stored in the PromiseCompletion, passing the baton that
// was provided in construction
void PromiseCompletion::CallCallback(bool isFulfilled, const Napi::CallbackInfo&info) {
  Napi::Value resultOfPromise;

  if (info.Length() > 0) {
    resultOfPromise = info[0];
  }

  PromiseCompletion *promiseCompletion = ObjectWrap::Unwrap<PromiseCompletion>(info.This().ToObject());

  (*promiseCompletion->callback)(isFulfilled, promiseCompletion->baton, resultOfPromise);
}

Napi::Value PromiseCompletion::PromiseFulfilled(const Napi::CallbackInfo& info) {
  CallCallback(true, info);
}

Napi::Value PromiseCompletion::PromiseRejected(const Napi::CallbackInfo& info) {
  CallCallback(false, info);
}
