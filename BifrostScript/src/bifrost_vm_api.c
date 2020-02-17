#include "bifrost_vm.h"

#include "bifrost_vm_debug.h"
#include "bifrost_vm_gc.h"
#include <bifrost/data_structures/bifrost_dynamic_string.h>
#include <string.h>  // memset, memmove

struct bfValueHandle_t
{
  bfVMValue     value;
  bfValueHandle prev;
  bfValueHandle next;
};

bfVMValue bfVM_getHandleValue(bfValueHandle h)
{
  return h->value;
}

bfValueHandle bfVM_getHandleNext(bfValueHandle h)
{
  return h->next;
}

BifrostObjModule*     bfVM_findModule(BifrostVM* self, const char* name, size_t name_len);
uint32_t              bfVM_getSymbol(BifrostVM* self, bfStringRange name);
static BifrostVMError bfVM_runModule(BifrostVM* self, BifrostObjModule* module);
static BifrostVMError bfVM_compileIntoModule(BifrostVM* self, BifrostObjModule* module, const char* source, size_t source_len);

void bfVMParams_init(BifrostVMParams* self)
{
  self->error_fn           = NULL;                 /* errors will have to be check with return values and 'bfVM_errorString' */
  self->print_fn           = NULL;                 /* 'print' will be a no op.                                               */
  self->module_fn          = NULL;                 /* unable to load user modules                                            */
  self->memory_fn          = bfGCDefaultAllocator; /* uses c library's realloc and free by default                           */
  self->min_heap_size      = 1000000;              /* 1mb                                                                    */
  self->heap_size          = 5242880;              /* 5mb                                                                    */
  self->heap_growth_factor = 0.5f;                 /* Grow by x1.5                                                           */
  self->user_data          = NULL;                 /* User data for the memory allocator, and maybe other future things.     */
}

static inline void bfVM_assertStackIndex(const BifrostVM* self, size_t idx)
{
  const size_t size = Array_size(&self->stack);

  if (!(idx < size))
  {
    assert(idx < size && "Invalid index passed into bfVM_stack* function.");
  }
}

BifrostVM* bfVM_new(const BifrostVMParams* params)
{
  BifrostVM* self = params->memory_fn(params->user_data, NULL, 0u, sizeof(BifrostVM), sizeof(void*));

  if (self)
  {
    bfVM_ctor(self, params);
  }

  return self;
}

static unsigned ModuleMap_hash(const void* key)
{
  return ((const BifrostObjStr*)key)->hash;
}

static int ModuleMap_cmp(const void* lhs, const void* rhs)
{
  const BifrostObjStr* str_lhs = (const BifrostObjStr*)lhs;
  const BifrostObjStr* str_rhs = (const BifrostObjStr*)rhs;

  return str_lhs->hash == str_rhs->hash && String_cmp(str_lhs->value, str_rhs->value) == 0;
}

void bfVM_ctor(BifrostVM* self, const BifrostVMParams* params)
{
  memset(self, 0x0, sizeof(*self));

  self->frames            = OLD_bfArray_newA(self->frames, 12);
  self->stack             = OLD_bfArray_newA(self->stack, 10);
  self->stack_top         = self->stack;
  self->symbols           = OLD_bfArray_newA(self->symbols, 10);
  self->params            = *params;
  self->gc_object_list    = NULL;
  self->last_error        = String_new("");
  self->bytes_allocated   = sizeof(BifrostVM);
  self->handles           = NULL;
  self->free_handles      = NULL;
  self->parser_stack      = NULL;
  self->temp_roots_top    = 0;
  self->gc_is_running     = bfFalse;
  self->finalized         = NULL;
  self->current_native_fn = NULL;

  /*
    NOTE(Shareef):
      Custom dtor are not needed as the strings being
      stored in the map will be garbage collected.
  */
  BifrostHashMapParams hash_params;
  bfHashMapParams_init(&hash_params);
  hash_params.hash       = ModuleMap_hash;
  hash_params.cmp        = ModuleMap_cmp;
  hash_params.value_size = sizeof(BifrostObjModule*);
  bfHashMap_ctor(&self->modules, &hash_params);

  self->build_in_symbols[BIFROST_VM_SYMBOL_CTOR] = bfVM_getSymbol(self, bfMakeStringRangeC("ctor"));
  self->build_in_symbols[BIFROST_VM_SYMBOL_DTOR] = bfVM_getSymbol(self, bfMakeStringRangeC("dtor"));
  self->build_in_symbols[BIFROST_VM_SYMBOL_CALL] = bfVM_getSymbol(self, bfMakeStringRangeC("call"));
}

static inline BifrostVMError bfVM__moduleMake(BifrostVM* self, const char* module, BifrostObjModule** out)
{
  static const char* ANON_MODULE_NAME = "__anon_module__";

  const bfBool32 is_anon = module == NULL;

  if (is_anon)
  {
    module = ANON_MODULE_NAME;
  }

  // TODO(SR):
  //   Make it so this check only happens in debug builds??
  const bfStringRange name_range = bfMakeStringRangeC(module);
  *out                           = bfVM_findModule(self, module, bfStringRange_length(&name_range));

  if (*out)
  {
    return BIFROST_VM_ERROR_MODULE_ALREADY_DEFINED;
  }

  *out = bfVM_createModule(self, name_range);

  if (!is_anon)
  {
    bfGCPushRoot(self, &(*out)->super);
    BifrostObjStr* const module_name = bfVM_createString(self, name_range);
    bfHashMap_set(&self->modules, module_name, out);
    bfGCPopRoot(self);
  }

  return BIFROST_VM_ERROR_NONE;
}

BifrostVMError bfVM_moduleMake(BifrostVM* self, size_t idx, const char* module)
{
  bfVM_assertStackIndex(self, idx);

  BifrostObjModule* temp;
  BifrostVMError    err = bfVM__moduleMake(self, module, &temp);
  self->stack_top[idx]  = FROM_POINTER(temp);
  return err;
}

static void bfVM_moduleLoadStdIOPrint(BifrostVM* vm, int32_t num_args)
{
  const bfPrintFn print = vm->params.print_fn;

  if (print && num_args)
  {
    // TODO: Make this safer and better
    char buffer[1024];

    char* buffer_head = buffer;
    char* buffer_end  = buffer + sizeof(buffer);

    for (int32_t i = 0; i < num_args && buffer < buffer_end; ++i)
    {
      const bfVMValue value         = vm->stack_top[i];
      const size_t    written_bytes = bfDbgValueToString(value, buffer_head, buffer_end - buffer_head);

      buffer_head += written_bytes;
    }

    print(vm, buffer);
  }
}

void bfVM_moduleLoadStd(BifrostVM* self, size_t idx, uint32_t module_flags)
{
  if (module_flags & BIFROST_VM_STD_MODULE_IO)
  {
    if (bfVM_moduleMake(self, idx, "std:io") == BIFROST_VM_ERROR_NONE)
    {
      bfVM_stackStoreNativeFn(self, idx, "print", &bfVM_moduleLoadStdIOPrint, -1);
    }
  }
}

BifrostVMError bfVM_moduleLoad(BifrostVM* self, size_t idx, const char* module)
{
  bfVM_assertStackIndex(self, idx);

  BifrostObjModule* const module_obj = bfVM_findModule(self, module, strlen(module));

  if (module_obj)
  {
    self->stack_top[idx] = FROM_POINTER(module_obj);
    return BIFROST_VM_ERROR_NONE;
  }

  return BIFROST_VM_ERROR_MODULE_NOT_FOUND;
}

typedef struct
{
  const char* str;
  size_t      str_len;
  unsigned    hash;

} TempModuleName;

static int bfVM_moduleUnloadCmp(const void* lhs, const void* rhs)
{
  const TempModuleName* str_lhs = (const TempModuleName*)lhs;
  const BifrostObjStr*  str_rhs = (const BifrostObjStr*)rhs;

  return str_lhs->hash == str_rhs->hash && String_ccmpn(str_rhs->value, str_lhs->str, str_lhs->str_len) == 0;
}

void bfVM_moduleUnload(BifrostVM* self, const char* module)
{
  /*
    NOTE(Shareef):
      The GC will handle deleting the module and
      string whenever we are low on memory.
   */
  const TempModuleName tmn =
   {
    .str     = module,
    .str_len = strlen(module),
    .hash    = bfString_hash(module)};

  bfHashMap_removeCmp(&self->modules, &tmn, &bfVM_moduleUnloadCmp);
}

void bfVM_moduleUnloadAll(BifrostVM* self)
{
  bfHashMap_clear(&self->modules);
}

size_t bfVM_stackSize(const BifrostVM* self)
{
  return Array_size(&self->stack) - (self->stack_top - self->stack);
}

BifrostVMError bfVM_stackResize(BifrostVM* self, size_t size)
{
  const size_t stack_size     = Array_size(&self->stack);
  const size_t stack_used     = self->stack_top - self->stack;
  const size_t requested_size = stack_used + size;

  if (stack_size < requested_size)
  {
    /*
      TODO(SR):
        Make "Array_resize" return whether or not the realloc was successful.
    */
    Array_resize(&self->stack, requested_size);
    self->stack_top = self->stack + stack_used;
  }

  return BIFROST_VM_ERROR_NONE;
}

bfVMValue bfVM_stackFindVariable(BifrostObjModule* module_obj, const char* variable, size_t variable_len)
{
  assert(module_obj && "bfVM_stackFindVariable: Module must not be null.");

  const size_t num_vars = Array_size(&module_obj->variables);

  for (size_t i = 0; i < num_vars; ++i)
  {
    BifrostVMSymbol* const var = module_obj->variables + i;

    if (String_length(var->name) == variable_len && strncmp(variable, var->name, variable_len) == 0)
    {
      return var->value;
    }
  }

  return VAL_NULL;
}

static int bfVM__stackStoreVariable(BifrostVM* self, bfVMValue obj, bfStringRange field_symbol, bfVMValue value);

void bfVM_stackMakeInstance(BifrostVM* self, size_t clz_idx, size_t dst_idx)
{
  bfVM_assertStackIndex(self, clz_idx);
  bfVM_assertStackIndex(self, dst_idx);

  bfVMValue clz_value = self->stack_top[clz_idx];

  // TODO(SR): Only in Debug Builds
  assert(IS_POINTER(clz_value) && "The value being read is not an object.");

  BifrostObj* obj = AS_POINTER(clz_value);

  // TODO(SR): Only in Debug Builds
  assert(obj->type == BIFROST_VM_OBJ_CLASS && "This object is not a class.");

  self->stack_top[dst_idx] = FROM_POINTER(bfVM_createInstance(self, (BifrostObjClass*)obj));
}

void* bfVM_stackMakeReference(BifrostVM* self, size_t idx, size_t extra_data_size)
{
  bfVM_assertStackIndex(self, idx);
  self->stack_top[idx] = FROM_POINTER(bfVM_createReference(self, extra_data_size));

  return bfVM_stackReadInstance(self, idx);
}

static BifrostObjModule* bfVM__findModule(bfVMValue obj)
{
  if (!IS_POINTER(obj))
  {
    return NULL;
  }

  BifrostObj* obj_ptr = AS_POINTER(obj);

  if (obj_ptr->type == BIFROST_VM_OBJ_INSTANCE)
  {
    BifrostObjInstance* inst = (BifrostObjInstance*)obj_ptr;
    return inst->clz->module;
  }
  else if (obj_ptr->type == BIFROST_VM_OBJ_CLASS)
  {
    BifrostObjClass* clz = (BifrostObjClass*)obj_ptr;
    return clz->module;
  }
  else if (obj_ptr->type == BIFROST_VM_OBJ_MODULE)
  {
    return (BifrostObjModule*)obj_ptr;
  }

  return NULL;
}

BifrostObjClass* createClassBinding(BifrostVM* self, bfVMValue obj, const BifrostVMClassBind* clz_bind)
{
  BifrostObjModule* module_obj = bfVM__findModule(obj);

  if (module_obj == NULL)
  {
    return NULL;
  }

  const bfStringRange      name   = bfMakeStringRangeC(clz_bind->name);
  BifrostObjClass* const   clz    = bfVM_createClass(self, module_obj, name, NULL, clz_bind->extra_data_size);
  const BifrostMethodBind* method = clz_bind->methods;

  clz->finalizer = clz_bind->finalizer;

  bfGCPushRoot(self, &clz->super);
  if (bfVM__stackStoreVariable(self, obj, name, FROM_POINTER(clz)))
  {
    bfGCPopRoot(self);

    return NULL;
  }

  while (method->name && method->fn)
  {
    BifrostObjNativeFn* const fn = bfVM_createNativeFn(self, method->fn, method->arity, method->num_statics);

    bfGCPushRoot(self, &fn->super);
    bfVM_xSetVariable(&clz->symbols, self, bfMakeStringRangeC(method->name), FROM_POINTER(fn));
    bfGCPopRoot(self);

    ++method;
  }

  bfGCPopRoot(self);

  return clz;
}

void* bfVM_stackMakeReferenceClz(BifrostVM* self, size_t module_idx, const BifrostVMClassBind* clz_bind, size_t dst_idx)
{
  bfVM_assertStackIndex(self, module_idx);
  bfVM_assertStackIndex(self, dst_idx);

  BifrostObjReference* ref = bfVM_createReference(self, clz_bind->extra_data_size);
  self->stack_top[dst_idx] = FROM_POINTER(ref);
  ref->clz                 = createClassBinding(self, self->stack_top[module_idx], clz_bind);

  return ref->extra_data;
}

void bfVM_stackMakeWeakRef(BifrostVM* self, size_t idx, void* value)
{
  bfVM_assertStackIndex(self, idx);

  self->stack_top[idx] = FROM_POINTER(bfVM_createWeakRef(self, value));
}

bfBool32 bfVMGrabObjectsOfType(bfVMValue obj_a, bfVMValue obj_b, BifrostVMObjType type_a, BifrostVMObjType type_b, BifrostObj** out_a, BifrostObj** out_b)
{
  if (IS_POINTER(obj_a) && IS_POINTER(obj_b))
  {
    BifrostObj* const obj_a_ptr = BIFROST_AS_OBJ(obj_a);
    BifrostObj* const obj_b_ptr = BIFROST_AS_OBJ(obj_b);

    if (obj_a_ptr->type == type_a && obj_b_ptr->type == type_b)
    {
      *out_a = obj_a_ptr;
      *out_b = obj_b_ptr;
    }
  }

  return bfFalse;
}

void bfVM_referenceSetClass(BifrostVM* self, size_t idx, size_t clz_idx)
{
  bfVM_assertStackIndex(self, idx);
  bfVM_assertStackIndex(self, clz_idx);

  const bfVMValue obj     = self->stack_top[idx];
  const bfVMValue clz     = self->stack_top[clz_idx];
  BifrostObj*     obj_ptr = NULL;
  BifrostObj*     clz_ptr = NULL;

  if (bfVMGrabObjectsOfType(obj, clz, BIFROST_VM_OBJ_REFERENCE, BIFROST_VM_OBJ_CLASS, &obj_ptr, &clz_ptr))
  {
    ((BifrostObjReference*)obj_ptr)->clz = (BifrostObjClass*)clz_ptr;
  }
}

void bfVM_classSetBaseClass(BifrostVM* self, size_t idx, size_t clz_idx)
{
  bfVM_assertStackIndex(self, idx);
  bfVM_assertStackIndex(self, clz_idx);

  const bfVMValue obj     = self->stack_top[idx];
  const bfVMValue clz     = self->stack_top[clz_idx];
  BifrostObj*     obj_ptr = NULL;
  BifrostObj*     clz_ptr = NULL;

  if (bfVMGrabObjectsOfType(obj, clz, BIFROST_VM_OBJ_CLASS, BIFROST_VM_OBJ_CLASS, &obj_ptr, &clz_ptr))
  {
    ((BifrostObjClass*)obj_ptr)->base_clz = (BifrostObjClass*)clz_ptr;
  }
}

void bfVM_stackLoadVariable(BifrostVM* self, size_t dst_idx, size_t inst_or_class_or_module, const char* variable)
{
  bfVM_assertStackIndex(self, dst_idx);
  bfVM_assertStackIndex(self, inst_or_class_or_module);

  BifrostObj*         obj      = AS_POINTER(self->stack_top[inst_or_class_or_module]);
  const bfStringRange var_name = bfMakeStringRangeC(variable);
  const size_t        symbol   = bfVM_getSymbol(self, var_name);

  if (obj->type == BIFROST_VM_OBJ_INSTANCE)
  {
    BifrostObjInstance* const inst = (BifrostObjInstance*)obj;

    bfVMValue* value = bfHashMap_get(&inst->fields, self->symbols[symbol]);

    if (value)
    {
      self->stack_top[dst_idx] = *value;
      return;
    }

    /*
      NOTE(Shareef):
        Fall back to class if not on instance.
     */
    obj = &inst->clz->super;
  }

  if (obj->type == BIFROST_VM_OBJ_CLASS)
  {
    BifrostObjClass* const clz = (BifrostObjClass*)obj;

    if (symbol < Array_size(&clz->symbols))
    {
      self->stack_top[dst_idx] = clz->symbols[symbol].value;
    }
    else
    {
      self->stack_top[dst_idx] = VAL_NULL;
    }
  }
  else if (obj->type == BIFROST_VM_OBJ_MODULE)
  {
    BifrostObjModule* const module = (BifrostObjModule*)obj;
    self->stack_top[dst_idx]       = bfVM_stackFindVariable(module, var_name.bgn, bfStringRange_length(&var_name));
  }
  else
  {
    self->stack_top[dst_idx] = VAL_NULL;
  }
}

static int bfVM__stackStoreVariable(BifrostVM* self, bfVMValue obj, bfStringRange field_symbol, bfVMValue value)
{
  if (!IS_POINTER(obj))
  {
    return 1;
  }

  BifrostObj*         obj_ptr = AS_POINTER(obj);
  const size_t        symbol  = bfVM_getSymbol(self, field_symbol);
  const BifrostString sym_str = self->symbols[symbol];

  if (obj_ptr->type == BIFROST_VM_OBJ_INSTANCE)
  {
    BifrostObjInstance* inst = (BifrostObjInstance*)obj_ptr;

    bfHashMap_set(&inst->fields, sym_str, &value);
  }
  else if (obj_ptr->type == BIFROST_VM_OBJ_CLASS)
  {
    BifrostObjClass* clz = (BifrostObjClass*)obj_ptr;

    bfVM_xSetVariable(&clz->symbols, self, field_symbol, value);
  }
  else if (obj_ptr->type == BIFROST_VM_OBJ_MODULE)
  {
    BifrostObjModule* const module_obj = (BifrostObjModule*)obj_ptr;

    bfVM_xSetVariable(&module_obj->variables, self, field_symbol, value);
  }
  else
  {
    return 2;
  }

  return 0;
}

BifrostVMError bfVM_stackStoreVariable(BifrostVM* self, size_t inst_or_class_or_module, const char* field, size_t value_idx)
{
  bfVM_assertStackIndex(self, value_idx);
  bfVM_assertStackIndex(self, inst_or_class_or_module);

  const bfVMValue     obj      = self->stack_top[inst_or_class_or_module];
  const bfStringRange var_name = bfMakeStringRangeC(field);
  const bfVMValue     value    = self->stack_top[value_idx];

  if (bfVM__stackStoreVariable(self, obj, var_name, value))
  {
    return BIFROST_VM_ERROR_INVALID_OP_ON_TYPE;
  }

  return BIFROST_VM_ERROR_NONE;
}

BifrostVMError bfVM_stackStoreNativeFn(BifrostVM* self, size_t inst_or_class_or_module, const char* field, bfNativeFnT func, int32_t arity)
{
  return bfVM_stackStoreClosure(self, inst_or_class_or_module, field, func, arity, 0);
}

BifrostVMError bfVM_closureGetStatic(BifrostVM* self, size_t dst_idx, size_t static_idx)
{
  bfVM_assertStackIndex(self, dst_idx);
  bfVM_assertStackIndex(self, static_idx);

  const BifrostObjNativeFn* native_fn = self->current_native_fn;

  if (!native_fn || static_idx >= native_fn->num_statics)
  {
    return BIFROST_VM_ERROR_INVALID_ARGUMENT;
  }

  self->stack_top[dst_idx] = native_fn->statics[static_idx];

  return BIFROST_VM_ERROR_NONE;
}

BifrostVMError bfVM_stackStoreClosure(BifrostVM* self, size_t inst_or_class_or_module, const char* field, bfNativeFnT func, int32_t arity, uint32_t num_statics)
{
  bfVM_assertStackIndex(self, inst_or_class_or_module);

  const bfVMValue     obj      = self->stack_top[inst_or_class_or_module];
  const bfStringRange var_name = bfMakeStringRangeC(field);

  if (bfVM__stackStoreVariable(self, obj, var_name, FROM_POINTER(bfVM_createNativeFn(self, func, arity, num_statics))))
  {
    return BIFROST_VM_ERROR_INVALID_OP_ON_TYPE;
  }

  return BIFROST_VM_ERROR_NONE;
}

BifrostVMError bfVM_closureSetStatic(BifrostVM* self, size_t closure_idx, size_t static_idx, size_t value_idx)
{
  bfVM_assertStackIndex(self, closure_idx);
  bfVM_assertStackIndex(self, value_idx);

  const bfVMValue obj = self->stack_top[closure_idx];

  if (!IS_POINTER(obj))
  {
    return BIFROST_VM_ERROR_INVALID_OP_ON_TYPE;
  }

  BifrostObj* obj_ptr = BIFROST_AS_OBJ(obj);

  if (obj_ptr->type != BIFROST_VM_OBJ_NATIVE_FN)
  {
    return BIFROST_VM_ERROR_INVALID_OP_ON_TYPE;
  }

  BifrostObjNativeFn* native_fn = (BifrostObjNativeFn*)obj_ptr;

  if (static_idx >= native_fn->num_statics)
  {
    return BIFROST_VM_ERROR_INVALID_ARGUMENT;
  }

  native_fn->statics[static_idx] = self->stack_top[value_idx];

  return BIFROST_VM_ERROR_NONE;
}

BifrostVMError bfVM_stackStoreClass(BifrostVM* self, size_t inst_or_class_or_module, const BifrostVMClassBind* clz_bind)
{
  bfVM_assertStackIndex(self, inst_or_class_or_module);

  if (createClassBinding(self, self->stack_top[inst_or_class_or_module], clz_bind) == NULL)
  {
    return BIFROST_VM_ERROR_INVALID_OP_ON_TYPE;
  }

  return BIFROST_VM_ERROR_NONE;
}

void bfVM_stackSetString(BifrostVM* self, size_t idx, const char* value)
{
  bfVM_stackSetStringLen(self, idx, value, strlen(value));
}

void bfVM_stackSetStringLen(BifrostVM* self, size_t idx, const char* value, size_t len)
{
  bfVM_assertStackIndex(self, idx);
  self->stack_top[idx] = FROM_POINTER(bfVM_createString(self, (bfStringRange){.bgn = value, .end = value + len}));
}

void bfVM_stackSetNumber(BifrostVM* self, size_t idx, bfVMNumberT value)
{
  bfVM_assertStackIndex(self, idx);
  self->stack_top[idx] = FROM_NUMBER(value);
}

void bfVM_stackSetBool(BifrostVM* self, size_t idx, bfBool32 value)
{
  bfVM_assertStackIndex(self, idx);
  self->stack_top[idx] = bfVMValue_fromBool(value);
}

void bfVM_stackSetNil(BifrostVM* self, size_t idx)
{
  bfVM_assertStackIndex(self, idx);
  self->stack_top[idx] = VAL_NULL;
}

void* bfVM_stackReadInstance(const BifrostVM* self, size_t idx)
{
  bfVM_assertStackIndex(self, idx);
  const bfVMValue value = self->stack_top[idx];

  if (IS_NULL(value))
  {
    return NULL;
  }

  assert(IS_POINTER(value) && "The value being read is not an object.");

  BifrostObj* obj = AS_POINTER(value);

  if (obj->type == BIFROST_VM_OBJ_INSTANCE)
  {
    BifrostObjInstance* inst = (BifrostObjInstance*)obj;

    return inst->extra_data;
  }
  else if (obj->type == BIFROST_VM_OBJ_REFERENCE)
  {
    BifrostObjReference* inst = (BifrostObjReference*)obj;
    return inst->extra_data;
  }
  else if (obj->type == BIFROST_VM_OBJ_WEAK_REF)
  {
    BifrostObjWeakRef* inst = (BifrostObjWeakRef*)obj;
    return inst->data;
  }
  else
  {
    assert(!"This object is not a instance.");
  }

  return NULL;
}

const char* bfVM_stackReadString(const BifrostVM* self, size_t idx, size_t* out_size)
{
  bfVM_assertStackIndex(self, idx);
  bfVMValue value = self->stack_top[idx];

  assert(IS_POINTER(value) && "The value being read is not an object.");

  BifrostObj* obj = AS_POINTER(value);

  assert(obj->type == BIFROST_VM_OBJ_STRING && "This object is not a string.");

  BifrostObjStr* str = (BifrostObjStr*)obj;

  if (out_size)
  {
    *out_size = String_length(str->value);
  }

  return str->value;
}

bfVMNumberT bfVM_stackReadNumber(const BifrostVM* self, size_t idx)
{
  bfVM_assertStackIndex(self, idx);
  const bfVMValue value = self->stack_top[idx];

  assert(IS_NUMBER(value) && "The value is not a number.");

  return bfVmValue_asNumber(value);
}

bfBool32 bfVM_stackReadBool(const BifrostVM* self, size_t idx)
{
  bfVM_assertStackIndex(self, idx);
  const bfVMValue value = self->stack_top[idx];

  assert(IS_BOOL(value) && "The value is not a boolean.");

  return bfVMValue_isThuthy(value);
}

static int32_t bfVMGetArity(bfVMValue value)
{
  assert(IS_POINTER(value));

  const BifrostObj* const obj = AS_POINTER(value);

  if (obj->type == BIFROST_VM_OBJ_FUNCTION)
  {
    const BifrostObjFn* const fn = (const BifrostObjFn*)obj;
    return fn->arity;
  }

  if (obj->type == BIFROST_VM_OBJ_NATIVE_FN)
  {
    const BifrostObjNativeFn* const fn = (const BifrostObjNativeFn*)obj;
    return fn->arity;
  }
  // TODO: If an instance / reference has a 'call' operator that should be checked.

  assert(!"Invalid type for arity check!");
  return 0;
}

static BifrostVMType bfVMGetType(bfVMValue value)
{
  if (IS_BOOL(value))
  {
    return BIFROST_VM_BOOL;
  }

  if (IS_NUMBER(value))
  {
    return BIFROST_VM_NUMBER;
  }

  if (IS_POINTER(value))
  {
    const BifrostObj* const obj = AS_POINTER(value);

    if (obj->type == BIFROST_VM_OBJ_STRING)
    {
      return BIFROST_VM_STRING;
    }

    if (obj->type == BIFROST_VM_OBJ_INSTANCE || obj->type == BIFROST_VM_OBJ_REFERENCE || obj->type == BIFROST_VM_OBJ_WEAK_REF)
    {
      return BIFROST_VM_OBJECT;
    }

    if (obj->type == BIFROST_VM_OBJ_FUNCTION || obj->type == BIFROST_VM_OBJ_NATIVE_FN)
    {
      return BIFROST_VM_FUNCTION;
    }

    if (obj->type == BIFROST_VM_OBJ_MODULE)
    {
      return BIFROST_VM_MODULE;
    }
  }

  if (value == VAL_NULL)
  {
    return BIFROST_VM_NIL;
  }

  return BIFROST_VM_UNDEFINED;
}

BifrostVMType bfVM_stackGetType(BifrostVM* self, size_t idx)
{
  bfVM_assertStackIndex(self, idx);
  return bfVMGetType(self->stack_top[idx]);
}

int32_t bfVM_stackGetArity(const BifrostVM* self, size_t idx)
{
  bfVM_assertStackIndex(self, idx);
  return bfVMGetArity(self->stack_top[idx]);
}

int32_t bfVM_handleGetArity(bfValueHandle handle)
{
  return bfVMGetArity(handle->value);
}

BifrostVMType bfVM_handleGetType(bfValueHandle handle)
{
  return bfVMGetType(handle->value);
}

bfValueHandle bfVM_stackMakeHandle(BifrostVM* self, size_t idx)
{
  bfVM_assertStackIndex(self, idx);

  bfValueHandle handle;

  if (self->free_handles)
  {
    handle             = self->free_handles;
    self->free_handles = self->free_handles->next;
  }
  else
  {
    handle = bfGCAllocMemory(self, NULL, 0u, sizeof(struct bfValueHandle_t), sizeof(void*));
  }

  if (handle)
  {
    handle->value = self->stack_top[idx];
    handle->prev  = NULL;
    handle->next  = self->handles;

    if (self->handles)
    {
      self->handles->prev = handle;
    }

    self->handles = handle;
  }

  return handle;
}

void bfVM_stackLoadHandle(BifrostVM* self, size_t dst_idx, bfValueHandle handle)
{
  bfVM_assertStackIndex(self, dst_idx);
  self->stack_top[dst_idx] = handle->value;
}

void bfVM_stackDestroyHandle(BifrostVM* self, bfValueHandle handle)
{
  if (handle)
  {
    if (self->handles == handle)
    {
      self->handles = handle->next;
    }

    if (handle->next)
    {
      handle->next->prev = handle->prev;
    }

    if (handle->prev)
    {
      handle->prev->next = handle->next;
    }

    /*
    TODO(SR):
      Only do this for debug extra security builds.
   */
    handle->value = VAL_NULL;
    handle->next  = NULL;
    handle->prev  = NULL;

    handle->next       = self->free_handles;
    self->free_handles = handle;
  }
}

// TODO(SR): Optimize the main interpreter loop.
enum
{
  REG_RA,
  REG_RB,
  REG_RC,
  REG_RBx,
};

#define bfVM_decodeOp(inst) ((uint8_t)((inst)&BIFROST_INST_OP_MASK))
#define bfVM_decodeRa(inst) ((uint32_t)(((inst) >> BIFROST_INST_RA_OFFSET) & BIFROST_INST_RA_MASK))
#define bfVM_decodeRb(inst) ((uint32_t)(((inst) >> BIFROST_INST_RB_OFFSET) & BIFROST_INST_RB_MASK))
#define bfVM_decodeRc(inst) ((uint32_t)(((inst) >> BIFROST_INST_RC_OFFSET) & BIFROST_INST_RC_MASK))
#define bfVM_decodeRBx(inst) ((uint32_t)(((inst) >> BIFROST_INST_RBx_OFFSET) & BIFROST_INST_RBx_MASK))
#define bfVM_decodeRsBx(inst) ((int32_t)(bfVM_decodeRBx((inst)) - BIFROST_INST_RsBx_MAX))

static void bfVM_decode(const bfInstruction inst, uint8_t* op_out, uint32_t* ra_out, uint32_t* rb_out, uint32_t* rc_out, uint32_t* rbx_out, int32_t* rsbx_out)
{
  *op_out   = bfVM_decodeOp(inst);
  *ra_out   = bfVM_decodeRa(inst);
  *rb_out   = bfVM_decodeRb(inst);
  *rc_out   = bfVM_decodeRc(inst);
  *rbx_out  = bfVM_decodeRBx(inst);
  *rsbx_out = bfVM_decodeRsBx(inst);
}

static bfBool32 bfVM_ensureStackspace2(BifrostVM* self, size_t stack_space, const bfVMValue* top)
{
  const size_t stack_size     = Array_size(&self->stack);
  const size_t stack_used     = top - self->stack;
  const size_t requested_size = stack_used + stack_space;

  if (stack_size < requested_size)
  {
    Array_resize(&self->stack, requested_size);
    return bfTrue;
  }

  return bfFalse;
}

static void bfVM_ensureStackspace(BifrostVM* self, size_t stack_space)
{
  const size_t stack_used     = self->stack_top - self->stack;
  const size_t requested_size = stack_used + stack_space;

  if (bfVM_ensureStackspace2(self, stack_space, self->stack_top))
  {
    self->stack_top = self->stack + requested_size;
  }
}

BifrostVMStackFrame* bfVM_pushCallFrame(BifrostVM* self, BifrostObjFn* fn, size_t new_start)
{
  const size_t old_top = self->stack_top - self->stack;

  if (fn)
  {
    bfVM_ensureStackspace(self, fn->needed_stack_space);
  }
  else
  {
    self->stack_top = self->stack + new_start;
  }

  BifrostVMStackFrame* new_frame = Array_emplace(&self->frames);
  new_frame->ip                  = fn ? fn->instructions : NULL;
  new_frame->fn                  = fn;
  new_frame->stack               = new_start;
  new_frame->old_stack           = old_top;

  return new_frame;
}

static void bfVM_popAllCallFrames(BifrostVM* self, const BifrostVMStackFrame* ref_frame)
{
  const size_t    num_frames   = ref_frame - self->frames;
  const size_t    total_frames = Array_size(&self->frames);
  const bfErrorFn error_fn     = self->params.error_fn;

  if (error_fn)
  {
    error_fn(self, BIFROST_VM_ERROR_STACK_TRACE_BEGIN, -1, "");

    BifrostString error_str = String_new("");

    for (size_t i = num_frames; i < total_frames; ++i)
    {
      const BifrostVMStackFrame* frame    = Array_at(&self->frames, i);
      const BifrostObjFn* const  fn       = frame->fn;
      const uint16_t             line_num = fn ? fn->line_to_code[frame->ip - fn->instructions] : (uint16_t)-1;
      const char* const          fn_name  = fn ? fn->name : "<native>";

      String_sprintf(&error_str, "%*.s[%zu] Stack Frame Line(%u): %s\n", (int)i * 3, "", i, (unsigned)line_num, fn_name);

      error_fn(self, BIFROST_VM_ERROR_STACK_TRACE, (int)line_num, error_str);
    }

    String_delete(error_str);

    error_fn(self, BIFROST_VM_ERROR_STACK_TRACE, -1, self->last_error);
    error_fn(self, BIFROST_VM_ERROR_STACK_TRACE_END, -1, "");
  }

  self->stack_top = self->stack + ref_frame->old_stack;
  Array_resize(&self->frames, num_frames);
}

void bfVM_popCallFrame(BifrostVM* self, BifrostVMStackFrame* frame)
{
  self->stack_top = self->stack + frame->old_stack;
  Array_pop(&self->frames);
}

BifrostVMError bfVM_execTopFrame(BifrostVM* self)
{
  const BifrostVMStackFrame* const reference_frame = Array_back(&self->frames);
  BifrostVMError                   err             = BIFROST_VM_ERROR_NONE;

#define BF_RUNTIME_ERROR(...)                     \
  String_sprintf(&self->last_error, __VA_ARGS__); \
  goto runtime_error

/*
  NOTE(Shareef):
    Must be called after any allocation since an allocation may cause
    a GC leading to a finalizer being called and since a finalizer
    is user defined code it may do anything.
 */
#define BF_REFRESH_LOCALS() \
  locals = self->stack + frame->stack

frame_start:;
  BifrostVMStackFrame* frame          = Array_back(&self->frames);
  BifrostObjModule*    current_module = frame->fn->module;
  bfVMValue*           constants      = frame->fn->constants;
  bfVMValue*           locals         = self->stack + frame->stack;

  while (bfTrue)
  {
    uint8_t  op;
    uint32_t regs[4];
    int32_t  rsbx;
    bfVM_decode(*frame->ip, &op, regs + 0, regs + 1, regs + 2, regs + 3, &rsbx);

    switch (op & BIFROST_INST_OP_MASK)
    {
      case BIFROST_VM_OP_RETURN:
      {
        locals[0] = locals[regs[REG_RBx]];
        goto halt;
      }
      case BIFROST_VM_OP_LOAD_SYMBOL:
      {
        const bfVMValue     obj_value  = locals[regs[REG_RB]];
        const uint32_t      symbol     = regs[REG_RC];
        const BifrostString symbol_str = self->symbols[symbol];

        if (!IS_POINTER(obj_value))
        {
          char error_buffer[512];
          bfDbgValueToString(obj_value, error_buffer, sizeof(error_buffer));
          BF_RUNTIME_ERROR("Cannot load symbol (%s) from non object %s\n", symbol_str, error_buffer);
        }

        BifrostObj* obj = AS_POINTER(obj_value);

        if (obj->type == BIFROST_VM_OBJ_INSTANCE)
        {
          BifrostObjInstance* inst = (BifrostObjInstance*)obj;

          bfVMValue* value = bfHashMap_get(&inst->fields, self->symbols[symbol]);

          if (value)
          {
            locals[regs[REG_RA]] = *value;
          }
          else if (inst->clz)
          {
            obj = &inst->clz->super;
          }
        }
        else if (obj->type == BIFROST_VM_OBJ_REFERENCE || obj->type == BIFROST_VM_OBJ_WEAK_REF)
        {
          BifrostObjReference* inst = (BifrostObjReference*)obj;

          if (inst->clz)
          {
            obj = &inst->clz->super;
          }
        }

        if (obj->type == BIFROST_VM_OBJ_CLASS)
        {
          BifrostObjClass* original_clz = (BifrostObjClass*)obj;
          BifrostObjClass* clz          = original_clz;
          bfBool32         found_field  = bfFalse;

          while (clz)
          {
            if (symbol < Array_size(&clz->symbols) && clz->symbols[symbol].value != VAL_NULL)
            {
              locals[regs[REG_RA]] = clz->symbols[symbol].value;
              found_field          = bfTrue;
              break;
            }

            clz = clz->base_clz;
          }

          if (!found_field)
          {
            BF_RUNTIME_ERROR("'%s::%s' is not defined (also not found in any base class).\n", original_clz->name, self->symbols[symbol]);
          }
        }
        else if (obj->type == BIFROST_VM_OBJ_MODULE)
        {
          BifrostObjModule* module = (BifrostObjModule*)obj;

          locals[regs[REG_RA]] = bfVM_stackFindVariable(module, symbol_str, String_length(symbol_str));
        }
        else
        {
          BF_RUNTIME_ERROR("(%u) ERROR, loading a symbol (%s) on a non instance obj.\n", obj->type, self->symbols[symbol]);
        }
        break;
      }
      case BIFROST_VM_OP_STORE_SYMBOL:
      {
        const BifrostString sym_str   = self->symbols[regs[REG_RB]];
        const int           err_store = bfVM__stackStoreVariable(self, locals[regs[REG_RA]], bfMakeStringRangeLen(sym_str, String_length(sym_str)), locals[regs[REG_RC]]);

        if (err_store)
        {
          if (err_store == 1)
          {
            BF_RUNTIME_ERROR("Cannot store symbol into non object\n");
          }

          if (err_store == 2)
          {
            BF_RUNTIME_ERROR("ERRRO, storing a symbol on a non instance or class obj.\n");
          }
        }
        break;
      }
      case BIFROST_VM_OP_LOAD_BASIC:
      {
        static const bfVMValue s_Literals[] =
         {
          VAL_TRUE,
          VAL_FALSE,
          VAL_NULL,
         };

        const uint32_t action = regs[REG_RBx];

        if (action < BIFROST_VM_OP_LOAD_BASIC_CURRENT_MODULE)
        {
          locals[regs[REG_RA]] = s_Literals[action];
        }
        else if (action == BIFROST_VM_OP_LOAD_BASIC_CURRENT_MODULE)
        {
          locals[regs[REG_RA]] = FROM_POINTER(current_module);
        }
        else
        {
          locals[regs[REG_RA]] = constants[regs[REG_RBx] - BIFROST_VM_OP_LOAD_BASIC_CONSTANT];
        }

        break;
      }
      case BIFROST_VM_OP_NEW_CLZ:
      {
        const bfVMValue value = locals[regs[REG_RBx]];

        if (IS_POINTER(value))
        {
          BifrostObj* obj = AS_POINTER(value);

          if (obj->type == BIFROST_VM_OBJ_CLASS)
          {
            BifrostObjClass* clz  = (BifrostObjClass*)obj;
            void* const      inst = bfVM_createInstance(self, clz);
            BF_REFRESH_LOCALS();
            locals[regs[REG_RA]] = FROM_POINTER(inst);
          }
          else
          {
            char string_buffer[512];
            bfDbgValueTypeToString(value, string_buffer, sizeof(string_buffer));

            BF_RUNTIME_ERROR("Called new on a non Class type (%s).\n", string_buffer);
          }
        }
        else
        {
          char string_buffer[512];
          bfDbgValueTypeToString(value, string_buffer, sizeof(string_buffer));

          BF_RUNTIME_ERROR("Called new on a non Class type (%s).\n", string_buffer);
        }
        break;
      }
      case BIFROST_VM_OP_NOT:
      {
        locals[regs[REG_RA]] = bfVMValue_isThuthy(locals[regs[REG_RBx]]) ? VAL_FALSE : VAL_TRUE;
        break;
      }
      case BIFROST_VM_OP_STORE_MOVE:
      {
        locals[regs[REG_RA]] = locals[regs[REG_RBx]];
        break;
      }
      case BIFROST_VM_OP_CALL_FN:
      {
        const bfVMValue value     = locals[regs[REG_RB]];
        const uint32_t  ra        = regs[REG_RA];
        const size_t    new_stack = frame->stack + ra;
        uint32_t        num_args  = regs[REG_RC];

        if (IS_POINTER(value))
        {
          BifrostObj*               obj      = AS_POINTER(value);
          const BifrostObjInstance* instance = (const BifrostObjInstance*)obj;

          if (obj->type == BIFROST_VM_OBJ_INSTANCE || obj->type == BIFROST_VM_OBJ_REFERENCE || obj->type == BIFROST_VM_OBJ_WEAK_REF)
          {
            instance = (const BifrostObjInstance*)obj;
            obj      = instance->clz ? &instance->clz->super : obj;
          }

          if (obj->type == BIFROST_VM_OBJ_CLASS)
          {
            const BifrostObjClass* const clz      = (const BifrostObjClass*)obj;
            const size_t                 call_sym = self->build_in_symbols[BIFROST_VM_SYMBOL_CALL];

            if (call_sym < Array_size(&clz->symbols))
            {
              const bfVMValue call_value = clz->symbols[call_sym].value;

              if (IS_POINTER(call_value))
              {
                BifrostObj* const call_obj = BIFROST_AS_OBJ(call_value);

                if (call_obj->type != BIFROST_VM_OBJ_FUNCTION && call_obj->type != BIFROST_VM_OBJ_NATIVE_FN)
                {
                  BF_RUNTIME_ERROR("'%s::call' must be defined as a function to use instance as function.\n", clz->name);
                }

                if (bfVM_ensureStackspace2(self, num_args + (size_t)1, locals + ra))
                {
                  BF_REFRESH_LOCALS();
                }

                bfVMValue* new_top = locals + ra;

                memmove(new_top + 1, new_top, sizeof(bfVMValue) * num_args);

                new_top[0] = FROM_POINTER(instance);
                obj        = call_obj;
                ++num_args;
              }
              else
              {
                BF_RUNTIME_ERROR("'%s::call' must be defined as a function to use instance as function.\n", clz->name);
              }
            }
            else
            {
              BF_RUNTIME_ERROR("%s does not define a 'call' function.\n", clz->name);
            }
          }

          if (obj->type == BIFROST_VM_OBJ_FUNCTION)
          {
            BifrostObjFn* const fn = (BifrostObjFn*)obj;

            if (fn->arity >= 0 && num_args != (size_t)fn->arity)
            {
              BF_RUNTIME_ERROR("Function (%s) called with %i argument(s) but requires %i.\n", fn->name, (int)num_args, (int)fn->arity);
            }

            ++frame->ip;
            bfVM_pushCallFrame(self, fn, new_stack);
            goto frame_start;
          }

          if (obj->type == BIFROST_VM_OBJ_NATIVE_FN)
          {
            const BifrostObjNativeFn* const fn = (const BifrostObjNativeFn*)obj;

            if (fn->arity >= 0 && num_args != (uint32_t)fn->arity)
            {
              BF_RUNTIME_ERROR("Function<native> called with %i arguments but requires %i.\n", (int)num_args, (int)fn->arity);
            }

            BifrostVMStackFrame* const native_frame = bfVM_pushCallFrame(self, NULL, new_stack);
            self->current_native_fn                 = fn;
            fn->value(self, (int32_t)num_args);
            self->current_native_fn = NULL;
            bfVM_popCallFrame(self, native_frame);

            BF_REFRESH_LOCALS();
          }
          else
          {
            BF_RUNTIME_ERROR("Not a callable value.\n");
          }
        }
        else
        {
          BF_RUNTIME_ERROR("Not a pointer value to call.\n");
        }
        break;
      }
      case BIFROST_VM_OP_MATH_ADD:
      {
        const bfVMValue lhs = locals[regs[REG_RB]];
        const bfVMValue rhs = locals[regs[REG_RC]];

        if (IS_NUMBER(lhs) && IS_NUMBER(rhs))
        {
          locals[regs[REG_RA]] = FROM_NUMBER(bfVmValue_asNumber(lhs) + bfVmValue_asNumber(rhs));
        }
        else if ((IS_POINTER(lhs) && BIFROST_AS_OBJ(lhs)->type == BIFROST_VM_OBJ_STRING) || (IS_POINTER(rhs) && BIFROST_AS_OBJ(rhs)->type == BIFROST_VM_OBJ_STRING))
        {
          char         string_buffer[512];
          const size_t offset = bfDbgValueToString(lhs, string_buffer, sizeof(string_buffer));
          bfDbgValueToString(rhs, string_buffer + offset, sizeof(string_buffer) - offset);

          BifrostObjStr* const str_obj = bfVM_createString(self, bfMakeStringRangeC(string_buffer));

          BF_REFRESH_LOCALS();

          locals[regs[REG_RA]] = FROM_POINTER(str_obj);
        }
        else
        {
          char         string_buffer[512];
          const size_t offset = bfDbgValueTypeToString(lhs, string_buffer, sizeof(string_buffer));
          bfDbgValueTypeToString(rhs, string_buffer + offset + 1, sizeof(string_buffer) - offset - 1);

          BF_RUNTIME_ERROR("'+' operator of two incompatible types (%s + %s).", string_buffer, string_buffer + offset + 1);
        }
        break;
      }
      case BIFROST_VM_OP_MATH_SUB:
      {
        const bfVMValue lhs = locals[regs[REG_RB]];
        const bfVMValue rhs = locals[regs[REG_RC]];

        if (!IS_NUMBER(lhs) || !IS_NUMBER(rhs))
        {
          BF_RUNTIME_ERROR("Subtraction is not allowed on non number values.\n");
        }

        locals[regs[REG_RA]] = FROM_NUMBER(bfVmValue_asNumber(lhs) - bfVmValue_asNumber(rhs));
        break;
      }
      case BIFROST_VM_OP_MATH_MUL:
      {
        locals[regs[REG_RA]] = bfVMValue_mul(locals[regs[REG_RB]], locals[regs[REG_RC]]);
        break;
      }
      case BIFROST_VM_OP_MATH_DIV:
      {
        locals[regs[REG_RA]] = bfVMValue_div(locals[regs[REG_RB]], locals[regs[REG_RC]]);
        break;
      }
      case BIFROST_VM_OP_CMP_EE:
      {
        locals[regs[REG_RA]] = bfVMValue_fromBool(bfVMValue_ee(locals[regs[REG_RB]], locals[regs[REG_RC]]));
        break;
      }
      case BIFROST_VM_OP_CMP_LT:
      {
        locals[regs[REG_RA]] = bfVMValue_fromBool(bfVMValue_lt(locals[regs[REG_RB]], locals[regs[REG_RC]]));
        break;
      }
      case BIFROST_VM_OP_CMP_GT:
      {
        locals[regs[REG_RA]] = bfVMValue_fromBool(bfVMValue_gt(locals[regs[REG_RB]], locals[regs[REG_RC]]));
        break;
      }
      case BIFROST_VM_OP_CMP_GE:
      {
        locals[regs[REG_RA]] = bfVMValue_fromBool(bfVMValue_ge(locals[regs[REG_RB]], locals[regs[REG_RC]]));
        break;
      }
      case BIFROST_VM_OP_CMP_AND:
      {
        locals[regs[REG_RA]] = bfVMValue_fromBool(bfVMValue_isThuthy(locals[regs[REG_RB]]) && bfVMValue_isThuthy(locals[regs[REG_RC]]));
        break;
      }
      case BIFROST_VM_OP_CMP_OR:
      {
        locals[regs[REG_RA]] = bfVMValue_fromBool(bfVMValue_isThuthy(locals[regs[REG_RB]]) || bfVMValue_isThuthy(locals[regs[REG_RC]]));
        break;
      }
      case BIFROST_VM_OP_JUMP:
      {
        frame->ip += rsbx;
        continue;
      }
      case BIFROST_VM_OP_JUMP_IF:
      {
        if (bfVMValue_isThuthy(locals[regs[REG_RA]]))
        {
          frame->ip += rsbx;
          continue;
        }
        break;
      }
      case BIFROST_VM_OP_JUMP_IF_NOT:
      {
        if (!bfVMValue_isThuthy(locals[regs[REG_RA]]))
        {
          frame->ip += rsbx;
          continue;
        }
        break;
      }
      default:
      {
        BF_RUNTIME_ERROR("Invalid OP: %i\n", (int)op);
        // break;
      }
    }

    ++frame->ip;
  }

#undef BF_RUNTIME_ERROR
#undef BF_REFRESH_LOCALS

runtime_error:
  bfVM_popAllCallFrames(self, reference_frame);
  err = BIFROST_VM_ERROR_RUNTIME;
  goto done;

halt:
  bfVM_popCallFrame(self, frame);

  if (reference_frame < frame)
  {
    goto frame_start;
  }

done:
  return err;
}

BifrostVMError bfVM_call(BifrostVM* self, size_t idx, size_t args_start, int32_t num_args)
{
  bfVM_assertStackIndex(self, idx);
  bfVMValue      value = self->stack_top[idx];
  BifrostVMError err   = BIFROST_VM_ERROR_NONE;

  assert(IS_POINTER(value));

  BifrostObj* obj = AS_POINTER(value);

  const size_t base_stack = (self->stack_top - self->stack);

  if (obj->type == BIFROST_VM_OBJ_FUNCTION)
  {
    /* NOTE(Shareef):
        The 'bfVM_execTopFrame' function automatically pops the
        stackframe once the call is done.
    */

    BifrostObjFn* const fn = (BifrostObjFn*)obj;

    if (fn->arity < 0 || fn->arity == num_args)
    {
      bfVM_pushCallFrame(self, fn, base_stack + args_start);
      err = bfVM_execTopFrame(self);
    }
    else
    {
      err = BIFROST_VM_ERROR_FUNCTION_ARITY_MISMATCH;
    }
  }
  else if (obj->type == BIFROST_VM_OBJ_NATIVE_FN)
  {
    BifrostObjNativeFn* native_fn = (BifrostObjNativeFn*)obj;

    // TODO(SR):
    //   Add an API to be able to set errors from user defined functions.
    BifrostVMStackFrame* frame = bfVM_pushCallFrame(self, NULL, base_stack + args_start);
    native_fn->value(self, num_args);
    bfVM_popCallFrame(self, frame);
  }
  else
  {
    assert(!"bfVM_call called with a non function object.");
  }

  return err;
}

BifrostVMError bfVM_execInModule(BifrostVM* self, const char* module, const char* source, size_t source_length)
{
  BifrostObjModule* module_obj;
  BifrostVMError    err = bfVM__moduleMake(self, module, &module_obj);

  if (!err)
  {
    bfGCPushRoot(self, &module_obj->super);

    err = bfVM_compileIntoModule(self, module_obj, source, source_length) ||
          bfVM_runModule(self, module_obj);

    bfVM_stackResize(self, 1);
    self->stack_top[0] = FROM_POINTER(module_obj);
    bfGCPopRoot(self);
  }

  return err;
}

void bfVM_gc(BifrostVM* self)
{
  if (!self->gc_is_running)
  {
    self->gc_is_running = bfTrue;
    bfGCCollect(self);
    self->gc_is_running = bfFalse;
  }
}

const char* bfVM_buildInSymbolStr(const BifrostVM* self, BifrostVMBuildInSymbol symbol)
{
  (void)self;
  static const char* const ENUM_TO_STR[] =
   {
    "ctor",
    "dtor",
    "call",
    "__error__",
   };

  return ENUM_TO_STR[symbol];
}

ConstBifrostString bfVM_errorString(const BifrostVM* self)
{
  return self->last_error;
}

void bfVM_dtor(BifrostVM* self)
{
  BifrostObj* garbage_list = self->gc_object_list;

  while (garbage_list)
  {
    void* const next = garbage_list->next;
    bfObjFinalize(self, garbage_list);
    garbage_list = next;
  }

  while (self->gc_object_list)
  {
    void* const next = self->gc_object_list->next;
    bfVMObject_delete(self, self->gc_object_list);
    self->gc_object_list = next;
  }

  while (self->finalized)
  {
    void* const next = self->finalized->next;
    bfVMObject_delete(self, self->finalized);
    self->finalized = next;
  }

  const size_t num_symbols = Array_size(&self->symbols);

  for (size_t i = 0; i < num_symbols; ++i)
  {
    String_delete(self->symbols[i]);
  }

  Array_delete(&self->symbols);
  Array_delete(&self->frames);
  Array_delete(&self->stack);
  bfHashMap_dtor(&self->modules);
  String_delete(self->last_error);

  while (self->free_handles)
  {
    const bfValueHandle next = self->free_handles->next;  // NOLINT(misc-misplaced-const)
    bfGCAllocMemory(self, self->free_handles, sizeof(struct bfValueHandle_t), 0u, sizeof(void*));
    self->free_handles = next;
  }

  if (self->handles)
  {
    assert(!"You are leaking a handle to a VM Object.");
  }
}

void bfVM_delete(BifrostVM* self)
{
  bfVM_dtor(self);
  bfGCAllocMemory(self, self, sizeof(BifrostVM), 0u, sizeof(void*));
}

BifrostObjModule* bfVM_findModule(BifrostVM* self, const char* name, size_t name_len)
{
  const size_t    hash    = bfString_hashN(name, name_len);
  BifrostHashMap* modules = &self->modules;

  bfHashMapFor(it, modules)
  {
    const BifrostObjStr* key     = it.key;
    const size_t         key_len = String_length(key->value);

    if (key->hash == hash && key_len == name_len && String_ccmpn(key->value, name, name_len) == 0)
    {
      return *(void**)it.value;
    }
  }

  return NULL;
}

static int bfVM_getSymbolHelper(const void* lhs, const void* rhs)
{
  const bfStringRange* const name    = lhs;
  const BifrostString* const sym     = (void*)rhs;
  const size_t               lhs_len = bfStringRange_length(name);
  const size_t               rhs_len = String_length(*sym);

  return lhs_len == rhs_len && String_ccmpn(*sym, name->bgn, lhs_len) == 0;
}

uint32_t bfVM_getSymbol(BifrostVM* self, bfStringRange name)
{
  size_t idx = Array_find(&self->symbols, &name, &bfVM_getSymbolHelper);

  if (idx == BIFROST_ARRAY_INVALID_INDEX)
  {
    idx                = Array_size(&self->symbols);
    BifrostString* sym = Array_emplace(&self->symbols);
    *sym               = String_newLen(name.bgn, bfStringRange_length(&name));
  }

  return (uint32_t)idx;
}

static BifrostVMError bfVM_runModule(BifrostVM* self, BifrostObjModule* module)
{
  const size_t old_top = self->stack_top - self->stack;
  bfVM_pushCallFrame(self, &module->init_fn, old_top);
  const BifrostVMError err = bfVM_execTopFrame(self);
  return err;
}

static BifrostVMError bfVM_compileIntoModule(BifrostVM* self, BifrostObjModule* module, const char* source, size_t source_len)
{
#define KEYWORD(kw, tt)                  \
  {                                      \
    kw, sizeof(kw) - 1,                  \
    {                                    \
      .type = (tt), .as = {.str = (kw) } \
    }                                    \
  }

  static const bfKeyword s_Keywords[] =
   {
    KEYWORD("true", CONST_BOOL),
    KEYWORD("false", CONST_BOOL),
    KEYWORD("return", CTRL_RETURN),
    KEYWORD("if", CTRL_IF),
    KEYWORD("for", CTRL_FOR),
    KEYWORD("else", CTRL_ELSE),
    KEYWORD("while", BIFROST_TOKEN_CTRL_WHILE),
    KEYWORD("func", FUNC),
    KEYWORD("var", VAR_DECL),
    KEYWORD("nil", CONST_NIL),
    KEYWORD("class", BIFROST_TOKEN_CLASS),
    KEYWORD("import", IMPORT),
    KEYWORD("break", BIFROST_TOKEN_CTRL_BREAK),
    KEYWORD("new", BIFROST_TOKEN_NEW),
    KEYWORD("static", BIFROST_TOKEN_STATIC),
    KEYWORD("as", BIFROST_TOKEN_AS),
    KEYWORD("super", BIFROST_TOKEN_SUPER),
   };
#undef KEYWORD

  const BifrostLexerParams lex_params =
   {
    .source       = source,
    .length       = source_len,
    .keywords     = s_Keywords,
    .num_keywords = bfCArraySize(s_Keywords),
    .vm           = self,
   };

  BifrostLexer lexer = bfLexer_make(&lex_params);

  BifrostParser parser;
  bfParser_ctor(&parser, self, &lexer, module);
  const bfBool32 has_error = bfParser_compile(&parser);
  bfParser_dtor(&parser);

  return has_error ? BIFROST_VM_ERROR_COMPILE : BIFROST_VM_ERROR_NONE;
}

BifrostObjModule* bfVM_importModule(BifrostVM* self, const char* from, const char* name, size_t name_len)
{
  BifrostObjModule* m = bfVM_findModule(self, name, name_len);

  if (!m)
  {
    const bfModuleFn module_fn = self->params.module_fn;

    if (module_fn)
    {
      const bfStringRange name_range  = {.bgn = name, .end = name + name_len};
      BifrostObjStr*      module_name = bfVM_createString(self, name_range);

      bfGCPushRoot(self, &module_name->super);

      BifrostVMModuleLookUp look_up =
       {
        .source     = NULL,
        .source_len = 0u,
       };

      module_fn(self, from, module_name->value, &look_up);

      if (look_up.source && look_up.source_len)
      {
        m = bfVM_createModule(self, name_range);

        bfGCPushRoot(self, &m->super);

        // NOTE(Shareef): No error is 0. So if an error occurs we short-circuit
        const bfBool32 has_error = bfVM_compileIntoModule(self, m, look_up.source, look_up.source_len) || bfVM_runModule(self, m);

        if (!has_error)
        {
          bfHashMap_set(&self->modules, module_name, &m);
        }

        // m
        bfGCPopRoot(self);
        bfGCAllocMemory(self, (void*)look_up.source, look_up.source_len, 0u, sizeof(void*));
      }
      else
      {
        String_sprintf(&self->last_error, "Failed to find module '%.*s'", name_len, name);
      }

      // module_name
      bfGCPopRoot(self);
    }
    else
    {
      String_sprintf(&self->last_error, "No module function registered when loading module '%.*s'", name_len, name);
    }
  }

  return m;
}