Napi::Value GitRevwalk::FileHistoryWalk(const Napi::CallbackInfo& info)
{
  if (info.Length() == 0 || !info[0].IsString()) {
    Napi::Error::New(env, "File path to get the history is required.").ThrowAsJavaScriptException();
    return env.Null();
  }

  if (info.Length() == 1 || !info[1].IsNumber()) {
    Napi::Error::New(env, "Max count is required and must be a number.").ThrowAsJavaScriptException();
    return env.Null();
  }

  if (info.Length() == 2 || !info[2].IsFunction()) {
    Napi::Error::New(env, "Callback is required and must be a Function.").ThrowAsJavaScriptException();
    return env.Null();
  }

  FileHistoryWalkBaton* baton = new FileHistoryWalkBaton;

  baton->error_code = GIT_OK;
  baton->error = NULL;
  Napi::String from_js_file_path(env, info[0].ToString());
  baton->file_path = strdup(*from_js_file_path);
  baton->max_count = Napi::To<unsigned int>(info[1]);
  baton->out = new std::vector< std::pair<git_commit *, std::pair<char *, git_delta_t> > *>;
  baton->out->reserve(baton->max_count);
  baton->walk = info.This())->GetValue(.Unwrap<GitRevwalk>();

  Napi::FunctionReference *callback = new Napi::FunctionReference(info[2].As<Napi::Function>());
  FileHistoryWalkWorker *worker = new FileHistoryWalkWorker(baton, callback);
  worker->SaveToPersistent("fileHistoryWalk", info.This());

  worker.Queue();
  return;
}

void GitRevwalk::FileHistoryWalkWorker::Execute()
{
  git_repository *repo = git_revwalk_repository(baton->walk);
  git_oid *nextOid = (git_oid *)malloc(sizeof(git_oid));
  giterr_clear();
  for (
    unsigned int i = 0;
    i < baton->max_count && (baton->error_code = git_revwalk_next(nextOid, baton->walk)) == GIT_OK;
    ++i
  ) {
    // check if this commit has the file
    git_commit *nextCommit;

    if ((baton->error_code = git_commit_lookup(&nextCommit, repo, nextOid)) != GIT_OK) {
      break;
    }

    git_tree *thisTree, *parentTree;
    if ((baton->error_code = git_commit_tree(&thisTree, nextCommit)) != GIT_OK) {
      git_commit_free(nextCommit);
      break;
    }

    git_diff *diffs;
    git_diff_options opts = GIT_DIFF_OPTIONS_INIT;
    char *file_path = strdup(baton->file_path);
    opts.pathspec.strings = &file_path;
    opts.pathspec.count = 1;
    git_commit *parent;
    unsigned int parents = git_commit_parentcount(nextCommit);
    if (parents > 1) {
      git_commit_free(nextCommit);
      continue;
    } else if (parents == 1) {
      if ((baton->error_code = git_commit_parent(&parent, nextCommit, 0)) != GIT_OK) {
        git_commit_free(nextCommit);
        break;
      }
      if (
        (baton->error_code = git_commit_tree(&parentTree, parent)) != GIT_OK ||
        (baton->error_code = git_diff_tree_to_tree(&diffs, repo, parentTree, thisTree, &opts)) != GIT_OK
      ) {
        git_commit_free(nextCommit);
        git_commit_free(parent);
        break;
      }
    } else {
      if ((baton->error_code = git_diff_tree_to_tree(&diffs, repo, NULL, thisTree, &opts)) != GIT_OK) {
        git_commit_free(nextCommit);
        break;
      }
    }

    free(file_path);
    opts.pathspec.strings = NULL;
    opts.pathspec.count = 0;

    bool flag = false;
    bool doRenamedPass = false;
    unsigned int numDeltas = git_diff_num_deltas(diffs);
    for (unsigned int j = 0; j < numDeltas; ++j) {
      git_patch *nextPatch;
      baton->error_code = git_patch_from_diff(&nextPatch, diffs, j);

      if (baton->error_code < GIT_OK) {
        break;
      }

      if (nextPatch == NULL) {
        continue;
      }

      const git_diff_delta *delta = git_patch_get_delta(nextPatch);
      bool isEqualOldFile = !strcmp(delta->old_file.path, baton->file_path);
      bool isEqualNewFile = !strcmp(delta->new_file.path, baton->file_path);

      if (isEqualNewFile) {
        if (delta->status == GIT_DELTA_ADDED || delta->status == GIT_DELTA_DELETED) {
          doRenamedPass = true;
          break;
        }
        std::pair<git_commit *, std::pair<char *, git_delta_t> > *historyEntry;
        if (!isEqualOldFile) {
          historyEntry = new std::pair<git_commit *, std::pair<char *, git_delta_t> >(
            nextCommit,
            std::pair<char *, git_delta_t>(strdup(delta->old_file.path), delta->status)
          );
        } else {
          historyEntry = new std::pair<git_commit *, std::pair<char *, git_delta_t> >(
            nextCommit,
            std::pair<char *, git_delta_t>(strdup(delta->new_file.path), delta->status)
          );
        }
        baton->out->push_back(historyEntry);
        flag = true;
      }

      git_patch_free(nextPatch);

      if (flag) {
        break;
      }
    }

    if (doRenamedPass) {
      git_diff_free(diffs);

      if (parents == 1) {
        if ((baton->error_code = git_diff_tree_to_tree(&diffs, repo, parentTree, thisTree, NULL)) != GIT_OK) {
          git_commit_free(nextCommit);
          break;
        }
        if ((baton->error_code = git_diff_find_similar(diffs, NULL)) != GIT_OK) {
          git_commit_free(nextCommit);
          break;
        }
      } else {
        if ((baton->error_code = git_diff_tree_to_tree(&diffs, repo, NULL, thisTree, NULL)) != GIT_OK) {
          git_commit_free(nextCommit);
          break;
        }
        if((baton->error_code = git_diff_find_similar(diffs, NULL)) != GIT_OK) {
          git_commit_free(nextCommit);
          break;
        }
      }

      flag = false;
      numDeltas = git_diff_num_deltas(diffs);
      for (unsigned int j = 0; j < numDeltas; ++j) {
        git_patch *nextPatch;
        baton->error_code = git_patch_from_diff(&nextPatch, diffs, j);

        if (baton->error_code < GIT_OK) {
          break;
        }

        if (nextPatch == NULL) {
          continue;
        }

        const git_diff_delta *delta = git_patch_get_delta(nextPatch);
        bool isEqualOldFile = !strcmp(delta->old_file.path, baton->file_path);
        bool isEqualNewFile = !strcmp(delta->new_file.path, baton->file_path);
        int oldLen = strlen(delta->old_file.path);
        int newLen = strlen(delta->new_file.path);
        char *outPair = new char[oldLen + newLen + 2];
        strcpy(outPair, delta->new_file.path);
        outPair[newLen] = '\n';
        outPair[newLen + 1] = '\0';
        strcat(outPair, delta->old_file.path);

        if (isEqualNewFile) {
          std::pair<git_commit *, std::pair<char *, git_delta_t> > *historyEntry;
          if (!isEqualOldFile) {
            historyEntry = new std::pair<git_commit *, std::pair<char *, git_delta_t> >(
              nextCommit,
              std::pair<char *, git_delta_t>(strdup(outPair), delta->status)
            );
          } else {
            historyEntry = new std::pair<git_commit *, std::pair<char *, git_delta_t> >(
              nextCommit,
              std::pair<char *, git_delta_t>(strdup(delta->new_file.path), delta->status)
            );
          }
          baton->out->push_back(historyEntry);
          flag = true;
        } else if (isEqualOldFile) {
          std::pair<git_commit *, std::pair<char *, git_delta_t> > *historyEntry;
          historyEntry = new std::pair<git_commit *, std::pair<char *, git_delta_t> >(
            nextCommit,
            std::pair<char *, git_delta_t>(strdup(outPair), delta->status)
          );
          baton->out->push_back(historyEntry);
          flag = true;
        }

        delete[] outPair;

        git_patch_free(nextPatch);

        if (flag) {
          break;
        }
      }
    }

    git_diff_free(diffs);

    if (!flag && nextCommit != NULL) {
      git_commit_free(nextCommit);
    }

    if (baton->error_code != GIT_OK) {
      break;
    }
  }

  free(nextOid);

  if (baton->error_code != GIT_OK) {
    if (baton->error_code != GIT_ITEROVER) {
      baton->error = git_error_dup(giterr_last());

      while(!baton->out->empty())
      {
        std::pair<git_commit *, std::pair<char *, git_delta_t> > *pairToFree = baton->out->back();
        baton->out->pop_back();
        git_commit_free(pairToFree->first);
        free(pairToFree->second.first);
        free(pairToFree);
      }

      delete baton->out;

      baton->out = NULL;
    }
  } else {
    baton->error_code = GIT_OK;
  }
}

void GitRevwalk::FileHistoryWalkWorker::OnOK()
{
  if (baton->out != NULL) {
    unsigned int size = baton->out->size();
    Napi::Array result = Napi::Array::New(env, size);
    for (unsigned int i = 0; i < size; i++) {
      Local<v8::Object> historyEntry = Napi::Object::New(env);
      std::pair<git_commit *, std::pair<char *, git_delta_t> > *batonResult = baton->out->at(i);
      (historyEntry).Set(Napi::String::New(env, "commit"), GitCommit::New(batonResult->first, true));
      (historyEntry).Set(Napi::String::New(env, "status"), Napi::Number::New(env, batonResult->second.second));
      if (batonResult->second.second == GIT_DELTA_RENAMED) {
        char *namePair = batonResult->second.first;
        char *split = strchr(namePair, '\n');
        *split = '\0';
        char *oldName = split + 1;

        (historyEntry).Set(Napi::String::New(env, "oldName"), Napi::New(env, oldName));
        (historyEntry).Set(Napi::String::New(env, "newName"), Napi::New(env, namePair));
      }
      (result).Set(Napi::Number::New(env, i), historyEntry);

      free(batonResult->second.first);
      free(batonResult);
    }

    Local<v8::Value> argv[2] = {
      env.Null(),
      result
    };
    callback->Call(2, argv, async_resource);

    delete baton->out;
    return;
  }

  if (baton->error) {
    Local<v8::Object> err;
    if (baton->error->message) {
      err = Napi::Error::New(env, baton->error->message)->ToObject();
    } else {
      err = Napi::Error::New(env, "Method fileHistoryWalk has thrown an error.")->ToObject();
    }
    err.Set(Napi::String::New(env, "errno"), Napi::New(env, baton->error_code));
    err.Set(Napi::String::New(env, "errorFunction"), Napi::String::New(env, "Revwalk.fileHistoryWalk"));
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
    Local<v8::Object> err = Napi::Error::New(env, "Method next has thrown an error.")->ToObject();
    err.Set(Napi::String::New(env, "errno"), Napi::New(env, baton->error_code));
    err.Set(Napi::String::New(env, "errorFunction"), Napi::String::New(env, "Revwalk.fileHistoryWalk"));
    Local<v8::Value> argv[1] = {
      err
    };
    callback->Call(1, argv, async_resource);
    return;
  }

  callback->Call(0, NULL, async_resource);
}
