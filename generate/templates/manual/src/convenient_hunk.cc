#include <napi.h>
#include <uv.h>
#include <string.h>

extern "C" {
  #include <git2.h>
}

#include "../include/functions/copy.h"
#include "../include/convenient_hunk.h"
#include "../include/diff_line.h"

using namespace std;
using namespace Napi;

void HunkDataFree(HunkData *hunk) {
  while (!hunk->lines->empty()) {
    git_diff_line *line = hunk->lines->back();
    hunk->lines->pop_back();
    free((void *)line->content);
    free((void *)line);
  }
  delete hunk->lines;
  delete hunk;
}

ConvenientHunk::ConvenientHunk(HunkData *raw) {
  this->hunk = raw;
}

ConvenientHunk::~ConvenientHunk() {
  HunkDataFree(this->hunk);
}

void ConvenientHunk::InitializeComponent(Local<v8::Object> target) {
  Napi::HandleScope scope(env);

  Napi::FunctionReference tpl = Napi::Function::New(env, JSNewFunction);


  tpl->SetClassName(Napi::String::New(env, "ConvenientHunk"));

  InstanceMethod("size", &Size),
  InstanceMethod("lines", &Lines),

  InstanceMethod("oldStart", &OldStart),
  InstanceMethod("oldLines", &OldLines),
  InstanceMethod("newStart", &NewStart),
  InstanceMethod("newLines", &NewLines),
  InstanceMethod("headerLen", &HeaderLen),
  InstanceMethod("header", &Header),

  Napi::Function _constructor = Napi::GetFunction(tpl);
  constructor.Reset(_constructor);
  (target).Set(Napi::String::New(env, "ConvenientHunk"), _constructor);
}

Napi::Value ConvenientHunk::JSNewFunction(const Napi::CallbackInfo& info) {

  if (info.Length() == 0 || !info[0].IsExternal()) {
       Napi::Error::New(env, "A new ConvenientHunk cannot be instantiated.").ThrowAsJavaScriptException();
       return env.Null();
   }

  ConvenientHunk* object = new ConvenientHunk(static_cast<HunkData *>(info[0].As<Napi::External>()->Value()));
  object->Wrap(info.This());

  return info.This();
}

Local<v8::Value> ConvenientHunk::New(void *raw) {
  Napi::EscapableHandleScope scope(env);
  Local<v8::Value> argv[1] = { Napi::External::New(env, (void *)raw) };
  return scope.Escape(Napi::NewInstance(Napi::New(env, ConvenientHunk::constructor), 1, argv));
}

HunkData *ConvenientHunk::GetValue() {
  return this->hunk;
}

size_t ConvenientHunk::GetSize() {
  return this->hunk->numLines;
}

Napi::Value ConvenientHunk::Size(const Napi::CallbackInfo& info) {
  Local<v8::Value> to;
  to = Napi::Number::New(env, info.This())->GetSize().Unwrap<ConvenientHunk>();
  return to;
}

Napi::Value ConvenientHunk::Lines(const Napi::CallbackInfo& info) {
  if (info.Length() == 0 || !info[0].IsFunction()) {
    Napi::Error::New(env, "Callback is required and must be a Function.").ThrowAsJavaScriptException();
    return env.Null();
  }

  LinesBaton *baton = new LinesBaton;

  baton->hunk = info.This())->GetValue(.Unwrap<ConvenientHunk>();

  Napi::FunctionReference *callback = new Napi::FunctionReference(info[0].As<Napi::Function>());
  LinesWorker *worker = new LinesWorker(baton, callback);

  worker->SaveToPersistent("hunk", info.This());

  worker.Queue();
  return;
}

void ConvenientHunk::LinesWorker::Execute() {
  baton->lines = new std::vector<git_diff_line *>;
  baton->lines->reserve(baton->hunk->numLines);
  for (unsigned int i = 0; i < baton->hunk->numLines; ++i) {
    git_diff_line *storeLine = (git_diff_line *)malloc(sizeof(git_diff_line));
    storeLine->origin = baton->hunk->lines->at(i)->origin;
    storeLine->old_lineno = baton->hunk->lines->at(i)->old_lineno;
    storeLine->new_lineno = baton->hunk->lines->at(i)->new_lineno;
    storeLine->num_lines = baton->hunk->lines->at(i)->num_lines;
    storeLine->content_len = baton->hunk->lines->at(i)->content_len;
    storeLine->content_offset = baton->hunk->lines->at(i)->content_offset;
    storeLine->content = strdup(baton->hunk->lines->at(i)->content);
    baton->lines->push_back(storeLine);
  }
}

void ConvenientHunk::LinesWorker::OnOK() {
  unsigned int size = baton->lines->size();
  Napi::Array result = Napi::Array::New(env, size);

  for(unsigned int i = 0; i < size; ++i) {
    (result).Set(Napi::Number::New(env, i), GitDiffLine::New(baton->lines->at(i), true));
  }

  delete baton->lines;

  Local<v8::Value> argv[2] = {
    env.Null(),
    result
  };
  callback->Call(2, argv, async_resource);
}

Napi::Value ConvenientHunk::OldStart(const Napi::CallbackInfo& info) {
  Local<v8::Value> to;
  int old_start = Napi::ObjectWrap::Unwrap<ConvenientHunk>(info.This())->GetValue()->hunk.old_start;
  return Napi::Number::New(env, old_start);
}


Napi::Value ConvenientHunk::OldLines(const Napi::CallbackInfo& info) {
  Local<v8::Value> to;
  int old_lines = Napi::ObjectWrap::Unwrap<ConvenientHunk>(info.This())->GetValue()->hunk.old_lines;
  return Napi::Number::New(env, old_lines);
}

Napi::Value ConvenientHunk::NewStart(const Napi::CallbackInfo& info) {
  Local<v8::Value> to;
  int new_start = Napi::ObjectWrap::Unwrap<ConvenientHunk>(info.This())->GetValue()->hunk.new_start;
  return Napi::Number::New(env, new_start);
}

Napi::Value ConvenientHunk::NewLines(const Napi::CallbackInfo& info) {
  Local<v8::Value> to;
  int new_lines = Napi::ObjectWrap::Unwrap<ConvenientHunk>(info.This())->GetValue()->hunk.new_lines;
  return Napi::Number::New(env, new_lines);
}

Napi::Value ConvenientHunk::HeaderLen(const Napi::CallbackInfo& info) {
  Local<v8::Value> to;
  size_t header_len = Napi::ObjectWrap::Unwrap<ConvenientHunk>(info.This())->GetValue()->hunk.header_len;
  return Napi::Number::New(env, header_len);
}

Napi::Value ConvenientHunk::Header(const Napi::CallbackInfo& info) {
  Local<v8::Value> to;

  char *header = Napi::ObjectWrap::Unwrap<ConvenientHunk>(info.This())->GetValue()->hunk.header;
  if (header) {
    to = Napi::String::New(env, header);
  } else {
    to = env.Null();
  }

  return to;
}

Napi::FunctionReference ConvenientHunk::constructor;
