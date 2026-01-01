#ifndef PLAYER_H
#define PLAYER_H

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "player_hand.h"
#include "deck2.h"

typedef struct Player{
    int id ; 
    char name[32] ;
    hand hand ;
    int skipped ;
}Player;

void init_player(Player* p, char* p_name, int player_id);

void init_card_draw(Player* p, Deck* deck);

void init_card_draw_reload(Player* p, Deck* deck, int num_players);

#endif