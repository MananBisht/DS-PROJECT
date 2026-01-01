#ifndef CLASSIC_UNO_H
#define CLASSIC_UNO_H

#include "include/raylib.h"
#include "gui.h"
#include "cards.h"
#include "game.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
// gcc main1.c gui.c uno_classic_raylib.c -o main1.exe -O1 -Wall -std=c17 -Wno-missing-braces -I include/ -L lib/ -lraylib -lopengl32 -lgdi32 -lwinmm
bool is_valid_play(Card selected, Card top) {
    return (selected.color == top.color) ||
           (selected.type == top.type) ||
           (selected.type == Wild) ||
           (selected.type == Wild_draw_4);
}

void DrawCardVisual(Card c, float x, float y, bool highlight) {
    Color col = GetColorForCard(c.color);
    Rectangle rect = {x, y, 70, 100};
    DrawRectangleRec(rect, highlight ? Fade(col, 0.7f) : col);
    DrawRectangleLinesEx(rect, 3, BLACK);
    const char *typeStr = type_to_string(c.type);
    if (c.type == Number)
        DrawText(TextFormat("%d", c.number), x + 25, y + 40, 24, WHITE);
    else
        DrawText(typeStr, x + 10, y + 40, 14, WHITE);
}

void DrawCardBack(float x, float y) {
    Rectangle rect = {x, y, 70, 100};
    DrawRectangleRec(rect, DARKGRAY);
    DrawRectangleLinesEx(rect, 3, BLACK);
    DrawText("UNO", x + 15, y + 40, 18, WHITE);
}

void start_classic_uno(void) {
    const int screenW = 1000, screenH = 700;
    InitWindow(screenW, screenH, "Classic UNO");
    SetTargetFPS(60);
    srand(time(NULL));

    Game game;
    char player_names[4][32] = {"Player 1", "Player 2", "Player 3", "Player 4"};
    init_game(&game, 4, player_names);

    bool cardPlayed = false;
    bool drawClicked = false;
    int messageTimer = 0;

    // Wild color selector
    int choosingColor = 0;
    card_color chosenColor = COLOR_NONE;

    while (!WindowShouldClose()) {
        Player *current = get_current_player(&game);
        Card *top = &game.discard_pile.cards[game.discard_pile.top];
        Vector2 mouse = GetMousePosition();

        // --- Handle Wild color selection ---
        if (choosingColor) {
            BeginDrawing();
            ClearBackground((Color){39, 174, 96, 255});
            DrawText("Choose a color:", 400, 250, 28, WHITE);
            DrawRectangle(350, 320, 70, 70, RED);
            DrawRectangle(450, 320, 70, 70, GREEN);
            DrawRectangle(550, 320, 70, 70, BLUE);
            DrawRectangle(650, 320, 70, 70, YELLOW);
            EndDrawing();

            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                Vector2 m = GetMousePosition();
                if (CheckCollisionPointRec(m, (Rectangle){350, 320, 70, 70})) chosenColor = COLOR_RED;
                else if (CheckCollisionPointRec(m, (Rectangle){450, 320, 70, 70})) chosenColor = COLOR_GREEN;
                else if (CheckCollisionPointRec(m, (Rectangle){550, 320, 70, 70})) chosenColor = COLOR_BLUE;
                else if (CheckCollisionPointRec(m, (Rectangle){650, 320, 70, 70})) chosenColor = COLOR_YELLOW;

                top->color = chosenColor;
                choosingColor = 0;
                next_turn(&game);
            }
            continue;
        }

        // --- Input (Player 1 only) ---
        if (game.current_turn == 0) {
            hand_node *node = current->hand.head;
            int i = 0;
            bool played = false;

            while (node) {
                float x = 200 + i * 90;
                float y = 550;
                Rectangle rect = {x, y, 70, 100};

                if (CheckCollisionPointRec(mouse, rect) &&
                    IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                    Card selected = node->card;
                    if (is_valid_play(selected, *top)) {
                        remove_card(&current->hand, selected);
                        game.discard_pile.cards[++game.discard_pile.top] = selected;
                        game.discard_pile.size++;

                        // --- Power card effects ---
                        switch (selected.type) {
                            case Skip:
                                next_turn(&game); // skip next player
                                break;
                            case Reverse:
                                game.direction *= -1;
                                if (game.num_players == 2)
                                    next_turn(&game);
                                break;
                            case Draw_2: {
                                next_turn(&game);
                                Player *nextP = get_current_player(&game);
                                add_card(&nextP->hand, &game.draw_pile);
                                add_card(&nextP->hand, &game.draw_pile);
                                break;
                            }
                            case Wild:
                                choosingColor = 1;
                                break;
                            case Wild_draw_4: {
                                choosingColor = 1;
                                next_turn(&game);
                                Player *nextP = get_current_player(&game);
                                for (int k = 0; k < 4; k++)
                                    add_card(&nextP->hand, &game.draw_pile);
                                break;
                            }
                            default:
                                next_turn(&game);
                                break;
                        }

                        cardPlayed = true;
                        messageTimer = 60;
                        played = true;
                        break;
                    }
                }
                node = node->next;
                i++;
            }

            Rectangle drawPile = {820, 300, 100, 140};
            if (!played && CheckCollisionPointRec(mouse, drawPile) &&
                IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                add_card(&current->hand, &game.draw_pile);
                drawClicked = true;
                messageTimer = 60;
                next_turn(&game);
            }
        }

        // --- Check Win ---
        if (current->hand.size == 0) {
            BeginDrawing();
            ClearBackground((Color){20, 160, 90, 255});
            DrawText(TextFormat("%s WINS!", current->name), 400, 300, 40, YELLOW);
            DrawText("Press ESC to quit.", 380, 360, 20, WHITE);
            EndDrawing();
            if (IsKeyPressed(KEY_ESCAPE)) break;
            continue;
        }

        // --- Render ---
        BeginDrawing();
        ClearBackground((Color){39, 174, 96, 255});

        DrawText("CLASSIC UNO", 400, 20, 28, WHITE);
        DrawText(TextFormat("Current Player: %s", current->name), 40, 30, 20, WHITE);

        DrawRectangle(450, 300, 100, 140, GetColorForCard(top->color));
        DrawRectangleLinesEx((Rectangle){450, 300, 100, 140}, 3, BLACK);
        if (top->type == Number)
            DrawText(TextFormat("%d", top->number), 490, 360, 24, WHITE);
        else
            DrawText(type_to_string(top->type), 470, 360, 20, WHITE);

        DrawRectangle(820, 300, 100, 140, DARKBLUE);
        DrawRectangleLinesEx((Rectangle){820, 300, 100, 140}, 3, BLACK);
        DrawText("DRAW", 845, 360, 20, WHITE);

        if (game.current_turn == 0) {
            hand_node *node = current->hand.head;
            int i = 0;
            while (node) {
                float x = 200 + i * 90;
                float y = 550;
                bool hovered = CheckCollisionPointRec(mouse, (Rectangle){x, y, 70, 100});
                DrawCardVisual(node->card, x, y, hovered);
                node = node->next;
                i++;
            }
        }

        for (int p = 1; p < game.num_players; p++) {
            int x = 50 + (p * 250);
            int y = 100;
            for (int c = 0; c < game.players[p].hand.size; c++) {
                DrawCardBack(x + c * 15, y);
            }
            DrawText(game.players[p].name, x + 15, y - 25, 20, WHITE);
        }

        if (cardPlayed && messageTimer > 0) {
            DrawText("Card Played!", 440, 120, 24, WHITE);
            messageTimer--;
            if (messageTimer <= 0) cardPlayed = false;
        }
        if (drawClicked && messageTimer > 0) {
            DrawText("Card Drawn!", 440, 150, 24, WHITE);
            messageTimer--;
            if (messageTimer <= 0) drawClicked = false;
        }

        EndDrawing();
    }

    CloseWindow();
}

#endif
