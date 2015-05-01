#include <stdio.h>
#include <sys/time.h> // gettimeofday
#include "libnez.h"
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

static int nezvm_string_equal(nezvm_string_ptr_t str, const char *t) {
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

static inline void PUSH_IP(ParsingContext ctx, const NezVMInstruction *inst) {
  (ctx->stack_pointer++)->func = inst;
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

#define GET_ADDR(PC) ((PC)->addr)
#define DISPATCH_NEXT goto *GET_ADDR(++pc)
#define JUMP(dst) goto *GET_ADDR(pc = dst)
#define RET goto *GET_ADDR(pc = (POP_SP(context))->func)

#define OP(OP) NEZVM_OP_##OP:

long nez_VM_Execute(ParsingContext context, NezVMInstruction *inst) {
  static const void *table[] = {
#define DEFINE_TABLE(NAME) &&NEZVM_OP_##NAME,
    NEZ_IR_EACH(DEFINE_TABLE)
#undef DEFINE_TABLE
  };

  register const char *cur = context->inputs + context->pos;
  register int failflag = 0;
  register const NezVMInstruction *pc;
  pc = inst + 1;
  fprintf(stderr, "%zd\n", sizeof(const char*));

  if (inst == NULL) {
    return (long)table;
  }

  PUSH_IP(context, inst);

  goto *GET_ADDR(pc);

  OP(EXIT) {
    context->pos = cur - context->inputs;
    return failflag;
  }
  OP(JUMP) {
    NezVMInstruction *dst = pc->arg0.jump;
    JUMP(dst);
  }
  OP(CALL) {
    NezVMInstruction *dst = pc->arg0.jump;
    PUSH_IP(context, pc + 1);
    JUMP(dst);
  }
  OP(RET) {
    RET;
  }
  OP(IFFAIL) {
    NezVMInstruction *dst = pc->arg0.jump;
    if (failflag) {
      JUMP(dst);
    } else {
      DISPATCH_NEXT;
    }
  }
  OP(IFSUCC) {
    NezVMInstruction *dst = pc->arg0.jump;
    if (failflag == 0) {
      JUMP(dst);
    } else {
      DISPATCH_NEXT;
    }
  }
  OP(CHAR) {
    char ch = *cur++;
    if (pc->arg0.c == ch) {
      DISPATCH_NEXT;
    } else {
      --cur;
      failflag = 1;
      JUMP(pc->arg1.jump);
    }
  }
  OP(CHARMAP) {
    if (bitset_get(pc->arg0.set, *cur++)) {
      DISPATCH_NEXT;
    } else {
      --cur;
      failflag = 1;
      JUMP(pc->arg1.jump);
    }
  }
  OP(STRING) {
    int next;
    if ((next = nezvm_string_equal(pc->arg0.str, cur)) > 0) {
      cur += next;
      DISPATCH_NEXT;
    } else {
      failflag = 1;
      JUMP(pc->arg1.jump);
    }
  }
  OP(ANY) {
    if (*cur++ != 0) {
      DISPATCH_NEXT;
    } else {
      --cur;
      failflag = 1;
      JUMP(pc->arg0.jump);
    }
  }
  OP(PUSHpos) {
    PUSH_SP(context, cur);
    DISPATCH_NEXT;
  }
  OP(POPpos) {
    (void)POP_SP(context);
    DISPATCH_NEXT;
  }
  OP(GETpos) {
    cur = (context->stack_pointer-1)->pos;
    DISPATCH_NEXT;
  }
  OP(STOREpos) {
    cur = POP_SP(context)->pos;
    DISPATCH_NEXT;
  }
  OP(STOREflag) {
    failflag = pc->arg0.val;
    DISPATCH_NEXT;
  }
  OP(NOTCHAR) {
    if (*cur == pc->arg0.c) {
      failflag = 1;
      JUMP(pc->arg1.jump);
    }
    DISPATCH_NEXT;
  }
  OP(NOTCHARMAP) {
    if (bitset_get(pc->arg0.set, *cur)) {
      failflag = 1;
      JUMP(pc->arg1.jump);
    }
    DISPATCH_NEXT;
  }
  OP(NOTSTRING) {
    if (nezvm_string_equal(pc->arg0.str, cur) > 0) {
      failflag = 1;
      JUMP(pc->arg1.jump);
    }
    DISPATCH_NEXT;
  }
  OP(NOTCHARANY) {
    if (*cur++ == pc->arg0.c) {
      --cur;
      failflag = 1;
      JUMP(pc->arg1.jump);
    }
    DISPATCH_NEXT;
  }
  OP(OPTIONALCHAR) {
    if (*cur == pc->arg0.c) {
      ++cur;
    }
    DISPATCH_NEXT;
  }
  OP(OPTIONALCHARMAP) {
    if (bitset_get(pc->arg0.set, *cur)) {
      ++cur;
    }
    DISPATCH_NEXT;
  }
  OP(OPTIONALSTRING) {
    cur += nezvm_string_equal(pc->arg0.str, cur);
    DISPATCH_NEXT;
  }
  OP(ZEROMORECHARMAP) {
  L_head:
    ;
    if (bitset_get(pc->arg0.set, *cur)) {
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

NezVMInstruction *nez_VM_Prepare(ParsingContext context,
                                        NezVMInstruction *inst) {
  long i;
  const void **table = (const void **)nez_VM_Execute(context, NULL);
  NezVMInstruction *ip = inst;
  for (i = 0; i < context->bytecode_length; i++) {
    NezVMInstruction *pc = (NezVMInstruction *)ip;
    pc->addr = (const void *)table[pc->opcode];
    ++ip;
  }
  return inst;
}
