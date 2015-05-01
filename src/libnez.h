#include <stdint.h>
#include <stdlib.h>
#include <assert.h>

#ifndef LIBNEZ_H
#define LIBNEZ_H

#if 0
struct ParsingObject {
  int refc; // referencing counting gc
  int child_size;
  struct ParsingObject **child;
  struct ParsingObject *parent;
  long start_pos;
  long end_pos;
  const char *tag;
  const char *value;
};

#define LazyLink_T 0
#define LazyCapture_T 1
#define LazyTag_T 2
#define LazyValue_T	3
#define LazyLeftJoin_T 4
#define LazyNew_T 5

struct ParsingLog {
  int type;
  const char* value;
  long pos;
  struct ParsingObject *po;
  struct ParsingLog *next;
};

struct MemoryPool {
  struct ParsingObject *object_pool;
  struct ParsingLog *log_pool;
  size_t oidx;
  size_t lidx;
  size_t init_size;
};

#endif

union StackEntry {
  const char* pos;
  const struct NezVMInstruction *func;
};

struct ParsingContext {
  char *inputs;
  size_t input_size;
  long pos;
  //struct ParsingObject *left;
  //struct ParsingObject *unusedObject;

  //int logStackSize;
  //struct ParsingLog *logStack;
  //struct ParsingLog *unusedLog;

  // size_t pool_size;
  //struct MemoryPool *mpool;

  long bytecode_length;
  long startPoint;

  size_t stack_size;
  union StackEntry* stack_pointer;
  union StackEntry* stack_pointer_base;
  // long *stack_pointer;
  // struct NezVMInstruction **call_stack_pointer;
  // long *stack_pointer_base;
  // struct NezVMInstruction **call_stack_pointer_base;
};


#if 0
typedef struct ParsingObject *ParsingObject;
typedef struct ParsingLog *ParsingLog;
typedef struct MemoryPool *MemoryPool;
#endif

typedef struct ParsingContext *ParsingContext;
typedef union StackEntry* StackEntry;

#if 0
extern MemoryPool nez_CreateMemoryPool(MemoryPool mpool, size_t init_size);
extern void MemoryPool_Reset(MemoryPool mpool);
extern void nez_DisposeMemoryPool(MemoryPool mpool);

static inline ParsingObject MemoryPool_AllocParsingObject(MemoryPool mpool) {
  assert(mpool->oidx < mpool->init_size);
  return &mpool->object_pool[mpool->oidx++];
}

static inline ParsingLog MemoryPool_AllocParsingLog(MemoryPool mpool) {
  assert(mpool->lidx < mpool->init_size);
  return &mpool->log_pool[mpool->lidx++];
}
#endif

#define PARSING_CONTEXT_MAX_STACK_LENGTH 1024
ParsingContext nez_CreateParsingContext(const char *filename);
void nez_DisposeParsingContext(ParsingContext ctx);

#if 0
void nez_DisposeObject(ParsingObject pego);

void nez_setObject(ParsingContext ctx, ParsingObject *var, ParsingObject o);
ParsingObject nez_setObject_(ParsingContext ctx, ParsingObject var, ParsingObject o);
ParsingObject nez_newObject(ParsingContext ctx, const char *start);
ParsingObject nez_newObject_(ParsingContext ctx, long start, long end,
                             const char* tag, const char* value);
void nez_pushDataLog(ParsingContext ctx, int type, long pos,
                     int index, const char* value, ParsingObject po);
ParsingObject nez_commitLog(ParsingContext ctx, int mark);
void nez_abortLog(ParsingContext ctx, int mark);
int nez_markLogStack(ParsingContext ctx);
#endif

#endif