#include <stdio.h>
#include <string.h>
#include "libnez.h"
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

static uint16_t read16(char *inputs, byteCodeInfo *info) {
  uint16_t value = (uint8_t)inputs[info->pos++];
  value = (value) | ((uint8_t)inputs[info->pos++] << 8);
  return value;
}

static uint32_t read32(char *inputs, byteCodeInfo *info) {
  uint32_t value = 0;
  value = (uint8_t)inputs[info->pos++];
  value = (value) | ((uint8_t)inputs[info->pos++] << 8);
  value = (value) | ((uint8_t)inputs[info->pos++] << 16);
  value = (value) | ((uint8_t)inputs[info->pos++] << 24);
  return value;
}

static uint64_t read64(char *inputs, byteCodeInfo *info) {
  uint64_t value1 = read32(inputs, info);
  uint64_t value2 = read32(inputs, info);
  return value2 << 32 | value1;
}

static uint32_t Loader_Read32(ByteCodeLoader *loader) {
  return read32(loader->input, loader->info);
}

static uint16_t Loader_Read16(ByteCodeLoader *loader) {
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

static nezvm_string_ptr_t Loader_ReadName(ByteCodeLoader *loader) {
  uint32_t len = Loader_Read16(loader);
  nezvm_string_ptr_t str = (nezvm_string_ptr_t)__malloc(sizeof(*str) + len);
  str->len = len;
  memcpy(str->text, loader->input + loader->info->pos, len);
  str->text[len] = 0;
  loader->info->pos += len;
  return str;
}

void nez_EmitInstruction(NezVMInstruction* ir, ByteCodeLoader *loader) {
  switch(ir->opcode) {
    case NEZVM_OP_JUMP:
    case NEZVM_OP_CALL:
    case NEZVM_OP_IFFAIL:
    case NEZVM_OP_IFSUCC:
    case NEZVM_OP_ANY: {
      ir->arg0.jump = Loader_GetJumpAddr(loader, ir);
      break;
    }
    case NEZVM_OP_CHAR:
    case NEZVM_OP_NOTCHAR:
    case NEZVM_OP_NOTCHARANY: {
      ir->arg0.c = Loader_Read32(loader);
      ir->arg1.jump = Loader_GetJumpAddr(loader, ir);
      break;
    }
    case NEZVM_OP_CHARMAP:
    case NEZVM_OP_NOTCHARMAP: {
      int len = Loader_Read16(loader);
      ir->arg0.set = (bitset_ptr_t)__malloc(sizeof(bitset_t));
      bitset_init(ir->arg0.set);
      for (int i = 0; i < len; i++) {
        unsigned c = Loader_Read32(loader);
        bitset_set(ir->arg0.set, c);
      }
      ir->arg1.jump = Loader_GetJumpAddr(loader, ir);
      break;
    }
    case NEZVM_OP_STRING:
    case NEZVM_OP_NOTSTRING: {
      ir->arg0.str = Loader_ReadString(loader);
      ir->arg1.jump = Loader_GetJumpAddr(loader, ir);
      break;
    }
    case NEZVM_OP_STOREflag: {
      ir->arg0.val = Loader_Read32(loader);
      break;
    }
    case NEZVM_OP_LEFTJOIN: {
      ir->arg0.val = Loader_Read32(loader);
      break;
    }
    case NEZVM_OP_COMMIT: {
      ir->arg0.val = Loader_Read32(loader);
      break;
    }
    case NEZVM_OP_TAG:
    case NEZVM_OP_VALUE: {
      ir->arg0.str = Loader_ReadName(loader);
      break;
    }
    case NEZVM_OP_OPTIONALCHAR: {
      ir->arg0.c = Loader_Read32(loader);
      break;
    }
    case NEZVM_OP_OPTIONALCHARMAP:
    case NEZVM_OP_ZEROMORECHARMAP: {
      int len = Loader_Read16(loader);
      ir->arg0.set = (bitset_ptr_t)__malloc(sizeof(bitset_t));
      bitset_init(ir->arg0.set);
      for (int i = 0; i < len; i++) {
        unsigned c = Loader_Read32(loader);
        bitset_set(ir->arg0.set, c);
      }
      break;
    }
    case NEZVM_OP_OPTIONALSTRING: {
      ir->arg0.str = Loader_ReadString(loader);
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

  /* determine the starting point */
  int ruleSize = read32(buf, &info);
  char **ruleTable = (char **)malloc(sizeof(char *) * ruleSize);
  for (int i = 0; i < ruleSize; i++) {
    int ruleNameLen = read32(buf, &info);
    ruleTable[i] = (char *)malloc(ruleNameLen);
    for (int j = 0; j < ruleNameLen; j++) {
      ruleTable[i][j] = buf[info.pos++];
    }
    long index = read64(buf, &info);
    if (nonTerminalName != NULL) {
      if (!strcmp(ruleTable[i], nonTerminalName)) {
        context->startPoint = index;
      }
    }
  }

  /* bytecode length */
  info.bytecode_length = read64(buf, &info);
  dump_byteCodeInfo(&info);
  free(info.filename);

  /* dispose rule table to deterimine the starting point */
  for (int i = 0; i < ruleSize; i++) {
    free(ruleTable[i]);
  }
  free(ruleTable);

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
    inst->opcode = buf[info.pos++];
    fprintf(stderr, "%s\n", get_opname(inst->opcode));
    nez_EmitInstruction(inst, &loader);
    inst++;
  }

#if PEGVM_DEBUG
  dump_NezVMInstructions(inst, info.bytecode_length);
#endif

  context->bytecode_length = info.bytecode_length;
  context->pool_size = info.pool_size_info;
#if defined(NEZVM_COUNT_BYTECODE_MALLOCED_SIZE)
  fprintf(stderr, "instruction_size=%zd\n", sizeof(*inst));
  fprintf(stderr, "malloced_size=%zd[Byte], %zd[Byte]\n",
          (sizeof(*inst) * info.bytecode_length),
          bytecode_malloced_size);
#endif
  free(buf);
  return nez_VM_Prepare(context, head);
}

void nez_DisposeInstruction(NezVMInstruction *ir, long length) {
  for (long i = 0; i < length; i++) {
    switch (ir[i].opcode) {
      case NEZVM_OP_CHARMAP:
      case NEZVM_OP_NOTCHARMAP:
      case NEZVM_OP_OPTIONALCHARMAP:
      case NEZVM_OP_ZEROMORECHARMAP: {
        free(ir->arg0.set);
        break;
      }
      case NEZVM_OP_STRING:
      case NEZVM_OP_NOTSTRING:
      case NEZVM_OP_OPTIONALSTRING:
      case NEZVM_OP_TAG:
      case NEZVM_OP_VALUE: {
        free(ir->arg0.str);
        break;
      }
    }
  }
  free(ir);
}