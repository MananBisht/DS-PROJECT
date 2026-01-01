#ifndef PLAYER_HAND_H   
#define PLAYER_HAND_H   

#include <stdlib.h>
#include <stdio.h>
#include "cards.h"
#include "deck2.h"


typedef struct hand_node{
    Card card ;
    struct hand_node *next ;
    struct hand_node *prev ;
}hand_node; 

typedef struct hand{
    hand_node *head ;
    hand_node *tail ;
    int size ;
}hand ;

void init_hand(hand* hand);

void add_card(hand* hand, Card card);

void add_discard_card(hand* hand, Card card);

int compare_cards(Card a, Card b);

int can_play(Card card, Card discard_pile_top);

int get_valid_cards(hand* hand, Card discard_pile_top, Card playable[]);

void remove_card(hand* hand, Card card);

const char* color_to_string(card_color color);

const char* type_to_string(card_type type);

void display_hand(hand* hand);

Card get_card_by_index(hand* hand, int idx);

#endif