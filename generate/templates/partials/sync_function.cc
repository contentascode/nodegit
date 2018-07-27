
{%partial doc .%}
NAN_METHOD({{ cppClassName }}::{{ cppFunctionName }}) {
  Napi::EscapableHandleScope scope(env);
  {%partial guardArguments .%}

  {%each .|returnsInfo 'true' as _return %}
    {%if _return.shouldAlloc %}
  {{ _return.cType }}{{ _return.name }} = ({{ _return.cType }})malloc(sizeof({{ _return.cType|unPointer }}));
    {%else%}
  {{ _return.cType|unPointer }} {{ _return.name }} = {{ _return.cType|unPointer|defaultValue }};
    {%endif%}
  {%endeach%}

  {%each args|argsInfo as arg %}
    {%if not arg.isSelf %}
      {%if not arg.isReturn %}
        {%partial convertFromV8 arg %}
        {%if arg.saveArg %}
  v8::Napi::Object {{ arg.name }}(info[{{ arg.jsArg }}]->ToObject());
  {{ cppClassName }} *thisObj = Napi::ObjectWrap::Unwrap<{{ cppClassName }}>(info.This());

  thisObj->{{ cppFunctionName }}_{{ arg.name }}.Reset({{ arg.name }});
        {%endif%}
      {%endif%}
    {%endif%}
  {%endeach%}

{%each args|argsInfo as arg %}
{%endeach%}

{%-- Inside a free call, if the value is already free'd don't do it again.--%}
{% if cppFunctionName == "Free" %}
if (Napi::ObjectWrap::Unwrap<{{ cppClassName }}>(info.This())->GetValue() != NULL) {
{% endif %}

  giterr_clear();

  {
    LockMaster lockMaster(/*asyncAction: */false{%each args|argsInfo as arg %}
      {%if arg.cType|isPointer%}{%if not arg.isReturn%}
        ,{%if arg.isSelf %}
    Napi::ObjectWrap::Unwrap<{{ arg.cppClassName }}>(info.This())->GetValue()
        {%else%}
    from_{{ arg.name }}
      {%endif%}
      {%endif%}{%endif%}
    {%endeach%});

  {%if .|hasReturnValue %}
    {{ return.cType }} result = {%endif%}{{ cFunctionName }}(
    {%each args|argsInfo as arg %}
      {%if arg.isReturn %}
        {%if not arg.shouldAlloc %}&{%endif%}
      {%endif%}
      {%if arg.isSelf %}
  Napi::ObjectWrap::Unwrap<{{ arg.cppClassName }}>(info.This())->GetValue()
      {%elsif arg.isReturn %}
  {{ arg.name }}
      {%else%}
  from_{{ arg.name }}
      {%endif%}
      {%if not arg.lastArg %},{%endif%}
    {%endeach%}
    );

  {%if .|hasReturnValue |and return.isErrorCode %}
    if (result != GIT_OK) {
    {%each args|argsInfo as arg %}
      {%if arg.shouldAlloc %}
      free({{ arg.name }});
      {%elsif arg | isOid %}
      if (info[{{ arg.jsArg }}].IsString()) {
        free({{ arg.name }});
      }
      {%endif%}
    {%endeach%}

      if (giterr_last()) {
        Napi::Error::New(env, giterr_last()->message).ThrowAsJavaScriptException();
        return env.Null();
      } else {
        Napi::Error::New(env, "Unknown Error").ThrowAsJavaScriptException();
        return env.Null();
      }
    }
  {%endif%}

  {% if cppFunctionName == "Free" %}
    Napi::ObjectWrap::Unwrap<{{ cppClassName }}>(info.This())->ClearValue();
  }
  {% endif %}


  {%each args|argsInfo as arg %}
    {%if arg | isOid %}
    if (info[{{ arg.jsArg }}].IsString()) {
      free((void *)from_{{ arg.name }});
    }
    {%endif%}
  {%endeach%}

  {%if not .|returnsCount %}
    return return scope.Escape(env.Undefined());
  {%else%}
    {%if return.cType | isPointer %}
    // null checks on pointers
    if (!result) {
      return return scope.Escape(env.Undefined());
    }
    {%endif%}

    Napi::Value to;
    {%if .|returnsCount > 1 %}
    v8::Napi::Object toReturn = Napi::Object::New(env);
    {%endif%}
    {%each .|returnsInfo as _return %}
      {%partial convertToV8 _return %}
      {%if .|returnsCount > 1 %}
    (toReturn).Set(Napi::String::New(env, "{{ _return.returnNameOrName }}"), to);
      {%endif%}
    {%endeach%}
    {%if .|returnsCount == 1 %}
    return return scope.Escape(to);
    {%else%}
    return return scope.Escape(toReturn);
    {%endif%}
  {%endif%}
  }
}
