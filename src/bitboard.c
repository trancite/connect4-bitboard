#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "bitboard.h"
#include "config.h"



extern int order[MAX_M];

uint64_t nodes[MAX_N * MAX_M];

int bit_height;

uint128_t col_mask;

uint64_t total_nodes = 0;

uint128_t col_mask_n;

int top_row;

extern int use_heuristic;

uint64_t zobrist_keys[2][128];

TTBucket* transposition_table = NULL;

uint128_t global_bottom_mask = 0;

uint128_t global_board_mask = 0;

uint8_t mirror_lookup[128];



static uint64_t seed = 98768645463325252ULL;


void generate_column_order(int *order);



uint64_t xorshift64star()
{
    uint64_t x = seed;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    seed = x;
    return x * 2685821657736338717ULL;
}

static inline int count_bits(uint128_t num)
{
    return __builtin_popcountll((uint64_t)(num >> 64)) + __builtin_popcountll((uint64_t)num);
}

static inline int get_bit_index(uint128_t move)
{
    uint64_t low = (uint64_t)move;
    uint64_t high = (uint64_t)(move >> 64);
    if (low != 0) return __builtin_ctzll(low);
    else return 64 + __builtin_ctzll(high);
}



static uint128_t mirror_board(uint128_t board)
{
    uint128_t mirrored = 0;
    uint128_t col_mask = ((uint128_t)1 << bit_height) - 1;

    for (int col = 0; col < M; col++)
    {
        uint128_t col_bits = (board >> (col * bit_height)) & col_mask;
        mirrored |= col_bits << ((M - 1 - col) * bit_height);
    }

    return mirrored;
}

void init_zobrist()
{
    bit_height = N + 1;
    col_mask = (((uint128_t)1 << N) - 1);
    col_mask_n = (((uint128_t)1 << N) - 1);
    top_row = N - 1;
    seed = 98768645463325252ULL;
    generate_column_order(order);

    for (int i = 0; i < 128; i++)
    {
        int row = i % bit_height;
        int col = i / bit_height;
        int mirror_col = (M - 1) - col;
        mirror_lookup[i] = (mirror_col * bit_height) + row;
    }

    global_bottom_mask = 0;
    global_board_mask = 0;

    for (int c = 0; c < M; c++)
    {
        global_board_mask |= (col_mask_n << (c * bit_height));
        global_bottom_mask |= ((uint128_t)1 << (c * bit_height));
    }

    for (int p = 0; p < 2; p++)
    {
        for (int i = 0; i < 128; i++)
        {
            zobrist_keys[p][i] = xorshift64star();
        }
    }

    if (transposition_table) free(transposition_table);

    transposition_table = (TTBucket*)calloc(TT_SIZE, sizeof(TTBucket));

    if (!transposition_table)
    {
        printf("Error: No memory for TT\n");
        exit(1);
    }

    for (int i = 0; i < TT_SIZE; i++)
    {
        transposition_table[i].deep.best_move = -1;
        transposition_table[i].recent.best_move = -1;
    }
}

void free_TT(TTBucket *table)
{
    if (table) free(table);
}

static uint64_t compute_hash(uint128_t p1, uint128_t p2)
{
    uint64_t h = 0;
    uint64_t low = (uint64_t)p1;
    uint64_t high = (uint64_t)(p1 >> 64);

    while (low)
    {
        h ^= zobrist_keys[0][__builtin_ctzll(low)];
        low &= (low - 1);
    }
    while (high)
    {
        h ^= zobrist_keys[0][__builtin_ctzll(high) + 64];
        high &= (high - 1);
    }

    low = (uint64_t)p2;
    high = (uint64_t)(p2 >> 64);

    while (low)
    {
        h ^= zobrist_keys[1][__builtin_ctzll(low)];
        low &= (low - 1);
    }
    while (high)
    {
        h ^= zobrist_keys[1][__builtin_ctzll(high) + 64];
        high &= (high - 1);
    }
    return h;
}


void save_position_TT_s(uint64_t hash, int16_t value, int8_t depth, uint8_t flag, int8_t best_movement)
{
    uint64_t index = hash & TT_MASK;
    TTBucket* bucket = &transposition_table[index];

    int is_mate = (value >= WIN_THRESHOLD || value <= -WIN_THRESHOLD);


    if (bucket->deep.key == hash)
    {
        if (depth >= bucket->deep.depth || is_mate)
        {
            bucket->deep.value = value;
            bucket->deep.depth = depth;
            bucket->deep.flag = flag;
            bucket->deep.best_move = best_movement;
        }
        return;
    }


    if (depth >= bucket->deep.depth || is_mate || bucket->deep.key == 0)
    {
        bucket->deep.key = hash;
        bucket->deep.value = value;
        bucket->deep.depth = depth;
        bucket->deep.flag = flag;
        bucket->deep.best_move = best_movement;
    }
    else
    {

        bucket->recent.key = hash;
        bucket->recent.value = value;
        bucket->recent.depth = depth;
        bucket->recent.flag = flag;
        bucket->recent.best_move = best_movement;
    }
}


TTnode* TT_check(uint64_t hash)
{
    uint64_t index = hash & TT_MASK;
    TTBucket* bucket = &transposition_table[index];


    if (bucket->deep.key == hash) return &bucket->deep;

    if (bucket->recent.key == hash) return &bucket->recent;

    return NULL;
}


static inline uint128_t get_legal_move(uint128_t p1, uint128_t p2, int col)
{
    uint128_t occupied = p1 | p2;
    int shift = col * bit_height;
    uint128_t col_bits = (occupied >> shift) & col_mask_n;
    if (col_bits == col_mask_n) return 0;
    return (col_bits + 1) << shift;
}

static inline uint128_t get_possible_moves(uint128_t p_me, uint128_t p_opp)
{
    uint128_t occupied = p_me | p_opp;
    return (occupied + global_bottom_mask) & global_board_mask & (~occupied);
}



static uint128_t compute_winning_spots(uint128_t p_me, uint128_t p_opp)
{
    uint128_t threats = (p_me << 1) & (p_me << 2) & (p_me << 3);

    {
        int d = bit_height;
        uint128_t l1 = p_me << d;
        uint128_t r1 = p_me >> d;

        uint128_t L = l1 & (l1 << d);
        uint128_t R = r1 & (r1 >> d);

        threats |= (L & (l1 << (2*d)));
        threats |= (R & (r1 >> (2*d)));
        threats |= (L & r1);
        threats |= (R & l1);
    }

    {
        int d = bit_height + 1;
        uint128_t l1 = p_me << d;
        uint128_t r1 = p_me >> d;
        uint128_t L = l1 & (l1 << d);
        uint128_t R = r1 & (r1 >> d);

        threats |= (L & (l1 << (2*d)));
        threats |= (R & (r1 >> (2*d)));
        threats |= (L & r1);
        threats |= (R & l1);
    }

    {
        int d = bit_height - 1;
        uint128_t l1 = p_me << d;
        uint128_t r1 = p_me >> d;
        uint128_t L = l1 & (l1 << d);
        uint128_t R = r1 & (r1 >> d);

        threats |= (L & (l1 << (2*d)));
        threats |= (R & (r1 >> (2*d)));
        threats |= (L & r1);
        threats |= (R & l1);
    }

    return threats & global_board_mask & ~(p_me | p_opp);
}


int score_move(uint128_t p_me, uint128_t p_opp, uint128_t move)
{
    uint128_t threats = compute_winning_spots(p_me | move, p_opp);

    return count_bits(threats);
}



static inline int has_won_board(uint128_t player)
{
    uint128_t m = player & (player >> 1);
    if (m & (m >> 2)) return 1;
    m = player & (player >> bit_height);
    if (m & (m >> (2 * bit_height))) return 1;
    m = player & (player >> (bit_height - 1));
    if (m & (m >> (2 * (bit_height - 1)))) return 1;
    m = player & (player >> (bit_height + 1));
    if (m & (m >> (2 * (bit_height + 1)))) return 1;
    return 0;
}

static inline int has_won_general(uint128_t player)
{
    int dirs[] = {1, bit_height, bit_height + 1, bit_height - 1};

    for (int d = 0; d < 4; d++)
    {
        int dir = dirs[d];
        uint128_t temp = player;
        for (int i = 1; i < k; i++)
        {
            temp &= (player >> (i * dir));
        }

        if (temp != 0) return 1;
    }
    return 0;
}

static inline uint128_t compute_winning_spots_general(uint128_t p_me, uint128_t p_opp)
{
    uint128_t threats = 0;
    int dirs[] = {1, bit_height, bit_height + 1, bit_height - 1};

    {
        uint128_t temp = p_me;
        for(int i=1; i < k; i++) temp &= (p_me >> i);
        threats |= (temp << 1);
    }


    for (int i = 1; i < 4; i++)
    {
        int d = dirs[i];

        for (int gap = 0; gap < k; gap++) {
            uint128_t mask = ~(uint128_t)0;

            for (int bit = 0; bit < k; bit++)
            {
                if (bit == gap) continue;
                int dist = bit - gap;
                if (dist > 0) mask &= (p_me >> (dist * d));
                else mask &= (p_me << ((-dist) * d));
            }
            threats |= mask;
        }
    }

    return threats & global_board_mask & ~(p_me | p_opp);
}

int get_immediate_win_move(uint128_t p_me, uint128_t p_opp)
{
    uint128_t threats = compute_winning_spots(p_me, p_opp);
    uint128_t occupied = p_me | p_opp;
    uint128_t empty = ~occupied;
    uint128_t legal_candidates = (occupied + global_bottom_mask) & global_board_mask & empty;
    uint128_t winning_moves = legal_candidates & threats;

    if (winning_moves == 0) return -1;

    int bit_idx = get_bit_index(winning_moves);
    return bit_idx / bit_height;
}

int general_score_move(uint128_t p_me, uint128_t p_opp, uint128_t move)
{
    uint128_t threats = compute_winning_spots_general(p_me | move, p_opp);

    return count_bits(threats);
}

static inline int16_t score_to_tt(int16_t score, int depth)
{
    if (score >= WIN_THRESHOLD) return score + depth;
    if (score <= -WIN_THRESHOLD) return score - depth;
    return score;
}

static inline int16_t score_from_tt(int16_t score, int depth)
{
    if (score >= WIN_THRESHOLD) return score - depth;
    if (score <= -WIN_THRESHOLD) return score + depth;
    return score;
}

int16_t general_negamax(uint128_t p_me, uint128_t p_opp, int8_t depth, int16_t alpha, int16_t beta, int color, uint64_t normal_hash, uint64_t mirror_hash, int8_t local_max_depth)
{
    total_nodes++;

    uint64_t hash = (normal_hash < mirror_hash) ? normal_hash : mirror_hash;
    TTnode* tt_entry = TT_check(hash);
    int left_depth = local_max_depth - depth;
    int tt_move = -1;

    if (tt_entry != NULL)
    {
        if (tt_entry->best_move != -1)
        {
            tt_move = tt_entry->best_move;
            if (hash == mirror_hash && normal_hash != mirror_hash) tt_move = (M - 1) - tt_move;
        }

        if (tt_entry->depth >= left_depth)
        {
            int16_t tt_value = score_from_tt(tt_entry->value, depth);
            if (tt_entry->flag == TT_EXACT) return tt_value;
            else if (tt_entry->flag == TT_ALPHA && tt_value <= alpha) return tt_value;
            else if (tt_entry->flag == TT_BETA && tt_value >= beta) return tt_value;
        }
    }

    if (has_won_general(p_opp)) return -(MAX_SCORE - depth);

    uint128_t winning_positions = compute_winning_spots_general(p_me, p_opp);
    uint128_t losing_positions = compute_winning_spots_general(p_opp, p_me);
    uint128_t possible = get_possible_moves(p_me, p_opp);

    if (winning_positions)
    {
        uint128_t winning_moves = possible & winning_positions;
        if (winning_moves)
        {
            int winning_col = get_bit_index(winning_moves) / bit_height;
            int col_to_save = winning_col;
            if (hash == mirror_hash && normal_hash != mirror_hash) col_to_save = (M - 1) - col_to_save;
            save_position_TT_s(hash, score_to_tt(MAX_SCORE - (depth + 1), depth), left_depth, TT_EXACT, col_to_save);
            return (MAX_SCORE - (depth + 1));
        }
    }

    if (possible == 0) return 0;

    uint128_t forced_moves = possible & losing_positions;

    if (forced_moves)
    {
        if (forced_moves & (forced_moves - 1))
        {
            int16_t loss_score = -(MAX_SCORE - (depth + 2));
            save_position_TT_s(hash, score_to_tt(loss_score, depth), left_depth, TT_EXACT, -1);
            return loss_score;
        }

        possible = forced_moves;
    }

    else
    {
        uint128_t suicide_moves = (losing_positions >> 1);
        possible &= ~suicide_moves;
        if (possible == 0)
        {
            int16_t loss_score = -(MAX_SCORE - (depth + 2));
            save_position_TT_s(hash, score_to_tt(loss_score, depth), left_depth, TT_EXACT, -1);
            return loss_score;
        }
    }

    if (depth >= local_max_depth)
    {
        if (use_heuristic) return (count_bits(winning_positions) - count_bits(losing_positions));
        return 0;
    }

    int16_t alpha_original = alpha;
    int16_t best_val = -INF;
    int best_col = -1;

    if (tt_move != -1 && (get_legal_move(p_me, p_opp, tt_move) & possible))
    {
        uint128_t move = get_legal_move(p_me, p_opp, tt_move);
        int bit_idx = get_bit_index(move);
        int mirror_bit_idx = mirror_lookup[bit_idx];
        uint64_t next_nh = normal_hash ^ zobrist_keys[color][bit_idx];
        uint64_t next_mh = mirror_hash ^ zobrist_keys[color][mirror_bit_idx];

        int16_t val = -general_negamax(p_opp, p_me | move, depth + 1, -beta, -alpha, 1 - color, next_nh, next_mh, local_max_depth);

        if (val > best_val) { best_val = val; best_col = tt_move; }
        if (val > alpha) alpha = val;

        if (alpha >= beta)
        {
            int col_to_save = best_col;
            if (hash == mirror_hash && normal_hash != mirror_hash) col_to_save = (M - 1) - col_to_save;
            save_position_TT_s(hash, score_to_tt(best_val, depth), left_depth, TT_BETA, col_to_save);
            return best_val;
        }
    }

    MoveEntry moves_list[M];
    int moves_count = 0;

    for (int i = 0; i < M; i++)
    {
        int col = order[i];
        if (col == tt_move) continue;

        uint128_t move = get_legal_move(p_me, p_opp, col);
        if ((move & possible) == 0) continue;

        int score = score_move(p_me, p_opp, move);
        int pos = moves_count++;
        while (pos > 0 && moves_list[pos - 1].score < score)
        {
            moves_list[pos] = moves_list[pos - 1];
            pos--;
        }
        moves_list[pos].col = col;
        moves_list[pos].move_bit = move;
        moves_list[pos].score = score;
    }

    if (moves_count == 0 && best_val == -INF) return 0;

    uint64_t* current_turn_keys = zobrist_keys[color];

    for (int i = 0; i < moves_count; i++)
    {
        int col = moves_list[i].col;
        uint128_t move = moves_list[i].move_bit;
        int bit_idx = get_bit_index(move);
        int mirror_bit_idx = mirror_lookup[bit_idx];
        uint64_t next_nh = normal_hash ^ current_turn_keys[bit_idx];
        uint64_t next_mh = mirror_hash ^ current_turn_keys[mirror_bit_idx];

        int16_t val;

        if (best_val <= -INF)
        {
            val = -general_negamax(p_opp, p_me | move, depth + 1, -beta, -alpha, 1 - color, next_nh, next_mh, local_max_depth);
        }
        else
        {
            val = -general_negamax(p_opp, p_me | move, depth + 1, -alpha - 1, -alpha, 1 - color, next_nh, next_mh, local_max_depth);
            if (val > alpha && val < beta)
            {
                val = -general_negamax(p_opp, p_me | move, depth + 1, -beta, -alpha, 1 - color, next_nh, next_mh, local_max_depth);
            }
        }

        if (val > best_val) { best_val = val; best_col = col; }
        if (val >= beta) break;
        if (val > alpha) alpha = val;
    }

    uint8_t flag = TT_EXACT;
    if (best_val <= alpha_original) flag = TT_ALPHA;
    else if (best_val >= beta) flag = TT_BETA;

    int col_to_save = best_col;
    if (col_to_save != -1 && hash == mirror_hash && normal_hash != mirror_hash) col_to_save = (M - 1) - col_to_save;
    save_position_TT_s(hash, score_to_tt(best_val, depth), left_depth, flag, col_to_save);

    return best_val;
}

int16_t negamax(uint128_t p_me, uint128_t p_opp, int8_t depth, int16_t alpha, int16_t beta, int color, uint64_t normal_hash, uint64_t mirror_hash, int8_t local_max_depth)
{
    total_nodes++;

    uint64_t hash = (normal_hash < mirror_hash) ? normal_hash : mirror_hash;
    TTnode* tt_entry = TT_check(hash);
    int left_depth = local_max_depth - depth;
    int tt_move = -1;

    if (tt_entry != NULL)
    {
        if (tt_entry->best_move != -1)
        {
            tt_move = tt_entry->best_move;
            if (hash == mirror_hash && normal_hash != mirror_hash) tt_move = (M - 1) - tt_move;
        }

        if (tt_entry->depth >= left_depth)
        {
            int16_t tt_value = score_from_tt(tt_entry->value, depth);
            if (tt_entry->flag == TT_EXACT) return tt_value;
            else if (tt_entry->flag == TT_ALPHA && tt_value <= alpha) return tt_value;
            else if (tt_entry->flag == TT_BETA && tt_value >= beta) return tt_value;
        }
    }

    if (has_won_general(p_opp)) return -(MAX_SCORE - depth);

    uint128_t winning_positions = compute_winning_spots(p_me, p_opp);
    uint128_t losing_positions = compute_winning_spots(p_opp, p_me);
    uint128_t possible = get_possible_moves(p_me, p_opp);

    if (winning_positions)
    {
        uint128_t winning_moves = possible & winning_positions;
        if (winning_moves)
        {
            int winning_col = get_bit_index(winning_moves) / bit_height;
            int col_to_save = winning_col;
            if (hash == mirror_hash && normal_hash != mirror_hash) col_to_save = (M - 1) - col_to_save;
            save_position_TT_s(hash, score_to_tt(MAX_SCORE - (depth + 1), depth), left_depth, TT_EXACT, col_to_save);
            return (MAX_SCORE - (depth + 1));
        }
    }

    if (possible == 0) return 0;

    uint128_t forced_moves = possible & losing_positions;
    if (forced_moves)
    {
        if (forced_moves & (forced_moves - 1))
        {
            int16_t loss_score = -(MAX_SCORE - (depth + 2));
            save_position_TT_s(hash, score_to_tt(loss_score, depth), left_depth, TT_EXACT, -1);
            return loss_score;
        }

        possible = forced_moves;
    }
    else
    {
        uint128_t suicide_moves = (losing_positions >> 1);
        possible &= ~suicide_moves;
        if (possible == 0)
        {
            int16_t loss_score = -(MAX_SCORE - (depth + 2));
            save_position_TT_s(hash, score_to_tt(loss_score, depth), left_depth, TT_EXACT, -1);
            return loss_score;
        }
    }

    if (depth >= local_max_depth)
    {
        if (use_heuristic) return (count_bits(winning_positions) - count_bits(losing_positions));
        return 0;
    }

    int16_t alpha_original = alpha;
    int16_t best_val = -INF;
    int best_col = -1;

    if (tt_move != -1 && (get_legal_move(p_me, p_opp, tt_move) & possible))
    {
        uint128_t move = get_legal_move(p_me, p_opp, tt_move);
        int bit_idx = get_bit_index(move);
        int mirror_bit_idx = mirror_lookup[bit_idx];
        uint64_t next_nh = normal_hash ^ zobrist_keys[color][bit_idx];
        uint64_t next_mh = mirror_hash ^ zobrist_keys[color][mirror_bit_idx];

        int16_t val = -negamax(p_opp, p_me | move, depth + 1, -beta, -alpha, 1 - color, next_nh, next_mh, local_max_depth);

        if (val > best_val) { best_val = val; best_col = tt_move; }
        if (val > alpha) alpha = val;

        if (alpha >= beta)
        {
            int col_to_save = best_col;
            if (hash == mirror_hash && normal_hash != mirror_hash) col_to_save = (M - 1) - col_to_save;
            save_position_TT_s(hash, score_to_tt(best_val, depth), left_depth, TT_BETA, col_to_save);
            return best_val;
        }
    }

    MoveEntry moves_list[M];
    int moves_count = 0;

    for (int i = 0; i < M; i++)
    {
        int col = order[i];
        if (col == tt_move) continue;

        uint128_t move = get_legal_move(p_me, p_opp, col);
        if ((move & possible) == 0) continue;

        int score = score_move(p_me, p_opp, move);
        int pos = moves_count++;
        while (pos > 0 && moves_list[pos - 1].score < score)
        {
            moves_list[pos] = moves_list[pos - 1];
            pos--;
        }
        moves_list[pos].col = col;
        moves_list[pos].move_bit = move;
        moves_list[pos].score = score;
    }

    if (moves_count == 0 && best_val == -INF) return 0;

    uint64_t* current_turn_keys = zobrist_keys[color];

    for (int i = 0; i < moves_count; i++)
    {
        int col = moves_list[i].col;
        uint128_t move = moves_list[i].move_bit;
        int bit_idx = get_bit_index(move);
        int mirror_bit_idx = mirror_lookup[bit_idx];
        uint64_t next_nh = normal_hash ^ current_turn_keys[bit_idx];
        uint64_t next_mh = mirror_hash ^ current_turn_keys[mirror_bit_idx];

        int16_t val;

        if (best_val <= -INF)
        {
            val = -negamax(p_opp, p_me | move, depth + 1, -beta, -alpha, 1 - color, next_nh, next_mh, local_max_depth);
        }
        else
        {
            val = -negamax(p_opp, p_me | move, depth + 1, -alpha - 1, -alpha, 1 - color, next_nh, next_mh, local_max_depth);
            if (val > alpha && val < beta)
            {
                val = -negamax(p_opp, p_me | move, depth + 1, -beta, -alpha, 1 - color, next_nh, next_mh, local_max_depth);
            }
        }

        if (val > best_val) { best_val = val; best_col = col; }
        if (val >= beta) break;
        if (val > alpha) alpha = val;
    }

    uint8_t flag = TT_EXACT;
    if (best_val <= alpha_original) flag = TT_ALPHA;
    else if (best_val >= beta) flag = TT_BETA;

    int col_to_save = best_col;
    if (col_to_save != -1 && hash == mirror_hash && normal_hash != mirror_hash) col_to_save = (M - 1) - col_to_save;
    save_position_TT_s(hash, score_to_tt(best_val, depth), left_depth, flag, col_to_save);

    return best_val;
}



int get_best_move_general_negamax(Bitboard *root, double allowed_seconds, int verbose)
{
    if (transposition_table == NULL) init_zobrist();

    memset(nodes, 0, sizeof(nodes));
    total_nodes = 0;



    clock_t start_loop_time = clock();
    int best_move_final = -1;
    int16_t score_final = 0;

    for(int i=0; i<M; i++)
    {
        if(get_legal_move(root->player_1, root->player_2, order[i]))
        {
            best_move_final = order[i];
            break;
        }
    }

    uint64_t h = compute_hash(root->player_1, root->player_2);
    uint64_t hm = compute_hash(mirror_board(root->player_1), mirror_board(root->player_2));
    uint64_t hash_key = (h < hm) ? h : hm;
    uint128_t occupied = root->player_1 | root->player_2;
    int pieces_on_board = count_bits(occupied);
    int max_possible_depth = N*M - pieces_on_board;
    uint64_t nodes_last_iteration = 0;
    uint64_t nodes_at_start_of_iter = 0;
    int reached_depth = 0;
    for (int depth = 1; depth <= max_possible_depth + 1; depth++)
    {
        nodes_at_start_of_iter = total_nodes;
        reached_depth = depth;

        int16_t score = general_negamax(root->player_1, root->player_2, 0, -INF, INF, 0, h, hm, depth);

        TTnode* entry = TT_check(hash_key);

        if (entry != NULL && entry->key == hash_key && entry->best_move != -1)
        {
            int move = entry->best_move;
            if (hash_key == hm && h != hm) move = (M - 1) - move;
            best_move_final = move;
            score_final = score;
        }

        double total_elapsed = (double)(clock() - start_loop_time) / CLOCKS_PER_SEC;
        double ebf = 0.0;
        uint64_t nodes_this_iteration = total_nodes - nodes_at_start_of_iter;

        if (nodes_last_iteration > 0)
        {
            ebf = (double)nodes_this_iteration / (double)nodes_last_iteration;
        }

        nodes_last_iteration = nodes_this_iteration;

        if (verbose)
        {
            printf("Depth %2d: Score %6d, Move %d, Nodes: %llu, EBF:%.2f, Time: %.3fs\n",
                   depth, score, best_move_final + 1, (unsigned long long)total_nodes, ebf, total_elapsed);
        }

        if (score >= WIN_THRESHOLD || score <= -WIN_THRESHOLD) break;

        if (total_elapsed >= allowed_seconds) break;
    }

    double final_time = (double)(clock() - start_loop_time) / CLOCKS_PER_SEC;

    if (score_final >= WIN_THRESHOLD)
    {
        int plies = MAX_SCORE - score_final;
        printf(">>> VICTORY IN %d MOVES (Total time: %.3fs) <<<\n", (plies + 1) / 2, final_time);
        fflush(stdout);
        wait_seconds(1);
    }
    else if (score_final <= -WIN_THRESHOLD)
    {
        int plies = MAX_SCORE + score_final;
        printf(">>> POTENTIAL DEFEAT IN %d MOVES (Total time: %.3fs) <<<\n", (plies + 1) / 2, final_time);
        fflush(stdout);
        wait_seconds(1);
    }
    else
    {
        printf("Search finished. Depth: %d. Time: %.3fs\n", reached_depth, final_time);
    }
    fflush(stdout);
    root->value = score_final;
    return best_move_final;
}



int get_best_move_negamax(Bitboard *root, double allowed_seconds, int verbose)
{
    if (transposition_table == NULL) init_zobrist();

    memset(nodes, 0, sizeof(nodes));
    total_nodes = 0;


    int win_col = get_immediate_win_move(root->player_1, root->player_2);
    if (win_col != -1)
    {
        root->value = MAX_SCORE;
        if(verbose) printf("Instant win found.\n");
        return win_col;
    }

    clock_t start_loop_time = clock();
    int best_move_final = -1;
    int16_t score_final = 0;

    for(int i=0; i<M; i++)
    {
        if(get_legal_move(root->player_1, root->player_2, order[i]))
        {
            best_move_final = order[i];
            break;
        }
    }

    uint64_t h = compute_hash(root->player_1, root->player_2);
    uint64_t hm = compute_hash(mirror_board(root->player_1), mirror_board(root->player_2));
    uint64_t hash_key = (h < hm) ? h : hm;
    uint128_t occupied = root->player_1 | root->player_2;
    int pieces_on_board = count_bits(occupied);
    int max_possible_depth = N*M - pieces_on_board;
    uint64_t nodes_last_iteration = 0;
    uint64_t nodes_at_start_of_iter = 0;
    int reached_depth = 0;
    for (int depth = 1; depth <= max_possible_depth + 1; depth++)
    {
        nodes_at_start_of_iter = total_nodes;
        reached_depth = depth;

        int16_t score = negamax(root->player_1, root->player_2, 0, -INF, INF, 0, h, hm, depth);

        TTnode* entry = TT_check(hash_key);

        if (entry != NULL && entry->key == hash_key && entry->best_move != -1)
        {
            int move = entry->best_move;
            if (hash_key == hm && h != hm) move = (M - 1) - move;
            best_move_final = move;
            score_final = score;
        }

        double total_elapsed = (double)(clock() - start_loop_time) / CLOCKS_PER_SEC;
        double ebf = 0.0;
        uint64_t nodes_this_iteration = total_nodes - nodes_at_start_of_iter;

        if (nodes_last_iteration > 0)
        {
            ebf = (double)nodes_this_iteration / (double)nodes_last_iteration;
        }

        nodes_last_iteration = nodes_this_iteration;

        if (verbose)
        {
            printf("Depth %2d: Score %6d, Move %d, Nodes: %llu, EBF:%.2f, Time: %.3fs\n",
                   depth, score, best_move_final + 1, (unsigned long long)total_nodes, ebf, total_elapsed);
        }

        if (score >= WIN_THRESHOLD || score <= -WIN_THRESHOLD) break;

        if (total_elapsed >= allowed_seconds) break;
    }

    double final_time = (double)(clock() - start_loop_time) / CLOCKS_PER_SEC;

    if (score_final >= WIN_THRESHOLD && verbose)
    {
        int plies = MAX_SCORE - score_final;
        printf(">>> VICTORY IN %d MOVES (Total time: %.3fs) <<<\n", (plies + 1) / 2, final_time);
        wait_seconds(1);
    }
    else if (score_final <= -WIN_THRESHOLD && verbose)
    {
        int plies = MAX_SCORE + score_final;
        printf(">>> POTENTIAL DEFEAT IN %d MOVES (Total time: %.3fs) <<<\n", (plies + 1) / 2, final_time);
        wait_seconds(1);
    }
    else
    {
        printf("Search finished. Depth: %d. Time: %.3fs\n", reached_depth, final_time);
        wait_seconds(1);
    }
    fflush(stdout);
    root->value = score_final;
    return best_move_final;
}


int assignTable_board(Bitboard *receptor, char table[MAX_N][MAX_M])
{
    receptor->player_1 = 0;
    receptor->player_2 = 0;
    for (int row = 0; row < N; row++)
    {
        for (int col = 0; col < M; col++)
        {
            if (table[row][col] != '-')
            {
                int bit_row = top_row - row;
                uint128_t mask = ((uint128_t)1) << (col * bit_height + bit_row);
                if (table[row][col] == 'X') receptor->player_1 |= mask;
                else receptor->player_2 |= mask;
            }
        }
    }
    return 1;
}