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
        TAG_BUILTIN,
        TAG_FN_CALL,
        TAG_FN_RETURN,
        TAG_EOF
    } tag;
    union {
        Token push_token;
        SpecialFormFn builtin;
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
    IntVec labels;
    IntVec fns;
    OpcodeVec opcodes;

    /*builtins names*/
    StringId builtin_plus;
    StringId builtin_minus;
    StringId builtin_times;
    StringId builtin_divide;
    StringId builtin_dup;
    StringId builtin_emit;
    StringId builtin_period;

    /*
    StringId builtin_compile;
    StringId builtin_stop_compile;
    StringId builtin_if;
    StringId builtin_then;
    StringId builtin_label;
    StringId builtin_jump_equal;
    */
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

char * forth_get_string(Forth * f, StringId string) {
    return f->strings.items[string.i].str;
}

void forth_report_error(Forth * f, char * msg) {
    (void) f;
    printf("Error: %s\n", msg);
    abort();
}

#define builtin_math_fn(name, operator) \
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

builtin_math_fn(builtin_plus, +)
builtin_math_fn(builtin_minus, -)
builtin_math_fn(builtin_times, *)
builtin_math_fn(builtin_divide, /)

#undef builtin_math_fn

void builtin_dup(Forth * f) {
    Token top = TokenVec_pop(&f->stack);
    TokenVec_push(&f->stack, top);
    TokenVec_push(&f->stack, top);
}

void builtin_emit(Forth * f) {
    Token top = TokenVec_pop(&f->stack);
    if(top.tag != TAG_INTEGER) {
        forth_report_error(f, "Expected integer at stack top inside \'emit\'");
    }
    printf("%c", (char)top.value.integer);
}

void builtin_period(Forth * f) {
    Token top = TokenVec_pop(&f->stack);
    if(top.tag == TAG_INTEGER) {
        printf("%d\n", top.value.integer);
    } else if(top.tag == TAG_REAL) {
        printf("%f\n", top.value.real);
    } else {
        assert(top.tag == TAG_SYMBOL);
        printf("%s\n", forth_get_string(f, top.value.symbol));
    }
}

void forth_util_create_fn(Forth * f, StringId fn_name_id, int fn_instruction_ptr) {
    while(f->fns.len < fn_name_id.i + 1) {
        IntVec_push(&f->fns, -1);
    }
    f->fns.items[fn_name_id.i] = fn_instruction_ptr;
}

void forth_util_create_label(Forth * f, StringId label_name_id, int label_instruction_ptr) {
    while(f->labels.len < label_name_id.i + 1) {
        IntVec_push(&f->labels, -1);
    }
    f->fns.items[label_name_id.i] = label_instruction_ptr;
}

/*ensure the special form names are set*/
void forth_ensure_valid_builtins(Forth * f) {
    if(f->builtin_dup.i == NULL_STRING_ID)
        f->builtin_dup = forth_string_id(f, "dup");

    if(f->builtin_plus.i == NULL_STRING_ID)
        f->builtin_plus = forth_string_id(f, "+");

    if(f->builtin_minus.i == NULL_STRING_ID)
        f->builtin_minus= forth_string_id(f, "-");

    if(f->builtin_times.i == NULL_STRING_ID)
        f->builtin_times = forth_string_id(f, "*");

    if(f->builtin_divide.i == NULL_STRING_ID)
        f->builtin_divide = forth_string_id(f, "/");

    if(f->builtin_emit.i == NULL_STRING_ID)
        f->builtin_emit = forth_string_id(f, "emit");

    if(f->builtin_period.i == NULL_STRING_ID)
        f->builtin_period= forth_string_id(f, ".");
}

void forth_top_level(Forth * f, TokenVec * toks, int * i) {
    Token tok = toks->items[*i];
    Opcode opcode = {0};

    *i += 1;

    forth_ensure_valid_builtins(f);

    if(tok.tag == TAG_INTEGER || tok.tag == TAG_REAL) {
        opcode.tag = TAG_PUSH_TOKEN;
        opcode.value.push_token = tok;
        OpcodeVec_push(&f->opcodes, opcode);
    } else if (tok.tag == TAG_SYMBOL) {
        opcode.tag = TAG_BUILTIN;

        if(tok.value.symbol.i == f->builtin_plus.i) {
            opcode.value.builtin = builtin_plus;
        } else if(tok.value.symbol.i == f->builtin_minus.i) {
            opcode.value.builtin = builtin_minus;
        } else if(tok.value.symbol.i == f->builtin_times.i) {
            opcode.value.builtin = builtin_times;
        } else if(tok.value.symbol.i == f->builtin_divide.i) {
            opcode.value.builtin = builtin_divide;
        } else if(tok.value.symbol.i == f->builtin_emit.i) {
            opcode.value.builtin = builtin_emit;
        } else if(tok.value.symbol.i == f->builtin_period.i) {
            opcode.value.builtin = builtin_period;
        } else {
            /*symbol must be a word function*/
            assert(0 && "TODO");
        }

        OpcodeVec_push(&f->opcodes, opcode);
    } else {
        assert(0 && "Unreachable code");
    }
}

int forth_run_opcode(Forth * f) {
    const Opcode opcode = f->opcodes.items[f->instruction_ptr]; 
    f->instruction_ptr += 1;

    if(opcode.tag == TAG_EOF) {
        return 1;
    }
    if(opcode.tag == TAG_PUSH_TOKEN) {
        TokenVec_push(&f->stack, opcode.value.push_token);   
    } else if(opcode.tag == TAG_BUILTIN) {
        opcode.value.builtin(f);  
    } else if(opcode.tag == TAG_FN_CALL) {
        IntVec_push(&f->return_stack, f->instruction_ptr);
        f->instruction_ptr = opcode.value.fn_call;
    } else {
        assert(0 && "invalid opcode tag");
    }

    return 0;
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
        Opcode eof = {0};
        eof.tag = TAG_EOF;
        f->instruction_ptr = f->opcodes.len;
        while((unsigned int)current_token < tokens.len) {
            forth_top_level(f, &tokens, &current_token);
        }
        OpcodeVec_push(&f->opcodes, eof);
    }

    while(forth_run_opcode(f) == 0) {}
}



int main() {
    Forth f = {0};
    char * input = "1 2 3 4 5 + . * . 45 emit";
    forth_eval(&f, input, strlen(input));
}
