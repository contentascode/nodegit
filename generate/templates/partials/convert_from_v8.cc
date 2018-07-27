{%if not isPayload %}
// start convert_from_v8 block
  {%if cType|isPointer %}
  {{ cType }} from_{{ name }} = NULL;
  {%elsif cType|isDoublePointer %}
  {{ cType }} from_{{ name }} = NULL;
  {%else%}
  {{ cType }} from_{{ name }};
  {%endif%}
  {%if isOptional | or isBoolean %}

    {%if cppClassName == 'GitStrarray'%}
    {%-- Print nothing --%}
    {% elsif cppClassName == 'GitBuf' %}
    {%-- Print nothing --%}
    {%else%}
    if (info[{{ jsArg }}]->Is{{ cppClassName|cppToV8 }}()) {
      {%endif%}
    {%endif%}
  {%if cppClassName == 'String'%}

  Napi::String {{ name }}(env, info[{{ jsArg }}]->ToString());
  // malloc with one extra byte so we can add the terminating null character C-strings expect:
  from_{{ name }} = ({{ cType }}) malloc({{ name }}.Length() + 1);
  // copy the characters from the nodejs string into our C-string (used instead of strdup or strcpy because nulls in
  // the middle of strings are valid coming from nodejs):
  memcpy((void *)from_{{ name }}, *{{ name }}, {{ name }}.Length());
  // ensure the final byte of our new string is null, extra casts added to ensure compatibility with various C types
  // used in the nodejs binding generation:
  memset((void *)(((char *)from_{{ name }}) + {{ name }}.Length()), 0, 1);
  {%elsif cppClassName == 'GitStrarray' %}

  from_{{ name }} = StrArrayConverter::Convert(info[{{ jsArg }}]);
  {%elsif  cppClassName == 'GitBuf' %}

  from_{{ name }} = GitBufConverter::Convert(info[{{ jsArg }}]);
  {%elsif cppClassName == 'Wrapper'%}

  Napi::String {{ name }}(env, info[{{ jsArg }}]->ToString());
  // malloc with one extra byte so we can add the terminating null character C-strings expect:
  from_{{ name }} = ({{ cType }}) malloc({{ name }}.Length() + 1);
  // copy the characters from the nodejs string into our C-string (used instead of strdup or strcpy because nulls in
  // the middle of strings are valid coming from nodejs):
  memcpy((void *)from_{{ name }}, *{{ name }}, {{ name }}.Length());
  // ensure the final byte of our new string is null, extra casts added to ensure compatibility with various C types
  // used in the nodejs binding generation:
  memset((void *)(((char *)from_{{ name }}) + {{ name }}.Length()), 0, 1);
  {%elsif cppClassName == 'Array'%}

  Array *tmp_{{ name }} = *info[{{ jsArg }}].As<Napi::Array>();
  from_{{ name }} = ({{ cType }})malloc(tmp_{{ name }}->Length() * sizeof({{ cType|replace '**' '*' }}));
      for (unsigned int i = 0; i < tmp_{{ name }}->Length(); i++) {
    {%--
      // FIXME: should recursively call convertFromv8.
    --%}
      from_{{ name }}[i] = Napi::ObjectWrap::Unwrap<{{ arrayElementCppClassName }}>(tmp_{{ name }}->Get(Napi::New(env, static_cast<double>(i)))->ToObject())->GetValue();
      }
  {%elsif cppClassName == 'Function'%}
  {%elsif cppClassName == 'Buffer'%}

  from_{{ name }} = Buffer::Data(info[{{ jsArg }}]->ToObject());
  {%elsif cppClassName|isV8Value %}

    {%if cType|isPointer %}
      *from_{{ name }} = ({{ cType|unPointer }}) {{ cast }} {%if isEnum %}(int){%endif%} info[{{ jsArg }}].As<v8::{{ cppClassName }}>()->Value();
    {%else%}
      from_{{ name }} = ({{ cType }}) {{ cast }} {%if isEnum %}(int){%endif%} info[{{ jsArg }}].As<v8::{{ cppClassName }}>()->Value();
    {%endif%}
  {%elsif cppClassName == 'GitOid'%}
  if (info[{{ jsArg }}].IsString()) {
    // Try and parse in a string to a git_oid
    Napi::String oidString(env, info[{{ jsArg }}]->ToString());
    git_oid *oidOut = (git_oid *)malloc(sizeof(git_oid));

    if (git_oid_fromstr(oidOut, (const char *) strdup(*oidString)) != GIT_OK) {
      free(oidOut);

      if (giterr_last()) {
        Napi::Error::New(env, giterr_last()->message).ThrowAsJavaScriptException();
        return env.Null();
      } else {
        Napi::Error::New(env, "Unknown Error").ThrowAsJavaScriptException();
        return env.Null();
      }
    }

    {%if cType|isDoublePointer %}
    from_{{ name }} = &oidOut;
    {%else%}
    from_{{ name }} = oidOut;
    {%endif%}
  }
  else {
    {%if cType|isDoublePointer %}*{%endif%}from_{{ name }} = Napi::ObjectWrap::Unwrap<{{ cppClassName }}>(info[{{ jsArg }}]->ToObject())->GetValue();
  }
  {%else%}
    {%if cType|isDoublePointer %}*{%endif%}from_{{ name }} = Napi::ObjectWrap::Unwrap<{{ cppClassName }}>(info[{{ jsArg }}]->ToObject())->GetValue();
  {%endif%}

  {%if isBoolean %}
  }
  else {
    from_{{ name }} = info[{{ jsArg }}]->IsTrue() ? 1 : 0;
  }
  {%elsif isOptional %}
    {%if cppClassName != 'GitStrarray'%}
  }
  else {
    from_{{ name }} = 0;
  }
    {%endif%}
  {%endif%}
// end convert_from_v8 block
{%endif%}
