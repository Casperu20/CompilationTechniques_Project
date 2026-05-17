#include <stdio.h>
#include "type_analyzer.h"
#include "atomc.h"

extern Token *crtTk;

Type createType(int typeBase, Symbol *s, int nElements) {
    Type t;
    t.typeBase = typeBase;
    t.s = s;
    t.nElements = nElements;
    return t;
}

int isArrayType(const Type *t) {
    /* In the original AtomC model nElements >= 0 means array.
       This project also uses -2 for expression-sized arrays such as [20/4+5]. */
    return t->nElements >= 0 || t->nElements == -2;
}

int canBeScalar(const Ret *r) {
    if (isArrayType(&r->type)) return 0;
    if (r->type.typeBase == TB_STRUCT || r->type.typeBase == TB_VOID) return 0;
    return r->type.typeBase == TB_CHAR || r->type.typeBase == TB_INT || r->type.typeBase == TB_DOUBLE;
}

int convTo(const Type *src, const Type *dst) {
    int srcArray = isArrayType(src);
    int dstArray = isArrayType(dst);

    if (srcArray) {
        if (dstArray) {
            if (src->typeBase != dst->typeBase) return 0;
            if (src->typeBase == TB_STRUCT && src->s != dst->s) return 0;
            return 1;
        }
        return 0;
    }

    if (dstArray) return 0;

    switch (src->typeBase) {
        case TB_CHAR:
        case TB_INT:
        case TB_DOUBLE:
            return dst->typeBase == TB_CHAR || dst->typeBase == TB_INT || dst->typeBase == TB_DOUBLE;

        case TB_STRUCT:
            return dst->typeBase == TB_STRUCT && src->s == dst->s;
    }

    return 0;
}

int arithTypeTo(const Type *s1, const Type *s2, Type *dst) {
    if (isArrayType(s1) || isArrayType(s2)) return 0;
    if (s1->typeBase == TB_STRUCT || s2->typeBase == TB_STRUCT) return 0;
    if (s1->typeBase == TB_VOID || s2->typeBase == TB_VOID) return 0;

    if (s1->typeBase == TB_DOUBLE || s2->typeBase == TB_DOUBLE) {
        *dst = createType(TB_DOUBLE, NULL, -1);
        return 1;
    }
    if (s1->typeBase == TB_INT || s2->typeBase == TB_INT) {
        *dst = createType(TB_INT, NULL, -1);
        return 1;
    }
    if (s1->typeBase == TB_CHAR && s2->typeBase == TB_CHAR) {
        *dst = createType(TB_CHAR, NULL, -1);
        return 1;
    }
    return 0;
}

int symbolsLen(const Symbols *symbols) {
    return (int)(symbols->end - symbols->begin);
}

static Symbol *addExtFunc(const char *name, Type type) {
    Symbol *s = addSymbol(&symbols, name, CLS_EXTFUNC);
    s->mem = MEM_GLOBAL;
    s->type = type;
    initSymbols(&s->args);
    return s;
}

static Symbol *addFuncArg(Symbol *func, const char *name, Type type) {
    Symbol *a = addSymbol(&func->args, name, CLS_VAR);
    a->mem = MEM_ARG;
    a->type = type;
    return a;
}

void addExtFuncs(void) {
    Symbol *s;

    s = addExtFunc("put_s", createType(TB_VOID, NULL, -1));
    addFuncArg(s, "s", createType(TB_CHAR, NULL, 0));

    s = addExtFunc("get_s", createType(TB_VOID, NULL, -1));
    addFuncArg(s, "s", createType(TB_CHAR, NULL, 0));

    s = addExtFunc("put_i", createType(TB_VOID, NULL, -1));
    addFuncArg(s, "i", createType(TB_INT, NULL, -1));

    addExtFunc("get_i", createType(TB_INT, NULL, -1));

    s = addExtFunc("put_d", createType(TB_VOID, NULL, -1));
    addFuncArg(s, "d", createType(TB_DOUBLE, NULL, -1));

    addExtFunc("get_d", createType(TB_DOUBLE, NULL, -1));

    s = addExtFunc("put_c", createType(TB_VOID, NULL, -1));
    addFuncArg(s, "c", createType(TB_CHAR, NULL, -1));

    addExtFunc("get_c", createType(TB_CHAR, NULL, -1));

    addExtFunc("seconds", createType(TB_DOUBLE, NULL, -1));
}
