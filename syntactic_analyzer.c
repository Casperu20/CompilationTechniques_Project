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
int exprAnd();
int exprEq();
int exprRel();
int exprAdd();
int exprMul();
int exprCast();
int exprUnary();
int exprPostfix();
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

int arrayDecl() {
    if (!consume(LBRACKET)) return 0;

    consume(CT_INT); // optional

    if (!consume(RBRACKET))
        tkerr(crtTk, "missing ]");

    return 1;
}

int varDef() {
    Token *startTk = crtTk;

    if (!typeBase()) return 0;

    if (!consume(ID)) {
        crtTk = startTk;
        return 0;
    }

    arrayDecl(); // optional

    // Handle comma-separated declarators (e.g., int i, v[5], s;)
    while (consume(COMMA)) {
        if (!consume(ID)) tkerr(crtTk, "missing ID after comma");
        arrayDecl(); // optional
    }

    if (!consume(SEMICOLON))
        tkerr(crtTk, "missing ;");

    return 1;
}

int structDef() {
    Token *startTk = crtTk;

    if (!consume(STRUCT)) return 0;
    if (!consume(ID)) {crtTk = startTk; return 0;}
    if (!consume(LACC)) {crtTk = startTk; return 0;}

    while (varDef()) {}

    if (!consume(RACC)) tkerr(crtTk, "missing } in struct definition");
    if (!consume(SEMICOLON)) tkerr(crtTk, "missing ; after struct definition");

    return 1;
}

int fnParam() {
    Token *startTk = crtTk;

    if (!typeBase()) return 0;
    if (!consume(ID)) {
        crtTk = startTk;
        return 0;
    }

    arrayDecl(); /* optional */
    return 1;
}

int fnDef() {
    Token *startTk = crtTk;

    if (typeBase() || consume(VOID)) {
        if (!consume(ID)) {
            crtTk = startTk;
            return 0;
        }

        if (!consume(LPAR)) {
            crtTk = startTk;
            return 0;
        }

        if (fnParam()) {
            while (consume(COMMA)) {
                if (!fnParam()) tkerr(crtTk, "missing function parameter after ,");
            }
        }

        if (!consume(RPAR)) tkerr(crtTk, "missing ) in function definition");
        if (!stmCompound()) tkerr(crtTk, "missing function body");
        return 1;
    }

    crtTk = startTk;
    return 0;
}

int stmCompound() {
    if (!consume(LACC)) return 0;

    while (1) {
        if (varDef()) {}
        else if (stm()) {}
        else break;
    }

    if (!consume(RACC)) tkerr(crtTk, "missing } in compound statement");
    return 1;
}

int stm() {
    Token *startTk = crtTk;

    if (stmCompound()) return 1;

    crtTk = startTk;
    if (consume(IF)) {
        if (!consume(LPAR)) tkerr(crtTk, "missing ( after if");
        if (!expr()) tkerr(crtTk, "missing expression in if condition");
        if (!consume(RPAR)) tkerr(crtTk, "missing ) after if condition");
        if (!stm()) tkerr(crtTk, "missing statement after if");
        if (consume(ELSE)) {
            if (!stm()) tkerr(crtTk, "missing statement after else");
        }
        return 1;
    }

    crtTk = startTk;
    if (consume(WHILE)) {
        if (!consume(LPAR)) tkerr(crtTk, "missing ( after while");
        if (!expr()) tkerr(crtTk, "missing expression in while condition");
        if (!consume(RPAR)) tkerr(crtTk, "missing ) after while condition");
        if (!stm()) tkerr(crtTk, "missing statement after while");
        return 1;
    }

    crtTk = startTk;
    if (consume(FOR)) {
        if (!consume(LPAR)) tkerr(crtTk, "missing ( after for");
        expr();
        if (!consume(SEMICOLON)) tkerr(crtTk, "missing first ; in for");
        expr();
        if (!consume(SEMICOLON)) tkerr(crtTk, "missing second ; in for");
        expr();
        if (!consume(RPAR)) tkerr(crtTk, "missing ) after for");
        if (!stm()) tkerr(crtTk, "missing statement after for");
        return 1;
    }

    crtTk = startTk;
    if (consume(BREAK)) {
        if (!consume(SEMICOLON)) tkerr(crtTk, "missing ; after break");
        return 1;
    }

    crtTk = startTk;
    if (consume(RETURN)) {
        expr();
        if (!consume(SEMICOLON)) tkerr(crtTk, "missing ; after return");
        return 1;
    }

    crtTk = startTk;
    if (consume(SEMICOLON)) return 1;

    if (expr()) {
        if (!consume(SEMICOLON)) tkerr(crtTk, "missing ;");
        return 1;
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
    if (!exprAnd()) return 0;
    while (consume(OR)) {
        if (!exprAnd()) tkerr(crtTk, "missing expression after ||");
    }
    return 1;
}

int exprAnd() {
    if (!exprEq()) return 0;
    while (consume(AND)) {
        if (!exprEq()) tkerr(crtTk, "missing expression after &&");
    }
    return 1;
}

int exprEq() {
    if (!exprRel()) return 0;
    while (1) {
        if (consume(EQUAL) || consume(NOTEQ)) {
            if (!exprRel()) tkerr(crtTk, "missing expression in equality operator");
        } else {
            break;
        }
    }
    return 1;
}

int exprRel() {
    if (!exprAdd()) return 0;
    while (1) {
        if (consume(LESS) || consume(LESSEQ) || consume(GREATER) || consume(GREATEREQ)) {
            if (!exprAdd()) tkerr(crtTk, "missing expression in relational operator");
        } else {
            break;
        }
    }
    return 1;
}

int exprAdd() {
    if (!exprMul()) return 0;
    while (1) {
        if (consume(ADD) || consume(SUB)) {
            if (!exprMul()) tkerr(crtTk, "missing expression after + or -");
        } else {
            break;
        }
    }
    return 1;
}

int exprMul() {
    if (!exprCast()) return 0;
    while (1) {
        if (consume(MUL) || consume(DIV)) {
            if (!exprCast()) tkerr(crtTk, "missing expression after * or /");
        } else {
            break;
        }
    }
    return 1;
}

int exprCast() {
    Token *startTk = crtTk;

    if (consume(LPAR)) {
        if (typeBase()) {
            arrayDecl(); /* optional */
            if (!consume(RPAR)) tkerr(crtTk, "missing ) in cast expression");
            if (!exprCast()) tkerr(crtTk, "missing expression after cast");
            return 1;
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
    if (!exprPrimary()) return 0;

    while (1) {
        Token *startTk = crtTk;

        if (consume(LBRACKET)) {
            if (!expr()) tkerr(crtTk, "missing expression inside []");
            if (!consume(RBRACKET)) tkerr(crtTk, "missing ] in postfix expression");
            continue;
        }

        crtTk = startTk;
        if (consume(DOT)) {
            if (!consume(ID)) tkerr(crtTk, "missing ID after .");
            continue;
        }

        crtTk = startTk;
        break;
    }

    return 1;
}

int exprPrimary() {
    Token *startTk = crtTk;

    if (consume(ID)) {
        if (consume(LPAR)) {
            if (expr()) {
                while (consume(COMMA)) {
                    if (!expr()) tkerr(crtTk, "missing expression after ,");
                }
            }
            if (!consume(RPAR)) tkerr(crtTk, "missing ) in function call");
        }
        return 1;
    }

    crtTk = startTk;
    if (consume(CT_INT) || consume(CT_REAL) || consume(CT_CHAR) || consume(CT_STRING)) return 1;

    crtTk = startTk;
    if (consume(LPAR)) {
        if (!expr()) tkerr(crtTk, "missing expression in ()");
        if (!consume(RPAR)) tkerr(crtTk, "missing )");
        return 1;
    }

    crtTk = startTk;
    return 0;
}

