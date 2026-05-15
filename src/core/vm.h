/*
 *  Copyright (c) 2020-2022 Thakee Nathees
 *  Copyright (c) 2021-2022 Pocketlang Contributors
 *  Distributed Under The MIT License
 */

#ifndef PK_VM_H
#define PK_VM_H

#ifndef PK_AMALGAMATED
#include "compiler.h"
#include "core.h"
#endif

// The maximum number of temporary object reference to protect them from being
// garbage collected.
#define MAX_TEMP_REFERENCE 64

// The capacity of the builtin function array in the VM.
#define BUILTIN_FN_CAPACITY 50

// Initially allocated call frame capacity. Will grow dynamically.
#define INITIAL_CALL_FRAMES 4

// The minimum size of the stack that will be initialized for a fiber before
// running one.
#define MIN_STACK_SIZE 128

// The allocated size that will trigger the first GC. (~10MB).
#define INITIAL_GC_SIZE (1024 * 1024 * 10)

// The heap size might shrink if the remaining allocated bytes after a GC
// is less than the one before the last GC. So we need a minimum size.
#define MIN_HEAP_SIZE (1024 * 1024)

// The heap size for the next GC will be calculated as the bytes we have
// allocated so far plus the fill factor of it.
#define HEAP_FILL_PERCENT 75

// Evaluated to "true" if a runtime error set on the current fiber.
#define VM_HAS_ERROR(vm) (vm->fiber->error != NULL)

// Set the error message [err] to the [vm]'s current fiber.
#define VM_SET_ERROR(vm, err)        \
  do {                               \
    ASSERT(!VM_HAS_ERROR(vm), OOPS); \
    (vm->fiber->error = err);        \
  } while (false)

// A doubly link list of vars that have reference in the host application.
// Handles are wrapper around Var that lives on the host application.
class PkHandle {
public:
  Var value;

  PkHandle* prev;
  PkHandle* next;
};

// PocketLang Virtual Machine. It'll contain the state of the execution, stack,
// heap, and manage memory allocations.
class PKVM {
public:

  // The first object in the link list of all heap allocated objects.
  Object* first;

  // The number of bytes allocated by the vm and not (yet) garbage collected.
  size_t bytes_allocated;

  // The number of bytes that'll trigger the next GC.
  size_t next_gc;

  // True if PKVM is running a garbage collection, and no new allocation is
  // allowed in this phase.
  bool collecting_garbage;

  // Minimum size the heap could get.
  size_t min_heap_size;

  // The heap size for the next GC will be calculated as the bytes we have
  // allocated so far plus the fill factor of it.
  int heap_fill_percent;

  // In the tri coloring scheme gray is the working list. We recursively pop
  // from the list color it black and add it's referenced objects to gray_list.

  // Working set is the is the list of objects that were marked reachable from
  // VM's root (ex: stack values, temp references, handles, vm's running fiber,
  // current compiler etc). But yet tobe perform a reachability analysis of the
  // objects it reference to.
  Object** working_set;
  int working_set_count;
  int working_set_capacity;

  // A stack of temporary object references to ensure that the object
  // doesn't garbage collected.
  Object* temp_reference[MAX_TEMP_REFERENCE];
  int temp_reference_count;

  // Pointer to the first handle in the doubly linked list of handles. Handles
  // are wrapper around Var that lives on the host application. This linked
  // list will keep them alive till the host uses the variable.
  PkHandle* handles;

  // VM's configurations.
  PkConfiguration config;

  // Current compiler reference to mark it's heap allocated objects. Note that
  // The compiler isn't heap allocated. It'll be a link list of all the
  // compiler we have so far. A new compiler will be created and appended when
  // a new module is being imported and compiled at compiletime.
  Compiler* compiler;

  // A map of all the modules which are compiled or natively registered.
  // The key of the modules will be:
  // 1. Native modules  : name of the module.
  // 2. Compiled script :
  //      - module name if one defined with the module keyword
  //      - otherwise path of the module.
  Map* modules;

  // List of directories that used for search modules.
  List* search_paths;

  // Array of all builtin functions.
  Closure* builtins_funcs[BUILTIN_FN_CAPACITY];
  int builtins_count;

  // An array of all the primitive types' class except for OBJ_INST. Since the
  // type of the objects are enums starting from 0 we can directly get the
  // class by using their enum (ex: primitives[OBJ_LIST]).
  Class* builtin_classes[PK_INSTANCE];

  // Current fiber.
  Fiber* fiber;

  // VM methods (converted from free functions).
  void* vmRealloc(void* memory, size_t old_size, size_t new_size);
  PkHandle* vmNewHandle(Var value);
  void vmEnsureStackSize(Fiber* fiber, int size);
  void vmCollectGarbage();
  void vmPushTempRef(Object* obj);
  void vmPopTempRef();
  void vmRegisterModule(Module* module, String* key);
  Module* vmGetModule(String* key);
  bool vmPrepareFiber(Fiber* fiber, int argc, Var* argv);
  bool vmSwitchFiber(Fiber* fiber, Var* value);
  void vmYieldFiber(Var* value);
  PkResult vmRunFiber(Fiber* fiber);
  PkResult vmCallFunction(Closure* fn, int argc, Var* argv, Var* ret);
  PkResult vmCallMethod(Var self, Closure* fn, int argc, Var* argv, Var* ret);
  Var vmImportModule(String* from, String* path);
#ifndef PK_NO_DL
  void vmUnloadDlHandle(void* handle);
#endif

private:
  void vmReportError();
};

#endif // PK_VM_H
