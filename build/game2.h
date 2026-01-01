#ifndef GAME_H
#define GAME_H

#include "deck2.h"
#include "player.h"

#define MAX_PLAYERS 4

typedef struct {
    Deck draw_pile;
    Deck discard_pile;
    Player players[MAX_PLAYERS];
    int num_players;
    int current_turn;
    int direction;
} Game;


void init_game(Game* game, int num_players, char player_names[][32]);

void next_turn(Game* game);

Player* get_current_player(Game* game);

int apply_card_effect(Game* game, Card card);

void player_draw(Game* game, int player_id);

void reshuffle_deck(Game* game);

int has_valid_card(hand* h, Card top);

int check_winner(Game* game, int player_id);

void handle_wild_draw4_challenge(Game* game, int challenger_id, int prev_id);

int play_card(Game* game, int player_id, Card card, card_color chosen_color);
// --- Initialization ---

#endif
