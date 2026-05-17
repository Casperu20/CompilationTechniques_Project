#include <stdio.h>
#include "atomc.h"
#include "domain_analysis.h"
#include "type_analyzer.h"

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
int expr(Ret *r);
int exprAssign(Ret *r);
int exprOr(Ret *r);
int exprOrAux(Ret *r);
int exprAnd(Ret *r);
int exprAndAux(Ret *r);
int exprEq(Ret *r);
int exprEqAux(Ret *r);
int exprRel(Ret *r);
int exprRelAux(Ret *r);
int exprAdd(Ret *r);
int exprAddAux(Ret *r);
int exprMul(Ret *r);
int exprMulAux(Ret *r);
int exprCast(Ret *r);
int exprUnary(Ret *r);
int exprPostfix(Ret *r);
int exprPostfixAux(Ret *r);
int exprPrimary(Ret *r);

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
    addExtFuncs();

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
    Ret rSize;

    if (consume(LBRACKET)) {
        startExpr = crtTk;

        if (consume(CT_INT)) {
            if (crtTk->code == RBRACKET) {
                t->nElements = (int)consumedTk->i;
            } else {
                crtTk = startExpr;
                if (!expr(&rSize)) {
                    tkerr(crtTk, "invalid array size expression");
                }
                if (!canBeScalar(&rSize)) {
                    tkerr(crtTk, "array size expression must be scalar");
                }
                {
                    Type tInt = createType(TB_INT, NULL, -1);
                    if (!convTo(&rSize.type, &tInt)) {
                        tkerr(crtTk, "array size expression must be convertible to int");
                    }
                }
                t->nElements = -2; /* expression-sized array */
            }
        } else if (expr(&rSize)) {
            if (!canBeScalar(&rSize)) {
                tkerr(crtTk, "array size expression must be scalar");
            }
            {
                Type tInt = createType(TB_INT, NULL, -1);
                if (!convTo(&rSize.type, &tInt)) {
                    tkerr(crtTk, "array size expression must be convertible to int");
                }
            }
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
                Ret rInit;
                if (!expr(&rInit)) {
                    tkerr(crtTk, "missing expression after =");
                }
                if (!convTo(&rInit.type, &t)) {
                    tkerr(crtTk, "initializer cannot be converted to variable type");
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
                        Ret rInit;
                        if (!expr(&rInit)) {
                            tkerr(crtTk, "missing expression after =");
                        }
                        if (!convTo(&rInit.type, &t2)) {
                            tkerr(crtTk, "initializer cannot be converted to variable type");
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
        Ret rCond;
        if (consume(LPAR)) {
            if (expr(&rCond)) {
                if (!canBeScalar(&rCond)) {
                    tkerr(crtTk, "the if condition must be a scalar value");
                }
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
        Ret rCond;
        if (consume(LPAR)) {
            if (expr(&rCond)) {
                if (!canBeScalar(&rCond)) {
                    tkerr(crtTk, "the while condition must be a scalar value");
                }
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
        Ret rInit, rCond, rStep;
        if (consume(LPAR)) {
            expr(&rInit); // optional

            if (consume(SEMICOLON)) {
                if (expr(&rCond)) { // optional
                    if (!canBeScalar(&rCond)) {
                        tkerr(crtTk, "the for condition must be a scalar value");
                    }
                }

                if (consume(SEMICOLON)) {
                    expr(&rStep); // optional

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
        Ret rExpr;
        if (expr(&rExpr)) {
            if (!crtFunc) {
                tkerr(crtTk, "return outside function");
            }
            if (crtFunc->type.typeBase == TB_VOID) {
                tkerr(crtTk, "a void function cannot return a value");
            }
            if (!canBeScalar(&rExpr)) {
                tkerr(crtTk, "the return value must be a scalar value");
            }
            if (!convTo(&rExpr.type, &crtFunc->type)) {
                tkerr(crtTk, "cannot convert the return expression type to the function return type");
            }
        } else {
            if (crtFunc && crtFunc->type.typeBase != TB_VOID) {
                tkerr(crtTk, "a non-void function must return a value");
            }
        }

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
    {
        Ret rExpr;
        if (expr(&rExpr)) {
            if (consume(SEMICOLON)) {
                return 1;
            } else {
                tkerr(crtTk, "missing ;");
            }
        }
    }

    crtTk = startTk;
    return 0;
}

int expr(Ret *r) {
    return exprAssign(r);
}

int exprAssign(Ret *r) {
    Token *startTk = crtTk;
    Ret rDst;

    if (exprUnary(&rDst)) {
        if (consume(ASSIGN)) {
            if (!exprAssign(r)) tkerr(crtTk, "missing expression after =");
            if (!rDst.lval) tkerr(crtTk, "the assign destination must be a left-value");
            if (rDst.ct) tkerr(crtTk, "the assign destination cannot be constant");
            if (!canBeScalar(&rDst)) tkerr(crtTk, "the assign destination must be scalar");
            if (!canBeScalar(r)) tkerr(crtTk, "the assign source must be scalar");
            if (!convTo(&r->type, &rDst.type)) tkerr(crtTk, "the assign source cannot be converted to destination");
            r->lval = 0;
            r->ct = 0;
            return 1;
        }
    }

    crtTk = startTk;
    return exprOr(r);
}

int exprOr(Ret *r) {
    if (exprAnd(r)) {
        if (exprOrAux(r)) {
            return 1;
        }
    }
    return 0;
}

int exprOrAux(Ret *r) {
    if (consume(OR)) {
        Ret right;
        Type tDst;
        if (exprAnd(&right)) {
            if (!arithTypeTo(&r->type, &right.type, &tDst)) tkerr(crtTk, "invalid operand type for ||");
            *r = (Ret){ createType(TB_INT, NULL, -1), 0, 0, {0} };
            if (exprOrAux(r)) return 1;
        }
        tkerr(crtTk, "missing expression after ||");
    }
    return 1;
}

int exprAnd(Ret *r) {
    if (exprEq(r)) {
        if (exprAndAux(r)) {
            return 1;
        }
    }
    return 0;
}

int exprAndAux(Ret *r) {
    if (consume(AND)) {
        Ret right;
        Type tDst;
        if (exprEq(&right)) {
            if (!arithTypeTo(&r->type, &right.type, &tDst)) tkerr(crtTk, "invalid operand type for &&");
            *r = (Ret){ createType(TB_INT, NULL, -1), 0, 0, {0} };
            if (exprAndAux(r)) return 1;
        }
        tkerr(crtTk, "missing expression after &&");
    }
    return 1;
}

int exprEq(Ret *r) {
    if (exprRel(r)) {
        if (exprEqAux(r)) {
            return 1;
        }
    }
    return 0;
}

int exprEqAux(Ret *r) {
    if (consume(EQUAL) || consume(NOTEQ)) {
        Ret right;
        Type tDst;
        if (exprRel(&right)) {
            if (!arithTypeTo(&r->type, &right.type, &tDst)) tkerr(crtTk, "invalid operand type for == or !=");
            *r = (Ret){ createType(TB_INT, NULL, -1), 0, 0, {0} };
            if (exprEqAux(r)) return 1;
        }
        tkerr(crtTk, "missing expression after equality operator");
    }
    return 1;
}

int exprRel(Ret *r) {
    if (exprAdd(r)) {
        if (exprRelAux(r)) {
            return 1;
        }
    }
    return 0;
}

int exprRelAux(Ret *r) {
    if (consume(LESS) || consume(LESSEQ) || consume(GREATER) || consume(GREATEREQ)) {
        Ret right;
        Type tDst;
        if (exprAdd(&right)) {
            if (!arithTypeTo(&r->type, &right.type, &tDst)) tkerr(crtTk, "invalid operand type for <, <=, >, >=");
            *r = (Ret){ createType(TB_INT, NULL, -1), 0, 0, {0} };
            if (exprRelAux(r)) return 1;
        }
        tkerr(crtTk, "missing expression after relational operator");
    }
    return 1;
}

int exprAdd(Ret *r) {
    if (exprMul(r)) {
        if (exprAddAux(r)) {
            return 1;
        }
    }
    return 0;
}

int exprAddAux(Ret *r) {
    if (consume(ADD) || consume(SUB)) {
        Ret right;
        Type tDst;
        if (exprMul(&right)) {
            if (!arithTypeTo(&r->type, &right.type, &tDst)) tkerr(crtTk, "invalid operand type for + or -");
            *r = (Ret){ tDst, 0, 0, {0} };
            if (exprAddAux(r)) return 1;
        }
        tkerr(crtTk, "missing expression after + or -");
    }
    return 1;
}

int exprMul(Ret *r) {
    if (exprCast(r)) {
        if (exprMulAux(r)) {
            return 1;
        }
    }
    return 0;
}

int exprMulAux(Ret *r) {
    if (consume(MUL) || consume(DIV)) {
        Ret right;
        Type tDst;
        if (exprCast(&right)) {
            if (!arithTypeTo(&r->type, &right.type, &tDst)) tkerr(crtTk, "invalid operand type for * or /");
            *r = (Ret){ tDst, 0, 0, {0} };
            if (exprMulAux(r)) return 1;
        }
        tkerr(crtTk, "missing expression after * or /");
    }
    return 1;
}

int exprCast(Ret *r) {
    Token *startTk = crtTk;
    Type t;
    Ret op;

    if (consume(LPAR)) {
        if (typeBase(&t)) {
            arrayDecl(&t); /* optional */
            if (consume(RPAR)) {
                if (exprCast(&op)) {
                    if (t.typeBase == TB_STRUCT) tkerr(crtTk, "cannot convert to a struct type");
                    if (op.type.typeBase == TB_STRUCT) tkerr(crtTk, "cannot convert a struct");
                    if (isArrayType(&op.type) && !isArrayType(&t)) tkerr(crtTk, "an array can be converted only to another array");
                    if (!isArrayType(&op.type) && isArrayType(&t)) tkerr(crtTk, "a scalar can be converted only to another scalar");
                    *r = (Ret){ t, 0, 0, {0} };
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
    return exprUnary(r);
}

int exprUnary(Ret *r) {
    Token *startTk = crtTk;

    if (consume(SUB)) {
        if (!exprUnary(r)) tkerr(crtTk, "missing expression after unary operator");
        if (!canBeScalar(r)) tkerr(crtTk, "unary - must have a scalar operand");
        r->lval = 0;
        r->ct = 0;
        return 1;
    }

    crtTk = startTk;
    if (consume(NOT)) {
        if (!exprUnary(r)) tkerr(crtTk, "missing expression after unary operator");
        if (!canBeScalar(r)) tkerr(crtTk, "unary ! must have a scalar operand");
        *r = (Ret){ createType(TB_INT, NULL, -1), 0, 0, {0} };
        return 1;
    }

    crtTk = startTk;
    return exprPostfix(r);
}

int exprPostfix(Ret *r) {
    if (exprPrimary(r)) {
        if (exprPostfixAux(r)) {
            return 1;
        }
    }
    return 0;
}

int exprPostfixAux(Ret *r) {
    if (consume(LBRACKET)) {
        Ret idx;
        if (expr(&idx)) {
            if (consume(RBRACKET)) {
                Type tInt = createType(TB_INT, NULL, -1);
                if (!isArrayType(&r->type)) tkerr(crtTk, "only an array can be indexed");
                if (!convTo(&idx.type, &tInt)) tkerr(crtTk, "the index is not convertible to int");
                r->type.nElements = -1;
                r->lval = 1;
                r->ct = 0;
                if (exprPostfixAux(r)) return 1;
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
            Symbol *field;
            if (r->type.typeBase != TB_STRUCT || !r->type.s) {
                tkerr(tkField, "a field can only be selected from a struct");
            }
            field = findSymbolInList(&r->type.s->members, tkField->text);
            if (!field) {
                tkerr(tkField, "the structure %s does not have a field %s", r->type.s->name, tkField->text);
            }
            *r = (Ret){ field->type, 1, isArrayType(&field->type), {0} };
            if (exprPostfixAux(r)) return 1;
        } else {
            tkerr(crtTk, "missing ID after .");
        }
    }

    return 1;
}

int exprPrimary(Ret *r) {
    Token *startTk = crtTk;

    if (consume(ID)) {
        Token *tkName = consumedTk;
        Symbol *s = findSymbol(&symbols, tkName->text);

        if (!s) {
            tkerr(tkName, "undefined symbol: %s", tkName->text);
        }

        if (consume(LPAR)) {
            Symbol **param;
            if (s->cls != CLS_FUNC && s->cls != CLS_EXTFUNC) {
                tkerr(tkName, "not a function: %s", tkName->text);
            }

            param = s->args.begin;
            if (crtTk->code != RPAR) {
                Ret rArg;
                if (!expr(&rArg)) tkerr(crtTk, "missing expression in function call");
                if (param == s->args.end) tkerr(tkName, "too many arguments in function call");
                if (!convTo(&rArg.type, &(*param)->type)) tkerr(tkName, "in call, cannot convert the argument type to the parameter type");
                param++;
                while (consume(COMMA)) {
                    if (!expr(&rArg)) tkerr(crtTk, "missing expression after ,");
                    if (param == s->args.end) tkerr(tkName, "too many arguments in function call");
                    if (!convTo(&rArg.type, &(*param)->type)) tkerr(tkName, "in call, cannot convert the argument type to the parameter type");
                    param++;
                }
            }
            if (!consume(RPAR)) tkerr(crtTk, "missing ) in function call");
            if (param != s->args.end) tkerr(tkName, "too few arguments in function call");

            *r = (Ret){ s->type, 0, 0, {0} };
        } else {
            if (s->cls == CLS_FUNC || s->cls == CLS_EXTFUNC) {
                tkerr(tkName, "a function can only be called: %s", tkName->text);
            }
            if (s->cls == CLS_STRUCT) {
                tkerr(tkName, "struct name used as value: %s", tkName->text);
            }
            *r = (Ret){ s->type, 1, isArrayType(&s->type), {0} };
        }
        return 1;
    }

    crtTk = startTk;
    if (consume(CT_INT)) {
        *r = (Ret){ createType(TB_INT, NULL, -1), 0, 1, {0} };
        r->ctVal.i = consumedTk->i;
        return 1;
    }

    crtTk = startTk;
    if (consume(CT_REAL)) {
        *r = (Ret){ createType(TB_DOUBLE, NULL, -1), 0, 1, {0} };
        r->ctVal.d = consumedTk->r;
        return 1;
    }

    crtTk = startTk;
    if (consume(CT_CHAR)) {
        *r = (Ret){ createType(TB_CHAR, NULL, -1), 0, 1, {0} };
        r->ctVal.i = consumedTk->i;
        return 1;
    }

    crtTk = startTk;
    if (consume(CT_STRING)) {
        *r = (Ret){ createType(TB_CHAR, NULL, 0), 0, 1, {0} };
        r->ctVal.str = consumedTk->text;
        return 1;
    }

    crtTk = startTk;
    if (consume(LPAR)) {
        if (!expr(r)) tkerr(crtTk, "missing expression in ()");
        if (!consume(RPAR)) tkerr(crtTk, "missing )");
        return 1;
    }

    crtTk = startTk;
    return 0;
}
