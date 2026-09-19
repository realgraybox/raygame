/*
 * Copyright (C) 2026 M. Glargaard, aka graybox
 *
 * This software is provided "as-is", without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would
 *    be appreciated but is not required.
 *
 * 2. Altered source versions must be plainly marked as such, and must not
 *    be misrepresented as being the original software.
 *
 * 3. This notice may not be removed or altered from any source distribution.
 */

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "font8x8_basic.h"

#define MAX_MAP_SIZE  128   //32		
#define MAX_ROOMS_LIMIT   64	//32		
#define MAX_ENEMIES_LIMIT 64 //32
#define MAX_SCREEN_W    960 	//640
#define MAX_SCREEN_H   720  //480

typedef struct {
    int screenW;
    int screenH;
    int mapSize;
    int maxRooms;
    int maxEnemies;
    int startLevel;
    int noSound;
    int autoplay;
} Config;

void set_window_icon(Display *dpy, Window win) {
	
	//The mini-map / The maze (8x8 pixels)
	unsigned long icon_data[] = {
    8, 8, // width, height
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFF000033, 0xFF000033, 0xFFFFFFFF, 0xFF000033, 0xFF000033, 0xFF000033, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFF000033, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFF000033, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFF000033, 0xFF000033, 0xFF000033, 0xFF000033, 0xFFFFFFFF, 0xFF000033, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFF000033, 0xFFFFFFFF, 0xFFFFFFFF, 0xFF000033, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFF000033, 0xFF000033, 0xFF000033, 0xFF000033, 0xFF000033, 0xFF000033, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFF000033, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF
};
   
    int data_length = 2 + (8 * 8); // 2 (measure) + (width * height)

    // Find or create the correct Atom for netwm-icon
    Atom wm_icon = XInternAtom(dpy, "_NET_WM_ICON", False);
    Atom cardinal = XInternAtom(dpy, "CARDINAL", False);

    // Write the data to the window's property
    XChangeProperty(dpy, win, wm_icon, cardinal, 32, 
                    PropModeReplace, (unsigned char*)icon_data, data_length);
    XFlush(dpy); 
}

void parse_args(Config *cfg, int argc, char **argv) {
    for(int i = 1; i < argc; i++) {
        if((strcmp(argv[i], "-h") == 0) || (strcmp(argv[i], "--help") == 0)) {
            printf("Usage: %s [arguments]\n\n", argv[0]);
            printf("Available arguments:\n");
            printf("  --level <number>      Starting level (default: 1)\n");
            printf("  --screenw <number>    Screen width (Max: %d)\n", MAX_SCREEN_W);
            printf("  --screenh <number>    Screen height (Max: %d)\n", MAX_SCREEN_H);
            printf("  --mapsize <number>    Map size (Max: %d)\n", MAX_MAP_SIZE);
            printf("  --enemies <number>    Maximum number of enemies (Max: %d)\n", MAX_ENEMIES_LIMIT);
            printf("  --rooms <number>      Maximum number of rooms (Max: %d)\n", MAX_ROOMS_LIMIT);
            printf("  --nosound          	Disable sound in the game\n");
            printf("  --autoplay         	Enable automatic play mode (AI/bot)\n");
            printf("  -h, --help         	Show this menu and exit\n");
            printf("Ingame controls:\n");
            printf("  Key p: Pause / Key a: Automode / Key s: Toggle sound on/off\n");
            printf("  Arrow keys: Move left/right/forward/backward\n");
            exit(0); 
        }
        else if(strcmp(argv[i], "--level") == 0 && i + 1 < argc) {
            cfg->startLevel = atoi(argv[++i]);
        }
        else if(strcmp(argv[i], "--screenh") == 0 && i + 1 < argc) {
            cfg->screenH = atoi(argv[++i]);
        }
        else if(strcmp(argv[i], "--screenw") == 0 && i + 1 < argc) {
            cfg->screenW = atoi(argv[++i]);
        }
        else if(strcmp(argv[i], "--mapsize") == 0 && i + 1 < argc) {
            cfg->mapSize = atoi(argv[++i]);
        }
        else if(strcmp(argv[i], "--enemies") == 0 && i + 1 < argc) {
            cfg->maxEnemies = atoi(argv[++i]);
        }
        else if(strcmp(argv[i], "--rooms") == 0 && i + 1 < argc) {
            cfg->maxRooms = atoi(argv[++i]);
        }
        else if(strcmp(argv[i], "--nosound") == 0) {
            cfg->noSound = 1;
        }
        else if(strcmp(argv[i], "--autoplay") == 0) {
            cfg->autoplay = 1;
        }
    }
}

void validate_config(Config *cfg) {
    // Validate card size (between 32 and MAX)
    if(cfg->mapSize < 32)
        cfg->mapSize = 32;

    if(cfg->mapSize > MAX_MAP_SIZE)
        cfg->mapSize = MAX_MAP_SIZE;

    // Validate space (must not exceed mapSize*2 or the absolute maximum limit)
    // Also ensure that there is always at least one room.
    if(cfg->maxRooms < 1)
        cfg->maxRooms = 1;

    if(cfg->maxRooms > cfg->mapSize * 2)
        cfg->maxRooms = cfg->mapSize * 2;

    if(cfg->maxRooms > MAX_ROOMS_LIMIT)
        cfg->maxRooms = MAX_ROOMS_LIMIT;

    // Validate enemies (between 0 and MAX)
    if(cfg->maxEnemies < 0)
        cfg->maxEnemies = 0;

    if(cfg->maxEnemies > MAX_ENEMIES_LIMIT)
        cfg->maxEnemies = MAX_ENEMIES_LIMIT;
        
    // Validate screen width (between 320 and MAX)
    if(cfg->screenW < 320)
        cfg->screenW = 320;

    if(cfg->screenW > MAX_SCREEN_W)
        cfg->screenW = MAX_SCREEN_W;

    // Validate screen height (between 240 and MAX)
	if(cfg->screenH < 240)
        cfg->screenH = 240;

    if(cfg->screenH > MAX_SCREEN_H)
        cfg->screenH = MAX_SCREEN_H;
        
    // Validate start level (cannot be less than 1)
    if(cfg->startLevel < 1)
        cfg->startLevel = 1;
}


#define FOV 0.66
#define TARGET_FPS 30.0		//60.0
#define PLAYER_MOVE_SPEED 3.0
#define PLAYER_ROT_SPEED  2.0
#define ENEMY_SPEED       0.7
#define ENEMY_RADIUS      0.2
#define ENEMY_DAMAGE_DIST 0.5
#define CANDLE_DRAIN_PER_SECOND 0.15
#define TILE_EMPTY 0
#define TILE_WALL  1
#define TILE_EXIT  2
#define SKY_TOP    0x1a2d50
#define SKY_BOTTOM 0x506090
#define FLOOR_A    0x303030
#define FLOOR_B    0x505050

typedef struct {
    double x;
    double y;
} Vec2;

typedef struct {
    double x;
    double y;
    double dirX;
    double dirY;
    double planeX;
    double planeY;
} Player;

typedef struct {
    int x;
    int y;
    int w;
    int h;
} Room;

typedef struct {
    double x;
    double y;
    double spawnX;
    double spawnY;
    
    double alert;
    int alive;
    double pingCooldown;
} Enemy;

typedef struct {
    Display *display;
    Window window;
    GC gc;
    uint32_t *framebuffer;
    XImage *image;
    int width;
    int height;   
    Atom wm_delete_window;
} Renderer;

typedef struct {
    int up;
    int down;
    int left;
    int right;
} Input;

#define MAX_PATH_LENGTH 512
typedef struct { 
	int x, y; 
} Waypoint;


typedef struct {
	Config config;

    Renderer renderer;

    Player player;

    Enemy enemies[MAX_ENEMIES_LIMIT];
    int enemyCount;

    Room rooms[MAX_ROOMS_LIMIT];
    int roomCount;

    int map[MAX_MAP_SIZE][MAX_MAP_SIZE];

    double zbuffer[MAX_SCREEN_W];

    double candle;
	int waitingForNextLevel;
	int replay_level;
    int level;
    int levelComplete;
    double overlayTimer;

    Input input;
    
    int exitX;
	int exitY;
	int autoplay;
	double autoTurnTimer;
	double autoTurnSpeed;
	double stuckTimer;
	double lastX;
	double lastY;
	
	int introMode;
	int soundEnabled;
	int paused;
	int running; 
	double audioTimer;

	Waypoint path[MAX_PATH_LENGTH];	//pathfinder
	int pathLength;	//pathfinder
	int currentWaypointIndex;	//pathfinder
	

} Game;
#ifdef USE_SOUND
#include "sound.c"
SoundState sound;
#endif

// pathfinder
#include <string.h>

// Support structure for BFS
typedef struct { int x, y; } Point;

int find_path_bfs(Game *g, int startX, int startY, int targetX, int targetY, Waypoint *outPath) {
    int mapSize = g->config.mapSize;
    
    // Arrays to keep track of visited cells and where we came from (-1 means unvisited)
    // Uses static allocation based on a maximum of 128x128 to avoid malloc in every frame
    static int parentX[128][128];
    static int parentY[128][128];
    memset(parentX, -1, sizeof(parentX));
    memset(parentY, -1, sizeof(parentY));

    // A simple queue for BFS
    static Point queue[128 * 128];
    int head = 0, tail = 0;

    // Starting point
    queue[tail++] = (Point){startX, startY};
    parentX[startY][startX] = startX; // Mark as visited
    parentY[startY][startX] = startY;

    int found = 0;
    int dx[] = {0, 0, -1, 1}; // Directions: Up, Down, Left, Right
    int dy[] = {-1, 1, 0, 0};

    while (head < tail) {
        Point current = queue[head++];

        if (current.x == targetX && current.y == targetY) {
            found = 1;
            break;
        }

        for (int i = 0; i < 4; i++) {
            int nx = current.x + dx[i];
            int ny = current.y + dy[i];

            // Check the map boundaries and whether it is a wall.
            if (nx >= 0 && nx < mapSize && ny >= 0 && ny < mapSize) {
                if (g->map[ny][nx] != TILE_WALL && parentX[ny][nx] == -1) {
                    parentX[ny][nx] = current.x;
                    parentY[ny][nx] = current.y;
                    queue[tail++] = (Point){nx, ny};
                }
            }
        }
    }

    if (!found) return 0; //No route found (e.g., enclosed space)

    // Reconstruct the route in reverse.
    Waypoint tempPath[128 * 128];
    int tempLength = 0;
    int cx = targetX, cy = targetY;

    while (cx != startX || cy != startY) {
        tempPath[tempLength++] = (Waypoint){cx, cy};
        int px = parentX[cy][cx];
        int py = parentY[cy][cx];
        cx = px;
        cy = py;
    }

    // Reverse the route so it runs from start to finish, and save it to outPath.
    int count = 0;
    for (int i = tempLength - 1; i >= 0 && count < MAX_PATH_LENGTH; i--) {
        outPath[count++] = tempPath[i];
    }

    return count; // Returns the length of the route
}
//end pathfinder

static inline uint32_t rgb(int r, int g, int b) {
    return (((uint32_t)(r & 255)) << 16) | (((uint32_t)(g & 255)) << 8) | (uint32_t)(b & 255);
}

static inline void pset(Renderer *r, int x, int y, uint32_t c) {
    if(x < 0 || x >= r->width || y < 0 || y >=r->height)
        return;

    r->framebuffer[y * r->width + x] = c;
}

static inline int map_at(Game *g, int x, int y) {
    if(x < 0 || y < 0 || x >= g->config.mapSize || y >= g->config.mapSize)
        return TILE_WALL;

    return g->map[y][x];
}

void set_player_direction(Player *p, double x, double y) {
    p->dirX = x;
    p->dirY = y;
    p->planeX = y * FOV;
    p->planeY = -x * FOV;
}

void clear_map(Game *g) {
    for(int y = 0; y < g->config.mapSize; y++) {
        for(int x = 0; x < g->config.mapSize; x++) {
            g->map[y][x] = TILE_WALL;
        }
    }
}

void carve_room(Game *g, int x, int y, int w, int h) {
    for(int yy = y; yy < y + h; yy++) {
        for(int xx = x; xx < x + w; xx++) {
            g->map[yy][xx] = TILE_EMPTY;
        }
    }
}

// Simplified and faster corridor carving with fewer conditions in the loop.
void carve_corridor(Game *g, int x1, int my1, int x2, int y2) {
    int x = x1;
    int y = my1;

    int stepX = (x2 > x1) ? 1 : -1;
    int stepY = (y2 > my1) ? 1 : -1;

    while (x != x2) {
        g->map[y][x] = TILE_EMPTY;
        x += stepX; // Constant evaluation of (x2 > x1) removed from the loop
    }

    while (y != y2) {
        g->map[y][x] = TILE_EMPTY;
        y += stepY; // Constant evaluation of (y2 > my1) removed from the loop
    }
}

void restart_level(Game *g) {
    Room *start = &g->rooms[0];

    g->player.x = start->x + start->w / 2;
    g->player.y = start->y + start->h / 2;

    set_player_direction(&g->player, -1.0, 0.0);

    g->candle = 1.0;

    for(int i = 0; i < g->enemyCount; i++) {
        Enemy *e = &g->enemies[i];

        e->x = e->spawnX;
        e->y = e->spawnY;

        e->alert = 0.0;
        e->alive = 1;
    }
}

void generate_level(Game *g) {
	
    g->roomCount = 0;

    clear_map(g);

    for(int r = 0; r < g->config.maxRooms; r++) {
        int minRoom = g->config.mapSize / 16;
		int maxRoom = g->config.mapSize / 8;

		if(minRoom < 4) minRoom = 4;
		if(maxRoom < 6) maxRoom = 6;
		
		int w = minRoom + rand() % (maxRoom - minRoom + 1);
        
        int h = 4 + rand() % 6;

        int x = rand() % (g->config.mapSize - w - 2) + 1;
        int y = rand() % (g->config.mapSize - h - 2) + 1;

        int valid = 1;

        for(int yy = y - 1; yy < y + h + 1; yy++) {
            for(int xx = x - 1; xx < x + w + 1; xx++) {
                if(map_at(g, xx, yy) == TILE_EMPTY) {
                    valid = 0;
                }
            }
        }

        if(!valid)
            continue;

        carve_room(g, x, y, w, h);

        g->rooms[g->roomCount++] = (Room){x, y, w, h};

        if(g->roomCount >= g->config.maxRooms)
            break;
    }

    if(g->roomCount == 0) {
        generate_level(g);
        return;
    }

    for(int i = 1; i < g->roomCount; i++) {
        int x1 = g->rooms[i - 1].x + g->rooms[i - 1].w / 2;
        int my1 = g->rooms[i - 1].y + g->rooms[i - 1].h / 2;

        int x2 = g->rooms[i].x + g->rooms[i].w / 2;
        int y2 = g->rooms[i].y + g->rooms[i].h / 2;

        carve_corridor(g, x1, my1, x2, y2);
    }

    Room *start = &g->rooms[0];
    Room *end   = &g->rooms[g->roomCount - 1];

    g->player.x = start->x + start->w / 2;
    g->player.y = start->y + start->h / 2;

    set_player_direction(&g->player, -1.0, 0.0);

    g->exitX = end->x + end->w / 2;
	g->exitY = end->y + end->h / 2;
    g->map[g->exitY][g->exitX] = TILE_EXIT;

    g->enemyCount = 0;
    
    int targetEnemies = 3 + g->level * 2;
	if(targetEnemies > g->config.maxEnemies)
		targetEnemies = g->config.maxEnemies;
		
    for(int i = 1; i < g->roomCount && g->enemyCount < targetEnemies; i++) {
        Enemy *e = &g->enemies[g->enemyCount++];

        e->x = g->rooms[i].x + rand() % g->rooms[i].w;
        e->y = g->rooms[i].y + rand() % g->rooms[i].h;
        
        e->spawnX = e->x;
		e->spawnY = e->y;

        e->alert = 0.0;
        e->alive = 1;
    }

    g->candle = 1.0;
}

int can_see_player(Game *g, double ex, double ey) {
    double vx = g->player.x - ex;
    double vy = g->player.y - ey;

    double dist = sqrt(vx * vx + vy * vy);

    if(dist < 0.0001)
        return 1;

    vx /= dist;
    vy /= dist;

    double x = ex;
    double y = ey;

    for(double t = 0; t < dist; t += 0.1) {
        x += vx * 0.1;
        y += vy * 0.1;

        if(map_at(g, (int)x, (int)y) == TILE_WALL)
            return 0;
    }

    return 1;
}

// optimized wall_distance
double wall_distance(Game *g, double angleOffset) {
    Player *p = &g->player;

    // Rotate the player's direction vector with angleOffset
    double cosA = cos(angleOffset);
    double sinA = sin(angleOffset);
    
    double rayX = p->dirX * cosA - p->dirY * sinA;
    double rayY = p->dirX * sinA + p->dirY * cosA;

    // Current area on the map
    int mapX = (int)p->x;
    int mapY = (int)p->y;

    //Distance from one x/y-line to the next x/y-line
    double deltaX = (rayX == 0.0) ? 1e30 : fabs(1.0 / rayX);
    double deltaY = (rayY == 0.0) ? 1e30 : fabs(1.0 / rayY);

    double sideDistX;
    double sideDistY;

    int stepX;
    int stepY;

    // Initialize direction and side clearances
    if(rayX < 0) {
        stepX = -1;
        sideDistX = (p->x - mapX) * deltaX;
    } else {
        stepX = 1;
        sideDistX = (mapX + 1.0 - p->x) * deltaX;
    }

    if(rayY < 0) {
        stepY = -1;
        sideDistY = (p->y - mapY) * deltaY;
    } else {
        stepY = 1;
        sideDistY = (mapY + 1.0 - p->y) * deltaY;
    }

    // DDA raycast loop
    int side = 0;
    int hit = 0;
    double max_dist = 6.0; // Maintains previous maximum range.

    while(!hit) {
        if(sideDistX < sideDistY) {
            sideDistX += deltaX;
            mapX += stepX;
            side = 0;
        } else {
            sideDistY += deltaY;
            mapY += stepY;
            side = 1;
        }

        // Check if we are out of bounds.
        if(mapX < 0 || mapY < 0 || mapX >= g->config.mapSize || mapY >= g->config.mapSize) {
            break;
        }

        // Check if the ray hits a wall using your map_at function.
        if(map_at(g, mapX, mapY) == TILE_WALL) {
            hit = 1;
        }

        // Calculate the preliminary distance to comply with the max_dist limit.
        double currentDist = (side == 0) ? (sideDistX - deltaX) : (sideDistY - deltaY);
        if(currentDist > max_dist) {
            return max_dist;
        }
    }

    if(!hit) {
        return max_dist;
    }

    // Calculate the precise perpendicular distance to the wall.
    double dist;
    if(side == 0) {
        dist = (mapX - p->x + (1 - stepX) / 2.0) / rayX;
    } else {
        dist = (mapY - p->y + (1 - stepY) / 2.0) / rayY;
    }

    return (dist < 0.001) ? 0.001 : dist;
}

void rotate_player(Game *g, double angle) {
	
	Player *p = &g->player;
	
    double oldDirX = p->dirX;

     p->dirX = p->dirX * cos(angle) - p->dirY * sin(angle);
     p->dirY = oldDirX * sin(angle) + p->dirY * cos(angle);

    double oldPlaneX = p->planeX;

    p->planeX = p->planeX * cos(angle) - p->planeY * sin(angle);
    p->planeY = oldPlaneX * sin(angle) + p->planeY * cos(angle);
}

#ifdef ORIG
void autoplay_update(Game *g, double dt) {
    Player *p = &g->player;

    double moveSpeed = 1.5 * dt;
    double dx = p->x - g->lastX;
	double dy = p->y - g->lastY;
	double moved = dx*dx + dy*dy;
	
	g->lastX = p->x;
	g->lastY = p->y; 
	
	if(moved < 0.00001) {
			g->stuckTimer += dt;
	} else {
			g->stuckTimer = 0;
	}
	if(g->stuckTimer > 0.25) {
		// rotate a fixed amount (always left) instead of random
        rotate_player(g, 0.8);   // about 45 degrees
		// nudge sideways (perpendicular to direction)
        p->x += p->dirY * 0.3;   // left/right depends on coordinate system
        p->y -= p->dirX * 0.3;
		g->stuckTimer = 0;
		 // reset wall logic so it doesn't instantly re-trigger
		g->autoTurnTimer = 0.0;
		return;
	}
	
    // continue wall-escape turning
    if(g->autoTurnTimer > 0.0) {
		rotate_player(g, g->autoTurnSpeed * dt);   // always turn same direction
        g->autoTurnTimer -= dt;
    }
    // look-ahead collision check
    double lookAhead = 1.2;
    double sideOffset = 0.4;
    double tx[3], ty[3];
    
    tx[0] = p->x + p->dirX * lookAhead;
    ty[0] = p->y + p->dirY * lookAhead;
    tx[1] = p->x + p->dirX * lookAhead + p->dirY * sideOffset;
    ty[1] = p->y + p->dirY * lookAhead - p->dirX * sideOffset;
    tx[2] = p->x + p->dirX * lookAhead - p->dirY * sideOffset;
    ty[2] = p->y + p->dirY * lookAhead + p->dirX * sideOffset;

    // Only the center point decides if we need to turn
	int cx = (int)tx[0], cy = (int)ty[0];
	if (cx >= 0 && cx < g->config.mapSize  && cy >= 0 && cy < g->config.mapSize ) {
		if (g->map[cy][cx] == TILE_WALL) {
			double leftOpen =	wall_distance(g, -0.8);
			double rightOpen = wall_distance(g, 0.8);

			if(leftOpen > rightOpen) {
				g->autoTurnSpeed = -1.5;
			} else {
				g->autoTurnSpeed = 1.5;	
			}
			g->autoTurnTimer = 0.2 + (rand() % 100) / 500.0;
			return;
		}
	}

	// Center is free – move forward (ignore side points)
	double nx = p->x + p->dirX * moveSpeed;
	double ny = p->y + p->dirY * moveSpeed;
	int mmx = (int)nx, mmy = (int)ny;
	if (mmx >= 0 && mmx < g->config.mapSize && mmy >= 0 && mmy < g->config.mapSize
		&& g->map[mmy][mmx] != TILE_WALL) {
		p->x = nx;
		p->y = ny;
	}
}
#else

void autoplay_update(Game *g, double dt) {
    Player *p = &g->player;
    int px = (int)p->x;
    int py = (int)p->y;

    // 1. GENERATE ROUTE IF NECESSARY
    int needNewPath = (g->pathLength == 0);
    
    if (!needNewPath && g->currentWaypointIndex < g->pathLength) {
        Waypoint currentWP = g->path[g->currentWaypointIndex];
        int distToExpectedCell = abs(px - currentWP.x) + abs(py - currentWP.y);
        if (distToExpectedCell > 2) needNewPath = 1; 
    }

    if (needNewPath) {
        int targetX = g->exitX;
        int targetY = g->exitY;
        g->pathLength = find_path_bfs(g, px, py, targetX, targetY, g->path);
        g->currentWaypointIndex = 0;
        if (g->pathLength == 0) return;
    }

    if (g->currentWaypointIndex >= g->pathLength) {
        g->pathLength = 0; 
        return;
    }

    // 2. GET NEXT MILESTONE
    Waypoint wp = g->path[g->currentWaypointIndex];
    double targetX = wp.x + 0.5;
    double targetY = wp.y + 0.5;

    double dx = targetX - p->x;
    double dy = targetY - p->y;
    double dist = sqrt(dx * dx + dy * dy);

    if (dist < 0.35) {
        g->currentWaypointIndex++;
        return;
    }

	// 3. DYNAMIC SENSITIVITY BASED ON HEALTH
    // Assumes max health is 100. If health is high, the bot reacts less frantically.
    double healthPercent = (double)g->candle / 100.0;
    if (healthPercent > 1.0) healthPercent = 1.0;
    if (healthPercent < 0.1) healthPercent = 0.1; // Undgå total ligegyldighed hvis meget lavt

    // Adjust the danger zone and strength based on health:
    // High health = smaller danger zone (e.g., 1.5 tiles) and weaker knockback
    // Low health = large danger zone (e.g., 2.8 tiles) and aggressive evasion
    double dangerRadius = 1.6 - (healthPercent * 0.8); 
    double evadeStrength = 1.8 - (healthPercent * 1.4);

	// 4. POTENTIAL FIELDS (Calculation of forces)
    double targetMag = sqrt(dx * dx + dy * dy);
    double desiredX = (targetMag > 0) ? (dx / targetMag) : 0;
    double double_desiredY = (targetMag > 0) ? (dy / targetMag) : 0;

	double avoidX = 0;
	double avoidY = 0;

	for (int i = 0; i < g->enemyCount; i++) {
		if (!g->enemies[i].alive) continue;

		double edx = p->x - g->enemies[i].x;
		double edy = p->y - g->enemies[i].y;
		double eDist = sqrt(edx * edx + edy * edy);

		if (eDist < dangerRadius && eDist > 0.05) {
			double force = (dangerRadius - eDist) / dangerRadius;
			
			// --- 1-TILE CORRIDOR DETECTION & OPTIMIZATION ---
			// We check if the bot is locked in a horizontal or vertical corridor
			int ipx = (int)p->x;
			int ipy = (int)p->y;
			
			int wallLeft  = (g->map[ipy][ipx - 1] == TILE_WALL);
			int wallRight = (g->map[ipy][ipx + 1] == TILE_WALL);
			int wallUp    = (g->map[ipy - 1][ipx] == TILE_WALL);
			int wallDown  = (g->map[ipy + 1][ipx] == TILE_WALL);

			double pushX = edx / eDist;
			double pushY = edy / eDist;

			if (wallLeft && wallRight) {
				// The bot is in a vertical corridor (1 cell wide).
				// Completely ignore x-repulsion (the walls keep us on track).
				pushX = 0; 
			} else if (wallUp && wallDown) {
				// The bot is in a horizontal corridor (1 tile wide).
				// Completely ignore y-repulsion.
				pushY = 0;
			}

			avoidX += pushX * force * evadeStrength;
			avoidY += pushY * force * evadeStrength;
		}
	}


    // Combine route and evasion
    double finalMoveX = desiredX + avoidX;
    double finalMoveY = double_desiredY + avoidY;

    // 5. SMOOTH ANGLE INTERPOLATION (Removes the jerky look)
    double actualMoveMag = sqrt(finalMoveX * finalMoveX + finalMoveY * finalMoveY);
    double targetAngle = (actualMoveMag > 0.1) ? atan2(finalMoveY, finalMoveX) : atan2(dy, dx);
    double currentAngle = atan2(p->dirY, p->dirX);
    
    double angleDiff = targetAngle - currentAngle;
    while (angleDiff < -M_PI) angleDiff += 2 * M_PI;
    while (angleDiff >  M_PI) angleDiff -= 2 * M_PI;

    // Slightly increased rotation speed to match the new movement speed
    double rotationSpeed = 3.5 * dt; 
    
    // Threshold: If the change is minimal (less than ~3 degrees), do not shake the camera.
    if (fabs(angleDiff) > 0.06) {
        if (angleDiff > 0) {
            rotate_player(g, rotationSpeed);
        } else {
            rotate_player(g, -rotationSpeed);
        }
    }

    // 6. MOVE THE BOT (Faster speed)
    if (fabs(angleDiff) < M_PI / 2.0) {
        // Increased base speed from 1.5 to 2.2 to make it more agile
        double moveSpeed = /*2.2*/PLAYER_MOVE_SPEED * dt; 

        if (actualMoveMag > 0.001) {
            finalMoveX = (finalMoveX / actualMoveMag) * moveSpeed;
            finalMoveY = (finalMoveY / actualMoveMag) * moveSpeed;
        }

        double nx = p->x + finalMoveX;
        double ny = p->y + finalMoveY;

        int nmx = (int)nx;
        int nmy = (int)ny;
        int pmx = (int)p->x;
        int pmy = (int)p->y;

        int moved = 0;
        if (g->map[pmy][nmx] != TILE_WALL) {
            p->x = nx;
            moved = 1;
        }
        if (g->map[nmy][pmx] != TILE_WALL) {
            p->y = ny;
            moved = 1;
        }

        if (!moved && actualMoveMag > 0.1) {
            g->pathLength = 0;
        }
    }
}
#endif

void update_player(Game *g, double dt) {
    Player *p = &g->player;

    double moveSpeed = PLAYER_MOVE_SPEED * dt;
    double rotSpeed  = PLAYER_ROT_SPEED * dt;

    if(g->input.up) {
        double nx = p->x + p->dirX * moveSpeed;
        double ny = p->y + p->dirY * moveSpeed;

        if(map_at(g, (int)nx, (int)p->y) != TILE_WALL)
            p->x = nx;

        if(map_at(g, (int)p->x, (int)ny) != TILE_WALL)
            p->y = ny;
    }

    if(g->input.down) {
        double nx = p->x - p->dirX * moveSpeed;
        double ny = p->y - p->dirY * moveSpeed;

        if(map_at(g, (int)nx, (int)p->y) != TILE_WALL)
            p->x = nx;

        if(map_at(g, (int)p->x, (int)ny) != TILE_WALL)
            p->y = ny;
    }

    if(g->input.left) {
		rotate_player(g, rotSpeed);
	}
	
	 if(g->input.right) {
		 rotate_player(g, -rotSpeed);
	 } 
}

void update_enemies(Game *g, double dt) {
    for(int i = 0; i < g->enemyCount; i++) {
        Enemy *e = &g->enemies[i];

        if(!e->alive)
            continue;

        int visible = can_see_player(g, e->x, e->y);

        if(visible) {
            e->alert = 2.0;
            e->pingCooldown -= dt;
			if(e->pingCooldown <= 0.0) {
#ifdef USE_SOUND            
				snd.ping.timer = 0.4;		//snd.playerspot.timer = 0.1;
#endif
				e->pingCooldown = 2.0 + (rand() % 300) / 100.0;
			}
		} else {
			 e->pingCooldown = 0.0;
		}
		
        e->alert -= dt;

        if(e->alert <= 0.0)
            continue;

        double vx = g->player.x - e->x;
        double vy = g->player.y - e->y;

        double distSq = vx * vx + vy * vy;

        if(distSq < ENEMY_DAMAGE_DIST * ENEMY_DAMAGE_DIST) {
            g->candle -= CANDLE_DRAIN_PER_SECOND * dt;
            //printf("candle %f\n",g->candle);
#ifdef USE_SOUND			
			snd.damage.timer = 0.25;
#endif			
            if(g->candle < 0.0)
                g->candle = 0.0;

            if(g->candle <= 0.0) {
				if (!g->autoplay) {
				g->levelComplete = 1;
				g->waitingForNextLevel = 1;
				g->replay_level =1;
			} else {
				restart_level(g);	//skip overlay if in automode
			}
            return;
            }
        }

        double dist = sqrt(distSq);

        if(dist < 0.001)
            continue;

        vx /= dist;
        vy /= dist;
        
		double enemySpeed =  ENEMY_SPEED + g->level * 0.08;
		if(enemySpeed > 2.5) enemySpeed = 2.5;
		
        double nx = e->x + vx * enemySpeed * dt;
        double ny = e->y + vy * enemySpeed * dt;

        if(map_at(g, (int)nx, (int)e->y) == TILE_EMPTY)
            e->x = nx;

        if(map_at(g, (int)e->x, (int)ny) == TILE_EMPTY)
            e->y = ny;
    }
}

void update_game(Game *g, double dt) {
    if(g->levelComplete)
        return;

	if(g->autoplay || g->introMode) 
		autoplay_update(g,dt);
	else
		update_player(g, dt);

    update_enemies(g, dt);

    int tile = map_at(g, (int)g->player.x, (int)g->player.y);

    if(!g->levelComplete && tile == TILE_EXIT)	{
			g->level++;
			printf("DBG: Level %d\n", g->level);
			if (!g->autoplay) {
				g->levelComplete = 1;
				g->waitingForNextLevel = 1;
			} else {
				generate_level(g);
			}
	}	  
}

void draw_sky(Renderer *r, double candle) {
    int halfHeight = r->height / 2;

    for(int y = 0; y < halfHeight; y++) {
        double t = (double)y / halfHeight;

       // Precalculate the color once per horizontal line
        int rcol = (int)((20 + t * 40) * candle);
        int gcol = (int)((40 + t * 50) * candle);
        int bcol = (int)((80 + t * 60) * candle);
        
        uint32_t c = rgb(rcol, gcol, bcol);

        // Get the pointer to the start of this specific line (y) in the framebuffer
        uint32_t *fb_ptr = &r->framebuffer[y * r->width];

        // Write directly and sequentially to memory
        for(int x = 0; x < r->width; x++) {
            *fb_ptr++ = c;
        }
    }
}

//optimized without pset
void draw_floor(Game *g) {
    Renderer *r = &g->renderer;
    Player *p = &g->player;

    double rayDirX0 = p->dirX - p->planeX;
    double rayDirY0 = p->dirY - p->planeY;

    double rayDirX1 = p->dirX + p->planeX;
    double rayDirY1 = p->dirY + p->planeY;

    const int rA = (FLOOR_A >> 16) & 255;
    const int gA = (FLOOR_A >> 8) & 255;
    const int bA = FLOOR_A & 255;

    const int rB = (FLOOR_B >> 16) & 255;
    const int gB = (FLOOR_B >> 8) & 255;
    const int bB = FLOOR_B & 255;

    double invScreenW = 1.0 / g->config.screenW;

    for(int y = g->config.screenH / 2 + 1; y < g->config.screenH; y++) {
        int pY = y - g->config.screenH / 2;

        double posZ = 0.5 * g->config.screenH;
        double rowDist = posZ / pY;

        double stepX = rowDist * (rayDirX1 - rayDirX0) * invScreenW;
        double stepY = rowDist * (rayDirY1 - rayDirY0) * invScreenW;

        double floorX = p->x + rowDist * rayDirX0;
        double floorY = p->y + rowDist * rayDirY0;

        double fogFactor = (1.0 / (1.0 + rowDist * 0.12)) * g->candle;

        uint32_t final_rA = (uint32_t)(rA * fogFactor);
        uint32_t final_gA = (uint32_t)(gA * fogFactor);
        uint32_t final_bA = (uint32_t)(bA * fogFactor);

        uint32_t final_rB = (uint32_t)(rB * fogFactor);
        uint32_t final_gB = (uint32_t)(gB * fogFactor);
        uint32_t final_bB = (uint32_t)(bB * fogFactor);

        uint32_t colorA = (final_rA << 16) | (final_gA << 8) | final_bA;
        uint32_t colorB = (final_rB << 16) | (final_gB << 8) | final_bB;

        // 1. Initialize the pointer to the start of this horizontal row (x = 0)
        uint32_t *fb_ptr = &r->framebuffer[y * r->width];

        for(int x = 0; x < g->config.screenW; x++) {
            int cellX = (int)floorX;
            int cellY = (int)floorY;
            
            // Determine how close we are to the edge of a map tile.
			// We examine the fractional parts of the raw floating-point coordinates.
			double fx = floorX - cellX;
			double fy = floorY - cellY;
			uint32_t c;
			// If we are very close to the edge of a field, we draw a dark joint.
			int is_gulv_fuge = (fx < 0.03 || fx > 0.97 || fy < 0.03 || fy > 0.97);
			
			if (is_gulv_fuge) {
				c = 0x111111; //Almost black grout (also affected by the mist afterwards)
			} else {
				c = ((cellX + cellY) & 1) ? colorA : colorB; // Your original shades of gray
			}
			

            //uint32_t c = ((cellX + cellY) & 1) ? colorA : colorB;

            // 2. Write directly to memory and move the pointer one pixel to the right (++)
            *fb_ptr++ = c;

            floorX += stepX;
            floorY += stepY;
        }
    }
}

//optimized version
void render_walls(Game *g) {
    Renderer *r = &g->renderer;
    Player *p = &g->player;
    int screenW = g->config.screenW;
    int screenH = g->config.screenH;

    for(int x = 0; x < screenW; x++) {
        double cam = 2.0 * x / (double)screenW - 1.0;

        double rayX = p->dirX + p->planeX * cam;
        double rayY = p->dirY + p->planeY * cam;

        int mapX = (int)p->x;
        int mapY = (int)p->y;

        double deltaX = (rayX == 0.0) ? 1e30 : fabs(1.0 / rayX);
        double deltaY = (rayY == 0.0) ? 1e30 : fabs(1.0 / rayY);

        double sideDistX;
        double sideDistY;

        int stepX;
        int stepY;

        if(rayX < 0) {
            stepX = -1;
            sideDistX = (p->x - mapX) * deltaX;
        } else {
            stepX = 1;
            sideDistX = (mapX + 1.0 - p->x) * deltaX;
        }

        if(rayY < 0) {
            stepY = -1;
            sideDistY = (p->y - mapY) * deltaY;
        } else {
            stepY = 1;
            sideDistY = (mapY + 1.0 - p->y) * deltaY;
        }

        int hit = 0;
        int side = 0;
        
        // New and lightning-fast DDA loop in render_walls without external function calls:
        int mapSize = g->config.mapSize;
        int tile;
		while(!hit) {
            if(sideDistX < sideDistY) {
                sideDistX += deltaX;
                mapX += stepX;
                side = 0;
            } else {
                sideDistY += deltaY;
                mapY += stepY;
                side = 1;
            }

           // Security check integrated directly (or remove entirely if your map always has outer walls)
            if(mapX < 0 || mapY < 0 || mapX >= mapSize || mapY >= mapSize) {
                tile = TILE_WALL;
                hit = 1;
                break;
            }

            // Direct access to the 2D array in memory (no function overhead)
            if(g->map[mapY][mapX] != TILE_EMPTY) {
                hit = 1;
            }
        }
        
        tile = (mapX < 0 || mapY < 0 || mapX >= mapSize || mapY >= mapSize) ? TILE_WALL : g->map[mapY][mapX];
        
        double dist;

        if(side == 0) {
            dist = (mapX - p->x + (1 - stepX) / 2.0) / rayX;
        } else {
            dist = (mapY - p->y + (1 - stepY) / 2.0) / rayY;
        }

        if(dist < 0.001) {
            dist = 0.001;
        }

        g->zbuffer[x] = dist;

        int lineH = (int)(screenH / dist);
        int start = -lineH / 2 + screenH / 2;
        int end = lineH / 2 + screenH / 2;

        if(start < 0) start = 0;
        if(end >= screenH) end = screenH - 1;

        // 1. PRE-CALCULATION: Fog and light factors apply to the ENTIRE strip, so they are calculated here once.
        double fog = 1.0 / (1.0 + dist * 0.15);
        double totalLightFactor = fog * g->candle;

        double wallX = (side == 0) ? (p->y + dist * rayY) : (p->x + dist * rayX);
        wallX -= floor(wallX);
        int texX = (int)(wallX * 64);

        // 2. PRE-CALCULATION: Extract the color logic from the loop.
        // We prepare the base color 'base_c' and the shadow color 'side' upfront.
        uint32_t base_c;
        if(tile == TILE_WALL) {
            base_c = 0x888888; // Primary color
        } else {
            base_c = 0xff4444; // Alternative wall (e.g. door/red wall)
        }

       // If we hit a Y-side, we halve the color components beforehand (side shading)
        if(side) {
            base_c = (base_c >> 1) & 0x7f7f7f;
        }

        // Find the raw RGB values ​​for the selected wall type
        int base_r = (base_c >> 16) & 255;
        int base_g = (base_c >> 8) & 255;
        int base_b = base_c & 255;

       // Also prepare the secondary pattern color in case the procedure check fails
        uint32_t alt_c = (tile == TILE_WALL) ? 0x444444 : 0xaa0000;
        if(side) {
            alt_c = (alt_c >> 1) & 0x7f7f7f;
        }
        int alt_r = (alt_c >> 16) & 255;
        int alt_g = (alt_c >> 8) & 255;
        int alt_b = alt_c & 255;

        // Pre-calculating the framebuffer pointer to the bottom of this column
        uint32_t *fb_ptr = &r->framebuffer[start * r->width + x];
        int fb_stride = r->width; // How many elements do we have to move down to hit the next y?
        
        // CALCULATE THIS OUTSIDE THE Y-LOOP:
		// How much texY changes per screen pixel (in 32-bit fixed-point)
		double texStep = 64.0 / (double)lineH;
		// Starting position for the texture (handles cases where the wall is cut off at the top)
		double texPos = (start - screenH / 2 + lineH / 2) * texStep;

        // 3. THE OPTIMIZED PIXEL LOOP
        // No if-checks for screen boundaries (we know they are valid here)
        // No heavy floating-point multiplications involving per-pixel fog
   
#ifdef ORIG     
        for(int y = start; y < end; y++) {
			// Cast directly to int (equivalent to the previous division, but costs almost nothing)
			int texY = (int)texPos & 63; 
			texPos += texStep; // Add the step for the next pixel
			
			int use_base = (tile == TILE_WALL) ? ((texX ^ texY) & 8) : ((texX * texY) & 16);

			int rr, gg, bb;
			if(use_base) {
				rr = (int)(base_r * totalLightFactor);
				gg = (int)(base_g * totalLightFactor);
				bb = (int)(base_b * totalLightFactor);
			} else {
				rr = (int)(alt_r * totalLightFactor);
				gg = (int)(alt_g * totalLightFactor);
				bb = (int)(alt_b * totalLightFactor);
			}

			*fb_ptr = (((uint32_t)rr) << 16) | (((uint32_t)gg) << 8) | (uint32_t)bb;
			fb_ptr += fb_stride; 
		}
#else
        for(int y = start; y < end; y++) {
            // Cast directly to int (equivalent to the previous division, but costs almost nothing)
            int texY = (int)texPos & 63; 
            texPos += texStep; // Add the step for the next pixel
            
          // 1. BRICK LOGIC:
            int brick_x = texX;
            // If we are on an odd-numbered row of bricks (e.g., every 8th pixel vertically),
            // we shift the bricks horizontally by 8 pixels to create a staggered bond.
            if (((texY / 8) & 1) == 0) {
                brick_x += 8; 
            }

            // Create mortar joints by checking if we hit the edge of a brick (bricks are 16x8 pixels here)
            int is_fuge = ((brick_x & 15) == 0) || ((texY & 7) == 0);

            int rr, gg, bb;
            if (is_fuge) {
                // Dark gray grout color (0x33, 0x33, 0x33), multiplied by the light factor
                rr = (int)(0x33 * totalLightFactor);
                gg = (int)(0x33 * totalLightFactor);
                bb = (int)(0x33 * totalLightFactor);
            } else {
				// If it's not a mortar joint, we draw the brick itself.
                // We reuse your existing logic to choose between the primary and secondary wall color.
                int use_base = (tile == TILE_WALL) ? ((texX ^ texY) & 8) : ((texX * texY) & 16);

                if(use_base) {
                    rr = (int)(base_r * totalLightFactor);
                    gg = (int)(base_g * totalLightFactor);
                    bb = (int)(base_b * totalLightFactor);
                } else {
                    rr = (int)(alt_r * totalLightFactor);
                    gg = (int)(alt_g * totalLightFactor);
                    bb = (int)(alt_b * totalLightFactor);
                }
            }

           // Write directly to memory and move the pointer down one row
            *fb_ptr = (((uint32_t)rr) << 16) | (((uint32_t)gg) << 8) | (uint32_t)bb;
            fb_ptr += fb_stride; 
        }

#endif
    }
}


#ifdef ORIG
void render_enemies(Game *g) {
    Renderer *r = &g->renderer;
    Player *p = &g->player;

    double dist[g->config.maxEnemies];
    int order[g->config.maxEnemies];

    for(int i = 0; i < g->enemyCount; i++) {
        order[i] = i;

        double vx = p->x - g->enemies[i].x;
        double vy = p->y - g->enemies[i].y;

        dist[i] = vx * vx + vy * vy;
    }

    // Sort farthest-to-nearest
    for(int i = 0; i < g->enemyCount - 1; i++) {
        for(int j = i + 1; j < g->enemyCount; j++) {
            if(dist[i] < dist[j]) {
                double td = dist[i];
                dist[i] = dist[j];
                dist[j] = td;

                int to = order[i];
                order[i] = order[j];
                order[j] = to;
            }
        }
    }

    for(int k = 0; k < g->enemyCount; k++) {
        int i = order[k];

        Enemy *e = &g->enemies[i];

        if(!e->alive)
            continue;

        double spriteX = e->x - p->x;
        double spriteY = e->y - p->y;

        double invDet = 1.0 / (p->planeX * p->dirY - p->dirX * p->planeY);

        double transformX = invDet * (p->dirY * spriteX - p->dirX * spriteY);

        double transformY = invDet * (-p->planeY * spriteX + p->planeX * spriteY);

        if(transformY <= 0)
            continue;

        int screenX = (int)((g->config.screenW / 2) * (1 + transformX / transformY));

        int size = abs((int)(g->config.screenH / transformY * 0.2));

        if(size > 200)
            size = 200;

        int startX = screenX - size / 2;
        int endX   = screenX + size / 2;

        int startY =g->config.screenH / 2 - size / 2;
        int endY   =g->config.screenH / 2 + size / 2;

        for(int y = startY; y < endY; y++) {
            for(int x = startX; x < endX; x++) {
                if(x < 0 || x >= g->config.screenW || y < 0 || y >=g->config.screenH)
                    continue;

                if(transformY >= g->zbuffer[x])
                    continue;

                double dx = x - screenX;
                double dy = y -g->config.screenH / 2;

                double d = dx * dx + dy * dy;

                if(d > (size * size) / 4)
                    continue;

                int glow = (int)(255 - (d * 255 / ((size * size) / 4)));

				//glow *= (int)(g->candle);
				glow = (int)(glow * g->candle);
                pset(r, x, y, rgb(0, glow, 0));
            }
        }
    }
}

#else
#include <stdlib.h>
#include <math.h>

// Unified structure for sorting to keep data together in the CPU cache
typedef struct {
    int index;
    double dist;
} EnemySortItem;

// Comparison function for qsort (sorts from farthest to nearest)
int compare_enemies(const void *a, const void *b) {
    double distA = ((EnemySortItem*)a)->dist;
    double distB = ((EnemySortItem*)b)->dist;
    if (distA < distB) return 1;
    if (distA > distB) return -1;
    return 0;
}

void render_enemies(Game *g) {
    Renderer *r = &g->renderer;
    Player *p = &g->player;
    int screenW = g->config.screenW;
    int screenH = g->config.screenH;

   // 1. Calculate invDet once (depends only on the player)
    double invDet = 1.0 / (p->planeX * p->dirY - p->dirX * p->planeY);

  // Dynamic Array on the stack (VLA) for sorting
    EnemySortItem sortList[g->config.maxEnemies];
    unsigned int activeCount = 0;

   // Filter live enemies and calculate distance
    for(int i = 0; i < g->enemyCount; i++) {
        if(!g->enemies[i].alive) continue;

        double vx = p->x - g->enemies[i].x;
        double vy = p->y - g->enemies[i].y;
        
        sortList[activeCount].index = i;
        sortList[activeCount].dist = vx * vx + vy * vy;
        activeCount++;
    }

    // 2. Quicksort O(N log N) instead of O(N^2)
    qsort(sortList, activeCount, sizeof(EnemySortItem), compare_enemies);

   // Draw enemies
    unsigned int k;
    for(k = 0; k < activeCount; k++) {
        int i = sortList[k].index;
        Enemy *e = &g->enemies[i];

        double spriteX = e->x - p->x;
        double spriteY = e->y - p->y;

        double transformX = invDet * (p->dirY * spriteX - p->dirX * spriteY);
        double transformY = invDet * (-p->planeY * spriteX + p->planeX * spriteY);

        if(transformY <= 0)
            continue;

        int screenX = (int)((screenW / 2) * (1 + transformX / transformY));
        int size = abs((int)(screenH / transformY * 0.2));

        if(size > 200)
            size = 200;

        int halfSize = size / 2;
        int startX = screenX - halfSize;
        int endX   = screenX + halfSize;

        int halfScreenH = screenH / 2;
        int startY = halfScreenH - halfSize;
        int endY   = halfScreenH + halfSize;

        // 3. CLIPPING: Limit loops to the screen's actual boundaries beforehand
        if (startX < 0) startX = 0;
        if (endX > screenW) endX = screenW;
        if (startY < 0) startY = 0;
        if (endY > screenH) endY = screenH;

        double radiusSq = (double)(size * size) / 4.0;
        if (radiusSq <= 0) continue; // Avoid division by zero in the glow calculation.
        
        double invRadiusSq = 255.0 / radiusSq; // Pre-calculate the division
        double candleGlow = g->candle * invRadiusSq;

        // Loop over the clipped coordinates
        for(int x = startX; x < endX; x++) {
            // Z-buffer check moved out of the Y-loop (saves thousands of checks)
            if(transformY >= g->zbuffer[x])
                continue;

            double dx = x - screenX;
            double dxSq = dx * dx;

            for(int y = startY; y < endY; y++) {
                double dy = y - halfScreenH;
                double d = dxSq + dy * dy;

                if(d > radiusSq)
                    continue;

                // Optimized mathematics without unnecessary data type casts in the middle of the formula
                int glow = (int)(255 - (d * candleGlow));
                if (glow < 0) glow = 0; // Protection against negative values

                pset(r, x, y, rgb(0, glow, 0));
            }
        }
    }
}

#endif	//ORIG

void render_minimap(Game *g) {
    Renderer *r = &g->renderer;

    int scale = 2;

    for(int y = 0; y < g->config.mapSize; y++) {
        for(int x = 0; x < g->config.mapSize; x++) {
            uint32_t c = 0;

            if(g->map[y][x] == TILE_WALL)
                c = 0xffffff;

            if(g->map[y][x] == TILE_EXIT)
                c = 0xff0000;

            if(c) {
                for(int yy = 0; yy < scale; yy++) {
                    for(int xx = 0; xx < scale; xx++) {
                        pset(r,
                             x * scale + xx,
                             y * scale + yy,
                             c);
                    }
                }
            }
        }
    }

    // Player marker
    pset(r, (int)(g->player.x * scale), (int)(g->player.y * scale), 0x00ff00);

    // Candle bar
    int barW = g->config.mapSize * scale;
    int filled = (int)(g->candle * barW);

    for(int x = 0; x < filled; x++) {
        pset(r, x, g->config.mapSize * scale - 2, 0xffaa00);
    }
}

int text_width(const char *text) {
    int len = 0;

    while(text[len])
        len++;

    return len * 8;
}

void draw_char(Renderer *r, int x, int y, unsigned char c, uint32_t color) {
    for(int row = 0; row < 8; row++) {
        for(int col = 0; col < 8; col++) {
            if(font8x8_basic[c][row] & (1 << col)) {
                pset(r, x + col, y + row, color);
            }
        }
    }
}

void draw_text(Renderer *r, int x, int y, const char *text, uint32_t color) {
    while(*text) {
        draw_char(r, x, y,(unsigned char)*text, color);
        x += 8;
        text++;
    }
}

void draw_text_center(Renderer *r, int y, const char *text, uint32_t color) {
    draw_text(r, (r->width - text_width(text)) / 2, y, text, color);
}

void render_overlay(Game *g) {
	if(!g->levelComplete && !g->introMode && !g->paused)
        return;

    Renderer *r = &g->renderer;

    int x0 = g->config.screenW / 4;
    int x1 = g->config.screenW * 3 / 4;

    int my0 =g->config.screenH / 3;
    int my1 =g->config.screenH * 2 / 3;

    // Black box
    for(int y = my0; y < my1; y++) {
        for(int x = x0; x < x1; x++) {
            pset(r, x, y, 0x000000);
        }
    }

    // White border
    for(int x = x0; x < x1; x++) {
        pset(r, x, my0, 0xffffff);
        pset(r, x, my1 - 1, 0xffffff);
    }

    for(int y = my0; y < my1; y++) {
        pset(r, x0, y, 0xffffff);
        pset(r, x1 - 1, y, 0xffffff);
    }
    
    char buffer[64];
    
    draw_text_center(r, my0 + 40, "RAYGAME 1.0", 0xffffff);
    
	if (g->replay_level) {
		draw_text_center(r, my0 + 70, "YOUR CANDLE DIED - TRY AGAIN?", 0xffffff);
		draw_text_center(r, my0 + 100, "ENTER = RETRY", 0xffffff);
	} else if (g->introMode) {
		draw_text_center(r, my0 + 70, "ESCAPE THE DUNGEON", 0xffffff);
		draw_text_center(r, my0 + 100, "PRESS ENTER TO START", 0xffffff);
	} else if (g->paused) {
		draw_text_center(r, my0 + 70, "PAUSED", 0xffffff);
		draw_text_center(r, my0 + 100, "PRESS P TO CONTINUE", 0xffffff);
	} else if (g->level == 30) {	
		draw_text_center(r, my0 + 70, "YOU ESCAPED THE DUNGEON!", 0xffffff);
		draw_text_center(r, my0 + 100, "CONGRATULATIONS!", 0xffffff);
	} else {
		snprintf(buffer, sizeof(buffer), "LEVEL %d COMPLETE", g->level -1);
		draw_text_center(r, my0 + 70, buffer, 0xffffff);
		draw_text_center(r, my0 + 100, "ENTER = NEXT LEVEL", 0xffffff);
	}
	if (!g->paused) {
		draw_text_center(r, my0 + 130, "ESC = QUIT", 0xffffff);
	}
}

void render(Game *g) {
    draw_sky(&g->renderer, g->candle);
    draw_floor(g);
    render_walls(g);
    render_enemies(g);
    render_minimap(g);
    render_overlay(g);
}

void handle_input(Game *g) {
    XEvent e;

    while(XPending(g->renderer.display)) {
        XNextEvent(g->renderer.display, &e);
        
        // Allows the game to be closed using the window's "X" button with the mouse.
		if(e.type == ClientMessage) {
			g->running = 0;
			return;
		}
        
        if(e.type == KeyPress) {
            KeySym k = XLookupKeysym(&e.xkey, 0);
            

			// Set running to 0 instead of calling exit(0) directly
            if(k == XK_Escape) {
                g->running = 0;
                return; // Interrupt the input loop immediately.
            }

            if(k == XK_Up)
                g->input.up = 1;

            if(k == XK_Down)
                g->input.down = 1;

            if(k == XK_Left)
                g->input.left = 1;

            if(k == XK_Right)
                g->input.right = 1;
                
			if(k == XK_a)	{
				g->autoplay = !g->autoplay;
				
			}
			if(k == XK_p) {
				g->paused = !g->paused;
			}
#ifdef USE_SOUND
			if(k == XK_s) {
				g->soundEnabled = !g->soundEnabled;
			}
#endif                
            if(k == XK_Return && (g->waitingForNextLevel || g->replay_level)) {
				
				if (g->replay_level) {
					restart_level(g);
					g->replay_level = 0;
					g->waitingForNextLevel = 0;
					g->levelComplete = 0;
				} else {
					if (g->level < 30) {
						printf("level %d\n", g->level);
						generate_level(g);
						g->waitingForNextLevel = 0;
						g->levelComplete = 0;
					} else {
						//endgame overlay
						g->waitingForNextLevel = 0;
						g->levelComplete = 0;
					}
				}
			}
			if(k == XK_Return && g->introMode) {
				g->introMode = 0;
				g->autoplay = 0;

				restart_level(g);

				return;
			}
        }

        if(e.type == KeyRelease) {
            KeySym k = XLookupKeysym(&e.xkey, 0);

            if(k == XK_Up)
                g->input.up = 0;

            if(k == XK_Down)
                g->input.down = 0;

            if(k == XK_Left)
                g->input.left = 0;

            if(k == XK_Right)
                g->input.right = 0;
        }
    }
}

void renderer_init(Renderer *r, const Config *cfg) {
    r->display = XOpenDisplay(NULL);

    if(!r->display) {
        fprintf(stderr, "Failed to open X display\n");
        exit(1);
    }
    
    r->width  = cfg->screenW;
	r->height = cfg->screenH;

    int screen = DefaultScreen(r->display);

    r->window = XCreateSimpleWindow(
        r->display,
        RootWindow(r->display, screen),
        0,
        0,
        (unsigned int)r->width,
       (unsigned int)r->height,
        1,
        0,
        0
    );

    XSelectInput(
        r->display,
        r->window,
        KeyPressMask |
        KeyReleaseMask |
        ExposureMask
    );
    
    XStoreName(r->display, r->window, "Raygame 1.0");

    XMapWindow(r->display, r->window);
    // set icon
	set_window_icon(r->display, r->window);
	
	// --- NEW CODE: Tell X11 that we want to handle the close button (X) ourselves ---
    r->wm_delete_window = XInternAtom(r->display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(r->display, r->window, &r->wm_delete_window, 1);

    r->gc = XCreateGC(
        r->display,
        r->window,
        0,
        NULL
    );

    r->framebuffer = calloc((size_t)r->width * (size_t)r->height, sizeof(uint32_t));

    r->image = XCreateImage(
        r->display,
        DefaultVisual(r->display, screen),
        (unsigned int)DefaultDepth(r->display, screen),
        ZPixmap,
        0,
        (char *)r->framebuffer,
        (unsigned int)r->width,
       (unsigned int)r->height,
        32,
        0
    );
}

void renderer_shutdown(Renderer *r) {
    if(r->image) {
        r->image->data = NULL;
        XDestroyImage(r->image);
    }
    free(r->framebuffer);
    XFreeGC(r->display, r->gc);
    XDestroyWindow(r->display, r->window);
    XCloseDisplay(r->display);
}

int main(int argc, char **argv) {
		
	Config cfg = {
		.screenW    = 640,
		.screenH    = 480,
		.mapSize    = 32,
		.maxRooms   = 32,
		.maxEnemies = 32,
		.startLevel = 1,
		.noSound    = 0,
		.autoplay = 0
	};
	
	parse_args(&cfg, argc, argv);
	
	validate_config(&cfg);
		
	Game game;
	memset(&game, 0, sizeof(game));
	
	game.config = cfg;
	game.autoplay = game.config.autoplay;
	
    srand((unsigned int)time(NULL));
#ifdef USE_SOUND
    sound_init();
    game.soundEnabled = 1;
#endif

    renderer_init(&game.renderer, &cfg);

    game.level = cfg.startLevel;

    generate_level(&game);
    
    game.introMode = 1;
    game.running = 1; //<--- Start the game now

    struct timespec t0;
    struct timespec t1;

    clock_gettime(CLOCK_MONOTONIC, &t0);
	// If handle_input has set running to 0 (e.g., via Escape), 
    // we should skip the rest of this frame to close immediately.
     while(game.running) {
        handle_input(&game);
        
        if (!game.running) {
            break;
        }
        

        clock_gettime(CLOCK_MONOTONIC, &t1);

        double dt = (double)(t1.tv_sec - t0.tv_sec) + (double)(t1.tv_nsec - t0.tv_nsec) / 1e9;

        t0 = t1;

        // Clamp delta time. Prevents: giant physics jumps, 
        // unstable movement after debugger pauses
        // window drag stalls

        if(dt > 0.1)
            dt = 0.1;
            
		if(!game.paused) {
			update_game(&game, dt);
        }        
        
#ifdef USE_SOUND
		if (!cfg.noSound && game.soundEnabled) {
			game.audioTimer += dt;
			while(game.audioTimer >= AUDIO_DT) {
				sound_update(&game);
				game.audioTimer -= AUDIO_DT;
			}
		}
#endif
        render(&game);

        XPutImage(
            game.renderer.display,
            game.renderer.window,
            game.renderer.gc,
            game.renderer.image,
            0,
            0,
            0,
            0,
            (unsigned int)cfg.screenW,
			(unsigned int)cfg.screenH
        );

        // Simple frame limiter
        double target = 1.0 / TARGET_FPS;

        if(dt < target) {
            struct timespec ts;

            ts.tv_sec = 0;
            ts.tv_nsec = (long)((target - dt) * 1e9);

            nanosleep(&ts, NULL);
        }
    }
#ifdef USE_SOUND
	sound_shutdown();
#endif
    renderer_shutdown(&game.renderer);
printf("The game was closed correctly, and all resources have been released..\n");
    return 0;
}
