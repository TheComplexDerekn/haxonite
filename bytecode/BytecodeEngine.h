//========================================================================
//
// BytecodeEngine.h
//
// The bytecode interpreter.
//
// Part of the Haxonite project, under the MIT License.
// Copyright 2025 Derek Noonburg
//
//========================================================================

#ifndef BytecodeEngine_h
#define BytecodeEngine_h

#include <stdint.h>
#include <stdio.h>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include "ConfigFile.h"

//------------------------------------------------------------------------

class BytecodeEngine;

using NativeFunc = void (*)(BytecodeEngine&);

// Native functions should be defined like this:
//     static NativeFuncDefn(foo) {
//       ...
//       engine.doReturn();
//     }
// (the 'static' is optional)
#define NativeFuncDefn(name) void __attribute__((aligned(16))) name(BytecodeEngine &engine)

//------------------------------------------------------------------------

// Cell types:
// - int (56-bit)             - tag = 11111 111
// - float (32-bit)           - tag = 00000 111
// - bool (1-bit)             - tag = 00001 111
// - bytecode addr (56-bit)   - tag = 00010 111
// - saved reg (56-bit)       - tag = 00011 111
// - heap ptr (61-bit)        - tag =       000  } 'ptr' refers to any of these
// - non-heap ptr (61-bit)    - tag =       001  }
// - resource ptr (61-bit)    - tag =       010  }
// - native func ptr (61-bit) - tag =       100

using Cell = uint64_t;

static inline int cellType(Cell cell) {
  int t = (int)cell & 7;
  return (t == 7) ? ((int)cell & 0xff) : t;
}

static inline bool cellIsInt(Cell cell)          { return (cell & 0xff) == 0xff; }
static inline bool cellIsFloat(Cell cell)        { return (cell & 0xff) == 0x07; }
static inline bool cellIsBool(Cell cell)         { return (cell & 0xff) == 0x0f; }
static inline bool cellIsBytecodeAddr(Cell cell) { return (cell & 0xff) == 0x17; }
static inline bool cellIsSavedReg(Cell cell)     { return (cell & 0xff) == 0x1f; }
static inline bool cellIsError(Cell cell)        { return (cell & 0xff) == 0x27; }
static inline bool cellIsHeapPtr(Cell cell)      { return (cell & 0x07) == 0x00; }
static inline bool cellIsNonHeapPtr(Cell cell)   { return (cell & 0x07) == 0x01; }
static inline bool cellIsResourcePtr(Cell cell)  { return (cell & 0x07) == 0x02; }
static inline bool cellIsPtr(Cell cell)          { return (cell & 0x04) == 0x00; }
static inline bool cellIsNativePtr(Cell cell)    { return (cell & 0x07) == 0x04; }
static inline bool cellIsNilHeapPtr(Cell cell)   { return cell == 0; }
static inline bool cellIsNilPtr(Cell cell)       { return (cell & ~(uint64_t)7) == 0; }

static inline int64_t cellInt(Cell cell)         { return (int64_t)cell >> 8; }
static inline float cellFloat(Cell cell) {
  union {
    uint32_t i;
    float f;
  } u;
  u.i = (uint32_t)(cell >> 32);
  return u.f;
}
static inline bool cellBool(Cell cell)            { return cell & 0x100; }
static inline size_t cellBytecodeAddr(Cell cell)  { return cell >> 8; }
static inline size_t cellSavedReg(Cell cell)      { return cell >> 8; }
static inline void *cellHeapPtr(Cell cell)        { return (void *)cell; }
static inline void *cellNonHeapPtr(Cell cell)     { return (void *)(cell & ~(uint64_t)7); }
static inline void *cellResourcePtr(Cell cell)    { return (void *)(cell & ~(uint64_t)7); }
static inline void *cellPtr(Cell cell)            { return (void *)(cell & ~(uint64_t)7); }
static inline NativeFunc cellNativePtr(Cell cell) { return (NativeFunc)(cell & ~(uint64_t)7); }

static inline Cell cellMakeInt(int64_t x)          { return ((uint64_t)x << 8) | 0xff; }
static inline Cell cellMakeFloat(float x) {
  union {
    uint32_t i;
    float f;
  } u;
  u.f = x;
  return ((uint64_t)u.i << 32) | 0x07;
}
static inline Cell cellMakeBool(bool x)            { return x ? 0x10f : 0x00f; }
static inline Cell cellMakeBytecodeAddr(size_t x)  { return ((uint64_t)x << 8) | 0x17; }
static inline Cell cellMakeSavedReg(size_t x)      { return ((uint64_t)x << 8) | 0x1f; }
static inline Cell cellMakeError()                 { return 0x27; }
static inline Cell cellMakeHeapPtr(void *x)        { return (uint64_t)x; }
static inline Cell cellMakeNonHeapPtr(void *x)     { return (uint64_t)x | 0x01; }
static inline Cell cellMakeResourcePtr(void *x)    { return (uint64_t)x | 0x02; }
static inline Cell cellMakeNativePtr(NativeFunc x) { return (uint64_t)x | 0x04; }
static inline Cell cellMakeNilHeapPtr()            { return 0; }
static inline Cell cellMakeNilResourcePtr()        { return 0x02; }

#define cellNilHeapPtrInit ((Cell)0)

//------------------------------------------------------------------------

// Heap object access.
// The pointer arg to these functions comes from cellPtr().

static inline int64_t heapObjSize(void *p) { return (int64_t)(*(uint64_t *)p >> 8); }
static inline void heapObjSetSize(void *p, int64_t size) {
  *(uint64_t *)p = (size << 8) | *(uint8_t *)p;
}
static inline int heapObjGCTag(void *p) { return *(uint8_t *)p & 3; }
static inline int heapObjTypeTag(void *p) { return *(uint8_t *)p >> 2; }
static inline void *heapObjRelocatedPtr(void *p) { return (void *)(*(uint64_t *)p & ~(uint64_t)7); }

#define gcTagBlob       ((uint8_t)0)
#define gcTagRelocated  ((uint8_t)1)
#define gcTagTuple      ((uint8_t)2)
#define gcTagHandle     ((uint8_t)3)

//------------------------------------------------------------------------

struct ResourceObject {
  void (*finalizer)(ResourceObject *resObj);
  ResourceObject *prev;
  ResourceObject *next;
  bool marked;
};

//------------------------------------------------------------------------

class BytecodeEngine {
public:

  // Create a BytecodeEngine, with a stack of [aStackSize] cells, and
  // a heap of [aInitialHeapSize] bytes. The stack size is fixed. The
  // heap can grow as needed.
  BytecodeEngine(const std::string &configPath, size_t aStackSize, size_t aInitialHeapSize,
		 bool aVerbose);

  //--- load and run

  // Load a bytecode file. This replaces any current bytecode in the
  // engine. It also resets the stack and heap. Any native functions
  // must be added (via addNativeFunction()) before calling this.
  // Returns true on success, false on failure.
  bool loadBytecodeFile(const std::string &path);

  // Looks for a bytecode function named [name]. If found: calls it,
  // with [nArgs] arguments on the stack, then returns true. If not
  // found: returns false.
  bool callFunction(const std::string &name, int nArgs);

  // Calls [funcPtr] with [nArgs] arguments.
  void callFunctionPtr(Cell &funcPtrCell, int nArgs);

  //--- setup

  // Add a native function, which will be available to the bytecode.
  void addNativeFunction(const std::string &name, NativeFunc func);

  //--- support for native functions

  // Return the number of args passed to this function.
  int nArgs();

  // Return the [idx]th arg passed to this function. The returned
  // value is a pointer to a Cell on the stack; the GC may change the
  // pointer in the Cell.
  Cell &arg(int idx);

  // Push a Cell onto the stack.
  void push(Cell cell);

  // Pop a Cell off the stack.
  // NB: if the returned Cell contains a pointer, it must be
  // registered as a GC root before any allocation function is called.
  Cell pop();

  // Type-checked pops.
  int64_t popInt();
  float popFloat();
  bool popBool();
  size_t popBytecodeAddr();
  size_t popSavedReg();
  void *popHeapPtr();
  void *popNonHeapPtr();
  void *popPtr();
  NativeFunc popNativeFunc();

  //--- heap

  // Allocate a blob (non-pointer data) with [size] bytes of data.
  // The returned pointer is on the heap, so it should immediately be
  // pushed or otherwise made visible to the GC.
  void *heapAllocBlob(uint64_t size, uint8_t typeTag);

  // Allocate a tuple (sequence of cells) with [size] cells.  The
  // returned pointer is on the heap, so it should immediately be
  // pushed or otherwise made visible to the GC.
  void *heapAllocTuple(uint64_t size, uint8_t typeTag);

  // Allocate a handle (single cell) with size field set to [size].
  // The returned pointer is on the heap, so it should immediately be
  // pushed or otherwise made visible to the GC.
  void *heapAllocHandle(uint64_t size, uint8_t typeTag);

  // Push a Cell reference onto the stack of GC roots.
  void pushGCRoot(Cell &cell);

  // Pop a Cell reference off the stack of GC roots. popGCRoot() must
  // be called in the reverse order of pushGCRoot().
  void popGCRoot(Cell &cell);

  // Add a resource object to the list of live resource objects.
  void addResourceObject(ResourceObject *resObj);

  // Remove a resource object from the list of live resource objects.
  // This is intended to be called when a resource is explicitly
  // closed; it does not call the finalizer.
  void removeResourceObject(ResourceObject *resObj);

  // Return the current heap size.
  size_t currentHeapSize();

  //--- config file

  // Get the config item corresponding to [cmd] in section
  // [sectionTag]. Returns null if that item isn't present.
  ConfigFile::Item *configItem(const std::string &sectionTag, const std::string &cmd);

  //--- fatal errors

  // Emit an error message and exit.
  [[noreturn]] static void fatalError(const char *msg);

  // If [ptr] is nil, throw a fatal error.
  static void failOnNilPtr(void *ptr) {
    if (!ptr) {
      fatalError("Nil pointer dereference");
    }
  }

  // If [cell] is a nil pointer, throw a fatal error.
  static void failOnNilPtr(Cell &cell) {
    if (cellIsNilPtr(cell)) {
      fatalError("Nil pointer dereference");
    }
  }

private:

  void loadConfigFile(const std::string &configPath);
  bool load(const std::string &path);
  void run();
  void doReturn();
  uint8_t readBytecodeUint8();
  int32_t readBytecodeInt32();
  uint32_t readBytecodeUint32();
  int64_t readBytecodeInt56();
  uint64_t readBytecodeUint56();
  uint64_t readBytecodeUint64();
  float readBytecodeFloat32();
  bool writeBytecodeUint64(size_t addr, uint64_t value);

  void heapInit();
  void *heapAlloc(uint64_t nWords, uint64_t size, uint8_t typeTag, uint8_t gcTag);
  void gc(uint64_t nWords);
  void fullGC(size_t newHeapSize);
  void quickGC(size_t newHeapSize);
  void scanResourceObjects();

  ConfigFile cfg;
  bool verbose;

  std::vector<uint8_t> bytecode;
  std::vector<uint8_t> data;

  std::unique_ptr<Cell[]> stack;
  size_t stackSize;

  std::unique_ptr<uint64_t[]> heap;
  size_t heapSize;
  size_t heapNext;
  size_t prevCompactedHeapSize;
  size_t initialHeapSize;
  std::vector<Cell*> gcRoots;
  ResourceObject *resObjs;

  std::unordered_map<std::string, size_t> funcDefns;
  std::unordered_map<std::string, size_t> dataDefns;
  std::unordered_map<std::string, NativeFunc> nativeFuncs;

  size_t sp;			// stack pointer (stack address)
  size_t fp;			// frame pointer (stack address)
  size_t ap;			// arg pointer (stack address)
  size_t pc;			// program counter (bytecode address)
};

#endif // BytecodeEngine_h
