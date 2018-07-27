Napi::Value GitPatch::ConvenientFromDiff(const Napi::CallbackInfo& info) {
  if (info.Length() == 0 || !info[0].IsObject()) {
    Napi::Error::New(env, "Diff diff is required.").ThrowAsJavaScriptException();
    return env.Null();
  }

  if (info.Length() == 1 || !info[1].IsFunction()) {
    Napi::Error::New(env, "Callback is required and must be a Function.").ThrowAsJavaScriptException();
    return env.Null();
  }

  ConvenientFromDiffBaton *baton = new ConvenientFromDiffBaton;

  baton->error_code = GIT_OK;
  baton->error = NULL;

  baton->diff = info[0].ToObject())->GetValue(.Unwrap<GitDiff>();
  baton->out = new std::vector<PatchData *>;
  baton->out->reserve(git_diff_num_deltas(baton->diff));

  Napi::FunctionReference *callback = new Napi::FunctionReference(info[1].As<Napi::Function>());
  ConvenientFromDiffWorker *worker = new ConvenientFromDiffWorker(baton, callback);

  worker->SaveToPersistent("diff", info[0]);

  worker.Queue();
  return;
}

void GitPatch::ConvenientFromDiffWorker::Execute() {
  giterr_clear();

  {
    LockMaster lockMaster(true, baton->diff);
    std::vector<git_patch *> patchesToBeFreed;

    for (int i = 0; i < git_diff_num_deltas(baton->diff); ++i) {
      git_patch *nextPatch;
      int result = git_patch_from_diff(&nextPatch, baton->diff, i);

      if (result) {
        while (!patchesToBeFreed.empty())
        {
          git_patch_free(patchesToBeFreed.back());
          patchesToBeFreed.pop_back();
        }

        while (!baton->out->empty()) {
          PatchDataFree(baton->out->back());
          baton->out->pop_back();
        }

        baton->error_code = result;

        if (giterr_last() != NULL) {
          baton->error = git_error_dup(giterr_last());
        }

        delete baton->out;
        baton->out = NULL;

        return;
      }

      if (nextPatch != NULL) {
        baton->out->push_back(createFromRaw(nextPatch));
        patchesToBeFreed.push_back(nextPatch);
      }
    }

    while (!patchesToBeFreed.empty())
    {
      git_patch_free(patchesToBeFreed.back());
      patchesToBeFreed.pop_back();
    }
  }
}

void GitPatch::ConvenientFromDiffWorker::OnOK() {
  if (baton->out != NULL) {
    unsigned int size = baton->out->size();
    Napi::Array result = Napi::Array::New(env, size);

    for (unsigned int i = 0; i < size; ++i) {
      (result).Set(Napi::Number::New(env, i), ConvenientPatch::New((void *)baton->out->at(i)));
    }

    delete baton->out;

    Local<v8::Value> argv[2] = {
      env.Null(),
      result
    };
    callback->Call(2, argv, async_resource);

    return;
  }

  if (baton->error) {
    Local<v8::Object> err;
    if (baton->error->message) {
      err = Napi::Error::New(env, baton->error->message)->ToObject();
    } else {
      err = Napi::Error::New(env, "Method convenientFromDiff has thrown an error.")->ToObject();
    }
    err.Set(Napi::String::New(env, "errno"), Napi::New(env, baton->error_code));
    err.Set(Napi::String::New(env, "errorFunction"), Napi::String::New(env, "Patch.convenientFromDiff"));
    Local<v8::Value> argv[1] = {
      err
    };
    callback->Call(1, argv, async_resource);
    if (baton->error->message)
    {
      free((void *)baton->error->message);
    }

    free((void *)baton->error);

    return;
  }

  if (baton->error_code < 0) {
    Local<v8::Object> err = Napi::Error::New(env, "method convenientFromDiff has thrown an error.")->ToObject();
    err.Set(Napi::String::New(env, "errno"), Napi::New(env, baton->error_code));
    err.Set(Napi::String::New(env, "errorFunction"), Napi::String::New(env, "Patch.convenientFromDiff"));
    Local<v8::Value> argv[1] = {
      err
    };
    callback->Call(1, argv, async_resource);

    return;
  }

  callback->Call(0, NULL);
}
