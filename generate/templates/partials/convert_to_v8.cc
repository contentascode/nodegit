// start convert_to_v8 block
{% if cppClassName == 'String' %}
  if ({{= parsedName =}}){
    {% if size %}
      to = Napi::String::New(env, {{= parsedName =}}, {{ size }});
    {% elsif cType == 'char **' %}
      to = Napi::String::New(env, *{{= parsedName =}});
    {% else %}
      to = Napi::String::New(env, {{= parsedName =}});
    {% endif %}
  }
  else {
    to = env.Null();
  }

  {% if freeFunctionName %}
    {{ freeFunctionName }}({{= parsedName =}});
  {% endif %}

{% elsif cppClassName|isV8Value %}

  {% if isCppClassIntType %}
    to = Napi::{{ cppClassName }}::New(env, ({{ parsedClassName }}){{= parsedName =}});
  {% else %}
    to = Napi::{{ cppClassName }}::New(env, {% if needsDereference %}*{% endif %}{{= parsedName =}});
  {% endif %}

{% elsif cppClassName == 'External' %}

  to = Napi::External::New(env, (void *){{= parsedName =}});

{% elsif cppClassName == 'Array' %}

  {%-- // FIXME this is not general purpose enough. --%}
  {% if size %}
    v8::Napi::Array tmpArray = Napi::Array::New(env, {{= parsedName =}}->{{ size }});
    for (unsigned int i = 0; i < {{= parsedName =}}->{{ size }}; i++) {
      (tmpArray).Set(Napi::Number::New(env, i), Napi::String::New(env, {{= parsedName =}}->{{ key }}[i]));
    }
  {% else %}
    v8::Napi::Array tmpArray = Napi::Array::New(env, {{= parsedName =}});
  {% endif %}

  to = tmpArray;
{% elsif cppClassName == 'GitBuf' %}
  {% if doNotConvert %}
  to = env.Null();
  {% else %}
  if ({{= parsedName =}}) {
    to = Napi::String::New(env, {{= parsedName =}}->ptr, {{= parsedName = }}->size);
  }
  else {
    to = env.Null();
  }
  {% endif %}
{% else %}
  {% if copy %}
    if ({{= parsedName =}} != NULL) {
      {{= parsedName =}} = ({{ cType|replace '**' '*' }} {% if not cType|isPointer %}*{% endif %}){{ copy }}({{= parsedName =}});
    }
  {% endif %}

  if ({{= parsedName =}} != NULL) {
    // {{= cppClassName }} {{= parsedName }}
    {% if cppClassName == 'Wrapper' %}
      to = {{ cppClassName }}::New({{= parsedName =}});
    {% else %}
      to = {{ cppClassName }}::New({{= parsedName =}}, {{ selfFreeing|toBool }} {% if ownedByThis %}, info.This(){% endif %});
    {% endif %}
  }
  else {
    to = env.Null();
  }

{% endif %}
// end convert_to_v8 block
