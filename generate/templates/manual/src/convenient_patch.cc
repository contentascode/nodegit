#include <napi.h>
#include <uv.h>
#include <string.h>

extern "C" {
  #include <git2.h>
}

#include "../include/convenient_hunk.h"
#include "../include/convenient_patch.h"
#include "../include/functions/copy.h"
#include "../include/diff_file.h"

using namespace std;
using namespace Napi;

void PatchDataFree(PatchData *patch) {
  free((void *)patch->old_file.path);
  free((void *)patch->new_file.path);
  while(!patch->hunks->empty()) {
    HunkData *hunk = patch->hunks->back();
    patch->hunks->pop_back();
    while (!hunk->lines->empty()) {
      git_diff_line *line = hunk->lines->back();
      hunk->lines->pop_back();
      free((void *)line->content);
      free((void *)line);
    }
    delete hunk;
  }
  delete patch;
}

PatchData *createFromRaw(git_patch *raw) {
  PatchData *patch = new PatchData;
  const git_diff_delta *delta = git_patch_get_delta(raw);

  patch->status = delta->status;

  patch->old_file = delta->old_file;
  patch->old_file.path = strdup(delta->old_file.path);

  patch->new_file = delta->new_file;
  patch->new_file.path = strdup(delta->new_file.path);

  git_patch_line_stats(
    &patch->lineStats.context,
    &patch->lineStats.additions,
    &patch->lineStats.deletions,
    raw
  );

  patch->numHunks = git_patch_num_hunks(raw);
  patch->hunks = new std::vector<HunkData *>;
  patch->hunks->reserve(patch->numHunks);

  for (unsigned int i = 0; i < patch->numHunks; ++i) {
    HunkData *hunkData = new HunkData;
    const git_diff_hunk *hunk = NULL;
    int result = git_patch_get_hunk(&hunk, &hunkData->numLines, raw, i);
    if (result != 0) {
      continue;
    }

    hunkData->hunk.old_start = hunk->old_start;
    hunkData->hunk.old_lines = hunk->old_lines;
    hunkData->hunk.new_start = hunk->new_start;
    hunkData->hunk.new_lines = hunk->new_lines;
    hunkData->hunk.header_len = hunk->header_len;
    memcpy(&hunkData->hunk.header, &hunk->header, 128);

    hunkData->lines = new std::vector<git_diff_line *>;
    hunkData->lines->reserve(hunkData->numLines);

    static const int noNewlineStringLength = 29;
    bool EOFFlag = false;
    for (unsigned int j = 0; j < hunkData->numLines; ++j) {
      git_diff_line *storeLine = (git_diff_line *)malloc(sizeof(git_diff_line));
      const git_diff_line *line = NULL;
      int result = git_patch_get_line_in_hunk(&line, raw, i, j);
      if (result != 0) {
        continue;
      }

      if (j == 0) {
        int calculatedContentLength = line->content_len;
        if (
          calculatedContentLength > noNewlineStringLength &&
          !strncmp(
              &line->content[calculatedContentLength - noNewlineStringLength],
              "\n\\ No newline at end of file\n", (std::min)(calculatedContentLength, noNewlineStringLength)
        )) {
          EOFFlag = true;
        }
      }

      storeLine->origin = line->origin;
      storeLine->old_lineno = line->old_lineno;
      storeLine->new_lineno = line->new_lineno;
      storeLine->num_lines = line->num_lines;
      storeLine->content_len = line->content_len;
      storeLine->content_offset = line->content_offset;
      char * transferContent;
      if (EOFFlag) {
        transferContent = (char *)malloc(storeLine->content_len + noNewlineStringLength + 1);
        memcpy(transferContent, line->content, storeLine->content_len);
        memcpy(transferContent + storeLine->content_len, "\n\\ No newline at end of file\n", noNewlineStringLength);
        transferContent[storeLine->content_len + noNewlineStringLength] = '\0';
      } else {
        transferContent = (char *)malloc(storeLine->content_len + 1);
        memcpy(transferContent, line->content, storeLine->content_len);
        transferContent[storeLine->content_len] = '\0';
      }
      storeLine->content = strdup(transferContent);
      free((void *)transferContent);
      hunkData->lines->push_back(storeLine);
    }
    patch->hunks->push_back(hunkData);
  }

  return patch;
}

ConvenientPatch::ConvenientPatch(PatchData *raw) {
  this->patch = raw;
}

ConvenientPatch::~ConvenientPatch() {
  PatchDataFree(this->patch);
}

void ConvenientPatch::InitializeComponent(Local<v8::Object> target) {
  Napi::HandleScope scope(env);

  Napi::FunctionReference tpl = Napi::Function::New(env, JSNewFunction);


  tpl->SetClassName(Napi::String::New(env, "ConvenientPatch"));

  InstanceMethod("hunks", &Hunks),
  InstanceMethod("lineStats", &LineStats),
  InstanceMethod("size", &Size),

  InstanceMethod("oldFile", &OldFile),
  InstanceMethod("newFile", &NewFile),
  InstanceMethod("status", &Status),
  InstanceMethod("isUnmodified", &IsUnmodified),
  InstanceMethod("isAdded", &IsAdded),
  InstanceMethod("isDeleted", &IsDeleted),
  InstanceMethod("isModified", &IsModified),
  InstanceMethod("isRenamed", &IsRenamed),
  InstanceMethod("isCopied", &IsCopied),
  InstanceMethod("isIgnored", &IsIgnored),
  InstanceMethod("isUntracked", &IsUntracked),
  InstanceMethod("isTypeChange", &IsTypeChange),
  InstanceMethod("isUnreadable", &IsUnreadable),
  InstanceMethod("isConflicted", &IsConflicted),

  Napi::Function _constructor = Napi::GetFunction(tpl);
  constructor.Reset(_constructor);
  (target).Set(Napi::String::New(env, "ConvenientPatch"), _constructor);
}

Napi::Value ConvenientPatch::JSNewFunction(const Napi::CallbackInfo& info) {

  if (info.Length() == 0 || !info[0].IsExternal()) {
       Napi::Error::New(env, "A new ConvenientPatch cannot be instantiated.").ThrowAsJavaScriptException();
       return env.Null();
   }

  ConvenientPatch* object = new ConvenientPatch(static_cast<PatchData *>(info[0].As<Napi::External>()->Value()));
  object->Wrap(info.This());

  return info.This();
}

Local<v8::Value> ConvenientPatch::New(void *raw) {
  Napi::EscapableHandleScope scope(env);
  Local<v8::Value> argv[1] = { Napi::External::New(env, (void *)raw) };
  return scope.Escape(Napi::NewInstance(Napi::New(env, ConvenientPatch::constructor), 1, argv));
}

ConvenientLineStats ConvenientPatch::GetLineStats() {
  return this->patch->lineStats;
}

git_delta_t ConvenientPatch::GetStatus() {
  return this->patch->status;
}

git_diff_file ConvenientPatch::GetOldFile() {
  return this->patch->old_file;
}

git_diff_file ConvenientPatch::GetNewFile() {
  return this->patch->new_file;
}

size_t ConvenientPatch::GetNumHunks() {
  return this->patch->numHunks;
}

PatchData *ConvenientPatch::GetValue() {
  return this->patch;
}

Napi::Value ConvenientPatch::Hunks(const Napi::CallbackInfo& info) {
  if (info.Length() == 0 || !info[0].IsFunction()) {
    Napi::Error::New(env, "Callback is required and must be a Function.").ThrowAsJavaScriptException();
    return env.Null();
  }

  HunksBaton *baton = new HunksBaton;

  baton->patch = info.This())->GetValue(.Unwrap<ConvenientPatch>();

  Napi::FunctionReference *callback = new Napi::FunctionReference(info[0].As<Napi::Function>());
  HunksWorker *worker = new HunksWorker(baton, callback);

  worker->SaveToPersistent("patch", info.This());

  worker.Queue();
  return;
}

void ConvenientPatch::HunksWorker::Execute() {
  // copy hunks
  baton->hunks = new std::vector<HunkData *>;
  baton->hunks->reserve(baton->patch->numHunks);

  for (unsigned int i = 0; i < baton->patch->numHunks; ++i) {
    HunkData *hunkData = new HunkData;
    hunkData->numLines = baton->patch->hunks->at(i)->numLines;
    hunkData->hunk.old_start = baton->patch->hunks->at(i)->hunk.old_start;
    hunkData->hunk.old_lines = baton->patch->hunks->at(i)->hunk.old_lines;
    hunkData->hunk.new_start = baton->patch->hunks->at(i)->hunk.new_start;
    hunkData->hunk.new_lines = baton->patch->hunks->at(i)->hunk.new_lines;
    hunkData->hunk.header_len = baton->patch->hunks->at(i)->hunk.header_len;
    memcpy(&hunkData->hunk.header, &baton->patch->hunks->at(i)->hunk.header, 128);

    hunkData->lines = new std::vector<git_diff_line *>;
    hunkData->lines->reserve(hunkData->numLines);

    for (unsigned int j = 0; j < hunkData->numLines; ++j) {
      git_diff_line *storeLine = (git_diff_line *)malloc(sizeof(git_diff_line));
      storeLine->origin = baton->patch->hunks->at(i)->lines->at(j)->origin;
      storeLine->old_lineno = baton->patch->hunks->at(i)->lines->at(j)->old_lineno;
      storeLine->new_lineno = baton->patch->hunks->at(i)->lines->at(j)->new_lineno;
      storeLine->num_lines = baton->patch->hunks->at(i)->lines->at(j)->num_lines;
      storeLine->content_len = baton->patch->hunks->at(i)->lines->at(j)->content_len;
      storeLine->content_offset = baton->patch->hunks->at(i)->lines->at(j)->content_offset;
      storeLine->content = strdup(baton->patch->hunks->at(i)->lines->at(j)->content);
      hunkData->lines->push_back(storeLine);
    }
    baton->hunks->push_back(hunkData);
  }
}

void ConvenientPatch::HunksWorker::OnOK() {
  unsigned int size = baton->hunks->size();
  Napi::Array result = Napi::Array::New(env, size);

  for(unsigned int i = 0; i < size; ++i) {
    (result).Set(Napi::Number::New(env, i), ConvenientHunk::New(baton->hunks->at(i)));
  }

  delete baton->hunks;

  Local<v8::Value> argv[2] = {
    env.Null(),
    result
  };
  callback->Call(2, argv, async_resource);
}

Napi::Value ConvenientPatch::LineStats(const Napi::CallbackInfo& info) {
  Napi::EscapableHandleScope scope(env);

  Local<v8::Value> to;
  Napi::Object toReturn = Napi::Object::New(env);
  ConvenientLineStats stats = info.This())->GetLineStats(.Unwrap<ConvenientPatch>();

  to = Napi::Number::New(env, stats.context);
  (toReturn).Set(Napi::String::New(env, "total_context"), to);
  to = Napi::Number::New(env, stats.additions);
  (toReturn).Set(Napi::String::New(env, "total_additions"), to);
  to = Napi::Number::New(env, stats.deletions);
  (toReturn).Set(Napi::String::New(env, "total_deletions"), to);

  return return scope.Escape(toReturn);
}

Napi::Value ConvenientPatch::Size(const Napi::CallbackInfo& info) {
  Local<v8::Value> to;

  to = Napi::Number::New(env, info.This())->GetNumHunks().Unwrap<ConvenientPatch>();

  return to;
}

Napi::Value ConvenientPatch::OldFile(const Napi::CallbackInfo& info) {
  Napi::EscapableHandleScope scope(env);

  Local<v8::Value> to;
  git_diff_file *old_file = (git_diff_file *)malloc(sizeof(git_diff_file));
  *old_file = info.This())->GetOldFile(.Unwrap<ConvenientPatch>();

  to = GitDiffFile::New(old_file, true);

  return return to;
}

Napi::Value ConvenientPatch::NewFile(const Napi::CallbackInfo& info) {
  Napi::EscapableHandleScope scope(env);

  Local<v8::Value> to;
  git_diff_file *new_file = (git_diff_file *)malloc(sizeof(git_diff_file));
  *new_file = info.This())->GetNewFile(.Unwrap<ConvenientPatch>();
  if (new_file != NULL) {
    to = GitDiffFile::New(new_file, true);
  } else {
    to = env.Null();
  }

  return return to;
}

Napi::Value ConvenientPatch::Status(const Napi::CallbackInfo& info) {
  Local<v8::Value> to;
  to = Napi::Number::New(env, info.This())->GetStatus().Unwrap<ConvenientPatch>();
  return to;
}

Napi::Value ConvenientPatch::IsUnmodified(const Napi::CallbackInfo& info) {
  Napi::EscapableHandleScope scope(env);

  Local<v8::Value> to;
  to = Napi::Boolean::New(env, info.This())->GetStatus() == GIT_DELTA_UNMODIFIED.Unwrap<ConvenientPatch>();
  return to;
}

Napi::Value ConvenientPatch::IsAdded(const Napi::CallbackInfo& info) {
  Local<v8::Value> to;
  to = Napi::Boolean::New(env, info.This())->GetStatus() == GIT_DELTA_ADDED.Unwrap<ConvenientPatch>();
  return to;
}

Napi::Value ConvenientPatch::IsDeleted(const Napi::CallbackInfo& info) {
  Local<v8::Value> to;
  to = Napi::Boolean::New(env, info.This())->GetStatus() == GIT_DELTA_DELETED.Unwrap<ConvenientPatch>();
  return to;
}

Napi::Value ConvenientPatch::IsModified(const Napi::CallbackInfo& info) {
  Local<v8::Value> to;
  to = Napi::Boolean::New(env, info.This())->GetStatus() == GIT_DELTA_MODIFIED.Unwrap<ConvenientPatch>();
  return to;
}

Napi::Value ConvenientPatch::IsRenamed(const Napi::CallbackInfo& info) {
  Local<v8::Value> to;
  to = Napi::Boolean::New(env, info.This())->GetStatus() == GIT_DELTA_RENAMED.Unwrap<ConvenientPatch>();
  return to;
}

Napi::Value ConvenientPatch::IsCopied(const Napi::CallbackInfo& info) {
  Local<v8::Value> to;
  to = Napi::Boolean::New(env, info.This())->GetStatus() == GIT_DELTA_COPIED.Unwrap<ConvenientPatch>();
  return to;
}

Napi::Value ConvenientPatch::IsIgnored(const Napi::CallbackInfo& info) {
  Local<v8::Value> to;
  to = Napi::Boolean::New(env, info.This())->GetStatus() == GIT_DELTA_IGNORED.Unwrap<ConvenientPatch>();
  return to;
}

Napi::Value ConvenientPatch::IsUntracked(const Napi::CallbackInfo& info) {
  Local<v8::Value> to;
  to = Napi::Boolean::New(env, info.This())->GetStatus() == GIT_DELTA_UNTRACKED.Unwrap<ConvenientPatch>();
  return to;
}

Napi::Value ConvenientPatch::IsTypeChange(const Napi::CallbackInfo& info) {
  Local<v8::Value> to;
  to = Napi::Boolean::New(env, info.This())->GetStatus() == GIT_DELTA_TYPECHANGE.Unwrap<ConvenientPatch>();
  return to;
}

Napi::Value ConvenientPatch::IsUnreadable(const Napi::CallbackInfo& info) {
  Local<v8::Value> to;
  to = Napi::Boolean::New(env, info.This())->GetStatus() == GIT_DELTA_UNREADABLE.Unwrap<ConvenientPatch>();
  return to;
}

Napi::Value ConvenientPatch::IsConflicted(const Napi::CallbackInfo& info) {
  Local<v8::Value> to;
  to = Napi::Boolean::New(env, info.This())->GetStatus() == GIT_DELTA_CONFLICTED.Unwrap<ConvenientPatch>();
  return to;
}

Napi::FunctionReference ConvenientPatch::constructor;
