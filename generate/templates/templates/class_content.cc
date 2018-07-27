#include <napi.h>
#include <uv.h>
#include <string.h>

extern "C" {
  #include <git2.h>
  {% each cDependencies as dependency %}
    #include <{{ dependency }}>
  {% endeach %}
}

#include "../include/nodegit.h"
#include "../include/lock_master.h"
#include "../include/functions/copy.h"
#include "../include/{{ filename }}.h"
#include "nodegit_wrapper.cc"
#include "../include/async_libgit2_queue_worker.h"

{% each dependencies as dependency %}
  #include "{{ dependency }}"
{% endeach %}

#include <iostream>

using namespace std;
using namespace Napi;

{% if cType %}
  {{ cppClassName }}::~{{ cppClassName }}() {
    // this will cause an error if you have a non-self-freeing object that also needs
    // to save values. Since the object that will eventually free the object has no
    // way of knowing to free these values.
    {% each functions as function %}
      {% if not function.ignore %}
        {% each function.args as arg %}
          {% if arg.saveArg %}

      {{ function.cppFunctionName }}_{{ arg.name }}).Reset();

          {% endif %}
        {% endeach %}
      {% endif %}
    {% endeach %}
  }

  void {{ cppClassName }}::InitializeComponent(Napi::Object target) {
    Napi::HandleScope scope(env);

    v8::Napi::FunctionReference tpl = Napi::Function::New(env, JSNewFunction);


    tpl->SetClassName(Napi::String::New(env, "{{ jsClassName }}"));

    {% each functions as function %}
      {% if not function.ignore %}
        {% if function.isPrototypeMethod %}
          Napi::SetPrototypeMethod(tpl, "{{ function.jsFunctionName }}", {{ function.cppFunctionName }});
        {% else %}
          Napi::SetMethod(tpl, "{{ function.jsFunctionName }}", {{ function.cppFunctionName }});
        {% endif %}
      {% endif %}
    {% endeach %}

    {% each fields as field %}
      {% if not field.ignore %}
        Napi::SetPrototypeMethod(tpl, "{{ field.jsFunctionName }}", {{ field.cppFunctionName }});
      {% endif %}
    {% endeach %}

    InitializeTemplate(tpl);

    v8::Napi::Function _constructor = Napi::GetFunction(tpl);
    constructor.Reset(_constructor);
    (target).Set(Napi::String::New(env, "{{ jsClassName }}"), _constructor);
  }

{% else %}

  void {{ cppClassName }}::InitializeComponent(Napi::Object target) {
    Napi::HandleScope scope(env);

    v8::Napi::Object object = Napi::Object::New(env);

    {% each functions as function %}
      {% if not function.ignore %}
        Napi::SetMethod(object, "{{ function.jsFunctionName }}", {{ function.cppFunctionName }});
      {% endif %}
    {% endeach %}

    (target).Set(Napi::String::New(env, "{{ jsClassName }}"), object);
  }

{% endif %}

{% each functions as function %}
  {% if not function.ignore %}
    {% if function.isManual %}
      {{= function.implementation =}}
    {% elsif function.isAsync %}
      {% partial asyncFunction function %}
    {% else %}
      {% partial syncFunction function %}
    {% endif %}
  {% endif %}
{% endeach %}

{% partial fields . %}

{%if cType %}
// force base class template instantiation, to make sure we get all the
// methods, statics, etc.
template class NodeGitWrapper<{{ cppClassName }}Traits>;
{% endif %}
