// client.c  (patched for Baloo2, rounded buttons under discard pile, player table, YOUR TURN)
#pragma comment(lib, "ws2_32.lib") // Link against ws2_32.lib
#pragma comment(lib, "raylib.lib") // Link against raylib.lib

// --- Critical Fix for Windows/Raylib Conflicts ---
#define WIN32_LEAN_AND_MEAN
#define NOGDI
#define NOUSER
#define NOMINMAX
#define NOIME
#define NOSOUND
#define NOMCX
#define NOHELP

#include <WinSock2.h>
#include <WS2tcpip.h>
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <math.h>

#include "include/raylib.h"
#include "helpers_ui.h"
#include "serializer.h"      // serialize_card, deserialize_card, serialize_hand (if needed)
#include "shared_protocol.h" // S2C_*, C2S_* constants
#include "game2.h"
#include "player_hand.h"     // Card, card_color, MAX_PLAYERS (provided in your project)

// --- UI / Layout constants ---
#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 720

// Default server values
#define DEFAULT_SERVER_IP "127.0.0.1"
// #define DEFAULT_SERVER_IP "192.168.1.12"
#define DEFAULT_SERVER_PORT 8888
Font gameFont;  // global
// --- Client-side game state ---

typedef enum {
    SCREEN_CONNECT,
    SCREEN_GAME
} AppScreen;

static AppScreen currentScreen = SCREEN_CONNECT;

char serverIp[32]   = DEFAULT_SERVER_IP;
char serverPort[8]  = "8888";

bool ipEdit = true;
bool portEdit = false;
typedef struct {
    Card pending_wild_card;
    Card my_hand[64];
    int my_hand_size;
    Card top_card;
    bool top_card_valid;
    int player_hand_sizes[MAX_PLAYERS];
    int current_turn;
    int my_player_id;
    int total_players;
    int direction;
    bool is_my_turn;
    bool game_over;
    int winner_id;
    bool drawn;
    bool need_uno_client;     // true when server says you must call UNO
    unsigned int uno_time_ms; // countdown in milliseconds
    double uno_start_time;
    bool choosing_color;
    char status_message[256];
} ClientGameState;

// --- Globals ---
CRITICAL_SECTION g_game_lock;
ClientGameState g_state;
SOCKET g_server_socket = INVALID_SOCKET;
bool g_connected = false;

// Background / flow globals
float timeFlow = 0.0f;
int flowDir = 1;

// Font
Font gameFont;

// --- Function prototypes ---
void UpdateText(char *buffer, int maxLen);
void request_hand_refresh();
DWORD WINAPI receive_thread(LPVOID lpParam);
void process_server_message(const char* msg);
void send_message_to_server(const char* msg);
void initialize_game_state();
void draw_game_screen();
void handle_input();
void parse_and_set_hand(const char* list); // helper

// Text input helper for IP / Port fields
void UpdateText(char *buffer, int maxLen)
{
    int key = GetCharPressed();

    while (key > 0)
    {
        int len = strlen(buffer);

        // printable characters only
        if ((key >= 32) && (key <= 125) && (len < maxLen - 1))
        {
            buffer[len] = (char)key;
            buffer[len + 1] = '\0';
        }

        key = GetCharPressed();
    }

    if (IsKeyPressed(KEY_BACKSPACE))
    {
        int len = strlen(buffer);
        if (len > 0)
            buffer[len - 1] = '\0';
    }
}

// --- Utility UI helpers (local) ---
void UpdateFlow()
{
    timeFlow += 0.01f * flowDir;   // slow smooth motion
}
void DrawFlowingBackground(Color A, Color B)
{
    float freq = 0.005f;   // how “wide” the waves are
    float h = SCREEN_HEIGHT;

    // Draw vertical lines across the screen — Raylib is fast enough for this resolution
    for (int x = 0; x < SCREEN_WIDTH; x++)
    {
        float t = 0.5f + 0.5f * sinf(x * freq + timeFlow);

        Color c = (Color){
            (unsigned char)(A.r + (B.r - A.r) * t),
            (unsigned char)(A.g + (B.g - A.g) * t),
            (unsigned char)(A.b + (B.b - A.b) * t),
            255
        };

        DrawLine(x, 0, x, h, c);
    }
}

void GetNeutralFlowColors(Color* A, Color* B)
{
    *A = (Color){ 225, 230, 245, 255 };   // soft neutral-teal white
    *B = (Color){ 195, 205, 225, 255 };   // slightly darker cool neutral
}

// Player list table
void DrawPlayerTable()
{
    float boxX = SCREEN_WIDTH - 220;
    float boxY = 60;
    float boxW = 200;
    float boxH = 160;

    DrawRectangleRounded((Rectangle){boxX, boxY, boxW, boxH}, 0.18f, 6, (Color){255,255,255,40});
    DrawRectangleRoundedLines((Rectangle){boxX, boxY, boxW, boxH}, 0.18f, 6, (Color){0,0,0,50});

    DrawTextEx(gameFont, "Players", (Vector2){boxX + 12, boxY + 8}, 20, 1, (Color){40,40,40,220});

    float y = boxY + 36;
    for (int i = 0; i < g_state.total_players && i < MAX_PLAYERS; i++) {
        char entry[64];
        snprintf(entry, sizeof(entry), "P%d", i);

        // player name or (You)
        char info[64];
        if (i == g_state.my_player_id)
            snprintf(info, sizeof(info), "%s  -  %d cards (you)", entry, g_state.player_hand_sizes[i]);
        else
            snprintf(info, sizeof(info), "%s  -  %d cards", entry, g_state.player_hand_sizes[i]);

        Color col = (i == g_state.current_turn) ? (Color){20,120,255,220} : (Color){60,60,60,200};
        DrawTextEx(gameFont, info, (Vector2){boxX + 12, y}, 18, 1, col);
        y += 28;
    }
}
void DrawConnectScreen()
{
    Vector2 mouse = GetMousePosition();

    Rectangle ipBox   = { 480, 250, 320, 42 };
    Rectangle portBox = { 480, 310, 320, 42 };
    Rectangle btnBox  = { 480, 380, 320, 50 };

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        ipEdit   = CheckCollisionPointRec(mouse, ipBox);
        portEdit = CheckCollisionPointRec(mouse, portBox);
    }

    if (ipEdit)   UpdateText(serverIp, sizeof(serverIp));
    if (portEdit) UpdateText(serverPort, sizeof(serverPort));

    DrawTextEx(gameFont, "CONNECT TO SERVER", (Vector2){500, 160}, 36, 2, BLUE);

    DrawText("Server IP", 360, 260, 20, GRAY);
    DrawRectangleRounded(ipBox, 0.3f, 8, RAYWHITE);
    DrawText(serverIp, ipBox.x + 8, ipBox.y + 10, 20, BLACK);

    DrawText("Port", 360, 320, 20, GRAY);
    DrawRectangleRounded(portBox, 0.3f, 8, RAYWHITE);
    DrawText(serverPort, portBox.x + 8, portBox.y + 10, 20, BLACK);

    Color btn = CheckCollisionPointRec(mouse, btnBox) ? DARKBLUE : BLUE;
    DrawRectangleRounded(btnBox, 0.4f, 10, btn);
    DrawText("JOIN GAME", btnBox.x + 95, btnBox.y + 12, 24, WHITE);

    // CONNECT
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) &&
        CheckCollisionPointRec(mouse, btnBox))
    {
        struct sockaddr_in server_addr;
        g_server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(atoi(serverPort));

        if (InetPton(AF_INET, serverIp, &server_addr.sin_addr) != 1) {
            sprintf(g_state.status_message, "Invalid IP address");
            return;
        }

        if (connect(g_server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
            sprintf(g_state.status_message, "Connection Failed!");
            closesocket(g_server_socket);
            return;
        }

        g_connected = true;
        CreateThread(NULL, 0, receive_thread, NULL, 0, NULL);
        currentScreen = SCREEN_GAME;

        sprintf(g_state.status_message, "Connected to %s", serverIp);
    }
}

// --- Main ---
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0) {
        printf("WSAStartup failed.\n");
        return 1;
    }

    // Init Raylib
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "UNO Client");
    SetTargetFPS(60);

    // Load game font (Baloo2)
    gameFont = LoadFont("assets/font/Baloo2-VariableFont_wght.ttf");
    SetTextureFilter(gameFont.texture, TEXTURE_FILTER_BILINEAR);

    initialize_game_state();
    InitializeCriticalSection(&g_game_lock);

    // Start receive thread
    CreateThread(NULL, 0, receive_thread, NULL, 0, NULL);

    // Game/render loop
    while (!WindowShouldClose()) {

        BeginDrawing();

        EnterCriticalSection(&g_game_lock);

        if (currentScreen == SCREEN_CONNECT) {
            DrawConnectScreen();
        } else {
            handle_input();
            draw_game_screen();
        }

        LeaveCriticalSection(&g_game_lock);
        EndDrawing();
    }


    // Cleanup
    CloseWindow();
    if (g_connected) closesocket(g_server_socket);
    DeleteCriticalSection(&g_game_lock);
    UnloadFont(gameFont);
    WSACleanup();
    return 0;
}

// --- Initialization ---
void initialize_game_state() {
    g_state.my_hand_size = 0;
    g_state.current_turn = -1;
    g_state.my_player_id = -1;
    g_state.total_players = 0;
    g_state.direction = 1;
    g_state.is_my_turn = false;
    g_state.game_over = false;
    g_state.top_card_valid = false;
    g_state.choosing_color = false;
    g_state.need_uno_client = false;
    g_state.uno_time_ms = 0;
    g_state.uno_start_time = 0.0;

    g_state.drawn = false;
    memset(&g_state.pending_wild_card, 0, sizeof(Card));
    sprintf(g_state.status_message, "Waiting to join...");
    for (int i = 0; i < MAX_PLAYERS; i++) g_state.player_hand_sizes[i] = 0;
}

// --- Networking: send helper ---
void send_message_to_server(const char* msg) {
    if (!g_connected || g_server_socket == INVALID_SOCKET) return;
    send(g_server_socket, msg, (int)strlen(msg), 0);
}
void request_hand_refresh() {
    send_message_to_server(C2S_GET_HAND);
}
// --- Receiver thread ---
DWORD WINAPI receive_thread(LPVOID lpParam) {
    char buf[1024];
    int r;
    while (1) {
        r = recv(g_server_socket, buf, sizeof(buf)-1, 0);
        if (r > 0) {
            buf[r] = '\0';
            process_server_message(buf);
        } else if (r == 0) {
            printf("Server closed connection.\n");
            EnterCriticalSection(&g_game_lock);
            sprintf(g_state.status_message, "Disconnected from server.");
            g_connected = false;
            LeaveCriticalSection(&g_game_lock);
            break;
        } else {
            printf("recv error: %d\n", WSAGetLastError());
            EnterCriticalSection(&g_game_lock);
            sprintf(g_state.status_message, "Network error: %d", WSAGetLastError());
            LeaveCriticalSection(&g_game_lock);
            break;
        }
    }
    return 0;
}

void process_server_message(const char* msg) {
    printf("[CLIENT] Received: %s\n", msg);

    EnterCriticalSection(&g_game_lock);

    // Debug print
    printf("Server -> %s\n", msg);

    if (strncmp(msg, S2C_ID_ASSIGN, strlen(S2C_ID_ASSIGN)) == 0) {
        int id = atoi(msg + strlen(S2C_ID_ASSIGN));
        g_state.my_player_id = id;
        sprintf(g_state.status_message, "Assigned Player ID: %d", id);
    }
    else if (strncmp(msg, S2C_HAND, strlen(S2C_HAND)) == 0) {
        const char* payload = msg + strlen(S2C_HAND);
        parse_and_set_hand(payload);
        sprintf(g_state.status_message, "Hand received (%d cards)", g_state.my_hand_size);
    }else if (strcmp(msg, S2C_RESET_GAME) == 0){
        initialize_game_state();
        request_hand_refresh();
        printf("Game restarted\n");
    }

    else if (strncmp(msg, S2C_STATE, strlen(S2C_STATE)) == 0) {
    const char* p = msg + strlen(S2C_STATE);
    char buf[512];
    strncpy(buf, p, sizeof(buf)-1); buf[sizeof(buf)-1] = '\0';

    char* token = strtok(buf, ":");
        while (token) {
        if (strncmp(token, "TOP_", 4) == 0) {
            const char* cardtok = token + 4;
            g_state.top_card = deserialize_card(cardtok);
            g_state.top_card_valid = true;

        } else if (strncmp(token, "TURN_", 5) == 0) {
            g_state.current_turn = atoi(token + 5);
            g_state.is_my_turn = (g_state.my_player_id == g_state.current_turn);

        } else if (strncmp(token, "DIR_", 4) == 0) {
            g_state.direction = atoi(token + 4);
            flowDir = g_state.direction;

        } else if (strncmp(token, "N_", 2) == 0) {
            g_state.total_players = atoi(token + 2);

        } else if (token[0] == 'P' && strchr(token, '_')) {
            int pid=0, sz=0;
            if (sscanf(token, "P%d_%d", &pid, &sz) == 2) {
                if (pid >=0 && pid < MAX_PLAYERS)
                    g_state.player_hand_sizes[pid] = sz;
            }
        }
        token = strtok(NULL, ":");
    }

    // 🔥 NEW: detect WIN from STATE, e.g. "...:WIN:0"
    const char *winPos = strstr(p, "WIN:");
    if (winPos) {
        int winId = atoi(winPos + 4);

        g_state.game_over   = true;
        g_state.winner_id   = winId;
        g_state.is_my_turn  = false;
        g_state.choosing_color = false;
        g_state.drawn       = false;

        if (winId == g_state.my_player_id)
            sprintf(g_state.status_message, "YOU WON THE GAME!");
        else
            sprintf(g_state.status_message, "PLAYER %d WON!", winId);

        printf("CLIENT: detected WIN from STATE. Winner = %d\n", winId);
    }
    else {
        // normal turn status only if game not over
        if (g_state.is_my_turn)
            sprintf(g_state.status_message, "Your turn");
        else
            sprintf(g_state.status_message, "Waiting for turn");
    }

    request_hand_refresh();
}


    else if (strncmp(msg, S2C_TURN, strlen(S2C_TURN)) == 0) {
        int who = atoi(msg + strlen(S2C_TURN));
        g_state.current_turn = who;
        g_state.is_my_turn = (who == g_state.my_player_id);
        g_state.drawn = false;
        if (g_state.is_my_turn) sprintf(g_state.status_message, "Your turn");
        else sprintf(g_state.status_message, "Waiting for turn");
        request_hand_refresh();
    }
    else if (strncmp(msg, S2C_MSG, strlen(S2C_MSG)) == 0) {
        const char* text = msg + strlen(S2C_MSG);
        snprintf(g_state.status_message, sizeof(g_state.status_message), "%s", text);
    }
    else if (strncmp(msg, S2C_WIN, strlen(S2C_WIN)) == 0)
{
    int id = atoi(msg + strlen(S2C_WIN));

    g_state.game_over    = true;
    g_state.winner_id    = id;
    g_state.is_my_turn   = false;
    g_state.choosing_color = false;
    g_state.drawn        = false;

    if (id == g_state.my_player_id)
        sprintf(g_state.status_message, "YOU WON THE GAME!");
    else
        sprintf(g_state.status_message, "PLAYER %d WON!", id);

    printf("CLIENT: GAME OVER triggered from S2C_WIN. Winner: %d\n", id);
}


    else if (strncmp(msg, S2C_COLOR_REQ, strlen(S2C_COLOR_REQ)) == 0) {
        g_state.choosing_color = true;
        snprintf(g_state.status_message, sizeof(g_state.status_message),
             "Choose a color for your Wild card!");
    }
    else if (strncmp(msg, S2C_UNO_START, strlen(S2C_UNO_START)) == 0) {
        unsigned int ms = atoi(msg + strlen(S2C_UNO_START));
        g_state.need_uno_client = true;
        g_state.uno_time_ms = (ms > 0 ? ms : 3000);
        g_state.uno_start_time = GetTime();
        snprintf(g_state.status_message, sizeof(g_state.status_message),
                 "UNO! Click the UNO button within %.1f seconds!", g_state.uno_time_ms / 1000.0f);
    }
    else if (strncmp(msg, S2C_UNO_OK, strlen(S2C_UNO_OK)) == 0) {
        int pid = atoi(msg + strlen(S2C_UNO_OK));
        if (pid == g_state.my_player_id)
            snprintf(g_state.status_message, sizeof(g_state.status_message), "UNO called successfully!");
        else
            snprintf(g_state.status_message, sizeof(g_state.status_message), "Player %d called UNO!", pid);
        g_state.need_uno_client = false;
    }

    LeaveCriticalSection(&g_game_lock);
}

// --- parse_and_set_hand ---
void parse_and_set_hand(const char* list) {
    g_state.my_hand_size = 0;
    if (!list || strlen(list) == 0) return;
    char buff[512];
    strncpy(buff, list, sizeof(buff)-1);
    buff[sizeof(buff)-1] = '\0';

    char* tok = strtok(buff, ",");
    while (tok && g_state.my_hand_size < 64) {
        Card c = deserialize_card(tok);
        g_state.my_hand[g_state.my_hand_size++] = c;
        tok = strtok(NULL, ",");
    }
}

// --- UI input handling (mouse-only) ---
// Note: card clicking remains here. Draw/UNO/Pass are handled in draw_game_screen's UI buttons.
void handle_input() {
    EnterCriticalSection(&g_game_lock);
    bool need_join = (g_state.my_player_id == -1);
    LeaveCriticalSection(&g_game_lock);
    if (need_join) {
        send_message_to_server("JOIN:Player");
        return;
    }

    if (g_state.choosing_color && IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
        Vector2 m = GetMousePosition();
        Rectangle rR = { SCREEN_WIDTH/2 - 220, SCREEN_HEIGHT/2 - 50, 80, 80 };
        Rectangle rG = { SCREEN_WIDTH/2 - 120, SCREEN_HEIGHT/2 - 50, 80, 80 };
        Rectangle rB = { SCREEN_WIDTH/2 -  20, SCREEN_HEIGHT/2 - 50, 80, 80 };
        Rectangle rY = { SCREEN_WIDTH/2 +  80, SCREEN_HEIGHT/2 - 50, 80, 80 };

        char chosen = 0;
        if (CheckCollisionPointRec(m, rR)) chosen = 'R';
        else if (CheckCollisionPointRec(m, rG)) chosen = 'G';
        else if (CheckCollisionPointRec(m, rB)) chosen = 'B';
        else if (CheckCollisionPointRec(m, rY)) chosen = 'Y';

        if (chosen) {
            char msg[32];
            snprintf(msg, sizeof(msg), C2S_COLOR_SELECT"%c", chosen);
            send_message_to_server(msg);

            g_state.choosing_color = false;
            g_state.is_my_turn = false;
            snprintf(g_state.status_message, sizeof(g_state.status_message),
                     "You chose color %c", chosen);
        }
        return;
    }

    // Card clicking (bottom-center) remains
    if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
        Vector2 m = GetMousePosition();

        EnterCriticalSection(&g_game_lock);
        bool myTurn = g_state.is_my_turn;
        LeaveCriticalSection(&g_game_lock);

        // 2) Card clicking (bottom-center)
        EnterCriticalSection(&g_game_lock);
        if (g_state.is_my_turn && g_state.my_hand_size > 0) {
            float overlap = 95.0f;
            float totalWidth = (g_state.my_hand_size - 1) * overlap + CARD_WIDTH;
            float startX = (SCREEN_WIDTH / 2.0f) - (totalWidth / 2.0f);
            float y = SCREEN_HEIGHT - CARD_HEIGHT - 100.0f;

            for (int i = 0; i < g_state.my_hand_size; i++) {
                Rectangle cardRect = { startX + i * overlap, y, CARD_WIDTH, CARD_HEIGHT };

                if (CheckCollisionPointRec(m, cardRect)) {
                    Card top = g_state.top_card;
                    Card candidate = g_state.my_hand[i];

                    if (can_play(candidate, top) == 1) {
                        if (candidate.type == Wild || candidate.type == Wild_draw_4) {
                            char cardstr[16];
                            strcpy(cardstr, serialize_card(candidate));
                            char outmsg[64];
                            snprintf(outmsg, sizeof(outmsg), C2S_PLAY"%s", cardstr);
                            send_message_to_server(outmsg);
                            snprintf(g_state.status_message, sizeof(g_state.status_message),
                                     "Played Wild. Waiting for color request...");
                            LeaveCriticalSection(&g_game_lock);
                            return;
                        }

                        // Normal card play
                        char cardstr[16];
                        strcpy(cardstr, serialize_card(candidate));
                        char outmsg[64];
                        snprintf(outmsg, sizeof(outmsg), C2S_PLAY"%s", cardstr);
                        send_message_to_server(outmsg);

                        for (int j = i; j < g_state.my_hand_size - 1; j++)
                            g_state.my_hand[j] = g_state.my_hand[j + 1];

                        g_state.my_hand_size--;
                        g_state.is_my_turn = false;
                        sprintf(g_state.status_message, "Played %s", cardstr);
                    } else {
                        snprintf(g_state.status_message, sizeof(g_state.status_message), "You can't play that card");
                    }

                    break;
                }
            }
        }
        LeaveCriticalSection(&g_game_lock);
    }
}

float sLerp(float a, float b, float t) {
    return a + (b - a) * t;
}

// --- UI drawing ---
void draw_game_screen() {
    // Background
    Color A, B;
    GetNeutralFlowColors(&A, &B);
    UpdateFlow();
    DrawFlowingBackground(A, B);

    // Status (top-right)
    int pad = 12;
    Vector2 statusSize = MeasureTextEx(gameFont, g_state.status_message, 20, 1);
    float statusX = SCREEN_WIDTH - statusSize.x - pad;
    float statusY = pad;
    DrawTextEx(gameFont, g_state.status_message, (Vector2){ statusX, statusY }, 20, 1, (Color){80,80,80,220});
    // ================== GAME OVER MODE ==================
if (g_state.game_over)
{
    // Dark overlay
    DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, (Color){0, 0, 0, 80});

    // Main text
    char winbuf[64];
    sprintf(winbuf, "PLAYER %d WINS!", g_state.winner_id);

    Vector2 size = MeasureTextEx(gameFont, winbuf, 42, 2);

    DrawRectangleRounded(
        (Rectangle){
            SCREEN_WIDTH/2 - (size.x/2) - 40,
            SCREEN_HEIGHT/2 - 140,
            size.x + 80,
            120
        },
        0.3f, 10,
        (Color){20,20,20,200}
    );

    DrawTextEx(gameFont, winbuf,
        (Vector2){
            SCREEN_WIDTH/2 - size.x/2,
            SCREEN_HEIGHT/2 - 110
        },
        42, 2,
        GOLD
    );

    // Play Again button
    Rectangle againBtn = {
        SCREEN_WIDTH/2 - 140,
        SCREEN_HEIGHT/2 + 10,
        280,
        60
    };

    if (DrawButton(againBtn, "PLAY AGAIN")) {
        send_message_to_server(C2S_PLAY_AGAIN);
    }

    return;   // IMPORTANT: stops rest of draw_game_screen()
}
// =====================================================


    // Top Card
    Vector2 topPos = { SCREEN_WIDTH / 2.0f - CARD_WIDTH / 2.0f, SCREEN_HEIGHT / 2.0f - CARD_HEIGHT / 2.0f - 40.0f };
    if (g_state.top_card_valid) {
        DrawTextEx(gameFont, "Top Card", (Vector2){ topPos.x - 10, topPos.y - 30 }, 20, 1, (Color){80,80,80,220});
        DrawCard(g_state.top_card, topPos);
    } else {
        DrawTextEx(gameFont, "Top Card: (none)", (Vector2){ SCREEN_WIDTH/2 - 60, SCREEN_HEIGHT/2 - 100 }, 20, 1, (Color){80,80,80,220});
    }

    // Center "YOUR TURN" big display
    float baseSize = 52;                         // your base size
float pulse = 1.0f + 0.05f * sinf(GetTime() * 5.0f);
float size = baseSize * pulse;               // scaled size

const char* msg = g_state.is_my_turn ? "YOUR TURN" : "Waiting...";

// measure text at scaled size
Vector2 tsize = MeasureTextEx(gameFont, msg, size, 2);

// compute centered position
float tx = SCREEN_WIDTH/2  - tsize.x/2;
float ty = SCREEN_HEIGHT/2 - 180 - tsize.y/2;

// draw centered, pulsing text
DrawTextEx(gameFont, msg, (Vector2){tx, ty}, size, 2, 
           g_state.is_my_turn ? BLUE : (Color){120,120,120,255});

    // Player table (right)
    DrawPlayerTable();

    // Buttons under discard pile (rounded)
    float btnW = 120, btnH = 44;
    float gap = 18;
    float totalBtns = 3;
    // Position below top card center
    float btnY = topPos.y + CARD_HEIGHT + 24;
    float startX = SCREEN_WIDTH/2 - ((btnW * totalBtns + gap * (totalBtns-1)) / 2.0f);

    Rectangle drawBtn = { startX, btnY, btnW, btnH };
    Rectangle unoBtn  = { startX + (btnW + gap), btnY, btnW, btnH };
    Rectangle passBtn = { startX + 2*(btnW + gap), btnY, btnW, btnH };

    // Draw and handle clicks (DrawButton returns true on click)
    if (DrawButton(drawBtn, "Draw Card")) {
        EnterCriticalSection(&g_game_lock);
        if (g_state.is_my_turn && !g_state.drawn) {
            send_message_to_server(C2S_DRAW);
            g_state.drawn = true;
        } else if (g_state.drawn) {
            snprintf(g_state.status_message, sizeof(g_state.status_message), "You already drew! Play or Pass.");
        } else {
            snprintf(g_state.status_message, sizeof(g_state.status_message), "Not your turn");
        }
        LeaveCriticalSection(&g_game_lock);
    }

    if (DrawButton(unoBtn, "UNO!")) {
        EnterCriticalSection(&g_game_lock);
        if (g_state.need_uno_client) {
            send_message_to_server(C2S_UNO);
            g_state.need_uno_client = false;
            snprintf(g_state.status_message, sizeof(g_state.status_message), "UNO sent!");
        } else {
            snprintf(g_state.status_message, sizeof(g_state.status_message), "UNO not required right now");
        }
        LeaveCriticalSection(&g_game_lock);
    }

    if (DrawButton(passBtn, "Pass")) {
        EnterCriticalSection(&g_game_lock);
        if (g_state.is_my_turn) {
            if (g_state.drawn) {
                send_message_to_server(C2S_PASS);
                g_state.is_my_turn = false;
                snprintf(g_state.status_message, sizeof(g_state.status_message), "You passed your turn.");
            } else {
                snprintf(g_state.status_message, sizeof(g_state.status_message), "You must draw before passing!");
            }
        } else {
            snprintf(g_state.status_message, sizeof(g_state.status_message), "Not your turn.");
        }
        LeaveCriticalSection(&g_game_lock);
    }

    // Player Hand
    float yBase = SCREEN_HEIGHT - CARD_HEIGHT - 100.0f;
    float overlap = 95.0f;  // Increase for less overlap
    float totalWidth = (g_state.my_hand_size - 1) * overlap + CARD_WIDTH;
    float startXHand = (SCREEN_WIDTH - totalWidth) / 2.0f;

    Vector2 mouse = GetMousePosition();
    int hoveredIndex = -1;

    for (int i = g_state.my_hand_size - 1; i >= 0; i--) {
        float x = startXHand + i * overlap;
        Rectangle cardRect = { x, yBase, CARD_WIDTH, CARD_HEIGHT };
        if (CheckCollisionPointRec(mouse, cardRect)) {
            hoveredIndex = i;
            break;
        }
    }

    static float lift[128] = {0};

    for (int i = 0; i < g_state.my_hand_size; i++) {
        float x = startXHand + i * overlap;
        float y = yBase;

        if (i == hoveredIndex)
            lift[i] = sLerp(lift[i], -25.0f, 0.25f);
        else
            lift[i] = sLerp(lift[i], 0.0f, 0.25f);

        if (i == hoveredIndex) {

    // Soft but visible glow
    DrawRectangleRounded(
        (Rectangle){
            x - 4,                     // only slightly bigger
            y + lift[i] - 4,
            CARD_WIDTH + 8,
            CARD_HEIGHT + 8
        },
        0.18f, 14,
        (Color){255, 200, 60, 110}     // stronger glow (alpha 110)
    );

    // Crisp inner outline
    DrawRectangleRoundedLines(
        (Rectangle){
            x - 2,
            y + lift[i] - 2,
            CARD_WIDTH + 4,
            CARD_HEIGHT + 4
        },
        0.18f, 14,
        (Color){120, 255, 200, 220}     // bright mint outline
    );
}

        Vector2 pos = { x, y + lift[i] };
        DrawCard(g_state.my_hand[i], pos);
        DrawRectangleRoundedLines((Rectangle){x, y + lift[i], CARD_WIDTH, CARD_HEIGHT}, 0.2f, 8, (Color){0, 0, 0, 30});
    }

    // Current Turn info (bottom-right)
    char turnbuf[64];
    snprintf(turnbuf, sizeof(turnbuf), "Turn: P%d", g_state.current_turn);
    DrawTextEx(gameFont, turnbuf, (Vector2){ SCREEN_WIDTH - 220, SCREEN_HEIGHT - 40 }, 20, 1, (Color){80,80,80,220});

    if (g_state.choosing_color) {
        DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, (Color){0, 0, 0, 150});
        DrawTextEx(gameFont, "Choose a color!", (Vector2){SCREEN_WIDTH/2 - 120, SCREEN_HEIGHT/2 - 120}, 30, 1, WHITE);

        Color softRed = (Color){255, 120, 120, 255};
        Color softGreen = (Color){150, 220, 150, 255};
        Color softBlue = (Color){130, 180, 255, 255};
        Color softYellow = (Color){255, 245, 170, 255};

        Rectangle rR = { SCREEN_WIDTH/2 - 220, SCREEN_HEIGHT/2 - 50, 80, 80 };
        Rectangle rG = { SCREEN_WIDTH/2 - 120, SCREEN_HEIGHT/2 - 50, 80, 80 };
        Rectangle rB = { SCREEN_WIDTH/2 -  20, SCREEN_HEIGHT/2 - 50, 80, 80 };
        Rectangle rY = { SCREEN_WIDTH/2 +  80, SCREEN_HEIGHT/2 - 50, 80, 80 };

        DrawRectangleRounded(rR, 0.3f, 8, softRed);
        DrawRectangleRounded(rG, 0.3f, 8, softGreen);
        DrawRectangleRounded(rB, 0.3f, 8, softBlue);
        DrawRectangleRounded(rY, 0.3f, 8, softYellow);

        DrawRectangleLinesEx(rR, 3, WHITE);
        DrawRectangleLinesEx(rG, 3, WHITE);
        DrawRectangleLinesEx(rB, 3, WHITE);
        DrawRectangleLinesEx(rY, 3, WHITE);
    }
}


// // client.c
// #pragma comment(lib, "ws2_32.lib") // Link against ws2_32.lib
// #pragma comment(lib, "raylib.lib") // Link against raylib.lib

// // --- Critical Fix for Windows/Raylib Conflicts ---
// #define WIN32_LEAN_AND_MEAN
// #define NOGDI
// #define NOUSER
// #define NOMINMAX
// #define NOIME
// #define NOSOUND
// #define NOMCX
// #define NOHELP

// #include <WinSock2.h>
// #include <WS2tcpip.h>
// #include <windows.h>
// #include <stdio.h>
// #include <string.h>
// #include <stdbool.h>
// #include <stdlib.h>

// #include "include/raylib.h"
// #include "helpers_ui.h"
// #include "serializer.h"      // serialize_card, deserialize_card, serialize_hand (if needed)
// #include "shared_protocol.h" // S2C_*, C2S_* constants
// #include "game2.h"  
// #include "player_hand.h"         // Card, card_color, MAX_PLAYERS (provided in your project)
// #include <math.h>
// // --- UI / Layout constants ---
// #define SCREEN_WIDTH 1280
// #define SCREEN_HEIGHT 720

// // Default server values (you said you'll change IP when necessary)
// #define DEFAULT_SERVER_IP "127.0.0.1"
// // #define DEFAULT_SERVER_IP "10.69.47.199"
// #define DEFAULT_SERVER_PORT 8888


// // --- Client-side game state ---
// typedef struct {
//     Card pending_wild_card;
//     Card my_hand[64];
//     int my_hand_size;
//     Card top_card;
//     bool top_card_valid;
//     int player_hand_sizes[MAX_PLAYERS];
//     int current_turn;
//     int my_player_id;
//     int total_players;
//     int direction;
//     bool is_my_turn;
//     bool game_over;
//     int winner_id;
//     bool drawn;
//     bool need_uno_client;     // ✅ true when server says you must call UNO
//     unsigned int uno_time_ms; // ✅ countdown in milliseconds
//     double uno_start_time;
//     bool choosing_color;
//     char status_message[256];
// } ClientGameState;

// // --- Globals ---
// CRITICAL_SECTION g_game_lock;
// ClientGameState g_state;
// SOCKET g_server_socket = INVALID_SOCKET;
// bool g_connected = false;
// float bgOffset = 0.0f;
// float flow = 0.0f;
// int bgDirection = 1;
// float bgShift = 0.0f;
// float timeFlow = 0.0f;
// int flowDir = 1;


// // --- Function prototypes ---
// void request_hand_refresh();
// DWORD WINAPI receive_thread(LPVOID lpParam);
// void process_server_message(const char* msg);
// void send_message_to_server(const char* msg);
// void initialize_game_state();
// void draw_game_screen();
// void handle_input();
// void parse_and_set_hand(const char* list); // helper

// // --- Main ---
// int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
//     WSADATA wsaData;
//     if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0) {
//         printf("WSAStartup failed.\n");
//         return 1;
//     }

//     // Init Raylib
//     InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "UNO Client");
//     SetTargetFPS(60);

//     initialize_game_state();
//     InitializeCriticalSection(&g_game_lock);

//     // --- Networking setup ---
//     struct sockaddr_in server_addr;
//     g_server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
//     if (g_server_socket == INVALID_SOCKET) {
//         printf("Socket creation failed: %d\n", WSAGetLastError());
//         WSACleanup();
//         return 1;
//     }

//     server_addr.sin_family = AF_INET;
//     server_addr.sin_port = htons(DEFAULT_SERVER_PORT);

//     if (InetPton(AF_INET, DEFAULT_SERVER_IP, &server_addr.sin_addr) != 1) {
//         printf("Invalid address/ Address not supported\n");
//         closesocket(g_server_socket);
//         WSACleanup();
//         return 1;
//     }

//     if (connect(g_server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
//         printf("Connection failed: %d\n", WSAGetLastError());
//         closesocket(g_server_socket);
//         WSACleanup();
//         return 1;
//     }

//     g_connected = true;
//     printf("Connected to server %s:%d\n", DEFAULT_SERVER_IP, DEFAULT_SERVER_PORT);

//     // Start receive thread
//     CreateThread(NULL, 0, receive_thread, NULL, 0, NULL);

//     // Game/render loop
//     while (!WindowShouldClose()) {
//         handle_input();

//         BeginDrawing();
//         ClearBackground(RAYWHITE);

//         EnterCriticalSection(&g_game_lock);
//         draw_game_screen();
//         LeaveCriticalSection(&g_game_lock);

//         EndDrawing();
//     }

//     // Cleanup
//     CloseWindow();
//     if (g_connected) closesocket(g_server_socket);
//     DeleteCriticalSection(&g_game_lock);
//     WSACleanup();
//     return 0;
// }

// // --- Initialization ---
// void initialize_game_state() {
//     g_state.my_hand_size = 0;
//     g_state.current_turn = -1;
//     g_state.my_player_id = -1;
//     g_state.total_players = 0;
//     g_state.direction = 1;
//     g_state.is_my_turn = false;
//     g_state.game_over = false;
//     g_state.top_card_valid = false;
//     g_state.choosing_color = false ;
//         g_state.need_uno_client = false;
//     g_state.uno_time_ms = 0;
//     g_state.uno_start_time = 0.0;
//     Font gameFont = LoadFont("assets/font/Baloo2-Regular.ttf");
//     SetTextureFilter(gameFont.texture, TEXTURE_FILTER_BILINEAR);

//     g_state.drawn = false ;
//     memset(&g_state.pending_wild_card, 0, sizeof(Card));
//     sprintf(g_state.status_message, "Waiting to join...");
//     for (int i = 0; i < MAX_PLAYERS; i++) g_state.player_hand_sizes[i] = 0;
// }

// // --- Networking: send helper ---
// void send_message_to_server(const char* msg) {
//     if (!g_connected || g_server_socket == INVALID_SOCKET) return;
//     send(g_server_socket, msg, (int)strlen(msg), 0);
// }
// void request_hand_refresh() {
//     send_message_to_server(C2S_GET_HAND); // You can define this in shared_protocol.h
// }
// // --- Receiver thread ---
// DWORD WINAPI receive_thread(LPVOID lpParam) {
//     char buf[1024];
//     int r;
//     while (1) {
//         r = recv(g_server_socket, buf, sizeof(buf)-1, 0);
//         if (r > 0) {
//             buf[r] = '\0';
//             process_server_message(buf);
//         } else if (r == 0) {
//             printf("Server closed connection.\n");
//             EnterCriticalSection(&g_game_lock);
//             sprintf(g_state.status_message, "Disconnected from server.");
//             g_connected = false;
//             LeaveCriticalSection(&g_game_lock);
//             break;
//         } else {
//             printf("recv error: %d\n", WSAGetLastError());
//             EnterCriticalSection(&g_game_lock);
//             sprintf(g_state.status_message, "Network error: %d", WSAGetLastError());
//             LeaveCriticalSection(&g_game_lock);
//             break;
//         }
//     }
//     return 0;
// }

// void process_server_message(const char* msg) {
//     EnterCriticalSection(&g_game_lock);

//     // Debug print
//     printf("Server -> %s\n", msg);

//     if (strncmp(msg, S2C_ID_ASSIGN, strlen(S2C_ID_ASSIGN)) == 0) {
//         int id = atoi(msg + strlen(S2C_ID_ASSIGN));
//         g_state.my_player_id = id;
//         sprintf(g_state.status_message, "Assigned Player ID: %d", id);
//     }
//     else if (strncmp(msg, S2C_HAND, strlen(S2C_HAND)) == 0) {
//         // Format after prefix: comma-separated cards, e.g. "R1,G5,W+4"
//         const char* payload = msg + strlen(S2C_HAND);
//         parse_and_set_hand(payload);
//         sprintf(g_state.status_message, "Hand received (%d cards)", g_state.my_hand_size);
//     }
//     else if (strncmp(msg, S2C_STATE, strlen(S2C_STATE)) == 0) {
//         // Example format created by server: "TOP_R1:TURN_1:DIR_1:P0_5:P1_7:"
//         const char* p = msg + strlen(S2C_STATE);
//         // tokenize by ':' segments
//         char buf[512];
//         strncpy(buf, p, sizeof(buf)-1); buf[sizeof(buf)-1] = '\0';
//         char* token = strtok(buf, ":");
//         while (token) {
//             if (strncmp(token, "TOP_", 4) == 0) {
//                 // parse top card
//                 const char* cardtok = token + 4;
//                 g_state.top_card = deserialize_card(cardtok);
//                 g_state.top_card_valid = true;
//             } else if (strncmp(token, "TURN_", 5) == 0) {
//                 g_state.current_turn = atoi(token + 5);
//                 // set is_my_turn based on player id
//                 g_state.is_my_turn = (g_state.my_player_id == g_state.current_turn);
//             } else if (strncmp(token, "DIR_", 4) == 0) {
//                 g_state.direction = atoi(token + 4);
//                 flowDir = g_state.direction;

//             } else if (token[0] == 'P' && strchr(token, '_')) {
//                 // P<i>_<size>
//                 int pid=0, sz=0;
//                 if (sscanf(token, "P%d_%d", &pid, &sz) == 2) {
//                     if (pid >= 0 && pid < MAX_PLAYERS) g_state.player_hand_sizes[pid] = sz;
//                 }
//             }
//             token = strtok(NULL, ":");
//         }
//         // update status
//         if (g_state.is_my_turn) sprintf(g_state.status_message, "Your turn");
//         else sprintf(g_state.status_message, "Waiting for turn");
//         request_hand_refresh();
//     }
//     else if (strncmp(msg, S2C_TURN, strlen(S2C_TURN)) == 0) {
//         // S2C_TURNn
//         int who = atoi(msg + strlen(S2C_TURN));
//         g_state.current_turn = who;
//         g_state.is_my_turn = (who == g_state.my_player_id);
//         g_state.drawn = false;
//         if (g_state.is_my_turn) sprintf(g_state.status_message, "Your turn");
//         else sprintf(g_state.status_message, "Waiting for turn");
//         request_hand_refresh();
//     }
//     else if (strncmp(msg, S2C_MSG, strlen(S2C_MSG)) == 0) {
//         const char* text = msg + strlen(S2C_MSG);
//         snprintf(g_state.status_message, sizeof(g_state.status_message), "%s", text);
//     }
//     else if (strncmp(msg, S2C_WIN, strlen(S2C_WIN)) == 0) {
//         int winner = atoi(msg + strlen(S2C_WIN));
//         g_state.game_over = true;
//         g_state.winner_id = winner;
//         snprintf(g_state.status_message, sizeof(g_state.status_message), "Game over! Winner: P%d", winner);
//     }
//     else if (strncmp(msg, S2C_COLOR_REQ, strlen(S2C_COLOR_REQ)) == 0) {
//         g_state.choosing_color = true;
//         snprintf(g_state.status_message, sizeof(g_state.status_message),
//              "Choose a color for your Wild card!");
//     }
//     // other message types can be added here...
//         else if (strncmp(msg, S2C_UNO_START, strlen(S2C_UNO_START)) == 0) {
//         // Server started UNO timer for this player
//         unsigned int ms = atoi(msg + strlen(S2C_UNO_START));
//         g_state.need_uno_client = true;
//         g_state.uno_time_ms = (ms > 0 ? ms : 3000);
//         g_state.uno_start_time = GetTime();
//         snprintf(g_state.status_message, sizeof(g_state.status_message),
//                  "UNO! Click the UNO button within %.1f seconds!", g_state.uno_time_ms / 1000.0f);
//     }
//     else if (strncmp(msg, S2C_UNO_OK, strlen(S2C_UNO_OK)) == 0) {
//         int pid = atoi(msg + strlen(S2C_UNO_OK));
//         if (pid == g_state.my_player_id)
//             snprintf(g_state.status_message, sizeof(g_state.status_message), "UNO called successfully!");
//         else
//             snprintf(g_state.status_message, sizeof(g_state.status_message), "Player %d called UNO!", pid);
//         g_state.need_uno_client = false;
//     }
//     else if (strncmp(msg, S2C_MSG, strlen(S2C_MSG)) == 0) {
//         const char* text = msg + strlen(S2C_MSG);
//         snprintf(g_state.status_message, sizeof(g_state.status_message), "%s", text);
//     }

//     LeaveCriticalSection(&g_game_lock);
// }

// // --- parse_and_set_hand --- uses deserialize_card from serializer.h
// void parse_and_set_hand(const char* list) {
//     g_state.my_hand_size = 0;
//     if (!list || strlen(list) == 0) return;
//     // copy to mutable buffer
//     char buff[512];
//     strncpy(buff, list, sizeof(buff)-1);
//     buff[sizeof(buff)-1] = '\0';

//     char* tok = strtok(buff, ",");
//     while (tok && g_state.my_hand_size < 64) {
//         Card c = deserialize_card(tok);
//         g_state.my_hand[g_state.my_hand_size++] = c;
//         tok = strtok(NULL, ",");
//     }
// }

// // --- UI input handling (mouse-only) ---
// void handle_input() {
//     // If not assigned id yet, send a JOIN once (server will reply with ID)
//     EnterCriticalSection(&g_game_lock);
//     bool need_join = (g_state.my_player_id == -1);
//     LeaveCriticalSection(&g_game_lock);
//     if (need_join) {
//         // Send a JOIN message. Server may expect a name; using "Player" + id fallback.
//         send_message_to_server("JOIN:Player");
//         // Wait for ID assignment from server...
//         return;
//     }
// if (g_state.choosing_color && IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
//     Vector2 m = GetMousePosition();
//     Rectangle rR = { SCREEN_WIDTH/2 - 220, SCREEN_HEIGHT/2 - 50, 80, 80 };
//     Rectangle rG = { SCREEN_WIDTH/2 - 120, SCREEN_HEIGHT/2 - 50, 80, 80 };
//     Rectangle rB = { SCREEN_WIDTH/2 -  20, SCREEN_HEIGHT/2 - 50, 80, 80 };
//     Rectangle rY = { SCREEN_WIDTH/2 +  80, SCREEN_HEIGHT/2 - 50, 80, 80 };

//     char chosen = 0;
//     if (CheckCollisionPointRec(m, rR)) chosen = 'R';
//     else if (CheckCollisionPointRec(m, rG)) chosen = 'G';
//     else if (CheckCollisionPointRec(m, rB)) chosen = 'B';
//     else if (CheckCollisionPointRec(m, rY)) chosen = 'Y';

//     if (chosen) {
//         char msg[32];
//         snprintf(msg, sizeof(msg), C2S_COLOR_SELECT"%c", chosen);
//         send_message_to_server(msg);

//         g_state.choosing_color = false;
//         g_state.is_my_turn = false; // server will send next S2C_TURN
//         snprintf(g_state.status_message, sizeof(g_state.status_message),
//                  "You chose color %c", chosen);
//     }
//     return;
// }



//     // Mouse clicks handled on release to avoid multiple fire
//     if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
//         Vector2 m = GetMousePosition();

//         // 1) Buttons area (bottom-left)
//        // 1) Buttons area (bottom-left)
//         Rectangle draw_btn = { 20, SCREEN_HEIGHT - 60, 120, 40 };
//         Rectangle uno_btn  = { 150, SCREEN_HEIGHT - 60, 120, 40 };

// // Lock game state to check if it's your turn
//         EnterCriticalSection(&g_game_lock);
//         bool myTurn = g_state.is_my_turn;
//         LeaveCriticalSection(&g_game_lock);

//         if (CheckCollisionPointRec(m, draw_btn)) {
//             if (myTurn && !g_state.drawn) {
//                 send_message_to_server(C2S_DRAW);
//                 g_state.drawn = true;  // Mark that player drew once
//             } else if (g_state.drawn) {
//                     snprintf(g_state.status_message, sizeof(g_state.status_message),
//                         "You already drew! Play or Pass.");
//             } else {
//                     snprintf(g_state.status_message, sizeof(g_state.status_message),
//                         "Not your turn");
//             }
//             return;
//         }


//         if (CheckCollisionPointRec(m, uno_btn)) {
//     EnterCriticalSection(&g_game_lock);
//     bool can_uno = g_state.need_uno_client;
//     LeaveCriticalSection(&g_game_lock);

//     if (can_uno) {
//         send_message_to_server(C2S_UNO);
//         printf("[CLIENT] Sent UNO call\n");
//         EnterCriticalSection(&g_game_lock);
//         g_state.need_uno_client = false; // prevent double UNO click
//         snprintf(g_state.status_message, sizeof(g_state.status_message), "UNO sent!");
//         LeaveCriticalSection(&g_game_lock);
//     } else {
//         snprintf(g_state.status_message, sizeof(g_state.status_message),
//                  "UNO not required right now");
//     }
//     return;
// }

//         Rectangle pass_btn = { 280, SCREEN_HEIGHT - 60, 120, 40 };
//         if (CheckCollisionPointRec(m, pass_btn)) {
//     if (g_state.is_my_turn) {
//         // Only allow pass if the player has drawn once this turn
//         if (g_state.drawn) {
//             send_message_to_server(C2S_PASS);
//             g_state.is_my_turn = false;
//             snprintf(g_state.status_message, sizeof(g_state.status_message), "You passed your turn.");
//         } else {
//             snprintf(g_state.status_message, sizeof(g_state.status_message), "You must draw before passing!");
//         }
//     } else {
//         snprintf(g_state.status_message, sizeof(g_state.status_message), "Not your turn.");
//     }
//     return;
// }

//         // 2) Card clicking (bottom-center)
//         EnterCriticalSection(&g_game_lock);
//         if (g_state.is_my_turn && g_state.my_hand_size > 0) {
//             float totalWidth = g_state.my_hand_size * (CARD_WIDTH + CARD_PADDING);
//             float startX = (SCREEN_WIDTH / 2.0f) - (totalWidth / 2.0f) + CARD_PADDING;
//             float y = SCREEN_HEIGHT - CARD_HEIGHT - 100.0f;

//             for (int i = 0; i < g_state.my_hand_size; i++) {
//                 Rectangle cardRect = { startX + i * (CARD_WIDTH + CARD_PADDING), y, CARD_WIDTH, CARD_HEIGHT };

//                 if (CheckCollisionPointRec(m, cardRect)) {
//                     Card top = g_state.top_card;
//                     Card candidate = g_state.my_hand[i];

//             // ✅ Check if this card is playable before sending
//                    if (can_play(candidate, top) == 1) {
//                         // inside the loop when a card is clicked
// Card candidate = g_state.my_hand[i];
// Card top = g_state.top_card;

// if (candidate.type == Wild || candidate.type == Wild_draw_4) {
//     // Send the play request for the wild itself
//     char cardstr[16];
//     strcpy(cardstr, serialize_card(candidate));
//     char outmsg[64];
//     snprintf(outmsg, sizeof(outmsg), C2S_PLAY"%s", cardstr);
//     send_message_to_server(outmsg);

//     // Do NOT remove the card locally yet; server resolves after color
//     // Do NOT open color UI yet; wait for S2C_COLOR_REQ from server
//     snprintf(g_state.status_message, sizeof(g_state.status_message),
//              "Played Wild. Waiting for color request...");
//     LeaveCriticalSection(&g_game_lock);
//     return;
// }


//     // --- Normal card play ---
//                         char cardstr[16];
//                         strcpy(cardstr, serialize_card(candidate));

//                         char outmsg[64];
//                         snprintf(outmsg, sizeof(outmsg), C2S_PLAY"%s", cardstr);
//                         send_message_to_server(outmsg);

//                         for (int j = i; j < g_state.my_hand_size - 1; j++)
//                             g_state.my_hand[j] = g_state.my_hand[j + 1];

//                         g_state.my_hand_size--;
//                         g_state.is_my_turn = false;
//                         sprintf(g_state.status_message, "Played %s", cardstr);
//                     } else {
//                         snprintf(g_state.status_message, sizeof(g_state.status_message), "You can't play that card");
//                     }

//                     break; // Exit loop after click
//                 }
//             }
//         }


//         LeaveCriticalSection(&g_game_lock);
//     }
// }

// float sLerp(float a, float b, float t) {
//     return a + (b - a) * t;
// }

// void UpdateFlow()
// {
//     timeFlow += 0.01f * flowDir;   // slow smooth motion
// }
// void DrawFlowingBackground(Color A, Color B)
// {
//     float freq = 0.005f;   // how “wide” the waves are
//     float h = SCREEN_HEIGHT;

//     for (int x = 0; x < SCREEN_WIDTH; x++)
//     {
//         float t = 0.5f + 0.5f * sinf(x * freq + timeFlow);

//         Color c = (Color){
//             (unsigned char)(A.r + (B.r - A.r) * t),
//             (unsigned char)(A.g + (B.g - A.g) * t),
//             (unsigned char)(A.b + (B.b - A.b) * t),
//             255
//         };

//         DrawLine(x, 0, x, h, c);
//     }
// }
// void GetFlowColors(Color* A, Color* B)
// {
//     *A = (Color){ 225, 230, 245, 255 };   // soft neutral-teal white
//     *B = (Color){ 195, 205, 225, 255 };   // slightly darker cool neutral
// }

// void UpdateBackground()
// {
//     bgShift += 0.4f * bgDirection;   // visible motion

//     if (bgShift > SCREEN_WIDTH) bgShift = 0;
//     if (bgShift < 0) bgShift = SCREEN_WIDTH;
// }
// void DrawFinalGradient(Color A, Color B)
// {
//     // Base gradient (static)
//     DrawRectangleGradientH(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, A, B);

//     // Moving overlay (super subtle)
//     Color overlayA = (Color){255, 255, 255, 35}; // 15% bright
//     Color overlayB = (Color){255, 255, 255, 0};

//     DrawRectangleGradientH(bgShift, 0, SCREEN_WIDTH, SCREEN_HEIGHT, overlayA, overlayB);
//     DrawRectangleGradientH(bgShift - SCREEN_WIDTH, 0, SCREEN_WIDTH, SCREEN_HEIGHT, overlayA, overlayB);
// }


// void GetGradientColors(Card top, Color* A, Color* B)
// {
//     switch (top.color) {
//         case COLOR_RED:
//             *A = (Color){255, 190, 190, 255};
//             *B = (Color){240, 150, 150, 255};
//             break;

//         case COLOR_GREEN:
//             *A = (Color){200, 240, 210, 255};
//             *B = (Color){160, 220, 180, 255};
//             break;

//         case COLOR_BLUE:
//             *A = (Color){190, 210, 255, 255};
//             *B = (Color){150, 180, 230, 255};
//             break;

//         case COLOR_YELLOW:
//             *A = (Color){255, 245, 200, 255};
//             *B = (Color){245, 230, 170, 255};
//             break;

//         default:
//             *A = (Color){230, 230, 230, 255};
//             *B = (Color){200, 200, 200, 255};
//             break;
//     }
// }

// void DrawSoftGradient(Color baseA, Color baseB)
// {
//     Color c1 = ColorLerp(baseA, baseB, flow);
//     Color c2 = ColorLerp(baseA, baseB, 1.0f - flow);

//     DrawRectangleGradientH(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, c1, c2);
// }

// void DrawBackgroundOverlay()
// {
//     Color fade = (Color){255, 255, 255, 30}; // subtle glow
//     DrawCircleGradient(
//         SCREEN_WIDTH/2, SCREEN_HEIGHT/2,
//         SCREEN_WIDTH,
//         fade, BLANK
//     );
// }


// // --- UI drawing ---
// void draw_game_screen() {

//     if (g_state.top_card_valid) {
// Color A, B;
// // GetFlowColors(g_state.top_card, &A, &B);
// GetFlowColors(&A, &B);
// UpdateFlow();
// DrawFlowingBackground(A, B);

//     } else {
//         ClearBackground(RAYWHITE);
//     }

//     int pad = 12;
//     Vector2 statusSize = MeasureTextEx(GetFontDefault(), g_state.status_message, 20, 1);
//     float statusX = SCREEN_WIDTH - statusSize.x - pad;
//     float statusY = pad;
//     DrawText(g_state.status_message, (int)statusX, (int)statusY, 20, DARKGRAY);

//     // 🃏 2. Top Card (center)
//     if (g_state.top_card_valid) {
//         Vector2 topPos = { SCREEN_WIDTH / 2.0f - CARD_WIDTH / 2.0f, SCREEN_HEIGHT / 2.0f - CARD_HEIGHT / 2.0f - 40.0f };
//         DrawText("Top Card", (int)topPos.x - 10, (int)topPos.y - 30, 20, DARKGRAY);
//         DrawCard(g_state.top_card, topPos);
//     } else {
//         DrawText("Top Card: (none)", SCREEN_WIDTH/2 - 60, SCREEN_HEIGHT/2 - 100, 20, DARKGRAY);
//     }

//     // 👥 3. Opponent Info (right side)
//     DrawText("Players:", SCREEN_WIDTH - 220, 60, 20, DARKGRAY);
//     for (int i = 0; i < MAX_PLAYERS; i++) {
//         if (g_state.player_hand_sizes[i] > 0 || i < g_state.total_players) {
//             char buf[64];
//             snprintf(buf, sizeof(buf), "P%d: %d cards", i, g_state.player_hand_sizes[i]);
//             DrawText(buf, SCREEN_WIDTH - 220, 100 + i * 28, 20, (i == g_state.current_turn) ? BLUE : BLACK);
//             if (i == g_state.my_player_id)
//                 DrawText("(You)", SCREEN_WIDTH - 120, 100 + i * 28, 18, DARKGRAY);
//         }
//     }

//     // 🧩 4. Buttons (bottom-left)
//     Rectangle draw_btn = { 20, SCREEN_HEIGHT - 60, 120, 40 };
//     Rectangle uno_btn  = { 150, SCREEN_HEIGHT - 60, 120, 40 };
//     Rectangle pass_btn = { 280, SCREEN_HEIGHT - 60, 120, 40 };

//     DrawButton(pass_btn, "Pass");
//     DrawButton(draw_btn, "Draw Card");
//     DrawButton(uno_btn, "UNO!");

//     // 🎴 5. Player Hand (bottom-center)
//     float yBase = SCREEN_HEIGHT - CARD_HEIGHT - 100.0f;
//     float overlap = 95.0f;  // Increase for less overlap
//     float totalWidth = (g_state.my_hand_size - 1) * overlap + CARD_WIDTH;
//     float startX = (SCREEN_WIDTH - totalWidth) / 2.0f;

//     Vector2 mouse = GetMousePosition();
//     int hoveredIndex = -1;

//     // ✅ Reverse loop for collision detection (ensures correct card selection)
//     for (int i = g_state.my_hand_size - 1; i >= 0; i--) {
//         float x = startX + i * overlap;
//         Rectangle cardRect = { x, yBase, CARD_WIDTH, CARD_HEIGHT };
//         if (CheckCollisionPointRec(mouse, cardRect)) {
//             hoveredIndex = i;
//             break;
//         }
//     }

//     // 🪄 Draw cards with smooth hover animation
//     for (int i = 0; i < g_state.my_hand_size; i++) {
//         float x = startX + i * overlap;
//         float y = yBase;

//         // ✨ Hover animation (smoothly lift up)
//         static float lift[108]; // Enough space for animations
//         if (i == hoveredIndex)
//             lift[i] = sLerp(lift[i], -25.0f, 0.25f); // Smooth upward motion
//         else
//             lift[i] = sLerp(lift[i], 0.0f, 0.25f);   // Smoothly return down

//         Vector2 pos = { x, y + lift[i] };
//         DrawCard(g_state.my_hand[i], pos);

//         // 🩶 Soft shadow (for depth)
//         DrawRectangleRoundedLines((Rectangle){x, y + lift[i], CARD_WIDTH, CARD_HEIGHT}, 0.2f, 8, (Color){0, 0, 0, 30});
//     }

//     // 🔁 6. Current Turn Info (bottom-right)
//     char turnbuf[64];
//     snprintf(turnbuf, sizeof(turnbuf), "Turn: P%d", g_state.current_turn);
//     DrawText(turnbuf, SCREEN_WIDTH - 220, SCREEN_HEIGHT - 40, 20, DARKGRAY);

//     // 🏁 7. Game Over Banner
//     if (g_state.game_over) {
//         char winbuf[128];
//         snprintf(winbuf, sizeof(winbuf), "Game Over! Winner: P%d", g_state.winner_id);
//         Vector2 s = MeasureTextEx(GetFontDefault(), winbuf, 36, 2);
//         DrawRectangle((SCREEN_WIDTH - s.x) / 2 - 12, (SCREEN_HEIGHT - s.y) / 2 - 12, s.x + 24, s.y + 24, (Color){0, 0, 0, 180});
//         DrawText(winbuf, (int)((SCREEN_WIDTH - s.x)/2), (int)((SCREEN_HEIGHT - s.y)/2), 36, WHITE);
//     }

//     // 🎨 8. Wild Color Chooser Overlay
//     if (g_state.choosing_color) {
//         DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, (Color){0, 0, 0, 150});
//         DrawText("Choose a color!", SCREEN_WIDTH/2 - 120, SCREEN_HEIGHT/2 - 120, 30, WHITE);

//         // Use soft pastel tones instead of harsh colors
//         Color softRed = (Color){255, 120, 120, 255};
//         Color softGreen = (Color){150, 220, 150, 255};
//         Color softBlue = (Color){130, 180, 255, 255};
//         Color softYellow = (Color){255, 245, 170, 255};

//         Rectangle rR = { SCREEN_WIDTH/2 - 220, SCREEN_HEIGHT/2 - 50, 80, 80 };
//         Rectangle rG = { SCREEN_WIDTH/2 - 120, SCREEN_HEIGHT/2 - 50, 80, 80 };
//         Rectangle rB = { SCREEN_WIDTH/2 - 20,  SCREEN_HEIGHT/2 - 50, 80, 80 };
//         Rectangle rY = { SCREEN_WIDTH/2 + 80,  SCREEN_HEIGHT/2 - 50, 80, 80 };

//         DrawRectangleRounded(rR, 0.3f, 8, softRed);
//         DrawRectangleRounded(rG, 0.3f, 8, softGreen);
//         DrawRectangleRounded(rB, 0.3f, 8, softBlue);
//         DrawRectangleRounded(rY, 0.3f, 8, softYellow);

//         DrawRectangleLinesEx(rR, 3, WHITE);
//         DrawRectangleLinesEx(rG, 3, WHITE);
//         DrawRectangleLinesEx(rB, 3, WHITE);
//         DrawRectangleLinesEx(rY, 3, WHITE);
//     }
// }

