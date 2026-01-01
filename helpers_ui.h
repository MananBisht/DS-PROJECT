#ifndef HELPERS_UI_H
#define HELPERS_UI_H

#include "include/raylib.h"
#include "cards.h"

// Define card sizes
#define CARD_WIDTH   100
#define CARD_HEIGHT  150
#define CARD_PADDING 10

// --- Function Declarations ---

// Gets the raylib color for a card
Color GetCardColor(card_color color);

// Draws a single card at a position
void DrawCard(Card card, Vector2 position);

// Draws the back of a card
void DrawCardBack(Vector2 position);

// Draws a text button and returns true if clicked
bool DrawButton(Rectangle bounds, const char* text);

#endif