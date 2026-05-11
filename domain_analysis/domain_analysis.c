#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "domain_analysis.h"

Symbols symbols;
int crtDepth = 0;
Symbol *crtFunc = NULL;
Symbol *crtStruct = NULL;

void initSymbols(Symbols *symbols) {
    symbols->begin = NULL;
    symbols->end = NULL;
    symbols->after = NULL;
}

void initDomainAnalysis(void) {
    initSymbols(&symbols);
    crtDepth = 0;
    crtFunc = NULL;
    crtStruct = NULL;
}

void addSymbolPtr(Symbols *symbols, Symbol *s) {
    if (symbols->end == symbols->after) {
        int count = (int)(symbols->end - symbols->begin);
        int n = count * 2;
        if (n == 0) n = 1;

        symbols->begin = (Symbol **)realloc(symbols->begin, n * sizeof(Symbol *));
        if (!symbols->begin) {
            printf("not enough memory\n");
            exit(1);
        }
        symbols->end = symbols->begin + count;
        symbols->after = symbols->begin + n;
    }

    *symbols->end++ = s;
}

Symbol *addSymbol(Symbols *symbols, const char *name, int cls) {
    Symbol *s = (Symbol *)malloc(sizeof(Symbol));
    if (!s) {
        printf("not enough memory\n");
        exit(1);
    }

    memset(s, 0, sizeof(Symbol));
    s->name = name;
    s->cls = cls;
    s->depth = crtDepth;
    s->type.nElements = -1;
    initSymbols(&s->args);

    addSymbolPtr(symbols, s);
    return s;
}

Symbol *dupSymbol(const Symbol *src) {
    Symbol *s = (Symbol *)malloc(sizeof(Symbol));
    if (!s) {
        printf("not enough memory\n");
        exit(1);
    }

    memcpy(s, src, sizeof(Symbol));
    initSymbols(&s->args);
    return s;
}

Symbol *findSymbol(Symbols *symbols, const char *name) {
    Symbol **p;

    for (p = symbols->end; p != symbols->begin;) {
        p--;
        if (strcmp((*p)->name, name) == 0) {
            return *p;
        }
    }

    return NULL;
}

Symbol *findSymbolInDomain(Symbols *symbols, const char *name) {
    Symbol **p;

    for (p = symbols->end; p != symbols->begin;) {
        p--;
        if ((*p)->depth != crtDepth) {
            break;
        }
        if (strcmp((*p)->name, name) == 0) {
            return *p;
        }
    }

    return NULL;
}

void deleteSymbolsAfter(Symbols *symbols, Symbol **start) {
    Symbol **p;

    for (p = start; p != symbols->end; p++) {
        free(*p);
    }
    symbols->end = start;
}

void pushDomain(void) {
    crtDepth++;
}

void dropDomain(void) {
    Symbol **p = symbols.end;

    while (p != symbols.begin) {
        p--;
        if ((*p)->depth < crtDepth) {
            p++;
            break;
        }
    }

    deleteSymbolsAfter(&symbols, p);
    crtDepth--;
}

static const char *clsName(int cls) {
    switch (cls) {
        case CLS_VAR: return "VAR";
        case CLS_FUNC: return "FUNC";
        case CLS_EXTFUNC: return "EXTFUNC";
        case CLS_STRUCT: return "STRUCT";
    }
    return "?";
}

static const char *memName(int mem) {
    switch (mem) {
        case MEM_GLOBAL: return "GLOBAL";
        case MEM_ARG: return "ARG";
        case MEM_LOCAL: return "LOCAL";
    }
    return "?";
}

static const char *typeBaseName(int tb) {
    switch (tb) {
        case TB_INT: return "int";
        case TB_DOUBLE: return "double";
        case TB_CHAR: return "char";
        case TB_STRUCT: return "struct";
        case TB_VOID: return "void";
    }
    return "?";
}

static void printType(const Type *t) {
    printf("%s", typeBaseName(t->typeBase));
    if (t->typeBase == TB_STRUCT && t->s) {
        printf(" %s", t->s->name);
    }
    if (t->nElements > 0) {
        printf("[%d]", t->nElements);
    } else if (t->nElements == 0) {
        printf("[]");
    } else if (t->nElements == -2) {
        printf("[expr]");
    }
}

static void showSymbolList(const char *title, Symbols *list) {
    Symbol **p;

    if (list->begin == list->end) return;

    printf("  %s:\n", title);
    for (p = list->begin; p != list->end; p++) {
        Symbol *s = *p;
        printf("    %s  cls=%s  mem=%s  type=", s->name, clsName(s->cls), memName(s->mem));
        printType(&s->type);
        printf("  depth=%d\n", s->depth);
    }
}

void showDomainSymbols(void) {
    Symbol **p;

    printf("\nSymbols:\n");
    for (p = symbols.begin; p != symbols.end; p++) {
        Symbol *s = *p;

        printf("%s  cls=%s  mem=%s  type=", s->name, clsName(s->cls), memName(s->mem));
        printType(&s->type);
        printf("  depth=%d\n", s->depth);

        if (s->cls == CLS_FUNC) {
            showSymbolList("args", &s->args);
        } else if (s->cls == CLS_STRUCT) {
            showSymbolList("members", &s->members);
        }
    }
}
