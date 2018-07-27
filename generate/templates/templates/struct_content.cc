#include <napi.h>
#include <uv.h>
#include <string.h>
#ifdef WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif // win32

extern "C" {
  #include <git2.h>
  {% each cDependencies as dependency %}
    #include <{{ dependency }}>
  {% endeach %}
}

#include <iostream>
#include "../include/nodegit.h"
#include "../include/lock_master.h"
#include "../include/functions/copy.h"
#include "../include/{{ filename }}.h"
#include "nodegit_wrapper.cc"

{% each dependencies as dependency %}
  #include "{{ dependency }}"
{% endeach %}

using namespace Napi;
using namespace std;


// generated from struct_content.cc
{{ cppClassName }}::{{ cppClassName }}() : NodeGitWrapper<{{ cppClassName }}Traits>(NULL, true, Napi::Object())
{
  {% if ignoreInit == true %}
  this->raw = new {{ cType }};
  {% else %}
    {% if isExtendedStruct %}
      {{ cType }}_extended wrappedValue = {{ cType|upper }}_INIT;
      this->raw = ({{ cType }}*) malloc(sizeof({{ cType }}_extended));
      memcpy(this->raw, &wrappedValue, sizeof({{ cType }}_extended));
    {% else %}
      {{ cType }} wrappedValue = {{ cType|upper }}_INIT;
      this->raw = ({{ cType }}*) malloc(sizeof({{ cType }}));
      memcpy(this->raw, &wrappedValue, sizeof({{ cType }}));
    {% endif %}
  {% endif %}

  this->ConstructFields();
}

{{ cppClassName }}::{{ cppClassName }}({{ cType }}* raw, bool selfFreeing, Napi::Object owner)
 : NodeGitWrapper<{{ cppClassName }}Traits>(raw, selfFreeing, owner)
{
  this->ConstructFields();
}

{{ cppClassName }}::~{{ cppClassName }}() {
  {% each fields|fieldsInfo as field %}
    {% if not field.ignore %}
      {% if not field.isEnum %}
        {% if field.isCallbackFunction %}
          if (this->{{ field.name }}.HasCallback()) {
            {% if isExtendedStruct %}
              (({{ cType }}_extended *)this->raw)->payload = NULL;
            {% else %}
              this->raw->{{ fields|payloadFor field.name }} = NULL;
            {% endif %}
          }
        {% endif %}
      {% endif %}
    {% endif %}
  {% endeach %}
}

void {{ cppClassName }}::ConstructFields() {
  {% each fields|fieldsInfo as field %}
    {% if not field.ignore %}
      {% if not field.isEnum %}
        {% if field.hasConstructor |or field.isLibgitType %}
          v8::Napi::Object {{ field.name }}Temp = {{ field.cppClassName }}::New(
            {%if not field.cType|isPointer %}&{%endif%}this->raw->{{ field.name }},
            false
          )->ToObject();
          this->{{ field.name }}.Reset({{ field.name }}Temp);

        {% elsif field.isCallbackFunction %}

          // Set the static method call and set the payload for this function to be
          // the current instance
          this->raw->{{ field.name }} = NULL;
          {% if isExtendedStruct  %}
            (({{ cType }}_extended *)this->raw)->payload = (void *)this;
          {% else %}
            this->raw->{{ fields|payloadFor field.name }} = (void *)this;
          {% endif %}
        {% elsif field.payloadFor %}

          v8::Napi::Value {{ field.name }} = env.Undefined();
          this->{{ field.name }}.Reset({{ field.name }});
        {% endif %}
      {% endif %}
    {% endif %}
  {% endeach %}
}

void {{ cppClassName }}::InitializeComponent(Napi::Object target) {
  Napi::HandleScope scope(env);

  v8::Napi::FunctionReference tpl = Napi::Function::New(env, JSNewFunction);


  tpl->SetClassName(Napi::String::New(env, "{{ jsClassName }}"));

  {% each fields as field %}
    {% if not field.ignore %}
    {% if not field | isPayload %}
      Napi::SetAccessor(tpl->InstanceTemplate(), Napi::String::New(env, "{{ field.jsFunctionName }}"), Get{{ field.cppFunctionName}}, Set{{ field.cppFunctionName}});
    {% endif %}
    {% endif %}
  {% endeach %}

  InitializeTemplate(tpl);

  v8::Napi::Function _constructor = Napi::GetFunction(tpl);
  constructor.Reset(_constructor);
  (target).Set(Napi::String::New(env, "{{ jsClassName }}"), _constructor);
}

{% partial fieldAccessors . %}

// force base class template instantiation, to make sure we get all the
// methods, statics, etc.
template class NodeGitWrapper<{{ cppClassName }}Traits>;
