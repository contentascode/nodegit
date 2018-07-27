#ifndef GITFILTERREGISTRY_H
#define GITFILTERREGISTRY_H
#include <napi.h>
#include <uv.h>
#include <string>
#include <queue>
#include <utility>

#include "async_baton.h"
#include "nodegit_wrapper.h"
#include "promise_completion.h"

extern "C" {
#include <git2.h>
}

#include "../include/typedefs.h"

#include "../include/filter.h"

using namespace Napi;


class GitFilterRegistry : public Napi::ObjectWrap<GitFilterRegistry> {
   public:
    static void InitializeComponent(Napi::Object target);

    static Napi::Persistent<v8::Object> persistentHandle;

  private:
         
    static Napi::Value GitFilterRegister(const Napi::CallbackInfo& info);

    static Napi::Value GitFilterUnregister(const Napi::CallbackInfo& info);

    struct FilterRegisterBaton {
      const git_error *error;
      git_filter *filter;
      char *filter_name;
      int filter_priority;
      int error_code;
    };

    struct FilterUnregisterBaton {
      const git_error *error;
      char *filter_name;
      int error_code;
    };

    class RegisterWorker : public Napi::AsyncWorker {
      public:
        RegisterWorker(FilterRegisterBaton *_baton, Napi::FunctionReference *callback) 
        : Napi::AsyncWorker(callback), baton(_baton) {};
        ~RegisterWorker() {};
        void Execute();
        void OnOK();

      private:
        FilterRegisterBaton *baton;
    };

    class UnregisterWorker : public Napi::AsyncWorker {
      public:
        UnregisterWorker(FilterUnregisterBaton *_baton, Napi::FunctionReference *callback) 
        : Napi::AsyncWorker(callback), baton(_baton) {};
        ~UnregisterWorker() {};
        void Execute();
        void OnOK();

      private:
        FilterUnregisterBaton *baton;
    };
};

#endif
