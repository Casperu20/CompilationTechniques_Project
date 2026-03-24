#include <stdio.h>

enum{ID, END, CT_INT}; // tokens codes

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

#define SAFEALLOC(var, Type) if((var = (Type*)malloc(sizeof(Type))) == NULL) err("not enough memory");

void err(const char *fmt,...){
    va_list va;
    va_start(va, fmt);
    fprintf(stderr, "error in line %d: ", tk ? tk->line : line);
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

int getNextToken() {
    int state = 0, nCh;
    char ch;
    const char *pStartCh;
    Token *tk;

    while (1) {
        ch = *pCrtCh;

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



int main(){



    return 0;
}