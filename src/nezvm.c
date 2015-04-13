#include <stdio.h>
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

#define PUSH_IP(PC) *cp++ = (PC)
#define POP_IP() --cp
#define SP_TOP(INST) (*sp)
#define PUSH_SP(INST) (*sp++ = (INST))
#define POP_SP(INST) (*--sp)

#define GET_ADDR(PC) ((PC)->base).addr
#define DISPATCH_NEXT goto *GET_ADDR(++pc)
#define JUMP(dst) goto *GET_ADDR(pc = dst)
#define JUMP_REL(dst) goto *GET_ADDR(pc += dst)
#define RET goto *GET_ADDR(pc = *POP_IP())

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
  ParsingObject *osp;
  const NezVMInstruction **cp;
  const char **sp;
  pc = inst + context->startPoint;
  sp = (const char **)context->stack_pointer;
  cp = (const NezVMInstruction **)context->call_stack_pointer;
  int num = 0;

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
    NezVMInstruction *dst = ((IJUMP *)pc)->jump;
    JUMP(dst);
  }
  OP(CALL) {
    NezVMInstruction *dst = ((ICALL *)pc)->jump;
    PUSH_IP(pc + 1);
    JUMP(dst);
  }
  OP(RET) {
    RET;
  }
  OP(IFFAIL) {
    NezVMInstruction *dst = ((IIFFAIL *)pc)->jump;
    if (failflag) {
      JUMP(dst);
    } else {
      DISPATCH_NEXT;
    }
  }
  OP(IFSUCC) {
    NezVMInstruction *dst = ((IIFSUCC *)pc)->jump;
    if (failflag == 0) {
      JUMP(dst);
    } else {
      DISPATCH_NEXT;
    }
  }
  OP(CHAR) {
    char ch = *cur++;
    ICHAR *ir = (ICHAR *)pc;
    if (ir->c == ch) {
      DISPATCH_NEXT;
    } else {
      --cur;
      failflag = 1;
      JUMP(ir->jump);
    }
  }
  OP(CHARMAP) {
    ICHARMAP *ir = (ICHARMAP *)pc;
    if (bitset_get(ir->set, *cur++)) {
      DISPATCH_NEXT;
    } else {
      --cur;
      failflag = 1;
      JUMP(ir->jump);
    }
  }
  OP(STRING) {
    ISTRING *ir = (ISTRING *)pc;
    int next;
    if ((next = nezvm_string_equal(ir->str, cur)) > 0) {
      cur += next;
      DISPATCH_NEXT;
    } else {
      failflag = 1;
      JUMP(ir->jump);
    }
  }
  OP(ANY) {
    IANY *ir = (IANY *)pc;
    if (*cur++ != 0) {
      DISPATCH_NEXT;
    } else {
      --cur;
      failflag = 1;
      JUMP(ir->jump);
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
    ISTOREflag *ir = (ISTOREflag *)pc;
    failflag = ir->val;
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
    ICOMMIT *ir = (ICOMMIT *)pc;
    LEFT = nez_commitLog(context, (int)POP_SP());
    nez_pushDataLog(context, LazyLink_T, 0, ir->index, NULL, LEFT);
    DISPATCH_NEXT;
  }
  OP(ABORT) {
    nez_abortLog(context, (int)POP_SP());
    DISPATCH_NEXT;
  }
  OP(TAG) {
    ITAG *ir = (ITAG *)pc;
    nez_pushDataLog(context, LazyTag_T, 0, 0, (const char*)&ir->tag->text, NULL);
    DISPATCH_NEXT;
  }
  OP(VALUE) {
    IVALUE *ir = (IVALUE *)pc;
    nez_pushDataLog(context, LazyValue_T, 0, 0, (const char*)&ir->value->text, NULL);
    DISPATCH_NEXT;
  }
  OP(MEMOIZE) {
    IMEMOIZE *ir = (IMEMOIZE *)pc;
    int mp = ir->memoPoint;
    int len = cur - POP_SP();
    nez_setMemo(context, cur, mp, len, NULL);
    DISPATCH_NEXT;
  }
  OP(LOOKUP) {
    ILOOKUP *ir = (ILOOKUP *)pc;
    int mp = ir->memoPoint;
    MemoEntry m = nez_getMemo(context, cur, mp);
    if(m != NULL) {
      if(!m->consumed) {
        JUMP(ir->jump);
      }
      cur += m->consumed;
    }
    DISPATCH_NEXT;
  }
  OP(MEMOIZENODE) {
    IMEMOIZENODE *ir = (IMEMOIZENODE *)pc;
    int mp = ir->memoPoint;
    int len = cur - POP_SP();
    nez_setMemo(context, cur, mp, len, LEFT);
    DISPATCH_NEXT;
  }
  OP(LOOKUPNODE) {
    ILOOKUPNODE *ir = (ILOOKUPNODE *)pc;
    int mp = ir->memoPoint;
    MemoEntry m = nez_getMemo(context, cur, mp);
    if(m != NULL) {
      if(!m->consumed) {
        JUMP(ir->jump);
      }
      cur += m->consumed;
      nez_pushDataLog(context, LazyLink_T, 0, ir->index, NULL, m->result);
    }
    DISPATCH_NEXT;
  }
  OP(NOTCHAR) {
    INOTCHAR *ir = (INOTCHAR *)pc;
    if (*cur == ir->c) {
      failflag = 1;
      JUMP(ir->jump);
    }
    DISPATCH_NEXT;
  }
  OP(NOTCHARMAP) {
    INOTCHARMAP *ir = (INOTCHARMAP *)pc;
    if (bitset_get(ir->set, *cur)) {
      failflag = 1;
      JUMP(ir->jump);
    }
    DISPATCH_NEXT;
  }
  OP(NOTSTRING) {
    INOTSTRING *ir = (INOTSTRING *)pc;
    if (nezvm_string_equal(ir->str, cur) > 0) {
      failflag = 1;
      JUMP(ir->jump);
    }
    DISPATCH_NEXT;
  }
  OP(NOTCHARANY) {
    INOTCHARANY *ir = (INOTCHARANY *)pc;
    if (*cur++ == ir->c) {
      --cur;
      failflag = 1;
      JUMP(ir->jump);
    }
    DISPATCH_NEXT;
  }
  OP(OPTIONALCHAR) {
    IOPTIONALCHAR *ir = (IOPTIONALCHAR *)pc;
    if (*cur == ir->c) {
      ++cur;
    }
    DISPATCH_NEXT;
  }
  OP(OPTIONALCHARMAP) {
    IOPTIONALCHARMAP *ir = (IOPTIONALCHARMAP *)pc;
    if (bitset_get(ir->set, *cur)) {
      ++cur;
    }
    DISPATCH_NEXT;
  }
  OP(OPTIONALSTRING) {
    IOPTIONALSTRING *ir = (IOPTIONALSTRING *)pc;
    cur += nezvm_string_equal(ir->str, cur);
    DISPATCH_NEXT;
  }
  OP(ZEROMORECHARMAP) {
    IZEROMORECHARMAP *ir = (IZEROMORECHARMAP *)pc;
  L_head:
    ;
    if (bitset_get(ir->set, *cur)) {
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
    NezVMInstructionBase *pc = (NezVMInstructionBase *)ip;
    pc->addr = (const void *)table[pc->opcode];
    ++ip;
  }
  return inst;
}
