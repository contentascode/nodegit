#ifndef CONVENIENTPATCH_H
#define CONVENIENTPATCH_H
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
#include "../include/convenient_hunk.h"

struct ConvenientLineStats {
  size_t context;
  size_t additions;
  size_t deletions;
};

struct PatchData {
  ConvenientLineStats lineStats;
  git_delta_t status;
  git_diff_file new_file;
  git_diff_file old_file;
  std::vector<HunkData *> *hunks;
  size_t numHunks;
};

PatchData *createFromRaw(git_patch *raw);
void PatchDataFree(PatchData *patch);

using namespace Napi;

class ConvenientPatch : public Napi::ObjectWrap<ConvenientPatch> {
  public:
    static Napi::FunctionReference constructor;
    static void InitializeComponent (Napi::Object target);

    static Napi::Value New(void *raw);

    ConvenientLineStats GetLineStats();
    git_delta_t GetStatus();
    git_diff_file GetOldFile();
    git_diff_file GetNewFile();
    size_t GetNumHunks();
    PatchData *GetValue();

  private:
    ConvenientPatch(PatchData *raw);
    ~ConvenientPatch();

    PatchData *patch;

    static Napi::Value JSNewFunction(const Napi::CallbackInfo& info);

    // patch methods
    static Napi::Value LineStats(const Napi::CallbackInfo& info);

    // hunk methods
    static Napi::Value Size(const Napi::CallbackInfo& info);

    struct HunksBaton {
      PatchData *patch;
      std::vector<HunkData *> *hunks;
    };
    class HunksWorker : public Napi::AsyncWorker {
      public:
        HunksWorker(
            HunksBaton *_baton,
            Napi::FunctionReference *callback
        ) : Napi::AsyncWorker(callback)
          , baton(_baton) {};
        ~HunksWorker() {};
        void Execute();
        void OnOK();

      private:
        HunksBaton *baton;
    };

    static Napi::Value Hunks(const Napi::CallbackInfo& info);

    // delta methods
    static Napi::Value OldFile(const Napi::CallbackInfo& info);
    static Napi::Value NewFile(const Napi::CallbackInfo& info);

    // convenient status methods
    static Napi::Value Status(const Napi::CallbackInfo& info);
    static Napi::Value IsUnmodified(const Napi::CallbackInfo& info);
    static Napi::Value IsAdded(const Napi::CallbackInfo& info);
    static Napi::Value IsDeleted(const Napi::CallbackInfo& info);
    static Napi::Value IsModified(const Napi::CallbackInfo& info);
    static Napi::Value IsRenamed(const Napi::CallbackInfo& info);
    static Napi::Value IsCopied(const Napi::CallbackInfo& info);
    static Napi::Value IsIgnored(const Napi::CallbackInfo& info);
    static Napi::Value IsUntracked(const Napi::CallbackInfo& info);
    static Napi::Value IsTypeChange(const Napi::CallbackInfo& info);
    static Napi::Value IsUnreadable(const Napi::CallbackInfo& info);
    static Napi::Value IsConflicted(const Napi::CallbackInfo& info);

    // Hunk methods
};

#endif
