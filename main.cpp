
#ifdef _WIN32
    #define Rectangle Win32Rectangle
    #define CloseWindow Win32CloseWindow
    #define ShowCursor Win32ShowCursor
    
    #include <enet/enet.h>
    
    #undef Rectangle
    #undef CloseWindow
    #undef ShowCursor
    
    #undef DrawText
    #undef DrawTextEx
    #undef PlaySound
    #undef LoadImage

#else

    #include <enet/enet.h>

#endif


#include "raylib.h"
#include <iostream>
#include <deque>
#include <string>
#include <cstring> 
#include <vector> 

#define MENU_BUTTON_WIDTH 280 
#define MENU_BUTTON_HEIGHT 100

const int GRID_WIDTH = 38;
const int GRID_HEIGHT = 20;
const int CELL_SIZE = 40;
const int OFFSET_X = 200; 
const int OFFSET_Y = 200;

enum PacketType {

    PACKET_CREATE_ROOM,
    PACKET_ROOM_CREATED,
    PACKET_JOIN_ROOM,
    PACKET_JOIN_SUCCESS,
    PACKET_ROOM_NOT_FOUND,
    PACKET_MOVE,
    PACKET_SCORE,       
    PACKET_GAME_OVER    
};

struct NetworkPacket {

    PacketType type;
    char roomCode[10];
    int x;
    int y;
    int score;
    bool gameOver;
}; 

struct Fruit {

    Vector2 pos;
    float timer;
    int type;
};

void SpawnFruit(Fruit& fruit, const std::deque<Vector2>& snakeBody) {

    bool validPos = false;

    while (!validPos) {

        fruit.pos.x = (float)GetRandomValue(0, GRID_WIDTH - 1);
        fruit.pos.y = (float)GetRandomValue(0, GRID_HEIGHT - 1);
        
        validPos = true;

        for (const auto& segment : snakeBody) {

            if (segment.x == fruit.pos.x && segment.y == fruit.pos.y) {

                validPos = false;
                break;
            }
        }
    }

    fruit.timer = 10.0f; 
    fruit.type = GetRandomValue(0,6);
}

struct Bomb {

    Vector2 pos;

    float timer;
    float explosionTimer;
    float spawnCooldown;

    bool active;
    bool exploding;
};

void SpawnBomb(Bomb& bomb, const std::deque<Vector2>& snake1, const std::deque<Vector2>& snake2 = {}) {

    bool validPos = false;

    while(!validPos) {

        bomb.pos.x = (float)GetRandomValue(1, GRID_WIDTH - 2);
        bomb.pos.y = (float)GetRandomValue(1, GRID_HEIGHT -2);

        validPos = true;

        for(const auto& segment : snake1) {

            if(segment.x == bomb.pos.x && segment.y == bomb.pos.y) validPos = false;
        }

        for (const auto& segment : snake2) {
            if (segment.x == bomb.pos.x && segment.y == bomb.pos.y) validPos = false;
        }
    }

    bomb.timer = 5.0f;
    bomb.active = true;
    bomb.exploding = false;
    bomb.explosionTimer = 0.0f;
}

Color GetColorFromFruitType(int type) {

    switch(type) {

        case 0: return RED;
        case 1: return ORANGE;
        case 2: return YELLOW;
        case 3: return GOLD;
        case 4: return GREEN;
        case 5: return PURPLE;
        case 6: return MAGENTA;
        default: return BLUE;
    }
}

struct Particle {

    Vector2 position;
    Vector2 velocity;
    Color color;
    float lifeTime;
    float maxlifeTime;
    float size;
};

std::vector<Particle> activeParticles;

void SpawnFruitParticles(Vector2 fruitGridPos, Color fruitColor) {

    int numParticles = GetRandomValue(15, 25);

    float pixelX = OFFSET_X + fruitGridPos.x * CELL_SIZE + CELL_SIZE / 2.0f;
    float pixelY = OFFSET_Y + fruitGridPos.y * CELL_SIZE + CELL_SIZE / 2.0f;

    for (int i = 0; i < numParticles; i++) {

        Particle p;

        p.position = {pixelX, pixelY};
        p.velocity = { (float)GetRandomValue(-250, 250) / 10.0f, (float)GetRandomValue(-60, 60) / 10.0f };
        p.color = fruitColor;
        p.maxlifeTime = (float)GetRandomValue(50, 100) / 100.0f; 
        p.lifeTime = p.maxlifeTime;
        p.size = (float)GetRandomValue(5, 10); 
        activeParticles.push_back(p);
    }
}

void DrawSnake(const std::deque<Vector2>& snake, Color color) {

    for (size_t i = 0; i < snake.size(); i++) {

        Rectangle segmentRec = {

            (float)(OFFSET_X + snake[i].x * CELL_SIZE), 
            (float)(OFFSET_Y + snake[i].y * CELL_SIZE), 
            (float)CELL_SIZE, (float)CELL_SIZE
        };

        DrawRectangleRounded( {segmentRec.x + 4, segmentRec.y + 4, segmentRec.width, segmentRec.height}, 0.5f, 4, Fade(BLACK, 0.3f));
        DrawRectangleRounded(segmentRec, 0.5f, 4, color);

        if (i==0) {

            DrawCircle((int)segmentRec.x + 12, (int)segmentRec.y + 12, 4, WHITE);
            DrawCircle((int)segmentRec.x + 12, (int)segmentRec.y + 12, 2, BLACK);
            DrawCircle((int)segmentRec.x + 12, (int)segmentRec.y + CELL_SIZE - 12, 4, WHITE);
            DrawCircle((int)segmentRec.x + 12, (int)segmentRec.y + CELL_SIZE - 12, 2, BLACK);
        }
    }
}

int main() {

    const int screenWidth = 1920;
    const int screenHeight = 1080;

    InitWindow(screenWidth, screenHeight, "Snake Game - Network Multiplayer");

    InitAudioDevice();

    Music bgMusic = LoadMusicStream("./images/musicSnake.mp3");
    Music winMusic = LoadMusicStream("./images/castig.mp3");
    Music lostMusic = LoadMusicStream("./images/pierd.mp3");

    PlayMusicStream(bgMusic);
    SetMusicVolume(bgMusic, 0.5f);
    SetMusicVolume(winMusic, 0.5f);
    SetMusicVolume(lostMusic, 0.5f);

    Color snakeColor = BLUE;

    bool isRainbow = false;

    Texture2D menuBg = LoadTexture("./images/menu.png"); 

    Image imgTemp = LoadImage("./images/image1.png");
    ImageResize(&imgTemp, screenWidth, screenHeight);
    Texture2D singleplayerBg = LoadTextureFromImage(imgTemp);
    UnloadImage(imgTemp);

    Image imgLost = LoadImage("./images/lost.png");
    ImageResize(&imgLost, screenWidth, screenHeight);
    Texture2D lostBg = LoadTextureFromImage(imgLost);
    UnloadImage(imgLost);

    Image imgWon = LoadImage("./images/won.png");
    ImageResize(&imgWon, screenWidth, screenHeight);
    Texture2D wonBg = LoadTextureFromImage(imgWon);
    UnloadImage(imgWon);

    Image imgSetup = LoadImage("./images/verde.jpg");
    ImageResize(&imgSetup, screenWidth, screenHeight);
    Texture2D setupBg = LoadTextureFromImage(imgSetup);
    UnloadImage(imgSetup);

    Rectangle btnSingleplayer = { 564, 806, 280, 100 };
    Rectangle btnMultiplayer = { 1100, 806, 280, 100 };
    Rectangle btnPlayAgain = {500, 590, 405, 115};
    Rectangle btnBackToMenu = {1030, 590, 405, 115};
    Rectangle btnExit = { (1920 / 2) - 75, 1020, 150, 50};

    SetTargetFPS(10); 

    enum GameScreen { MENU, SINGLEPLAYER, MULTIPLAYER, MULTIPLAYER_SETUP };
    GameScreen currentScreen = MENU;

    if (enet_initialize() != 0) {

        std::cerr << "Failed to initialize ENet" << std::endl;
    }

    ENetHost* host = nullptr;
    ENetPeer* peer = nullptr;

    bool isServer = false;
    bool isConnected = false;


    std::deque<Vector2> snake = {{19, 10}, {18, 10}, {17, 10}};

    Vector2 snakeDir = {1, 0};
    Vector2 nextDir = {1, 0};

    float moveUpdateTimer = 0.0f;
    float moveSpeed = 0.12f; 
    float ghostTimer = 0.0f;

    std::deque<Vector2> localSnake = {{5, 10}, {4, 10}, {3, 10}};
    std::deque<Vector2> remoteSnake = {{33, 10}, {34, 10}, {35, 10}};

    Vector2 localSnakeDir = {1, 0};
    Vector2 nextDirLocal = {1, 0};

    int localScore = 0;
    int remoteScore = 0;
    bool gameOverMulti = false;
    int winnerMultiplayer = 0;
    float rainbowTimerMulti = 0.0f;

    Fruit fruit;

    int score = 0;
    bool gameOver = false;
    bool gameWon = false;

    SpawnFruit(fruit, snake);

    Bomb bomb;
    bomb.active = false;
    bomb.exploding = false;
    bomb.spawnCooldown = 10.0f;

    char roomCode[10] = "\0";
    int cursorPosCode = 0;
    bool inLobby = false;

    std::string errorMessage = "";
    float errorTimer = 0.0f;

    while (!WindowShouldClose()) {
        

        UpdateMusicStream(bgMusic);
        UpdateMusicStream(winMusic);
        UpdateMusicStream(lostMusic);

        bool needToCleanHost = false;

        if (host != nullptr) {

            ENetEvent event;

            while (enet_host_service(host, &event, 0) > 0) {

                switch (event.type) {
                    case ENET_EVENT_TYPE_CONNECT:
                        if (isServer) {

                            std::cout << "Client connected!" << std::endl;
                        }
                        break;

                    case ENET_EVENT_TYPE_RECEIVE: {
                        if (event.packet->dataLength < sizeof(NetworkPacket)) {

                            enet_packet_destroy(event.packet);
                            break;
                        }

                        NetworkPacket* packet = (NetworkPacket*)event.packet->data;
                        
                        if (packet->type == PACKET_ROOM_CREATED) {

                            strncpy(roomCode, packet->roomCode, 9);

                            roomCode[9] = '\0';
                            inLobby = true;
                            errorMessage = "";
                        } 
                        else if (packet->type == PACKET_JOIN_SUCCESS) {

                            currentScreen = MULTIPLAYER;
                            errorMessage = "";
                        }
                        else if (packet->type == PACKET_ROOM_NOT_FOUND) {

                            errorMessage = "PAROLA INCORECTA!";
                            errorTimer = 3.0f;
                            needToCleanHost = true;
                        }

                        else if (packet->type == PACKET_MOVE) {

                            if (packet->x >= 0 && packet->x < GRID_WIDTH && packet->y >= 0 && packet->y < GRID_HEIGHT) {

                                remoteSnake.push_front({(float)packet->x, (float)packet->y});
                                if (remoteSnake.size() > 1) {

                                    remoteSnake.pop_back();
                                }
                            }
                        } 
                        else if (packet->type == PACKET_SCORE) {

                            remoteScore = packet->score;

                            if (remoteScore >= 30) {

                                winnerMultiplayer = 2;
                            }
                        }

                        else if (packet->type == PACKET_GAME_OVER) {
                            
                            winnerMultiplayer = 1; 
                        }
                        
                        enet_packet_destroy(event.packet);
                        break;
                    }

                    case ENET_EVENT_TYPE_DISCONNECT:
                        std::cout << "Client disconnected!" << std::endl;
                        peer = nullptr;
                        currentScreen = MENU;
                        winnerMultiplayer = 0;
                        needToCleanHost = true;
                        break;

                    default:
                        break;
                }
            }
        }

        if (needToCleanHost && host != nullptr) {

            enet_host_destroy(host);
            host = nullptr;
            peer = nullptr;
        }

        if (errorTimer > 0.0f) {

            errorTimer -= GetFrameTime();
            if (errorTimer <= 0.0f) {

                errorMessage = "";
            }
        }

        Vector2 mousePoint = GetMousePosition();

        if (currentScreen == MENU) {

            bool hoverSingle = CheckCollisionPointRec(mousePoint, btnSingleplayer);
            bool hoverMulti = CheckCollisionPointRec(mousePoint, btnMultiplayer);

            if (hoverSingle && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {

                currentScreen = SINGLEPLAYER;
            }
            if (hoverMulti && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {

                currentScreen = MULTIPLAYER_SETUP;
            }
        }
        else if (currentScreen == MULTIPLAYER_SETUP) {

            int charPressed = GetCharPressed();

            while(charPressed != 0) {

                if ((charPressed >= 48 && charPressed <= 57) || 
                    (charPressed >= 65 && charPressed <= 90) || 
                    (charPressed >= 97 && charPressed <= 122)) {
                    
                    if (charPressed >= 97 && charPressed <= 122) {

                        charPressed -= 32;
                    }

                    if (charPressed == 'C' && cursorPosCode == 0) {
                    } else if (cursorPosCode < 4) {

                        roomCode[cursorPosCode] = (char)charPressed;
                        roomCode[cursorPosCode+1] = '\0';
                        cursorPosCode++;
                    }
                }

                charPressed = GetCharPressed();
            }

            if (IsKeyPressed(KEY_BACKSPACE) && cursorPosCode > 0) {

                roomCode[--cursorPosCode]='\0';
            }

            if (IsKeyPressed(KEY_C) && host == nullptr && cursorPosCode == 0) {

                host = enet_host_create(nullptr, 1, 2, 0, 0);
                
                if (host != nullptr) {

                    ENetAddress address; 
                    enet_address_set_host(&address, "192.168.1.129");
                    address.port = 7777;
                    peer = enet_host_connect(host, &address, 2, 0);

                    if (peer != nullptr) {

                        isServer = true;

                        ENetEvent event;

                        if (enet_host_service(host, &event, 1000) > 0 && event.type == ENET_EVENT_TYPE_CONNECT) {

                            NetworkPacket createPacket;
                            createPacket.type = PACKET_CREATE_ROOM;
                            ENetPacket* ePacket = enet_packet_create(&createPacket, sizeof(NetworkPacket), ENET_PACKET_FLAG_RELIABLE);

                            enet_peer_send(peer, 0, ePacket);
                            enet_host_flush(host);
                        } else {

                            enet_host_destroy(host);
                            host = nullptr;
                        }
                    } else {

                        enet_host_destroy(host);
                        host = nullptr;
                    }
                }
            }

            if (IsKeyPressed(KEY_ENTER) && cursorPosCode > 0 && host == nullptr) {

                host = enet_host_create(nullptr, 1, 2, 0, 0);
                
                if (host != nullptr) {

                    ENetAddress address;
                    enet_address_set_host(&address, "192.168.1.129");
                    address.port = 7777;
                    peer = enet_host_connect(host, &address, 2, 0);

                    if (peer != nullptr) {

                        isServer = false;

                        ENetEvent event;

                        if (enet_host_service(host, &event, 1000) > 0 && event.type == ENET_EVENT_TYPE_CONNECT) {

                            NetworkPacket joinPacket;
                            joinPacket.type = PACKET_JOIN_ROOM;
                            strncpy(joinPacket.roomCode, roomCode, 9);
                            joinPacket.roomCode[9] = '\0';
                            
                            ENetPacket* ePacket = enet_packet_create(&joinPacket, sizeof(NetworkPacket), ENET_PACKET_FLAG_RELIABLE);

                            enet_peer_send(peer, 0, ePacket);  
                            enet_host_flush(host); 
                        } else {

                            enet_host_destroy(host);
                            host = nullptr;
                            errorMessage = "SERVER INACCESIBIL!";
                            errorTimer = 3.0f;
                        }
                    }       
                    else {

                        enet_host_destroy(host);
                        host = nullptr;
                    }  
                }       
            }

            if (IsKeyPressed(KEY_ESCAPE)) {

                currentScreen = MENU;
                cursorPosCode = 0;
                roomCode[0] = '\0';
                errorMessage = "";
                errorTimer = 0.0f;
            }
        }
        else if (currentScreen == SINGLEPLAYER) {
            
            if (CheckCollisionPointRec(mousePoint, btnExit) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {

                if (host != nullptr) { 

                    if (peer != nullptr) {

                        enet_peer_disconnect_now(peer, 0); 
                    }
                    enet_host_destroy(host); 

                    host = nullptr; 
                    peer = nullptr; 
                }
                currentScreen = MENU;
            }

            if (!gameOver && !gameWon) {
                
                if(ghostTimer > 0.0f)
                {

                    ghostTimer -= GetFrameTime();

                    if(ghostTimer <= 0.0f) {

                        isRainbow = false;
                        snakeColor = BLUE;
                    }
                }

                if (!bomb.active && !bomb.exploding) {

                    bomb.spawnCooldown -= GetFrameTime();

                    if(bomb.spawnCooldown <= 0.0f) SpawnBomb(bomb, snake);
                } else if (bomb.active) {

                    bomb.timer -= GetFrameTime();

                    if(bomb.timer <= 0.0f) {

                        bomb.active = false;
                        bomb.exploding = true;
                        bomb.explosionTimer = 1.0f;
                    }
                } else if (bomb.exploding) {

                    bomb.explosionTimer -= GetFrameTime();

                    if (bomb.explosionTimer <= 0.0f) {

                        bomb.exploding = false;
                        bomb.spawnCooldown = (float)GetRandomValue(10,15);
                    }
                }

                fruit.timer -= GetFrameTime();

                if (fruit.timer <= 0.0f) {

                    SpawnFruit(fruit, snake);
                }

                if (IsKeyPressed(KEY_UP) && snakeDir.y != 1) nextDir = {0, -1};
                if (IsKeyPressed(KEY_DOWN) && snakeDir.y != -1) nextDir = {0, 1};
                if (IsKeyPressed(KEY_LEFT) && snakeDir.x != 1) nextDir = {-1, 0};
                if (IsKeyPressed(KEY_RIGHT) && snakeDir.x != -1) nextDir = {1, 0};

                moveUpdateTimer += GetFrameTime();

                if (moveUpdateTimer >= moveSpeed) {

                    moveUpdateTimer = 0.0f;
                    snakeDir = nextDir;

                    Vector2 newHead = {snake.front().x + snakeDir.x, snake.front().y + snakeDir.y};

                    if (newHead.x < 0 || newHead.x >= GRID_WIDTH || newHead.y < 0 || newHead.y >= GRID_HEIGHT) {

                        gameOver = true;
                    }
                    
                    if(ghostTimer <= 0.0f) {

                        for (const auto& segment : snake) {

                            if (segment.x == newHead.x && segment.y == newHead.y) {

                                gameOver = true;
                            }
                        }
                    }

                    if(bomb.active && newHead.x == bomb.pos.x && newHead.y == bomb.pos.y) {

                        gameOver = true;
                    }

                    if (bomb.exploding) {

                        for (const auto& segment : snake) {

                            if(abs(segment.x - bomb.pos.x) <=1 && abs(segment.y - bomb.pos.y) <= 1) {

                                gameOver = true;
                            }
                        }
                    }

                    if (!gameOver) {

                        snake.push_front(newHead);

                        if (newHead.x == fruit.pos.x && newHead.y == fruit.pos.y) {

                            if(fruit.type == 6) {

                                ghostTimer = 5.0f;
                                SpawnFruitParticles(fruit.pos, MAGENTA);
                                SpawnFruit(fruit,snake);
                                isRainbow = true;
                            } else {
                                
                                score++;
                                SpawnFruitParticles(fruit.pos, GetColorFromFruitType(fruit.type));
                                isRainbow = false;
                                snakeColor = GetColorFromFruitType(fruit.type);

                                if(score >= 30) gameWon = true;
                                else SpawnFruit(fruit, snake);
                            }   
                        } else {

                            snake.pop_back();
                        }
                    }
                }
            } else {

                bool clickPlayAgain = CheckCollisionPointRec(mousePoint, btnPlayAgain) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
                bool clickBackToMenu = CheckCollisionPointRec(mousePoint, btnBackToMenu) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
                
                if (clickPlayAgain) {
                    
                    StopMusicStream(lostMusic);
                    StopMusicStream(winMusic);
                    PlayMusicStream(bgMusic);

                    snake = {{19, 10}, {18, 10}, {17, 10}};
                    snakeDir = {1, 0};
                    nextDir = {1, 0};
                    score = 0;
                    gameOver = false;
                    gameWon = false;

                    bomb.active = false;
                    bomb.exploding = false;
                    bomb.spawnCooldown = 10.0f;

                    SpawnFruit(fruit, snake);
                } 
                
                if (clickBackToMenu) {
                    
                    StopMusicStream(lostMusic);
                    StopMusicStream(winMusic);
                    PlayMusicStream(bgMusic);

                    snake = {{19, 10}, {18, 10}, {17, 10}};
                    snakeDir = {1, 0};
                    nextDir = {1, 0};
                    score = 0;
                    gameOver = false;
                    gameWon = false;

                    bomb.active = false;
                    bomb.exploding = false;
                    bomb.spawnCooldown = 10.0f;

                    SpawnFruit(fruit, snake);

                    currentScreen=MENU;
                }
            }
        }
        else if (currentScreen == MULTIPLAYER) {

            if (CheckCollisionPointRec(mousePoint, btnExit) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {

                if (host != nullptr) { 

                    if (peer != nullptr) {

                        enet_peer_disconnect_now(peer, 0); 
                    }

                    enet_host_destroy(host); 

                    host = nullptr; 
                    peer = nullptr; 
                }
                currentScreen = MENU;
            }
            
            if (!gameOverMulti && winnerMultiplayer == 0) {

                if (!bomb.active && !bomb.exploding) {

                    bomb.spawnCooldown -= GetFrameTime();

                    if(bomb.spawnCooldown <= 0.0f) {

                        SpawnBomb(bomb, localSnake, remoteSnake);
                    }
                } else if (bomb.active) {

                    bomb.timer -=GetFrameTime();

                    if(bomb.timer <= 0.0f) {

                        bomb.active = false;
                        bomb.exploding = true;
                        bomb.explosionTimer = 1.0f;
                    }
                } else if(bomb.exploding) {

                    bomb.explosionTimer -=GetFrameTime();

                    if(bomb.explosionTimer <= 0.0f) {

                        bomb.exploding = false;
                        bomb.spawnCooldown = (float)GetRandomValue(10,15);
                    }
                }
                
                if (rainbowTimerMulti > 0.0f) {

                    rainbowTimerMulti -=GetFrameTime();
                }
                fruit.timer -= GetFrameTime();

                if (fruit.timer <= 0.0f) {

                    SpawnFruit(fruit, localSnake);
                }

                if (IsKeyPressed(KEY_UP) && localSnakeDir.y != 1) nextDirLocal = {0, -1};
                if (IsKeyPressed(KEY_DOWN) && localSnakeDir.y != -1) nextDirLocal = {0, 1};
                if (IsKeyPressed(KEY_LEFT) && localSnakeDir.x != 1) nextDirLocal = {-1, 0};
                if (IsKeyPressed(KEY_RIGHT) && localSnakeDir.x != -1) nextDirLocal = {1, 0};

                moveUpdateTimer += GetFrameTime();

                if (moveUpdateTimer >= moveSpeed) {

                    moveUpdateTimer = 0.0f;
                    localSnakeDir = nextDirLocal;

                    Vector2 newHead = {localSnake.front().x + localSnakeDir.x, localSnake.front().y + localSnakeDir.y};

                    if (newHead.x < 0 || newHead.x >= GRID_WIDTH || newHead.y < 0 || newHead.y >= GRID_HEIGHT) {

                        winnerMultiplayer = 2;
                    }

                    for (const auto& segment : localSnake) {

                        if (segment.x == newHead.x && segment.y == newHead.y) {

                            winnerMultiplayer = 2;
                        }
                    }

                    for (const auto& segment : remoteSnake) {

                        if (segment.x == newHead.x && segment.y == newHead.y) {

                            winnerMultiplayer = 2;
                        }
                    }

                    for (const auto& segment : remoteSnake) {

                        if (segment.x == newHead.x && segment.y == newHead.y) {

                            winnerMultiplayer = 2; 
                        }
                    }

                    if (bomb.active && newHead.x == bomb.pos.x && newHead.y == bomb.pos.y) {

                        winnerMultiplayer = 2;
                    }
                    if (bomb.exploding) {

                        for (const auto& segment : localSnake) {

                            if (abs(segment.x - bomb.pos.x) <= 1 && abs(segment.y - bomb.pos.y) <= 1) {

                                winnerMultiplayer = 2;
                            }
                        }
                    }

                    if (winnerMultiplayer == 2) {

                        if (host != nullptr && peer != nullptr && peer->state == ENET_PEER_STATE_CONNECTED) {

                            NetworkPacket overPacket;
                            overPacket.type = PACKET_GAME_OVER;
                            ENetPacket* ePacket = enet_packet_create(&overPacket, sizeof(NetworkPacket), ENET_PACKET_FLAG_RELIABLE);
                            enet_peer_send(peer, 0, ePacket);
                        }
                    }

                    if (winnerMultiplayer == 0) {

                        localSnake.push_front(newHead);

                        if (newHead.x == fruit.pos.x && newHead.y == fruit.pos.y) {

                            if (fruit.type == 6) {

                                localScore +=5;
                                rainbowTimerMulti = 5.0f;
                            } else {

                                localScore++;
                            }

                            SpawnFruitParticles(fruit.pos, GetColorFromFruitType(fruit.type));

                            if (localScore >= 30) {

                                winnerMultiplayer = 1;
                            } else {

                                SpawnFruit(fruit, localSnake);
                            }
                            
                            if (host != nullptr && peer != nullptr) {

                                NetworkPacket scorePacket;

                                scorePacket.type = PACKET_SCORE;
                                scorePacket.score = localScore;

                                ENetPacket* ePacket = enet_packet_create(&scorePacket, sizeof(NetworkPacket), ENET_PACKET_FLAG_RELIABLE);

                                if (peer != nullptr && peer->state == ENET_PEER_STATE_CONNECTED) {

                                    enet_peer_send(peer, 0, ePacket);
                                }
                            }
                            
                        } else {

                            localSnake.pop_back();
                        }

                        if (host != nullptr && peer != nullptr) {

                            NetworkPacket sendPacket;

                            sendPacket.type = PACKET_MOVE;
                            sendPacket.x = (int)newHead.x;
                            sendPacket.y = (int)newHead.y;
                            sendPacket.score = localScore;

                            ENetPacket* enetPacket = enet_packet_create(&sendPacket, sizeof(NetworkPacket), ENET_PACKET_FLAG_RELIABLE);

                            if (peer != nullptr && peer->state == ENET_PEER_STATE_CONNECTED){

                                enet_peer_send(peer, 0, enetPacket);                               
                            }
                        }
                    }
                }
            } else {

                bool clickPlayAgain = CheckCollisionPointRec(mousePoint, btnPlayAgain) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
                bool clickBackToMenu = CheckCollisionPointRec(mousePoint, btnBackToMenu) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
                
                if (clickPlayAgain) {
                    
                    StopMusicStream(lostMusic);
                    StopMusicStream(winMusic);
                    PlayMusicStream(bgMusic);

                    localSnake = {{5, 10}, {4, 10}, {3, 10}};
                    remoteSnake = {{33, 10}, {34, 10}, {35, 10}};
                    localSnakeDir = {1, 0};
                    nextDirLocal = {1, 0};
                    localScore = 0;
                    remoteScore = 0;
                    gameOverMulti = false;
                    winnerMultiplayer = 0;

                    bomb.active = false;
                    bomb.exploding = false;
                    bomb.spawnCooldown = 10.0f;

                    SpawnFruit(fruit, localSnake);
                }

                if (clickBackToMenu) {

                    StopMusicStream(lostMusic);
                    StopMusicStream(winMusic);
                    PlayMusicStream(bgMusic);

                    if (host != nullptr) {
                            if (peer != nullptr) {
                                enet_peer_disconnect_now(peer, 0); 
                            }
                        enet_host_destroy(host);

                        host = nullptr;
                        peer = nullptr;
                    }

                    localSnake = {{5, 10}, {4, 10}, {3, 10}};
                    remoteSnake = {{33, 10}, {34, 10}, {35, 10}};
                    localSnakeDir = {1, 0};
                    nextDirLocal = {1, 0};
                    localScore = 0;
                    remoteScore = 0;
                    gameOverMulti = false;
                    winnerMultiplayer = 0;
                    currentScreen = MENU;

                    bomb.active = false;
                    bomb.exploding = false;
                    bomb.spawnCooldown = 10.0f;
                }
            }
        }

        for (auto it = activeParticles.begin(); it != activeParticles.end(); ) {

            it->position.x += it->velocity.x;
            it->position.y += it->velocity.y;
            it->lifeTime -= GetFrameTime();
            
            if (it->lifeTime <= 0.0f) {

                it = activeParticles.erase(it); 
            } else {

                ++it;
            }
        }

        BeginDrawing();
        ClearBackground(RAYWHITE);

        if (currentScreen == MENU) {

            DrawTexture(menuBg, 0, 0, WHITE);
            
            if (CheckCollisionPointRec(mousePoint, btnSingleplayer)) {

                DrawRectangleRounded(btnSingleplayer, 1.0f, 16, Fade(WHITE, 0.3f)); 
            }
            if (CheckCollisionPointRec(mousePoint, btnMultiplayer)) {

                DrawRectangleRounded(btnMultiplayer, 1.0f, 16, Fade(WHITE, 0.3f));
            }
        }
        else if (currentScreen == MULTIPLAYER_SETUP) {

            DrawTexture(setupBg, 0, 0, WHITE);

            if (inLobby) {

                DrawText("Wating for another player...", 200, 200, 30, DARKGRAY);
                DrawText(TextFormat("Room code: %s", roomCode), 200, 250, 40, RED);
                DrawText("Share this code with your friend!", 200, 320, 20, DARKGRAY);
            }
            else {

                DrawText ("=== MULTIPLAYER ===" , 760, 200, 40, BLACK);
                DrawText("Room Code: ", 700, 350, 30, BLACK);
                DrawRectangle(700, 400, 300, 50, RAYWHITE);
                DrawText(roomCode, 720, 410, 30, BLACK);

                if (cursorPosCode == 0) {

                    DrawText ("Type code...", 720, 420, 30, LIGHTGRAY);
                }

                DrawText("Press ENTER to JOIN with code", 700, 600, 30, MAROON);
                DrawText("- OR -", 700, 650, 30, BLACK);
                DrawText("Press C to CREATE a new room", 700, 700, 30, DARKBLUE);
                DrawText("Press ESC to go back", 700, 800, 30, BLACK);
                
                if (errorMessage != "") {

                    DrawText(errorMessage.c_str(), 700, 500, 40, RED);
                }
            }
        }
        else if (currentScreen == SINGLEPLAYER) {

            DrawTexture(singleplayerBg, 0, 0, WHITE);

            Color btnExitColor = CheckCollisionPointRec(mousePoint, btnExit) ? GOLD : ORANGE;

            DrawRectangleRounded(btnExit, 0.3f, 4, btnExitColor);
            DrawText("EXIT", (int)btnExit.x + 45, (int)btnExit.y + 15, 20, WHITE);

            if (!gameOver && !gameWon) {

                int fruitX = OFFSET_X + (int)fruit.pos.x * CELL_SIZE + CELL_SIZE / 2;
                int fruitY = OFFSET_Y + (int)fruit.pos.y * CELL_SIZE + CELL_SIZE / 2;
                
                float r = CELL_SIZE / 2.5f;

                if (fruit.type == 0) {

                    DrawCircle(fruitX, fruitY, r, RED);
                    DrawLineEx({(float)fruitX, (float)fruitY - r}, {(float)fruitX + 5, (float)fruitY - r - 8}, 3, GREEN);
                }
                else if (fruit.type == 1) {

                    DrawCircle(fruitX, fruitY, r, ORANGE);
                    DrawEllipse(fruitX + 2, fruitY - (int)r - 2, 4, 2, LIME);
                }
                else if (fruit.type == 2) {

                    DrawCircle(fruitX, fruitY, r, YELLOW);
                    DrawEllipse(fruitX + 2, fruitY - (int)r - 2, 4, 2, LIME);
                }
                else if (fruit.type == 3) {

                    Vector2 p1 = {(float)fruitX - 3, (float)fruitY - 10};
                    Vector2 p2 = {(float)fruitX + 3, (float)fruitY + 1};
                    Vector2 p3 = {(float)fruitX - 2, (float)fruitY + 12};  

                    DrawLineEx(p1, p2, 10, GOLD); 
                    DrawLineEx(p2, p3, 10, GOLD); 
                    DrawCircle(fruitX + 3, fruitY + 1, 5, GOLD); 
                    DrawRectangle(fruitX - 5, fruitY - 14, 4, 4, DARKBROWN);
                }
                else if (fruit.type == 4) { 

                    DrawCircle(fruitX, fruitY, r, GREEN);
                    DrawLineEx({(float)fruitX, (float)fruitY - 5},{(float)fruitX, (float)fruitY - 12}, 2, DARKGREEN);
                }
                else if (fruit.type == 5) { 

                DrawCircle(fruitX - 6, fruitY+2, r - 4, PURPLE);
                DrawCircle(fruitX + 6, fruitY+2, r - 4, PURPLE);
                DrawCircle(fruitX, fruitY + 10, r - 4, PURPLE);
                }
                else if (fruit.type == 6) {

                    DrawPoly({(float)fruitX, (float)fruitY}, 5, r+5, 0, MAGENTA);
                    DrawPolyLines({(float)fruitX, (float)fruitY}, 5, r+2, 0, WHITE);
                }

                if (isRainbow) {

                    Color rainbowColors[] = { RED, ORANGE, YELLOW, GREEN, BLUE, PURPLE, MAGENTA };

                    int colorIndex = (int)(GetTime()*15)%7;
                    snakeColor = rainbowColors[colorIndex];
                }

                DrawSnake(snake,snakeColor);

                if (bomb.active) {

                    int bx = OFFSET_X + (int)bomb.pos.x * CELL_SIZE + CELL_SIZE / 2;
                    int by = OFFSET_Y + (int)bomb.pos.y * CELL_SIZE + CELL_SIZE / 2;

                    DrawCircle(bx, by, CELL_SIZE / 2.5f, BLACK); 
                    DrawRectangle(bx - 4, by - CELL_SIZE/2.5f - 4, 8, 6, DARKGRAY); 

                    DrawLineEx({(float)bx, (float)by - CELL_SIZE/2.5f - 4}, {(float)bx + 5, (float)by - CELL_SIZE/2.5f - 12}, 2, LIGHTGRAY);
                    if ((int)(GetTime() * 15) % 2 == 0) DrawCircle(bx + 5, by - CELL_SIZE/2.5f - 12, 4, ORANGE);

                    DrawText(TextFormat("%d", (int)bomb.timer+1), bx - 5, by - 8, 20, WHITE);
                }
                else if (bomb.exploding) {

                    int bx = OFFSET_X + ((int)bomb.pos.x - 1) * CELL_SIZE;
                    int by = OFFSET_Y + ((int)bomb.pos.y - 1) * CELL_SIZE;
                    
                    float alpha1 = bomb.explosionTimer; 
                    float alpha2 = bomb.explosionTimer * 0.7f;
                    
                    DrawRectangle(bx, by, CELL_SIZE * 3, CELL_SIZE * 3, Fade(RED, alpha1));
                    DrawRectangle(bx + 5, by + 5, CELL_SIZE * 3 - 10, CELL_SIZE * 3 - 10, Fade(ORANGE, alpha2));

                }
                for(const auto& p : activeParticles) {

                    float alpha = p.lifeTime / p.maxlifeTime;

                    DrawRectangle((int)p.position.x, (int)p.position.y, (int)p.size, (int)p.size, Fade(p.color, alpha));
                } 
            }

            DrawCircle(OFFSET_X + 20, OFFSET_Y - 80, 20, RED); 
            DrawText(TextFormat("%d", score), OFFSET_X + 50, OFFSET_Y - 100, 50, WHITE);
            
            int seconds = (int)fruit.timer;

            DrawText(TextFormat("0:%02d", seconds), OFFSET_X + GRID_WIDTH * CELL_SIZE - 120, OFFSET_Y - 100, 50, WHITE);

            if (gameOver) {
                
                if(IsMusicStreamPlaying(bgMusic)) StopMusicStream(bgMusic);
                if(!IsMusicStreamPlaying(lostMusic)) PlayMusicStream(lostMusic);

                DrawTexture(lostBg, 0, 0, WHITE);

                if(CheckCollisionPointRec(mousePoint, btnPlayAgain)) {

                    DrawRectangleRounded(btnPlayAgain, 1.0f, 32, Fade(WHITE, 0.3f));
                }
                 if(CheckCollisionPointRec(mousePoint, btnBackToMenu)) {

                    DrawRectangleRounded(btnBackToMenu, 1.0f, 32, Fade(WHITE, 0.3f));
                }
            }
            if (gameWon) {
                
                if (IsMusicStreamPlaying(bgMusic)) StopMusicStream(bgMusic);
                if(!IsMusicStreamPlaying(winMusic)) PlayMusicStream(winMusic);

                DrawTexture(wonBg, 0, 0, WHITE);

                if(CheckCollisionPointRec(mousePoint, btnPlayAgain)) {

                    DrawRectangleRounded(btnPlayAgain, 1.0f, 32, Fade(WHITE, 0.3f));
                }
                 if(CheckCollisionPointRec(mousePoint, btnBackToMenu)) {

                    DrawRectangleRounded(btnBackToMenu, 1.0f, 32, Fade(WHITE, 0.3f));
                }
            }
        }
        else if (currentScreen == MULTIPLAYER) {

            DrawTexture(singleplayerBg, 0, 0, WHITE);

            Color btnExitColor = CheckCollisionPointRec(mousePoint, btnExit) ? GOLD : ORANGE;

            DrawRectangleRounded(btnExit, 0.3f, 4, btnExitColor);
            DrawText("EXIT", (int)btnExit.x + 45, (int)btnExit.y + 15, 20, WHITE);

            if (winnerMultiplayer == 0) {

                int fruitX = OFFSET_X + (int)fruit.pos.x * CELL_SIZE + CELL_SIZE / 2;
                int fruitY = OFFSET_Y + (int)fruit.pos.y * CELL_SIZE + CELL_SIZE / 2;
                
                float r = CELL_SIZE / 2.5f;

                if (fruit.type == 0) {

                    DrawCircle(fruitX, fruitY, r, RED);
                    DrawLineEx({(float)fruitX, (float)fruitY - r}, {(float)fruitX + 5, (float)fruitY - r - 8}, 3, GREEN);
                }
                else if (fruit.type == 1) {

                    DrawCircle(fruitX, fruitY, r, ORANGE);
                    DrawEllipse(fruitX + 2, fruitY - (int)r - 2, 4, 2, LIME);
                }
                else if (fruit.type == 2) {

                    DrawCircle(fruitX, fruitY, r, YELLOW);
                    DrawEllipse(fruitX + 2, fruitY - (int)r - 2, 4, 2, LIME);
                }
                else if (fruit.type == 3) {

                    Vector2 p1 = {(float)fruitX - 3, (float)fruitY - 10};
                    Vector2 p2 = {(float)fruitX + 3, (float)fruitY + 1};
                    Vector2 p3 = {(float)fruitX - 2, (float)fruitY + 12};  

                    DrawLineEx(p1, p2, 10, GOLD); 
                    DrawLineEx(p2, p3, 10, GOLD); 
                    DrawCircle(fruitX + 3, fruitY + 1, 5, GOLD); 
                    DrawRectangle(fruitX - 5, fruitY - 14, 4, 4, DARKBROWN);
                }

                else if (fruit.type == 4) { 

                    DrawCircle(fruitX, fruitY, r, GREEN);
                    DrawLineEx({(float)fruitX, (float)fruitY - 5},{(float)fruitX, (float)fruitY - 12}, 2, DARKGREEN);
                }
                else if (fruit.type == 5) { 

                    DrawCircle(fruitX - 6, fruitY+2, r - 4, PURPLE);
                    DrawCircle(fruitX + 6, fruitY+2, r - 4, PURPLE);
                    DrawCircle(fruitX, fruitY + 10, r - 4, PURPLE);
                }
                else if (fruit.type == 6) {

                    DrawPoly({(float)fruitX, (float)fruitY}, 5, r+5, 0, MAGENTA);
                    DrawPolyLines({(float)fruitX, (float)fruitY}, 5, r+2, 0, WHITE);
                }

                Color localColor = Color{80, 80, 255, 255};

                if(rainbowTimerMulti > 0.0f) {

                    Color rainbowColors[] = {RED, ORANGE, YELLOW, GREEN, BLUE, PURPLE, MAGENTA};

                    localColor = rainbowColors[(int)(GetTime()*15)%7];
                }

                DrawSnake(localSnake, localColor);
                DrawSnake(remoteSnake, Color{200, 80, 255, 255});

                if (bomb.active) {

                    int bx = OFFSET_X + (int)bomb.pos.x * CELL_SIZE + CELL_SIZE / 2;
                    int by = OFFSET_Y + (int)bomb.pos.y * CELL_SIZE + CELL_SIZE / 2;

                    DrawCircle(bx, by, CELL_SIZE / 2.5f, BLACK); 
                    DrawRectangle(bx - 4, by - CELL_SIZE/2.5f - 4, 8, 6, DARKGRAY);

                    DrawLineEx({(float)bx, (float)by - CELL_SIZE/2.5f - 4}, {(float)bx + 5, (float)by - CELL_SIZE/2.5f - 12}, 2, LIGHTGRAY);
                    if ((int)(GetTime() * 15) % 2 == 0) DrawCircle(bx + 5, by - CELL_SIZE/2.5f - 12, 4, ORANGE);
                    
                    DrawText(TextFormat("%d", (int)bomb.timer+1), bx - 5, by - 8, 20, WHITE);
                }
                else if (bomb.exploding) {

                    int bx = OFFSET_X + ((int)bomb.pos.x - 1) * CELL_SIZE;
                    int by = OFFSET_Y + ((int)bomb.pos.y - 1) * CELL_SIZE;
                    
                    float alpha1 = bomb.explosionTimer; 
                    float alpha2 = bomb.explosionTimer * 0.7f;
                    
                    DrawRectangle(bx, by, CELL_SIZE * 3, CELL_SIZE * 3, Fade(RED, alpha1));
                    DrawRectangle(bx + 5, by + 5, CELL_SIZE * 3 - 10, CELL_SIZE * 3 - 10, Fade(ORANGE, alpha2));
                }

                for (const auto& p : activeParticles) {

                    float alpha = p.lifeTime / p.maxlifeTime; 

                    DrawRectangle((int)p.position.x, (int)p.position.y, (int)p.size, (int)p.size, Fade(p.color, alpha));
                }
            }

            DrawCircle(OFFSET_X + 20, OFFSET_Y - 80, 20, RED); 
            DrawRectangle(OFFSET_X + 50, OFFSET_Y - 88, 16, 16, Color{80, 80, 255, 255}); 
            DrawText(TextFormat("%d", localScore), OFFSET_X + 75, OFFSET_Y - 100, 50, WHITE);

            DrawCircle(OFFSET_X + 170, OFFSET_Y - 80, 20, RED); 
            DrawRectangle(OFFSET_X + 200, OFFSET_Y - 88, 16, 16, Color{200, 80, 255, 255}); 
            DrawText(TextFormat("%d", remoteScore), OFFSET_X + 225, OFFSET_Y - 100, 50, WHITE);

            int seconds = (int)fruit.timer;

            DrawText(TextFormat("0:%02d", seconds), OFFSET_X + GRID_WIDTH * CELL_SIZE - 120, OFFSET_Y - 100, 50, WHITE);

            if (winnerMultiplayer != 0) {

                if(IsMusicStreamPlaying(bgMusic)) StopMusicStream(bgMusic);

                if (winnerMultiplayer == 1) {
                    
                    if(!IsMusicStreamPlaying(winMusic)) PlayMusicStream(winMusic);

                    DrawTexture(wonBg, 0, 0, WHITE);
                } else {
                    
                    if(!IsMusicStreamPlaying(lostMusic)) PlayMusicStream(lostMusic);

                    DrawTexture(lostBg, 0, 0, WHITE);
                }

                if(CheckCollisionPointRec(mousePoint, btnPlayAgain)) {

                    DrawRectangleRounded(btnPlayAgain, 1.0f, 32, Fade(WHITE, 0.3f));
                }
                if(CheckCollisionPointRec(mousePoint, btnBackToMenu)) {

                    DrawRectangleRounded(btnBackToMenu, 1.0f, 32, Fade(WHITE, 0.3f));
                }
            }
        }

        EndDrawing();
    }
    
    if (host != nullptr) {

        if (peer != nullptr) {

            enet_peer_disconnect_now(peer, 0);
        }
        enet_host_destroy(host);
    }
    enet_deinitialize();

    UnloadTexture(menuBg);
    UnloadTexture(singleplayerBg);
    UnloadTexture(lostBg);
    UnloadTexture(wonBg);
    UnloadTexture(setupBg);
    UnloadMusicStream(bgMusic);
    UnloadMusicStream(winMusic);
    UnloadMusicStream(lostMusic);
    CloseAudioDevice();
    CloseWindow();

    return 0;
}