template<typename Traits>
NodeGitWrapper<Traits>::NodeGitWrapper(typename Traits::cType *raw, bool selfFreeing, Napi::Object owner) {
  if (!owner.IsEmpty()) {
    // if we have an owner, there are two options - either we duplicate the raw object
    // (so we own the duplicate, and can self-free it)
    // or we keep a handle on the owner so it doesn't get garbage collected
    // while this wrapper is accessible
    if(Traits::isDuplicable) {
      Traits::duplicate(&this->raw, raw);
      selfFreeing = true;
    } else {
      this->owner.Reset(owner);
      this->raw = raw;
      selfFreeing = false;
    }
  } else {
    this->raw = raw;
  }
  this->selfFreeing = selfFreeing;

  if (selfFreeing) {
    SelfFreeingInstanceCount++;
  } else {
    NonSelfFreeingConstructedCount++;
  }
}

template<typename Traits>
NodeGitWrapper<Traits>::NodeGitWrapper(const char *error) {
  selfFreeing = false;
  raw = NULL;
  Napi::Error::New(env, error).ThrowAsJavaScriptException();

}

template<typename Traits>
NodeGitWrapper<Traits>::~NodeGitWrapper() {
  if(Traits::isFreeable && selfFreeing) {
    Traits::free(raw);
    SelfFreeingInstanceCount--;
    raw = NULL;
  }
}

template<typename Traits>
NAN_METHOD(NodeGitWrapper<Traits>::JSNewFunction) {
  cppClass * instance;

  if (info.Length() == 0 || !info[0].IsExternal()) {
    Napi::TryCatch tryCatch;
    instance = new cppClass();
    // handle the case where the default constructor is not supported
    if(tryCatch.HasCaught()) {
      delete instance;
      tryCatch.ReThrow();
      return;
    }
  } else {
    instance = new cppClass(static_cast<cType *>(
      info[0].As<Napi::External>()->Value()),
      info[1].As<Napi::Boolean>().Value(),
      info.Length() >= 3 && !info[2].IsEmpty() && info[2].IsObject() ? info[2].ToObject() : Local<v8::Object>()
    );
  }

  instance->Wrap(info.This());
  return info.This();
}

template<typename Traits>
Napi::Value NodeGitWrapper<Traits>::New(const typename Traits::cType *raw, bool selfFreeing, Napi::Object owner) {
  Napi::EscapableHandleScope scope(env);
  Local<v8::Value> argv[3] = { Napi::External::New(env, (void *)raw), Napi::New(env, selfFreeing), owner };
  return scope.Escape(
    Napi::NewInstance(
      Napi::New(env, constructor),
      owner.IsEmpty() ? 2 : 3, // passing an empty handle as part of the arguments causes a crash
      argv
    ));
}

template<typename Traits>
typename Traits::cType *NodeGitWrapper<Traits>::GetValue() {
  return raw;
}

template<typename Traits>
void NodeGitWrapper<Traits>::ClearValue() {
  raw = NULL;
}

template<typename Traits>
Napi::FunctionReference NodeGitWrapper<Traits>::constructor;

template<typename Traits>
int NodeGitWrapper<Traits>::SelfFreeingInstanceCount;

template<typename Traits>
int NodeGitWrapper<Traits>::NonSelfFreeingConstructedCount;

template<typename Traits>
NAN_METHOD(NodeGitWrapper<Traits>::GetSelfFreeingInstanceCount) {
  return SelfFreeingInstanceCount;
}

template<typename Traits>
NAN_METHOD(NodeGitWrapper<Traits>::GetNonSelfFreeingConstructedCount) {
  return NonSelfFreeingConstructedCount;
}

template<typename Traits>
void NodeGitWrapper<Traits>::InitializeTemplate(Napi::FunctionReference &tpl) {
  Napi::SetMethod(tpl, "getSelfFreeingInstanceCount", GetSelfFreeingInstanceCount);
  Napi::SetMethod(tpl, "getNonSelfFreeingConstructedCount", GetNonSelfFreeingConstructedCount);
}
