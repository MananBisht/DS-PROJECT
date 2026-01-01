// #endif
#ifndef SHARED_PROTOCOL_H
#define SHARED_PROTOCOL_H

// General buffer size for network messages
#define BUFFER_SIZE 512
    // server tells: you must call UNO within N ms
   // UNO accepted (on time)
 
#define C2S_PLAY_WILD "PLAY_WILD:" 
 
#define S2C_COLOR_REQ "COLOR_REQ:"     // Server -> Client: ask to pick color
#define C2S_COLOR_SELECT "COLOR_SELECT:"
// --- Server-to-Client Protocol Prefixes (S2C) ---
// Assigns the player ID: S2C_ID_ASSIGN<ID> (e.g., "ID:0")
#define S2C_ID_ASSIGN "ID:" 

// Sends the full game state: S2C_STATE<Data> (e.g., "STATE:TOP_R1:TURN_1:DIR_1:P0_7:P1_8:...")
#define S2C_STATE "STATE:" 

#define S2C_INIT "INIT:" 
// Sends the player's current hand: S2C_HAND<Cards> (e.g., "HAND:R1,G5,B_S,WILD")
#define S2C_HAND "HAND:"

// Sends a simple informational message: S2C_MSG<Message>
#define S2C_MSG "MSG:"

#define S2C_WIN "WIN:" 
// Sends turn signal: S2C_TURN
#define S2C_TURN "TURN"

// --- Client-to-Server Protocol Prefixes (C2S) ---
// Player plays a card: C2S_PLAY<Card> (e.g., "PLAY:R1")
#define C2S_PLAY "PLAY:"

// Player draws a card: C2S_DRAW
#define C2S_DRAW "DRAW"

// Player calls UNO: C2S_UNO


#define C2S_PASS "PASS"
// Player challenges Wild Draw 4: C2S_CHALLENGE
#define C2S_CHALLENGE "CHAL"
#define C2S_PLAY_AGAIN   "C2S_PLAY_AGAIN"
#define S2C_RESET_GAME   "S2C_RESET_GAME"
#define C2S_GET_HAND "GET_HAND:"
#define C2S_UNO       "UNO:"       // client -> server
#define S2C_UNO_START "UNO_START:" // server -> client followed by milliseconds (e.g. "UNO_START:3000")
#define S2C_UNO_OK    "UNO_OK:"    // server -> client followed by player id
#define S2C_UNO_LATE  "UNO_LATE:"
#endif