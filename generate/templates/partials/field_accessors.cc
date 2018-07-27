{% each fields|fieldsInfo as field %}
  {% if not field.ignore %}
    NAN_GETTER({{ cppClassName }}::Get{{ field.cppFunctionName }}) {

      {{ cppClassName }} *wrapper = Napi::ObjectWrap::Unwrap<{{ cppClassName }}>(info.This());

      {% if field.isEnum %}
        return Napi::New(env, (int)wrapper->GetValue()->{{ field.name }});

      {% elsif field.isLibgitType | or field.payloadFor %}
        return Napi::New(env, wrapper->{{ field.name }});

      {% elsif field.isCallbackFunction %}
        if (wrapper->{{field.name}}.HasCallback()) {
          return wrapper->{{ field.name }}.GetCallback()->GetFunction();
        } else {
          return env.Undefined();
        }

      {% elsif field.cppClassName == 'String' %}
        if (wrapper->GetValue()->{{ field.name }}) {
          return Napi::String::New(env, wrapper->GetValue()->{{ field.name }});
        }
        else {
          return;
        }

      {% elsif field.cppClassName|isV8Value %}
        return Napi::{{ field.cppClassName }}::New(env, wrapper->GetValue()->{{ field.name }});
      {% endif %}
    }

    NAN_SETTER({{ cppClassName }}::Set{{ field.cppFunctionName }}) {
      {{ cppClassName }} *wrapper = Napi::ObjectWrap::Unwrap<{{ cppClassName }}>(info.This());

      {% if field.isEnum %}
        if (value.IsNumber()) {
          wrapper->GetValue()->{{ field.name }} = ({{ field.cType }}) value.As<Napi::Number>().Int32Value();
        }

      {% elsif field.isLibgitType %}
        v8::Napi::Object {{ field.name }}(value->ToObject());

        wrapper->{{ field.name }}.Reset({{ field.name }});

        wrapper->raw->{{ field.name }} = {% if not field.cType | isPointer %}*{% endif %}{% if field.cppClassName == 'GitStrarray' %}StrArrayConverter::Convert({{ field.name }}->ToObject()){% else %}Napi::ObjectWrap::Unwrap<{{ field.cppClassName }}>({{ field.name }}->ToObject())->GetValue(){% endif %};

      {% elsif field.isCallbackFunction %}
        Napi::FunctionReference *callback = NULL;
        int throttle = {%if field.return.throttle %}{{ field.return.throttle }}{%else%}0{%endif%};
        bool waitForResult = true;

        if (value->IsFunction()) {
          callback = new Napi::FunctionReference(value.As<Napi::Function>());
        } else if (value.IsObject()) {
          v8::Napi::Object object = value.As<Napi::Object>();
          v8::Napi::String callbackKey;
          Napi::MaybeNapi::Value maybeObjectCallback = (object).Get(Napi::String::New(env, "callback"));
          if (!maybeObjectCallback.IsEmpty()) {
            v8::Napi::Value objectCallback = maybeObjectCallback;
            if (objectCallback->IsFunction()) {
              callback = new Napi::FunctionReference(objectCallback.As<Napi::Function>());

              Napi::MaybeNapi::Value maybeObjectThrottle = (object).Get(Napi::String::New(env, "throttle"));
              if(!maybeObjectThrottle.IsEmpty()) {
                v8::Napi::Value objectThrottle = maybeObjectThrottle;
                if (objectThrottle.IsNumber()) {
                  throttle = (int)objectThrottle.As<Napi::Number>()->Value();
                }
              }

              Napi::MaybeNapi::Value maybeObjectWaitForResult = (object).Get(Napi::String::New(env, "waitForResult"));
              if(!maybeObjectWaitForResult.IsEmpty()) {
                Napi::Value objectWaitForResult = maybeObjectWaitForResult;
                waitForResult = (bool)objectWaitForResult.As<Napi::Boolean>().Value();
              }
            }
          }
        }
        if (callback) {
          if (!wrapper->raw->{{ field.name }}) {
            wrapper->raw->{{ field.name }} = ({{ field.cType }}){{ field.name }}_cppCallback;
          }

          wrapper->{{ field.name }}.SetCallback(callback, throttle, waitForResult);
        }

      {% elsif field.payloadFor %}
        wrapper->{{ field.name }}.Reset(value);

      {% elsif field.cppClassName == 'String' %}
        if (wrapper->GetValue()->{{ field.name }}) {
        }

        Napi::String str(env, value);
        wrapper->GetValue()->{{ field.name }} = strdup(*str);

      {% elsif field.isCppClassIntType %}
        if (value.IsNumber()) {
          wrapper->GetValue()->{{ field.name }} = value->{{field.cppClassName}}Value();
        }

      {% else %}
        if (value.IsNumber()) {
          wrapper->GetValue()->{{ field.name }} = ({{ field.cType }}) value.As<Napi::Number>().Int32Value();
        }
      {% endif %}
    }

    {% if field.isCallbackFunction %}
      {{ cppClassName }}* {{ cppClassName }}::{{ field.name }}_getInstanceFromBaton({{ field.name|titleCase }}Baton* baton) {
        {% if isExtendedStruct %}
          return static_cast<{{ cppClassName }}*>((({{cType}}_extended *)baton->self)->payload);
        {% else %}
          return static_cast<{{ cppClassName }}*>(baton->
          {% each field.args|argsInfo as arg %}
            {% if arg.payload == true %}
              {{arg.name}}
            {% elsif arg.lastArg %}
              {{arg.name}}
            {% endif %}
          {% endeach %});
        {% endif %}
      }

      {{ field.return.type }} {{ cppClassName }}::{{ field.name }}_cppCallback (
        {% each field.args|argsInfo as arg %}
          {{ arg.cType }} {{ arg.name}}{% if not arg.lastArg %},{% endif %}
        {% endeach %}
      ) {
        {{ field.name|titleCase }}Baton *baton =
          new {{ field.name|titleCase }}Baton({{ field.return.noResults }});

        {% each field.args|argsInfo as arg %}
          baton->{{ arg.name }} = {{ arg.name }};
        {% endeach %}

        {{ cppClassName }}* instance = {{ field.name }}_getInstanceFromBaton(baton);

        {% if field.return.type == "void" %}
          if (instance->{{ field.name }}.WillBeThrottled()) {
            delete baton;
          } else if (instance->{{ field.name }}.ShouldWaitForResult()) {
            baton->ExecuteAsync({{ field.name }}_async);
            delete baton;
          } else {
            baton->ExecuteAsync({{ field.name }}_async, deleteBaton);
          }
          return;
        {% else %}
          {{ field.return.type }} result;

          if (instance->{{ field.name }}.WillBeThrottled()) {
            result = baton->defaultResult;
            delete baton;
          } else if (instance->{{ field.name }}.ShouldWaitForResult()) {
            result = baton->ExecuteAsync({{ field.name }}_async);
            delete baton;
          } else {
            result = baton->defaultResult;
            baton->ExecuteAsync({{ field.name }}_async, deleteBaton);
          }
          return result;
        {% endif %}
      }


      void {{ cppClassName }}::{{ field.name }}_async(void *untypedBaton) {
        Napi::HandleScope scope(env);

        {{ field.name|titleCase }}Baton* baton = static_cast<{{ field.name|titleCase }}Baton*>(untypedBaton);
        {{ cppClassName }}* instance = {{ field.name }}_getInstanceFromBaton(baton);

        if (instance->{{ field.name }}.GetCallback()->IsEmpty()) {
          {% if field.return.type == "int" %}
            baton->result = baton->defaultResult; // no results acquired
          {% endif %}
          baton->Done();
          return;
        }

        {% each field.args|argsInfo as arg %}
          {% if arg.name == "payload" %}
            {%-- Do nothing --%}
          {% elsif arg.isJsArg %}
            {% if arg.cType == "const char *" %}
              if (baton->{{ arg.name }} == NULL) {
                  baton->{{ arg.name }} = "";
              }
            {% elsif arg.cppClassName == "String" %}
              Napi::Value src;
              if (baton->{{ arg.name }} == NULL) {
                  src = env.Null();
              }
              else {
                src = Napi::String::New(env, *baton->{{ arg.name }});
              }
            {% endif %}
          {% endif %}
        {% endeach %}

        {% if field.isSelfReferential %}
          {% if field.args|jsArgsCount|subtract 2| setUnsigned == 0 %}
            v8::Napi::Value *argv = NULL;
          {% else %}
            v8::Napi::Value argv[{{ field.args|jsArgsCount|subtract 2| setUnsigned }}] = {
          {% endif %}
        {% else %}
          v8::Napi::Value argv[{{ field.args|jsArgsCount }}] = {
        {% endif %}
        {% each field.args|argsInfo as arg %}
          {% if field.isSelfReferential %}
            {% if not arg.firstArg %}
              {% if field.args|jsArgsCount|subtract 1|or 0 %}
                {% if arg.cppClassName == "String" %}
                  {%-- src is always the last arg --%}
                  src
                {% elsif arg.isJsArg %}
                  {% if arg.isEnum %}
                    Napi::New(env, (int)baton->{{ arg.name }}),
                  {% elsif arg.isLibgitType %}
                    {{ arg.cppClassName }}::New(baton->{{ arg.name }}, false),
                  {% elsif arg.cType == "size_t" %}
                    Napi::New(env, (unsigned int)baton->{{ arg.name }}),
                  {% elsif arg.name == "payload" %}
                    {%-- skip, filters should not have a payload --%}
                  {% else %}
                    Napi::New(env, baton->{{ arg.name }}),
                  {% endif %}
                {% endif %}
              {% endif %}
            {% endif %}
          {% else %}
            {% if arg.name == "payload" %}
              {%-- payload is always the last arg --%}
              Napi::New(env, instance->{{ fields|payloadFor field.name }})
            {% elsif arg.isJsArg %}
              {% if arg.isEnum %}
                Napi::New(env, (int)baton->{{ arg.name }}),
              {% elsif arg.isLibgitType %}
                {{ arg.cppClassName }}::New(baton->{{ arg.name }}, false),
              {% elsif arg.cType == "size_t" %}
                // HACK: NAN should really have an overload for Napi::New to support size_t
                Napi::New(env, (unsigned int)baton->{{ arg.name }}),
              {% elsif arg.cppClassName == "String" %}
                Napi::New(env, baton->{{ arg.name }}),
              {% else %}
                Napi::New(env, baton->{{ arg.name }}),
              {% endif %}
            {% endif %}
          {% endif %}
        {% endeach %}
        {% if not field.isSelfReferential %}
          };
        {% elsif field.args|jsArgsCount|subtract 2| setUnsigned > 0  %}
          };
        {% endif %}

        Napi::TryCatch tryCatch;

        // TODO This should take an async_resource, but we will need to figure out how to pipe the correct context into this
        {% if field.isSelfReferential %}
          Napi::MaybeLocal<v8::Value> maybeResult = Napi::Call(*(instance->{{ field.name }}.GetCallback()), {{ field.args|jsArgsCount|subtract 2| setUnsigned }}, argv);
        {% else  %}
          Napi::MaybeLocal<v8::Value> maybeResult = Napi::Call(*(instance->{{ field.name }}.GetCallback()), {{ field.args|jsArgsCount }}, argv);
        {% endif %}

        Napi::Value result;
        if (!maybeResult.IsEmpty()) {
          result = maybeResult;
        }

        if(PromiseCompletion::ForwardIfPromise(result, baton, {{ cppClassName }}::{{ field.name }}_promiseCompleted)) {
          return;
        }

        {% if field.return.type == "void" %}
          baton->Done();
        {% else %}
          {% each field|returnsInfo false true as _return %}
            if (result.IsEmpty() || result->IsNativeError()) {
              baton->result = {{ field.return.error }};
            }
            else if (!result->IsNull() && !result->IsUndefined()) {
              {% if _return.isOutParam %}
              {{ _return.cppClassName }}* wrapper = Napi::ObjectWrap::Unwrap<{{ _return.cppClassName }}>(result->ToObject());
              wrapper->selfFreeing = false;

              *baton->{{ _return.name }} = wrapper->GetValue();
              baton->result = {{ field.return.success }};
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
        {% endif %}
      }

      void {{ cppClassName }}::{{ field.name }}_promiseCompleted(bool isFulfilled, AsyncBaton *_baton, Napi::Value result) {
        Napi::HandleScope scope(env);

        {{ field.name|titleCase }}Baton* baton = static_cast<{{ field.name|titleCase }}Baton*>(_baton);
        {% if field.return.type == "void" %}
          baton->Done();
        {% else %}
          if (isFulfilled) {
            {% each field|returnsInfo false true as _return %}
              if (result.IsEmpty() || result->IsNativeError()) {
                baton->result = {{ field.return.error }};
              }
              else if (!result->IsNull() && !result->IsUndefined()) {
                {% if _return.isOutParam %}
                {{ _return.cppClassName }}* wrapper = Napi::ObjectWrap::Unwrap<{{ _return.cppClassName }}>(result->ToObject());
                wrapper->selfFreeing = false;

                *baton->{{ _return.name }} = wrapper->GetValue();
                baton->result = {{ field.return.success }};
                {% else %}
                if (result.IsNumber()) {
                  baton->result = result.As<Napi::Number>().Int32Value();
                }
                else{
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
            {% if isExtendedStruct %}
              {{ cppClassName }}* instance = static_cast<{{ cppClassName }}*>((({{cType}}_extended *)baton->self)->payload);
            {% else %}
              {{ cppClassName }}* instance = static_cast<{{ cppClassName }}*>(baton->{% each field.args|argsInfo as arg %}
              {% if arg.payload == true %}{{arg.name}}{% elsif arg.lastArg %}{{arg.name}}{% endif %}
              {% endeach %});
            {% endif %}
            Napi::Object parent = instance->handle();
            SetPrivate(parent, Napi::String::New(env, "NodeGitPromiseError"), result);

            baton->result = {{ field.return.error }};
          }
          baton->Done();
        {% endif %}
      }
    {% endif %}
  {% endif %}
{% endeach %}
