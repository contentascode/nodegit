Napi::Value GitCommit::ExtractSignature(const Napi::CallbackInfo& info)
{
  if (info.Length() == 0 || !info[0].IsObject()) {
    Napi::Error::New(env, "Repository repo is required.").ThrowAsJavaScriptException();
    return env.Null();
  }

  if (info.Length() == 1 || (!info[1].IsObject() && !info[1].IsString())) {
    Napi::Error::New(env, "Oid commit_id is required.").ThrowAsJavaScriptException();
    return env.Null();
  }

  if (info.Length() == 2 || (info.Length() == 3 && !info[2].IsFunction())) {
    Napi::Error::New(env, "Callback is required and must be a Function.").ThrowAsJavaScriptException();
    return env.Null();
  }

  if (info.Length() >= 4) {
    if (!info[2].IsString() && !info[2].IsUndefined() && !info[2].IsNull()) {
      Napi::Error::New(env, "String signature_field must be a string or undefined/null.").ThrowAsJavaScriptException();
      return env.Null();
    }

    if (!info[3].IsFunction()) {
      Napi::Error::New(env, "Callback is required and must be a Function.").ThrowAsJavaScriptException();
      return env.Null();
    }
  }

  ExtractSignatureBaton* baton = new ExtractSignatureBaton;

  baton->error_code = GIT_OK;
  baton->error = NULL;
  baton->signature = GIT_BUF_INIT_CONST(NULL, 0);
  baton->signed_data = GIT_BUF_INIT_CONST(NULL, 0);
  baton->repo = info[0].ToObject())->GetValue(.Unwrap<GitRepository>();

  // baton->commit_id
  if (info[1].IsString()) {
    Napi::String oidString(env, info[1].ToString());
    baton->commit_id = (git_oid *)malloc(sizeof(git_oid));
    if (git_oid_fromstr(baton->commit_id, (const char *)strdup(*oidString)) != GIT_OK) {
      free(baton->commit_id);

      if (giterr_last()) {
        Napi::Error::New(env, giterr_last()->message).ThrowAsJavaScriptException();
        return env.Null();
      } else {
        Napi::Error::New(env, "Unknown Error").ThrowAsJavaScriptException();
        return env.Null();
      }
    }
  } else {
    baton->commit_id = info[1].ToObject())->GetValue(.Unwrap<GitOid>();
  }

  // baton->field
  if (info[2].IsString()) {
    Napi::String field(env, info[2].ToString());
    baton->field = (char *)malloc(field.Length() + 1);
    memcpy((void *)baton->field, *field, field.Length());
    baton->field[field.Length()] = 0;
  } else {
    baton->field = NULL;
  }

  Napi::FunctionReference *callback;
  if (info[2].IsFunction()) {
    callback = new Napi::FunctionReference(info[2].As<Napi::Function>());
  } else {
    callback = new Napi::FunctionReference(info[3].As<Napi::Function>());
  }

  ExtractSignatureWorker *worker = new ExtractSignatureWorker(baton, callback);
  worker->SaveToPersistent("repo", info[0].ToObject());
  worker->SaveToPersistent("commit_id", info[1].ToObject());
  worker.Queue();
  return;
}

void GitCommit::ExtractSignatureWorker::Execute()
{
  giterr_clear();

  {
    LockMaster lockMaster(
      /*asyncAction: */true,
      baton->repo
    );

    baton->error_code = git_commit_extract_signature(
      &baton->signature,
      &baton->signed_data,
      baton->repo,
      baton->commit_id,
      (const char *)baton->field
    );

    if (baton->error_code != GIT_OK && giterr_last() != NULL) {
      baton->error = git_error_dup(giterr_last());
    }
  }
}

void GitCommit::ExtractSignatureWorker::OnOK()
{
  if (baton->error_code == GIT_OK)
  {
    Local<v8::Object> result = Napi::Object::New(env);
    (
      result).Set(Napi::String::New(env, "signature"),
      Napi::String::New(env, baton->signature.ptr, baton->signature.size)
    );
    (
      result).Set(Napi::String::New(env, "signedData"),
      Napi::String::New(env, baton->signed_data.ptr, baton->signed_data.size)
    );

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
    Local<v8::Object> err = Napi::Error::New(env, "Extract Signature has thrown an error.")->ToObject();
    err.Set(Napi::String::New(env, "errno"), Napi::New(env, baton->error_code));
    err.Set(Napi::String::New(env, "errorFunction"), Napi::String::New(env, "Commit.extractSignature"));
    Local<v8::Value> argv[1] = {
      err
    };
    callback->Call(1, argv, async_resource);
  }
  else
  {
    callback->Call(0, NULL, async_resource);
  }

  git_buf_free(&baton->signature);
  git_buf_free(&baton->signed_data);

  if (baton->field != NULL) {
    free((void *)baton->field);
  }

  delete baton;
}
