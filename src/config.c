#include "config.h"


void generate_column_order(int *order)
{
    int idx = 0;
    int center = (M - 1) / 2;

    order[idx++] = center;
    int offset = 1;
    while (idx < M)
    {
        int right = center + offset;
        int left = center - offset;
        if (right < M) order[idx++] = right;
        if (left >= 0)  order[idx++] = left;
        offset++;
    }
}


void show_table(char table[MAX_N][MAX_M])
{
    printf("  ┌");
    for (int j = 0; j < M - 1; j++) printf("───┬");
    printf("───┐\n");

    for (int i = 0; i < N; i++)
    {
        printf("  │");
        for (int j = 0; j < M; j++)
        {
            if (table[i][j] == 'X')
            {
                printf(RED " ✖ " RESET);

            }
            else if (table[i][j] == 'O')
            {
                printf(BLUE " ● " RESET);
            }
            else {
                printf("   ");
            }
            printf("│");
        }
        printf("\n");

        if (i < N - 1)
        {
            printf("  ├");
            for (int j = 0; j < M - 1; j++) printf("───┼");
            printf("───┤\n");
        }
    }

    printf("  └");
    for (int j = 0; j < M - 1; j++) printf("───┴");
    printf("───┘\n");

    printf("   ");
    for (int j = 0; j < M; j++)
    {
        if (j < 9) printf(" %d  ", j + 1);
        else       printf(" %d ", j + 1);
    }
    printf("\n");
}


void wait_seconds(double seconds)
{
    fflush(stdout);

    if (seconds <= 0.0) return;

    #ifdef _WIN32
        Sleep((DWORD)(seconds * 1000));
    #else
        struct timespec ts;
        ts.tv_sec = (time_t)seconds;
        ts.tv_nsec = (long)((seconds - ts.tv_sec) * 1e9);
        nanosleep(&ts, NULL);
    #endif
}


void init_globals() {
    generate_column_order(order);
}