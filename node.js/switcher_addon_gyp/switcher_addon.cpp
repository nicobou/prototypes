#define BUILDING_NODE_EXTENSION
#include <node.h>

#include <switcher/base-entity-manager.h>

static switcher::BaseEntityManager switcher_manager;

v8::Handle<v8::Value> Create(const v8::Arguments& args) {
  v8::HandleScope scope;
  
  if (args.Length() < 1) {
    ThrowException(v8::Exception::TypeError(v8::String::New("Wrong number of arguments")));
    return scope.Close(v8::Undefined());
  }

  if (!args[0]->IsString() ) {
    ThrowException(v8::Exception::TypeError(v8::String::New("Wrong arguments")));
    return scope.Close(v8::Undefined());
  }
  
  v8::String::AsciiValue val(args[0]->ToString());

  v8::Local<v8::String> name = v8::String::New(switcher_manager.create(std::string(*val)).c_str());
  //v8::Local<v8::String> name = v8::String::New("truc");
  return scope.Close(name);
}

v8::Handle<v8::Value> Invoke(const v8::Arguments& args) {
  v8::HandleScope scope;
  
  if (args.Length() < 3) {
    ThrowException(v8::Exception::TypeError(v8::String::New("Wrong number of arguments")));
    return scope.Close(v8::Undefined());
  }

  if (!args[0]->IsString() || !args[1]->IsString() || !args[2]->IsArray ()) {
    ThrowException(v8::Exception::TypeError(v8::String::New("Wrong arguments")));
    return scope.Close(v8::Undefined());
  }
  
  v8::String::AsciiValue element_name(args[0]->ToString());

  v8::String::AsciiValue function_name(args[1]->ToString());

  v8::Local<v8::Object> obj_arguments = args[2]->ToObject();
  v8::Local<v8::Array> arguments = obj_arguments->GetPropertyNames();

  std::vector<std::string> vector_arg;
 
  for(unsigned int i = 0; i < arguments->Length(); i++) {
    v8::String::AsciiValue val(obj_arguments->Get(i)->ToString());
    vector_arg.push_back(std::string(*val));
    }

  v8::Handle<v8::Boolean> res = v8::Boolean::New(switcher_manager.invoke(std::string(*element_name),
									 std::string(*function_name),
									 vector_arg));
  return scope.Close(res);
}

v8::Handle<v8::Value> SetProperty(const v8::Arguments& args) {
  v8::HandleScope scope;
  
  if (args.Length() != 3) {
    ThrowException(v8::Exception::TypeError(v8::String::New("Wrong number of arguments")));
    return scope.Close(v8::Undefined());
  }
  
  if (!args[0]->IsString() || !args[1]->IsString() || !args[2]->IsString ()) {
    ThrowException(v8::Exception::TypeError(v8::String::New("Wrong arguments")));
    return scope.Close(v8::Undefined());
  }
  
  v8::String::AsciiValue element_name(args[0]->ToString());
  v8::String::AsciiValue property_name(args[1]->ToString());
  v8::String::AsciiValue property_val(args[2]->ToString());

 
  v8::Handle<v8::Boolean> res = v8::Boolean::New(switcher_manager.set_property(std::string(*element_name),
									       std::string(*property_name),
									       std::string(*property_val)));
  return scope.Close(res);
}


// ------------ node init functions -------------------------------
void Init(v8::Handle<v8::Object> target) {

  gst_init (NULL,NULL);
  GMainLoop *loop = g_main_loop_new (NULL, FALSE);  

  target->Set(v8::String::NewSymbol("create"),
	      v8::FunctionTemplate::New(Create)->GetFunction());  
  
  target->Set(v8::String::NewSymbol("invoke"),
	      v8::FunctionTemplate::New(Invoke)->GetFunction());  

  target->Set(v8::String::NewSymbol("set"),
	      v8::FunctionTemplate::New(SetProperty)->GetFunction());  
  
}

NODE_MODULE(switcher_addon, Init)
