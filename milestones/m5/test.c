#include <stdint.h>
#include <string.h>
#include <stdlib.h>

typedef __SIZE_TYPE__ size_t;

#define WASM_EXPORT(name) \
  __attribute__((export_name(#name))) \
  name
#define WASM_IMPORT(mod, name) \
  __attribute__((import_module(#mod))) \
  __attribute__((import_name(#name))) \
  name

#define ASSERT(x) do { if (!(x)) __builtin_trap(); } while (0)

typedef __externref_t externref;

void WASM_IMPORT(rt, invoke)(externref, int);
void WASM_IMPORT(rt, out_of_memory)(void);

// Useful for debugging.
void WASM_IMPORT(env, wasm_log)(void*);
void WASM_IMPORT(env, wasm_logobj)(externref);
void WASM_IMPORT(env, wasm_logi)(int);
externref WASM_IMPORT(env, window)(void);
externref WASM_IMPORT(env, to_jsstring)(char*);
externref WASM_IMPORT(env, to_jsnumber)(int*);
externref WASM_IMPORT(env, js_property_get)(externref, externref);

void *malloc(size_t size);
void free(void *p);
                          
typedef uint32_t Handle;

struct freelist {
  Handle handle;
  struct freelist *next;
};

static struct freelist *freelist = 0;

static Handle freelist_pop (void) {
  ASSERT(freelist != 0x0);
  struct freelist *head = freelist;
  Handle ret = head->handle;
  freelist = head->next;
  free(head);
  return ret;
}

struct freelist *tmpHead;
static void init(void) {
  tmpHead = malloc(sizeof(struct freelist));
}
static void freelist_push (Handle h) {
  struct freelist *head = malloc(sizeof(struct freelist));
  if (!head) {
    out_of_memory();
    __builtin_trap();
  }
  head->handle = h;
  head->next = freelist;
  freelist = head;
}

static externref objects[0];

static void expand_table (void) {
  size_t old_size = __builtin_wasm_table_size(objects);
  size_t grow = (old_size >> 1) + 1;
  if (__builtin_wasm_table_grow(objects,
                                __builtin_wasm_ref_null_extern(),
                                grow) == -1) {
    out_of_memory();
    __builtin_trap();
  }
  size_t end = __builtin_wasm_table_size(objects);
  while (end != old_size) {
    freelist_push (--end);
  }
}

static Handle intern(externref obj) {
  if (!freelist) expand_table();
  Handle ret = freelist_pop();
  __builtin_wasm_table_set(objects, ret, obj);
  return ret;
}

static void release(Handle h) {
  if (h == -1) return;
  __builtin_wasm_table_set(objects, h, __builtin_wasm_ref_null_extern());
  freelist_push(h);
}

static externref handle_value(Handle h) {
  return h == -1
    ? __builtin_wasm_ref_null_extern()
    : __builtin_wasm_table_get(objects, h);
}

static Handle jsGetProp(Handle obj, char *prop) {
  externref jsObj = handle_value(obj);
  externref jsProp = to_jsstring(prop);
  externref jsVal = js_property_get(jsObj, jsProp);
  return intern(jsVal);
}

static Handle logObject(Handle obj){
  externref jsObj = handle_value(obj);
  wasm_logobj(jsObj);
  return obj;
} 

struct obj {
  uintptr_t callback_handle;
  uint32_t hello;
};

void WASM_EXPORT(run)(){
  Handle w = intern(window());
  Handle h = jsGetProp(w, "document");
  wasm_logobj(handle_value(h));
}

struct obj* WASM_EXPORT(make_obj)() {
  struct obj* obj = malloc(sizeof(struct obj));
  if (!obj) {
    out_of_memory();
    __builtin_trap();
  }
  obj->callback_handle = -1;
  obj->hello = 1;
  return obj;
}

void WASM_EXPORT(free_obj)(struct obj* obj) {
  uintptr_t handle = obj->callback_handle;
  free(obj);
  release(handle);
}

void WASM_EXPORT(attach_callback)(struct obj* obj, externref callback) {
  release(obj->callback_handle);
  obj->callback_handle = intern(callback);
}

void WASM_EXPORT(invoke_callback)(struct obj* obj, int arg) {
  invoke(handle_value(obj->callback_handle), arg - 1);
}
