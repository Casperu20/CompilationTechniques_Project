#ifndef TYPE_ANALYZER_H
#define TYPE_ANALYZER_H

#include "domain_analysis.h"

typedef union {
    long int i;          /* int, char */
    double d;            /* double */
    const char *str;     /* char[] */
} CtVal;

typedef struct {
    Type type;
    int lval;
    int ct;
    CtVal ctVal;
} Ret;

Type createType(int typeBase, Symbol *s, int nElements);
int isArrayType(const Type *t);
int canBeScalar(const Ret *r);
int convTo(const Type *src, const Type *dst);
int arithTypeTo(const Type *s1, const Type *s2, Type *dst);
void addExtFuncs(void);
int symbolsLen(const Symbols *symbols);

#endif
