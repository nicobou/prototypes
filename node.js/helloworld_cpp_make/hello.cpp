#include <v8.h>

extern "C"
void init(v8::Handle<v8::Object> target) 
{
    v8::HandleScope scope;
    target->Set(v8::String::New("hello"), v8::String::New("World"));
}

