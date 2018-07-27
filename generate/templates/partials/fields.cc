{% each fields|fieldsInfo as field %}
  {% if not field.ignore %}
    NAN_METHOD({{ cppClassName }}::{{ field.cppFunctionName }}) {
      Napi::Value to;

      {% if field | isFixedLengthString %}
      char* {{ field.name }} = (char *)Napi::ObjectWrap::Unwrap<{{ cppClassName }}>(info.This())->GetValue()->{{ field.name }};
      {% else %}
      {{ field.cType }}
        {% if not field.cppClassName|isV8Value %}
          {% if not field.cType|isPointer %}
        *
          {% endif %}
        {% endif %}
        {{ field.name }} =
        {% if not field.cppClassName|isV8Value %}
          {% if not field.cType|isPointer %}
        &
          {% endif %}
        {% endif %}
        Napi::ObjectWrap::Unwrap<{{ cppClassName }}>(info.This())->GetValue()->{{ field.name }};
      {% endif %}

      {% partial convertToV8 field %}
      return to;
    }
  {% endif %}
{% endeach %}
