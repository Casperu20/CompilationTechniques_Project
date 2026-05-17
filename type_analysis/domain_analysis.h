#ifndef DOMAIN_ANALYSIS_H
#define DOMAIN_ANALYSIS_H

#include "atomc.h"

enum { TB_INT, TB_DOUBLE, TB_CHAR, TB_STRUCT, TB_VOID };
enum { CLS_VAR, CLS_FUNC, CLS_EXTFUNC, CLS_STRUCT };
enum { MEM_GLOBAL, MEM_ARG, MEM_LOCAL };

typedef struct _Symbol Symbol;

typedef struct {
    int typeBase;       /* TB_* */
    Symbol *s;          /* struct definition for TB_STRUCT */
    int nElements;      /* >0 const array size, 0 unsized array, -1 non-array, -2 expr-sized array */
} Type;

typedef struct {
    Symbol **begin;
    Symbol **end;
    Symbol **after;
} Symbols;

struct _Symbol {
    const char *name;
    int cls;            /* CLS_* */
    int mem;            /* MEM_* */
    Type type;
    int depth;

    union {
        Symbols args;       /* used for functions */
        Symbols members;    /* used for structs */
    };
};

extern Symbols symbols;
extern int crtDepth;
extern Symbol *crtFunc;
extern Symbol *crtStruct;

void initDomainAnalysis(void);
void initSymbols(Symbols *symbols);
Symbol *addSymbol(Symbols *symbols, const char *name, int cls);
Symbol *dupSymbol(const Symbol *src);
void addSymbolPtr(Symbols *symbols, Symbol *s);
Symbol *findSymbol(Symbols *symbols, const char *name);
Symbol *findSymbolInDomain(Symbols *symbols, const char *name);
Symbol *findSymbolInList(Symbols *list, const char *name);
void deleteSymbolsAfter(Symbols *symbols, Symbol **start);
void pushDomain(void);
void dropDomain(void);
void showDomainSymbols(void);

#endif
