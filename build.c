#!/usr/bin/env tcc -run

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define SRC "main.c"
#define CFLAGS "-g -Wall -Wextra"
#define CLANG_FLAGS "-fsanitize=address -fsanitize=undefined" " " CFLAGS
#define CC "clang"
#define BUILDDIR ".build"
#define OUT BUILDDIR "/main"

int main(int argc, char ** argv) {
    int result = 0;
    enum {CLEAN, RUN, BUILD} mode = BUILD;
    
    if(argc > 1) {
        if(strcmp(argv[1], "clean") == 0) {
            mode = CLEAN;
        } else if(strcmp(argv[1], "run") == 0) {
            mode = RUN;
        } else if(strcmp(argv[1], "build") == 0) {
            mode = BUILD;
        } else {
            fprintf(stderr, "Invalid cli argument, expected \"build\", \"run\", or \"clean\"\n");
            return 1;
        }
    }

    if(mode == BUILD || mode == RUN) {
        system("mkdir -p " BUILDDIR);
        system(CC " " SRC " " CLANG_FLAGS " -o " OUT);
    }
    if(mode == RUN) {
        system("./" OUT);
    }
    if(mode == CLEAN) {
        system("trash " BUILDDIR);
    }
    return result;
}
