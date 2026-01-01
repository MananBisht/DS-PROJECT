#include "helpers_ui.h"
#include <stdio.h>
#include<string.h>

extern Font gameFont;

// Soft pastel palette (option 2)
#define SOFT_RED      (Color){255, 112, 112, 255}
#define SOFT_GREEN    (Color){166, 220, 160, 255}
#define SOFT_BLUE     (Color){146, 184, 245, 255}
#define SOFT_YELLOW   (Color){255, 232, 138, 255}
#define SOFT_GRAY     (Color){245, 245, 245, 255}
#define OUTLINE_GRAY  (Color){200, 200, 200, 255}

// Small helpers (assumes CARD_WIDTH, CARD_HEIGHT are defined elsewhere)
static inline Color _get_card_color(card_color c) {
    switch (c) {
    case COLOR_RED:    return SOFT_RED;
    case COLOR_GREEN:  return SOFT_GREEN;
    case COLOR_BLUE:   return SOFT_BLUE;
    case COLOR_YELLOW: return SOFT_YELLOW;
    default:           return SOFT_GRAY; // before a wild color chosen
    }
}

// Draw a soft ambient shadow (not a true blur but looks soft)
// rect: target rectangle for the card
static void DrawCardShadow(Rectangle rect) {
    // Slight offset and low alpha to simulate soft shadow
    Rectangle srect = { rect.x + 6.0f, rect.y + 6.0f, rect.width, rect.height };
    DrawRectangleRounded(srect, 0.20f, 16, (Color){0, 0, 0, 45});
}

// Main card drawing function (keeps previous signature)
void DrawCard(Card card, Vector2 pos) {
    Rectangle rect = { pos.x, pos.y, CARD_WIDTH, CARD_HEIGHT };

    // Draw shadow first (soft ambient)
    DrawCardShadow(rect);

    // --- Wild +4 AFTER choosing a color (solid colored card with "Wild +4" text) ---
    if (card.type == Wild_draw_4 && card.color != COLOR_NONE) {
        Color bg = _get_card_color(card.color);
        DrawRectangleRounded(rect, 0.20f, 16, bg);
        DrawRectangleRoundedLines(rect, 0.20f, 16, (Color){0,0,0,38});

        const char *text = "Wild +4";
        float fontSize = CARD_HEIGHT * 0.32f; // scale to card height
        Vector2 tsize = MeasureTextEx(gameFont, text, fontSize, 2);
        Vector2 textPos = { pos.x + (CARD_WIDTH - tsize.x) * 0.5f, pos.y + (CARD_HEIGHT - tsize.y) * 0.5f };
        DrawTextEx(gameFont, text, textPos, fontSize, 2, BLACK);
        return;
    }

    // --- Normal Wild AFTER choosing a color (solid colored card with "Wild") ---
    if (card.type == Wild && card.color != COLOR_NONE) {
        Color bg = _get_card_color(card.color);
        DrawRectangleRounded(rect, 0.20f, 16, bg);
        DrawRectangleRoundedLines(rect, 0.20f, 16, (Color){0,0,0,38});

        const char *text = "Wild";
        float fontSize = CARD_HEIGHT * 0.32f;
        Vector2 tsize = MeasureTextEx(gameFont, text, fontSize, 2);
        Vector2 textPos = { pos.x + (CARD_WIDTH - tsize.x) * 0.5f, pos.y + (CARD_HEIGHT - tsize.y) * 0.5f };
        DrawTextEx(gameFont, text, textPos, fontSize, 2, BLACK);
        return;
    }

    // --- Wild or Wild+4 BEFORE selecting color (classic black with 4 color squares) ---
    if (card.type == Wild || card.type == Wild_draw_4) {
        DrawRectangleRounded(rect, 0.20f, 16, BLACK);
        DrawRectangleRoundedLines(rect, 0.20f, 16, (Color){255,255,255,140});

        // Four small corner squares (classic UNO look)
        DrawRectangle(pos.x + 10, pos.y + 10, 20, 20, SOFT_RED);
        DrawRectangle(pos.x + CARD_WIDTH - 30, pos.y + 10, 20, 20, SOFT_GREEN);
        DrawRectangle(pos.x + 10, pos.y + CARD_HEIGHT - 30, 20, 20, SOFT_BLUE);
        DrawRectangle(pos.x + CARD_WIDTH - 30, pos.y + CARD_HEIGHT - 30, 20, 20, SOFT_YELLOW);

        // Center text
        const char *text = (card.type == Wild_draw_4) ? "Wild +4" : "Wild";
        float fontSize = CARD_HEIGHT * 0.30f;
        Vector2 tsize = MeasureTextEx(gameFont, text, fontSize, 2);
        Vector2 textPos = { pos.x + (CARD_WIDTH - tsize.x) * 0.5f, pos.y + (CARD_HEIGHT - tsize.y) * 0.5f };
        DrawTextEx(gameFont, text, textPos, fontSize, 2, WHITE);
        return;
    }

    // --- Normal colored cards (Number, Skip, Reverse, Draw_2) ---
    Color bg = _get_card_color(card.color);
    DrawRectangleRounded(rect, 0.20f, 16, bg);
    DrawRectangleRoundedLines(rect, 0.20f, 16, (Color){0,0,0,38});

    // Determine card label
    char label[16] = {0};
    if (card.type == Number) {
        sprintf(label, "%d", card.number);
    } else if (card.type == Skip) {
        strcpy(label, "Skip");
    } else if (card.type == Reverse) {
        strcpy(label, "Rev");
    } else if (card.type == Draw_2) {
        strcpy(label, "+2");
    } else {
        strcpy(label, "?");
    }

    float fontSize = CARD_HEIGHT * 0.30f;
    Vector2 tsize = MeasureTextEx(gameFont, label, fontSize, 2);
    Vector2 textPos = { pos.x + (CARD_WIDTH - tsize.x) * 0.5f, pos.y + (CARD_HEIGHT - tsize.y) * 0.5f };
    DrawTextEx(gameFont, label, textPos, fontSize, 2, BLACK);
}

// Draw the back of a UNO card (facedown)
void DrawCardBack(Vector2 pos) {
    Rectangle rect = { pos.x, pos.y, CARD_WIDTH, CARD_HEIGHT };
    DrawCardShadow(rect);

    DrawRectangleRounded(rect, 0.20f, 16, (Color){40, 40, 40, 220});
    DrawRectangleRoundedLines(rect, 0.20f, 16, (Color){255,255,255,60});

    // UNO logo / oval
    DrawEllipse(pos.x + CARD_WIDTH/2, pos.y + CARD_HEIGHT/2, CARD_WIDTH*0.36f, CARD_HEIGHT*0.44f, SOFT_RED);
    float fontSize = CARD_HEIGHT * 0.28f;
    Vector2 tsize = MeasureTextEx(gameFont, "UNO", fontSize, 2);
    DrawTextEx(gameFont, "UNO", (Vector2){ pos.x + (CARD_WIDTH - tsize.x) * 0.5f, pos.y + (CARD_HEIGHT - tsize.y) * 0.5f }, fontSize, 2, WHITE);
}

// Rounded button helper (keeps previous behavior)
bool DrawButton(Rectangle bounds, const char* text) {
    bool clicked = false;
    Vector2 mouse = GetMousePosition();

    bool hovered = CheckCollisionPointRec(mouse, bounds);
    Color normal = (Color){70, 135, 255, 255};
    Color hover  = (Color){90, 155, 255, 255};
    Color press  = (Color){55, 110, 220, 255};
    Color current = normal;

    if (hovered) {
        if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) current = press;
        else current = hover;
        if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) clicked = true;
    }

    DrawRectangleRounded(bounds, 0.28f, 12, current);
    DrawRectangleRoundedLines(bounds, 0.28f, 12, (Color){0,0,0,30});

    // Button text
    float fontSize = bounds.height * 0.45f;
    Vector2 tsize = MeasureTextEx(gameFont, text, fontSize, 2);
    DrawTextEx(gameFont, text, (Vector2){ bounds.x + (bounds.width - tsize.x) * 0.5f, bounds.y + (bounds.height - tsize.y) * 0.5f }, fontSize, 2, WHITE);

    return clicked;
}

