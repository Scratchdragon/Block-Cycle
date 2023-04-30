#include <raylib.h>

#include <stdio.h>
#include <math.h>

#define ENEMY_TYPES 3   // How many types of enemies there are
#define MAX_ENEMIES 255 // Max amount of enemies allowed on screen

#define DEBUG 0         // Debug mode will show all bounding boxes

#define MAX_COLLISION_LINES 4   // The max amount of collision lines a shield can have
#define SHIELD_COUNT 1          // The amount of shield types

// The enemy structure
typedef struct Enemy {
    // ID 0 means dead
    char id;
    Vector2 position;
    float rotation;
    float speed;
    Rectangle bounds;

    float timer;
    int state;
} Enemy;

// Basic line structure
typedef struct Line {
    Vector2 a;
    Vector2 b;
} Line;

// The shield structure
typedef struct Shield {
    Texture2D texture;
    Line lines[MAX_COLLISION_LINES];
} Shield;

typedef struct Bonus {
    const char * name;
    int reward;
} Bonus;

// Shield variables
Shield currentShield;              // The currently selected shield
Shield shields[SHIELD_COUNT];       // All of the shield types

// Enemy variables
Enemy enemies[MAX_ENEMIES];         // All present enemies
Texture2D enemyTex;                // The enemy texture
Vector3 enemyColors[ENEMY_TYPES]={ // The enemy colors
    (Vector3){
        0,
        0,
        255
    },
    (Vector3){
        0,
        255,
        0
    },
    (Vector3){
        0,
        255,
        255
    }
};

// Window variables
Vector2 windowSize;        // Window size
Vector2 center;             // Center of the window

// Player variables
Rectangle playerRect;      // The players bounding rectangle
bool died = false;          // If the player has died
float rotation = 0;         // Player rotation (degrees)
Texture2D playerTex[4];    // All of the players textures
int sprite = 0;             // The index of the players current texture

// Other textures
Texture2D coin;
Texture2D arrow;

// Scoring + money variables
long coins = 0;         // How many coins the player has
long score = 0;         // The players current score
Bonus bonuses[] = {     // Moves that can grant the player coins
    {"Close call", 10},
    {"Double kill", 5},
    {"Triple kill", 5},
    {"Multi kill", 10},
    {"Milestone", 20},
    {"New enemy", 20}
};
Bonus latestBonus;      // The latest bonus gained by the player
double bonusTime = 2;   // How long to display the bonus
double killTimer = 0;   // How long between kills (for kill based rewards)
int bonusId;            // The id of the latest bonus

// Shop variables
bool shop = false;      // If the player is in the shop
double shopTimer = 0;    // Time since shop opened (For animations)

void GetBonus(int id) {
    bonusId = id;
    latestBonus = bonuses[id];
    bonusTime = 0;
    coins += latestBonus.reward;
}

// Rotates a point around the 0,0 point
inline Vector2 RotatePoint(Vector2 point, float rotation) {
    return (Vector2){
        cos(rotation) * point.x - sin(rotation) * point.y,
        sin(rotation) * point.x + cos(rotation) * point.y
    };
}

// Rotates a line around the 0,0 point
inline Line RotateLine(Line l, float rotation) {
    return (Line) {
        RotatePoint(l.a, rotation),
        RotatePoint(l.b, rotation)
    };
}

// Checks if two lines are intersecting
inline bool LineCollision(Line a, Line b) {
    Vector2 point;
    return CheckCollisionLines(
        a.a,
        a.b,
        b.a,
        b.b,
        &point
    );
}

// Checks if a line and rectangle intersect
bool LineRectCollision(Line a, Rectangle b) {
    // Turn the rect into an array of lines
    Line rect[] = {
        {
            {b.x, b.y},
            {b.x, b.y + b.height}
        },
        {
            {b.x, b.y + b.height},
            {b.x + b.width, b.y + b.height}
        },
        {
            {b.x + b.width, b.y + b.height},
            {b.x + b.width, b.y}
        },
        {
            {b.x + b.width, b.y},
            {b.x, b.y}
        }
    };

    // Check each line
    for(int i = 0; i < 4; ++i) {
        if(LineCollision(rect[i], a))
            return true;
    }
    return false;
}

// Enemy update method
void UpdateEnemy(Enemy * enemyPtr, float deltaTime, float scale) {
    // Fade out a dieing enemy
    if(enemyPtr->state == 1) {
        // Increment the timer
        enemyPtr->timer += deltaTime;

        // Delete enemy when 0.5s is elapsed
        if(enemyPtr->timer > 0.5f) {
            enemyPtr->id = 0;

            // If player died then reset
            if(died) {
                score = -1; // Score -1 means to reset
                return;
            }

            // Increment the players score
            ++score;
        }
        return;
    }
    
    // Kill the enemy if the player died
    if(died)
        enemyPtr->state = 1;

    enemyPtr->position.x += sin(enemyPtr->rotation) * deltaTime * enemyPtr->speed * scale;
    enemyPtr->position.y += cos(enemyPtr->rotation) * deltaTime * enemyPtr->speed * scale;

    // Check collision
    if(CheckCollisionRecs(enemyPtr->bounds, playerRect) && enemyPtr->state != 2)
        died = true;
    
    // Check shield collision
    bool collide = false;
    for(int i = 0; i < MAX_COLLISION_LINES; ++i) {
        // Make sure the shields collision is rotated
        Line rotatedLine = RotateLine(currentShield.lines[i], (rotation + 270) * DEG2RAD);
        rotatedLine.a.x *= scale;
        rotatedLine.a.y *= scale;
        rotatedLine.b.x *= scale;
        rotatedLine.b.y *= scale;
        rotatedLine.a.x += center.x;
        rotatedLine.a.y += center.y;
        rotatedLine.b.x += center.x;
        rotatedLine.b.y += center.y;

        if(DEBUG)
            DrawLineEx(rotatedLine.a, rotatedLine.b, 1, BLUE);

        // Check if colliding with shield
        if(LineRectCollision(rotatedLine, enemyPtr->bounds)) {
            collide = true;
            break;
        }
    }
    if(collide) {
        if(enemyPtr->id == 3 && enemyPtr->state != 4) {
            if(enemyPtr->state != 2)
                enemyPtr->rotation = -enemyPtr->rotation;
            enemyPtr->state = 2;
        }
        else {
            enemyPtr->state = 1;
            
            // Check for bonuses
            if(sqrtf(pow(center.x - enemyPtr->position.x, 2) + pow(center.y - enemyPtr->position.y, 2)) < scale * 3)
                GetBonus(0); // Close call

            if(killTimer < 0.3) {
                if(bonusTime > 2)
                    GetBonus(1);
                else if(bonusId == 1)
                    GetBonus(2);
                else if(bonusId == 2 || bonusId == 3)
                    GetBonus(3);
                else
                    GetBonus(1);
            }
            killTimer = 0;
        }
    }

    // Per enemy types actions
    switch(enemyPtr->id) {
        case 3:
            if(enemyPtr->state == 2) {
                enemyPtr->timer += deltaTime;
                enemyPtr->rotation += deltaTime * PI / 2;
                if(enemyPtr->timer >= 2) {
                    enemyPtr->timer = 0;
                    enemyPtr->state = 4;
                    enemyPtr->rotation = atan2(center.x - enemyPtr->position.x, center.y - enemyPtr->position.y);
                }
            }
            break;
    }
}

void SpawnEnemy(int id) {
    // Find a free space for the enemy
    int index;
    for(index = 0; index<MAX_ENEMIES; ++index) {
        // Break when a free space is found
        if(enemies[index].id == 0)
            break;
    }

    // Calculate position
    Vector2 position = {
        GetRandomValue(0, windowSize.x),
        GetRandomValue(0, windowSize.y)
    };
    if (GetRandomValue(0, 1)) {
        // Horizontal wall
        position.x = GetRandomValue(0, 1) * windowSize.x;
    }
    else {
        // Vertical wall
        position.y = GetRandomValue(0, 1) * windowSize.y;
    }

    // Set the enemy
    enemies[index].id = id;
    enemies[index].speed = 8;
    enemies[index].position = position;
    enemies[index].state = 0;
    enemies[index].timer = 0;
    enemies[index].rotation = atan2(center.x - position.x, center.y - position.y);

    // Do enemy specific customization
    switch(id) {
        case 2:
            enemies[index].speed = 16;
            break;
        case 3:
            break;
        default:
            break;
    }
}

// The DrawShop method contains all the code used to render the shop
void DrawShop(float scale, float deltaTime) {
    float xOffset = shopTimer > 0.2 ? 0 : windowSize.x - (shopTimer * windowSize.x * 5);
    DrawRectangleRec(
        (Rectangle) {
            scale * 3 + xOffset,
            scale * 3,
            windowSize.x - scale * 6,
            windowSize.y - scale * 6
        },
        WHITE
    );
    DrawRectangleLinesEx(
        (Rectangle) {
            scale * 3 + xOffset,
            scale * 3,
            windowSize.x - scale * 6,
            windowSize.y - scale * 6
        },
        scale / 5,
        BLACK
    );

    DrawText("S H O P", center.x + xOffset - scale * 4.2, scale * 4, scale * 2, BLACK);
    DrawTextureEx(
        arrow, 
        (Vector2){windowSize.x - scale * 6.8 + xOffset, windowSize.y - scale * 6.8}, 
        0,
        scale / 12,
        WHITE
    );
}

// The render method should contain all rendering code
void Render(float scale, float deltaTime) {
    // Clear the screen
    ClearBackground(WHITE);

    // Render all enemies
    for(int i = 0; i<MAX_ENEMIES;++i) {
        // Don't render if the enemy is dead (id=0)
        if(!enemies[i].id)
            continue;

        // Detirmine the sprites original dimensions
        Rectangle source = {
            0,
            0,
            enemyTex.width,
            enemyTex.height
        };

        // Flip the sprite if looking left
        if(sin(enemies[i].rotation) < 0) {
            source = (Rectangle){
                enemyTex.width * 2,
                0,
                -enemyTex.width,
                enemyTex.height
            };
        }
        
        // Calculate bounds
        enemies[i].bounds = (Rectangle){
            enemies[i].position.x - scale,
            enemies[i].position.y - scale,
            scale * 2,
            scale * 2
        };

        // Render the enemy
        DrawTexturePro(
            enemyTex, 
            source,
            enemies[i].bounds,
            (Vector2){0, 0},
            0,
            (Color){
                enemyColors[enemies[i].id - 1].x,
                enemyColors[enemies[i].id - 1].y,
                enemyColors[enemies[i].id - 1].z,
                enemies[i].state == 1 ? (unsigned char)((0.5f - enemies[i].timer) * 510) : 255
            }
        );

        // Draw debug lines
        if(DEBUG)
            DrawRectangleLinesEx(enemies[i].bounds, 1, RED);
        
        // Update the enemy here to save on time
        if(!shop)
            UpdateEnemy(&enemies[i], deltaTime, scale);
    }
    
    // Draw the player
    DrawTexturePro(
        playerTex[sprite], 
        (Rectangle){
            0,
            0,
            playerTex[sprite].width,
            playerTex[sprite].height
        },
        playerRect,
        (Vector2){0, 0},
        0,
        WHITE
    );

    // Draw the players shield
    DrawTexturePro(
        currentShield.texture,
        (Rectangle){
            0,
            0,
            currentShield.texture.width,
            currentShield.texture.height
        },
        (Rectangle) {
            center.x,
            center.y,
            scale * 8,
            scale * 4
        },
        (Vector2){
            scale * 4, scale * 2
        },
        rotation - 90,
        WHITE
    );
    
    // Draw debug lines
    if(DEBUG)
        DrawRectangleLinesEx(playerRect, 1, GREEN);


    // Draw UI
    char str[255]; // Temp string for converting int to string
    sprintf(str, "%d", score);
    DrawText(str, scale / 2, scale / 2, scale * 2, BLACK);

    // Draw money
    sprintf(str, "%d", coins);
    DrawText(str, windowSize.x - (TextLength(str) + 3) * scale, scale / 2, scale * 2, BLACK);
    DrawTextureEx(coin, (Vector2){windowSize.x - scale * 2.5, scale / 1.9}, 0, scale / 5, WHITE);

    // Draw bonus
    if(bonusTime < 2) {
        // Convert the bonus' reward to as string
        sprintf(str, "%d", latestBonus.reward);

        // Concatenate all the string together
        const char * tokens[] = {
            "+", str, " ", latestBonus.name
        };
        const char * bonusStr = TextJoin(tokens, 4, "");

        // Draw the final string
        DrawText(
            bonusStr, 
            windowSize.x - (TextLength(bonusStr)) * scale / 2, 
            scale * 2.5, scale, 
            (Color){200, 200, 200, (unsigned char)(255 - 120 * bonusTime)}
        );
    }

    if(shopTimer > 0)
        DrawShop(scale, deltaTime);
}

int main() {
    // The time of the last frame
    float oldTime;

    // How many seconds until another enemy should spawn
    float spawnTime = 2;

    // Set the starting window size
    windowSize = (Vector2){800, 500};

    // Init the window
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(windowSize.x, windowSize.y, "Block-Cycle");

    // Init all the shields
    shields[0] = (Shield){ // Basic shield
        // Texture
        LoadTexture("resources/images/shield/basic.png"),
        // Collision lines
        {
            (Line){{2.2, -1.8}, {2.2, 1.8}}
        }
    };

    // Set the default shield
    currentShield = shields[0];

    // Load all the player textures
    playerTex[0] = LoadTexture("resources/images/up.png");
    playerTex[1] = LoadTexture("resources/images/right.png");
    playerTex[2] = LoadTexture("resources/images/down.png");
    playerTex[3] = LoadTexture("resources/images/left.png");

    // Load al the enemy textures
    enemyTex = LoadTexture("resources/images/enemies/enemy.png");
    
    // Load other textures
    coin = LoadTexture("resources/images/coin.png");
    arrow = LoadTexture("resources/images/arrow.png");

    // Scale of objects on the window
    float scale;

    // Time inbetween frames
    float deltaTime;

    // Main loop
    while(!WindowShouldClose()) {
        // Update window size
        windowSize.x = GetRenderWidth();
        windowSize.y = GetRenderHeight();

        // Update delta time
        deltaTime = GetFrameTime();

        // Get the window center
        center.x = windowSize.x / 2;
        center.y = windowSize.y / 2;

        // Update scale
        scale = sqrt(pow(windowSize.x, 2) + pow(windowSize.y, 2)) / 50;

        // Look at the mouse
        if(!died)
            rotation = 180 - round((atan2(GetMousePosition().x - center.x, GetMousePosition().y - center.y) / 3.1415)*180);

        // Get the players sprite index
        sprite = (int)round(rotation / 90) % 4;

        // Check if the time has elapsed the next spawn interval (and if not in shop)
        if(oldTime / spawnTime < round(oldTime / spawnTime) && GetTime() / spawnTime >= round(oldTime / spawnTime) && !shop)
            SpawnEnemy(GetRandomValue(1, ENEMY_TYPES));
        oldTime = GetTime();

        // Increment the timers
        killTimer += deltaTime;
        bonusTime += deltaTime;
        if(shop)
            shopTimer += deltaTime;
        else if(shopTimer > 0) {
            if(shopTimer > 0.2)
                shopTimer = 0.2;
            shopTimer -= deltaTime;
        }

        if(IsKeyReleased(KEY_SPACE))
            shop = !shop;

        // If score is -1 then reset
        if(score == -1) {
            died = false;
            score = 0;

            // Remove all enemies
            for(int i = 0; i<MAX_ENEMIES; ++i)
                enemies[i].id = 0;
        }

        // Update the players bounding rectangle
        playerRect = (Rectangle){
            center.x - scale,
            center.y - scale,
            scale * 2,
            scale * 2
        };

        // Draw everything
        BeginDrawing();
        Render(scale, deltaTime);
        EndDrawing();
    }

    // Unload everything and close the window
    for(int i = 0; i < 4; ++i)
        UnloadTexture(playerTex[i]);
    UnloadTexture(enemyTex);
    
    CloseWindow();
}