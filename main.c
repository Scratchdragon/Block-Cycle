#include <raylib.h>

#include <stdio.h>
#include <math.h>

#define ENEMY_TYPES 3   // How many types of enemies there are
#define MAX_ENEMIES 255 // Max amount of enemies allowed on screen

#define DEBUG 0         // Debug mode will show all bounding boxes

#define MAX_COLLISION_LINES 4   // The max amount of collision lines a shield can have
#define SHIELD_COUNT 1          // The amount of shield types

// The enemy structure
typedef struct enemy {
    // ID 0 means dead
    char id;
    Vector2 position;
    float rotation;
    float speed;
    Rectangle bounds;

    float timer;
    int state;
} enemy;

// Basic line structure
typedef struct line {
    Vector2 a;
    Vector2 b;
} line;

// The shield structure
typedef struct shield {
    Texture2D texture;
    line lines[MAX_COLLISION_LINES];
} shield;

typedef struct bonus {
    const char * name;
    int reward;
} bonus;

// Shield variables
shield current_shield;              // The currently selected shield
shield shields[SHIELD_COUNT];       // All of the shield types

// Enemy variables
enemy enemies[MAX_ENEMIES];         // All present enemies
Texture2D enemy_tex;                // The enemy texture
Vector3 enemy_colors[ENEMY_TYPES]={ // The enemy colors
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
Vector2 window_size;        // Window size
Vector2 center;             // Center of the window

// Player variables
Rectangle player_rect;      // The players bounding rectangle
bool died = false;          // If the player has died
float rotation = 0;         // Player rotation (degrees)
Texture2D player_tex[4];    // All of the players textures
int sprite = 0;             // The index of the players current texture

// Other textures
Texture2D coin;

// Scoring + money variables
long coins = 0;         // How many coins the player has
long score = 0;         // The players current score
bonus bonuses[] = {     // Moves that can grant the player coins
    {"Close call", 10},
    {"Double kill", 5},
    {"Triple kill", 5},
    {"Multi kill", 10},
    {"Milestone", 20},
    {"New enemy", 20}
};
bonus latest_bonus;     // The latest bonus gained by the player
double bonus_time = 2;  // How long to display the bonus
double kill_timer = 0;  // How long between kills (for kill based rewards)
int bonus_id;           // The id of the latest bonus

void get_bonus(int id) {
    bonus_id = id;
    latest_bonus = bonuses[id];
    bonus_time = 0;
    coins += latest_bonus.reward;
}

// Rotates a point around the 0,0 point
inline Vector2 rotate_point(Vector2 point, float rotation) {
    return (Vector2){
        cos(rotation) * point.x - sin(rotation) * point.y,
        sin(rotation) * point.x + cos(rotation) * point.y
    };
}

// Rotates a line around the 0,0 point
inline line rotate_line(line l, float rotation) {
    return (line) {
        rotate_point(l.a, rotation),
        rotate_point(l.b, rotation)
    };
}

// Checks if two lines are intersecting
inline bool line_collision(line a, line b) {
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
bool line_rect_collision(line a, Rectangle b) {
    // Turn the rect into an array of lines
    line rect[] = {
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
        if(line_collision(rect[i], a))
            return true;
    }
    return false;
}

// Enemy update method
void update_enemy(enemy * enemy_ptr, float deltat, float scale) {
    // Fade out a dieing enemy
    if(enemy_ptr->state == 1) {
        // Increment the timer
        enemy_ptr->timer += deltat;

        // Delete enemy when 0.5s is elapsed
        if(enemy_ptr->timer > 0.5f) {
            enemy_ptr->id = 0;

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
        enemy_ptr->state = 1;

    enemy_ptr->position.x += sin(enemy_ptr->rotation) * deltat * enemy_ptr->speed * scale;
    enemy_ptr->position.y += cos(enemy_ptr->rotation) * deltat * enemy_ptr->speed * scale;

    // Check collision
    if(CheckCollisionRecs(enemy_ptr->bounds, player_rect) && enemy_ptr->state != 2)
        died = true;
    
    // Check shield collision
    bool collide = false;
    for(int i = 0; i < MAX_COLLISION_LINES; ++i) {
        // Make sure the shields collision is rotated
        line rotated_line = rotate_line(current_shield.lines[i], (rotation + 270) * DEG2RAD);
        rotated_line.a.x *= scale;
        rotated_line.a.y *= scale;
        rotated_line.b.x *= scale;
        rotated_line.b.y *= scale;
        rotated_line.a.x += center.x;
        rotated_line.a.y += center.y;
        rotated_line.b.x += center.x;
        rotated_line.b.y += center.y;

        if(DEBUG)
            DrawLineEx(rotated_line.a, rotated_line.b, 1, BLUE);

        // Check if colliding with shield
        if(line_rect_collision(rotated_line, enemy_ptr->bounds)) {
            collide = true;
            break;
        }
    }
    if(collide) {
        if(enemy_ptr->id == 3 && enemy_ptr->state != 4) {
            if(enemy_ptr->state != 2)
                enemy_ptr->rotation = -enemy_ptr->rotation;
            enemy_ptr->state = 2;
        }
        else {
            enemy_ptr->state = 1;
            
            // Check for bonuses
            if(sqrtf(pow(center.x - enemy_ptr->position.x, 2) + pow(center.y - enemy_ptr->position.y, 2)) < scale * 3)
                get_bonus(0); // Close call

            if(kill_timer < 0.3) {
                if(bonus_time > 2)
                    get_bonus(1);
                else if(bonus_id == 1)
                    get_bonus(2);
                else if(bonus_id == 2 || bonus_id == 3)
                    get_bonus(3);
                else
                    get_bonus(1);
            }
            kill_timer = 0;
        }
    }

    // Per enemy types actions
    switch(enemy_ptr->id) {
        case 3:
            if(enemy_ptr->state == 2) {
                enemy_ptr->timer += deltat;
                enemy_ptr->rotation += deltat * PI / 2;
                if(enemy_ptr->timer >= 2) {
                    enemy_ptr->timer = 0;
                    enemy_ptr->state = 4;
                    enemy_ptr->rotation = atan2(center.x - enemy_ptr->position.x, center.y - enemy_ptr->position.y);
                }
            }
            break;
    }
}

void spawn_enemy(int id) {
    // Find a free space for the enemy
    int index;
    for(index = 0;index<MAX_ENEMIES;++index) {
        // Break when a free space is found
        if(enemies[index].id == 0)
            break;
    }

    // Calculate position
    Vector2 position = {
        GetRandomValue(0, window_size.x),
        GetRandomValue(0, window_size.y)
    };
    if (GetRandomValue(0, 1)) {
        // Horizontal wall
        position.x = GetRandomValue(0, 1) * window_size.x;
    }
    else {
        // Vertical wall
        position.y = GetRandomValue(0, 1) * window_size.y;
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

// The render method should contain all rendering code
void render(float scale, float deltat) {
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
            enemy_tex.width,
            enemy_tex.height
        };

        // Flip the sprite if looking left
        if(sin(enemies[i].rotation) < 0) {
            source = (Rectangle){
                enemy_tex.width * 2,
                0,
                -enemy_tex.width,
                enemy_tex.height
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
            enemy_tex, 
            source,
            enemies[i].bounds,
            (Vector2){0, 0},
            0,
            (Color){
                enemy_colors[enemies[i].id - 1].x,
                enemy_colors[enemies[i].id - 1].y,
                enemy_colors[enemies[i].id - 1].z,
                enemies[i].state == 1 ? (unsigned char)((0.5f - enemies[i].timer) * 510) : 255
            }
        );

        // Draw debug lines
        if(DEBUG)
            DrawRectangleLinesEx(enemies[i].bounds, 1, RED);
        
        // Update the enemy here to save on time
        update_enemy(&enemies[i], deltat, scale);
    }
    
    // Draw the player
    DrawTexturePro(
        player_tex[sprite], 
        (Rectangle){
            0,
            0,
            player_tex[sprite].width,
            player_tex[sprite].height
        },
        player_rect,
        (Vector2){0, 0},
        0,
        WHITE
    );

    // Draw the players shield
    DrawTexturePro(
        current_shield.texture,
        (Rectangle){
            0,
            0,
            current_shield.texture.width,
            current_shield.texture.height
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
        DrawRectangleLinesEx(player_rect, 1, GREEN);


    // Draw UI
    char str[255]; // Temp string for converting int to string
    sprintf(str, "%d", score);
    DrawText(str, scale / 2, scale / 2, scale * 2, BLACK);

    // Draw money
    sprintf(str, "%d", coins);
    DrawText(str, window_size.x - (TextLength(str) + 3) * scale, scale / 2, scale * 2, BLACK);
    DrawTextureEx(coin, (Vector2){window_size.x - scale * 2.5, scale / 1.9}, 0, scale / 5, WHITE);

    // Draw bonus
    if(bonus_time < 2) {
        // Convert the bonus' reward to as string
        sprintf(str, "%d", latest_bonus.reward);

        // Concatenate all the string together
        const char * tokens[] = {
            "+", str, " ", latest_bonus.name
        };
        const char * bonus_str = TextJoin(tokens, 4, "");

        // Draw the final string
        DrawText(
            bonus_str, 
            window_size.x - (TextLength(bonus_str)) * scale / 2, 
            scale * 2.5, scale, 
            (Color){200, 200, 200, (unsigned char)(255 - 120 * bonus_time)}
        );
    }
    bonus_time += deltat;
}

int main() {
    // The time of the last frame
    float old_time;

    // How many seconds until another enemy should spawn
    float spawn_time = 2;

    // Set the starting window size
    window_size = (Vector2){800, 500};

    // Init the window
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(window_size.x, window_size.y, "Block-Cycle");

    // Init all the shields
    shields[0] = (shield){ // Basic shield
        // Texture
        LoadTexture("resources/images/shield/basic.png"),
        // Collision lines
        {
            (line){{2.2, -1.8}, {2.2, 1.8}}
        }
    };

    // Set the default shield
    current_shield = shields[0];

    // Load all the player textures
    player_tex[0] = LoadTexture("resources/images/up.png");
    player_tex[1] = LoadTexture("resources/images/right.png");
    player_tex[2] = LoadTexture("resources/images/down.png");
    player_tex[3] = LoadTexture("resources/images/left.png");

    // Load al the enemy textures
    enemy_tex = LoadTexture("resources/images/enemies/enemy.png");
    
    // Load other textures
    coin = LoadTexture("resources/images/coin.png");

    // Scale of objects on the window
    float scale;

    // Time inbetween frames
    float deltat;

    // Main loop
    while(!WindowShouldClose()) {
        // Update window size
        window_size.x = GetRenderWidth();
        window_size.y = GetRenderHeight();

        // Update delta time
        deltat = GetFrameTime();

        // Get the window center
        center.x = window_size.x / 2;
        center.y = window_size.y / 2;

        // Update scale
        scale = sqrt(pow(window_size.x, 2) + pow(window_size.y, 2)) / 50;

        // Look at the mouse
        if(!died)
            rotation = 180 - round((atan2(GetMousePosition().x - center.x, GetMousePosition().y - center.y) / 3.1415)*180);

        // Get the players sprite index
        sprite = (int)round(rotation / 90) % 4;

        if(old_time / spawn_time < round(old_time / spawn_time) && GetTime() / spawn_time >= round(old_time / spawn_time)) {
            spawn_enemy(GetRandomValue(1, ENEMY_TYPES));
        }
        old_time = GetTime();

        // Increment the kill timer
        kill_timer += deltat;

        // If score is -1 then reset
        if(score == -1) {
            died = false;
            score = 0;

            // Remove all enemies
            for(int i = 0; i<MAX_ENEMIES; ++i)
                enemies[i].id = 0;
        }

        // Update the players bounding rectangle
        player_rect = (Rectangle){
            center.x - scale,
            center.y - scale,
            scale * 2,
            scale * 2
        };

        // Draw everything
        BeginDrawing();
        render(scale, deltat);
        EndDrawing();
    }

    // Unload everything and close the window
    for(int i = 0; i < 4; ++i)
        UnloadTexture(player_tex[i]);
    UnloadTexture(enemy_tex);
    
    CloseWindow();
}