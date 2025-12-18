#include "config.h"
#include "bitboard.h"
#define CURSOR_HOME "\033[H"
#define HIDE_CURSOR "\033[?25l"
#define SHOW_CURSOR "\033[?25h"

int N = 6;
int M = 7;
int k = 4;
int order[MAX_M];
extern TTBucket* transposition_table;
int use_heuristic = 0;


double ai_time_limit = 5.0;

void generate_column_order(int *order);

double uniform_0_1()
{
    return (double) rand() / (double) RAND_MAX;
}


void clear_screen_with_delay(double seconds)
{
    wait_seconds(seconds);

    printf("\033[H\033[J");

    fflush(stdout);
}

void fill_with_dash(char table[MAX_N][MAX_M])
{
    for (int i = 0; i < N; i++)
        for (int j = 0; j < M; j++)
            table[i][j] = '-';
}


void spin_table(int revolution, char table[MAX_N][MAX_M])
{
    if ((revolution == 1) || (revolution == -1))
    {
        for (int i = 0; i < N; i++)
            for (int j = i; j < M; j++)
            {
                char auxiliar = table[i][j];
                table[i][j] = table[j][i];
                table[j][i] = auxiliar;
            }
    }

    if (revolution == 1)
    {
        for (int j = 0; j < M; j++)
            for (int i = 0; i < N / 2; i++)
            {
                char auxiliar = table[i][j];
                table[i][j] = table[N - i - 1][j];
                table[N - i - 1][j] = auxiliar;
            }
    }
    else if (revolution == -1)
    {
        for (int i = 0; i < N; i++)
            for (int j = 0; j < M / 2; j++)
            {
                char auxiliar = table[i][j];
                table[i][j] = table[i][M - j - 1];
                table[i][M - j - 1] = auxiliar;
            }
    }
    else
    {
        spin_table(1, table);
        spin_table(1, table);
    }
}

void apply_gravity_column(char table[MAX_N][MAX_M], int column)
{
    int counter = N - 1;
    for (int i = N - 1; i >= 0; i--)
    {
        if (table[i][column] != '-')
        {
            if (counter > i) {
                table[counter][column] = table[i][column];
                table[i][column] = '-';
            }
            counter--;
        }
    }
}

void apply_gravity(char table[MAX_N][MAX_M])
{
    for (int j = 0; j < M; j++)
        apply_gravity_column(table, j);
}


int action_animated(char table[MAX_N][MAX_M], int column, char player)
{
    if (column < 0 || column >= M || table[0][column] != '-')
    {
        return -1;
    }

    printf(HIDE_CURSOR);
    fflush(stdout);
    clear_screen_with_delay(0);
    show_table(table);

    for (int row = 0; row < N; row++)
    {
        if (row == N-1 || table[row+1][column] != '-')
        {
            clear_screen_with_delay(0.025);
            table[row][column] = player;
            show_table(table);
            #ifdef _WIN32

            #else
            printf("\a");
            #endif
            return row;
        }

        table[row][column] = player;
        clear_screen_with_delay(0.025);
        show_table(table);
        table[row][column] = '-';
    }
    return -1;
}


char full_line_detector(char table[MAX_N][MAX_M])
{
    for (int i = 0; i < N; i++)
    {
        for (int j = 0; j < M; j++)
        {
            char player = table[i][j];
            if (player == '-') continue;

            int dirs[4][2] = {{0,1}, {1,0}, {1,1}, {1,-1}};

            for (int d = 0; d < 4; d++)
            {
                int r = i, c = j, count = 0;
                for (int step = 0; step < k; step++)
                {
                    if (r >= 0 && r < N && c >= 0 && c < M && table[r][c] == player)
                        count++;
                    else
                        break;

                    r += dirs[d][0];
                    c += dirs[d][1];
                }
                if (count == k) return player;
            }
        }
    }
    return '-';
}

int detectTie(char table[MAX_N][MAX_M])
{
    for (int i = 0; i < N; i++)
        for (int j = 0; j < M; j++)
            if (table[i][j] == '-') return 0;
    return 1;
}

char change_player(char player)
{
    return (player == 'X') ? 'O' : 'X';
}

int get_human_move(char player)
{
    int col;
    while (1)
    {
        printf("Choose column for '%c' (1-%d): ", player, M);
        if (scanf("%d", &col) == 1)
        {
            if (col >= 1 && col <= M) return col - 1;
            printf("This column is outside the board. Maybe in another DLC\n");
        }
        else
        {
            while(getchar() != '\n');
            printf("You know this is not a column.\n");
        }
    }
}

char process_spin(char table[MAX_N][MAX_M], double prob)
{
    if (prob > 0 && uniform_0_1() <= prob)
    {
        printf("¡Let's rotate!'...\n");
        spin_table(rand() % 2 == 0 ? 1 : -1, table);
        clear_screen_with_delay(1);
        show_table(table);
        clear_screen_with_delay(0.5);
        apply_gravity(table);
        show_table(table);
        return full_line_detector(table);
    }
    return '-';
}



int get_move_bitboard_AI(char table[MAX_N][MAX_M], double limit_time_seconds, int verbose)
{
    Bitboard root;
    assignTable_board(&root, table);
    if (k == 4) {return get_best_move_negamax(&root, limit_time_seconds, verbose);}
    else {return get_best_move_general_negamax(&root, limit_time_seconds, verbose);}
}

int HumanVsHuman(char initial_character, double spin_probability)
{
    char table[MAX_N][MAX_M];
    fill_with_dash(table);
    char winner = '-';
    char spin_winner = '-';
    char current_player = initial_character;
    int row_played, col, tie = 0;

    clear_screen_with_delay(0);
    show_table(table);

    while (winner == '-' && !tie)
    {
        row_played = -1;
        while (row_played == -1)
        {
            col = get_human_move(current_player);
            row_played = action_animated(table, col, current_player);
            if (row_played == -1) { printf("This column is full.\n"); wait_seconds(1); }
        }
        char spin_winner = process_spin(table, spin_probability);
        if (spin_winner != '-') winner = spin_winner;

        else
        {
            if (full_line_detector(table) != '-') winner = current_player;
            else tie = detectTie(table);
        }
        current_player = change_player(current_player);
    }
    show_table(table);
    if (winner != '-') printf("Winner: %c\n", winner);
    else if (spin_winner != '-' )printf("Winner: %c\n", spin_winner);
    else printf("Tie\n");
    return 0;
}


int HumanVsMachine_board(char first_player, double spin_probability, double time_limit)
{
    clear_screen_with_delay(0);
    printf("Do you want to play while seeing how the model thinks? (It’s cool!)\n1: Yes, 0: No");
    int verbose = 1;
    scanf("%d", &verbose);
    init_zobrist();
    char table[MAX_N][MAX_M];
    fill_with_dash(table);
    char winner = '-';
    char spin_winner = '-';
    char current_player = first_player;
    int row_played, col, tie = 0;

    clear_screen_with_delay(0);
    show_table(table);

    while (winner == '-' && !tie)
    {
        row_played = -1;
        while (row_played == -1)
        {
            if (current_player == 'O') col = get_human_move(current_player);

            else
            {
                printf("AI Thinking (Limit: %.1fs)...\n", time_limit);
                col = get_move_bitboard_AI(table, time_limit, verbose);
            }

            row_played = action_animated(table, col, current_player);
            if (row_played == -1 && current_player == 'O') { printf("Full.\n"); wait_seconds(1); }
        }
        char spin_winner = process_spin(table, spin_probability);
        if (spin_winner != '-') winner = spin_winner;
        else
        {
            if (full_line_detector(table) != '-') winner = current_player;
            else tie = detectTie(table);
        }
        current_player = change_player(current_player);
    }
    show_table(table);
    if (winner != '-')
    {
        if (winner == 'X') printf("Oh, you lost with the bitboard bot! This is not a surprise!\n");
        else if (winner == 'O') printf("Wow. That was amazing\n");
    }

    else if (spin_winner != '-')
    {
        if (spin_winner == 'X') printf("Oh, you lost with the bitboard bot and you had the help of the spin!\n");
        else if (spin_winner == 'O') printf("Well done! Now, try to win with spin probability set to 0\n");
    }

    else printf("Tie. Well done\n");
    return 0;
}

void game_menu()
{
    int opt = -1;
    char start_player = 'X';
    double spin_prob = 0.0;

    while (opt != 0)
    {
        clear_screen_with_delay(0);
        printf("\n=== CONFIGURATION ===\n");
        printf("Board: %dx%d (Connect %d) | You: O (AI: X) | Start: %c | Spin: %.0f%% | AI Time: %.1fs\n",
                N, M, k, start_player, spin_prob * 100.0, ai_time_limit);
        printf("----------------------------------------------------------------------\n");
        printf("1. Play vs Bitboard AI (Maximum depth, very hard on medium boards)\n");
        printf("2. Play vs Bitboard AI (with heuristic).\n");
        printf("3. Play with someone else\n");
        printf("4. Change board settings\n");
        printf("5. Change starting player\n");
        printf("6. Set Spin Probability\n");
        printf("7. Set AI Time Limit\n");
        printf("0. Exit\n");
        printf(">> ");

        if (scanf("%d", &opt) != 1) { while(getchar()!='\n'); continue; }

        if (opt == 2 || opt == 3)
        {
            if ((N + 1) * M > 120)
            {
                printf("Warning: Board too big for Bitboard AI. Resizing to 6x7...\n");
                N = 6; M = 7;
                if (transposition_table) { free(transposition_table); transposition_table = NULL; }
                generate_column_order(order);
                wait_seconds(2);
            }
        }

        double current_game_prob = spin_prob;
        if ((opt >= 1 && opt <= 4) && spin_prob > 0 && N != M)
        {
             printf("Warning: Spin only works on square boards (NxN).\n");
             printf("Starting game without spin...\n");
             current_game_prob = 0.0;
             wait_seconds(2);
        }

        switch(opt)
        {
            case 1:
                use_heuristic = 0;
                HumanVsMachine_board(start_player, current_game_prob, ai_time_limit);
                break;

            case 2:
                use_heuristic = 1;
                HumanVsMachine_board(start_player, current_game_prob, ai_time_limit);
                break;

            case 3:
                HumanVsHuman(start_player, current_game_prob);
                break;

            case 4:
                printf("Rows (4-%d): ", MAX_N); scanf("%d", &N);
                if (N > MAX_N) N = MAX_N;
                if (N < 4) N = 4;

                printf("Cols (4-%d): ", MAX_M); scanf("%d", &M);
                if (M > MAX_M) M = MAX_M;
                if (M < 4) M = 4;

                printf("K: "); scanf("%d", &k);

                if ((k > N) || (k > M) || (k < 2))
                {
                    k = 4;
                }

                if (transposition_table) { free(transposition_table); transposition_table = NULL; }
                generate_column_order(order);
                init_zobrist();

                printf("Settings updated.\n");
                wait_seconds(1);
                break;
            case 5:
                start_player = (start_player == 'O') ? 'X' : 'O';
                printf("Starting player set to: %c\n", start_player);
                wait_seconds(1);
                break;

            case 6:
                printf("Enter probability (0-100): ");
                int p;
                scanf("%d", &p);
                if(p < 0) p = 0;
                if(p > 100) p = 100;
                spin_prob = (double)p / 100.0;
                printf("Spin probability set to %.2f\n", spin_prob);
                wait_seconds(1);
                break;

            case 7:
                printf("Enter Bitboard AI thinking time in seconds (e.g., 5.0 by default): ");
                double t;
                scanf("%lf", &t);
                if (t < 0.1) t = 0.1;
                ai_time_limit = t;
                printf("AI time limit set to %.2fs. Remember that this is a lower bound, since the AI never stops in the middle of an iteration. \nIf you want a challenge, I personally recommend 5 seconds or more. For k = 4, the time will be accurate.\n", ai_time_limit);

                wait_seconds(4);
                break;

            case 0:
                printf("Bye.\n");
                if (transposition_table)
                {
                    free(transposition_table);
                    transposition_table = NULL;
                }
                exit(0);
        }

        if(opt != 4 && opt != 5 && opt != 6 && opt != 7) {
            printf("\nPress enter...");
            while(getchar()!='\n') { }
            getchar();
        }
    }
}

int main(void)
{
    #ifdef _WIN32
        #include <windows.h>
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD dwMode = 0;
        GetConsoleMode(hOut, &dwMode);
        dwMode |= 0x0004;
        SetConsoleMode(hOut, dwMode);
    #endif

    srand(time(NULL));
    game_menu();
    return 0;
}
