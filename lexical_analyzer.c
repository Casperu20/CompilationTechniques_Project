#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>

enum{ID, END, CT_INT, ASSIGN, EQUAL, BREAK, CHAR}; // tokens codes

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
    int length = pEnd - pStart;
    char *str = (char *)malloc(length + 1);
    if (str == NULL) err("not enough memory");
    memcpy(str, pStart, length);
    str[length] = '\0';
    return str;
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

// loads file into a null-terminated string
char *loadFile(const char *fileName) {
    FILE *f = fopen(fileName, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = (char *)malloc(n + 1);
    if (!buf) err("not enough memory");
    fread(buf, 1, n, f);
    buf[n] = '\0';
    fclose(f);
    return buf;
}

void showTokens(Token *tokens) {
    for (Token *tk = tokens; tk != NULL; tk = tk->next) {
        printf("Line %d: Code %d", tk->line, tk->code);
        if (tk->code == ID) printf(" (%s)", tk->text);
        printf("\n");
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

    free(buffer);

    return 0;
}