#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h> // gettimeofday
#include "nezvm.h"

void nez_PrintErrorInfo(const char *errmsg) {
  fprintf(stderr, "%s\n", errmsg);
  exit(EXIT_FAILURE);
}

static uint64_t timer() {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

static int nezvm_string_equal(struct nezvm_string* str, const char *t) {
  int len = str->len;
  const char *p = str->text;
  const char *end = p + len;
#if 0
  return (strncmp(p, t, len) == 0) ? len : 0;
#else
  while (p < end) {
    if (*p++ != *t++) {
      return 0;
    }
  }
  return len;
#endif
}

static inline void PUSH_IP(ParsingContext ctx, long jmp) {
  (ctx->stack_pointer++)->jmp = jmp;
  if(ctx->stack_pointer >= &ctx->stack_pointer_base[ctx->stack_size]) {
    nez_PrintErrorInfo("Error:stack over flow");
  }
}

static inline void PUSH_SP(ParsingContext ctx, const char* pos) {
  (ctx->stack_pointer++)->pos = pos;
  if(ctx->stack_pointer >= &ctx->stack_pointer_base[ctx->stack_size]) {
    nez_PrintErrorInfo("Error:stack over flow");
  }
}

static inline StackEntry POP_SP(ParsingContext ctx) {
  --ctx->stack_pointer;
  if(ctx->stack_pointer < ctx->stack_pointer_base) {
    nez_PrintErrorInfo("Error:POP_SP");
  }
  return ctx->stack_pointer;
}

// #if __GNUC__ >= 3
// #define likely(x) __builtin_expect(!!(x), 1)
// #define unlikely(x) __builtin_expect(!!(x), 0)
// #else
// #define likely(x) (x)
// #define unlikely(x) (x)
// #endif

//#define PUSH_IP(PC) (sp++)->func = (PC)
//#define POP_IP() --sp
//#define PUSH_SP(INST) ((sp++)->pos = (INST))
//#define POP_SP(INST) ((--sp)->pos)

#define NEXT_OP (OPJUMP[++pc->op])

#define GET_ADDR(PC) (OPJUMP[(PC)->op])
#define DISPATCH_NEXT goto *GET_ADDR(++pc)
#define JUMP(dst) goto *GET_ADDR(pc += dst)
#define RET goto *GET_ADDR(pc = inst + (POP_SP(context))->jmp)

#define OP(OP) NEZVM_OP_##OP: //fprintf(stderr, "[%d] %s\n", pc - inst, get_opname(pc->op));

long nez_VM_Execute(ParsingContext context, NezVMInstruction *inst) {
  static const void *OPJUMP[] = {
#define DEFINE_TABLE(NAME) &&NEZVM_OP_##NAME,
    NEZ_IR_EACH(DEFINE_TABLE)
#undef DEFINE_TABLE
  };

  register const char *cur = context->inputs + context->pos;
  register int failflag = 0;
  register const NezVMInstruction *pc;
  pc = inst + 1;
  // fprintf(stderr, "%zd\n", sizeof(const char*));

  PUSH_IP(context, 0);

  goto *GET_ADDR(pc);

  OP(EXIT) {
    context->pos = cur - context->inputs;
    return failflag;
  }
  OP(SUCC) {
    failflag = 0;
    DISPATCH_NEXT;
  }
  OP(FAIL) {
    failflag = 1;
    DISPATCH_NEXT;
  }
  OP(JUMP) {
    //fprintf(stderr, "%d\n", pc->arg);
    JUMP(pc->arg);
  }
  OP(CALL) {
    //fprintf(stderr, "[%ld] %c, jmp: %d\n", pc-inst, *cur, pc->arg);
    PUSH_IP(context, pc - inst + 1);
    JUMP(context->call_table[pc->arg]);
  }
  OP(RET) {
    RET;
  }
  OP(IFFAIL) {
    if (failflag) {
      JUMP(pc->arg);
    } else {
      DISPATCH_NEXT;
    }
  }
  OP(CHAR) {
    char ch = *cur++;
    if (pc->arg != ch) {
      --cur;
      failflag = 1;
    }
    DISPATCH_NEXT;
  }
  OP(CHARMAP) {
    if (!bitset_get(context->set_table[pc->arg].set, *cur++)) {
      --cur;
      failflag = 1;
      JUMP(context->set_table[pc->arg].jump);
    }
    DISPATCH_NEXT;
  }
  OP(STRING) {
    int next;
    if ((next = nezvm_string_equal(context->str_table[pc->arg].str, cur)) > 0) {
      cur += next;
    } else {
      failflag = 1;
      JUMP(context->str_table[pc->arg].jump);
    }
    DISPATCH_NEXT;
  }
  OP(ANY) {
    if (*cur++ == 0) {
      --cur;
      failflag = 1;
    }
    DISPATCH_NEXT;
  }
  OP(PUSH) {
    PUSH_SP(context, cur);
    DISPATCH_NEXT;
  }
  OP(POP) {
    (void)POP_SP(context);
    DISPATCH_NEXT;
  }
  OP(PEEK) {
    cur = (context->stack_pointer-1)->pos;
    DISPATCH_NEXT;
  }
  OP(STORE) {
    cur = POP_SP(context)->pos;
    DISPATCH_NEXT;
  }
  OP(NOTCHAR) {
    if (*cur == context->str_table[pc->arg].c) {
      failflag = 1;
      JUMP(context->str_table[pc->arg].jump);
    }
    DISPATCH_NEXT;
  }
  OP(NOTCHARMAP) {
    if (bitset_get(context->set_table[pc->arg].set, *cur)) {
      failflag = 1;
      JUMP(context->set_table[pc->arg].jump);
    }
    DISPATCH_NEXT;
  }
  OP(NOTSTRING) {
    if (nezvm_string_equal(context->str_table[pc->arg].str, cur) > 0) {
      failflag = 1;
      JUMP(context->str_table[pc->arg].jump);
    }
    DISPATCH_NEXT;
  }
  OP(OPTIONALCHAR) {
    if (*cur == pc->arg) {
      ++cur;
    }
    DISPATCH_NEXT;
  }
  OP(OPTIONALCHARMAP) {
    if (bitset_get(context->set_table[pc->arg].set, *cur)) {
      ++cur;
    }
    DISPATCH_NEXT;
  }
  OP(OPTIONALSTRING) {
    cur += nezvm_string_equal(context->str_table[pc->arg].str, cur);
    DISPATCH_NEXT;
  }
  OP(ZEROMORECHARMAP) {
  L_head:
    ;
    if (bitset_get(context->set_table[pc->arg].set, *cur)) {
      cur++;
      goto L_head;
    }
    DISPATCH_NEXT;
  }
  return -1;
}

// void dump_pego(ParsingObject *pego, char *source, int level);

void nez_Parse(ParsingContext context, NezVMInstruction *inst) {
  if (nez_VM_Execute(context, inst)) {
    nez_PrintErrorInfo("parse error");
  }
}

#define NEZVM_STAT 5
void nez_ParseStat(ParsingContext context, NezVMInstruction *inst) {
  for (int i = 0; i < NEZVM_STAT; i++) {
    uint64_t start, end;
    start = timer();
    if (nez_VM_Execute(context, inst)) {
      nez_PrintErrorInfo("parse error");
    }
    end = timer();
    fprintf(stderr, "ErapsedTime: %llu msec\n",
            (unsigned long long)end - start);
    context->pos = 0;
  }
  fprintf(stderr, "stack_size=%zd[Byte]\n", sizeof(union StackEntry) * context->stack_size);
}

char *loadFile(const char *filename, size_t *length);

ParsingContext nez_CreateParsingContext(const char *filename) {
  ParsingContext ctx = (ParsingContext)malloc(sizeof(struct ParsingContext));
  ctx->pos = ctx->input_size = 0;
  ctx->inputs = loadFile(filename, &ctx->input_size);
  ctx->stack_pointer_base =
      (StackEntry)malloc(sizeof(union StackEntry) * PARSING_CONTEXT_MAX_STACK_LENGTH);
  ctx->stack_pointer = &ctx->stack_pointer_base[0];
  ctx->stack_size = PARSING_CONTEXT_MAX_STACK_LENGTH;
  return ctx;
}

void nez_DisposeParsingContext(ParsingContext ctx) {
  free(ctx->inputs);
  free(ctx->stack_pointer_base);
  free(ctx);
}
