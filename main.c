
/*
#define TOK_PLUS 0
#define TOK_MINUS 1
#define TOK_TIMES 2
#define TOK_DIVIDE 3
#define TOK_DUP 4
#define TOK_COLON 5
#define TOK_SEMICOLON 6
#define TOK_PERIOD 7
#define TOK_JMP 8
#define TOK_JMP_NE 9
#define TOK_NAND 10
#define TOK_EMIT 11
*/

#include <string.h>
#include <limits.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>


/*Int Vec*/
typedef struct {
    int * items;
    int len;
    int cap;
} IntVec;

void intvec_push(IntVec* self, int value) {
    if(self->cap <= 0) {
        const int initial_cap = 16;
        self->items = malloc(sizeof(int) * initial_cap); 
        assert(self->items);
        self->cap = initial_cap;
    }
    if(self->len >= self->cap) {
        self->items = realloc(self->items, sizeof(int) * self->cap * 2);
        assert(self->items);
        self->cap *= 2;
    }
    self->items[self->len] = value;
    self->len += 1;
}

void intvec_push_array(IntVec* self, int * values, int count) {
    int i = 0;
    for(i = count - 1; i >= 0; --i) {
        intvec_push(self, values[i]);
    }
}

int intvec_pop(IntVec * self) {
    if(self->len <= 0) {
        assert(0 && "stack underflow");
    }
    self->len -= 1;
    return self->items[self->len];
}



/*Tokenizer Implementation*/
typedef struct Tokenizer {
    IntVec out;

    char ** strs;
    int strs_len;
    int strs_cap;

    char ** tokens;
    int len;
    int cap;
} Tokenizer;


int tokenizer_id(Tokenizer * t, char * tok) {
    int i = 0;
    if(tok == NULL) {
        return -1;
    }
    for(i = 0; i < t->len; ++i) {
        if(strcmp(t->tokens[i], tok) == 0) {
            return i;
        }
    }
    if(t->cap == 0) {
        t->cap = 128;
        t->len = 0;
        t->tokens = malloc(t->cap * sizeof(char *));
    } else if(t->len >= t->cap) {
        t->cap = (t->cap + 1) * 2;
        t->tokens = realloc(t->tokens, t->cap * sizeof(char*));
        assert(t->tokens);
    }

    t->tokens[t->len] = tok;
    t->len += 1;
    return t->len - 1;
}

void tokenize(Tokenizer * self, const char * const str) {
/*    printf("Tokenizing:%s", str);*/
    if(self->strs_cap <= 0) {
        const int initial_cap = 16;
        self->strs = malloc(sizeof(char *) * initial_cap);
        assert(self->strs);
        self->strs_cap = initial_cap;
        self->strs_len = 0;
    }

    if(self->strs_len >= self->strs_cap) {
        self->strs = realloc(self->strs, sizeof(char *) * self->strs_cap * 2);
        assert(self->strs);
        self->strs_cap *= 2;
    }


    self->strs[self->strs_len] = strdup(str);
    self->strs_len += 1;
    {
        IntVec tmp = {0};
        char * tok = strtok(self->strs[self->strs_len - 1], " \n");
        intvec_push(&tmp, -1);

        while(tok != NULL) {
            const int id = tokenizer_id(self, tok);
            /*printf("%s:%d\n", tok, id);*/
            intvec_push(&tmp, id);
            tok = strtok(NULL, " \n");
        }

        {
            int tok = intvec_pop(&tmp);
            while(tok != -1) {
                intvec_push(&self->out, tok);
                tok = intvec_pop(&tmp);
            }
        }
        free(tmp.items);
    }
}




int is_number(const char * tok) {
    const char * str = tok;
    while(*tok != 0) {
        if(*tok < '0' || *tok > '9') {
            return 0;
        }
        ++tok;
    }
    return 1;
}

void tokenize_file(Tokenizer * self, char * filename) {
    FILE * f = fopen(filename, "r");
    char * buffer;
    int length;
    {
        fseek (f, 0, SEEK_END);
        length = ftell (f);
        fseek (f, 0, SEEK_SET);
        buffer = malloc (length);
        if (buffer) {
            fread (buffer, 1, length, f);
        }
        fclose (f);
    }
    return tokenize(self, buffer);
}


#define word_max_tokens

struct Forth;

typedef void (*WordFn) (struct Forth *);

typedef struct {
    char * name;
    int tok;
    WordFn fn;
    int tokens[word_max_tokens];
} Word;

typedef struct {
    int dense_len;
    int dense_cap;
    Word * dense;
    IntVec sparse;
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
    IntVec stk;
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
