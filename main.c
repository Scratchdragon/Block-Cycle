#include <raylib.h>

#include <stdio.h>
#include <math.h>

#define ENEMY_TYPES 5           // How many types of enemies there are
#define MAX_ENEMIES 255         // Max amount of enemies allowed on screen

#define DEBUG 0                 // Debug mode will show all bounding boxes

#define MAX_COLLISION_LINES 4   // The max amount of collision lines a shield can have
#define SHIELD_COUNT 4          // The amount of shield types

#define SHOP_ITEM_COUNT 4       // How many items in the shop

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

// Player bonus structure
typedef struct Bonus {
    const char * name;
    int reward;
} Bonus;

// Shop items structure
typedef struct ShopItem {
    // Texture of item inside the shop
    Texture2D texture;
    // How many coins for item
    int cost;
    // Type can be 0 (shield) or 1 (heart)
    int type;
    // ID of the shield or quantity of the hearts
    int id;
} ShopItem;

// Shield variables
Shield currentShield;               // The currently selected shield
Shield shields[SHIELD_COUNT];       // All of theshield types

// Enemy variables
Enemy enemies[MAX_ENEMIES];         // All present enemies
Texture2D enemyTex;                 // The enemy texture
Vector3 enemyColors[ENEMY_TYPES]={  // The enemy colors
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
    },
    (Vector3){
        255,
        0,
        255
    },
    (Vector3){
        255, 
        128, 
        191
    }
};

// Window variables
Vector2 windowSize;         // Window size
Vector2 center;             // Center of the window

// Player variables
Rectangle playerRect;       // The players bounding rectangle
bool died = false;          // If the player has died
float deathTimer = 0;       // How long since player died (for animations)
float rotation = 0;         // Player rotation (degrees)
Texture2D playerTex[4];     // All of the players textures
int sprite = 0;             // The index of the players current texture
int hearts = 1;             // How many hearts the player has

// Other textures
Texture2D coin;
Texture2D arrow;
Texture2D heart;

// Scoring + money variables
int coins = 0;         // How many coins the player has
int score = 0;         // The players current score
Bonus bonuses[] = {     // Moves that can grant the player coins
    {"Close call", 5},
    {"Double kill", 2},
    {"Triple kill", 5},
    {"Multi kill", 10},
    {"Milestone", 10}
};
Bonus latestBonus;      // The latest bonus gained by the player
double bonusTime = 2;   // How long to display the bonus
double killTimer = 0;   // How long between kills (for kill based rewards)
int bonusId;            // The id of the latest bonus
int enemyLevel = 0;     // The max enemy level
int levelScores[ENEMY_TYPES] = {    // Each level's starting score
    0, 5, 10, 40, 80
};

// Shop variables
bool shopOpen = false;          // If the player is in the shop
double shopTimer = 0;           // Time since shop opened (For animations)
ShopItem shopItems[SHOP_ITEM_COUNT];
int shopPage = 0;               // The page the player is on

void GetBonus(int id) {
    bonusId = id;
    latestBonus = bonuses[id];
    bonusTime = 0;
    coins += latestBonus.reward;
}

// Rotates a point around the 0,0 point
Vector2 RotatePoint(Vector2 point, float rotation) {
    return (Vector2){
        cos(rotation) * point.x - sin(rotation) * point.y,
        sin(rotation) * point.x + cos(rotation) * point.y
    };
}

// Rotates a line around the 0,0 point
Line RotateLine(Line l, float rotation) {
    return (Line) {
        RotatePoint(l.a, rotation),
        RotatePoint(l.b, rotation)
    };
}

// Translate a Vector3 into a Color
Color ColorFromVec3(Vector3 vec3, float alpha) {
    return (Color){
        (unsigned char)vec3.x,
        (unsigned char)vec3.y,
        (unsigned char)vec3.z,
        (unsigned char)alpha
    };
}

// Checks if two lines are intersecting
bool LineCollision(Line a, Line b) {
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

// SpawnEnemy is used to create new enemies, it return the new enemy's index
int SpawnEnemy(int id, int state) {
    // Find a free space for the enemy
    int index;
    for(index = 0; index<MAX_ENEMIES; ++index) {
        // Break when a free space is found
        if(enemies[index].id == 0)
            break;
    }

    // Calculate position
    Vector2 position = {
        (float)GetRandomValue(0, windowSize.x),
        (float)GetRandomValue(0, windowSize.y)
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
    enemies[index].state = state;
    enemies[index].timer = 0;
    enemies[index].rotation = atan2(center.x - position.x, center.y - position.y);

    // Do enemy specific customization
    switch(id) {
        case 2:
        case 5:
            enemies[index].speed = 16;
            break;
        default:
            break;
    }
    return index;
}

// Spawns enemy without a provided state
int SpawnDefaultEnemy(int id) {
    return SpawnEnemy(id, 0);
}

// Changes the score and updates enemy level
void SetScore(int newScore) {
    // Set the score
    score = newScore;

    // Reset enemy level on score reset
    if(score <= 0) {
        enemyLevel = 0;
        return;
    }

    // Update the enemy level
    for(int i = 0; i < ENEMY_TYPES; ++i) {
        if(levelScores[i] == score) {
            enemyLevel = i;

            // Get the "Milestone" bonus
            GetBonus(4);
        }
    }
}

float Distance(Vector2 a, Vector2 b) {
    return sqrtf(pow(b.x - a.x, 2) + pow(b.y - a.y, 2));
}

// Enemy update method
void UpdateEnemy(Enemy * enemyPtr, float deltaTime, float scale) {
    // Fade out a dieing enemy
    if(enemyPtr->state == 1) {
        // Split if a normal purple enemy (not small)
        if(enemyPtr->id == 4 && enemyPtr->timer == 0) {
            // Spawn three small slimes
            for(int i = 0; i < 3; ++i) {
                int enemyIndex = SpawnEnemy(4, 2);
                enemies[enemyIndex].position = enemyPtr->position;
                enemies[enemyIndex].rotation = -enemyPtr->rotation + (float)GetRandomValue(-10, 10) / 50.0f;
                enemies[enemyIndex].timer = (float)GetRandomValue(10, 30) / 10.0f;
            }
            enemyPtr->id = 0;
            return;
        }

        // Increment the timer
        enemyPtr->timer += deltaTime;

        // Delete enemy when 0.5s is elapsed
        if(enemyPtr->timer > 0.5f) {
            enemyPtr->id = 0;

            // Increment the players score
            if(!died)
                SetScore(score + 1);
        }
        return;
    }
    
    // Kill the enemy if the player died
    if(died)
        enemyPtr->state = 1;

    // Move the enemy the towards where it is facing
    enemyPtr->position.x += sin(enemyPtr->rotation) * deltaTime * enemyPtr->speed * scale;
    enemyPtr->position.y += cos(enemyPtr->rotation) * deltaTime * enemyPtr->speed * scale;

    // Calculate bounds
    enemyPtr->bounds = (Rectangle){
        enemyPtr->position.x - scale,
        enemyPtr->position.y - scale,
        scale * 2,
        scale * 2
    };

    // Bounds are smaller for small purple slime and pink slimes
    if(enemyPtr->id == 4 && enemyPtr->state >= 2 || enemyPtr->id == 5) {
        enemyPtr->bounds = (Rectangle){
            enemyPtr->position.x - scale / 2,
            enemyPtr->position.y - scale / 2,
            scale,
            scale
        };
    }

    // Check collision with player (if not already dead)
    if(CheckCollisionRecs(enemyPtr->bounds, playerRect) && enemyPtr->state != 2 && enemyPtr->state != 1) {
        --hearts;
        if(hearts <= 0)
            died = true;
        
        enemyPtr->state = 1;
    }
    
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
            if(Distance(center, enemyPtr->position) < scale * 3)
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
        case 4:
            if(enemyPtr->state == 2) {
                if(enemyPtr->timer >= 0)
                    enemyPtr->timer -= deltaTime;
                else
                    enemyPtr->rotation = atan2(center.x - enemyPtr->position.x, center.y - enemyPtr->position.y);
            }
            break;
        case 5:
            if(Distance(center, enemyPtr->position) < scale * 6 && enemyPtr->state == 0) {
                enemyPtr->state = 2;
                enemyPtr->rotation = -enemyPtr->rotation;
                enemyPtr->timer = GetRandomValue(4, 8);
            }
            if(enemyPtr->state == 2) {
                enemyPtr->timer -= deltaTime;
                enemyPtr->rotation += deltaTime * 2;
                enemyPtr->position = (Vector2) {
                    sin(enemyPtr->rotation) * scale * 6 + center.x,
                    -cos(enemyPtr->rotation) * scale * 6 + center.y
                };

                if(enemyPtr->timer <= 0) {
                    enemyPtr->state = 3;
                    enemyPtr->speed = 8;
                    enemyPtr->rotation = -enemyPtr->rotation;
                }
            }
            break;
    }
}

// The DrawShop method contains all the code used to render the shop
void DrawShop(float scale, float deltaTime) {
    // Effecient way of doing a slide in/out animation
    float xOffset = shopTimer > 0.2f ? 0 : windowSize.x - (shopTimer * windowSize.x * 5);

    // Draw the shops background
    DrawRectangleRec(
        (Rectangle) {
            scale * 3 + xOffset,
            scale * 3,
            windowSize.x - scale * 6,
            windowSize.y - scale * 6
        },
        WHITE
    );

    // Draw the shops border
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

    // Draw shop title
    DrawText("S H O P", center.x + xOffset - scale * 3.6f, scale * 4, scale * 2, BLACK);

    // Get the position to display the next page button at
    Vector2 arrowPos = (Vector2){windowSize.x - scale * 6.8f + xOffset, windowSize.y - scale * 6.8f};
    // Images draw from the top left corner so the arrow's center is required to check hovering
    Vector2 arrowCenter = (Vector2){arrowPos.x + scale * arrow.width / 24, arrowPos.y + scale * arrow.height / 24};
    // Draw next page arrow (highlight if hovering)
    if(Distance(GetMousePosition(), arrowCenter) < scale) {
        DrawTextureEx(
            arrow, 
            arrowPos, 
            0,
            scale / 12,
            WHITE
        );

        // Increment the page if mouse down
        if(IsMouseButtonReleased(0)) {
            ++shopPage;

            // Loop back around to 0 if page number is too high
            if(shopPage * 3 > SHOP_ITEM_COUNT)
                shopPage = 0;
        }
    }
    else {
        DrawTextureEx(
            arrow, 
            arrowPos, 
            0,
            scale / 12,
            RAYWHITE
        );
    }

    // Draw the shop items
    for(int i = shopPage * 3; i<SHOP_ITEM_COUNT && i < 3 + shopPage * 3; ++i) {
        Color itemColor = RAYWHITE;

        Vector2 position = (Vector2){
            // Draw items relative to the center of the screen and offset image to be centered
            center.x + (i - 1 - shopPage * 3) * scale * 11 - (shopItems[i].texture.width * scale / 20) + xOffset,
            scale * 8
        };

        // Check if the mouse is within the shop items bounds
        if( GetMouseX() > position.x && 
            GetMouseY() > position.y && 
            GetMouseX() < position.x + shopItems[i].texture.width * scale / 10 && 
            GetMouseY() < position.y + shopItems[i].texture.height * scale / 10
        ) {
            // Highlight selected item
            itemColor = WHITE;

            // Check if player is and can afford buying the item
            if(IsMouseButtonReleased(0) && coins >= shopItems[i].cost) {
                coins -= shopItems[i].cost;
                switch(shopItems[i].type) {
                    case 0: // Shield
                        currentShield = shields[shopItems[i].id];
                        break;
                    case 1: // Heart
                        hearts += shopItems[i].id;
                        break;
                }
            }
        }

        // Draw the item
        DrawTextureEx(
            shopItems[i].texture,
            position,
            0,
            scale / 10,
            itemColor
        );
    }
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

        // Update the enemy here to save on time
        if(!shopOpen)
            UpdateEnemy(&enemies[i], deltaTime, scale);

        // Detirmine the sprites original dimensions
        Rectangle source = {
            0,
            0,
            (float)enemyTex.width,
            (float)enemyTex.height
        };

        // Flip the sprite if looking left
        if(sin(enemies[i].rotation) < 0) {
            source = (Rectangle){
                enemyTex.width * 2.0f,
                0,
                (float)-enemyTex.width,
                (float)enemyTex.height
            };
        }

        // Render the enemy
        DrawTexturePro(
            enemyTex, 
            source,
            enemies[i].bounds,
            (Vector2){0, 0},
            0,
            ColorFromVec3(
                enemyColors[enemies[i].id - 1], 
                enemies[i].state == 1 ? (0.5f - enemies[i].timer) * 510 : 255
            )
        );

        // Draw debug lines
        if(DEBUG)
            DrawRectangleLinesEx(enemies[i].bounds, 1, RED);
    }
    
    // Draw the player
    unsigned char playerAlpha = 255;
    if(died)
        playerAlpha = deathTimer < 0.5f ? (0.5f - deathTimer) * 510 : 0;
    
    DrawTexturePro(
        playerTex[sprite], 
        (Rectangle){
            0,
            0,
            (float)playerTex[sprite].width,
            (float)playerTex[sprite].height
        },
        playerRect,
        (Vector2){0, 0},
        0,
        (Color){
            255,
            255,
            255,
            playerAlpha
        }
    );

    // Draw the players shield
    DrawTexturePro(
        currentShield.texture,
        (Rectangle){
            0,
            0,
            (float)currentShield.texture.width,
            (float)currentShield.texture.height
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
        (Color){
            255,
            255,
            255,
            playerAlpha
        }
    );
    
    // Draw debug lines
    if(DEBUG)
        DrawRectangleLinesEx(playerRect, 1, GREEN);


    // Draw UI
    char str[255]; // Temp string for converting int to string
    sprintf(str, "%d", score);
    DrawText(str, scale / 2, scale / 2, scale * 2, BLACK);

    // Underline score with the next enemy color
    DrawRectangle(
        scale / 2, 
        scale * 2.2, 
        MeasureText(str, scale * 2), 
        scale / 4, 
        ColorFromVec3(enemyColors[enemyLevel], 255)
    );

    // Draw money
    sprintf(str, "%d", coins);
    DrawText(str, windowSize.x - (TextLength(str) + 3) * scale, scale / 2, scale * 2, BLACK);
    DrawTextureEx(coin, (Vector2){windowSize.x - scale * 2.5f, scale / 1.9f}, 0, scale / 5, WHITE);

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
            windowSize.x - TextLength(bonusStr) * scale / 2, 
            scale * 2.5, scale, 
            (Color){200, 200, 200, (unsigned char)(255 - 120 * bonusTime)}
        );
    }

    // Draw hearts
    for(int i = 0; i < hearts; ++i) {
        DrawTextureEx(
            heart,
            (Vector2){scale / 2 + scale * i * 2, windowSize.y - scale * 2},
            0,
            scale / 10,
            WHITE
        );
    }

    // If shop is open or still in animation then render it
    if(shopTimer > 0)
        DrawShop(scale, deltaTime);

    // Draw the death message if player is dead
    if(died) {
        Color textColor = BLACK;
        textColor.a = (unsigned char)(255 - playerAlpha);

        DrawText(
            "You died", 
            center.x - TextLength("You died") * scale, 
            center.y - scale * 4, 
            scale * 4, 
            textColor
        );

        // Make subtext more faded
        textColor.a /= 2;
        DrawText(
            "Press any key to continue", 
            center.x - TextLength("Press any key to continue") * scale / 4, 
            center.y, 
            scale, 
            textColor
        );
    }
}

// HandleInput contains all of the core user input processing code
void HandleInput(float deltaTime) {
    // Rotate player to look at the mouse
    rotation = 180 - round((atan2(GetMousePosition().x - center.x, GetMousePosition().y - center.y) / 3.1415)*180);

    // Space opens/closes the shop
    if(IsKeyPressed(KEY_SPACE))
        shopOpen = !shopOpen;
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
            (Line){{2.5, -1.6}, {2.5, 1.6}}
        }
    };
    shields[1] = (Shield){ // Long shield
        // Texture
        LoadTexture("resources/images/shield/long.png"),
        // Collision lines
        {
            (Line){{2.5, -2.2}, {2.5, 2.2}}
        }
    };
    shields[2] = (Shield){ // Armor shield
        // Texture
        LoadTexture("resources/images/shield/armor.png"),
        // Collision lines
        {
            (Line){{-1.4,  1.8}, {1.4, 1.8}},
            (Line){{-1.4, -1.8}, {1.4, -1.8}}
        }
    };
    shields[3] = (Shield){ // Boomerang shield
        // Texture
        LoadTexture("resources/images/shield/boomarang.png"),
        // Collision lines
        {
            (Line){{1.2,  1.8}, {2.9, 0}},
            (Line){{1.2, -1.8}, {2.9, 0}}
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
    heart = LoadTexture("resources/images/heart.png");

    // Load shop items
    shopItems[0] = (ShopItem){
        LoadTexture("resources/images/shop/heart.png"),
        10,
        1,
        1
    };
    shopItems[1] = (ShopItem){
        LoadTexture("resources/images/shop/long_shield.png"),
        10,
        0,
        1
    };
    shopItems[2] = (ShopItem){
        LoadTexture("resources/images/shop/armor_shield.png"),
        40,
        0,
        2
    };
    shopItems[3] = (ShopItem){
        LoadTexture("resources/images/shop/boomerang_shield.png"),
        60,
        0,
        3
    };

    // Get initial window size
    windowSize.x = GetRenderWidth();
    windowSize.y = GetRenderHeight();

    // Get initial window center
    center.x = windowSize.x / 2;
    center.y = windowSize.y / 2;

    // Scale of objects on the window
    float scale = sqrt(pow(windowSize.x, 2) + pow(windowSize.y, 2)) / 50;

    // Time inbetween frames
    float deltaTime;

    // Main loop
    while(!WindowShouldClose()) {
        // Update window metrics if window resized
        if(IsWindowResized()) {
            // Update window size
            windowSize.x = GetRenderWidth();
            windowSize.y = GetRenderHeight();

            // Get the window center
            center.x = windowSize.x / 2;
            center.y = windowSize.y / 2;

            // Move enemies to their new relative position to avoid teleporting
            for(int i = 0; i < MAX_ENEMIES; ++i) {
                enemies[i].position.x /= scale;
                enemies[i].position.y /= scale;
            }

            // Update scale
            scale = sqrt(pow(windowSize.x, 2) + pow(windowSize.y, 2)) / 50;

            // Move enemies to the new position on the window
            for(int i = 0; i < MAX_ENEMIES; ++i) {
                enemies[i].position.x *= scale;
                enemies[i].position.y *= scale;
            }
        }

        // Update delta time
        deltaTime = GetFrameTime();

        // Update the spawn time based on score
        spawnTime = 3 - score / 70.0f;

        // Don't lets enemies spawn faster than every half second
        if(spawnTime <= 0.5f)
            spawnTime = 0.5f;

        // Update the timers
        killTimer += deltaTime;
        bonusTime += deltaTime;
        if(shopOpen)
            shopTimer += deltaTime;
        else if(shopTimer > 0) {
            if(shopTimer > 0.2)
                shopTimer = 0.2;
            shopTimer -= deltaTime;
        }

        // Death specific actions
        if(!died) {
            // Check if the time has elapsed the next spawn interval (and if not in shop)
            if(oldTime / spawnTime < round(oldTime / spawnTime) && GetTime() / spawnTime >= round(oldTime / spawnTime) && !shopOpen)
                SpawnDefaultEnemy(GetRandomValue(1, enemyLevel + 1));
            oldTime = GetTime();
            
            // Update all input
            HandleInput(deltaTime);
        }
        else
            deathTimer += deltaTime;

        // Get the players sprite index
        sprite = (int)round(rotation / 90) % 4;

        // Wait for a keypress before resetting from death
        if(died && GetKeyPressed()) {
            died = false;
            score = 0;
            deathTimer = 0;
            hearts = 1;

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
    for(int i = 0; i < SHIELD_COUNT; ++i)
        UnloadTexture(shields[i].texture);
    for(int i = 0; i < SHOP_ITEM_COUNT; ++i)
        UnloadTexture(shopItems[i].texture);
    UnloadTexture(enemyTex);
    
    CloseWindow();
}