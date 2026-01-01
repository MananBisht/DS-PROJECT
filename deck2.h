#ifndef DECK2_H
#define DECK2_H

#include <stdio.h>
#include "cards.h"
#include <stdlib.h>
#include <time.h>

typedef struct {
    Card cards[108];
    int top;
    int size;
} Deck;


const char* get_color_name(card_color c);

void init_deck(Deck* deck);

void init_reloaded_deck(Deck* deck);

void shuffle_deck(Deck* deck);

Card draw_card(Deck* deck);

#endif
