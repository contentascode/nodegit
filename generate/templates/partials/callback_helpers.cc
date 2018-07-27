{%each args as cbFunction %}
  {%if cbFunction.isCallbackFunction %}

{{ cbFunction.return.type }} {{ cppClassName }}::{{ cppFunctionName }}_{{ cbFunction.name }}_cppCallback (
  {% each cbFunction.args|argsInfo as arg %}
    {{ arg.cType }} {{ arg.name}}{% if not arg.lastArg %},{% endif %}
  {% endeach %}
) {
  {{ cppFunctionName }}_{{ cbFunction.name|titleCase }}Baton baton({{ cbFunction.return.noResults }});

  {% each cbFunction.args|argsInfo as arg %}
    baton.{{ arg.name }} = {{ arg.name }};
  {% endeach %}

  return baton.ExecuteAsync({{ cppFunctionName }}_{{ cbFunction.name }}_async);
}

void {{ cppClassName }}::{{ cppFunctionName }}_{{ cbFunction.name }}_async(void *untypedBaton) {
  Napi::HandleScope scope(env);

  {{ cppFunctionName }}_{{ cbFunction.name|titleCase }}Baton* baton = static_cast<{{ cppFunctionName }}_{{ cbFunction.name|titleCase }}Baton*>(untypedBaton);

  {% each cbFunction.args|argsInfo as arg %}
    {% if arg | isPayload %}
      {% if cbFunction.payload.globalPayload %}
  Napi::FunctionReference* callback = (({{ cppFunctionName }}_globalPayload*)baton->{{ arg.name }})->{{ cbFunction.name }};
      {% else %}
  Napi::FunctionReference* callback = (Napi::FunctionReference *)baton->{{ arg.name }};
      {% endif %}
    {% endif %}
  {% endeach %}

  v8::Napi::Value argv[{{ cbFunction.args|jsArgsCount }}] = {
    {% each cbFunction.args|argsInfo as arg %}
      {% if arg | isPayload %}
        {%-- payload is always the last arg --%}
        // payload is null because we can use closure scope in javascript
        env.Undefined()
      {% elsif arg.isJsArg %}
        {% if arg.isEnum %}
          Napi::New(env, (int)baton->{{ arg.name }}),
        {% elsif arg.isLibgitType %}
          {{ arg.cppClassName }}::New(baton->{{ arg.name }}, false),
        {% elsif arg.cType == "size_t" %}
          // HACK: NAN should really have an overload for Napi::New to support size_t
          Napi::New(env, (unsigned int)baton->{{ arg.name }}),
        {% elsif arg.cppClassName == 'String' %}
          Napi::New(env, baton->{{ arg.name }}),
        {% else %}
          Napi::New(env, baton->{{ arg.name }}),
        {% endif %}
      {% endif %}
    {% endeach %}
  };

  Napi::TryCatch tryCatch;
  // TODO This should take an async_resource, but we will need to figure out how to pipe the correct context into this
  Napi::MaybeLocal<v8::Value> maybeResult = Napi::Call(*callback, {{ cbFunction.args|jsArgsCount }}, argv);
  Napi::Value result;
  if (!maybeResult.IsEmpty()) {
    result = maybeResult;
  }

  if(PromiseCompletion::ForwardIfPromise(result, baton, {{ cppFunctionName }}_{{ cbFunction.name }}_promiseCompleted)) {
    return;
  }

  {% each cbFunction|returnsInfo false true as _return %}
    if (result.IsEmpty() || result->IsNativeError()) {
      baton->result = {{ cbFunction.return.error }};
    }
    else if (!result->IsNull() && !result->IsUndefined()) {
      {% if _return.isOutParam %}
      {{ _return.cppClassName }}* wrapper = Napi::ObjectWrap::Unwrap<{{ _return.cppClassName }}>(result->ToObject());
      wrapper->selfFreeing = false;

      *baton->{{ _return.name }} = wrapper->GetValue();
      baton->result = {{ cbFunction.return.success }};
      {% else %}
      if (result.IsNumber()) {
        baton->result = result.As<Napi::Number>().Int32Value();
      }
      else {
        baton->result = baton->defaultResult;
      }
      {% endif %}
    }
    else {
      baton->result = baton->defaultResult;
    }
  {% endeach %}

  baton->Done();
}

void {{ cppClassName }}::{{ cppFunctionName }}_{{ cbFunction.name }}_promiseCompleted(bool isFulfilled, AsyncBaton *_baton, Napi::Value result) {
  Napi::HandleScope scope(env);

  {{ cppFunctionName }}_{{ cbFunction.name|titleCase }}Baton* baton = static_cast<{{ cppFunctionName }}_{{ cbFunction.name|titleCase }}Baton*>(_baton);

  if (isFulfilled) {
    {% each cbFunction|returnsInfo false true as _return %}
      if (result.IsEmpty() || result->IsNativeError()) {
        baton->result = {{ cbFunction.return.error }};
      }
      else if (!result->IsNull() && !result->IsUndefined()) {
        {% if _return.isOutParam %}
        {{ _return.cppClassName }}* wrapper = Napi::ObjectWrap::Unwrap<{{ _return.cppClassName }}>(result->ToObject());
        wrapper->selfFreeing = false;

        *baton->{{ _return.name }} = wrapper->GetValue();
        baton->result = {{ cbFunction.return.success }};
        {% else %}
        if (result.IsNumber()) {
          baton->result = result.As<Napi::Number>().Int32Value();
        }
        else {
          baton->result = baton->defaultResult;
        }
        {% endif %}
      }
      else {
        baton->result = baton->defaultResult;
      }
    {% endeach %}
  }
  else {
    // promise was rejected
    {{ cppClassName }}* instance = static_cast<{{ cppClassName }}*>(baton->{% each cbFunction.args|argsInfo as arg %}
      {% if arg.payload == true %}{{arg.name}}{% elsif arg.lastArg %}{{arg.name}}{% endif %}
    {% endeach %});
    Napi::Object parent = instance->handle();
    SetPrivate(parent, Napi::String::New(env, "NodeGitPromiseError"), result);

    baton->result = {{ cbFunction.return.error }};
  }
  baton->Done();
}
  {%endif%}
{%endeach%}
