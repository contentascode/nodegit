Napi::Value GitRemote::ReferenceList(const Napi::CallbackInfo& info)
{
  if (info.Length() == 0 || !info[0].IsFunction()) {
    Napi::Error::New(env, "Callback is required and must be a Function.").ThrowAsJavaScriptException();
    return env.Null();
  }

  ReferenceListBaton* baton = new ReferenceListBaton;

  baton->error_code = GIT_OK;
  baton->error = NULL;
  baton->out = new std::vector<git_remote_head*>;
  baton->remote = info.This())->GetValue(.Unwrap<GitRemote>();

  Napi::FunctionReference *callback = new Napi::FunctionReference(info[0].As<Napi::Function>());
  ReferenceListWorker *worker = new ReferenceListWorker(baton, callback);
  worker->SaveToPersistent("remote", info.This());
  worker.Queue();
  return;
}

void GitRemote::ReferenceListWorker::Execute()
{
  giterr_clear();

  {
    LockMaster lockMaster(
      /*asyncAction: */true,
      baton->remote
    );

    const git_remote_head **remote_heads;
    size_t num_remote_heads;
    baton->error_code = git_remote_ls(
      &remote_heads,
      &num_remote_heads,
      baton->remote
    );

    if (baton->error_code != GIT_OK) {
      baton->error = git_error_dup(giterr_last());
      delete baton->out;
      baton->out = NULL;
      return;
    }

    baton->out->reserve(num_remote_heads);

    for (size_t head_index = 0; head_index < num_remote_heads; ++head_index) {
      git_remote_head *remote_head = git_remote_head_dup(remote_heads[head_index]);
      baton->out->push_back(remote_head);
    }
  }
}

void GitRemote::ReferenceListWorker::OnOK()
{
  if (baton->out != NULL)
  {
    unsigned int size = baton->out->size();
    Napi::Array result = Napi::Array::New(env, size);
    for (unsigned int i = 0; i < size; i++) {
      (result).Set(Napi::Number::New(env, i), GitRemoteHead::New(baton->out->at(i), true));
    }

    delete baton->out;

    Local<v8::Value> argv[2] = {
      env.Null(),
      result
    };
    callback->Call(2, argv, async_resource);
  }
  else if (baton->error)
  {
    Local<v8::Value> argv[1] = {
      Napi::Error::New(env, baton->error->message)
    };
    callback->Call(1, argv, async_resource);
    if (baton->error->message)
    {
      free((void *)baton->error->message);
    }

    free((void *)baton->error);
  }
  else if (baton->error_code < 0)
  {
    Local<v8::Object> err = Napi::Error::New(env, "Reference List has thrown an error.")->ToObject();
    err.Set(Napi::String::New(env, "errno"), Napi::New(env, baton->error_code));
    err.Set(Napi::String::New(env, "errorFunction"), Napi::String::New(env, "Remote.referenceList"));
    Local<v8::Value> argv[1] = {
      err
    };
    callback->Call(1, argv, async_resource);
  }
  else
  {
    callback->Call(0, NULL, async_resource);
  }
}
