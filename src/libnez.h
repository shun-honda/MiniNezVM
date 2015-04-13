#include <stdint.h>
#include <stdlib.h>
#include <assert.h>

#ifndef LIBNEZ_H
#define LIBNEZ_H

struct ParsingObject {
  int refc; // referencing counting gc
  int child_size;
  union {
    struct ParsingObject *parent;
    struct ParsingObject **child;
  };
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

struct MemoEntry {
  long key;
  int consumed;
  struct ParsingObject *result;
};

struct MemoTable {
  int shift;
  int memoHit;
  int memoMiss;
  int memoStored;
  size_t size;
  struct MemoEntry** memoArray;
};

struct ParsingContext {
  char *inputs;
  size_t input_size;
  long pos;
  struct ParsingObject *left;
  struct ParsingObject *unusedObject;

  int logStackSize;
  struct ParsingLog *logStack;
  struct ParsingLog *unusedLog;

  size_t pool_size;
  struct MemoryPool *mpool;

  struct MemoTable *memoTable;

  long bytecode_length;
  long startPoint;

  long *stack_pointer;
  union NezVMInstruction **call_stack_pointer;
  long *stack_pointer_base;
  union NezVMInstruction **call_stack_pointer_base;
};

typedef struct ParsingObject *ParsingObject;
typedef struct ParsingLog *ParsingLog;
typedef struct ParsingContext *ParsingContext;
typedef struct MemoryPool *MemoryPool;
typedef struct MemoEntry *MemoEntry;
typedef struct MemoTable *MemoTable;

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

#define PARSING_CONTEXT_MAX_STACK_LENGTH 1024
ParsingContext nez_CreateParsingContext(const char *filename);
void nez_DisposeParsingContext(ParsingContext ctx);
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

void nez_setMemo(ParsingContext ctx, const char* pos, int memoPoint, int consumed, ParsingObject result);
MemoEntry nez_getMemo(ParsingContext ctx, const char* pos, int memoPoint);

#endif