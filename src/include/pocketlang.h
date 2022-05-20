/*
 *  Copyright (c) 2020-2022 Thakee Nathees
 *  Copyright (c) 2021-2022 Pocketlang Contributors
 *  Distributed Under The MIT License
 */

#ifndef POCKETLANG_H
#define POCKETLANG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

/*****************************************************************************/
/* POCKETLANG DEFINES                                                        */
/*****************************************************************************/

// The version number macros.
// Major Version - Increment when changes break compatibility.
// Minor Version - Increment when new functionality added to public api.
// Patch Version - Increment when bug fixed or minor changes between releases.
#define PK_VERSION_MAJOR 0
#define PK_VERSION_MINOR 1
#define PK_VERSION_PATCH 0

// String representation of the version.
#define PK_VERSION_STRING "0.1.0"

// Pocketlang visibility macros. define PK_DLL for using pocketlang as a 
// shared library and define PK_COMPILE to export symbols when compiling the
// pocketlang it self as a shared library.

#ifdef _MSC_VER
  #define _PK_EXPORT __declspec(dllexport)
  #define _PK_IMPORT __declspec(dllimport)
#elif defined(__GNUC__)
  #define _PK_EXPORT __attribute__((visibility ("default")))
  #define _PK_IMPORT
#else
  #define _PK_EXPORT
  #define _PK_IMPORT
#endif

#ifdef PK_DLL
  #ifdef PK_COMPILE
    #define PK_PUBLIC _PK_EXPORT
  #else
    #define PK_PUBLIC _PK_IMPORT
  #endif
#else
  #define PK_PUBLIC
#endif


/*****************************************************************************/
/* POCKETLANG TYPEDEFS & CALLBACKS                                           */
/*****************************************************************************/

// PocketLang Virtual Machine. It'll contain the state of the execution, stack,
// heap, and manage memory allocations.
typedef struct PKVM PKVM;

// A handle to the pocketlang variables. It'll hold the reference to the
// variable and ensure that the variable it holds won't be garbage collected
// till it's released with pkReleaseHandle().
typedef struct PkHandle PkHandle;

typedef enum PkVarType PkVarType;
typedef enum PkResult PkResult;
typedef struct PkConfiguration PkConfiguration;

// C function pointer which is callable from pocketLang by native module
// functions.
typedef void (*pkNativeFn)(PKVM* vm);

// A function that'll be called for all the allocation calls by PKVM.
//
// - To allocate new memory it'll pass NULL to parameter [memory] and the
//   required size to [new_size]. On failure the return value would be NULL.
//
// - When reallocating an existing memory if it's grow in place the return
//   address would be the same as [memory] otherwise a new address.
//
// - To free an allocated memory pass [memory] and 0 to [new_size]. The
//   function will return NULL.
typedef void* (*pkReallocFn)(void* memory, size_t new_size, void* user_data);

// Function callback to write [text] to stdout or stderr.
typedef void (*pkWriteFn) (PKVM* vm, const char* text);

// A function callback to read a line from stdin. The returned string shouldn't
// contain a line ending (\n or \r\n). The returned string **must** be
// allocated with pkRealloc() and the VM will claim the ownership of the
// string.
typedef char* (*pkReadFn) (PKVM* vm);

// Load and return the script. Called by the compiler to fetch initial source
// code and source for import statements. Return NULL to indicate failure to
// load. Otherwise the string **must** be allocated with pkRealloc() and
// the VM will claim the ownership of the string.
typedef char* (*pkLoadScriptFn) (PKVM* vm, const char* path);

// A function callback to resolve the import statement path. [from] path can
// be either path to a script or NULL if [path] is relative to cwd. The return
// value should be a normalized absolute path of the [path]. Return NULL to
// indicate failure to resolve. Othrewise the string **must** be allocated with
// pkRealloc() and the VM will claim the ownership of the string.
typedef char* (*pkResolvePathFn) (PKVM* vm, const char* from,
                                       const char* path);

// A function callback to allocate and return a new instance of the registered
// class. Which will be called when the instance is constructed. The returned/
// data is expected to be alive till the delete callback occurs.
typedef void* (*pkNewInstanceFn) (PKVM* vm);

// A function callback to de-allocate the allocated native instance of the
// registered class. This function is invoked at the GC execution. No object
// allocations are allowed during it, so **NEVER** allocate any objects
// inside them.
typedef void (*pkDeleteInstanceFn) (PKVM* vm, void*);

/*****************************************************************************/
/* POCKETLANG TYPES                                                          */
/*****************************************************************************/

// Type enum of the pocketlang's first class types. Note that Object isn't
// instanciable (as of now) but they're considered first calss.
enum PkVarType {
  PK_OBJECT = 0,

  PK_NULL,
  PK_BOOL,
  PK_NUMBER,
  PK_STRING,
  PK_LIST,
  PK_MAP,
  PK_RANGE,
  PK_MODULE,
  PK_CLOSURE,
  PK_FIBER,
  PK_CLASS,
  PK_INSTANCE,
};

// Result that pocketlang will return after a compilation or running a script
// or a function or evaluating an expression.
enum PkResult {
  PK_RESULT_SUCCESS = 0,    // Successfully finished the execution.

  // Note that this result is internal and will not be returned to the host
  // anymore.
  //
  // Unexpected EOF while compiling the source. This is another compile time
  // error that will ONLY be returned if we're compiling in REPL mode. We need
  // this specific error to indicate the host application to add another line
  // to the last input. If REPL is not enabled this will be compile error.
  PK_RESULT_UNEXPECTED_EOF,

  PK_RESULT_COMPILE_ERROR,  // Compilation failed.
  PK_RESULT_RUNTIME_ERROR,  // An error occurred at runtime.
};

struct PkConfiguration {

  // The callback used to allocate, reallocate, and free. If the function
  // pointer is NULL it defaults to the VM's realloc(), free() wrappers.
  pkReallocFn realloc_fn;

  pkWriteFn stderr_write;
  pkWriteFn stdout_write;
  pkReadFn stdin_read;

  pkResolvePathFn resolve_path_fn;
  pkLoadScriptFn load_script_fn;

  // If true stderr calls will use ansi color codes.
  bool use_ansi_color;

  // User defined data associated with VM.
  void* user_data;
};

/*****************************************************************************/
/* POCKETLANG PUBLIC API                                                     */
/*****************************************************************************/

// Create a new PkConfiguration with the default values and return it.
// Override those default configuration to adopt to another hosting
// application.
PK_PUBLIC PkConfiguration pkNewConfiguration();

// Allocate, initialize and returns a new VM.
PK_PUBLIC PKVM* pkNewVM(PkConfiguration* config);

// Clean the VM and dispose all the resources allocated by the VM.
PK_PUBLIC void pkFreeVM(PKVM* vm);

// Update the user data of the vm.
PK_PUBLIC void pkSetUserData(PKVM* vm, void* user_data);

// Returns the associated user data.
PK_PUBLIC void* pkGetUserData(const PKVM* vm);

// Invoke pocketlang's allocator directly.  This function should be called 
// when the host application want to send strings to the PKVM that are claimed
// by the VM once the caller returned it. For other uses you **should** call
// pkRealloc with [size] 0 to cleanup, otherwise there will be a memory leak.
//
// Internally it'll call `pkReallocFn` function that was provided in the
// configuration.
PK_PUBLIC void* pkRealloc(PKVM* vm, void* ptr, size_t size);

// Release the handle and allow its value to be garbage collected. Always call
// this for every handles before freeing the VM.
PK_PUBLIC void pkReleaseHandle(PKVM* vm, PkHandle* handle);

// Add a new module named [name] to the [vm]. Note that the module shouldn't
// already existed, otherwise an assertion will fail to indicate that.
PK_PUBLIC PkHandle* pkNewModule(PKVM* vm, const char* name);

// Register the module to the PKVM's modules map, once after it can be
// imported in other modules.
PK_PUBLIC void pkRegisterModule(PKVM* vm, PkHandle* module);

// Add a native function to the given module. If [arity] is -1 that means
// the function has variadic parameters and use pkGetArgc() to get the argc.
// Note that the function will be added as a global variable of the module.
PK_PUBLIC void pkModuleAddFunction(PKVM* vm, PkHandle* module,
                                   const char* name,
                                   pkNativeFn fptr, int arity);

// Create a new class on the [module] with the [name] and return it.
// If the [base_class] is NULL by default it'll set to "Object" class.
PK_PUBLIC PkHandle* pkNewClass(PKVM* vm, const char* name,
                               PkHandle* base_class, PkHandle* module,
                               pkNewInstanceFn new_fn,
                               pkDeleteInstanceFn delete_fn);

// Add a native method to the given class. If the [arity] is -1 that means
// the method has variadic parameters and use pkGetArgc() to get the argc.
PK_PUBLIC void pkClassAddMethod(PKVM* vm, PkHandle* cls,
                                const char* name,
                                pkNativeFn fptr, int arity);

// Run the source string. The [source] is expected to be valid till this
// function returns.
PK_PUBLIC PkResult pkRunString(PKVM* vm, const char* source);

// Run the file at [path] relative to the current working directory.
PK_PUBLIC PkResult pkRunFile(PKVM* vm, const char* path);

// FIXME:
// Currently exit function will terminate the process which should exit from
// the function and return to the caller.
//
// Run pocketlang REPL mode. If there isn't any stdin read function defined,
// or imput function ruturned NULL, it'll immediatly return a runtime error.
PK_PUBLIC PkResult pkRunREPL(PKVM* vm);

/*****************************************************************************/
/* NATIVE / RUNTIME FUNCTION API                                             */
/*****************************************************************************/

// Set a runtime error to VM.
PK_PUBLIC void pkSetRuntimeError(PKVM* vm, const char* message);

// Set a runtime error with C formated string.
PK_PUBLIC void pkSetRuntimeErrorFmt(PKVM* vm, const char* fmt, ...);

// Returns native [self] of the current method as a void*.
PK_PUBLIC void* pkGetSelf(const PKVM* vm);

// Return the current functions argument count. This is needed for functions
// registered with -1 argument count (which means variadic arguments).
PK_PUBLIC int pkGetArgc(const PKVM* vm);

// Check if the argc is in the range of (min <= argc <= max), if it's not, a
// runtime error will be set and return false, otherwise return true. Assuming
// that min <= max, and pocketlang won't validate this in release binary.
PK_PUBLIC bool pkCheckArgcRange(PKVM* vm, int argc, int min, int max);

// Helper function to check if the argument at the [arg] slot is Boolean and
// if not set a runtime error.
PK_PUBLIC bool pkValidateSlotBool(PKVM* vm, int arg, bool* value);

// Helper function to check if the argument at the [arg] slot is Number and
// if not set a runtime error.
PK_PUBLIC bool pkValidateSlotNumber(PKVM* vm, int arg, double* value);

// Helper function to check if the argument at the [arg] slot is String and
// if not set a runtime error.
PK_PUBLIC bool pkValidateSlotString(PKVM* vm, int arg,
                                    const char** value, uint32_t* length);

// Helper function to check if the argument at the [arg] slot is of type
// [type] and if not sets a runtime error.
PK_PUBLIC bool pkValidateSlotType(PKVM* vm, int arg, PkVarType type);

// Helper function to check if the argument at the [arg] slot is an instance
// of the class which is at the [cls] index. If not set a runtime error.
PK_PUBLIC bool pkValidateSlotInstanceOf(PKVM* vm, int arg, int cls);

// Helper function to check if the instance at the [inst] slot is an instance
// of the class which is at the [cls] index. The value will be set to [val]
// if the object at [cls] slot isn't a valid class a runtime error will be set
// and return false.
PK_PUBLIC bool pkIsSlotInstanceOf(PKVM* vm, int inst, int cls, bool* val);

// Make sure the fiber has [count] number of slots to work with (including the
// arguments).
PK_PUBLIC void pkReserveSlots(PKVM* vm, int count);

// Returns the available number of slots to work with. It has at least the
// number argument the function is registered plus one for return value.
PK_PUBLIC int pkGetSlotsCount(PKVM* vm);

// Returns the type of the variable at the [index] slot.
PK_PUBLIC PkVarType pkGetSlotType(PKVM* vm, int index);

// Returns boolean value at the [index] slot. If the value at the [index]
// is not a boolean it'll be casted (only for booleans).
PK_PUBLIC bool pkGetSlotBool(PKVM* vm, int index);

// Returns number value at the [index] slot. If the value at the [index]
// is not a boolean, an assertion will fail.
PK_PUBLIC double pkGetSlotNumber(PKVM* vm, int index);

// Returns the string at the [index] slot. The returned pointer is only valid
// inside the native function that called this. Afterwards it may garbage
// collected and become demangled. If the [length] is not NULL the length of
// the string will be written.
PK_PUBLIC const char* pkGetSlotString(PKVM* vm, int index, uint32_t* length);

// Capture the variable at the [index] slot and return its handle. As long as
// the handle is not released with `pkReleaseHandle()` the variable won't be
// garbage collected.
PK_PUBLIC PkHandle* pkGetSlotHandle(PKVM* vm, int index);

// Returns the native instance at the [index] slot. If the value at the [index]
// is not a valid native instance, an assertion will fail.
PK_PUBLIC void* pkGetSlotNativeInstance(PKVM* vm, int index);

// Set the [index] slot value as pocketlang null.
PK_PUBLIC void pkSetSlotNull(PKVM* vm, int index);

// Set the [index] slot boolean value as the given [value].
PK_PUBLIC void pkSetSlotBool(PKVM* vm, int index, bool value);

// Set the [index] slot numeric value as the given [value].
PK_PUBLIC void pkSetSlotNumber(PKVM* vm, int index, double value);

// Create a new String copying the [value] and set it to [index] slot.
PK_PUBLIC void pkSetSlotString(PKVM* vm, int index, const char* value);

// Create a new String copying the [value] and set it to [index] slot. Unlike
// the above function it'll copy only the spicified length.
PK_PUBLIC void pkSetSlotStringLength(PKVM* vm, int index,
                                     const char* value, uint32_t length);

// Create a new string copying from the formated string and set it to [index]
// slot.
PK_PUBLIC void pkSetSlotStringFmt(PKVM* vm, int index, const char* fmt, ...);

// Set the [index] slot's value as the given [handle]. The function won't
// reclaim the ownership of the handle and you can still use it till
// it's released by yourself.
PK_PUBLIC void pkSetSlotHandle(PKVM* vm, int index, PkHandle* handle);

/*****************************************************************************/
/* POCKET FFI                                                                */
/*****************************************************************************/

// Set the attribute with [name] of the instance at the [instance] slot to
// the value at the [value] index slot. Return true on success.
PK_PUBLIC bool pkSetAttribute(PKVM* vm, int instance, int value,
                              const char* name);

// Get the attribute with [name] of the instance at the [instance] slot and
// place it at the [index] slot. Return true on success.
PK_PUBLIC bool pkGetAttribute(PKVM* vm, int instance, const char* name,
                              int index);

// Place the [self] instance at the [index] slot.
PK_PUBLIC void pkPlaceSelf(PKVM* vm, int index);

// Import a module with the [path] and place it at [index] slot. The path
// sepearation should be '/'. Example: to import module "foo.bar" the [path]
// should be "foo/bar". On failure, it'll set an error and return false.
PK_PUBLIC bool pkImportModule(PKVM* vm, const char* path, int index);

// Set the [index] slot's value as the class of the [instance].
PK_PUBLIC void pkGetClass(PKVM* vm, int instance, int index);

// Creates a new instance of class at the [cls] slot, calls the constructor,
// and place it at the [index] slot. Returns true if the instance constructed
// successfully.
//
// [argc] is the argument count for the constructor, and [argv]
// is the first argument slot's index.
PK_PUBLIC bool pkNewInstance(PKVM* vm, int cls, int index, int argc, int argv);

// Calls a function at the [fn] slot, with [argc] argument where [argv] is the
// slot of the first argument. [ret] is the slot index of the return value. if
// [ret] < 0 the return value will be discarded.
PK_PUBLIC bool pkCallFunction(PKVM* vm, int fn, int argc, int argv, int ret);

// Calls a [method] on the [instance] with [argc] argument where [argv] is the
// slot of the first argument. [ret] is the slot index of the return value. if
// [ret] < 0 the return value will be discarded.
PK_PUBLIC bool pkCallMethod(PKVM* vm, int instance, const char* method,
                            int argc, int argv, int ret);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // POCKETLANG_H
