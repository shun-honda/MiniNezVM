#if 0
#include <stdio.h>
#include "libnez.h"

struct MemoryObjectHeader {
  struct MemoryObjectHeader *prev;
  struct MemoryObjectHeader *next;
};

MemoryPool nez_CreateMemoryPool(MemoryPool mpool, size_t init_size) {
  mpool->init_size = init_size;
  mpool->object_pool = (ParsingObject)malloc(sizeof(struct ParsingObject)*init_size);
  mpool->log_pool = (ParsingLog)malloc(sizeof(struct ParsingLog)*init_size);
  mpool->oidx = mpool->lidx = 0;
  assert(mpool->object_pool != NULL);
  return mpool;
}

void MemoryPool_Reset(MemoryPool mpool) { 
  fprintf(stderr, "opool_max=%zu, lpool_max=%zu, pool_max=%zu\n", mpool->oidx, mpool->lidx, mpool->init_size);
  mpool->oidx = mpool->lidx = 0;
}

void nez_DisposeMemoryPool(MemoryPool mpool) {
  MemoryPool_Reset(mpool);
  free(mpool->object_pool);
  free(mpool->log_pool);
  mpool->object_pool = NULL;
  mpool->log_pool = NULL;
}
#endif