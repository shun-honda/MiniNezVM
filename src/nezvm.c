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

static ParsingObject nez_newObject2(ParsingContext context, const char *cur,
                                    MemoryPool mpool, ParsingObject left) {
  ParsingObject po = nez_newObject(context, cur);
#if 1
  *po = *left;
#else
  memcpy(po, left, sizeof(*po));
#endif
  return po;
}

#if __GNUC__ >= 3
#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)
#else
#define likely(x) (x)
#define unlikely(x) (x)
#endif

#define PUSH_IP(PC) (sp++)->func = (PC)
#define POP_IP() --sp
#define SP_TOP(INST) (*sp)
#define PUSH_SP(INST) ((sp++)->pos = (INST))
#define POP_SP(INST) ((--sp)->pos)

#define GET_ADDR(PC) ((PC)->addr)
#define DISPATCH_NEXT goto *GET_ADDR(++pc)
#define JUMP(dst) goto *GET_ADDR(pc = dst)
#define JUMP_REL(dst) goto *GET_ADDR(pc += dst)
#define RET goto *GET_ADDR(pc = (POP_IP())->func)

#define OP(OP) NEZVM_OP_##OP:

long nez_VM_Execute(ParsingContext context, NezVMInstruction *inst) {
  static const void *table[] = {
#define DEFINE_TABLE(NAME) &&NEZVM_OP_##NAME,
    NEZ_IR_EACH(DEFINE_TABLE)
#undef DEFINE_TABLE
  };

#if 1
#define LEFT left
  ParsingObject left = NULL;
#else
#define LEFT context->left
#endif
  register const char *cur = context->inputs + context->pos;
#if 0
#define MPOOL pool
  MemoryPool pool = context->mpool;
#else
#define MPOOL context->mpool
#endif

  // const char *Reg1 = 0;
  // const char *Reg2 = 0;
  // const char *Reg3 = 0;
  register int failflag = 0;
  register const NezVMInstruction *pc;
  StackEntry sp;
  pc = inst + context->startPoint;
  sp = context->stack_pointer;

  if (inst == NULL) {
    return (long)table;
  }

  PUSH_IP(inst);
  LEFT = nez_setObject_(context, LEFT, nez_newObject(context, cur));

  goto *GET_ADDR(pc);

  OP(EXIT) {
    context->left = nez_commitLog(context, 0);
    context->pos = cur - context->inputs;
    return failflag;
  }
  OP(JUMP) {
    NezVMInstruction *dst = pc->arg0.jump;
    JUMP(dst);
  }
  OP(CALL) {
    NezVMInstruction *dst = pc->arg0.jump;
    PUSH_IP(pc + 1);
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
    PUSH_SP(cur);
    DISPATCH_NEXT;
  }
  OP(PUSHmark) {
    PUSH_SP((const char *)(long)nez_markLogStack(context));
    DISPATCH_NEXT;
  }
  OP(POPpos) {
    (void)POP_SP();
    DISPATCH_NEXT;
  }
  OP(STOREpos) {
    cur = POP_SP();
    DISPATCH_NEXT;
  }
  OP(STOREflag) {
    failflag = pc->arg0.val;
    DISPATCH_NEXT;
  }
  OP(NEW) {
    PUSH_SP((const char *)(long)nez_markLogStack(context));
    nez_pushDataLog(context, LazyNew_T, cur - context->inputs, -1, NULL, NULL);
    DISPATCH_NEXT;
  }
  OP(LEFTJOIN) {
    //PUSH_SP((const char *)(long)nez_markLogStack(context));
    nez_pushDataLog(context, LazyLeftJoin_T, cur - context->inputs, 0, NULL, NULL);
    DISPATCH_NEXT;
  }
  OP(CAPTURE) {
    nez_pushDataLog(context, LazyCapture_T, cur - context->inputs, 0, NULL, NULL);
    DISPATCH_NEXT;
  }
  OP(COMMIT) {
    ParsingObject po = nez_commitLog(context, (int)POP_SP());
    nez_pushDataLog(context, LazyLink_T, 0, pc->arg0.val, NULL, po);
    DISPATCH_NEXT;
  }
  OP(ABORT) {
    nez_abortLog(context, (int)POP_SP());
    DISPATCH_NEXT;
  }
  OP(TAG) {
    nez_pushDataLog(context, LazyTag_T, 0, 0, (const char*)&pc->arg0.str->text, NULL);
    DISPATCH_NEXT;
  }
  OP(VALUE) {
    nez_pushDataLog(context, LazyValue_T, 0, 0, (const char*)&pc->arg0.str->text, NULL);
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

void dump_pego(ParsingObject *pego, char *source, int level);

ParsingObject nez_Parse(ParsingContext context, NezVMInstruction *inst) {
  if (nez_VM_Execute(context, inst)) {
    nez_PrintErrorInfo("parse error");
  }
  dump_pego(&context->left, context->inputs, 0);
  return context->left;
}

#define NEZVM_STAT 5
void nez_ParseStat(ParsingContext context, NezVMInstruction *inst) {
  for (int i = 0; i < NEZVM_STAT; i++) {
    uint64_t start, end;
    MemoryPool_Reset(context->mpool);
    start = timer();
    if (nez_VM_Execute(context, inst)) {
      nez_PrintErrorInfo("parse error");
    }
    end = timer();
    fprintf(stderr, "ErapsedTime: %llu msec\n",
            (unsigned long long)end - start);
    nez_DisposeObject(context->left);
    context->unusedObject = NULL;
    context->unusedLog = NULL;
    context->pos = 0;
  }
}

void nez_Match(ParsingContext context, NezVMInstruction *inst) {
  uint64_t start, end;
  start = timer();
  if (nez_VM_Execute(context, inst)) {
    nez_PrintErrorInfo("parse error");
  }
  end = timer();
  fprintf(stderr, "ErapsedTime: %llu msec\n", (unsigned long long)end - start);
  fprintf(stdout, "match\n\n");
  nez_DisposeObject(context->left);
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
