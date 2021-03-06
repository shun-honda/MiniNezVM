#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include "nezvm.h"

#define NEZVM_COUNT_BYTECODE_MALLOCED_SIZE 1
#if defined(NEZVM_COUNT_BYTECODE_MALLOCED_SIZE)
static size_t bytecode_malloced_size = 0;
#endif
static void *__malloc(size_t size) {
#if defined(NEZVM_COUNT_BYTECODE_MALLOCED_SIZE)
  bytecode_malloced_size += size;
#endif
  return malloc(size);
}

static const char *get_opname(uint8_t opcode) {
  switch (opcode) {
#define OP_DUMPCASE(OP) \
  case NEZVM_OP_##OP:   \
    return "" #OP;
    NEZ_IR_EACH(OP_DUMPCASE);
  default:
    assert(0 && "UNREACHABLE");
    break;
#undef OP_DUMPCASE
  }
  return "";
}

typedef struct byteCodeInfo {
  int pos;
  uint8_t version0;
  uint8_t version1;
  uint32_t filename_length;
  uint8_t *filename;
  uint32_t pool_size_info;
  uint64_t bytecode_length;
} byteCodeInfo;

typedef struct ByteCodeLoader {
  char *input;
  byteCodeInfo *info;
  NezVMInstruction *head;
} ByteCodeLoader;

char *loadFile(const char *filename, size_t *length) {
  size_t len = 0;
  FILE *fp = fopen(filename, "rb");
  char *source;
  if (!fp) {
    nez_PrintErrorInfo("fopen error: cannot open file");
    return NULL;
  }
  fseek(fp, 0, SEEK_END);
  len = (size_t)ftell(fp);
  fseek(fp, 0, SEEK_SET);
  source = (char *)malloc(len + 1);
  if (len != fread(source, 1, len, fp)) {
    nez_PrintErrorInfo("fread error: cannot read file collectly");
  }
  source[len] = '\0';
  fclose(fp);
  *length = len;
  return source;
}

static void dump_byteCodeInfo(byteCodeInfo *info) {
  fprintf(stderr, "ByteCodeVersion:%u.%u\n", info->version0, info->version1);
  fprintf(stderr, "PEGFile:%s\n", info->filename);
  fprintf(stderr, "LengthOfByteCode:%zd\n", (size_t)info->bytecode_length);
  fprintf(stderr, "\n");
}

static int16_t read16(char *inputs, byteCodeInfo *info) {
  uint16_t value = (uint8_t)inputs[info->pos++];
  value = (value) | ((uint8_t)inputs[info->pos++] << 8);
  return value;
}

static int32_t read32(char *inputs, byteCodeInfo *info) {
  uint32_t value = 0;
  value = (uint8_t)inputs[info->pos++];
  value = (value) | ((uint8_t)inputs[info->pos++] << 8);
  value = (value) | ((uint8_t)inputs[info->pos++] << 16);
  value = (value) | ((uint8_t)inputs[info->pos++] << 24);
  return value;
}

static int64_t read64(char *inputs, byteCodeInfo *info) {
  uint64_t value1 = read32(inputs, info);
  uint64_t value2 = read32(inputs, info);
  return value2 << 32 | value1;
}

static int32_t Loader_Read32(ByteCodeLoader *loader) {
  return read32(loader->input, loader->info);
}

static int16_t Loader_Read16(ByteCodeLoader *loader) {
  return read16(loader->input, loader->info);
}

static NezVMInstruction *GetJumpAddr(NezVMInstruction *inst, int offset) {
  return inst + offset;
}

static NezVMInstruction *Loader_GetJumpAddr(ByteCodeLoader *loader,
                                            NezVMInstruction *inst) {
  int dst = Loader_Read32(loader);
  return GetJumpAddr(inst, dst);
}

static nezvm_string_ptr_t Loader_ReadString(ByteCodeLoader *loader) {
  uint32_t len = Loader_Read16(loader);
  nezvm_string_ptr_t str = (nezvm_string_ptr_t)__malloc(sizeof(*str) - 1 + len);
  str->len = len;
  for (uint32_t i = 0; i < len; i++) {
    str->text[i] = Loader_Read32(loader);
  }
  return str;
}

void nez_EmitInstruction(NezVMInstruction* ir, ByteCodeLoader *loader, ParsingContext context) {
  switch(ir->op) {
    case NEZVM_OP_JUMP:
    case NEZVM_OP_IFFAIL: {
      ir->arg = Loader_Read16(loader);
      break;
    }
    case NEZVM_OP_CALL: {
      ir->arg = loader->input[loader->info->pos++];
      context->call_table[ir->arg] = Loader_Read32(loader);
      // fprintf(stderr, "%d: %d\n", ir->arg, context->call_table[ir->arg]);
      break;
    }
    case NEZVM_OP_CHAR: {
      ir->arg = loader->input[loader->info->pos++];
      break;
    }
  }
}

NezVMInstruction *nez_VM_Prepare(ParsingContext, NezVMInstruction *);

NezVMInstruction *nez_LoadMachineCode(ParsingContext context,
                                      const char *fileName,
                                      const char *nonTerminalName) {
  NezVMInstruction *inst = NULL;
  NezVMInstruction *head = NULL;
  size_t len;
  char *buf = loadFile(fileName, &len);
  byteCodeInfo info;
  info.pos = 0;

  /* load bytecode header */
  info.version0 = buf[info.pos++]; /* version info */
  info.version1 = buf[info.pos++];
  info.filename_length = read32(buf, &info); /* file name */
  info.filename = malloc(sizeof(uint8_t) * info.filename_length + 1);
  for (uint32_t i = 0; i < info.filename_length; i++) {
    info.filename[i] = buf[info.pos++];
  }
  info.filename[info.filename_length] = 0;

  /* rule size */
  int rule_size = read32(buf, &info);
  context->call_table = (int *)malloc(sizeof(int) * rule_size);

  /* bytecode length */
  info.bytecode_length = read64(buf, &info);
  dump_byteCodeInfo(&info);
  free(info.filename);

  /* 
  ** head is a tmporary variable that indecates the begining
  ** of the instruction sequence
  */
  head = inst = __malloc(sizeof(*inst) * info.bytecode_length);
  memset(inst, 0, sizeof(*inst) * info.bytecode_length);

  /* init bytecode loader */
  ByteCodeLoader loader;
  loader.input = buf;
  loader.info = &info;
  loader.head = head;

  /* f_convert[] is function pointer that emit instruction */
  for (uint64_t i = 0; i < info.bytecode_length; i++) {
    inst->op = buf[info.pos++];
    // fprintf(stderr, "%s\n", get_opname(inst->op));
    nez_EmitInstruction(inst, &loader, context);
    inst++;
  }

#if PEGVM_DEBUG
  dump_NezVMInstructions(inst, info.bytecode_length);
#endif

  context->bytecode_length = info.bytecode_length;
#if defined(NEZVM_COUNT_BYTECODE_MALLOCED_SIZE)
  fprintf(stderr, "instruction_size=%zd\n", sizeof(*inst));
  fprintf(stderr, "malloced_size=%zd[Byte], %zd[Byte]\n",
          (sizeof(*inst) * info.bytecode_length),
          bytecode_malloced_size);
#endif
  free(buf);
  return head;
}

void nez_DisposeInstruction(NezVMInstruction *ir, long length) {
  free(ir);
}
