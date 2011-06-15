
#include <v8.h>
#include <node.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

using namespace std;
using namespace node;
using namespace v8;

static Handle<Value> DoSomethingAsync (const Arguments&);
static int DoSomething (eio_req *);
static int DoSomething_After (eio_req *);
extern "C" void init (Handle<Object>);

struct simple_request {
  int x;
  int y;
  char *name;
  Persistent<Function> cb;
};

static Handle<Value> DoSomethingAsync (const Arguments& args) {
  HandleScope scope;
  const char *usage = "usage: doSomething(x, y, name, cb)";
  
  // validate argument count
  if (args.Length() != 4) {
    return ThrowException(Exception::Error(String::New(usage)));
  }
  
  // read arguments
  int x = args[0]->Int32Value();
  int y = args[1]->Int32Value();
  String::Utf8Value name(args[2]);
  Local<Function> cb = Local<Function>::Cast(args[3]);

  // init request
  simple_request *sr = new simple_request();
  sr->name = new char[name.length() + 1];

  // assign arguments to request fields
  sr->cb = Persistent<Function>::New(cb);
  strncpy(sr->name, *name, name.length() + 1);
  sr->x = x;
  sr->y = y;

  eio_custom(DoSomething, EIO_PRI_DEFAULT, DoSomething_After, sr);
  ev_ref(EV_DEFAULT_UC);
  return Undefined();
}

// this function happens on the thread pool
// doing v8 things in here will make bad happen.
static int DoSomething (eio_req *req) {
  struct simple_request * sr = (struct simple_request *)req->data;
  sleep(2); // just to make it less pointless to be async.
  req->result = sr->x + sr->y;
  return 0;
}

static int DoSomething_After (eio_req *req) {
  HandleScope scope;
  ev_unref(EV_DEFAULT_UC);
  
  struct simple_request * sr = (struct simple_request *)req->data;
  
  Local<Value> argv[3];
  argv[0] = Local<Value>::New(Null());
  argv[1] = Integer::New(req->result);
  argv[2] = String::New(sr->name);
  
  //call callback
  TryCatch try_catch;
  sr->cb->Call(Context::GetCurrent()->Global(), 3, argv);
  if (try_catch.HasCaught()) {
    FatalException(try_catch);
  }
  
  // cleanup
  sr->cb.Dispose();
  delete[] sr->name;
  delete sr;
  
  return 0;
}

extern "C" void init (Handle<Object> target) {
  HandleScope scope;
  NODE_SET_METHOD(target, "doSomething", DoSomethingAsync);
}