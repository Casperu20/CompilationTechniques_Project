#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include "atomc.h"

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

static int isKeyword(const char *pStartCh, int nCh){
    if(nCh == 5 && !memcmp(pStartCh, "break", 5)) return BREAK;
    if(nCh == 4 && !memcmp(pStartCh, "char", 4)) return CHAR;
    if(nCh == 6 && !memcmp(pStartCh, "double", 6)) return DOUBLE;
    if(nCh == 4 && !memcmp(pStartCh, "else", 4)) return ELSE;
    if(nCh == 3 && !memcmp(pStartCh, "for", 3)) return FOR;
    if(nCh == 2 && !memcmp(pStartCh, "if", 2)) return IF;
    if(nCh == 3 && !memcmp(pStartCh, "int", 3)) return INT;
    if(nCh == 6 && !memcmp(pStartCh, "return", 6)) return RETURN;
    if(nCh == 6 && !memcmp(pStartCh, "struct", 6)) return STRUCT;
    if(nCh == 4 && !memcmp(pStartCh, "void", 4)) return VOID;
    if(nCh == 5 && !memcmp(pStartCh, "while", 5)) return WHILE;
    return -1;
}

static int decodeEscape(int c){
    switch(c){
        case 'a': return '\a';
        case 'b': return '\b';
        case 'f': return '\f';
        case 'n': return '\n';
        case 'r': return '\r';
        case 't': return '\t';
        case 'v': return '\v';
        case '\'': return '\'';
        case '"': return '"';
        case '?': return '?';
        case '\\': return '\\';
        case '0': return '\0';
        default: return -1;
    }
}

int getNextToken() {
    int state = 0; // The shared start state '0'
    char ch;
    const char *pStartCh = pCrtCh; // Mark the beginning of the token
    Token *tk;
    int charVal; // for char constants

    while (1) {
        ch = *pCrtCh;
        switch (state) {
            case 0: 
                if (ch == ' ' || ch == '\r' || ch == '\t') { pCrtCh++; pStartCh = pCrtCh; }
                else if (ch == '\n') { line++; pCrtCh++; pStartCh = pCrtCh; }
                else if (ch == 0) { state = 9; } // END 
                
                //transitions -> ID si Constants
                else if (isalpha(ch) || ch == '_') { state = 101; pCrtCh++; } // ID 
                else if (ch == '0') { state = 103; pCrtCh++; } // 0/Hex/Octal path
                else if (isdigit(ch) && ch != '0') { state = 108; pCrtCh++; } // Decimal
                else if (ch == '\'') { state = 116; pCrtCh++; } // Char 
                else if (ch == '\"') { state = 119; pCrtCh++; } // String
                
                // transitions -> delimitatori operatori
                else if (ch == ',') { state = 1; pCrtCh++; }
                else if (ch == ';') { state = 2; pCrtCh++; }
                else if (ch == '(') { state = 3; pCrtCh++; }
                else if (ch == ')') { state = 4; pCrtCh++; }
                else if (ch == '[') { state = 5; pCrtCh++; }
                else if (ch == ']') { state = 6; pCrtCh++; }
                else if (ch == '{') { state = 7; pCrtCh++; }
                else if (ch == '}') { state = 8; pCrtCh++; }
                else if (ch == '+') { state = 10; pCrtCh++; }
                else if (ch == '-') { state = 11; pCrtCh++; }
                else if (ch == '*') { state = 12; pCrtCh++; }
                else if (ch == '/') { 
                    if (pCrtCh[1] == '/') { // comment -> next!!!
                        pCrtCh += 2;
                        while(*pCrtCh && *pCrtCh != '\n' && *pCrtCh != '\r') pCrtCh++;
                        pStartCh = pCrtCh;
                    } else { state = 13; pCrtCh++; } 
                }
                else if (ch == '.') { state = 14; pCrtCh++; }
                else if (ch == '&') { state = 15; pCrtCh++; }
                else if (ch == '|') { state = 17; pCrtCh++; }
                else if (ch == '=') { state = 19; pCrtCh++; }
                else if (ch == '!') { state = 22; pCrtCh++; }
                else if (ch == '<') { state = 25; pCrtCh++; }
                else if (ch == '>') { state = 28; pCrtCh++; }
                else tkerr(NULL, "invalid character '%c'", ch);
                break;

            // ID & KEYWORD
            case 101: 
                if (isalnum(ch) || ch == '_') pCrtCh++; 
                else state = 102; 
                break;
            case 102: {
                int nCh = (int)(pCrtCh - pStartCh);
                int kw = isKeyword(pStartCh, nCh);
                if (kw != -1) return addTk(kw)->code;
                tk = addTk(ID);
                tk->text = createString(pStartCh, pCrtCh);
                return ID;
            }

            // NUMERIC CONSTANTS
            case 103: // found '0'
                if (ch == 'x' || ch == 'X') { state = 106; pCrtCh++; }
                else if (isdigit(ch)) { state = 104; pCrtCh++; }
                else if (ch == '.') { state = 110; pCrtCh++; }
                else state = 105; 
                break;
            case 104: // octal digits
                if (isdigit(ch)) pCrtCh++;
                else state = 105;
                break;
            case 106: // hex digits
                if (isxdigit(ch)) pCrtCh++;
                else state = 105;
                break;
            case 108: // f [1-9]
                if (isdigit(ch)) pCrtCh++;
                else if (ch == '.') { state = 110; pCrtCh++; }
                else if (ch == 'e' || ch == 'E') { state = 112; pCrtCh++; }
                else state = 105; 
                break;
            case 110: //  '.'
                if (isdigit(ch)) { state = 111; pCrtCh++; }
                else tkerr(NULL, "invalid real constant");
                break;
            case 111: // after decimal .
                if (isdigit(ch)) pCrtCh++;
                else if (ch == 'e' || ch == 'E') { state = 112; pCrtCh++; }
                else state = 115;
                break;
            case 112: //  'e'
                if (ch == '+' || ch == '-') { state = 113; pCrtCh++; }
                else if (isdigit(ch)) { state = 114; pCrtCh++; }
                else tkerr(NULL, "invalid exponent");
                break;
            case 113: // after + or -
                if (isdigit(ch)) { state = 114; pCrtCh++; }
                else tkerr(NULL, "invalid exponent");
                break;
            case 114:
                if (isdigit(ch)) pCrtCh++;
                else state = 115;
                break;

            // state-urile finalizatoare:
            
            case 105: // accept CT_INT
                {
                    char *endptr;
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
            case 115: // accept CT_REAL
                tk = addTk(CT_REAL);
                tk->r = strtod(pStartCh, NULL);
                return CT_REAL;

            // CHAR CONSTANT
            case 116: // after '
                if (ch == '\\') { state = 117; pCrtCh++; }
                else if (ch == '\'' || ch == '\n' || ch == '\r' || ch == 0) tkerr(NULL, "invalid char constant");
                else { charVal = ch; state = 118; pCrtCh++; }
                break;
            case 117: // after '\'
                charVal = decodeEscape(ch);
                if (charVal == -1) tkerr(NULL, "invalid escape sequence in char constant");
                state = 118; pCrtCh++;
                break;
            case 118: // expect '
                if (ch != '\'') tkerr(NULL, "missing closing quote in char constant");
                pCrtCh++;
                tk = addTk(CT_CHAR);
                tk->i = charVal;
                return CT_CHAR;

            // STRING CONSTANT
            case 119: // after "
                {
                    char *buf, *dst;
                    size_t cap = 16, len = 0;
                    buf = (char *)malloc(cap);
                    if(!buf) err("not enough memory");
                    while(*pCrtCh != '"'){
                        int c;
                        if(*pCrtCh == 0 || *pCrtCh == '\n' || *pCrtCh == '\r'){
                            free(buf);
                            tkerr(NULL, "unterminated string literal");
                        }
                        if(*pCrtCh == '\\'){
                            pCrtCh++;
                            c = decodeEscape(*pCrtCh);
                            if(c == -1){
                                free(buf);
                                tkerr(NULL, "invalid escape sequence in string");
                            }
                            pCrtCh++;
                        }else{
                            c = *pCrtCh;
                            pCrtCh++;
                        }
                        if(len + 1 >= cap){
                            cap *= 2;
                            dst = (char *)realloc(buf, cap);
                            if(!dst){
                                free(buf);
                                err("not enough memory");
                            }
                            buf = dst;
                        }
                        buf[len++] = (char)c;
                    }
                    pCrtCh++;
                    buf[len] = '\0';
                    tk = addTk(CT_STRING);
                    tk->text = buf;
                    return CT_STRING;
                }

            // OPERATORS 
            case 1: addTk(COMMA); return COMMA;
            case 2: addTk(SEMICOLON); return SEMICOLON;
            case 3: addTk(LPAR); return LPAR;
            case 4: addTk(RPAR); return RPAR;
            case 5: addTk(LBRACKET); return LBRACKET;
            case 6: addTk(RBRACKET); return RBRACKET;
            case 7: addTk(LACC); return LACC;
            case 8: addTk(RACC); return RACC;
            case 9: addTk(END); return END;
            case 10: addTk(ADD); return ADD;
            case 11: addTk(SUB); return SUB;
            case 12: addTk(MUL); return MUL;
            case 13: addTk(DIV); return DIV;
            case 14: addTk(DOT); return DOT;
            case 15: // inainte am avut &
                if (ch == '&') { pCrtCh++; addTk(AND); return AND; }
                else tkerr(NULL, "invalid character '&', did you mean '&&'?");
                break;
            case 17: // |
                if (ch == '|') { pCrtCh++; addTk(OR); return OR; }
                else tkerr(NULL, "invalid character '|', did you mean '||'?");
                break;
            case 19: // =
                if (ch == '=') { pCrtCh++; addTk(EQUAL); return EQUAL; }
                else { addTk(ASSIGN); return ASSIGN; }
            case 22: // !
                if (ch == '=') { pCrtCh++; addTk(NOTEQ); return NOTEQ; }
                else { addTk(NOT); return NOT; }
            case 25: // <
                if (ch == '=') { pCrtCh++; addTk(LESSEQ); return LESSEQ; }
                else { addTk(LESS); return LESS; }
            case 28: // >
                if (ch == '=') { pCrtCh++; addTk(GREATEREQ); return GREATEREQ; }
                else { addTk(GREATER); return GREATER; }
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

void parse();

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

    // showTokens(tokens); // optional: display the tokens (maybe debugging later but already tested)

    parse();

    freeTokens(tokens);
    free(buffer);

    return 0;
}