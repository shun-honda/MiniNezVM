#include <stdint.h>
#include "libnez.h"
#include "bitset.c"

#ifndef NEZVM_H
#define NEZVM_H

#define NEZVM_DEBUG 0
#define NEZVM_PROFILE 0

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

typedef union NezVMInstruction NezVMInstruction;
typedef bitset_t *bitset_ptr_t;

typedef struct byteCodeInfo {
  int pos;
  uint8_t version0;
  uint8_t version1;
  size_t memoTableSize;
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

typedef struct nezvm_string {
  unsigned len;
  char text[1];
} *nezvm_string_ptr_t;

typedef struct inst_table_t {
  NezVMInstruction *table[256];
} *inst_table_t;

typedef struct int_table_t {
  int table[256];
} *int_table_t;

typedef struct short_table_t {
  short table[256];
} *short_table_t;

typedef struct char_table_t {
  char table[256];
} *char_table_t;

typedef union NezVMInstructionBase {
  int opcode;
  const void *addr;
} NezVMInstructionBase;

#define OPCODE_IEXIT 0
typedef struct IEXIT {
	NezVMInstructionBase base;
} IEXIT;

#define OPCODE_IJUMP 1
typedef struct IJUMP {
	NezVMInstructionBase base;
	NezVMInstruction *jump;
} IJUMP;

#define OPCODE_ICALL 2
typedef struct ICALL {
	NezVMInstructionBase base;
	NezVMInstruction *jump;
} ICALL;

#define OPCODE_IRET 3
typedef struct IRET {
	NezVMInstructionBase base;
} IRET;

#define OPCODE_IIFFAIL 4
typedef struct IIFFAIL {
	NezVMInstructionBase base;
	NezVMInstruction *jump;
} IIFFAIL;

#define OPCODE_IIFSUCC 5
typedef struct IIFSUCC {
	NezVMInstructionBase base;
	NezVMInstruction *jump;
} IIFSUCC;

#define OPCODE_ICHAR 6
typedef struct ICHAR {
	NezVMInstructionBase base;
	char c;
	NezVMInstruction *jump;
} ICHAR;

#define OPCODE_ICHARMAP 7
typedef struct ICHARMAP {
	NezVMInstructionBase base;
	bitset_ptr_t set;
	NezVMInstruction *jump;
} ICHARMAP;

#define OPCODE_ISTRING 8
typedef struct ISTRING {
	NezVMInstructionBase base;
	nezvm_string_ptr_t str;
	NezVMInstruction *jump;
} ISTRING;

#define OPCODE_IANY 9
typedef struct IANY {
	NezVMInstructionBase base;
	NezVMInstruction *jump;
} IANY;

#define OPCODE_IPUSHpos 10
typedef struct IPUSHpos {
	NezVMInstructionBase base;
} IPUSHpos;

#define OPCODE_IPUSHmark 11
typedef struct IPUSHmark {
	NezVMInstructionBase base;
} IPUSHmark;

#define OPCODE_IPOPpos 12
typedef struct IPOPpos {
	NezVMInstructionBase base;
} IPOPpos;

#define OPCODE_ISTOREpos 13
typedef struct ISTOREpos {
	NezVMInstructionBase base;
} ISTOREpos;

#define OPCODE_ISTOREflag 14
typedef struct ISTOREflag {
	NezVMInstructionBase base;
	int val;
} ISTOREflag;

#define OPCODE_INEW 15
typedef struct INEW {
	NezVMInstructionBase base;
} INEW;

#define OPCODE_ILEFTJOIN 16
typedef struct ILEFTJOIN {
	NezVMInstructionBase base;
	int index;
} ILEFTJOIN;

#define OPCODE_ICAPTURE 17
typedef struct ICAPTURE {
	NezVMInstructionBase base;
} ICAPTURE;

#define OPCODE_ICOMMIT 18
typedef struct ICOMMIT {
	NezVMInstructionBase base;
	int index;
} ICOMMIT;

#define OPCODE_IABORT 19
typedef struct IABORT {
	NezVMInstructionBase base;
} IABORT;

#define OPCODE_ITAG 20
typedef struct ITAG {
	NezVMInstructionBase base;
	nezvm_string_ptr_t tag;
} ITAG;

#define OPCODE_IVALUE 21
typedef struct IVALUE {
	NezVMInstructionBase base;
	nezvm_string_ptr_t value;
} IVALUE;

#define OPCODE_IMEMOIZE 22
typedef struct IMEMOIZE {
	NezVMInstructionBase base;
	int memoPoint;
} IMEMOIZE;

#define OPCODE_ILOOKUP 23
typedef struct ILOOKUP {
	NezVMInstructionBase base;
	int memoPoint;
	NezVMInstruction *jump;
} ILOOKUP;

#define OPCODE_IMEMOIZENODE 24
typedef struct IMEMOIZENODE {
	NezVMInstructionBase base;
	int memoPoint;
} IMEMOIZENODE;

#define OPCODE_ILOOKUPNODE 25
typedef struct ILOOKUPNODE {
	NezVMInstructionBase base;
	int memoPoint;
	int index;
	NezVMInstruction *jump;
} ILOOKUPNODE;

#define OPCODE_INOTCHAR 26
typedef struct INOTCHAR {
	NezVMInstructionBase base;
	char c;
	NezVMInstruction *jump;
} INOTCHAR;

#define OPCODE_INOTCHARMAP 27
typedef struct INOTCHARMAP {
	NezVMInstructionBase base;
	bitset_ptr_t set;
	NezVMInstruction *jump;
} INOTCHARMAP;

#define OPCODE_INOTSTRING 28
typedef struct INOTSTRING {
	NezVMInstructionBase base;
	nezvm_string_ptr_t str;
	NezVMInstruction *jump;
} INOTSTRING;

#define OPCODE_INOTCHARANY 29
typedef struct INOTCHARANY {
	NezVMInstructionBase base;
	char c;
	NezVMInstruction *jump;
} INOTCHARANY;

#define OPCODE_IOPTIONALCHAR 30
typedef struct IOPTIONALCHAR {
	NezVMInstructionBase base;
	char c;
	NezVMInstruction *jump;
} IOPTIONALCHAR;

#define OPCODE_IOPTIONALCHARMAP 31
typedef struct IOPTIONALCHARMAP {
	NezVMInstructionBase base;
	bitset_ptr_t set;
	NezVMInstruction *jump;
} IOPTIONALCHARMAP;

#define OPCODE_IOPTIONALSTRING 32
typedef struct IOPTIONALSTRING {
	NezVMInstructionBase base;
	nezvm_string_ptr_t str;
	NezVMInstruction *jump;
} IOPTIONALSTRING;

#define OPCODE_IZEROMORECHARMAP 33
typedef struct IZEROMORECHARMAP {
	NezVMInstructionBase base;
	bitset_ptr_t set;
	NezVMInstruction *jump;
} IZEROMORECHARMAP;

#define NEZ_IR_MAX 34
#define NEZ_IR_EACH(OP)\
	OP(EXIT)\
	OP(JUMP)\
	OP(CALL)\
	OP(RET)\
	OP(IFFAIL)\
	OP(IFSUCC)\
	OP(CHAR)\
	OP(CHARMAP)\
	OP(STRING)\
	OP(ANY)\
	OP(PUSHpos)\
	OP(PUSHmark)\
	OP(POPpos)\
	OP(STOREpos)\
	OP(STOREflag)\
	OP(NEW)\
	OP(LEFTJOIN)\
	OP(CAPTURE)\
	OP(COMMIT)\
	OP(ABORT)\
	OP(TAG)\
	OP(VALUE)\
	OP(MEMOIZE)\
	OP(LOOKUP)\
	OP(MEMOIZENODE)\
	OP(LOOKUPNODE)\
	OP(NOTCHAR)\
	OP(NOTCHARMAP)\
	OP(NOTSTRING)\
	OP(NOTCHARANY)\
	OP(OPTIONALCHAR)\
	OP(OPTIONALCHARMAP)\
	OP(OPTIONALSTRING)\
	OP(ZEROMORECHARMAP)

union NezVMInstruction {
#define DEF_NEZVM_INST_UNION(OP) I##OP _##OP;
  NEZ_IR_EACH(DEF_NEZVM_INST_UNION);
#undef DEF_NEZVM_INST_UNION
  NezVMInstructionBase base;
};

enum nezvm_opcode {
#define DEFINE_ENUM(NAME) NEZVM_OP_##NAME,
  NEZ_IR_EACH(DEFINE_ENUM)
#undef DEFINE_ENUM
  NEZVM_OP_ERROR = -1
};

void nez_PrintErrorInfo(const char *errmsg);

/* loader's functions */
NezVMInstruction *nez_LoadMachineCode(ParsingContext context,
                                      const char *fileName,
                                      const char *nonTerminalName);
void nez_DisposeInstruction(NezVMInstruction *inst, long length);

ParsingObject nez_Parse(ParsingContext context, NezVMInstruction *inst);
void nez_ParseStat(ParsingContext context, NezVMInstruction *inst);
void nez_Match(ParsingContext context, NezVMInstruction *inst);

#endif
