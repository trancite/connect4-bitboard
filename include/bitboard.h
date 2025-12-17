#ifndef BITBOARD_H
#define BITBOARD_H


#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "config.h"


typedef struct bitboard {
    uint128_t player_1;
    uint128_t player_2;
    int16_t value;
} Bitboard;

typedef struct TTnode {
    uint64_t key;
    int16_t value;
    int8_t depth;
    uint8_t flag;
    int8_t best_move;
} TTnode;

typedef struct TTBucket {
    TTnode deep;
    TTnode recent;
} TTBucket;


typedef struct {
    int col;
    int score;
    uint128_t move_bit;
} MoveEntry;

extern uint64_t zobrist_keys[2][128];
extern TTBucket* transposition_table;
extern uint128_t global_bottom_mask;
extern uint128_t global_board_mask;


void init_zobrist();
void free_TT(TTBucket *transposition_table);

int get_best_move_general_negamax(Bitboard *root, double allowed_seconds, int verbose);
int get_best_move_negamax(Bitboard *root, double allowed_seconds, int verbose);

int assignTable_board(Bitboard *receptor, char table[MAX_N][MAX_M]);

#endif