
{%each args|argsInfo as arg%}
  {%if arg.isJsArg%}
    {%if not arg.isOptional%}
      {%if not arg.payloadFor %}
        {%if arg | isOid %}
  if (info.Length() == {{arg.jsArg}}
    || (!info[{{arg.jsArg}}].IsObject() && !info[{{arg.jsArg}}].IsString())) {
    Napi::Error::New(env, "{{arg.jsClassName}} {{arg.name}} is required.").ThrowAsJavaScriptException();
    return env.Null();
  }
        {%elsif arg.isCallbackFunction %}
  if (info.Length() == {{arg.jsArg}} || !info[{{arg.jsArg}}]->IsFunction()) {
    Napi::Error::New(env, "{{arg.jsClassName}} {{arg.name}} is required.").ThrowAsJavaScriptException();
    return env.Null();
  }
        {%elsif arg.cppClassName == "GitStrarray" %}
  if (info.Length() == {{arg.jsArg}} || !(info[{{arg.jsArg}}].As<Napi::Boolean>().Value())) {
    Napi::Error::New(env, "Array, String Object, or string {{arg.name}} is required.").ThrowAsJavaScriptException();
    return env.Null();
  }
        {%else%}
  if (info.Length() == {{arg.jsArg}} || !info[{{arg.jsArg}}]->Is{{arg.cppClassName|cppToV8}}()) {
    Napi::Error::New(env, "{{arg.jsClassName}} {{arg.name}} is required.").ThrowAsJavaScriptException();
    return env.Null();
  }

        {%endif%}
      {%endif%}
    {%endif%}
  {%endif%}
{%endeach%}
