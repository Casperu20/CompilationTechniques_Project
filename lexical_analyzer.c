#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>

enum{ID, END,
    CT_INT, CT_REAL, CT_CHAR, CT_STRING,
    BREAK, CHAR, DOUBLE, ELSE, FOR, IF, INT, RETURN, STRUCT, VOID, WHILE,
    COMMA, SEMICOLON, LPAR, RPAR, LBRACKET, RBRACKET, LACC, RACC,
    ADD, SUB, MUL, DIV, DOT,
    AND, OR, NOT,
    ASSIGN, EQUAL, NOTEQ, LESS, LESSEQ, GREATER, GREATEREQ
}; // tokens codes

typedef struct _Token{
    int code; // code (name)
    union{
        char *text; // used for ID, CT_STRING (dynamically allocated)
        long int i; // used for CT_INT, CT_CHAR
        double r; // used for CT_REAL
    };
    int line; // the input file line
    struct _Token *next; // link to the next token
}Token;

const char *pCrtCh;     // pointer to current character in the input string 
int line = 1;           // current line number 
Token *tokens = NULL;   // start of the tokens list 
Token *lastToken = NULL;// end of it

#define SAFEALLOC(var, Type) if((var = (Type*)malloc(sizeof(Type))) == NULL) err("not enough memory");

void err(const char *fmt,...){
    va_list va;
    va_start(va, fmt);
    fprintf(stderr, "error in line %d: ", line);
    vfprintf(stderr, fmt, va);
    fputc('\n', stderr);
    va_end(va);
    exit(-1);
}

void tkerr(const Token *tk,const char *fmt,...){
    va_list va;
    va_start(va, fmt);
    fprintf(stderr, "error in line %d: ", tk ? tk->line : line);
    vfprintf(stderr, fmt,va);
    fputc('\n', stderr);
    va_end(va);
    exit(-1);
}

Token *addTk(int code){
    Token *tk;
    SAFEALLOC(tk,Token)
    tk->code = code;
    tk->line = line;
    tk->next = NULL;
    if(lastToken){
        lastToken -> next=tk;
    }   
    else{
        tokens = tk;
    }
    lastToken = tk;
    return tk;
}

char *createString(const char *pStart, const char *pEnd) { // helper func to store identifier names and a way to load the file into a buffer
    int length = (int) (pEnd - pStart);
    char *str = (char *)malloc(length + 1);
    if (str == NULL) err("not enough memory");
    memcpy(str, pStart, (size_t)length);
    str[length] = '\0';
    return str;
}

int getNextToken() {
    int nCh;
    char ch;
    const char *pStartCh;
    Token *tk;

    while (1) {
        ch = *pCrtCh;

        if (ch == ' ' || ch == '\r' || ch == '\t') {
            pCrtCh++;
            continue;
        }
        if (ch == '\n') {
            line++;
            pCrtCh++;
            continue;
        }
        if (ch == 0) {
            addTk(END);
            return END;
        }

        if (isalpha((unsigned char)ch) || ch == '_') {
            pStartCh = pCrtCh;
            pCrtCh++;
            while(isalnum((unsigned char)*pCrtCh) || *pCrtCh == '_') pCrtCh++;
            nCh = (int)(pCrtCh - pStartCh);

            int kw = isKeyword(pStartCh, nCh);
            if(kw != -1){
                addTk(kw);
                return kw;
            }

            tk = addTk(ID);
            tk->text = createString(pStartCh, pCrtCh);
            return ID;
        }

        if(isdigit((unsigned char)ch)){
            char *endptr;

            if(ch == '0' && (pCrtCh[1] == 'x' || pCrtCh[1] == 'X')){
                pStartCh = pCrtCh;
                pCrtCh += 2;
                if(!isxdigit((unsigned char)*pCrtCh)){
                    tkerr(NULL, "invalid hexadecimal constant");
                }
                while(isxdigit((unsigned char)*pCrtCh)) pCrtCh++;
                tk = addTk(CT_INT);
                tk->i = strtol(pStartCh, &endptr, 0);
                return CT_INT;
            }

            pStartCh = pCrtCh;
            while(isdigit((unsigned char)*pCrtCh)) pCrtCh++;

            if(*pCrtCh == '.' || *pCrtCh == 'e' || *pCrtCh == 'E'){
                if(*pCrtCh == '.'){
                    pCrtCh++;
                    if(!isdigit((unsigned char)*pCrtCh)){
                        tkerr(NULL, "invalid real constant");
                    }
                    while(isdigit((unsigned char)*pCrtCh)) pCrtCh++;
                }
                if(*pCrtCh == 'e' || *pCrtCh == 'E'){
                    pCrtCh++;
                    if(*pCrtCh == '+' || *pCrtCh == '-') pCrtCh++;
                    if(!isdigit((unsigned char)*pCrtCh)){
                        tkerr(NULL, "invalid exponent in real constant");
                    }
                    while(isdigit((unsigned char)*pCrtCh)) pCrtCh++;
                }

                tk = addTk(CT_REAL);
                tk->r = strtod(pStartCh, &endptr);
                return CT_REAL;
            }

            if(pStartCh[0] == '0' && (pCrtCh - pStartCh) > 1){
                const char *p = pStartCh + 1;
                while(p < pCrtCh){
                    if(*p < '0' || *p > '7'){
                        tkerr(NULL, "invalid octal constant");
                    }
                    p++;
                }
            }

            tk = addTk(CT_INT);
            tk->i = strtol(pStartCh, &endptr, 0);
            return CT_INT;
        }

        switch (state) {
            case 0:
                if (isalpha(ch) || ch == '_') {
                    pStartCh = pCrtCh;
                    pCrtCh++;
                    state = 1;
                } else if (ch == '=') {
                    pCrtCh++;
                    state = 3;
                } else if (ch == ' ' || ch == '\r' || ch == '\t') {
                    pCrtCh++;
                } else if (ch == '\n') {
                    line++;
                    pCrtCh++;
                } else if (ch == 0) {
                    addTk(END);
                    return END;
                } else {
                    tkerr(addTk(END), "invalid character");
                }
                break;

            case 1:
                if (isalnum(ch) || ch == '_') {
                    pCrtCh++;
                } else {
                    state = 2;
                }
                break;

            case 2:
                nCh = pCrtCh - pStartCh;
                tk = (nCh == 5 && !memcmp(pStartCh, "break", 5)) ? addTk(BREAK) :
                     (nCh == 4 && !memcmp(pStartCh, "char", 4)) ? addTk(CHAR) :
                     NULL;
                
                if (!tk) {
                    tk = addTk(ID);
                    tk->text = createString(pStartCh, pCrtCh);
                }
                return tk->code;

            case 3:
                state = (ch == '=') ? 4 : 5;
                if (state == 4) pCrtCh++;
                break;

            case 4:
                addTk(EQUAL);
                return EQUAL;

            case 5:
                addTk(ASSIGN);
                return ASSIGN;
        }
    }
}

// loads file into a null-terminated string
char *loadFile(const char *fileName) {
    FILE *f = fopen(fileName, "rb");
    long n;
    char *buf;

    if (!f) return NULL;
    if(fseek(f, 0, SEEK_END) != 0){
        fclose(f);
        return NULL;
    }

    n = ftell(f);
    if (n < 0) {
        fclose(f);
        return NULL;
    }
    if(fseek(f, 0, SEEK_SET) != 0){
        fclose(f);
        return NULL;
    }

    buf = (char *)malloc((size_t)n + 1);
    if (!buf) err("not enough memory");

    if(fread(buf, 1, (size_t)n, f) != (size_t)n){   
        free(buf);
        fclose(f);
        return NULL;
    }
    buf[n] = '\0';
    fclose(f);
    return buf;
}

const char *tokenName(int code) {
    switch (code) {
        case ID: return "ID";
        case END: return "END";
        case CT_INT: return "CT_INT";
        case CT_REAL: return "CT_REAL";
        case CT_CHAR: return "CT_CHAR";
        case CT_STRING: return "CT_STRING";
        case BREAK: return "BREAK";
        case CHAR: return "CHAR";
        case DOUBLE: return "DOUBLE";
        case ELSE: return "ELSE";
        case FOR: return "FOR";
        case IF: return "IF";
        case INT: return "INT";
        case RETURN: return "RETURN";
        case STRUCT: return "STRUCT";
        case VOID: return "VOID";
        case WHILE: return "WHILE";
        case COMMA: return "COMMA";
        case SEMICOLON: return "SEMICOLON";
        case LPAR: return "LPAR";
        case RPAR: return "RPAR";
        case LBRACKET: return "LBRACKET";
        case RBRACKET: return "RBRACKET";
        case LACC: return "LACC";
        case RACC: return "RACC";
        case ADD: return "ADD";
        case SUB: return "SUB";
        case MUL: return "MUL";
        case DIV: return "DIV";
        case DOT: return "DOT";
        case AND: return "AND";
        case OR: return "OR";
        case NOT: return "NOT";
        case ASSIGN: return "ASSIGN";
        case EQUAL: return "EQUAL";
        case NOTEQ: return "NOTEQ";
        case LESS: return "LESS";
        case LESSEQ: return "LESSEQ";
        case GREATER: return "GREATER";
        case GREATEREQ: return "GREATEREQ";
        default: return "UNKNOWN";
    }
}

void showTokens(Token *tokens) {
    Token *tk;
    for (tk = tokens; tk != NULL; tk = tk->next) {
        printf("Line %d:\t %s", tk->line, tokenName(tk->code));
        switch (tk->code) {
            case ID:
            case CT_STRING:
                printf(":%s", tk->text);
                break;
            case CT_INT:
            case CT_CHAR:
                printf(":%ld", tk->i);
                break;
            case CT_REAL:
                printf(":%g", tk->r);
                break;
        }
        // if (tk->code == ID) printf(" (%s)", tk->text);
        printf("\n");
    }
}

void freeTokens(Token *tokens) {
    Token *tk = tokens, *aux;
    while(tk){
        aux = tk;
        tk = tk->next;
        if(aux->code == ID || aux->code == CT_STRING){
            free(aux->text);
        }
        free(aux);
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s <source_file>\n", argv[0]);
        return 1;
    }

    char *buffer = loadFile(argv[1]);
    if (!buffer) {
        fprintf(stderr, "Could not open file %s\n", argv[1]);
        return 1;
    }

    pCrtCh = buffer; // initialize the pointer to the start of the file string

    while (getNextToken() != END); // fill the tokens list 

    showTokens(tokens); 

    freeTokens(tokens);
    free(buffer);

    return 0;
}