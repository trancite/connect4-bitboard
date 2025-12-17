#ifndef CONFIG_H
#define CONFIG_H
#ifdef _WIN32
    #include <windows.h>
#else
    #define _POSIX_C_SOURCE 199309L
    #include <time.h>
    #include <unistd.h>
#endif

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

typedef unsigned __int128 uint128_t;

#define MAX_N 10
#define MAX_M 10

extern int N;
extern int M;
extern int k;
extern int order[MAX_M];
extern int use_heuristic;
extern double ai_time_limit;

extern int bit_height;
extern uint128_t col_mask;
extern int top_row;

#define INF 32000
#define MAX_SCORE 30000
#define WIN_THRESHOLD 29000

#define TT_SIZE (1 << 25)
#define TT_MASK (TT_SIZE - 1)
#define TT_EXACT 0
#define TT_ALPHA 1
#define TT_BETA  2

#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define BLUE    "\033[34m"
#define RESET   "\033[0m"

void generate_column_order(int *ord);
void wait_seconds(double seconds);
void show_table(char table[MAX_N][MAX_M]);

#endif