#include <stdio.h>
#include "atomc.h"

/* parser globals */
Token *crtTk;
Token *consumedTk;

/* forward declarations */
int unit();
int structDef();
int varDef();
int typeBase();
int arrayDecl();
int fnDef();
int fnParam();
int stm();
int stmCompound();
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

    if (!unit()) {
        tkerr(crtTk, "syntax error");
    }

    printf("Syntax OK\n");
}

int unit() {
    while (1) {
        if (structDef()) {}
        else if (fnDef()) {}
        else if (varDef()) {}
        else break;
    }

    if (!consume(END))
        tkerr(crtTk, "missing END");

    return 1;
}

int typeBase() {
    if (consume(INT)) return 1;
    if (consume(DOUBLE)) return 1;
    if (consume(CHAR)) return 1;

    if (consume(STRUCT)) {
        if (!consume(ID))
            tkerr(crtTk, "missing ID after struct");
        return 1;
    }

    return 0;
}

// as well improvement to accept exprs inside
int arrayDecl() {
    if (!consume(LBRACKET)) return 0;

    // consume(CT_INT); // optional
    expr(); // we may have nothing, 20, or 20/4 or n+1 idk

    if (!consume(RBRACKET))
        tkerr(crtTk, "missing ]");

    return 1;
}

// improvement as in C we should have: varDef: typeBase ID arrayDecl? (ASSIGN expr)? SEMICOLON
int varDef() {
    Token *startTk = crtTk;

    if (typeBase()) {
        if (consume(ID)) {
            arrayDecl(); // ? = optional

            // optional initialization: int b = 5 + 3 * 2 etc
            if (consume(ASSIGN)) {
                if (!expr()) {
                    tkerr(crtTk, "missing expression after =");
                }
            }

            // comma-separated declarators --> int i, v[5], s;
            while (consume(COMMA)) {
                if (consume(ID)) {
                    arrayDecl(); // ? = optional

                    // optional initializer --> int i = 5, v[5], s = "hello";
                    // {1,2,3} are not supported by the given grammar
                    if (consume(ASSIGN)) {
                        if (!expr()) {
                            tkerr(crtTk, "missing expression after =");
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

    if (consume(STRUCT)) {
        if (consume(ID)) {
            if (consume(LACC)) {
                while (varDef()) {
                }

                if (consume(RACC)) {
                    if (consume(SEMICOLON)) {
                        return 1;
                    } else {
                        tkerr(crtTk, "missing ; after struct definition");
                    }
                } else {
                    tkerr(crtTk, "missing } in struct definition");
                }
            } else {
                // NO ERROR here, bcs variable declaration: struct Pt points[20/4+5]; -->  varDef() try again
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

    if (typeBase()) {
        if (consume(ID)) {
            arrayDecl(); // optional
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

    if (typeBase() || consume(VOID)) {
        if (consume(ID)) {
            if (consume(LPAR)) {
                if (fnParam()) {
                    while (consume(COMMA)) {
                        if (!fnParam()) {
                            tkerr(crtTk, "missing function parameter after ,");
                        }
                    }
                }

                if (consume(RPAR)) {
                    if (stmCompound()) {
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

int stmCompound() {
    Token *startTk = crtTk;

    if (consume(LACC)) {
        while (1) {
            if (varDef()) {
            } else if (stm()) {
            } else {
                break;
            }
        }

        if (consume(RACC)) {
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

    if (stmCompound()) {
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

    if (consume(LPAR)) {
        if (typeBase()) {
            arrayDecl(); // optional

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

        crtTk = startTk;
    }

    if (exprUnary()) {
        return 1;
    }

    crtTk = startTk;
    return 0;
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
        if (expr()) {
            if (consume(RBRACKET)) {
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
        if (consume(LPAR)) {
            if (expr()) {
                while (consume(COMMA)) {
                    if (!expr()) {
                        tkerr(crtTk, "missing expression after ,");
                    }
                }
            }

            if (consume(RPAR)) {
                return 1;
            } else {
                tkerr(crtTk, "missing ) in function call");
            }
        }

        return 1;
    }

    crtTk = startTk;
    if (consume(CT_INT)) {
        return 1;
    }

    crtTk = startTk;
    if (consume(CT_REAL)) {
        return 1;
    }

    crtTk = startTk;
    if (consume(CT_CHAR)) {
        return 1;
    }

    crtTk = startTk;
    if (consume(CT_STRING)) {
        return 1;
    }

    crtTk = startTk;
    if (consume(LPAR)) {
        if (expr()) {
            if (consume(RPAR)) {
                return 1;
            } else {
                tkerr(crtTk, "missing )");
            }
        } else {
            tkerr(crtTk, "missing expression in ()");
        }
    }

    crtTk = startTk;
    return 0;
}

