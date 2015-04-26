#include <stdint.h>
#include "bitset.c"

#ifndef NEZVM_H
#define NEZVM_H

#define NEZVM_DEBUG 0
#define NEZVM_PROFILE 0

typedef bitset_t *bitset_ptr_t;

typedef struct nezvm_string {
  unsigned len;
  char text[1];
} *nezvm_string_ptr_t;

#define NEZ_IR_MAX 30
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
	OP(GETpos)\
	OP(STOREpos)\
	OP(STOREflag)\
	OP(NEW)\
	OP(LEFTJOIN)\
	OP(CAPTURE)\
	OP(COMMIT)\
	OP(ABORT)\
	OP(TAG)\
	OP(VALUE)\
	OP(NOTCHAR)\
	OP(NOTCHARMAP)\
	OP(NOTSTRING)\
	OP(NOTCHARANY)\
	OP(OPTIONALCHAR)\
	OP(OPTIONALCHARMAP)\
	OP(OPTIONALSTRING)\
	OP(ZEROMORECHARMAP)

typedef union value_t {
	char c;
	int val;
	nezvm_string_ptr_t str;
	bitset_ptr_t set;
	struct NezVMInstruction *jump;
} value_t;

typedef struct NezVMInstruction {
	union {
		int opcode;
		const void *addr;
	};
	value_t arg0;
	value_t arg1;
} NezVMInstruction;

enum nezvm_opcode {
#define DEFINE_ENUM(NAME) NEZVM_OP_##NAME,
  NEZ_IR_EACH(DEFINE_ENUM)
#undef DEFINE_ENUM
  NEZVM_OP_ERROR = -1
};

void nez_PrintErrorInfo(const char *errmsg);

NezVMInstruction *nez_LoadMachineCode(ParsingContext context,
                                      const char *fileName,
                                      const char *nonTerminalName);
void nez_DisposeInstruction(NezVMInstruction *inst, long length);

ParsingObject nez_Parse(ParsingContext context, NezVMInstruction *inst);
void nez_ParseStat(ParsingContext context, NezVMInstruction *inst);
void nez_Match(ParsingContext context, NezVMInstruction *inst);

#endif
