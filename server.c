#pragma comment(lib, "ws2_32.lib") // Link against ws2_32.lib

#include <WinSock2.h>
#include <WS2tcpip.h>
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "game2.h"
#include "serializer.h"
#include "shared_protocol.h"
#include <stdbool.h>

// --- Server Configuration ---
#define DEFAULT_PORT 8888
#define MAX_CLIENTS MAX_PLAYERS

#define UNO_TIMEOUT_MS 3000

static BOOL      need_uno[MAX_PLAYERS]      = {0};
static ULONGLONG uno_deadline_ms[MAX_PLAYERS] = {0};

static inline ULONGLONG now_ms(void) { return GetTickCount64(); }

bool wants_restart[MAX_PLAYERS] = {0};

// --- Global State ---
Game main_game;
int client_count = 0;
SOCKET client_sockets[MAX_CLIENTS] = { 0 };
char player_names[MAX_PLAYERS][32];

CRITICAL_SECTION game_lock;
HANDLE client_threads[MAX_CLIENTS];

static Card pending_wild[MAX_PLAYERS];
static bool waiting_for_color[MAX_PLAYERS];


// --- Function Declarations ---
DWORD WINAPI handle_client(LPVOID lpParam);
void broadcast_message(const char *msg, int exclude_id);
void send_to_player(int player_id, const char *msg);
void broadcast_gamestate();
void send_player_hand(int player_id);

static void apply_uno_penalty(int pid) {
    // Draw +2 for the player who failed to call UNO in time
    Player *p = &main_game.players[pid];
    add_card(&p->hand, draw_card(&main_game.draw_pile));
    add_card(&p->hand, draw_card(&main_game.draw_pile));
    need_uno[pid] = 0;

    send_player_hand(pid);

    char msg[128];
    sprintf(msg, S2C_MSG"Player %d failed to call UNO in time! +2 penalty.", pid);
    broadcast_message(msg, -1);
}

int main(int argc, char *argv[]) {
    WSADATA wsaData;
    SOCKET ListenSocket = INVALID_SOCKET;
    SOCKET ClientSocket = INVALID_SOCKET;
    struct sockaddr_in server_addr;

    printf("--- UNO Game Server ---\n");

    int num_players = 2; // default
    int port = DEFAULT_PORT;
    char ip[32] = "0.0.0.0"; // bind to all interfaces

    // --- Read arguments from launcher ---
    // Expected: server.exe <ip> <port> <players>
    if (argc == 4) {
        strcpy(ip, argv[1]);
        port = atoi(argv[2]);
        num_players = atoi(argv[3]);
        printf("[Launcher Mode] IP=%s PORT=%d PLAYERS=%d\n", ip, port, num_players);
    } else {
        // Fallback: manual input mode
        do {
            printf("Enter number of players (2-%d): ", MAX_PLAYERS);
            if (scanf("%d", &num_players) != 1) {
                while (getchar() != '\n');
                num_players = 0;
            }
            while (getchar() != '\n');
        } while (num_players < 2 || num_players > MAX_PLAYERS);

        printf("Enter names for %d players:\n", num_players);
        for (int i = 0; i < num_players; i++) {
            printf("Player %d name: ", i);
            if (fgets(player_names[i], sizeof(player_names[i]), stdin)) {
                size_t len = strlen(player_names[i]);
                if (len > 0 && player_names[i][len - 1] == '\n')
                    player_names[i][len - 1] = '\0';
            }
        }
    }

    // Auto-assign default names if launched from GUI
    if (argc == 4) {
        for (int i = 0; i < num_players; i++) {
            sprintf(player_names[i], "Player_%d", i + 1);
        }
    }

    // --- Winsock Setup ---
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("WSAStartup failed\n");
        return 1;
    }

    ListenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (ListenSocket == INVALID_SOCKET) {
        printf("socket failed\n");
        WSACleanup();
        return 1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = inet_addr(ip);


    // --- Winsock Setup ---
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("WSAStartup failed\n");
        return 1;
    }

    ListenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (ListenSocket == INVALID_SOCKET) {
        printf("socket failed\n");
        WSACleanup();
        return 1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(DEFAULT_PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(ListenSocket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        printf("bind failed\n");
        closesocket(ListenSocket);
        WSACleanup();
        return 1;
    }

    if (listen(ListenSocket, SOMAXCONN) == SOCKET_ERROR) {
        printf("listen failed\n");
        closesocket(ListenSocket);
        WSACleanup();
        return 1;
    }

    printf("Listening on port %d...\n", DEFAULT_PORT);

    // --- Game Init ---
    InitializeCriticalSection(&game_lock);
    init_game(&main_game, num_players, player_names);
    for (int i = 0; i < MAX_PLAYERS; ++i) {
    waiting_for_color[i] = false;
    // optional: pending_wild[i].type = Number; pending_wild[i].color = COLOR_NONE;
}


    // --- Accept Clients ---
    int player_id = 0;
    while (player_id < num_players) {
        printf("Waiting for player %d...\n", player_id);
        ClientSocket = accept(ListenSocket, NULL, NULL);
        if (ClientSocket == INVALID_SOCKET) {
            printf("accept failed\n");
            continue;
        }

        EnterCriticalSection(&game_lock);
        client_sockets[player_id] = ClientSocket;
        client_count++;
        LeaveCriticalSection(&game_lock);

        printf("Player %d (%s) connected.\n", player_id, player_names[player_id]);

        char welcome[BUFFER_SIZE];
        sprintf(welcome, S2C_ID_ASSIGN"%d", player_id);
        send_to_player(player_id, welcome);

        client_threads[player_id] = CreateThread(NULL, 0, handle_client, (LPVOID)(INT_PTR)player_id, 0, NULL);
        player_id++;
    }

    printf("All players connected. Starting game...\n");
    broadcast_gamestate();

    // Wait for all threads
    WaitForMultipleObjects(num_players, client_threads, TRUE, INFINITE);

    // Cleanup
    DeleteCriticalSection(&game_lock);
    for (int i = 0; i < num_players; i++) {
        closesocket(client_sockets[i]);
        CloseHandle(client_threads[i]);
    }
    closesocket(ListenSocket);
    WSACleanup();
    printf("Server closed.\n");
    return 0;
}
static void check_uno_timeouts(void) {
    ULONGLONG t = now_ms();
    for (int i = 0; i < main_game.num_players; i++) {
        if (need_uno[i] && t > uno_deadline_ms[i]) {
            apply_uno_penalty(i);
            broadcast_gamestate();
        }
    }
}

DWORD WINAPI uno_timer_thread(LPVOID lpParam) {
    int pid = (int)(INT_PTR)lpParam;
    Sleep(UNO_TIMEOUT_MS); // wait 3 seconds

    EnterCriticalSection(&game_lock);
    if (need_uno[pid] && main_game.players[pid].hand.size == 1) {
    printf("UNO timer expired for Player %d!\n", pid);
    apply_uno_penalty(pid);
    broadcast_gamestate();
} else {
    need_uno[pid] = 0;
}

    LeaveCriticalSection(&game_lock);
    return 0;
}


// ======================================================================
//                           HANDLE CLIENT
// ======================================================================
DWORD WINAPI handle_client(LPVOID lpParam) {
    int player_id = (int)(INT_PTR)lpParam;
    SOCKET sock = client_sockets[player_id];
    char recvbuf[BUFFER_SIZE];
    int result;

    printf("Thread started for Player %d\n", player_id);

    EnterCriticalSection(&game_lock);
    send_player_hand(player_id);
    LeaveCriticalSection(&game_lock);

    while ((result = recv(sock, recvbuf, BUFFER_SIZE - 1, 0)) > 0) {
        recvbuf[result] = '\0';
        printf("From Player %d: %s\n", player_id, recvbuf);

        // --- PLAY: ---
        if (strncmp(recvbuf, C2S_PLAY, strlen(C2S_PLAY)) == 0) {
            EnterCriticalSection(&game_lock);

            if (player_id != main_game.current_turn) {
                send_to_player(player_id, S2C_MSG"Not your turn");
                LeaveCriticalSection(&game_lock);
                continue;
            }

            const char *payload = recvbuf + strlen(C2S_PLAY);
            Card c = deserialize_card(payload);

            // Trim trailing newline if any (safer handling)
            // Note: payload is const char* from recvbuf; modify recvbuf directly instead:
            for (int i = 0; recvbuf[i]; i++) {
                if (recvbuf[i] == '\r' || recvbuf[i] == '\n') { recvbuf[i] = '\0'; break; }
            }

            if (c.type == Wild || c.type == Wild_draw_4) {
    // Store pending; do NOT remove from hand yet
                pending_wild[player_id] = c;
                waiting_for_color[player_id] = true;

                send_to_player(player_id, S2C_COLOR_REQ);
                printf("SERVER: Waiting for Player %d to choose color for %s\n",
                player_id, (c.type == Wild_draw_4) ? "Wild+4" : "Wild");

                LeaveCriticalSection(&game_lock);
                continue; // wait for COLOR_SELECT
            }
            int play_result = play_card(&main_game, player_id, c, c.color);
            if (play_result == 0) {
                send_to_player(player_id, S2C_MSG"Invalid play!");
            }else if (play_result == 1) {
                send_player_hand(player_id);
                // ✅ UNO timer logic
                
                broadcast_gamestate();
                char turn_msg[64];
                sprintf(turn_msg, S2C_TURN"%d", main_game.current_turn);
                broadcast_message(turn_msg, -1);

                printf("[DEBUG] After play: Player %d has %d cards. Setting need_uno[%d] = 1\n",
       player_id, main_game.players[player_id].hand.size, player_id);


                if (main_game.players[player_id].hand.size == 1) {
    need_uno[player_id] = 1;

    char start[64];
    sprintf(start, S2C_UNO_START"%u", (unsigned)UNO_TIMEOUT_MS);
    send_to_player(player_id, start);

    printf("Starting UNO timer for Player %d (%u ms)\n", player_id, UNO_TIMEOUT_MS);

    // 🧠 Start a detached thread that will wait and enforce penalty if needed
    CreateThread(NULL, 0, uno_timer_thread, (LPVOID)(INT_PTR)player_id, 0, NULL);
}
            }
            else if (play_result == 2) {
                send_player_hand(player_id);
                broadcast_gamestate();
                char win_msg[64];
                sprintf(win_msg, S2C_WIN"%d", player_id);
                broadcast_message(win_msg, -1);
                send_to_player(player_id, win_msg);
            }

            LeaveCriticalSection(&game_lock);
        }

        // --- COLOR SELECT: ---
        else if (strncmp(recvbuf, C2S_COLOR_SELECT, strlen(C2S_COLOR_SELECT)) == 0) {
    EnterCriticalSection(&game_lock);

    // Ignore stray color selects
    if (!waiting_for_color[player_id]) {
        // Silently ignore (or send a soft message)
        LeaveCriticalSection(&game_lock);
        continue;
    }

    char color_char = recvbuf[strlen(C2S_COLOR_SELECT)];
    card_color chosen = COLOR_NONE;
    switch (color_char) {
        case 'R': chosen = COLOR_RED; break;
        case 'G': chosen = COLOR_GREEN; break;
        case 'B': chosen = COLOR_BLUE; break;
        case 'Y': chosen = COLOR_YELLOW; break;
        default:  chosen = COLOR_NONE; break;
    }
    if (chosen == COLOR_NONE) {
        send_to_player(player_id, S2C_MSG"Invalid color selection");
        LeaveCriticalSection(&game_lock);
        continue;
    }

    // Resolve the pending wild now
    waiting_for_color[player_id] = false;

    Player *p = &main_game.players[player_id];
    Card wild = pending_wild[player_id];
    // Clear the slot for safety
    pending_wild[player_id].type = Number;
    pending_wild[player_id].color = COLOR_NONE;

    // Remove the wild from player's hand now (it was not removed earlier)
    remove_card(&p->hand, wild);

    // Put the colored wild on discard pile
    wild.color = chosen;
    main_game.discard_pile.top++;
    main_game.discard_pile.cards[main_game.discard_pile.top] = wild;
    main_game.discard_pile.size++;

    // Apply its effect; know how many advances apply_card_effect did
    int advanced = apply_card_effect(&main_game, wild);

    // Winner?
    if (check_winner(&main_game, player_id)) {
        send_player_hand(player_id);
        broadcast_gamestate();
        char win_msg[64];
        sprintf(win_msg, S2C_WIN"%d", player_id);
        broadcast_message(win_msg, -1);
        LeaveCriticalSection(&game_lock);
        continue;
    }

    // If the effect didn't advance the turn, advance once now
    if (advanced == 0) {
        next_turn(&main_game);
    }

    // Broadcast hand/state/turn
    send_player_hand(player_id);
    broadcast_gamestate();

    char turn_msg[64];
    sprintf(turn_msg, S2C_TURN"%d", main_game.current_turn);
    broadcast_message(turn_msg, -1);

    printf("SERVER: Wild resolved to %c, next turn -> Player %d\n",
           color_char, main_game.current_turn);

    LeaveCriticalSection(&game_lock);
}

        // --- DRAW: ---
        else if (strncmp(recvbuf, C2S_DRAW, strlen(C2S_DRAW)) == 0) {
    EnterCriticalSection(&game_lock);

    if (player_id != main_game.current_turn) {
        send_to_player(player_id, S2C_MSG"Not your turn");
        LeaveCriticalSection(&game_lock);
        continue;
    }

    // Player draws one card
    player_draw(&main_game, player_id);
    send_player_hand(player_id); // show hand including the drawn card

    // Find the last card in player's hand (the drawn card)
    Player *p = &main_game.players[player_id];
    hand_node *itr = p->hand.head;
    while (itr && itr->next) itr = itr->next; // last node
    Card drawn = itr->card;

    Card top = main_game.discard_pile.cards[main_game.discard_pile.top];

    if (can_play(drawn, top)) {
    if (drawn.type == Wild || drawn.type == Wild_draw_4) {
        pending_wild[player_id] = drawn;
        waiting_for_color[player_id] = true;
        send_to_player(player_id, S2C_COLOR_REQ);
        send_to_player(player_id, S2C_MSG"Drawn wild playable! Choose color.");
    } else {
        main_game.current_turn = player_id;
        char turn_msg[64];
        send_to_player(player_id, S2C_MSG"Drawn card playable! You may play it.");
        broadcast_message(turn_msg, -1);
    }


    } else {
        // Not playable: advance turn normally
        next_turn(&main_game);
        broadcast_gamestate();

        char turn_msg[64];
        sprintf(turn_msg, S2C_TURN"%d", main_game.current_turn);
        broadcast_message(turn_msg, -1);
    }

    LeaveCriticalSection(&game_lock);
}

        else if (strncmp(recvbuf, C2S_PASS, strlen(C2S_PASS)) == 0) {
    EnterCriticalSection(&game_lock);

    if (player_id != main_game.current_turn) {
        send_to_player(player_id, S2C_MSG"Not your turn.");
        LeaveCriticalSection(&game_lock);
        continue;
    }

    // Advance turn to next player
    next_turn(&main_game);
    broadcast_gamestate();

    char msg[BUFFER_SIZE];
    sprintf(msg, S2C_MSG"Player %d passed their turn.", player_id);
    broadcast_message(msg, -1);

    char turn_msg[64];
    sprintf(turn_msg, S2C_TURN"%d", main_game.current_turn);
    broadcast_message(turn_msg, -1);

    LeaveCriticalSection(&game_lock);
}

        // --- UNO: ---
else if (strncmp(recvbuf, C2S_UNO, strlen(C2S_UNO)) == 0) {
    EnterCriticalSection(&game_lock);
    printf("[DEBUG] UNO command received from Player %d. need_uno=%d\n",
           player_id, need_uno[player_id]);

    // Be lenient: allow UNO call if player has exactly 1 card
    bool allow_uno = need_uno[player_id] ||
                     (main_game.players[player_id].hand.size == 1);

    if (allow_uno) {
        need_uno[player_id] = 0; // cancel any pending timer
        char okmsg[64];
        sprintf(okmsg, S2C_UNO_OK"%d", player_id);
        send_to_player(player_id, okmsg);

        char notify[128];
        sprintf(notify, S2C_MSG"Player %d successfully called UNO!", player_id);
        broadcast_message(notify, -1);
    } else {
        send_to_player(player_id, S2C_MSG"UNO not required right now.");
    }

    LeaveCriticalSection(&game_lock);
}
// --- PLAY AGAIN ---
else if (strncmp(recvbuf, C2S_PLAY_AGAIN, strlen(C2S_PLAY_AGAIN)) == 0) {
    EnterCriticalSection(&game_lock);

    printf("Player %d requested PLAY AGAIN\n", player_id);
    wants_restart[player_id] = true;

    // Check if ALL players want restart
    bool all_ready = true;
    for (int i = 0; i < main_game.num_players; i++) {
        if (!wants_restart[i]) {
            all_ready = false;
            break;
        }
    }

    if (all_ready) {
        printf("All players ready. Restarting game...\n");

        // Reset flags
        for (int i = 0; i < main_game.num_players; i++) {
            wants_restart[i] = false;
            need_uno[i] = 0;
            waiting_for_color[i] = false;
        }

        // Reinitialize game
        init_game(&main_game, main_game.num_players, player_names);

        // Notify clients
        broadcast_message(S2C_RESET_GAME, -1);
        broadcast_gamestate();
    }

    LeaveCriticalSection(&game_lock);
}


        // --- GET HAND: ---
        else if (strncmp(recvbuf, C2S_GET_HAND, strlen(C2S_GET_HAND)) == 0) {
            EnterCriticalSection(&game_lock);
            send_player_hand(player_id);
            LeaveCriticalSection(&game_lock);
        }

        // --- UNKNOWN: ---
        else {
            send_to_player(player_id, S2C_MSG"Unknown command");
        }
        
    }
    

    printf("Player %d disconnected.\n", player_id);
    need_uno[player_id] = 0;


    EnterCriticalSection(&game_lock);
    closesocket(sock);
    client_sockets[player_id] = INVALID_SOCKET;
    LeaveCriticalSection(&game_lock);

    return 0;
}

void send_to_player(int player_id, const char *msg) {
    if (client_sockets[player_id] != INVALID_SOCKET) {
        send(client_sockets[player_id], msg, strlen(msg), 0);
    }
}

void broadcast_message(const char *msg, int exclude_id) {
    for (int i = 0; i < main_game.num_players; i++) {
        if (client_sockets[i] != INVALID_SOCKET && i != exclude_id)
            send_to_player(i, msg);
    }
}

void broadcast_gamestate() {
    char buffer[BUFFER_SIZE] = "";
    char tmp[64], cardstr[16];

    Card top = main_game.discard_pile.cards[main_game.discard_pile.top];
    strcpy(cardstr, serialize_card(top));

    sprintf(tmp, S2C_STATE"TOP_%s:", cardstr);
    strcat(buffer, tmp);

    sprintf(tmp, "TURN_%d:", main_game.current_turn);
    strcat(buffer, tmp);

    sprintf(tmp, "DIR_%d:", main_game.direction);
    strcat(buffer, tmp);

    // NEW: send total players
    sprintf(tmp, "N_%d:", main_game.num_players);
    strcat(buffer, tmp);

    for (int i = 0; i < main_game.num_players; i++) {
        sprintf(tmp, "P%d_%d:", i, main_game.players[i].hand.size);
        strcat(buffer, tmp);
    }

    broadcast_message(buffer, -1);
}

void send_player_hand(int player_id) {
    Player *p = &main_game.players[player_id];
    hand_node *itr = p->hand.head;
    char msg[BUFFER_SIZE] = S2C_HAND;
    char card_str[16];

    while (itr) {
        strcpy(card_str, serialize_card(itr->card));

        strcat(msg, card_str);
        if (itr->next) strcat(msg, ",");
        itr = itr->next;
    }

    send_to_player(player_id, msg);
}
