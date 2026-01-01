#ifndef SERIALIZER_H
#define SERIALIZER_H

#include "cards.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "player_hand.h" // For hand_node definition

// --- CARD SERIALIZATION (Card struct -> string) ---
// Format : R1, G+2, B_SKIP, Y_REV, W, W+4
char* serialize_card(Card card) {
    static char buffer[8]; 
    char colorChar = 'N'; 
    switch (card.color) {
        case COLOR_RED:    colorChar = 'R'; break;
        case COLOR_GREEN:  colorChar = 'G'; break;
        case COLOR_BLUE:   colorChar = 'B'; break;
        case COLOR_YELLOW: colorChar = 'Y'; break;
        default: break;
    }

    switch (card.type) {
        case Number:
            sprintf(buffer, "%c%d", colorChar, card.number);
            break;

        case Skip:
            sprintf(buffer, "%cS", colorChar);
            break;

        case Draw_2:
            sprintf(buffer, "%c+2", colorChar);
            break;

        case Reverse:
            sprintf(buffer, "%cR", colorChar);
            break;

        case Wild:
            if (card.color != COLOR_NONE && card.color != COLOR_NONE)
                sprintf(buffer, "W%c", colorChar);
            else
                sprintf(buffer, "W");
            break;

        case Wild_draw_4:
            if (card.color != COLOR_NONE && card.color != COLOR_NONE)
                sprintf(buffer, "W+4%c", colorChar);
            else
                sprintf(buffer, "W+4");
            break;

        default:
            sprintf(buffer, "ERR");
            break;
    }

    return buffer;
}


// --- CARD DESERIALIZATION (string -> Card struct) ---
Card deserialize_card(const char* token) {
    Card c = { COLOR_NONE, Number, 0 };

    int len = strlen(token);
    if (len == 0) return c;

    // --- WILD FAMILY -------------------------------------------------
    if (token[0] == 'W') {

        // Check Wild +4
        if (len >= 2 && token[1] == '+') {
            c.type = Wild_draw_4;

            // Format could be:  W+4   or  W+4R
            if (len == 4) {
                switch (token[3]) {
                    case 'R': c.color = COLOR_RED; break;
                    case 'G': c.color = COLOR_GREEN; break;
                    case 'B': c.color = COLOR_BLUE; break;
                    case 'Y': c.color = COLOR_YELLOW; break;
                }
            } else {
                c.color = COLOR_NONE;
            }
            return c;
        }

        // Check plain Wild (W or WR)
        c.type = Wild;

        if (len == 2) {
            switch (token[1]) {
                case 'R': c.color = COLOR_RED; break;
                case 'G': c.color = COLOR_GREEN; break;
                case 'B': c.color = COLOR_BLUE; break;
                case 'Y': c.color = COLOR_YELLOW; break;
                default: c.color = COLOR_NONE; break;
            }
        } else {
            c.color = COLOR_NONE;
        }

        return c;
    }

    // --- NORMAL CARDS -----------------------------------------------
    // First char = color
    switch (token[0]) {
        case 'R': c.color = COLOR_RED; break;
        case 'G': c.color = COLOR_GREEN; break;
        case 'B': c.color = COLOR_BLUE; break;
        case 'Y': c.color = COLOR_YELLOW; break;
        default: return c;
    }

    // 2nd char = type
    if (len == 2) {
        if (token[1] >= '0' && token[1] <= '9') {
            c.type = Number;
            c.number = token[1] - '0';
        }
        else if (token[1] == 'S') c.type = Skip;
        else if (token[1] == 'R') c.type = Reverse;
        else if (token[1] == '+') c.type = Draw_2;
    }
    else if (len == 3 && token[1] == '+' && token[2] == '2') {
        c.type = Draw_2;
    }

    return c;
}

// --- HAND SERIALIZATION (Server side) ---

char* serialize_hand(hand* h) {
    if (h->size == 0) {
        return strdup("");
    }

    int buffer_size = (8 + 1) * h->size + 1; 
    char* buffer = (char*)malloc(buffer_size);
    if (!buffer) return NULL;
    buffer[0] = '\0'; 

    hand_node* itr = h->head;
    while (itr) {

        strcat(buffer, serialize_card(itr->card));
        itr = itr->next;
        if (itr) {
            strcat(buffer, ",");
        }
    }
    return buffer;
}


#endif