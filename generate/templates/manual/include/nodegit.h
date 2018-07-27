#ifndef NODEGIT_H
#define NODEGIT_H

#include "thread_pool.h"

extern ThreadPool libgit2ThreadPool;

Napi::Value GetPrivate(Napi::Object object,
                                    Napi::String key);

void SetPrivate(Napi::Object object,
                    Napi::String key,
                    Napi::Value value);

#endif
