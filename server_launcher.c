// Glass-style UNO Server Launcher (asks only for number of players)
// ---------------------------------------------------------------
// Uses Raylib for a minimal, modern translucent GUI launcher.
#define WIN32_LEAN_AND_MEAN
#define NOGDI
#define NOUSER
#define NOMINMAX
#define NOIME
#define NOSOUND
#define NOMCX
#define NOHELP
#include "include/raylib.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
// Glass-style UNO Server Launcher (asks only for number of players)
// ---------------------------------------------------------------

void UpdateText(char *buffer, int maxLen);

int main(void) {
    SetConfigFlags(FLAG_WINDOW_UNDECORATED | FLAG_WINDOW_TRANSPARENT);
    InitWindow(480, 260, "UNO Server Launcher");
    SetTargetFPS(60);

    // Player count only
    char players[4] = "2";
    bool playersEdit = false;

    // UI rectangles
    Rectangle panel = {40, 40, 400, 180};
    Rectangle playerBox = {200, 100, 140, 36};
    Rectangle startBtn = {160, 160, 180, 45};

    while (!WindowShouldClose()) {
        Vector2 mouse = GetMousePosition();

        // Clicking input box
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            playersEdit = CheckCollisionPointRec(mouse, playerBox);
        }

        // Text input
        if (playersEdit) UpdateText(players, sizeof(players));

        // START button logic
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mouse, startBtn)) {

            char launchServer[256];
            sprintf(launchServer, "start server.exe 0.0.0.0 8888 %s", players);
            system(launchServer);
            Sleep(250);

            system("start client.exe");
            CloseWindow();
            return 0;
        }

        // DRAWING ----------------------------------------------------
        BeginDrawing();
        ClearBackground(BLANK);

        // Glass-blur panel look
        DrawRectangleRounded(panel, 0.20f, 16, (Color){255, 255, 255, 180});
        DrawRectangleRoundedLines(panel, 0.20f, 16, (Color){255, 255, 255, 200});

        DrawText("UNO Server Launcher", 110, 52, 26, (Color){255,255,255,230});

        DrawText("Players:", 85, 108, 22, (Color){240,240,240,240});

        // Player textbox
        DrawRectangleRounded(playerBox, 0.14f, 10, (Color){255,255,255,90});
        DrawText(players, playerBox.x + 10, playerBox.y + 7, 24, BLACK);

        // Button
        Color btnColor = CheckCollisionPointRec(mouse, startBtn) ?
                         (Color){255, 200, 80, 255} :
                         (Color){255, 180, 60, 255};
        DrawRectangleRounded(startBtn, 0.25f, 12, btnColor);
        DrawText("START", startBtn.x + 60, startBtn.y + 12, 24, WHITE);

        EndDrawing();
    }

    CloseWindow();
    return 0;
}

// Simple textbox helper
void UpdateText(char *buffer, int maxLen) {
    int key = GetCharPressed();
    while (key > 0) {
        int len = strlen(buffer);
        if (key >= '0' && key <= '9' && len < maxLen - 1) {
            buffer[len] = key;
            buffer[len + 1] = '\0';
        }
        key = GetCharPressed();
    }

    if (IsKeyPressed(KEY_BACKSPACE)) {
        int len = strlen(buffer);
        if (len > 0) buffer[len - 1] = '\0';
    }
}
