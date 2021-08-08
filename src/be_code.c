/********************************************************************
** Copyright (c) 2018-2020 Guan Wenliang
** This file is part of the Berry default interpreter.
** skiars@qq.com, https://github.com/Skiars/berry
** See Copyright Notice in the LICENSE file or at
** https://github.com/Skiars/berry/blob/master/LICENSE
********************************************************************/
#include "be_code.h"
#include "be_decoder.h"
#include "be_parser.h"
#include "be_lexer.h"
#include "be_vector.h"
#include "be_list.h"
#include "be_var.h"
#include "be_exec.h"
#include "be_vm.h"

#define NOT_MASK                (1 << 0)
#define NOT_EXPR                (1 << 1)
#define FUNC_RET_FLAG           (1 << 0)

#define isset(v, mask)          (((v) & (mask)) != 0)
#define min(a, b)               ((a) < (b) ? (a) : (b))
#define notexpr(e)              isset((e)->not, NOT_EXPR)
#define notmask(e)              isset((e)->not, NOT_MASK)
#define exp2anyreg(f, e)        exp2reg(f, e, (f)->freereg)
#define var2anyreg(f, e)        var2reg(f, e, (f)->freereg)
#define hasjump(e)              ((e)->t != (e)->f || notexpr(e))
#define code_bool(f, r, b, j)   codeABC(f, OP_LDBOOL, r, b, j)
#define code_call(f, a, b)      codeABC(f, OP_CALL, a, b, 0)
#define code_getmbr(f, a, b, c) codeABC(f, OP_GETMBR, a, b, c)
#define jumpboolop(e, b)        ((b) != notmask(e) ? OP_JMPT : OP_JMPF)

#if BE_USE_SCRIPT_COMPILER

static int var2reg(bfuncinfo *finfo, bexpdesc *e, int dst);

#if BE_DEBUG_RUNTIME_INFO
static void codelineinfo(bfuncinfo *finfo)
{
    bvector *vec = &finfo->linevec;
    int line = finfo->lexer->lastline;
    blineinfo *li = be_vector_end(vec);
    if (be_vector_isempty(vec) || li->linenumber != line) {
        be_vector_push(finfo->lexer->vm, vec, NULL);
        li = be_vector_end(vec);
        li->endpc = finfo->pc;
        li->linenumber = line;
        finfo->proto->lineinfo = be_vector_data(vec);
        finfo->proto->nlineinfo = be_vector_capacity(vec);
    } else {
        li->endpc = finfo->pc;
    }
}
#else
    #define codelineinfo(finfo)
#endif

/* Add new instruction in the code vector */
static int codeinst(bfuncinfo *finfo, binstruction ins)
{
    /* put new instruction in code array */
    be_vector_push_c(finfo->lexer->vm, &finfo->code, &ins);
    finfo->proto->code = be_vector_data(&finfo->code);
    finfo->proto->codesize = be_vector_capacity(&finfo->code);
    codelineinfo(finfo);
    return finfo->pc++;
}

static int codeABC(bfuncinfo *finfo, bopcode op, int a, int b, int c)
{
    return codeinst(finfo, ISET_OP(op)
        | ISET_RA(a) | ISET_RKB(b) | ISET_RKC(c));
}

static int codeABx(bfuncinfo *finfo, bopcode op, int a, int bx)
{
    return codeinst(finfo, ISET_OP(op) | ISET_RA(a) | ISET_Bx(bx));
}

/* Move value from register b to register a */
/* Check the previous instruction to compact both instruction as one if possible */
/* TODO make sure we don´t miss the update of the other register if optimized, add a comparison? */
/* If b is a constant, add LDCONST or add MOVE otherwise */
static void code_move(bfuncinfo *finfo, int a, int b)
{
    if (finfo->pc) {  /* If not the first instruction of the function */
        binstruction *i = be_vector_end(&finfo->code);  /* get the last instruction */
        bopcode op = IGET_OP(*i);
        if (op <= OP_LDNIL) { /* binop or unop */
            /* remove redundant MOVE instruction */
            int x = IGET_RA(*i), y = IGET_RKB(*i), z = IGET_RKC(*i);
            if (b == x && (a == y || (op < OP_NEG && a == z))) {
                *i = (*i & ~IRA_MASK) | ISET_RA(a);
                return;
            }
        }
    }
    if (isK(b)) {
        codeABx(finfo, OP_LDCONST, a, b & 0xFF);
    } else {
        codeABC(finfo, OP_MOVE, a, b, 0);
    }
}

/* Free register at top (checks that it´s a register) */
/* Warning: the register must be at top of stack */
static void free_expreg(bfuncinfo *finfo, bexpdesc *e)
{
    /* release temporary register */
    if (e && e->type == ETREG) {
        be_code_freeregs(finfo, 1);
    }
}

/* Allocate `count` new registers on the stack and uptade proto´s max nstack accordingly */
/* Note: deallocate is simpler and handled by a macro */
static void allocstack(bfuncinfo *finfo, int count)
{
    int nstack = finfo->freereg + count;
    if (nstack > finfo->proto->nstack) {
        if (nstack >= 255) {
            be_lexerror(finfo->lexer, "register overflow (more than 255)");
        }
        finfo->proto->nstack = (bbyte)nstack;
    }
}

/* Allocate `count` registers at top of stack, update stack accordingly */
int be_code_allocregs(bfuncinfo *finfo, int count)
{
    int base = finfo->freereg;
    allocstack(finfo, count);
    finfo->freereg += (char)count;
    return base;
}

static void setjump(bfuncinfo *finfo, int pc, int dst)
{
    binstruction *p = be_vector_at(&finfo->code, pc);
    int offset = dst - (pc + 1);
    /* instruction edit jump destination */
    *p = (*p & ~IBx_MASK) | ISET_sBx(offset);
}

static int isjumpbool(bfuncinfo *finfo, int pc)
{
    binstruction *p = be_vector_at(&finfo->code, pc);
    bopcode op = IGET_OP(*p);

    if (op == OP_JMPT || op == OP_JMPF) {
        return 1;
    }
    return 0;
}

static int get_jump(bfuncinfo *finfo, int pc)
{
    binstruction *i = be_vector_at(&finfo->code, pc);
    int offset = IGET_sBx(*i);
    return offset == NO_JUMP ? NO_JUMP : pc + 1 + offset;
}

static void patchlistaux(bfuncinfo *finfo, int list, int vtarget, int dtarget)
{
    while (list != NO_JUMP) {
        int next = get_jump(finfo, list);
        if (isjumpbool(finfo, list)) {
            setjump(finfo, list, dtarget);  /* jump to default target */
        } else {
            setjump(finfo, list, vtarget);
        }
        list = next;
    }
}

static int appendjump(bfuncinfo *finfo, bopcode op, bexpdesc *e)
{
    int reg = e ? var2anyreg(finfo, e) : 0;
    if (isK(reg)) {
        reg = be_code_allocregs(finfo, 1);
        code_move(finfo, reg, e->v.idx);
        e->v.idx = reg;
        e->type = ETREG;
    }
    return codeABx(finfo, op, reg, NO_JUMP + IsBx_MAX);
}

int be_code_jump(bfuncinfo *finfo)
{
    return appendjump(finfo, OP_JMP, NULL);
}

void be_code_jumpto(bfuncinfo *finfo, int dst)
{
    be_code_patchlist(finfo, be_code_jump(finfo), dst);
}

void be_code_jumpbool(bfuncinfo *finfo, bexpdesc *e, int jture)
{
    int pc = appendjump(finfo, jumpboolop(e, jture), e);
    be_code_conjump(finfo, jture ? &e->t : &e->f, pc);
    be_code_patchjump(finfo, jture ? e->f : e->t);
    free_expreg(finfo, e);
    jture ? (e->f = NO_JUMP) : (e->t = NO_JUMP);
    e->not = 0;
}

/* connect jump */
void be_code_conjump(bfuncinfo *finfo, int *list, int jmp)
{
    if (jmp == NO_JUMP) {
        return;
    }
    if (*list == NO_JUMP) {
        *list = jmp;
    } else {
        int next, l = *list;
        while ((next = (get_jump(finfo, l))) != NO_JUMP) {
            l = next;
        }
        setjump(finfo, l, jmp);
    }
}

void be_code_patchlist(bfuncinfo *finfo, int list, int dst)
{
    if (dst == finfo->pc) {
        be_code_patchjump(finfo, list);
    } else {
        patchlistaux(finfo, list, dst, dst);
    }
}

void be_code_patchjump(bfuncinfo *finfo, int jmp)
{
    patchlistaux(finfo, jmp, finfo->pc, finfo->pc);
}

/* Allocate new constant for value k */
/* If k is NULL then we push `nil` value */
static int newconst(bfuncinfo *finfo, bvalue *k)
{
    int idx = be_vector_count(&finfo->kvec);
    be_vector_push_c(finfo->lexer->vm, &finfo->kvec, k);
    finfo->proto->ktab = be_vector_data(&finfo->kvec);
    finfo->proto->nconst = be_vector_capacity(&finfo->kvec);
    if (k == NULL) {
        var_setnil(&finfo->proto->ktab[idx]);
    }
    return idx;
}

/* Find constant by value and return constant number, or -1 if constant does not exist */
/* The search is linear and lilited to 50 elements for performance reasons */
static int findconst(bfuncinfo *finfo, bexpdesc *e)
{
    int i, count = be_vector_count(&finfo->kvec);
    /* if the constant table is too large, the lookup
     * operation will become very time consuming.
     * so only search the constant table for the
     * previous value.
     **/
    count = count < 50 ? count : 50;
    for (i = 0; i < count; ++i) {
        bvalue *k = be_vector_at(&finfo->kvec, i);
        switch (e->type) {
        case ETINT:
            if (var_isint(k) && k->v.i == e->v.i) {
                return i;
            }
            break;
        case ETREAL:
            if (var_isreal(k) && k->v.r == e->v.r) {
                return i;
            }
            break;
        case ETSTRING:
            if (var_isstr(k) && be_eqstr(k->v.p, e->v.s)) {
                return i;
            }
            break;
        default:
            break;
        }
    }
    return -1;
}

/* convert expdesc to constant and return kreg index (either constant kindex or register number) */
static int exp2const(bfuncinfo *finfo, bexpdesc *e)
{
    int idx = findconst(finfo, e); /* does the constant already exist? */
    if (idx == -1) { /* if not add it */
        bvalue k;
        switch (e->type) {
        case ETINT:
            k.type = BE_INT;
            k.v.i = e->v.i;
            break;
        case ETREAL:
            k.type = BE_REAL;
            k.v.r = e->v.r;
            break;
        case ETSTRING:
            k.type = BE_STRING;
            k.v.s = e->v.s;
            break;
        default: /* all other values are filled later */
            break;
        }
        idx = newconst(finfo, &k);  /* create new constant */
    }
    if (idx < 256) {  /* if constant number fits in KB or KC */
        e->type = ETCONST;  /* new type is constant by index */
        e->v.idx = setK(idx);
    } else { /* index value is too large */
        e->type = ETREG;  /* does not fit in compact mode, allocate an explicit register and emit LDCONTS */
        e->v.idx = be_code_allocregs(finfo, 1);
        codeABx(finfo, OP_LDCONST, e->v.idx, idx);
    }
    return e->v.idx;
}

static void free_suffix(bfuncinfo *finfo, bexpdesc *e)
{
    int idx = e->v.ss.idx;
    int nlocal = be_list_count(finfo->local);
    /* release suffix register */
    if (!isK(idx) && idx >= nlocal) {
        be_code_freeregs(finfo, 1);
    }
    /* release object register */
    if (e->v.ss.tt == ETREG && (int)e->v.ss.obj >= nlocal && (e->v.ss.obj + 1 >= finfo->freereg)) {
        be_code_freeregs(finfo, 1);
    }
}

static int code_suffix(bfuncinfo *finfo, bopcode op, bexpdesc *e, int dst)
{
    free_suffix(finfo, e); /* free temporary registers */
    if (dst > finfo->freereg) {
        dst = finfo->freereg;
    }
    codeABC(finfo, op, dst, e->v.ss.obj, e->v.ss.idx);
    return dst;
}

/* idx: the proto index in proto_table
 * dst: the destination register
 **/
static void code_closure(bfuncinfo *finfo, int idx, int dst)
{
    codeABx(finfo, OP_CLOSURE, dst, idx); /* load closure to register */
}

/* Given an integer, check if we should create a constant */
/* True for values 0..3 and if there is room for kindex */
/* This optimization makes code more compact for commonly used ints */
static bbool constint(bfuncinfo *finfo, bint i)
{
    /* cache common numbers */
    if ((i < IsBx_MIN || i > IsBx_MAX) ||
        (i >= 0 && i <= 3 && be_vector_count(&finfo->kvec) < 256)) {
        return btrue;
    }
    return bfalse;
}

/* Compute variable from an expdesc */
/* Return constant index, or existing register or fallback to dst */
/* If dst is `freereg`, the register is allocated */
/* TODO member or index change dst, why? */
static int var2reg(bfuncinfo *finfo, bexpdesc *e, int dst)
{
    be_assert(e != NULL);
    switch (e->type) {
    case ETINT:
        if (constint(finfo, e->v.i)) {
            return exp2const(finfo, e);
        } else {
            codeABx(finfo, OP_LDINT, dst, var_toidx(e) + IsBx_MAX);
        }
        break;
    case ETBOOL:
        code_bool(finfo, dst, e->v.i != 0, 0);
        break;
    case ETNIL:
        codeABx(finfo, OP_LDNIL, dst, 0);
        break;
    case ETREAL:
    case ETSTRING:
        return exp2const(finfo, e);
    case ETPROTO:
        code_closure(finfo, e->v.idx, dst);
        break;
    case ETGLOBAL:
        codeABx(finfo, OP_GETGBL, dst, e->v.idx);
        break;
    case ETNGLOBAL:
        codeABC(finfo, OP_GETNGBL, dst, e->v.ss.idx, 0);
        break;
    case ETUPVAL:
        codeABx(finfo, OP_GETUPV, dst, e->v.idx);
        break;
    case ETMEMBER:
        dst = code_suffix(finfo, OP_GETMBR, e, dst);
        break;
    case ETINDEX:
        dst = code_suffix(finfo, OP_GETIDX, e, dst);
        break;
    case ETLOCAL: case ETREG: case ETCONST:
        return e->v.idx;
    default:
        return dst; /* error */
    }
    if (dst == finfo->freereg) { /* use a new register */
        be_code_allocregs(finfo, 1);
    }
    e->type = ETREG;
    e->v.idx = dst;
    return dst;
}

/* TODO */
static int exp2reg(bfuncinfo *finfo, bexpdesc *e, int dst)
{
    int reg = var2reg(finfo, e, dst);
    if (hasjump(e)) { /* if conditional expression */
        int pcf = NO_JUMP;  /* position of an eventual LOAD false */
        int pct = NO_JUMP;  /* position of an eventual LOAD true */
        int jpt = appendjump(finfo, jumpboolop(e, 1), e);
        reg = e->v.idx;
        be_code_conjump(finfo, &e->t, jpt);
        pcf = code_bool(finfo, reg, 0, 1);
        pct = code_bool(finfo, reg, 1, 0);
        patchlistaux(finfo, e->f, finfo->pc, pcf);
        patchlistaux(finfo, e->t, finfo->pc, pct);
        e->t = NO_JUMP;
        e->f = NO_JUMP;
        e->not = 0;
    }
    return reg;
}

/* Select dest registers from both expressions */
/* If one of them is already a register, keep it */
/* TODO if e1 or e2 are registers, we keep the lowest and free the highest (that must be at top) */
/* If none is a register, allocate a new one */
static int codedestreg(bfuncinfo *finfo, bexpdesc *e1, bexpdesc *e2)
{
    int dst, con1 = e1->type == ETREG, con2 = e2->type == ETREG;

    if (con1 && con2) {
        dst = min(e1->v.idx, e2->v.idx);
        be_code_freeregs(finfo, 1);
    } else if (con1) {
        dst = e1->v.idx;
    } else if (con2) {
        dst = e2->v.idx;
    } else {
        dst = be_code_allocregs(finfo, 1);
    }
    return dst;
}

/* compute binary expression and update e1 as result */
static void binaryexp(bfuncinfo *finfo, bopcode op, bexpdesc *e1, bexpdesc *e2)
{
    int src1 = exp2anyreg(finfo, e1);  /* constant or existing register or new register */
    int src2 = exp2anyreg(finfo, e2);  /* constant or existing register or new register */
    int dst = codedestreg(finfo, e1, e2);  /* compute target register */
    codeABC(finfo, op, dst, src1, src2);  /* emit operation */
    e1->type = ETREG;
    e1->v.idx = dst; /* update register as output */
}

void be_code_prebinop(bfuncinfo *finfo, int op, bexpdesc *e)
{
    switch (op) {
    case OptAnd:
        be_code_jumpbool(finfo, e, bfalse);
        break;
    case OptOr:
        be_code_jumpbool(finfo, e, btrue);
        break;
    default:
        exp2anyreg(finfo, e);
        break;
    }
}

/* Apply binary operator `op` to e1 and e2, result in e1 */
/* There is always a new temp register allocated to handle the second argument so you must free it on exit TODO */
void be_code_binop(bfuncinfo *finfo, int op, bexpdesc *e1, bexpdesc *e2)
{
    switch (op) {
    case OptAnd:
        var2anyreg(finfo, e2);
        be_code_conjump(finfo, &e2->f, e1->f);
        *e1 = *e2;
        break;
    case OptOr:
        var2anyreg(finfo, e2);
        be_code_conjump(finfo, &e2->t, e1->t);
        *e1 = *e2;
        break;
    case OptAdd: case OptSub: case OptMul: case OptDiv:
    case OptMod: case OptLT: case OptLE: case OptEQ:
    case OptNE: case OptGT: case OptGE: case OptConnect:
    case OptBitAnd: case OptBitOr: case OptBitXor:
    case OptShiftL: case OptShiftR:
        binaryexp(finfo, (bopcode)(op - OptAdd), e1, e2);
        break;
    default: break;
    }
}

/* Apply unary operator and return register number */
/* If input is register, change in place or allocate new register */
static void unaryexp(bfuncinfo *finfo, bopcode op, bexpdesc *e)
{
    int src = exp2anyreg(finfo, e);
    int dst = e->type == ETREG ? src : be_code_allocregs(finfo, 1);
    codeABC(finfo, op, dst, src, 0);
    e->type = ETREG;
    e->v.idx = dst;
}

/* Apply not to conditional expression */
/* If literal compute the value */
/* Or invert t/f subexpressions */
static void code_not(bexpdesc *e)
{
    switch (e->type) {
    case ETINT: e->v.i = e->v.i == 0; break;
    case ETREAL: e->v.i = e->v.r == cast(breal, 0); break;
    case ETNIL: e->v.i = 1; break;
    case ETBOOL: e->v.i = !e->v.i; break;
    case ETSTRING: e->v.i = 0; break;
    default: {
        int temp = e->t;
        e->t = e->f;
        e->f = temp;
        e->not = NOT_EXPR | (e->not ^ NOT_MASK);
        return;
    }
    }
    e->type = ETBOOL;
}

/* Negative value of literal or emit NEG opcode */
static int code_neg(bfuncinfo *finfo, bexpdesc *e)
{
    switch (e->type) {
    case ETINT: e->v.i = -e->v.i; break;
    case ETREAL: e->v.r = -e->v.r; break;
    case ETNIL: case ETBOOL: case ETSTRING:
        return 1; /* error */
    default:
        unaryexp(finfo, OP_NEG, e);
    }
    return 0;
}

/* Bit flip of literal or emit FLIP opcode */
static int code_flip(bfuncinfo *finfo, bexpdesc *e)
{
    switch (e->type) {
    case ETINT: e->v.i = ~e->v.i; break;
    case ETREAL: case ETNIL: case ETBOOL: case ETSTRING:
        return 2; /* error */
    default:
        unaryexp(finfo, OP_FLIP, e);
    }
    return 0;
}

/* Apply unary operator: not, neg or bitflip */
int be_code_unop(bfuncinfo *finfo, int op, bexpdesc *e)
{
    switch (op) {
    case OptNot:
        code_not(e); break;
    case OptFlip: /* do nothing */
        return code_flip(finfo, e);
    case OptSub:
        return code_neg(finfo, e);
    default:
        break;
    }
    return 0;
}

static void setbgblvar(bfuncinfo *finfo, bopcode op, bexpdesc *e1, int src)
{
    if (isK(src)) { /* move const to register */
        code_move(finfo, finfo->freereg, src);
        src = finfo->freereg;
    }
    codeABC(finfo, op, src, e1->v.idx, 0);
}

static void setsupvar(bfuncinfo *finfo, bopcode op, bexpdesc *e1, int src)
{
    if (isK(src)) { /* move const to register */
        code_move(finfo, finfo->freereg, src);
        src = finfo->freereg;
    }
    codeABx(finfo, op, src, e1->v.idx);
}

static void setsfxvar(bfuncinfo *finfo, bopcode op, bexpdesc *e1, int src)
{
    int obj = e1->v.ss.obj;
    free_suffix(finfo, e1);
    if (isK(obj)) { /* move const to register */
        code_move(finfo, finfo->freereg, obj);
        obj = finfo->freereg;
    }
    codeABC(finfo, op, obj, e1->v.ss.idx, src);
}

/* Assign expr e2 to e1 */
/* e1 must be in a register and have a valid idx */
/* return 1 if assignment was possible, 0 if type is not compatible */
int be_code_setvar(bfuncinfo *finfo, bexpdesc *e1, bexpdesc *e2)
{
    int src = exp2reg(finfo, e2,
        e1->type == ETLOCAL ? e1->v.idx : finfo->freereg); /* Convert e2 to kreg */
        /* If e1 is a local variable, use the register */

    if (e1->type != ETLOCAL || e1->v.idx != src) {
        free_expreg(finfo, e2); /* free source (checks only ETREG) */ /* TODO e2 is at top */
    }
    switch (e1->type) {
    case ETLOCAL: /* It can't be ETREG. */
        if (e1->v.idx != src) {
            code_move(finfo, e1->v.idx, src); /* do explicit move only if needed */
        }
        break;
    case ETGLOBAL: /* store to grobal R(A) -> G(Bx) by global index */
        setsupvar(finfo, OP_SETGBL, e1, src);
        break;
    case ETNGLOBAL: /* store to global R(A) -> G(Bx) by name */
        setbgblvar(finfo, OP_SETNGBL, e1, src);
        break;
    case ETUPVAL:
        setsupvar(finfo, OP_SETUPV, e1, src);
        break;
    case ETMEMBER: /* store to member R(A).RK(B) <- RK(C) */
        setsfxvar(finfo, OP_SETMBR, e1, src);
        break;
    case ETINDEX: /* store to member R(A)[RK(B)] <- RK(C) */
        setsfxvar(finfo, OP_SETIDX, e1, src);
        break;
    default:
        return 1;
    }
    return 0;
}

/* Get the expdesc as a register */
/* if already in a register, use the existing register */
/* if local or const, allocate a new register and copy value */
int be_code_nextreg(bfuncinfo *finfo, bexpdesc *e)
{
    int dst = finfo->freereg;
    int src = exp2anyreg(finfo, e); /* get variable register index */
    if (e->type != ETREG) { /* move local and const to new register */
        code_move(finfo, dst, src);
        be_code_allocregs(finfo, 1);
    } else {
        dst = src;
    }
    return dst;
}

int be_code_getmethod(bfuncinfo *finfo, bexpdesc *e)
{
    int dst = finfo->freereg;
    be_assert(e->type == ETMEMBER);
    dst = code_suffix(finfo, OP_GETMET, e, dst);
    /* method [object] args */
    be_code_allocregs(finfo, dst == finfo->freereg ? 2 : 1);
    return dst;
}

/* Generate a CALL instruction at base register with argc consecutive values */
/* i.e. arg1 is base+1... */
/* Important: argc registers are freed upon call, which are supposed to be registers above base */
/* TODO check that there are no more than `argc` registers above ‘base` on stack */
void be_code_call(bfuncinfo *finfo, int base, int argc)
{
    codeABC(finfo, OP_CALL, base, argc, 0);
    be_code_freeregs(finfo, argc);
}

int be_code_proto(bfuncinfo *finfo, bproto *proto)
{
    int idx = be_vector_count(&finfo->pvec);
    /* append proto to current function proto table */
    be_vector_push_c(finfo->lexer->vm, &finfo->pvec, &proto);
    finfo->proto->ptab = be_vector_data(&finfo->pvec);
    finfo->proto->nproto = be_vector_capacity(&finfo->pvec);
    return idx;
}

void be_code_closure(bfuncinfo *finfo, bexpdesc *e, int idx)
{
    int reg = e->type == ETGLOBAL ? finfo->freereg: e->v.idx;
    code_closure(finfo, idx, reg);
    if (e->type == ETGLOBAL) { /* store to grobal R(A) -> G(Bx) */
        codeABx(finfo, OP_SETGBL, reg, e->v.idx);
    }
}

void be_code_close(bfuncinfo *finfo, int isret)
{
    bblockinfo *binfo = finfo->binfo;
    if (isret) { /* in 'return' */
        while (binfo && !binfo->hasupval) {
            binfo = binfo->prev;
        }
        if (binfo) {
            codeABC(finfo, OP_CLOSE, 0, 0, 0);
        }
    } else if (binfo->prev) { /* leave block */
        if (binfo->hasupval) {
            codeABC(finfo, OP_CLOSE, binfo->nactlocals, 0, 0);
        }
    }
}

static void leave_function(bfuncinfo *finfo)
{
    int try_depth = 0; /* count of exception catch blocks */
    bblockinfo *binfo = finfo->binfo;
    for (; binfo; binfo = binfo->prev) {
        if (binfo->type & BLOCK_EXCEPT) {
            ++try_depth; /* leave the exception catch block */
        }
    }
    if (try_depth) { /* exception catch blocks that needs to leave */
        be_code_exblk(finfo, try_depth);
    }
}

void be_code_ret(bfuncinfo *finfo, bexpdesc *e)
{
    if (finfo->binfo->prev == NULL) {
        if (finfo->flags & FUNC_RET_FLAG) {
            return;
        }
        finfo->flags |= FUNC_RET_FLAG;
    }
    if (e) {
        int reg = exp2anyreg(finfo, e);
        be_code_close(finfo, 1);
        leave_function(finfo);
        codeABC(finfo, OP_RET, e->type != ETVOID, reg, 0);
        free_expreg(finfo, e);
    } else {
        be_code_close(finfo, 1);
        codeABC(finfo, OP_RET, 0, 0, 0);
    }
}

static void package_suffix(bfuncinfo *finfo, bexpdesc *c, bexpdesc *k)
{
    int key = exp2anyreg(finfo, k);
    c->v.ss.obj = exp2anyreg(finfo, c);
    c->v.ss.tt = c->type;
    c->v.ss.idx = key;
}

int be_code_nglobal(bfuncinfo *finfo, bexpdesc *k)
{
    return exp2anyreg(finfo, k);
}

void be_code_member(bfuncinfo *finfo, bexpdesc *c, bexpdesc *k)
{
    package_suffix(finfo, c, k);
    c->type = ETMEMBER;
}

void be_code_index(bfuncinfo *finfo, bexpdesc *c, bexpdesc *k)
{
    package_suffix(finfo, c, k);
    c->type = ETINDEX;
}

void be_code_class(bfuncinfo *finfo, bexpdesc *dst, bclass *c)
{
    int src;
    bvalue var;
    var_setclass(&var, c);
    src = newconst(finfo, &var);
    if (dst->type == ETLOCAL) {
        codeABx(finfo, OP_LDCONST, dst->v.idx, src);
    } else {
        codeABx(finfo, OP_LDCONST, finfo->freereg, src);
        codeABx(finfo, OP_SETGBL, finfo->freereg, dst->v.idx);
    }
    codeABx(finfo, OP_CLASS, 0, src);
}

void be_code_setsuper(bfuncinfo *finfo, bexpdesc *c, bexpdesc *s)
{
    int self = exp2anyreg(finfo, c);
    int super = exp2anyreg(finfo, s);
    codeABC(finfo, OP_SETSUPER, self, super, 0);
    free_expreg(finfo, c);
    free_expreg(finfo, s);
}

/* Emit IMPORT opcode for import module */
/* `m` is module name, is copied to register if not already */
/* `v` is destination where the imported module is stored */
/* If destination is a local variable, it is the destination of the IMPORT opcode */
/* otherwise the value is copied to a temporary register and stored to the destination */
/* TODO is this optilization useful, isn´t it done anyways by be_code_move optim? */
void be_code_import(bfuncinfo *finfo, bexpdesc *m, bexpdesc *v)
{
    int dst, src = exp2anyreg(finfo, m);
    if (v->type == ETLOCAL) {
        dst = v->v.idx;
        codeABC(finfo, OP_IMPORT, dst, src, 0);
    } else {
        dst = be_code_allocregs(finfo, 1);
        codeABC(finfo, OP_IMPORT, dst, src, 0);
        m->type = ETREG;
        m->v.idx = dst;
        be_code_setvar(finfo, v, m); /* TODO is the register freed? */
    }
}

int be_code_exblk(bfuncinfo *finfo, int depth)
{
    if (depth == 0) {
        return appendjump(finfo, OP_EXBLK, NULL);
    }
    codeABx(finfo, OP_EXBLK, 1, depth);
    return 0;
}

void be_code_catch(bfuncinfo *finfo, int base, int ecnt, int vcnt, int *jmp)
{
    codeABC(finfo, OP_CATCH, base, ecnt, vcnt);
    if (jmp) {
        *jmp = NO_JUMP; /* reset jump point */
        be_code_conjump(finfo, jmp, be_code_jump(finfo));
    }
}

/* Emit RAISE opcode */
/* e1 is the exception code */
/* e2 is the exception description */
/* both are materialized to a temp register (if not null) */
void be_code_raise(bfuncinfo *finfo, bexpdesc *e1, bexpdesc *e2)
{
    if (e1) {
        int src1 = exp2anyreg(finfo, e1);
        int src2 = e2 ? exp2anyreg(finfo, e2) : 0;
        codeABC(finfo, OP_RAISE, e2 != NULL, src1, src2);
    } else {
        codeABC(finfo, OP_RAISE, 2, 0, 0); /* rethrow the current exception with parameters already on top of stack */
    }
    /* release the register occupied by the expression */
    free_expreg(finfo, e1);
    free_expreg(finfo, e2);
}

#endif
