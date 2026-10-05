#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>

/*** TOKENS + TOKEN METADATA ***/
#define MAX_IDENT_LEN 63
#define MAX_SYMS 256
#define MAX_SEM_ERRS 256
#define MAX_ERRMSG 256

typedef enum {
    TOK_EOF = 0,

    TOK_INT_KW,
    TOK_PRINT_KW,

    TOK_IDENT,
    TOK_NUMBER,

    TOK_ASSIGN,
    TOK_PLUS,
    TOK_MINUS,
    TOK_MUL,
    TOK_DIV,

    TOK_SEMI,
    TOK_LPAREN,
    TOK_RPAREN,

    TOK_UNKNOWN
} TokenType;

typedef struct {
    TokenType type;
    char lexeme[128];
    long number;
    int line;
    int col;
} Token;

typedef struct {
    TokenType type;
    const char *enum_name;
    const char *expected;
} TokenInfo;

static const TokenInfo TOKEN_TABLE[] = {
    { TOK_INT_KW,   "TOKEN_INT",    "'int'"   },
    { TOK_PRINT_KW, "TOKEN_PRINT",  "'print'" },

    { TOK_IDENT,    "TOKEN_IDENT",  "IDENT"   },
    { TOK_NUMBER,   "TOKEN_NUMBER", "NUMBER"  },

    { TOK_ASSIGN,   "TOKEN_ASSIGN", "'='"     },
    { TOK_PLUS,     "TOKEN_PLUS",   "'+'"     },
    { TOK_MINUS,    "TOKEN_MINUS",  "'-'"     },
    { TOK_MUL,      "TOKEN_MUL",    "'*'"     },
    { TOK_DIV,      "TOKEN_DIV",    "'/'"     },

    { TOK_SEMI,     "TOKEN_SEMI",   "';'"     },
    { TOK_LPAREN,   "TOKEN_LPAREN", "'('"     },
    { TOK_RPAREN,   "TOKEN_RPAREN", "')'"     },

    { TOK_EOF,      "TOKEN_EOF",    "EOF"     },
    { TOK_UNKNOWN,  "TOKEN_UNKNOWN","UNKNOWN" }
};

static const TokenInfo* token_info(TokenType t) {
    for (size_t i = 0; i < sizeof(TOKEN_TABLE)/sizeof(TOKEN_TABLE[0]); i++) {
        if (TOKEN_TABLE[i].type == t) return &TOKEN_TABLE[i];
    }
    return &TOKEN_TABLE[sizeof(TOKEN_TABLE)/sizeof(TOKEN_TABLE[0]) - 1];
}

/*====== LEXER ======*/
typedef struct {
    const char *src;
    size_t len;
    size_t i;
    int line;
    int col;
} Lexer;

static void lexer_init(Lexer *L, const char *src) {
    L->src = src;
    L->len = strlen(src);
    L->i = 0;
    L->line = 1;
    L->col = 1;
}

static int lexer_peek(Lexer *L) {
    return (L->i >= L->len) ? EOF : (unsigned char)L->src[L->i];
}

static int lexer_get(Lexer *L) {
    if (L->i >= L->len) return EOF;
    int c = (unsigned char)L->src[L->i++];
    if (c == '\n') { L->line++; L->col = 1; }
    else { L->col++; }
    return c;
}

static void lexer_skip_ws_and_comments(Lexer *L) {
    for (;;) {
        int c = lexer_peek(L);
        if (c == EOF) return;

        if (isspace(c)) { lexer_get(L); continue; }

        if (c == '/' && L->i + 1 < L->len && L->src[L->i + 1] == '/') {
            lexer_get(L); lexer_get(L);
            while ((c = lexer_peek(L)) != EOF && c != '\n') lexer_get(L);
            continue;
        }
        return;
    }
}

static Token make_token(TokenType t, const char *lex, int line, int col) {
    Token tok;
    tok.type = t;
    tok.number = 0;
    tok.line = line;
    tok.col = col;
    tok.lexeme[0] = '\0';
    if (lex) snprintf(tok.lexeme, sizeof(tok.lexeme), "%s", lex);
    return tok;
}

static Token lexer_next(Lexer *L) {
    lexer_skip_ws_and_comments(L);

    int c = lexer_peek(L);
    if (c == EOF) return make_token(TOK_EOF, "EOF", L->line, L->col);

    int line = L->line, col = L->col;

    if (isalpha(c) || c == '_') {
        char buf[128];
        size_t k = 0;
        while ((c = lexer_peek(L)) != EOF && (isalnum(c) || c == '_')) {
            if (k + 1 < sizeof(buf)) buf[k++] = (char)lexer_get(L);
            else (void)lexer_get(L);
        }
        buf[k] = '\0';

        if (!strcmp(buf, "int"))   return make_token(TOK_INT_KW, buf, line, col);
        if (!strcmp(buf, "print")) return make_token(TOK_PRINT_KW, buf, line, col);
        return make_token(TOK_IDENT, buf, line, col);
    }

    if (isdigit(c)) {
        char buf[128];
        size_t k = 0;
        while ((c = lexer_peek(L)) != EOF && isdigit(c)) {
            if (k + 1 < sizeof(buf)) buf[k++] = (char)lexer_get(L);
            else (void)lexer_get(L);
        }
        buf[k] = '\0';
        Token t = make_token(TOK_NUMBER, buf, line, col);
        t.number = strtol(buf, NULL, 10);
        return t;
    }

    lexer_get(L);
    switch (c) {
        case '=': return make_token(TOK_ASSIGN, "=", line, col);
        case '+': return make_token(TOK_PLUS,   "+", line, col);
        case '-': return make_token(TOK_MINUS,  "-", line, col);
        case '*': return make_token(TOK_MUL,    "*", line, col);
        case '/': return make_token(TOK_DIV,    "/", line, col);
        case ';': return make_token(TOK_SEMI,   ";", line, col);
        case '(': return make_token(TOK_LPAREN, "(", line, col);
        case ')': return make_token(TOK_RPAREN, ")", line, col);
        default: {
            char buf[16];
            if (isprint(c)) snprintf(buf, sizeof(buf), "%c", c);
            else snprintf(buf, sizeof(buf), "0x%02X", (unsigned)c);
            return make_token(TOK_UNKNOWN, buf, line, col);
        }
    }
}

/*====== TOKEN DUMP ======*/
static void dump_tokens(const char *src, FILE *out) {
    Lexer L;
    lexer_init(&L, src);

    for (;;) {
        Token t = lexer_next(&L);
        const TokenInfo *info = token_info(t.type);

        if (t.type == TOK_EOF) {
            fprintf(out, "%d:%d  %s\n", t.line, t.col, info->enum_name);
            break;
        }
        fprintf(out, "%d:%d  %-13s '%s'\n", t.line, t.col, info->enum_name, t.lexeme);
    }
}

/*====== SYMBOL TABLE ======*/
typedef struct {
    char name[64];
    long value;
    int initialized;
} Symbol;

typedef struct {
    Symbol syms[MAX_SYMS];
    int count;
} SymTable;

static void sym_init(SymTable *T) { T->count = 0; }

static int sym_find(SymTable *T, const char *name) {
    for (int i = 0; i < T->count; i++)
        if (!strcmp(T->syms[i].name, name)) return i;
    return -1;
}

static int sym_declare(SymTable *T, const char *name) {
    if (T->count >= MAX_SYMS) return -1;
    snprintf(T->syms[T->count].name, sizeof(T->syms[T->count].name), "%s", name);
    T->syms[T->count].value = 0;
    T->syms[T->count].initialized = 0;
    return T->count++;
}

/*====== PARSER / INTERPRETER ======*/

typedef struct {
    int line, col;
    char msg[MAX_ERRMSG];
} SemErr;

typedef struct {
    Lexer lex;
    Token cur;
    SymTable st;

    int syntax_errors;
    int semantic_errors;
    int had_error;
    
    SemErr sem_errs[MAX_SEM_ERRS];
    int sem_count;
} Parser;

static void parser_advance(Parser *P) { P->cur = lexer_next(&P->lex); }

static void report_v(const char *kind, int line, int col, const char *fmt, va_list ap) {
    fprintf(stderr, "%s error at %d:%d: ", kind, line, col);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
}

static void syntax_error(Parser *P, const char *fmt, ...) {
    P->syntax_errors++;
    P->had_error = 1;

    va_list ap;
    va_start(ap, fmt);
    report_v("Syntax", P->cur.line, P->cur.col, fmt, ap);
    va_end(ap);
}

/* STRICT MODE: store semantic errors, print later only if syntax_errors==0 */
static void semantic_error(Parser *P, int line, int col, const char *fmt, ...) {
    P->semantic_errors++;

    if (P->sem_count < MAX_SEM_ERRS) {
        SemErr *e = &P->sem_errs[P->sem_count++];
        e->line = line;
        e->col  = col;

        va_list ap;
        va_start(ap, fmt);
        vsnprintf(e->msg, sizeof(e->msg), fmt, ap);
        va_end(ap);
    }
}

static void flush_semantic_errors_if_no_syntax(Parser *P) {
    if (P->syntax_errors != 0) return; 
    for (int i = 0; i < P->sem_count; i++) {
        fprintf(stderr, "Semantic error at %d:%d: %s\n",
                P->sem_errs[i].line, P->sem_errs[i].col, P->sem_errs[i].msg);
    }
}

static long parse_expr(Parser *P);
static long parse_term(Parser *P);
static long parse_factor(Parser *P);

static void sync_to_semi(Parser *P) {
    while (P->cur.type != TOK_SEMI && P->cur.type != TOK_EOF)
        parser_advance(P);
    if (P->cur.type == TOK_SEMI) parser_advance(P);
}

static void sync_to_semi_or_rparen(Parser *P) {
    while (P->cur.type != TOK_SEMI &&
           P->cur.type != TOK_RPAREN &&
           P->cur.type != TOK_EOF) {
        parser_advance(P);
    }
}

static void parse_stmt(Parser *P) {
    if (P->cur.type == TOK_INT_KW) {
        parser_advance(P);

        if (P->cur.type != TOK_IDENT) {
            syntax_error(P, "Expected identifier after 'int'");
            sync_to_semi(P);
            return;
        }

        if (strlen(P->cur.lexeme) > MAX_IDENT_LEN) {
            semantic_error(P, P->cur.line, P->cur.col,
                "Identifier '%s' is too long (max %d characters)",
                P->cur.lexeme, MAX_IDENT_LEN);
            sync_to_semi(P);
            return;
        }

        char name[MAX_IDENT_LEN + 1];
        strcpy(name, P->cur.lexeme);
        parser_advance(P);

        if (sym_find(&P->st, name) >= 0) {
            semantic_error(P, P->cur.line, P->cur.col,
                           "Variable '%s' redeclared", name);
        } else {
            sym_declare(&P->st, name);
        }

        if (P->cur.type != TOK_ASSIGN) {
            syntax_error(P, "Missing '=' in declaration");
            sync_to_semi(P);
            return;
        }
        parser_advance(P);

        long value = parse_expr(P);
        int idx = sym_find(&P->st, name);
        if (idx >= 0) {
            P->st.syms[idx].value = value;
            P->st.syms[idx].initialized = 1;
        }

        if (P->cur.type != TOK_SEMI) {
            if (!P->had_error) { 
                syntax_error(P, "Missing ';' at end of declaration");
            }
            sync_to_semi(P);
            return;
        }
        parser_advance(P);
        return;
    }

    if (P->cur.type == TOK_PRINT_KW) {
        parser_advance(P);

        if (P->cur.type != TOK_LPAREN) {
            syntax_error(P, "Missing '(' after 'print'");
            sync_to_semi(P);
            return;
        }
        parser_advance(P);

        long value = parse_expr(P);

        if (P->cur.type != TOK_RPAREN) {
            syntax_error(P, "Missing ')'");
            sync_to_semi(P);
            return;
        }
        parser_advance(P);

        if (P->cur.type != TOK_SEMI) {
            if (!P->had_error) {
                syntax_error(P, "Missing ';' after print");
            }
            sync_to_semi(P);
            return;
        }
        parser_advance(P);

        if (P->syntax_errors == 0) {
            printf("%ld\n", value);
        }
        return;
    }

    syntax_error(P, "Unexpected token '%s'", P->cur.lexeme);
    sync_to_semi(P);
}

static long parse_expr(Parser *P) {
    long v = parse_term(P);

    while (P->cur.type == TOK_PLUS || P->cur.type == TOK_MINUS) {
        TokenType op = P->cur.type;
        parser_advance(P);
        long r = parse_term(P);
        v = (op == TOK_PLUS) ? v + r : v - r;
    }
    if (P->cur.type == TOK_UNKNOWN) {
        syntax_error(P, "Unknown character '%s'", P->cur.lexeme);
        parser_advance(P);
        sync_to_semi_or_rparen(P);
    }
    else if (P->cur.type == TOK_IDENT || P->cur.type == TOK_NUMBER || P->cur.type == TOK_LPAREN) {
        syntax_error(P, "Missing operator before '%s'", P->cur.lexeme);
        sync_to_semi_or_rparen(P);
    }

    return v;
}


static long parse_term(Parser *P) {
    long v = parse_factor(P);
    while (P->cur.type == TOK_MUL || P->cur.type == TOK_DIV) {
        Token t = P->cur;
        parser_advance(P);
        long r = parse_factor(P);
        if (t.type == TOK_DIV && r == 0) {
            semantic_error(P, t.line, t.col, "Division by zero");
            v = 0;
        } else {
            v = (t.type == TOK_MUL) ? v * r : v / r;
        }
    }
    return v;
}

static long parse_factor(Parser *P) {
    if (P->cur.type == TOK_NUMBER) {
        long v = P->cur.number;
        parser_advance(P);
        return v;
    }

    if (P->cur.type == TOK_IDENT) {
        char idname[128];
        snprintf(idname, sizeof(idname), "%s", P->cur.lexeme);

        int idx = sym_find(&P->st, idname);
        if (idx < 0) {
            semantic_error(P, P->cur.line, P->cur.col,
                           "Use of undeclared variable '%s'", idname);
            parser_advance(P);
            return 0;
        }
        long v = P->st.syms[idx].value;
        parser_advance(P);
        return v;
    }

    if (P->cur.type == TOK_LPAREN) {
        parser_advance(P);
        long v = parse_expr(P);
        if (P->cur.type != TOK_RPAREN) {
            syntax_error(P, "Missing ')'");
        } else {
            parser_advance(P);
        }
        return v;
    }

    if (P->cur.type == TOK_UNKNOWN) {
        syntax_error(P, "Unknown character '%s'", P->cur.lexeme);
        parser_advance(P);
        sync_to_semi_or_rparen(P);
        return 0;
    }

    syntax_error(P, "Invalid expression");
    sync_to_semi_or_rparen(P);
    return 0;
}

static void parse_program(Parser *P) {
    while (P->cur.type != TOK_EOF)
        parse_stmt(P);
}

/*====== FILE IO + MAIN ======*/
static char* read_entire_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;

    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    long n = ftell(f);
    if (n < 0) { fclose(f); return NULL; }
    rewind(f);

    char *buf = (char*)malloc((size_t)n + 1);
    if (!buf) { fclose(f); return NULL; }

    size_t got = fread(buf, 1, (size_t)n, f);
    buf[got] = '\0';
    fclose(f);
    return buf;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr,
            "Usage:\n"
            "  %s <sourcefile>\n"
            "  %s --tokens <sourcefile>\n",
            argv[0], argv[0]);
        return 2;
    }

    if (strcmp(argv[1], "--tokens") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: %s --tokens <sourcefile>\n", argv[0]);
            return 2;
        }
        char *src = read_entire_file(argv[2]);
        if (!src) {
            fprintf(stderr, "Could not read file: %s\n", argv[2]);
            return 2;
        }
        dump_tokens(src, stdout);
        free(src);
        return 0;
    }

    char *src = read_entire_file(argv[1]);
    if (!src) {
        fprintf(stderr, "Could not read file: %s\n", argv[1]);
        return 2;
    }

    Parser P;
    lexer_init(&P.lex, src);
    sym_init(&P.st);
    P.syntax_errors = 0;
    P.semantic_errors = 0;
    P.had_error = 0;
    P.sem_count = 0;

    parser_advance(&P);
    parse_program(&P);
    flush_semantic_errors_if_no_syntax(&P);
    free(src);

    return (P.syntax_errors || (P.syntax_errors == 0 && P.semantic_errors)) ? 1 : 0;
}
