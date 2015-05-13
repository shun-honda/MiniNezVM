#ifndef NEZVM_H
#define NEZVM_H

#define NEZVM_DEBUG 0
#define NEZVM_PROFILE 0

typedef struct nezvm_string {
  unsigned len;
  char text[1];
} *nezvm_string_ptr_t;

#define NEZ_IR_MAX 11
#define NEZ_IR_EACH(OP)\
	OP(EXIT)\
	OP(SUCC)\
	OP(FAIL)\
	OP(JUMP)\
	OP(CALL)\
	OP(RET)\
	OP(IFFAIL)\
	OP(CHAR)\
	OP(PUSH)\
	OP(POP)\
	OP(PEEK)

typedef struct NezVMInstruction {
	short op : 5;
	short arg : 11;
} NezVMInstruction;

enum nezvm_opcode {
#define DEFINE_ENUM(NAME) NEZVM_OP_##NAME,
  NEZ_IR_EACH(DEFINE_ENUM)
#undef DEFINE_ENUM
  NEZVM_OP_ERROR = -1
};

union StackEntry {
  const char* pos;
  long jmp;
};

struct ParsingContext {
  char *inputs;
  size_t input_size;
  long pos;

  long bytecode_length;
  long startPoint;

  int* call_table;

  size_t stack_size;
  union StackEntry* stack_pointer;
  union StackEntry* stack_pointer_base;
};

typedef struct ParsingContext *ParsingContext;
typedef union StackEntry* StackEntry;

void nez_PrintErrorInfo(const char *errmsg);

NezVMInstruction *nez_LoadMachineCode(ParsingContext context,
                                      const char *fileName,
                                      const char *nonTerminalName);
void nez_DisposeInstruction(NezVMInstruction *inst, long length);

void nez_Parse(ParsingContext context, NezVMInstruction *inst);
void nez_ParseStat(ParsingContext context, NezVMInstruction *inst);

#define PARSING_CONTEXT_MAX_STACK_LENGTH 1024
ParsingContext nez_CreateParsingContext(const char *filename);
void nez_DisposeParsingContext(ParsingContext ctx);

#endif
