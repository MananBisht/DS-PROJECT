# 🎴UNO Multiplayer Game

> A real time multiplayer UNO card game built in C, implementing a client-server architecture using **TCP sockets (Winsock)**. Designed to demonstrate low-level networking multithreading, and game state synchronization.
  
<br>

## ▫️Gallery
> ### **Demo** <br>
![UNO Gameplay Demo](assets/gif/game.gif)

> ### **Terminal** <br>
![UNO Server side](assets/screenshots/server.png)

> ### **Application** <br>
![UNO Server side](assets/screenshots/client.png)   

<br>

## ▫️Features

### Core Gameplay
- Standard UNO rules and functionality<br>
<i><a target="_blank" href="https://gathertogethergames.com/uno">How to play UNO</a></i>

### Networking
- TCP for client to server communication
- Multithreaded server (one thread per client)
- Custom packet serialization / deserialization
- Centralized rule validation

### System Design
- Centralized game state
- Deterministic turn handling
- Timeout based penalties
- Robust synchronization logic

<br>

  
## ▫️Running and Compiling  
For client :  
```
cd build
gcc client.c helpers_ui.c player_hand.c -o client.exe -Iinclude -Llib -lraylib -lwinmm -lgdi32 -lws2_32
``` 
  
For server :
```
cd build
gcc server.c game2.c deck2.c player.c player_hand.c -o server.exe -lws2_32
``` 