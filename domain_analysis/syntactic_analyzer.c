#include <stdio.h>
#include "atomc.h"
#include "domain_analysis.h"

/* parser globals */
Token *crtTk;
Token *consumedTk;

/* forward declarations */
int unit();
int structDef();
int varDef();
int varDefCore(int allowInit);
int typeBase(Type *t);
int arrayDecl(Type *t);
int fnDef();
int fnParam();
int stm();
int stmCompound(int newDomain);
int expr();
int exprAssign();
int exprOr();
int exprOrAux();
int exprAnd();
int exprAndAux();
int exprEq();
int exprEqAux();
int exprRel();
int exprRelAux();
int exprAdd();
int exprAddAux();
int exprMul();
int exprMulAux();
int exprCast();
int exprUnary();
int exprPostfix();
int exprPostfixAux();
int exprPrimary();

/* tracks the type of the most recent primary/postfix expression,
 * used for struct field access checking in exprPostfixAux. */
static Type crtExprType;
static int crtExprTyped = 0;

int consume(int code) {
    if (crtTk->code == code) {
        consumedTk = crtTk;
        crtTk = crtTk->next;
        return 1;
    }
    return 0;
}

void parse() {
    crtTk = tokens;

    initDomainAnalysis();

    if (!unit()) {
        tkerr(crtTk, "syntax error");
    }

    printf("Syntax OK\n");
    showDomainSymbols();
}

int unit() {
    while (1) {
        if (structDef()) {
        } else if (fnDef()) {
        } else if (varDef()) {
        } else {
            break;
        }
    }

    if (!consume(END)) {
        tkerr(crtTk, "missing END");
    }

    return 1;
}

int typeBase(Type *t) {
    t->s = NULL;
    t->nElements = -1;

    if (consume(INT)) {
        t->typeBase = TB_INT;
        return 1;
    }

    if (consume(DOUBLE)) {
        t->typeBase = TB_DOUBLE;
        return 1;
    }

    if (consume(CHAR)) {
        t->typeBase = TB_CHAR;
        return 1;
    }

    if (consume(STRUCT)) {
        if (consume(ID)) {
            Token *tkName = consumedTk;
            Symbol *s = findSymbol(&symbols, tkName->text);

            if (!s || s->cls != CLS_STRUCT) {
                tkerr(crtTk, "undefined struct: %s", tkName->text);
            }

            t->typeBase = TB_STRUCT;
            t->s = s;
            return 1;
        } else {
            tkerr(crtTk, "missing ID after struct");
        }
    }

    return 0;
}

/* improvement: array size may be CT_INT, empty, or an expression like 20/4+5 */
int arrayDecl(Type *t) {
    Token *startExpr;

    if (consume(LBRACKET)) {
        startExpr = crtTk;

        if (consume(CT_INT)) {
            if (crtTk->code == RBRACKET) {
                t->nElements = (int)consumedTk->i;
            } else {
                crtTk = startExpr;
                if (!expr()) {
                    tkerr(crtTk, "invalid array size expression");
                }
                t->nElements = -2; /* expression-sized array */
            }
        } else if (expr()) {
            t->nElements = -2; /* expression-sized array */
        } else {
            t->nElements = 0; /* [] */
        }

        if (consume(RBRACKET)) {
            return 1;
        } else {
            tkerr(crtTk, "missing ]");
        }
    }

    return 0;
}

// improvement as in C we should have: varDef: typeBase ID arrayDecl? (ASSIGN expr)? SEMICOLON
int varDef() {
    return varDefCore(1);
}

int varDefCore(int allowInit) {
    Token *startTk = crtTk;
    Type baseType;

    if (typeBase(&baseType)) {
        if (consume(ID)) {
            Token *tkName = consumedTk;
            Type t = baseType;

            arrayDecl(&t); // ? = optional

            if (t.nElements == 0) {
                tkerr(crtTk, "a vector variable must have a specified dimension");
            }

            // optional initialization: int b = 5 + 3 * 2 etc
            if (allowInit && consume(ASSIGN)) {
                if (!expr()) {
                    tkerr(crtTk, "missing expression after =");
                }
            }

            if (findSymbolInDomain(&symbols, tkName->text)) {
                tkerr(crtTk, "symbol redefinition: %s", tkName->text);
            }

            {
                Symbol *var = addSymbol(&symbols, tkName->text, CLS_VAR);
                var->type = t;
                var->mem = (crtStruct || crtFunc) ? MEM_LOCAL : MEM_GLOBAL;

                if (crtStruct) {
                    addSymbolPtr(&crtStruct->members, dupSymbol(var));
                }
            }

            // comma-separated declarators --> int i, v[5], s;
            while (consume(COMMA)) {
                Type t2 = baseType;
                Token *tkName2;

                if (consume(ID)) {
                    tkName2 = consumedTk;
                    arrayDecl(&t2); // ? = optional

                    if (t2.nElements == 0) {
                        tkerr(crtTk, "a vector variable must have a specified dimension");
                    }

                    // optional initializer --> int i = 5, v[5], s = "hello";
                    // {1,2,3} are not supported by the given grammar
                    if (allowInit && consume(ASSIGN)) {
                        if (!expr()) {
                            tkerr(crtTk, "missing expression after =");
                        }
                    }

                    if (findSymbolInDomain(&symbols, tkName2->text)) {
                        tkerr(crtTk, "symbol redefinition: %s", tkName2->text);
                    }

                    {
                        Symbol *var = addSymbol(&symbols, tkName2->text, CLS_VAR);
                        var->type = t2;
                        var->mem = (crtStruct || crtFunc) ? MEM_LOCAL : MEM_GLOBAL;

                        if (crtStruct) {
                            addSymbolPtr(&crtStruct->members, dupSymbol(var));
                        }
                    }
                } else {
                    tkerr(crtTk, "missing ID after comma");
                }
            }

            if (consume(SEMICOLON)) {
                return 1;
            } else {
                tkerr(crtTk, "missing ;");
            }
        } else {
            crtTk = startTk;
            return 0;
        }
    }

    crtTk = startTk;
    return 0;
}

int structDef() {
    Token *startTk = crtTk;
    Symbol *oldStruct;

    if (consume(STRUCT)) {
        if (consume(ID)) {
            Token *tkName = consumedTk;

            if (consume(LACC)) {
                if (findSymbolInDomain(&symbols, tkName->text)) {
                    tkerr(crtTk, "symbol redefinition: %s", tkName->text);
                }

                oldStruct = crtStruct;
                crtStruct = addSymbol(&symbols, tkName->text, CLS_STRUCT);
                crtStruct->mem = MEM_GLOBAL;
                crtStruct->type.typeBase = TB_STRUCT;
                crtStruct->type.s = crtStruct;
                crtStruct->type.nElements = -1;

                pushDomain();

                while (varDefCore(0)) {
                }

                if (consume(RACC)) {
                    if (consume(SEMICOLON)) {
                        dropDomain();
                        crtStruct = oldStruct;
                        return 1;
                    } else {
                        tkerr(crtTk, "missing ; after struct definition");
                    }
                } else {
                    tkerr(crtTk, "missing } in struct definition");
                }
            } else {
                // NO ERROR here, bcs variable declaration: struct Pt points[20/4+5]; --> varDef() tries again
                crtTk = startTk;
                return 0;
            }
        } else {
            tkerr(crtTk, "missing struct name");
        }
    }

    crtTk = startTk;
    return 0;
}

int fnParam() {
    Token *startTk = crtTk;
    Type t;

    if (typeBase(&t)) {
        if (consume(ID)) {
            Token *tkName = consumedTk;

            if (arrayDecl(&t)) {
                t.nElements = 0; /* int v[10] parameter becomes int v[] */
            }

            if (findSymbolInDomain(&symbols, tkName->text)) {
                tkerr(crtTk, "symbol redefinition: %s", tkName->text);
            }

            {
                Symbol *param = addSymbol(&symbols, tkName->text, CLS_VAR);
                param->mem = MEM_ARG;
                param->type = t;

                if (crtFunc) {
                    addSymbolPtr(&crtFunc->args, dupSymbol(param));
                }
            }

            return 1;
        } else {
            tkerr(crtTk, "missing parameter name");
        }
    }

    crtTk = startTk;
    return 0;
}

int fnDef() {
    Token *startTk = crtTk;
    Type t;
    Symbol *oldFunc;

    if (typeBase(&t) || consume(VOID)) {
        if (consumedTk->code == VOID) {
            t.typeBase = TB_VOID;
            t.s = NULL;
            t.nElements = -1;
        }

        if (consume(ID)) {
            Token *tkName = consumedTk;

            if (consume(LPAR)) {
                if (findSymbolInDomain(&symbols, tkName->text)) {
                    tkerr(crtTk, "symbol redefinition: %s", tkName->text);
                }

                oldFunc = crtFunc;
                crtFunc = addSymbol(&symbols, tkName->text, CLS_FUNC);
                crtFunc->mem = MEM_GLOBAL;
                crtFunc->type = t;

                pushDomain();

                if (fnParam()) {
                    while (consume(COMMA)) {
                        if (!fnParam()) {
                            tkerr(crtTk, "missing function parameter after ,");
                        }
                    }
                }

                if (consume(RPAR)) {
                    if (stmCompound(0)) {
                        dropDomain();
                        crtFunc = oldFunc;
                        return 1;
                    } else {
                        tkerr(crtTk, "missing function body");
                    }
                } else {
                    tkerr(crtTk, "missing ) in function definition");
                }
            } else {
                crtTk = startTk;
                return 0;
            }
        } else {
            crtTk = startTk;
            return 0;
        }
    }

    crtTk = startTk;
    return 0;
}

int stmCompound(int newDomain) {
    Token *startTk = crtTk;

    if (consume(LACC)) {
        if (newDomain) {
            pushDomain();
        }

        while (1) {
            if (varDef()) {
            } else if (stm()) {
            } else {
                break;
            }
        }

        if (consume(RACC)) {
            if (newDomain) {
                dropDomain();
            }
            return 1;
        } else {
            tkerr(crtTk, "missing } in compound statement");
        }
    }

    crtTk = startTk;
    return 0;
}

int stm() {
    Token *startTk = crtTk;

    if (stmCompound(1)) {
        return 1;
    }

    crtTk = startTk;
    if (consume(IF)) {
        if (consume(LPAR)) {
            if (expr()) {
                if (consume(RPAR)) {
                    if (stm()) {
                        if (consume(ELSE)) {
                            if (!stm()) {
                                tkerr(crtTk, "missing statement after else");
                            }
                        }
                        return 1;
                    } else {
                        tkerr(crtTk, "missing statement after if");
                    }
                } else {
                    tkerr(crtTk, "missing ) after if condition");
                }
            } else {
                tkerr(crtTk, "missing expression in if condition");
            }
        } else {
            tkerr(crtTk, "missing ( after if");
        }
    }

    crtTk = startTk;
    if (consume(WHILE)) {
        if (consume(LPAR)) {
            if (expr()) {
                if (consume(RPAR)) {
                    if (stm()) {
                        return 1;
                    } else {
                        tkerr(crtTk, "missing statement after while");
                    }
                } else {
                    tkerr(crtTk, "missing ) after while condition");
                }
            } else {
                tkerr(crtTk, "missing expression in while condition");
            }
        } else {
            tkerr(crtTk, "missing ( after while");
        }
    }

    crtTk = startTk;
    if (consume(FOR)) {
        if (consume(LPAR)) {
            expr(); // optional

            if (consume(SEMICOLON)) {
                expr(); // optional

                if (consume(SEMICOLON)) {
                    expr(); // optional

                    if (consume(RPAR)) {
                        if (stm()) {
                            return 1;
                        } else {
                            tkerr(crtTk, "missing statement after for");
                        }
                    } else {
                        tkerr(crtTk, "missing ) after for");
                    }
                } else {
                    tkerr(crtTk, "missing second ; in for");
                }
            } else {
                tkerr(crtTk, "missing first ; in for");
            }
        } else {
            tkerr(crtTk, "missing ( after for");
        }
    }

    crtTk = startTk;
    if (consume(BREAK)) {
        if (consume(SEMICOLON)) {
            return 1;
        } else {
            tkerr(crtTk, "missing ; after break");
        }
    }

    crtTk = startTk;
    if (consume(RETURN)) {
        expr(); // optional

        if (consume(SEMICOLON)) {
            return 1;
        } else {
            tkerr(crtTk, "missing ; after return");
        }
    }

    crtTk = startTk;
    if (consume(SEMICOLON)) {
        return 1;
    }

    crtTk = startTk;
    if (expr()) {
        if (consume(SEMICOLON)) {
            return 1;
        } else {
            tkerr(crtTk, "missing ;");
        }
    }

    crtTk = startTk;
    return 0;
}

int expr() {
    return exprAssign();
}

int exprAssign() {
    Token *startTk = crtTk;

    if (exprUnary()) {
        if (consume(ASSIGN)) {
            if (!exprAssign()) tkerr(crtTk, "missing expression after =");
            return 1;
        }
    }

    crtTk = startTk;
    return exprOr();
}

int exprOr() {
    if (exprAnd()) {
        if (exprOrAux()) {
            return 1;
        }
    }
    return 0;
}

/* exprOrAux: OR exprAnd exprOrAux | epsilon */
int exprOrAux() {
    if (consume(OR)) {
        if (exprAnd()) {
            if (exprOrAux()) {
                return 1;
            }
        }
        tkerr(crtTk, "missing expression after ||");
    }
    return 1; /* epsilon */
}

int exprAnd() {
    if (exprEq()) {
        if (exprAndAux()) {
            return 1;
        }
    }
    return 0;
}

/* exprAndAux: AND exprEq exprAndAux | epsilon */
int exprAndAux() {
    if (consume(AND)) {
        if (exprEq()) {
            if (exprAndAux()) {
                return 1;
            }
        }
        tkerr(crtTk, "missing expression after &&");
    }
    return 1; /* epsilon */
}

int exprEq() {
    if (exprRel()) {
        if (exprEqAux()) {
            return 1;
        }
    }
    return 0;
}

/* exprEqAux: (EQUAL | NOTEQ) exprRel exprEqAux | epsilon */
int exprEqAux() {
    if (consume(EQUAL) || consume(NOTEQ)) {
        if (exprRel()) {
            if (exprEqAux()) {
                return 1;
            }
        }
        tkerr(crtTk, "missing expression after equality operator");
    }
    return 1; /* epsilon */
}

int exprRel() {
    if (exprAdd()) {
        if (exprRelAux()) {
            return 1;
        }
    }
    return 0;
}

/* exprRelAux: (LESS | LESSEQ | GREATER | GREATEREQ) exprAdd exprRelAux | epsilon */
int exprRelAux() {
    if (consume(LESS) || consume(LESSEQ) || consume(GREATER) || consume(GREATEREQ)) {
        if (exprAdd()) {
            if (exprRelAux()) {
                return 1;
            }
        }
        tkerr(crtTk, "missing expression after relational operator");
    }
    return 1; /* epsilon */
}

int exprAdd() {
    if (exprMul()) {
        if (exprAddAux()) {
            return 1;
        }
    }
    return 0;
}

/* exprAddAux: (ADD | SUB) exprMul exprAddAux | epsilon */
int exprAddAux() {
    if (consume(ADD) || consume(SUB)) {
        if (exprMul()) {
            if (exprAddAux()) {
                return 1;
            }
        }
        tkerr(crtTk, "missing expression after + or -");
    }
    return 1; /* epsilon */
}

int exprMul() {
    if (exprCast()) {
        if (exprMulAux()) {
            return 1;
        }
    }
    return 0;
}

/* exprMulAux: (MUL | DIV) exprCast exprMulAux | epsilon */
int exprMulAux() {
    if (consume(MUL) || consume(DIV)) {
        if (exprCast()) {
            if (exprMulAux()) {
                return 1;
            }
        }
        tkerr(crtTk, "missing expression after * or /");
    }
    return 1; /* epsilon */
}

int exprCast() {
    Token *startTk = crtTk;
    Type t;

    if (consume(LPAR)) {
        if (typeBase(&t)) {
            arrayDecl(&t); /* optional */
            if (consume(RPAR)) {
                if (exprCast()) {
                    return 1;
                } else {
                    tkerr(crtTk, "missing expression after cast");
                }
            } else {
                tkerr(crtTk, "missing ) in cast expression");
            }
        }
    }

    crtTk = startTk;
    return exprUnary();
}

int exprUnary() {
    Token *startTk = crtTk;

    if (consume(SUB) || consume(NOT)) {
        if (!exprUnary()) tkerr(crtTk, "missing expression after unary operator");
        return 1;
    }

    crtTk = startTk;
    return exprPostfix();
}

int exprPostfix() {
    if (exprPrimary()) {
        if (exprPostfixAux()) {
            return 1;
        }
    }
    return 0;
}

/* exprPostfixAux: LBRACKET expr RBRACKET exprPostfixAux | DOT ID exprPostfixAux | epsilon */
int exprPostfixAux() {
    if (consume(LBRACKET)) {
        /* save/restore around inner expr() so nested expressions
         * don't clobber the postfix-chain's current type */
        Type savedType = crtExprType;
        int savedTyped = crtExprTyped;

        if (expr()) {
            if (consume(RBRACKET)) {
                /* indexing: result is element type (drop one array level) */
                crtExprType = savedType;
                crtExprTyped = savedTyped;
                if (crtExprTyped) {
                    crtExprType.nElements = -1;
                }
                if (exprPostfixAux()) {
                    return 1;
                }
            } else {
                tkerr(crtTk, "missing ] in postfix expression");
            }
        } else {
            tkerr(crtTk, "missing expression inside []");
        }
    }

    if (consume(DOT)) {
        if (consume(ID)) {
            Token *tkField = consumedTk;

            if (crtExprTyped) {
                if (crtExprType.typeBase != TB_STRUCT || !crtExprType.s) {
                    tkerr(tkField, "field access on non-struct type: %s", tkField->text);
                }
                {
                    Symbol *field = findSymbolInList(&crtExprType.s->members, tkField->text);
                    if (!field) {
                        tkerr(tkField, "undefined struct member: %s", tkField->text);
                    }
                    crtExprType = field->type;
                    crtExprTyped = 1;
                }
            }

            if (exprPostfixAux()) {
                return 1;
            }
        } else {
            tkerr(crtTk, "missing ID after .");
        }
    }

    return 1; /* epsilon */
}

int exprPrimary() {
    Token *startTk = crtTk;

    if (consume(ID)) {
        Token *tkName = consumedTk;
        Symbol *s = findSymbol(&symbols, tkName->text);

        if (!s) {
            tkerr(tkName, "undefined symbol: %s", tkName->text);
        }

        if (consume(LPAR)) {
            /* function call */
            if (s->cls != CLS_FUNC && s->cls != CLS_EXTFUNC) {
                tkerr(tkName, "not a function: %s", tkName->text);
            }

            if (expr()) {
                while (consume(COMMA)) {
                    if (!expr()) tkerr(crtTk, "missing expression after ,");
                }
            }
            if (!consume(RPAR)) tkerr(crtTk, "missing ) in function call");

            /* result type is function return type */
            crtExprType = s->type;
            crtExprTyped = 1;
        } else {
            /* plain identifier use */
            if (s->cls == CLS_FUNC || s->cls == CLS_EXTFUNC) {
                tkerr(tkName, "function used as value: %s", tkName->text);
            }
            if (s->cls == CLS_STRUCT) {
                tkerr(tkName, "struct name used as value: %s", tkName->text);
            }
            crtExprType = s->type;
            crtExprTyped = 1;
        }
        return 1;
    }

    crtTk = startTk;
    if (consume(CT_INT)) {
        crtExprType.typeBase = TB_INT;
        crtExprType.s = NULL;
        crtExprType.nElements = -1;
        crtExprTyped = 1;
        return 1;
    }

    crtTk = startTk;
    if (consume(CT_REAL)) {
        crtExprType.typeBase = TB_DOUBLE;
        crtExprType.s = NULL;
        crtExprType.nElements = -1;
        crtExprTyped = 1;
        return 1;
    }

    crtTk = startTk;
    if (consume(CT_CHAR)) {
        crtExprType.typeBase = TB_CHAR;
        crtExprType.s = NULL;
        crtExprType.nElements = -1;
        crtExprTyped = 1;
        return 1;
    }

    crtTk = startTk;
    if (consume(CT_STRING)) {
        crtExprType.typeBase = TB_CHAR;
        crtExprType.s = NULL;
        crtExprType.nElements = 0;
        crtExprTyped = 1;
        return 1;
    }

    crtTk = startTk;
    if (consume(LPAR)) {
        if (!expr()) tkerr(crtTk, "missing expression in ()");
        if (!consume(RPAR)) tkerr(crtTk, "missing )");
        /* crtExprType is whatever expr() set */
        return 1;
    }

    crtTk = startTk;
    return 0;
}
