
{%partial doc .%}
NAN_METHOD({{ cppClassName }}::{{ cppFunctionName }}) {
  {%partial guardArguments .%}
  if (info.Length() == {{args|jsArgsCount}} || !info[{{args|jsArgsCount}}]->IsFunction()) {
    Napi::Error::New(env, "Callback is required and must be a Function.").ThrowAsJavaScriptException();
    return env.Null();
  }

  {{ cppFunctionName }}Baton* baton = new {{ cppFunctionName }}Baton;

  baton->error_code = GIT_OK;
  baton->error = NULL;

  {%each args|argsInfo as arg %}
    {%if arg.globalPayload %}
  {{ cppFunctionName }}_globalPayload* globalPayload = new {{ cppFunctionName }}_globalPayload;
    {%endif%}
  {%endeach%}

  {%each args|argsInfo as arg %}
    {%if not arg.isReturn %}
      {%if arg.isSelf %}
  baton->{{ arg.name }} = Napi::ObjectWrap::Unwrap<{{ arg.cppClassName }}>(info.This())->GetValue();
      {%elsif arg.isCallbackFunction %}
  if (!info[{{ arg.jsArg }}]->IsFunction()) {
    baton->{{ arg.name }} = NULL;
        {%if arg.payload.globalPayload %}
    globalPayload->{{ arg.name }} = NULL;
        {%else%}
    baton->{{ arg.payload.name }} = NULL;
        {%endif%}
  }
  else {
    baton->{{ arg.name}} = {{ cppFunctionName }}_{{ arg.name }}_cppCallback;
        {%if arg.payload.globalPayload %}
    globalPayload->{{ arg.name }} = new Napi::FunctionReference(info[{{ arg.jsArg }}].As<Napi::Function>());
        {%else%}
    baton->{{ arg.payload.name }} = new Napi::FunctionReference(info[{{ arg.jsArg }}].As<Napi::Function>());
        {%endif%}
  }
      {%elsif arg.payloadFor %}
        {%if arg.globalPayload %}
  baton->{{ arg.name }} = globalPayload;
        {%endif%}
      {%elsif arg.name %}
  {%partial convertFromV8 arg%}
        {%if not arg.payloadFor %}
  baton->{{ arg.name }} = from_{{ arg.name }};
          {%if arg | isOid %}
  baton->{{ arg.name }}NeedsFree = info[{{ arg.jsArg }}].IsString();
          {%endif%}
        {%endif%}
      {%endif%}
    {%elsif arg.shouldAlloc %}
      baton->{{arg.name}} = ({{ arg.cType }})malloc(sizeof({{ arg.cType|replace '*' '' }}));
      {%if arg.cppClassName == "GitBuf" %}
        baton->{{arg.name}}->ptr = NULL;
        baton->{{arg.name}}->size = baton->{{arg.name}}->asize = 0;
      {%endif%}
    {%endif%}
  {%endeach%}

  Napi::FunctionReference *callback = new Napi::FunctionReference(info[{{args|jsArgsCount}}].As<v8::Napi::Function>());
  {{ cppFunctionName }}Worker *worker = new {{ cppFunctionName }}Worker(baton, callback);
  {%each args|argsInfo as arg %}
    {%if not arg.isReturn %}
      {%if arg.isSelf %}
  worker->SaveToPersistent("{{ arg.name }}", info.This());
      {%elsif not arg.isCallbackFunction %}
  if (!info[{{ arg.jsArg }}]->IsUndefined() && !info[{{ arg.jsArg }}]->IsNull())
    worker->SaveToPersistent("{{ arg.name }}", info[{{ arg.jsArg }}]->ToObject());
      {%endif%}
    {%endif%}
  {%endeach%}

  AsyncLibgit2QueueWorker(worker);
  return;
}

void {{ cppClassName }}::{{ cppFunctionName }}Worker::Execute() {
  giterr_clear();

  {
    LockMaster lockMaster(/*asyncAction: */true{%each args|argsInfo as arg %}
      {%if arg.cType|isPointer%}{%if not arg.cType|isDoublePointer%}
        ,baton->{{ arg.name }}
      {%endif%}{%endif%}
    {%endeach%});

  {%if .|hasReturnType %}
  {{ return.cType }} result = {{ cFunctionName }}(
  {%else%}
  {{ cFunctionName }}(
  {%endif%}
    {%-- Insert Function Arguments --%}
    {%each args|argsInfo as arg %}
      {%-- turn the pointer into a ref --%}
    {%if arg.isReturn|and arg.cType|isDoublePointer %}&{%endif%}baton->{{ arg.name }}{%if not arg.lastArg %},{%endif%}

    {%endeach%}
    );

    {%if return.isResultOrError %}
    baton->error_code = result;
    if (result < GIT_OK && giterr_last() != NULL) {
      baton->error = git_error_dup(giterr_last());
    }

    {%elsif return.isErrorCode %}
    baton->error_code = result;

    if (result != GIT_OK && giterr_last() != NULL) {
      baton->error = git_error_dup(giterr_last());
    }

    {%elsif not return.cType == 'void' %}

    baton->result = result;

    {%endif%}
  }
}

void {{ cppClassName }}::{{ cppFunctionName }}Worker::OnOK() {
  {%if return.isResultOrError %}
  if (baton->error_code >= GIT_OK) {
  {%else%}
  if (baton->error_code == GIT_OK) {
  {%endif%}
    {%if return.isResultOrError %}
    Napi::Value result = Napi::Number::New(env, baton->error_code);

    {%elsif not .|returnsCount %}
    Napi::Value result = env.Undefined();
    {%else%}
    Napi::Value to;
      {%if .|returnsCount > 1 %}
    v8::Napi::Object result = Napi::Object::New(env);
      {%endif%}
      {%each .|returnsInfo 0 1 as _return %}
        {%partial convertToV8 _return %}
        {%if .|returnsCount > 1 %}
    (result).Set(Napi::String::New(env, "{{ _return.returnNameOrName }}"), to);
        {%endif%}
      {%endeach%}
      {%if .|returnsCount == 1 %}
    Napi::Value result = to;
      {%endif%}
    {%endif%}
    Napi::Value argv[2] = {
      env.Null(),
      result
    };
    callback->Call(2, argv, async_resource);
  } else {
    if (baton->error) {
      Napi::Object err;
      if (baton->error->message) {
        err = Napi::Error::New(env, baton->error->message)->ToObject();
      } else {
        err = Napi::Error::New(env, "Method {{ jsFunctionName }} has thrown an error.")->ToObject();
      }
      err.Set(Napi::String::New(env, "errno"), Napi::New(env, baton->error_code));
      err.Set(Napi::String::New(env, "errorFunction"), Napi::String::New(env, "{{ jsClassName }}.{{ jsFunctionName }}"));
      Napi::Value argv[1] = {
        err
      };
      callback->Call(1, argv, async_resource);
      if (baton->error->message)
        free((void *)baton->error->message);
      free((void *)baton->error);
    } else if (baton->error_code < 0) {
      std::queue< Napi::Value > workerArguments;
{%each args|argsInfo as arg %}
  {%if not arg.isReturn %}
    {%if not arg.isSelf %}
      {%if not arg.isCallbackFunction %}
      workerArguments.push(GetFromPersistent("{{ arg.name }}"));
      {%endif%}
    {%endif%}
  {%endif%}
{%endeach%}
      bool callbackFired = false;
      while(!workerArguments.empty()) {
        Napi::Value node = workerArguments.front();
        workerArguments.pop();

        if (
          !node.IsObject()
          || node->IsArray()
          || node->IsBooleanObject()
          || node->IsDate()
          || node->IsFunction()
          || node->IsNumberObject()
          || node->IsRegExp()
          || node->IsStringObject()
        ) {
          continue;
        }

        Napi::Object nodeObj = node->ToObject();
        Napi::Value checkValue = GetPrivate(nodeObj, Napi::String::New(env, "NodeGitPromiseError"));

        if (!checkValue.IsEmpty() && !checkValue->IsNull() && !checkValue->IsUndefined()) {
          Napi::Value argv[1] = {
            checkValue->ToObject()
          };
          callback->Call(1, argv, async_resource);
          callbackFired = true;
          break;
        }

        Napi::Array properties = nodeObj->GetPropertyNames();
        for (unsigned int propIndex = 0; propIndex < properties->Length(); ++propIndex) {
          Napi::String propName = properties->Get(propIndex)->ToString();
          Napi::Value nodeToQueue = nodeObj->Get(propName);
          if (!nodeToQueue->IsUndefined()) {
            workerArguments.push(nodeToQueue);
          }
        }
      }

      if (!callbackFired) {
        Napi::Object err = Napi::Error::New(env, "Method {{ jsFunctionName }} has thrown an error.")->ToObject();
        err.Set(Napi::String::New(env, "errno"), Napi::New(env, baton->error_code));
        err.Set(Napi::String::New(env, "errorFunction"), Napi::String::New(env, "{{ jsClassName }}.{{ jsFunctionName }}"));
        Napi::Value argv[1] = {
          err
        };
        callback->Call(1, argv, async_resource);
      }
    } else {
      callback->Call(0, NULL, async_resource);
    }

    {%each args|argsInfo as arg %}
      {%if arg.shouldAlloc %}
        {%if not arg.isCppClassStringOrArray %}
        {%elsif arg | isOid %}
    if (baton->{{ arg.name}}NeedsFree) {
      baton->{{ arg.name}}NeedsFree = false;
      free((void*)baton->{{ arg.name }});
    }
        {%elsif arg.isCallbackFunction %}
          {%if not arg.payload.globalPayload %}
    delete baton->{{ arg.payload.name }};
          {%endif%}
        {%elsif arg.globalPayload %}
    delete ({{ cppFunctionName}}_globalPayload*)baton->{{ arg.name }};
        {%else%}
    free((void*)baton->{{ arg.name }});
        {%endif%}
      {%endif%}
    {%endeach%}
  }

  {%each args|argsInfo as arg %}
    {%if arg.isCppClassStringOrArray %}
      {%if arg.freeFunctionName %}
  {{ arg.freeFunctionName }}(baton->{{ arg.name }});
      {%elsif not arg.isConst%}
  free((void *)baton->{{ arg.name }});
      {%endif%}
    {%elsif arg | isOid %}
  if (baton->{{ arg.name}}NeedsFree) {
    baton->{{ arg.name}}NeedsFree = false;
    free((void *)baton->{{ arg.name }});
  }
    {%elsif arg.isCallbackFunction %}
      {%if not arg.payload.globalPayload %}
  delete baton->{{ arg.payload.name }};
      {%endif%}
    {%elsif arg.globalPayload %}
  delete ({{ cppFunctionName}}_globalPayload*)baton->{{ arg.name }};
    {%endif%}
    {%if arg.cppClassName == "GitBuf" %}
      {%if cppFunctionName == "Set" %}
      {%else%}
        git_buf_free(baton->{{ arg.name }});
        free((void *)baton->{{ arg.name }});
      {%endif%}
    {%endif%}
  {%endeach%}

  delete baton;
}

{%partial callbackHelpers .%}
