#include <string.h>
#include <limits.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>


/*Vec*/
#define ImplementVec(T) \
typedef struct { \
    T * items; \
    unsigned int len; \
    unsigned int cap; \
} T##Vec; \
 \
void T##Vec_push(T##Vec* self, T value); \
void T##Vec_push(T##Vec* self, T value) { \
    if(self->cap <= 0) { \
        const int initial_cap = 16; \
        self->items = malloc(sizeof(T) * initial_cap);  \
        assert(self->items); \
        self->cap = initial_cap; \
    } \
    if(self->len >= self->cap) { \
        self->items = realloc(self->items, sizeof(T) * self->cap * 2); \
        assert(self->items); \
        self->cap *= 2; \
    } \
    self->items[self->len] = value; \
    self->len += 1; \
} \
 \
T T##Vec_pop(T##Vec* self); \
T T##Vec_pop(T##Vec* self) { \
    if(self->len <= 0) { \
        assert(0 && "stack underflow"); \
    } \
    self->len -= 1; \
    return self->items[self->len]; \
}


#define streql(a, b) (strncmp(a, b, 256) == 0)


struct Forth;
typedef void (*SpecialFormFn) (struct Forth *);

typedef struct {
    int i;
} StringId;
#define NULL_STRING_ID 0

typedef struct {
    char * str;
} String;

typedef struct {
    enum {
        TAG_INTEGER,
        TAG_REAL,
        TAG_SYMBOL
    } tag;
    union {
        int integer;
        float real;
        StringId symbol;
    } value;
} Token;

typedef struct {
    enum {
        TAG_PUSH_TOKEN,
        TAG_SPECIAL_FORM,
        TAG_FN_CALL,
        TAG_EOF
    } tag;
    union {
        Token push_token;
        SpecialFormFn special_form;
        int fn_call;
    } value;
} Opcode;

typedef int Int;
ImplementVec(Int)
ImplementVec(Token)
ImplementVec(Opcode)
ImplementVec(String)
ImplementVec(SpecialFormFn)

typedef struct Forth {
    StringVec memory_buffers;
    StringVec strings;
    TokenVec stack;
    int instruction_ptr;
    IntVec return_stack;
    OpcodeVec opcodes;

    /*special_forms names*/
    StringId special_form_plus;
    StringId special_form_minus;
    StringId special_form_times;
    StringId special_form_divide;
    StringId special_form_dup;
    StringId special_form_emit;
    StringId special_form_compile;
    StringId special_form_stop_compile;
} Forth;


int is_integer(char * tok) {
    while(*tok != 0) {
        if(*tok < '0' || *tok > '9') return 0;
        ++tok;
    }
    return 1;
}

int is_real(char * tok) {
    int decimal_found = 0;
    while(*tok != 0) {
        if(*tok < '0' || *tok > '9') return 0;
        if(*tok == '.') {
            if(decimal_found) {
                return 0;
            } else {
                decimal_found = 1;
            }
        }
        ++tok;
    }
    return 1;
}

StringId forth_string_id(Forth * f, char * str) {
    StringId result = {0};
    unsigned int i = 1;

    /*ensure null string is taken*/
    if(f->strings.len == 0) {
        String string = {0};
        StringVec_push(&f->strings, string);
    }

    for(i = 1; i < f->strings.len; ++i) {
        if(strcmp(f->strings.items[i].str, str) == 0) {
            result.i = i;
            return result;
        }
    }
    {
        String string = {0};
        string.str = str;
        StringVec_push(&f->strings, string);
        result.i = f->strings.len - 1;
        return result;
    }
}

void forth_report_error(Forth * f, char * msg) {
    (void) f;
    printf("Error: %s\n", msg);
    abort();
}

#define special_form_math_fn(name, operator) \
    void name(Forth * f) { \
        Token top = TokenVec_pop(&f->stack); \
        Token second = TokenVec_pop(&f->stack); \
        Token result = {0}; \
        if(top.tag == TAG_SYMBOL) { \
            forth_report_error(f, "Expected real or integer at stack top");  \
        } \
        if(second.tag == TAG_SYMBOL) { \
            forth_report_error(f, "Expected real or integer at stack second to top"); \
        } \
        if(top.tag != second.tag) { \
            forth_report_error(f, "Expected top and second to top types to match"); \
        } \
        if(top.tag == TAG_REAL) { \
            result.tag = TAG_REAL; \
            result.value.real = top.value.real operator second.value.real; \
            TokenVec_push(&f->stack, result); \
        } else if(top.tag == TAG_INTEGER) { \
            result.tag = TAG_INTEGER; \
            result.value.integer = top.value.integer operator second.value.integer; \
            TokenVec_push(&f->stack, result); \
        } else { \
            forth_report_error(f, "Invalid tag for stack item"); \
        } \
    }

special_form_math_fn(special_form_plus, +)
special_form_math_fn(special_form_minus, -)
special_form_math_fn(special_form_times, *)
special_form_math_fn(special_form_divide, /)

#undef special_form_math_fn

void special_form_dup(Forth * f) {
    Token top = TokenVec_pop(&f->stack);
    TokenVec_push(&f->stack, top);
    TokenVec_push(&f->stack, top);
}


/*ensure the special form names are set*/
void forth_ensure_valid_special_forms(Forth * f) {
    #define special_form_id_set(fieldname, stringname) \
        if(f->fieldname.i == NULL_STRING_ID) { \
            f->fieldname = forth_string_id(f, stringname); \
        }

    special_form_id_set(special_form_dup, "dup");
    special_form_id_set(special_form_plus, "+");
    special_form_id_set(special_form_minus, "-");
    special_form_id_set(special_form_divide, "/");
    special_form_id_set(special_form_times, "*");
    special_form_id_set(special_form_emit, "emit");

    #undef special_form_id_set
}

void forth_top_level(Forth * f, TokenVec * toks, int * i) {
    Token tok = toks->items[*i];
    Opcode opcode = {0};

    forth_ensure_valid_special_forms(f);

    if(tok.tag == TAG_INTEGER || tok.tag == TAG_REAL) {
        opcode.tag = TAG_PUSH_TOKEN;
        opcode.value.push_token = tok;
        OpcodeVec_push(&f->opcodes, opcode);
    } else if (tok.tag == TAG_SYMBOL) {
        #define if_special_form(fieldname) \
            if(tok.value.symbol.i == f->fieldname.i) { \
                opcode.tag = TAG_SPECIAL_FORM; \
                opcode.value.special_form = fieldname; \
                OpcodeVec_push(&f->opcodes, opcode); \
            }

        if_special_form(special_form_dup)
        else if_special_form(special_form_plus)
        else if_special_form(special_form_minus)
        else if_special_form(special_form_times)
        else if_special_form(special_form_divide)

        #undef if_special_form

    } else {
        assert(0 && "Unreachable code");
    }
}

void forth_eval(Forth * f, const char * const input, unsigned long len) {
    char * buf = strndup(input, len + 1);
    TokenVec tokens = {0};
    char * tok = strtok(buf, " \n");
    {
        String str = {0};
        str.str = buf;
        StringVec_push(&f->memory_buffers, str);
    }

    /*tokenize input*/
    while(tok != NULL) {
        Token token = {0};
        if(is_integer(tok)) {
            token.tag = TAG_INTEGER;
            token.value.integer = atoi(tok);
            TokenVec_push(&tokens, token);
        } else if(is_real(tok)) {
            token.tag = TAG_REAL;
            token.value.real = atof(tok);
            TokenVec_push(&tokens, token);
        } else {
            token.tag = TAG_SYMBOL;
            token.value.symbol = forth_string_id(f, tok);
            TokenVec_push(&tokens, token);
        }
        
        tok = strtok(NULL, " \n");
    }

    /*parse tokens into opcodes*/
    {
        int current_token = 0;
        forth_top_level(f, &tokens, &current_token);
    }
    /*
    for(i = 0; i < tokens.len; ++i) {
        const Token token = tokens.items[i];
        if(token.tag == TAG_INTEGER) {

        }
    }*/

}



int main() {
    Forth f = {0};
    char * input = "1 2 3 4 5";
    forth_eval(&f, input, strlen(input));
}

#if 0

struct Forth;
typedef void (*WordFn) (struct Forth *);

/*
typedef struct {
    char * name;
    int tok;
    WordFn fn;
    int tokens[word_max_tokens];
} Word;*/

typedef struct {
    int dense_len;
    int dense_cap;
    Word * dense;
    intvec sparse;
} WordSet;

void wordset_put(WordSet * self, Word word) {
    const int tok = word.tok;
    assert(tok >= 0);

    while(tok * 2 + 1 > self->sparse.len) {
        intvec_push(&self->sparse, -1);
    }
    {
        const int index = self->sparse.items[tok];
        if(index == -1) {
            /*put new*/
            if(self->dense_cap == 0) {
                self->dense = malloc(sizeof(Word) * 16);
                self->dense_cap = 16;
                self->dense_len = 0;
            }
            if(self->dense_len >= self->dense_cap) {
                self->dense_cap *= 2;
                self->dense = realloc(self->dense, sizeof(Word) * self->dense_cap);
            }
            self->dense[self->dense_len] = word;
            self->sparse.items[tok] = self->dense_len;
            self->dense_len += 1;
        } else {
            /*put replace_existing*/
            self->dense[index] = word;
        }

    }
}

Word * wordset_get(WordSet * self, int tok) {
    /*printf("Looking up tok:%d\n", tok); */
    if(tok < 0 || tok > self->sparse.len) {
        return NULL;
    } else {
        const int index = self->sparse.items[tok];
        if(index == -1) {
            return NULL;
        } else {
            /*printf("Word found:%s\b", self->dense[index].name);*/
            return &self->dense[index];
        }

    }
    
}

typedef struct Forth {
    Tokenizer tokenizer;
    WordSet words;
    intvec stk;
    int compile_mode;
    int exit;
} Forth;


int get_token(Forth * f) {
    if(f->tokenizer.out.len <= 0) {
        return -1;
    }
    return intvec_pop(&f->tokenizer.out);
}

char * token_value(Forth * f, int token) {
    if(token < 0) {
        return "<invalid_token>";
    }
    return f->tokenizer.tokens[token]; 
}

int token_id(Forth *f, char * tok) {
    return tokenizer_id(&f->tokenizer, tok);
}

/*builtin words*/
void builtin_plus(Forth * f) {
    intvec_push(&f->stk, intvec_pop(&f->stk) + intvec_pop(&f->stk));
}
void builtin_minus(Forth * f) {
    intvec_push(&f->stk, intvec_pop(&f->stk) - intvec_pop(&f->stk));
}
void builtin_times(Forth * f) {
    intvec_push(&f->stk, intvec_pop(&f->stk) * intvec_pop(&f->stk));
}
void builtin_divide(Forth * f) {
    intvec_push(&f->stk, intvec_pop(&f->stk) / intvec_pop(&f->stk));
}
void builtin_dup(Forth * f) {
    int top = intvec_pop(&f->stk);
    intvec_push(&f->stk, top);
    intvec_push(&f->stk, top);
}
void builtin_emit(Forth * f) {
    int top = intvec_pop(&f->stk);
    printf("%c", (char)top);
}
void builtin_period(Forth * f) {
    int top = intvec_pop(&f->stk);
    printf("%d\n", top);
}
void builtin_enter_compile_mode(Forth * f) {
    assert(f->compile_mode = 0);
    f->compile_mode = 1;
}
void builtin_exit_compile_mode(Forth * f) {
    assert(f->compile_mode = 1);
    f->compile_mode = 0;
}
void builtin_print_stack(Forth * f) {
    int i = 0; 

    for(i = 0; i < f->stk.len; ++i) {
        if(i != 0) printf(" ");
        printf("%d", f->stk.items[i]);
    }
    printf("\n");
}
void builtin_debug_tokens(Forth * f) {
    int i = 0;
    for(i = 0; i < f->tokenizer.len; ++i) {
        printf("%d:\"%s\"\n", i, f->tokenizer.tokens[i]);
    }
}
void builtin_exit(Forth * f) {
    f->exit = 1;
}

Forth forth_init() {
    Forth f = {0};
    /*add builtin words*/
    {
        Word w = {0};
        w.name = "+";
        w.tok = token_id(&f, w.name);
        w.fn = builtin_plus;
        wordset_put(&f.words, w);
    }
    {
        Word w = {0};
        w.name = "-";
        w.tok = token_id(&f, w.name);
        w.fn = builtin_minus;
        wordset_put(&f.words, w);
    }
    {
        Word w = {0};
        w.name = "*";
        w.tok = token_id(&f, w.name);
        w.fn = builtin_times;
        wordset_put(&f.words, w);
    }
    {
        Word w = {0};
        w.name = "/";
        w.tok = token_id(&f, w.name);
        w.fn = builtin_divide;
        wordset_put(&f.words, w);
    }
    {
        Word w = {0};
        w.name = "dup";
        w.tok = token_id(&f, w.name);
        w.fn = builtin_dup;
        wordset_put(&f.words, w);
    }
    {
        Word w = {0};
        w.name = ".";
        w.tok = token_id(&f, w.name);
        w.fn = builtin_period;
        wordset_put(&f.words, w);
    }
    {
        Word w = {0};
        w.name = "emit";
        w.tok = token_id(&f, w.name);
        w.fn = builtin_emit;
        wordset_put(&f.words, w);
    }
    {
        Word w = {0};
        w.name = ":";
        w.tok = token_id(&f, w.name);
        w.fn = builtin_enter_compile_mode;
        wordset_put(&f.words, w);
    }
    {
        Word w = {0};
        w.name = ";";
        w.tok = token_id(&f, w.name);
        w.fn = builtin_exit_compile_mode;
        wordset_put(&f.words, w);
    }
    {
        Word w = {0};
        w.name = ".s";
        w.tok = token_id(&f, w.name);
        w.fn = builtin_print_stack;
        wordset_put(&f.words, w);
    }
    {
        Word w = {0};
        w.name = "debug_tokens";
        w.tok = token_id(&f, w.name);
        w.fn = builtin_debug_tokens;
        wordset_put(&f.words, w);
    }
    {
        Word w = {0};
        w.name = "exit";
        w.tok = token_id(&f, w.name);
        w.fn = builtin_exit;
        wordset_put(&f.words, w);
    }

    return f;
}

#define streql(a, b) (strncmp(a, b, 128) == 0)

void forth_run(Forth * f) {

    int tok = get_token(f); 
    /*printf("processing token id %d:%s\n", tok, token_value(f, tok));*/
    while(tok >= 0) {
        if(f->exit) return;
        if(f->compile_mode == 0) {
            Word * w = wordset_get(&f->words, tok);
            if(w != NULL) {
                /*printf("Running word:%s from tok:%d\n", w->name, tok);*/
                fflush(stdout);
                assert(streql(w->name, token_value(f, tok)));
                if(w->fn) {
                    w->fn(f);
                    if(f->exit) return;
                } else {
                    /*todo custom compiled words*/
                    fprintf(stderr, "word has no definition (yet)\n");
                }
            } else {
                const char * value = token_value(f, tok);
                if(is_number(value)) {
                    intvec_push(&f->stk, atoi(value));
                } else {
                    fprintf(stderr, "invalid word \"%s\"\n", value);
                }
            }
        } else {
            fprintf(stderr, "Compile Mode not implemented yet\n");
        }
        printf("    Stack: ");
        builtin_print_stack(f);
        tok = get_token(f);
    }

}


int main(int argc, char ** argv) {
    Forth f = forth_init();
    char * line = NULL;
    size_t len = 0;
    /*assert(argc >= 1);
    tokenize_file(&f.tokenizer, argv[1]);*/
    while(f.exit == 0) {
        getline(&line, &len, stdin);
        tokenize(&f.tokenizer, line);
        forth_run(&f);
    }
    return 0;
}
#endif
