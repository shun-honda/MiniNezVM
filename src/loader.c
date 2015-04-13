#include <stdio.h>
#include <string.h>
#include "nezvm.h"

/* load input file or bytecode file */
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

static void Emit_EXIT(NezVMInstruction *inst, ByteCodeLoader *loader) {
  IEXIT *ir = (IEXIT *)inst;
  ir->base.opcode = OPCODE_IEXIT;
}

static void Emit_JUMP(NezVMInstruction *inst, ByteCodeLoader *loader) {
  IJUMP *ir = (IJUMP *)inst;
  ir->base.opcode = OPCODE_IJUMP;
  ir->jump = Loader_GetJumpAddr(loader, inst);
}

static void Emit_CALL(NezVMInstruction *inst, ByteCodeLoader *loader) {
  ICALL *ir = (ICALL *)inst;
  ir->base.opcode = OPCODE_ICALL;
  ir->jump = Loader_GetJumpAddr(loader, inst);
}

static void Emit_RET(NezVMInstruction *inst, ByteCodeLoader *loader) {
  IRET *ir = (IRET *)inst;
  ir->base.opcode = OPCODE_IRET;
}

static void Emit_IFFAIL(NezVMInstruction *inst, ByteCodeLoader *loader) {
  IIFFAIL *ir = (IIFFAIL *)inst;
  ir->base.opcode = OPCODE_IIFFAIL;
  ir->jump = Loader_GetJumpAddr(loader, inst);
}

static void Emit_IFSUCC(NezVMInstruction *inst, ByteCodeLoader *loader) {
  IIFSUCC *ir = (IIFSUCC *)inst;
  ir->base.opcode = OPCODE_IIFSUCC;
  ir->jump = Loader_GetJumpAddr(loader, inst);
}

static void Emit_CHAR(NezVMInstruction *inst, ByteCodeLoader *loader) {
  ICHAR *ir = (ICHAR *)inst;
  ir->base.opcode = OPCODE_ICHAR;
  ir->c = Loader_Read32(loader);
  ir->jump = Loader_GetJumpAddr(loader, inst);
}

static void Emit_CHARMAP(NezVMInstruction *inst, ByteCodeLoader *loader) {
  ICHARMAP *ir = (ICHARMAP *)inst;
  ir->base.opcode = OPCODE_ICHARMAP;
  int len = Loader_Read16(loader);
  ir->set = (bitset_ptr_t)__malloc(sizeof(bitset_t));
  bitset_init(ir->set);
  for (int i = 0; i < len; i++) {
    unsigned c = Loader_Read32(loader);
    bitset_set(ir->set, c);
  }
  ir->jump = Loader_GetJumpAddr(loader, inst);
}

static void Emit_STRING(NezVMInstruction *inst, ByteCodeLoader *loader) {
  ISTRING *ir = (ISTRING *)inst;
  ir->base.opcode = OPCODE_ISTRING;
  ir->str = Loader_ReadString(loader);
  ir->jump = Loader_GetJumpAddr(loader, inst);
}

static void Emit_ANY(NezVMInstruction *inst, ByteCodeLoader *loader) {
  IANY *ir = (IANY *)inst;
  ir->base.opcode = OPCODE_IANY;
  ir->jump = Loader_GetJumpAddr(loader, inst);
}

static void Emit_PUSHpos(NezVMInstruction *inst, ByteCodeLoader *loader) {
  IPUSHpos *ir = (IPUSHpos *)inst;
  ir->base.opcode = OPCODE_IPUSHpos;
}

static void Emit_PUSHmark(NezVMInstruction *inst, ByteCodeLoader *loader) {
  IPUSHmark *ir = (IPUSHmark *)inst;
  ir->base.opcode = OPCODE_IPUSHmark;
}

static void Emit_POPpos(NezVMInstruction *inst, ByteCodeLoader *loader) {
  IPOPpos *ir = (IPOPpos *)inst;
  ir->base.opcode = OPCODE_IPOPpos;
}

static void Emit_STOREpos(NezVMInstruction *inst, ByteCodeLoader *loader) {
  ISTOREpos *ir = (ISTOREpos *)inst;
  ir->base.opcode = OPCODE_ISTOREpos;
}

static void Emit_STOREflag(NezVMInstruction *inst, ByteCodeLoader *loader) {
  ISTOREflag *ir = (ISTOREflag *)inst;
  ir->base.opcode = OPCODE_ISTOREflag;
  ir->val = Loader_Read32(loader);
}

static void Emit_NEW(NezVMInstruction *inst, ByteCodeLoader *loader) {
  INEW *ir = (INEW *)inst;
  ir->base.opcode = OPCODE_INEW;
}

static void Emit_LEFTJOIN(NezVMInstruction *inst, ByteCodeLoader *loader) {
  ILEFTJOIN *ir = (ILEFTJOIN *)inst;
  ir->base.opcode = OPCODE_ILEFTJOIN;
  ir->index = Loader_Read32(loader);
}

static void Emit_CAPTURE(NezVMInstruction *inst, ByteCodeLoader *loader) {
  ICAPTURE *ir = (ICAPTURE *)inst;
  ir->base.opcode = OPCODE_ICAPTURE;
}

static void Emit_COMMIT(NezVMInstruction *inst, ByteCodeLoader *loader) {
  ICOMMIT *ir = (ICOMMIT *)inst;
  ir->base.opcode = OPCODE_ICOMMIT;
  ir->index = Loader_Read32(loader);
}

static void Emit_ABORT(NezVMInstruction *inst, ByteCodeLoader *loader) {
  IABORT *ir = (IABORT *)inst;
  ir->base.opcode = OPCODE_IABORT;
}

static void Emit_TAG(NezVMInstruction *inst, ByteCodeLoader *loader) {
  ITAG *ir = (ITAG *)inst;
  ir->base.opcode = OPCODE_ITAG;
  ir->tag = Loader_ReadName(loader);
}

static void Emit_VALUE(NezVMInstruction *inst, ByteCodeLoader *loader) {
  IVALUE *ir = (IVALUE *)inst;
  ir->base.opcode = OPCODE_IVALUE;
  ir->value = Loader_ReadName(loader);
}

static void Emit_MEMOIZE(NezVMInstruction *inst, ByteCodeLoader *loader) {
  IMEMOIZE *ir = (IMEMOIZE *)inst;
  ir->base.opcode = OPCODE_IMEMOIZE;
}

static void Emit_LOOKUP(NezVMInstruction *inst, ByteCodeLoader *loader) {
  ILOOKUP *ir = (ILOOKUP *)inst;
  ir->base.opcode = OPCODE_ILOOKUP;
}

static void Emit_MEMOIZENODE(NezVMInstruction *inst, ByteCodeLoader *loader) {
  IMEMOIZENODE *ir = (IMEMOIZENODE *)inst;
  ir->base.opcode = OPCODE_IMEMOIZENODE;
}

static void Emit_LOOKUPNODE(NezVMInstruction *inst, ByteCodeLoader *loader) {
  ILOOKUPNODE *ir = (ILOOKUPNODE *)inst;
  ir->base.opcode = OPCODE_ILOOKUPNODE;
}

static void Emit_NOTCHAR(NezVMInstruction *inst, ByteCodeLoader *loader) {
  INOTCHAR *ir = (INOTCHAR *)inst;
  ir->base.opcode = OPCODE_INOTCHAR;
  ir->c = Loader_Read32(loader);
  ir->jump = Loader_GetJumpAddr(loader, inst);
}

static void Emit_NOTCHARMAP(NezVMInstruction *inst, ByteCodeLoader *loader) {
  INOTCHARMAP *ir = (INOTCHARMAP *)inst;
  ir->base.opcode = OPCODE_INOTCHARMAP;
  int len = Loader_Read16(loader);
  ir->set = (bitset_ptr_t)__malloc(sizeof(bitset_t));
  bitset_init(ir->set);
  for (int i = 0; i < len; i++) {
    unsigned c = Loader_Read32(loader);
    bitset_set(ir->set, c);
  }
  ir->jump = Loader_GetJumpAddr(loader, inst);
}

static void Emit_NOTSTRING(NezVMInstruction *inst, ByteCodeLoader *loader) {
  INOTSTRING *ir = (INOTSTRING *)inst;
  ir->base.opcode = OPCODE_INOTSTRING;
  ir->str = Loader_ReadString(loader);
  ir->jump = Loader_GetJumpAddr(loader, inst);
}

static void Emit_NOTCHARANY(NezVMInstruction *inst, ByteCodeLoader *loader) {
  INOTCHARANY *ir = (INOTCHARANY *)inst;
  ir->base.opcode = OPCODE_INOTCHARANY;
  ir->c = Loader_Read32(loader);
  ir->jump = Loader_GetJumpAddr(loader, inst);
}

static void Emit_OPTIONALCHAR(NezVMInstruction *inst, ByteCodeLoader *loader) {
  IOPTIONALCHAR *ir = (IOPTIONALCHAR *)inst;
  ir->base.opcode = OPCODE_IOPTIONALCHAR;
  ir->c = Loader_Read32(loader);
}

static void Emit_OPTIONALCHARMAP(NezVMInstruction *inst, ByteCodeLoader *loader) {
  IOPTIONALCHARMAP *ir = (IOPTIONALCHARMAP *)inst;
  ir->base.opcode = OPCODE_IOPTIONALCHARMAP;
  int len = Loader_Read16(loader);
  ir->set = (bitset_ptr_t)__malloc(sizeof(bitset_t));
  bitset_init(ir->set);
  for (int i = 0; i < len; i++) {
    unsigned c = Loader_Read32(loader);
    bitset_set(ir->set, c);
  }
}

static void Emit_OPTIONALSTRING(NezVMInstruction *inst, ByteCodeLoader *loader) {
  IOPTIONALSTRING *ir = (IOPTIONALSTRING *)inst;
  ir->base.opcode = OPCODE_IOPTIONALSTRING;
  ir->str = Loader_ReadString(loader);
}

static void Emit_ZEROMORECHARMAP(NezVMInstruction *inst, ByteCodeLoader *loader) {
  IZEROMORECHARMAP *ir = (IZEROMORECHARMAP *)inst;
  ir->base.opcode = OPCODE_IZEROMORECHARMAP;
  int len = Loader_Read16(loader);
  ir->set = (bitset_ptr_t)__malloc(sizeof(bitset_t));
  bitset_init(ir->set);
  for (int i = 0; i < len; i++) {
    unsigned c = Loader_Read32(loader);
    bitset_set(ir->set, c);
  }
}

typedef void (*convert_to_lir_func_t)(NezVMInstruction *, ByteCodeLoader *);
static convert_to_lir_func_t f_convert[] = {
#define DEFINE_CONVERT_FUNC(OP) Emit_##OP,
  NEZ_IR_EACH(DEFINE_CONVERT_FUNC)
#undef DEFINE_CONVERT_FUNC
};

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
    int opcode = buf[info.pos++];
    //fprintf(stderr, "%s\n", get_opname(opcode));
    f_convert[opcode](inst, &loader);
    inst++;
  }

#if PEGVM_DEBUG
  dump_NezVMInstructions(inst, info.bytecode_length);
#endif

  context->bytecode_length = info.bytecode_length;
  context->pool_size = info.pool_size_info;
#if defined(PEGVM_COUNT_BYTECODE_MALLOCED_SIZE)
  fprintf(stderr, "malloced_size=%zdKB, %zdKB\n",
          (sizeof(*inst) * info.bytecode_length) / 1024,
          bytecode_malloced_size / 1024);
#endif
  free(buf);
  return nez_VM_Prepare(context, head);
}

void nez_DisposeInstruction(NezVMInstruction *inst, long length) {
  for (long i = 0; i < length; i++) {
    int opcode = inst[i].base.opcode;
    switch (opcode) {
      case OPCODE_ICHARMAP: {
        ICHARMAP *ir = (ICHARMAP *)inst;
        free(ir->set);
        break;
      }
      case OPCODE_ISTRING: {
        ISTRING *ir = (ISTRING *)inst;
        free(ir->str);
        break;
      }
      case OPCODE_ITAG: {
        ITAG *ir = (ITAG *)inst;
        free(ir->tag);
        break;
      }
      case OPCODE_IVALUE: {
        IVALUE *ir = (IVALUE *)inst;
        free(ir->value);
        break;
      }
      case OPCODE_INOTCHARMAP: {
        INOTCHARMAP *ir = (INOTCHARMAP *)inst;
        free(ir->set);
        break;
      }
      case OPCODE_INOTSTRING: {
        INOTSTRING *ir = (INOTSTRING *)inst;
        free(ir->str);
        break;
      }
      case OPCODE_IOPTIONALCHARMAP: {
        IOPTIONALCHARMAP *ir = (IOPTIONALCHARMAP *)inst;
        free(ir->set);
        break;
      }
      case OPCODE_IOPTIONALSTRING: {
        IOPTIONALSTRING *ir = (IOPTIONALSTRING *)inst;
        free(ir->str);
        break;
      }
      case OPCODE_IZEROMORECHARMAP: {
        IZEROMORECHARMAP *ir = (IZEROMORECHARMAP *)inst;
        free(ir->set);
        break;
      }
    }
  }
  free(inst);
}