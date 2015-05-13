#include <stdint.h>

#ifndef NEZVM_H
#define NEZVM_H

#define NEZVM_DEBUG 0
#define NEZVM_PROFILE 0

#define NEZ_IR_MAX 22
#define NEZ_IR_EACH(OP)\
	OP(EXIT)\
	OP(SUCC)\
	OP(FAIL)\
	OP(JUMP)\
	OP(CALL)\
	OP(RET)\
	OP(IFFAIL)\
	OP(CHAR)\
	OP(CHARMAP)\
	OP(STRING)\
	OP(ANY)\
	OP(PUSH)\
	OP(POP)\
	OP(PEEK)\
	OP(STORE)\
	OP(NOTCHAR)\
	OP(NOTCHARMAP)\
	OP(NOTSTRING)\
	OP(OPTIONALCHAR)\
	OP(OPTIONALCHARMAP)\
	OP(OPTIONALSTRING)\
	OP(ZEROMORECHARMAP)

typedef struct NezVMInstruction {
	unsigned short op : 5;
	short arg : 11;
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

void nez_Parse(ParsingContext context, NezVMInstruction *inst);
void nez_ParseStat(ParsingContext context, NezVMInstruction *inst);

#endif
