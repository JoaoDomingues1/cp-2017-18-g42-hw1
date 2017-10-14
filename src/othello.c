//
//  othello.c
//
//  Created by Bernardo Ferreira and João Lourenço on 22/09/17.
//  Copyright (c) 2017 DI-FCT-UNL. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
//new include
#include <cilk/cilk.h>
#include <cilk/cilk_api.h>
#include <limits.h>
#include <cilk/reducer_max.h>

#define RED     "\x1B[31m"
#define BLU     "\x1B[34m"
#define RESET   "\x1B[0m"
#define TOPLEFT "\x1B[0;0H"
#define CLEAR   "\x1B[2J"

#define E '-'
#define R 'R'
#define B 'B'

#define TRUE  1
#define FALSE 0

typedef struct{
	int i;
	int j;
	char turn;
	int right;
	int left;
	int up;
	int down;
	int up_right;
	int up_left;
	int down_right;
	int down_left;
	int heuristic;
} move;

typedef struct {
	char** gameboard;
	int parent_cant_play;
	cilk_c_reducer_max_int alpha;
	cilk_c_reducer_max_int beta;
} state;

struct timespec start, finish;
double elapsed;

char time_elapsed = 'n';
char print_mode = 'n';
int anim_mode = 0;
int board_size = 8;
int delay = 0;
int threads = 1;
int nMinMaxLevels = 1;

char** gameboard;

void calculate_elapsed() {

	clock_gettime(CLOCK_MONOTONIC, &finish);
	elapsed = (finish.tv_sec - start.tv_sec);
	elapsed += (finish.tv_nsec - start.tv_nsec) / 1000000000.0;
}

void print_time() {
	calculate_elapsed();

	printf("time :%f\n", elapsed);
}

void free_gameboard(char** board) {
	for (int i = 0; i < board_size; i++)
		free(board[i]);
	free(board);
}

void copyBoard(char** b1, char** b2)
{
    for (int i = 0; i < board_size; i++)
	{
		for (int j = 0; j < board_size; j++)
		{
		    b1[i][j] = b2[i][j];
		}
    }
}

void init_move(move* m, int i, int j, char turn) {
	m->i = i;
	m->j = j;
	m->turn = turn;
	m->right = 0;
	m->left = 0;
	m->up = 0;
	m->down = 0;
	m->up_right = 0;
	m->up_left = 0;
	m->down_right = 0;
	m->down_left = 0;
	m->heuristic = 0;
}

int valid_move(int i, int j) {
	if (i >= 0 && i < board_size && j >= 0 && j < board_size)
		return TRUE;
	else
		return FALSE;
}

void print_board(char** board) {
	if (print_mode == 's')
		return;
	if (anim_mode) {
		printf(TOPLEFT);
	}
	for (int i = 0; i < board_size; i++) {
		for (int j = 0; j < board_size; j++) {
			const char c = board[i][j];
			if (print_mode == 'n')
				printf("%c ", c);
			else if (print_mode == 'c') {
				if (c == R)
					printf("%s%c %s", RED, R, RESET);
				else if (c == B)
					printf("%s%c %s", BLU, B, RESET);
				else
					printf("%s%c %s", "", E, "");
			}
		}
		printf("\n");
	}
	for (int i = 0; i < 2 * board_size; i++)
		printf("=");
	printf("\n");
}

int score(char turn, char** board)
{
	int res = 0;
	for (int i = 0; i < board_size; i++) {
		for (int j = 0; j < board_size; j++) {
			if (board[i][j] == turn)
				res++;
		}
	}
	return res;
}

void print_scores() {
	int w = score(R, gameboard);
	int b = score(B, gameboard);
	printf("score - red:%i blue:%i\n", w, b);
}

void free_board(char** board) {
	for (int i = 0; i < board_size; i++)
		free(board[i]);
	free(board);
}

void finish_game() {

	print_board(gameboard);
	if (time_elapsed == 'y') {
		print_time();
	}
	print_scores();
	free_board(gameboard);
}

char** build_board()
{
    char** board = (char**)malloc(board_size * sizeof(char*));
    for (int i = 0; i < board_size; i++)
	{
        board[i] = (char*)malloc(board_size * sizeof(char));
        for (int j = 0; j < board_size; j++)
            board[i][j] = E;
    }
	return board;
}

void init_board(char** board)
{
    board[board_size/2 - 1][board_size/2 - 1] = R;
    board[board_size/2][board_size/2 - 1] = B;
    board[board_size/2 - 1][board_size/2] = B;
    board[board_size/2][board_size/2] = R;
}

void init_state(state* s, char** board)
{
	s->gameboard = build_board();
	copyBoard(s->gameboard, board);
	s->parent_cant_play = FALSE;
	//s->alpha = INT_MIN;
	//s->beta = INT_MAX;
}

//devolve o oponente do jogador indicado
char opponent(char turn) {
	if (turn == R)
		return B;
	else if (turn == B)
		return R;
	else
		return -1; //ERROR
}

void flip_direction (move* m, int inc_i, int inc_j, char** board)
{
	int i = m->i + inc_i;
	int j = m->j + inc_j;
	while (board[i][j] != m->turn)
	{
		board[i][j] = m->turn;
		i += inc_i;
		j += inc_j;
	}
}

void flip_board(move* m, char** board)
{
    board[m->i][m->j] = m->turn;
    if (m->right)
        flip_direction(m,1,0, board); //right
    if (m->left)
        flip_direction(m,-1,0, board); //left
    if (m->up)
        flip_direction(m,0,-1, board); //up
    if (m->down)
        flip_direction(m,0,1, board); //down
    if (m->up_right)
        flip_direction(m,1,-1, board); //up right
    if (m->up_left)
        flip_direction(m,-1,-1, board); //up left
    if (m->down_right)
        flip_direction(m,1,1, board); //down right
    if (m->down_left)
        flip_direction(m,-1,1, board); //down left
}

int get_direction_heuristic(move* m, char opp, int inc_i, int inc_j, char** board) {
	int heuristic = 0;
	int i = m->i + inc_i;
	int j = m->j + inc_j;
	char curr = opp;
	
	while (valid_move(i, j)) {
		curr = board[i][j];
		if (curr != opp)
			break;
		heuristic++;
		i += inc_i;
		j += inc_j;
	}
	if (curr == m->turn)
	{
		return heuristic;
	}
	else
		return 0;
}

void get_move(move* m, char** board) {
	if (board[m->i][m->j] != E)
		return;
	char opp = opponent(m->turn);
	int heuristic;

	heuristic = get_direction_heuristic(m, opp, 1, 0, board); //right
	if (heuristic > 0) {
		m->heuristic += heuristic;
		m->right = 1;
	}
	heuristic = get_direction_heuristic(m, opp, -1, 0, board); //left
	if (heuristic > 0) {
		m->heuristic += heuristic;
		m->left = 1;
	}
	heuristic = get_direction_heuristic(m, opp, 0, -1, board); //up
	if (heuristic > 0) {
		m->heuristic += heuristic;
		m->up = 1;
	}
	heuristic = get_direction_heuristic(m, opp, 0, 1, board); //down
	if (heuristic > 0) {
		m->heuristic += heuristic;
		m->down = 1;
	}
	heuristic = get_direction_heuristic(m, opp, 1, -1, board); //up right
	if (heuristic > 0) {
		m->heuristic += heuristic;
		m->up_right = 1;
	}
	heuristic = get_direction_heuristic(m, opp, -1, -1, board); //up left
	if (heuristic > 0) {
		m->heuristic += heuristic;
		m->up_left = 1;
	}
	heuristic = get_direction_heuristic(m, opp, 1, 1, board); //down right
	if (heuristic > 0) {
		m->heuristic += heuristic;
		m->down_right = 1;
	}
	heuristic = get_direction_heuristic(m, opp, -1, 1, board); //down left
	if (heuristic > 0) {
		m->heuristic += heuristic;
		m->down_left = 1;
	}
}

int minMax(state *prev, move moveT, int depth)
{
	//printf("%d\n", REDUCER_VIEW(prev->alpha));
	//printf("%d\n", REDUCER_VIEW(prev->beta));

	state current;
	init_state(&current, prev->gameboard);
	flip_board(&moveT, current.gameboard);
	if(depth <= 0)
	{
		return score(moveT.turn,  current.gameboard);
	}
	
	CILK_C_REDUCER_MAX(alpha, int, -REDUCER_VIEW(prev->beta));
	current.alpha = alpha;
	CILK_C_REGISTER_REDUCER(current.alpha);
	
	CILK_C_REDUCER_MAX(beta, int, -REDUCER_VIEW(prev->alpha));
	current.beta = beta;
	CILK_C_REGISTER_REDUCER(current.beta);
	
	//printf("%d\n", REDUCER_VIEW(current.alpha));
	//printf("%d\n", REDUCER_VIEW(current.beta));
	
	int cutoff = FALSE;

	CILK_C_REDUCER_MAX(bestscore, int, INT_MIN);
	CILK_C_REGISTER_REDUCER(bestscore);
	
	void get_score(int child_sc)
	{
		child_sc = -child_sc;
		
		if(child_sc > REDUCER_VIEW(bestscore))
		{
			CILK_C_REDUCER_MAX_CALC(bestscore, child_sc);
			if(child_sc > REDUCER_VIEW(current.alpha))
			{
				CILK_C_REDUCER_MAX_CALC(current.alpha, child_sc);
				if(child_sc > REDUCER_VIEW(current.beta))
				{
					CILK_C_REDUCER_MAX_CALC(current.beta, child_sc);
					cutoff = TRUE;
				}
			}
		}
	}
	
	int firstFound = FALSE;
	move m;
	
	for(int i = 0; i < board_size && !cutoff; i++)
	{
		for(int j = 0; j < board_size && !cutoff; j++)
		{
			init_move(&m,i,j,opponent(moveT.turn));
            get_move(&m, current.gameboard);
			if(m.heuristic > 0)
			{
				cilk_spawn get_score(minMax(prev, m, depth-1));
				if(!firstFound)
				{
					cilk_sync;
					firstFound = TRUE;
				}
			}
		}
	}
	cilk_sync;
	
	if(bestscore.value == INT_MIN)
	{
		if(prev->parent_cant_play)
		{
			CILK_C_UNREGISTER_REDUCER(bestscore);
			CILK_C_UNREGISTER_REDUCER(current.alpha);
			CILK_C_UNREGISTER_REDUCER(current.beta);
			return score(moveT.turn,  current.gameboard);
		}
		else
		{
			state a;
			init_state(&a, prev->gameboard);
			a.alpha = current.alpha;
			a.beta = current.beta;
			a.parent_cant_play = TRUE;
			get_score(minMax(&a, moveT, depth-1));
			
		}
	}
	else
	{
		current.parent_cant_play = FALSE;
	}
	free_gameboard(current.gameboard);
	int result = bestscore.value;
	CILK_C_UNREGISTER_REDUCER(bestscore);
	CILK_C_UNREGISTER_REDUCER(current.alpha);
	CILK_C_UNREGISTER_REDUCER(current.beta);
	return result;
}

int make_move(char turn, int depth)
{
    move best_move;
    best_move.heuristic = INT_MIN;
	state s;
	init_state(&s, gameboard);
	
	CILK_C_REDUCER_MAX(alpha, int, INT_MIN);
	s.alpha = alpha;
	CILK_C_REGISTER_REDUCER(s.alpha);
	
	CILK_C_REDUCER_MAX(beta, int, INT_MAX);
	s.beta = beta;
	CILK_C_REGISTER_REDUCER(s.beta);
	
	
	//int best_score = INT_MIN;
	
	//CILK_C_REDUCER_MAX(best_score, int, INT_MIN);
	//CILK_C_REGISTER_REDUCER(best_score);
	
	CILK_C_REDUCER_MAX_INDEX(best_score, int, INT_MIN);
	CILK_C_REGISTER_REDUCER(best_score);
	
	move* moves = (move*)malloc(board_size*sizeof(move));
	
	cilk_for (int i = 0; i < board_size; i++) {
		int best = INT_MIN;
		for (int j = 0; j < board_size; j++) {
			move m;
            init_move(&m,j,i,turn);
            get_move(&m, gameboard);
			if(m.heuristic > 0)
			{
				//printf("heuristic: %d\n", m.heuristic);
				//printf("minMax: %d\n", minMax(&s, m, depth-1));
				int score = minMax(&s, m, depth-1);
				//printf("score: %d\n", score);
				if(score > best)
				{
					best = score;
					moves[i] = m;
				}
			}
		}
		CILK_C_REDUCER_MAX_INDEX_CALC(best_score, i, best);
	}
	
	free_gameboard(s.gameboard);
	if(REDUCER_VIEW(best_score).value > INT_MIN)
	{
		best_move = moves[REDUCER_VIEW(best_score).index];
	}
	CILK_C_UNREGISTER_REDUCER(best_score);
	CILK_C_UNREGISTER_REDUCER(s.alpha);
	CILK_C_UNREGISTER_REDUCER(s.beta);
	free(moves);
	//printf("final: %d\n", best_move.heuristic);
	if (best_move.heuristic > INT_MIN) {
		flip_board(&best_move, gameboard);
		return TRUE;	//made a move
	} else 
		return FALSE;	//no move to make
}

void help(const char* prog_name)
{
    printf ("Usage: %s [-s] [-c] [-t] [-a] [-d <MILI_SECA>] [-b <BOARD_ZISE>] [-n <N_THREADS>]\n", prog_name);
    exit (1);
}

void get_flags(int argc, char * argv[])
{
    char ch;
    while ((ch = getopt(argc, argv, "scatd:b:n:")) != -1)
	{
        switch (ch)
		{
            case 's':
                print_mode = 's';
                break;
            case 'a':
                anim_mode = 1;
                break;
            case 'c':
                if (print_mode != 's')
                    print_mode = 'c';
                break;
            case 'd':
                delay = atoi(optarg);
                if (delay < 0)
				{
                    printf("Minimum delay is 0.\n");
                    help(argv[0]);
                }
                break;
            case 'b':
                board_size = atoi(optarg);
                if (board_size < 4)
				{
                    printf("Minimum board size is 4.\n");
                    help(argv[0]);
                }
                break;
            case 'n':
                threads = atoi(optarg);
                if (threads < 1)
				{
                    printf("Minimum threads is 1.\n");
                    help(argv[0]);
                    break;
                }
                __cilkrts_set_param("nworkers", optarg);
                break;
            case 't':
                // IMPLEMENT THIS OPTION
                time_elapsed = 'y';
				clock_gettime(CLOCK_MONOTONIC, &start);
                break;
            case '?':
            default:
                help(argv[0]);
        }
    }
}

int main (int argc, char * argv[])
{
    get_flags(argc,argv);
    // argc -= optind;
    // argv += optind;
    
	gameboard = build_board();
	init_board(gameboard);
	int cant_move_r = FALSE, cant_move_b = FALSE;
	char turn = R;

    if (anim_mode)
		printf(CLEAR);

	while (!cant_move_r || !cant_move_b)
	{
        print_board(gameboard);
		int cant_move = !make_move(turn, nMinMaxLevels);
		if (cant_move)
		{
			if (turn == R)
				cant_move_r = TRUE;
			else
				cant_move_b = TRUE;
		}
		else
		{
			if (turn == R)
				cant_move_r = FALSE;
			else
				cant_move_b = FALSE;
		}
		turn = opponent(turn);
        usleep(delay*1000);
	}
    finish_game();
}
