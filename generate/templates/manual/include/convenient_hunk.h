#ifndef CONVENIENTHUNK_H
#define CONVENIENTHUNK_H
// generated from class_header.h
#include <napi.h>
#include <uv.h>
#include <string>

#include "async_baton.h"
#include "promise_completion.h"

extern "C" {
#include <git2.h>
}

#include "../include/typedefs.h"

struct HunkData {
  git_diff_hunk hunk;
  std::vector<git_diff_line *> *lines;
  size_t numLines;
};

void HunkDataFree(HunkData *hunk);

using namespace Napi;

class ConvenientHunk : public Napi::ObjectWrap<ConvenientHunk> {
  public:
    static Napi::FunctionReference constructor;
    static void InitializeComponent (Napi::Object target);

    static Napi::Value New(void *raw);

    HunkData *GetValue();
    char *GetHeader();
    size_t GetSize();

  private:
    ConvenientHunk(HunkData *hunk);
    ~ConvenientHunk();

    HunkData *hunk;

    static Napi::Value JSNewFunction(const Napi::CallbackInfo& info);
    static Napi::Value Size(const Napi::CallbackInfo& info);

    static Napi::Value OldStart(const Napi::CallbackInfo& info);
    static Napi::Value OldLines(const Napi::CallbackInfo& info);
    static Napi::Value NewStart(const Napi::CallbackInfo& info);
    static Napi::Value NewLines(const Napi::CallbackInfo& info);
    static Napi::Value HeaderLen(const Napi::CallbackInfo& info);
    static Napi::Value Header(const Napi::CallbackInfo& info);

    struct LinesBaton {
      HunkData *hunk;
      std::vector<git_diff_line *> *lines;
    };
    class LinesWorker : public Napi::AsyncWorker {
      public:
        LinesWorker(
            LinesBaton *_baton,
            Napi::FunctionReference *callback
        ) : Napi::AsyncWorker(callback)
          , baton(_baton) {};
        ~LinesWorker() {};
        void Execute();
        void OnOK();

      private:
        LinesBaton *baton;
    };
    static Napi::Value Lines(const Napi::CallbackInfo& info);
};

#endif
