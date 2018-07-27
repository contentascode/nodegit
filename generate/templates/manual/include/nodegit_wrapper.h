#ifndef NODEGIT_WRAPPER_H
#define NODEGIT_WRAPPER_H

#include <napi.h>
#include <uv.h>

// the Traits template parameter supplies:
//  typename cppClass - the C++ type of the NodeGit wrapper (e.g. GitRepository)
//  typename cType - the C type of the libgit2 object being wrapped (e.g. git_repository)
//
//  static const bool isDuplicable
//  static void duplicate(cType **dest, cType *src) - duplicates src using dupFunction or cpyFunction
//
//  static const bool isFreeable
//  static void free(cType *raw) - frees the object using freeFunctionName

template<typename Traits>
class NodeGitWrapper : public Napi::ObjectWrap<NodeGitWrapper> {
public:
  // replicate Traits typedefs for ease of use
  typedef typename Traits::cType cType;
  typedef typename Traits::cppClass cppClass;

  // whether raw should be freed on destruction
  // TODO: this should be protected but we have a few use cases that change this to
  // false from the outside.  I suspect it gets turned to false to avoid
  // double-free problems in cases like when we pass cred objects to libgit2
  // and it frees them.  We should probably be NULLing raw in that case
  // (and through a method) instead of changing selfFreeing, but that's
  // a separate issue.
  bool selfFreeing;
protected:
  cType *raw;

  // owner of the object, in the memory management sense. only populated
  // when using ownedByThis, and the type doesn't have a dupFunction
  // CopyablePersistentTraits are used to get the reset-on-destruct behavior.
  Napi::Persistent<v8::Object, Napi::CopyablePersistentTraits<v8::Object> > owner;

  static Napi::FunctionReference constructor;

  // diagnostic count of self-freeing object instances
  static int SelfFreeingInstanceCount;
  // diagnostic count of constructed non-self-freeing object instances
  static int NonSelfFreeingConstructedCount;

  static void InitializeTemplate(Napi::FunctionReference &tpl);

  NodeGitWrapper(cType *raw, bool selfFreeing, Napi::Object owner);
  NodeGitWrapper(const char *error); // calls ThrowError
  ~NodeGitWrapper();

  static Napi::Value JSNewFunction(const Napi::CallbackInfo& info);

  static Napi::Value GetSelfFreeingInstanceCount(const Napi::CallbackInfo& info);
  static Napi::Value GetNonSelfFreeingConstructedCount(const Napi::CallbackInfo& info);

public:
  static Napi::Value New(const cType *raw, bool selfFreeing, Napi::Object owner = Napi::Object());

  cType *GetValue();
  void ClearValue();
};

#endif
