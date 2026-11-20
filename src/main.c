/*
 * CITY LIFE 3D — NOVA BAY ULTIMATE WORLD (V4)
 * Expanded Android open-world driving / city-life prototype.
 * C99 + raylib 5.5 + KarvinJ/raylib-mobile Android template.
 * This project is not affiliated with Rockstar Games or GTA.
 *
 * V4 adds: an extended desert drift zone, sand surface FX, expanded NPC/traffic
 * populations, a procedural tree/park distribution, 12 vehicle silhouettes,
 * six bundled original music tracks and a playlist for user-added audio assets.
 * Artwork remains procedural low-poly geometry; external premium models/PBR
 * materials and recorded artist songs are not included.
 */

#include "raymob.h"
#include "rlgl.h"
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define PI_F                 3.14159265358979323846f
#define RAD_TO_DEG           57.29577951308232f
#define CITY_HALF            384.0f
#define WORLD_HALF           820.0f
#define CITY_BLOCK           48.0f
#define CITY_ROAD_COUNT      17
#define MAX_BUILDINGS        1040
#define WALKER_COUNT         96
#define TRAFFIC_COUNT        60
#define TREE_CAPACITY        240
#define MUSIC_CAPACITY       32
#define MISSION_COUNT        12
#define DRAW_DISTANCE        214.0f
#define SAND_X_MIN           440.0f
#define SAND_X_MAX           820.0f
#define SAND_Z_MIN          -110.0f
#define SAND_Z_MAX           390.0f
#define SAND_CENTER_X        630.0f
#define SAND_CENTER_Z        140.0f
#define SMOKE_COUNT          64
#define SKID_COUNT           220
#define PLAYER_RADIUS        0.62f
#define CAR_RADIUS           1.55f

/* ---- Palette ----------------------------------------------------------- */
static const Color C_SKY          = { 111, 183, 221, 255 };
static const Color C_GRASS        = { 79, 143, 91, 255 };
static const Color C_GRASS_DARK   = { 57, 112, 75, 255 };
static const Color C_ROAD         = { 43, 49, 58, 255 };
static const Color C_ROAD_EDGE    = { 65, 72, 82, 255 };
static const Color C_SIDEWALK     = { 157, 157, 148, 255 };
static const Color C_CURB         = { 205, 202, 188, 255 };
static const Color C_LANE         = { 246, 194, 69, 255 };
static const Color C_WINDOW       = { 103, 174, 202, 255 };
static const Color C_WINDOW_LIT   = { 246, 205, 131, 255 };
static const Color C_GLASS_DARK   = { 50, 95, 120, 255 };
static const Color C_HUD          = { 12, 20, 31, 232 };
static const Color C_HUD_SOFT     = { 18, 29, 43, 222 };
static const Color C_GOLD         = { 255, 194, 66, 255 };
static const Color C_MINT         = { 91, 220, 174, 255 };
static const Color C_WHITE        = { 245, 247, 249, 255 };
static const Color C_RED          = { 225, 84, 74, 255 };
static const Color C_BLUE         = { 64, 143, 222, 255 };
static const Color C_DARK         = { 22, 28, 39, 255 };

/* ---- Data -------------------------------------------------------------- */
typedef struct {
    Vector3 position;
    float width, depth, height;
    int style;
    Color wall;
    Color trim;
    Color sign;
    unsigned int seed;
} Building;

typedef struct {
    Vector3 position;
    float yaw;
    float speed;
    float steering;
    Color paint;
    int model;
} Vehicle;

typedef struct {
    Vector3 position;
    float yaw;
    float phase;
    float speed;
    float routeMin;
    float routeMax;
    bool alongX;
    bool isStatic;
    int direction;
    Color shirt;
    Color trousers;
    Color skin;
} Pedestrian;

typedef struct {
    float x, z, scale;
    int species;
} WorldTree;

typedef struct {
    float x;
    float y;
} MoveInput;

typedef struct {
    Vector3 position;
    Vector3 velocity;
    float life;
    float maxLife;
    float size;
} SmokeParticle;

typedef struct {
    Vector3 a;
    Vector3 b;
    float life;
} SkidMark;

typedef enum {
    GAME_TITLE = 0,
    GAME_PLAYING,
    GAME_PAUSED
} GameState;

/* ---- Global city data -------------------------------------------------- */
static Building gBuildings[MAX_BUILDINGS];
static int gBuildingCount = 0;
static Pedestrian gPedestrians[WALKER_COUNT];
static Vehicle gTraffic[TRAFFIC_COUNT];
static WorldTree gTrees[TREE_CAPACITY];
static int gTreeCount = 0;
static Music gMusicTracks[MUSIC_CAPACITY];
static char gMusicTitles[MUSIC_CAPACITY][64];
static int gMusicCount = 0;
static SmokeParticle gSmoke[SMOKE_COUNT];
static SkidMark gSkids[SKID_COUNT];
static int gSmokeCursor = 0;
static int gSkidCursor = 0;
static float gSmokeSpawnTimer = 0.0f;
static float gNightAmount = 0.0f;
static float gDriftScore = 0.0f;
static int gBestDrift = 0;
static bool gRadioEnabled = true;
static bool gAudioReady = false;
static bool gAudioDeviceReady = false;
static Sound gRadioSounds[3] = {{0}}; /* fallback procedural radio */
static int gRadioIndex = 0;
static Sound gEngineSound = {0};

static const char *GetRadioTrackName(int index);

/* ---- Small helpers ----------------------------------------------------- */
static float ClampF(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static float LerpF(float a, float b, float t) {
    return a + (b - a) * t;
}

static float Distance2D(float x1, float z1, float x2, float z2) {
    float dx = x1 - x2;
    float dz = z1 - z2;
    return sqrtf(dx*dx + dz*dz);
}

static float NormalizeAngle(float a) {
    while (a > PI_F) a -= 2.0f*PI_F;
    while (a < -PI_F) a += 2.0f*PI_F;
    return a;
}

static Color ColorScale(Color c, float factor) {
    Color out = c;
    out.r = (unsigned char)ClampF((float)c.r*factor, 0.0f, 255.0f);
    out.g = (unsigned char)ClampF((float)c.g*factor, 0.0f, 255.0f);
    out.b = (unsigned char)ClampF((float)c.b*factor, 0.0f, 255.0f);
    return out;
}

static Color LerpColor(Color a, Color b, float t) {
    t = ClampF(t, 0.0f, 1.0f);
    return (Color){
        (unsigned char)LerpF((float)a.r, (float)b.r, t),
        (unsigned char)LerpF((float)a.g, (float)b.g, t),
        (unsigned char)LerpF((float)a.b, (float)b.b, t), 255
    };
}

static unsigned int Hash2(int a, int b, int c) {
    unsigned int h = (unsigned int)(a*73856093u) ^
                     (unsigned int)(b*19349663u) ^
                     (unsigned int)(c*83492791u);
    h ^= h >> 13;
    h *= 1274126177u;
    h ^= h >> 16;
    return h;
}

static bool PointInRect(Vector2 p, Rectangle r) {
    return p.x >= r.x && p.x <= r.x + r.width &&
           p.y >= r.y && p.y <= r.y + r.height;
}

static bool TouchHeld(Rectangle r) {
    int count = GetTouchPointCount();
    for (int i = 0; i < count; i++) {
        if (PointInRect(GetTouchPosition(i), r)) return true;
    }
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && PointInRect(GetMousePosition(), r)) return true;
    return false;
}

static bool TouchPressed(Rectangle r, bool *wasDown) {
    bool down = TouchHeld(r);
    bool pressed = down && !(*wasDown);
    *wasDown = down;
    return pressed;
}

static void DrawPanel(Rectangle r, Color fill, Color outline) {
    DrawRectangleRounded(r, 0.12f, 8, fill);
    DrawRectangleRoundedLines(r, 0.12f, 8, outline);
}

static void DrawButton(Rectangle r, const char *label, Color fill, bool active) {
    Color face = active ? ColorScale(fill, 1.23f) : fill;
    DrawRectangleRounded(r, 0.20f, 9, Fade(face, 0.91f));
    DrawRectangleRoundedLines(r, 0.20f, 9, Fade(C_WHITE, 0.82f));
    int size = (r.width < 82.0f) ? 15 : 17;
    int tw = MeasureText(label, size);
    DrawText(label, (int)(r.x + (r.width - (float)tw)*0.5f),
             (int)(r.y + (r.height - (float)size)*0.5f), size, C_WHITE);
}

/* ---- City generation --------------------------------------------------- */
static void AddBuilding(float x, float z, float w, float d, float h,
                        int style, unsigned int seed) {
    if (gBuildingCount >= MAX_BUILDINGS) return;

    static const Color walls[] = {
        { 206, 188, 165, 255 }, { 182, 194, 200, 255 },
        { 210, 171, 151, 255 }, { 170, 186, 176, 255 },
        { 190, 177, 209, 255 }, { 220, 205, 168, 255 },
        { 146, 171, 190, 255 }, { 204, 179, 185, 255 }
    };
    static const Color trims[] = {
        { 87, 100, 112, 255 }, { 73, 90, 103, 255 },
        { 116, 91, 75, 255 }, { 83, 105, 94, 255 }
    };
    static const Color signs[] = {
        { 40, 105, 151, 255 }, { 181, 76, 65, 255 },
        { 44, 133, 105, 255 }, { 126, 82, 156, 255 }
    };

    Building *b = &gBuildings[gBuildingCount++];
    b->position = (Vector3){ x, 0.0f, z };
    b->width = w;
    b->depth = d;
    b->height = h;
    b->style = style;
    b->seed = seed;
    b->wall = walls[seed % (sizeof(walls)/sizeof(walls[0]))];
    b->trim = trims[(seed >> 4) % (sizeof(trims)/sizeof(trims[0]))];
    b->sign = signs[(seed >> 7) % (sizeof(signs)/sizeof(signs[0]))];
}

static void BuildCity(void) {
    gBuildingCount = 0;

    /* Sixteen by sixteen blocks, split into distinct urban districts.
       A western corridor is reserved for the airport runway and terminal. */
    for (int bx = -8; bx < 8; bx++) {
        for (int bz = -8; bz < 8; bz++) {
            if (bx == 0 && bz == 0) continue;
            if (bx <= -6 && bz >= -3 && bz <= 2) continue;
            if (bx >= 4 && bz <= -6) continue; /* container harbour district */

            float cx = (float)bx*CITY_BLOCK + CITY_BLOCK*0.5f;
            float cz = (float)bz*CITY_BLOCK + CITY_BLOCK*0.5f;

            for (int sx = -1; sx <= 1; sx += 2) {
                for (int sz = -1; sz <= 1; sz += 2) {
                    unsigned int seed = Hash2(bx, bz, (sx+2)*5 + (sz+2));
                    int style = (int)(seed % 4u);
                    float h = 13.0f + (float)((seed >> 5) % 8u)*3.1f;
                    /* Downtown towers; industrial and old-town buildings stay lower. */
                    if (abs(bx) <= 2 && abs(bz) <= 2) h += 24.0f;
                    else if (bx >= 4) h = 9.0f + (float)((seed >> 8) % 5u)*1.8f;
                    else if (bz >= 4) h = 10.0f + (float)((seed >> 9) % 6u)*2.0f;
                    if (style == 2) h += 12.0f;
                    if (style == 3) h = 10.0f + (float)((seed >> 9) % 4u)*2.0f;
                    if (h > 82.0f) h = 82.0f;
                    float width = 11.5f + (float)((seed >> 12) % 4u)*0.75f;
                    float depth = 11.5f + (float)((seed >> 15) % 4u)*0.75f;
                    AddBuilding(cx + (float)sx*9.0f,
                                cz + (float)sz*9.0f,
                                width, depth, h, style, seed);
                }
            }
        }
    }
}

static bool HitsBuilding(float x, float z, float radius) {
    for (int i = 0; i < gBuildingCount; i++) {
        const Building *b = &gBuildings[i];
        float minX = b->position.x - b->width*0.5f;
        float maxX = b->position.x + b->width*0.5f;
        float minZ = b->position.z - b->depth*0.5f;
        float maxZ = b->position.z + b->depth*0.5f;
        float nearX = ClampF(x, minX, maxX);
        float nearZ = ClampF(z, minZ, maxZ);
        if (Distance2D(x, z, nearX, nearZ) < radius) return true;
    }
    return false;
}

static bool IsInSandArena(Vector3 p) {
    return p.x >= SAND_X_MIN && p.x <= SAND_X_MAX &&
           p.z >= SAND_Z_MIN && p.z <= SAND_Z_MAX;
}

static void MoveWithCollision(Vector3 *p, float dx, float dz, float radius) {
    float nx = ClampF(p->x + dx, -WORLD_HALF + 4.0f, WORLD_HALF - 4.0f);
    if (!HitsBuilding(nx, p->z, radius)) p->x = nx;

    float nz = ClampF(p->z + dz, -WORLD_HALF + 4.0f, WORLD_HALF - 4.0f);
    if (!HitsBuilding(p->x, nz, radius)) p->z = nz;
}

/* ---- World rendering --------------------------------------------------- */
static void DrawRoadNetwork(Vector3 focus) {
    /* City ground. */
    /* A wider ground plane connects the old city to the desert drift peninsula. */
    DrawPlane((Vector3){200.0f, -0.12f, 0.0f},
              (Vector2){1250.0f, 1700.0f}, C_GRASS);

    /* Ocean, beaches and a long coastal promenade north of the city. */
    DrawCube((Vector3){0.0f, -0.08f, -438.0f},
             860.0f, 0.12f, 122.0f, (Color){ 41, 126, 169, 255 });
    DrawCube((Vector3){0.0f, -0.01f, -377.0f},
             860.0f, 0.14f, 8.0f, (Color){ 224, 198, 153, 255 });
    DrawCube((Vector3){0.0f, 0.025f, -366.0f},
             860.0f, 0.11f, 3.0f, C_SIDEWALK);

    for (int i = 0; i < CITY_ROAD_COUNT; i++) {
        float c = -CITY_HALF + (float)i*CITY_BLOCK;
        float width = (i == 4) ? 17.0f : 14.0f;
        float sidewalk = 3.0f;

        /* Road surfaces. */
        DrawCube((Vector3){c, 0.0f, 0.0f}, width, 0.12f, 820.0f, C_ROAD);
        DrawCube((Vector3){0.0f, 0.0f, c}, 820.0f, 0.12f, width, C_ROAD);

        /* Road shoulders and sidewalks on both sides. */
        for (int side = -1; side <= 1; side += 2) {
            float offset = (width*0.5f) + sidewalk*0.5f;
            DrawCube((Vector3){c + (float)side*offset, 0.015f, 0.0f},
                     sidewalk, 0.15f, 820.0f, C_SIDEWALK);
            DrawCube((Vector3){0.0f, 0.015f, c + (float)side*offset},
                     820.0f, 0.15f, sidewalk, C_SIDEWALK);
            DrawCube((Vector3){c + (float)side*(width*0.5f + sidewalk), 0.10f, 0.0f},
                     0.22f, 0.18f, 820.0f, C_CURB);
            DrawCube((Vector3){0.0f, 0.10f, c + (float)side*(width*0.5f + sidewalk)},
                     820.0f, 0.18f, 0.22f, C_CURB);
        }

        /* Broken centre lines. */
        for (int m = -380; m <= 380; m += 18) {
            if (fabsf((float)m - focus.z) < DRAW_DISTANCE) {
                DrawCube((Vector3){c, 0.075f, (float)m}, 0.20f, 0.025f, 7.0f, C_LANE);
            }
            if (fabsf((float)m - focus.x) < DRAW_DISTANCE) {
                DrawCube((Vector3){(float)m, 0.075f, c}, 7.0f, 0.025f, 0.20f, C_LANE);
            }
        }
    }

    /* Two-lane access road from the city ring road to the desert drift zone. */
    DrawCube((Vector3){475.0f, 0.0f, 96.0f}, 190.0f, 0.13f, 17.0f, C_ROAD);
    DrawCube((Vector3){475.0f, 0.09f, 96.0f}, 190.0f, 0.025f, 0.22f, C_LANE);
    DrawCube((Vector3){475.0f, 0.10f, 87.0f}, 190.0f, 0.16f, 0.35f, C_CURB);
    DrawCube((Vector3){475.0f, 0.10f, 105.0f}, 190.0f, 0.16f, 0.35f, C_CURB);

    /* Crosswalk bars at the central civic plaza. */
    for (int i = -4; i <= 4; i++) {
        float p = (float)i*1.55f;
        DrawCube((Vector3){p, 0.09f, 8.8f}, 0.72f, 0.025f, 3.4f, C_WHITE);
        DrawCube((Vector3){8.8f, 0.09f, p}, 3.4f, 0.025f, 0.72f, C_WHITE);
        DrawCube((Vector3){p, 0.09f, -8.8f}, 0.72f, 0.025f, 3.4f, C_WHITE);
        DrawCube((Vector3){-8.8f, 0.09f, p}, 3.4f, 0.025f, 0.72f, C_WHITE);
    }
}

static void DrawBuildingWindows(const Building *b, float distance) {
    /* Distance-based detail keeps the expanded world practical on phones. */
    if (distance > 145.0f) return;

    int floors = (int)(b->height/3.25f);
    if (floors < 2) floors = 2;
    int floorLimit = (distance > 92.0f) ? 5 : 10;
    if (floors > floorLimit) floors = floorLimit;

    float windowY;
    float windowW = (b->style == 2) ? 1.55f : 1.35f;
    float windowH = (b->style == 2) ? 1.55f : 1.25f;
    float sideOffset = b->width*0.5f + 0.055f;
    float frontOffset = b->depth*0.5f + 0.055f;
    int columns = (distance > 92.0f) ? 2 : ((b->width > 13.0f) ? 3 : 2);

    for (int f = 0; f < floors; f++) {
        windowY = 2.1f + (float)f*3.05f;
        if (windowY + 0.8f > b->height - 0.8f) break;

        for (int col = 0; col < columns; col++) {
            float x = b->position.x + ((float)col - (float)(columns-1)*0.5f)*3.0f;
            float z1 = b->position.z + frontOffset;
            float z2 = b->position.z - frontOffset;
            unsigned int lit = Hash2((int)b->position.x + col, (int)b->position.z + f, b->style);
            Color glass = (gNightAmount > 0.34f || (lit % 7u) == 0u) ? C_WINDOW_LIT : C_WINDOW;
            DrawCube((Vector3){x, windowY, z1}, windowW, windowH, 0.09f, glass);
            DrawCube((Vector3){x, windowY, z2}, windowW, windowH, 0.09f, glass);
        }

        /* Side windows on glass and office buildings. */
        if (distance < 92.0f && (b->style == 1 || b->style == 2)) {
            for (int col = -1; col <= 1; col += 2) {
                float z = b->position.z + (float)col*2.5f;
                DrawCube((Vector3){b->position.x + sideOffset, windowY, z},
                         0.09f, windowH, windowW, C_GLASS_DARK);
                DrawCube((Vector3){b->position.x - sideOffset, windowY, z},
                         0.09f, windowH, windowW, C_GLASS_DARK);
            }
        }
    }
}

static void DrawBuilding(const Building *b, Vector3 focus) {
    float distance = Distance2D(focus.x, focus.z, b->position.x, b->position.z);
    if (distance > DRAW_DISTANCE) return;

    Vector3 center = {b->position.x, b->height*0.5f, b->position.z};
    DrawCube(center, b->width, b->height, b->depth, b->wall);
    DrawCubeWires(center, b->width, b->height, b->depth,
                  (Color){ 65, 70, 77, 255 });

    /* Contrasting roof cap and vertical facade strips. */
    DrawCube((Vector3){b->position.x, b->height + 0.20f, b->position.z},
             b->width + 0.45f, 0.40f, b->depth + 0.45f, b->trim);
    DrawCube((Vector3){b->position.x - b->width*0.5f + 0.30f,
                       b->height*0.5f, b->position.z},
             0.32f, b->height, b->depth, ColorScale(b->wall, 0.82f));

    DrawBuildingWindows(b, distance);

    /* Shopfront / awning gives street level a recognizable silhouette. */
    if (b->style == 3) {
        DrawCube((Vector3){b->position.x, 1.55f, b->position.z + b->depth*0.5f + 0.15f},
                 b->width*0.78f, 1.65f, 0.14f, C_GLASS_DARK);
        DrawCube((Vector3){b->position.x, 3.05f, b->position.z + b->depth*0.5f + 0.48f},
                 b->width*0.90f, 0.48f, 0.72f, b->sign);
        DrawCube((Vector3){b->position.x, 0.34f, b->position.z + b->depth*0.5f + 0.38f},
                 b->width*0.94f, 0.18f, 0.72f, b->trim);
    } else {
        DrawCube((Vector3){b->position.x, 1.45f, b->position.z + b->depth*0.5f + 0.06f},
                 b->width*0.72f, 1.35f, 0.10f, C_WINDOW);
    }

    /* Small balconies on selected residential towers. */
    if (distance < 115.0f && (b->style == 0 || b->style == 1)) {
        for (int f = 1; f < 4; f++) {
            float y = 2.7f + (float)f*6.0f;
            if (y < b->height - 2.0f && ((b->seed >> (unsigned int)f) & 1u)) {
                DrawCube((Vector3){b->position.x, y, b->position.z + b->depth*0.5f + 0.55f},
                         b->width*0.55f, 0.13f, 1.0f, b->trim);
                DrawCube((Vector3){b->position.x, y + 0.45f, b->position.z + b->depth*0.5f + 1.0f},
                         b->width*0.55f, 0.12f, 0.10f, C_WHITE);
            }
        }
    }

    /* Rooftop water tank / antenna. */
    if ((b->seed % 3u) == 0u) {
        DrawCylinder((Vector3){b->position.x + b->width*0.22f,
                               b->height + 0.85f,
                               b->position.z - b->depth*0.18f},
                     0.72f, 0.72f, 1.30f, 10, (Color){ 85, 111, 128, 255 });
    } else if ((b->seed % 4u) == 0u) {
        DrawCube((Vector3){b->position.x, b->height + 1.6f, b->position.z},
                 0.18f, 2.8f, 0.18f, b->trim);
        DrawSphere((Vector3){b->position.x, b->height + 3.05f, b->position.z},
                   0.23f, C_RED);
    }
}

static void DrawTree(float x, float z, float scale, int species) {
    DrawCylinder((Vector3){x, 0.95f*scale, z}, 0.22f*scale, 0.28f*scale,
                 1.9f*scale, 8, (Color){119, 82, 53, 255});
    /* Species 0: broadleaf, 1: pine, 2: palm-like crown. */
    if (species == 2) {
        DrawCylinder((Vector3){x, 2.95f*scale, z}, 0.05f*scale, 0.05f*scale,
                     4.0f*scale, 7, (Color){121, 89, 57, 255});
        for (int k = 0; k < 4; k++) {
            float y = (1.55f + (float)k*0.64f)*scale;
            DrawCylinder((Vector3){x, y, z}, (1.08f - (float)k*0.17f)*scale,
                         0.04f*scale, 1.05f*scale, 7,
                         (Color){42, (unsigned char)(105 + k*8), 64, 255});
        }
    } else if (species == 1) {
        DrawSphere((Vector3){x, 2.20f*scale, z}, 1.05f*scale,
                   (Color){58, 130, 73, 255});
        for (int k = 0; k < 6; k++) {
            float a = (float)k*PI_F/3.0f;
            DrawLine3D((Vector3){x, 2.95f*scale, z},
                       (Vector3){x + cosf(a)*1.8f*scale, 2.75f*scale,
                                 z + sinf(a)*1.8f*scale},
                       (Color){62, 137, 76, 255});
        }
    } else {
        DrawSphere((Vector3){x, 2.15f*scale, z}, 1.18f*scale,
                   (Color){51, 133, 76, 255});
        DrawSphere((Vector3){x - 0.58f*scale, 2.0f*scale, z + 0.12f*scale},
                   0.70f*scale, (Color){66, 151, 84, 255});
        DrawSphere((Vector3){x + 0.55f*scale, 2.08f*scale, z - 0.18f*scale},
                   0.72f*scale, (Color){43, 119, 69, 255});
    }
}

static void AddWorldTree(float x, float z, float scale, int species) {
    if (gTreeCount >= TREE_CAPACITY) return;
    gTrees[gTreeCount++] = (WorldTree){x, z, scale, species};
}

static void InitWorldTrees(void) {
    gTreeCount = 0;
    for (int bx = -8; bx < 8; bx++) {
        for (int bz = -8; bz < 8; bz++) {
            unsigned int seed = Hash2(bx, bz, 991);
            float cx = (float)bx*CITY_BLOCK + CITY_BLOCK*0.5f;
            float cz = (float)bz*CITY_BLOCK + CITY_BLOCK*0.5f;
            if ((seed % 2u) != 0u) continue;
            AddWorldTree(cx - 3.8f, cz - 2.2f, 0.72f + (float)(seed%40u)*0.01f,
                         (int)(seed%3u));
            if ((seed % 4u) == 0u)
                AddWorldTree(cx + 4.0f, cz + 3.1f, 0.65f + (float)((seed>>4)%35u)*0.01f,
                             (int)((seed>>3)%3u));
        }
    }
    /* Coastal promenade parks and neighbourhood green belts. */
    for (int i = 0; i < 26; i++) {
        float x = -330.0f + (float)(i%13)*55.0f;
        float z = (i < 13) ? -356.0f : 342.0f;
        AddWorldTree(x, z, 0.80f + (float)(i%4)*0.10f, i%3);
    }
}

static void DrawStreetLamp(float x, float z) {
    DrawCylinder((Vector3){x, 3.2f, z}, 0.10f, 0.10f, 6.4f, 8,
                 (Color){ 63, 72, 82, 255 });
    DrawCube((Vector3){x + 0.68f, 6.25f, z}, 1.45f, 0.12f, 0.12f,
             (Color){ 63, 72, 82, 255 });
    DrawCube((Vector3){x + 1.23f, 6.05f, z}, 0.52f, 0.18f, 0.52f,
             (Color){ 244, 218, 151, 255 });
    if (gNightAmount > 0.25f) {
        DrawSphere((Vector3){x + 1.23f, 5.95f, z}, 0.48f,
                   Fade((Color){255, 220, 145, 255}, gNightAmount*0.30f));
    }
}

static void DrawCentralPlaza(float time) {
    DrawCube((Vector3){24.0f, 0.04f, 24.0f}, 31.0f, 0.18f, 31.0f,
             (Color){ 176, 176, 162, 255 });
    DrawCube((Vector3){24.0f, 0.15f, 24.0f}, 19.0f, 0.12f, 19.0f,
             (Color){ 202, 193, 170, 255 });

    /* Fountain with a softly moving water column. */
    DrawCylinder((Vector3){24.0f, 0.60f, 24.0f}, 4.8f, 4.8f, 0.65f,
                 32, (Color){ 91, 114, 126, 255 });
    DrawCylinder((Vector3){24.0f, 1.02f, 24.0f}, 3.9f, 3.9f, 0.18f,
                 32, (Color){ 92, 173, 196, 255 });
    float waterHeight = 1.5f + 0.18f*sinf(time*2.4f);
    DrawCylinder((Vector3){24.0f, 1.65f + waterHeight*0.5f, 24.0f},
                 0.52f, 0.08f, waterHeight, 16,
                 (Color){ 126, 202, 221, 255 });
    DrawSphere((Vector3){24.0f, 1.55f + waterHeight, 24.0f}, 0.28f,
               (Color){ 176, 230, 237, 255 });

    for (int i = 0; i < 4; i++) {
        float a = (float)i*PI_F*0.5f;
        float x = 24.0f + cosf(a)*11.2f;
        float z = 24.0f + sinf(a)*11.2f;
        DrawCube((Vector3){x, 0.65f, z}, 4.2f, 0.55f, 0.70f,
                 (Color){ 110, 79, 57, 255 });
        DrawCube((Vector3){x, 0.98f, z}, 4.2f, 0.12f, 0.76f,
                 (Color){ 183, 143, 101, 255 });
    }

    DrawTree(10.0f, 10.0f, 0.92f, 0);
    DrawTree(38.0f, 10.0f, 0.92f, 2);
    DrawTree(10.0f, 38.0f, 0.92f, 1);
    DrawTree(38.0f, 38.0f, 0.92f, 0);
}

/* ---- Large-world landmarks -------------------------------------------- */
static void DrawAirport(Vector3 focus) {
    if (Distance2D(focus.x, focus.z, -330.0f, 0.0f) > DRAW_DISTANCE + 80.0f) return;
    DrawCube((Vector3){-330.0f, 0.025f, 0.0f}, 25.0f, 0.18f, 224.0f,
             (Color){ 48, 53, 62, 255 });
    DrawCube((Vector3){-330.0f, 0.13f, 0.0f}, 0.55f, 0.035f, 198.0f, C_WHITE);
    for (int z = -92; z <= 92; z += 24) {
        DrawCube((Vector3){-330.0f, 0.15f, (float)z}, 10.0f, 0.035f, 1.1f, C_WHITE);
    }
    /* Terminal and control tower. */
    DrawCube((Vector3){-292.0f, 5.0f, -82.0f}, 44.0f, 10.0f, 25.0f,
             (Color){ 174, 190, 197, 255 });
    DrawCube((Vector3){-292.0f, 9.0f, -68.8f}, 38.0f, 2.0f, 1.0f, C_WINDOW);
    DrawCube((Vector3){-270.0f, 17.0f, -76.0f}, 7.0f, 24.0f, 7.0f,
             (Color){ 125, 145, 155, 255 });
    DrawCube((Vector3){-270.0f, 30.0f, -76.0f}, 9.0f, 4.0f, 9.0f, C_GLASS_DARK);
    DrawCube((Vector3){-270.0f, 33.0f, -76.0f}, 4.0f, 0.35f, 4.0f, C_RED);
    /* Parked passenger aircraft, stylised from primitives. */
    DrawCylinderEx((Vector3){-330.0f, 3.1f, 0.0f},
                   (Vector3){-330.0f, 3.1f, -20.0f}, 1.7f, 1.7f, 12, C_WHITE);
    DrawCube((Vector3){-330.0f, 3.3f, -6.0f}, 30.0f, 0.28f, 7.5f, C_WHITE);
    DrawCube((Vector3){-330.0f, 3.3f, 1.0f}, 9.0f, 0.18f, 5.0f, C_WHITE);
    DrawCube((Vector3){-330.0f, 3.4f, -12.0f}, 4.8f, 0.18f, 8.5f, C_WHITE);
    DrawCube((Vector3){-330.0f, 4.3f, -9.0f}, 4.2f, 1.0f, 4.6f, C_GLASS_DARK);
}

static void DrawHarbor(Vector3 focus, float time) {
    if (Distance2D(focus.x, focus.z, 270.0f, -332.0f) > DRAW_DISTANCE + 100.0f) return;
    DrawCube((Vector3){270.0f, 0.02f, -352.0f}, 190.0f, 0.28f, 38.0f,
             (Color){ 112, 117, 118, 255 });
    for (int i = 0; i < 12; i++) {
        float x = 190.0f + (float)(i%4)*23.0f;
        float z = -338.0f - (float)(i/4)*12.0f;
        Color c = (i%3 == 0) ? (Color){190, 91, 61, 255} :
                  ((i%3 == 1) ? (Color){58, 112, 145, 255} : (Color){190, 157, 73, 255});
        DrawCube((Vector3){x, 3.0f, z}, 19.0f, 6.0f, 10.0f, c);
        DrawCube((Vector3){x, 6.2f, z}, 19.2f, 0.16f, 10.2f, ColorScale(c, 0.72f));
    }
    DrawCube((Vector3){270.0f, 0.25f, -392.0f}, 180.0f, 0.55f, 50.0f,
             (Color){ 100, 103, 101, 255 });
    for (int i = 0; i < 5; i++) {
        float x = 205.0f + (float)i*32.0f;
        DrawCylinder((Vector3){x, 6.0f, -370.0f}, 0.45f, 0.45f, 12.0f, 8,
                     (Color){ 53, 64, 73, 255 });
        DrawLine3D((Vector3){x, 12.0f, -370.0f},
                   (Vector3){x + 5.0f*sinf(time*0.2f), 20.0f, -365.0f}, C_GOLD);
    }
}

static void DrawDriftDesert(Vector3 focus, float time) {
    if (Distance2D(focus.x, focus.z, SAND_CENTER_X, SAND_CENTER_Z) > DRAW_DISTANCE + 240.0f) return;
    Color sand = (Color){210, 177, 116, 255};
    Color darkSand = (Color){177, 137, 83, 255};
    DrawCube((Vector3){SAND_CENTER_X, -0.045f, SAND_CENTER_Z}, 420.0f, 0.20f, 500.0f, sand);
    /* Rolling dune ridges around the playable drift basin. */
    for (int i = 0; i < 10; i++) {
        float x = 455.0f + (float)(i%5)*88.0f;
        float z = (i < 5) ? -72.0f : 345.0f;
        rlPushMatrix();
        rlTranslatef(x, 1.0f, z);
        rlScalef(1.75f, 0.48f, 1.30f);
        DrawSphere((Vector3){0.0f, 7.0f + (float)(i%3)*1.1f, 0.0f},
                   21.0f, (i%2) ? darkSand : (Color){220, 190, 135, 255});
        rlPopMatrix();
    }
    /* A large circular drift bowl and a second linked donut track. */
    for (int ring = 0; ring < 2; ring++) {
        float radius = 46.0f + (float)ring*1.35f;
        for (int i = 0; i < 72; i++) {
            float a0 = (float)i/72.0f*2.0f*PI_F;
            float a1 = (float)(i+1)/72.0f*2.0f*PI_F;
            DrawLine3D((Vector3){SAND_CENTER_X + cosf(a0)*radius, 0.08f,
                                  SAND_CENTER_Z + sinf(a0)*radius},
                       (Vector3){SAND_CENTER_X + cosf(a1)*radius, 0.08f,
                                  SAND_CENTER_Z + sinf(a1)*radius},
                       Fade(darkSand, 0.72f));
        }
    }
    for (int loop = 0; loop < 2; loop++) {
        float cx = SAND_CENTER_X + (loop == 0 ? -94.0f : 94.0f);
        for (int i = 0; i < 64; i++) {
            float a0 = (float)i/64.0f*2.0f*PI_F;
            float a1 = (float)(i+1)/64.0f*2.0f*PI_F;
            DrawLine3D((Vector3){cx + cosf(a0)*31.0f, 0.09f,
                                  SAND_CENTER_Z + sinf(a0)*42.0f},
                       (Vector3){cx + cosf(a1)*31.0f, 0.09f,
                                  SAND_CENTER_Z + sinf(a1)*42.0f},
                       Fade((Color){142, 105, 64, 255}, 0.82f));
        }
    }
    /* Start/finish gate, flag poles, floodlights and viewing platforms. */
    DrawCube((Vector3){SAND_CENTER_X, 6.0f, SAND_CENTER_Z - 72.0f}, 25.0f, 0.5f, 0.7f,
             (Color){34, 42, 49, 255});
    DrawCube((Vector3){SAND_CENTER_X - 12.0f, 3.2f, SAND_CENTER_Z - 72.0f}, 0.45f, 6.4f, 0.45f, C_DARK);
    DrawCube((Vector3){SAND_CENTER_X + 12.0f, 3.2f, SAND_CENTER_Z - 72.0f}, 0.45f, 6.4f, 0.45f, C_DARK);
    DrawCube((Vector3){SAND_CENTER_X, 6.45f, SAND_CENTER_Z - 72.0f}, 24.0f, 0.65f, 0.35f,
             (Color){197, 66, 54, 255});
    DrawCube((Vector3){SAND_CENTER_X, 4.2f, SAND_CENTER_Z - 94.0f}, 32.0f, 1.0f, 0.7f,
             (Color){30, 39, 49, 255});
    DrawCube((Vector3){SAND_CENTER_X, 4.25f, SAND_CENTER_Z - 93.55f}, 23.0f, 0.22f, 0.06f,
             C_GOLD);
    for (int i = 0; i < 8; i++) {
        float x = 470.0f + (float)i*43.0f;
        DrawCylinder((Vector3){x, 5.0f, -28.0f}, 0.20f, 0.20f, 10.0f, 8,
                     (Color){57, 64, 69, 255});
        if (gNightAmount > 0.25f)
            DrawSphere((Vector3){x, 9.8f, -28.0f}, 1.15f,
                       Fade((Color){255, 223, 162, 255}, 0.55f));
    }
    /* Static spectator stands on the southern edge. */
    DrawCube((Vector3){SAND_CENTER_X, 1.2f, 315.0f}, 150.0f, 2.4f, 14.0f,
             (Color){86, 91, 89, 255});
    for (int row = 0; row < 3; row++)
        DrawCube((Vector3){SAND_CENTER_X, 2.2f + (float)row*0.55f,
                            307.0f + (float)row*2.0f},
                 138.0f, 0.20f, 1.1f, (Color){145, 149, 143, 255});
    if (gNightAmount > 0.35f) {
        float pulse = 0.8f + 0.2f*sinf(time*2.0f);
        DrawSphere((Vector3){SAND_CENTER_X - 82.0f, 10.0f, SAND_CENTER_Z - 42.0f},
                   1.4f*pulse, Fade(C_GOLD, 0.55f));
        DrawSphere((Vector3){SAND_CENTER_X + 82.0f, 10.0f, SAND_CENTER_Z - 42.0f},
                   1.4f*pulse, Fade(C_GOLD, 0.55f));
    }
}

static void DrawWorldLandmarks(Vector3 focus, float time) {
    DrawAirport(focus);
    DrawHarbor(focus, time);
    DrawDriftDesert(focus, time);
    /* Distant low-poly hills give the outer districts a defined skyline. */
    if (focus.z > 170.0f) {
        for (int i = 0; i < 9; i++) {
            float x = -370.0f + (float)i*92.0f;
            float h = 38.0f + (float)((i*17)%43);
            DrawCylinder((Vector3){x, h*0.40f, 440.0f}, 38.0f, 10.0f, h, 7,
                         (Color){ 92, 112, 91, 255 });
        }
    }
}

static const char *GetDistrictName(Vector3 p) {
    if (IsInSandArena(p)) return "DRIFT DESERT / SAND ARENA";
    if (p.x < -270.0f && p.z > -170.0f && p.z < 170.0f) return "NOVA INTERNATIONAL AIRPORT";
    if (p.x > 170.0f && p.z < -275.0f) return "NOVA CONTAINER PORT";
    if (p.z < -250.0f) return "OCEAN BAY / COASTLINE";
    if (fabsf(p.x) < 120.0f && fabsf(p.z) < 120.0f) return "NOVA DOWNTOWN";
    if (p.x > 180.0f) return "INDUSTRIAL DISTRICT";
    if (p.z > 190.0f) return "OLD TOWN / RESIDENTIAL";
    if (p.x < -180.0f) return "WESTSIDE SUBURBS";
    return "NOVA BAY METROPOLIS";
}

/* ---- Vehicles ---------------------------------------------------------- */
static void DrawVehicleModel(const Vehicle *v, bool playerVehicle) {
    rlPushMatrix();
    rlTranslatef(v->position.x, v->position.y, v->position.z);
    rlRotatef(v->yaw*RAD_TO_DEG, 0.0f, 1.0f, 0.0f);

    float bodyY = (v->model == 1 || v->model == 6) ? 0.72f :
                  ((v->model == 2 || v->model == 4 || v->model == 5) ? 0.96f : 0.80f);
    float bodyW = (v->model == 1 || v->model == 3 || v->model == 4 || v->model == 5) ? 2.18f : 1.94f;
    float bodyL = (v->model == 1 || v->model == 3 || v->model == 4 || v->model == 5) ? 4.95f : 4.22f;
    Color paint = v->paint;
    Color glass = (Color){ 65, 121, 151, 255 };

    /* Main body, hood and trunk. */
    DrawCube((Vector3){0.0f, bodyY, 0.0f}, bodyW, 0.72f, bodyL, paint);
    DrawCubeWires((Vector3){0.0f, bodyY, 0.0f}, bodyW, 0.72f, bodyL,
                  ColorScale(paint, 0.62f));
    DrawCube((Vector3){0.0f, 1.18f, -1.12f}, bodyW*0.83f, 0.28f, 1.1f,
             ColorScale(paint, 1.12f));
    DrawCube((Vector3){0.0f, 1.17f, 1.38f}, bodyW*0.82f, 0.25f, 0.76f,
             ColorScale(paint, 0.88f));

    /* Cabin and windows. */
    float cabinHeight = (v->model == 1 || v->model == 6) ? 0.56f :
                        ((v->model == 2 || v->model == 4 || v->model == 5) ? 1.02f : 0.82f);
    float cabinY = (v->model == 1 || v->model == 6) ? 1.34f :
                   ((v->model == 2 || v->model == 4 || v->model == 5) ? 1.55f : 1.47f);
    DrawCube((Vector3){0.0f, cabinY, 0.24f}, bodyW*0.78f, cabinHeight,
             (v->model == 3 || v->model == 5) ? 1.42f : 2.0f, ColorScale(paint, 0.94f));
    DrawCube((Vector3){0.0f, 1.52f, -0.79f}, bodyW*0.72f, 0.58f, 0.08f, glass);
    DrawCube((Vector3){0.0f, 1.52f, 1.27f}, bodyW*0.70f, 0.55f, 0.08f, glass);
    DrawCube((Vector3){-bodyW*0.405f, 1.51f, 0.20f}, 0.08f, 0.58f, 1.45f, glass);
    DrawCube((Vector3){ bodyW*0.405f, 1.51f, 0.20f}, 0.08f, 0.58f, 1.45f, glass);

    /* Headlights, tail lamps and bumpers. */
    DrawCube((Vector3){-0.66f, 0.91f, -bodyL*0.5f - 0.035f},
             0.42f, 0.22f, 0.09f, (Color){ 255, 240, 190, 255 });
    DrawCube((Vector3){ 0.66f, 0.91f, -bodyL*0.5f - 0.035f},
             0.42f, 0.22f, 0.09f, (Color){ 255, 240, 190, 255 });
    DrawCube((Vector3){-0.67f, 0.91f, bodyL*0.5f + 0.035f},
             0.38f, 0.24f, 0.09f, C_RED);
    DrawCube((Vector3){ 0.67f, 0.91f, bodyL*0.5f + 0.035f},
             0.38f, 0.24f, 0.09f, C_RED);
    DrawCube((Vector3){0.0f, 0.47f, -bodyL*0.5f - 0.08f}, bodyW*0.94f, 0.18f, 0.18f,
             (Color){ 64, 68, 75, 255 });
    DrawCube((Vector3){0.0f, 0.47f, bodyL*0.5f + 0.08f}, bodyW*0.94f, 0.18f, 0.18f,
             (Color){ 64, 68, 75, 255 });

    /* Four low-poly tyres and silver hubs. */
    for (int sx = -1; sx <= 1; sx += 2) {
        for (int sz = -1; sz <= 1; sz += 2) {
            float x = (float)sx*(bodyW*0.51f);
            float z = (float)sz*(bodyL*0.31f);
            DrawSphere((Vector3){x, 0.50f, z}, 0.43f,
                       (Color){ 24, 28, 34, 255 });
            DrawSphere((Vector3){x*1.015f, 0.50f, z}, 0.20f,
                       (Color){ 157, 166, 174, 255 });
        }
    }

    /* Model-specific silhouettes: sport rear wing, SUV roof rack, pickup bed. */
    if (v->model == 1) {
        DrawCube((Vector3){0.0f, 1.48f, 1.72f}, 1.75f, 0.10f, 0.22f, C_DARK);
        DrawCube((Vector3){0.0f, 1.30f, 1.70f}, 0.10f, 0.36f, 0.14f, C_DARK);
    } else if (v->model == 2) {
        DrawCube((Vector3){0.0f, 2.12f, 0.20f}, 1.55f, 0.08f, 1.60f, C_DARK);
    } else if (v->model == 3) {
        DrawCube((Vector3){0.0f, 1.18f, 1.16f}, bodyW*0.78f, 0.48f, 1.02f,
                 ColorScale(paint, 0.76f));
    } else if (v->model == 4) {
        DrawCube((Vector3){0.0f, 2.12f, 0.10f}, 0.70f, 0.12f, 0.42f, C_WHITE);
        DrawCube((Vector3){-0.34f, 2.24f, 0.10f}, 0.22f, 0.12f, 0.32f, C_BLUE);
        DrawCube((Vector3){0.34f, 2.24f, 0.10f}, 0.22f, 0.12f, 0.32f, C_RED);
    } else if (v->model == 5) {
        DrawCube((Vector3){0.0f, 1.28f, 1.23f}, 1.72f, 0.82f, 1.28f, ColorScale(paint, 0.88f));
        DrawCube((Vector3){0.0f, 1.88f, 0.22f}, 1.50f, 0.20f, 1.55f, glass);
    } else if (v->model == 6) {
        DrawCube((Vector3){0.0f, 1.10f, 0.22f}, bodyW*0.72f, 0.12f, 2.00f, C_GOLD);
        DrawCube((Vector3){0.0f, 1.72f, 0.10f}, bodyW*0.74f, 0.12f, 1.65f, C_DARK);
    } else if (v->model == 7) {
        DrawCube((Vector3){0.0f, 2.05f, 0.12f}, 0.44f, 0.14f, 0.34f, C_GOLD);
        DrawCube((Vector3){0.0f, 0.91f, 0.0f}, bodyW*0.97f, 0.07f, bodyL*0.82f, C_WHITE);
    } else if (v->model == 8) {
        DrawCube((Vector3){0.0f, 1.44f, 1.70f}, 1.55f, 0.10f, 0.22f, C_DARK);
        DrawCube((Vector3){0.0f, 0.88f, -0.20f}, 0.24f, 0.08f, 3.60f, C_WHITE);
    } else if (v->model == 9) {
        DrawCube((Vector3){0.0f, 1.20f, -0.72f}, 0.62f, 0.17f, 0.80f, C_DARK);
        DrawCube((Vector3){0.0f, 1.50f, 1.60f}, 1.85f, 0.10f, 0.18f, C_DARK);
    } else if (v->model == 10) {
        DrawCube((Vector3){0.0f, 1.85f, -0.10f}, 1.45f, 0.10f, 1.90f, glass);
        DrawCube((Vector3){0.0f, 0.48f, -2.10f}, 1.70f, 0.12f, 0.10f, C_GOLD);
    } else if (v->model == 11) {
        DrawCube((Vector3){-0.78f, 1.90f, 0.10f}, 0.10f, 1.00f, 1.95f, C_DARK);
        DrawCube((Vector3){0.78f, 1.90f, 0.10f}, 0.10f, 1.00f, 1.95f, C_DARK);
        DrawCube((Vector3){0.0f, 2.40f, 0.10f}, 1.65f, 0.10f, 1.95f, C_DARK);
    }
    if (playerVehicle) {
        DrawCube((Vector3){0.0f, 1.91f, -0.15f}, 0.18f, 0.08f, 0.32f,
                 (Color){ 235, 235, 235, 255 });
    }
    rlPopMatrix();
}

static void DrawSandMeetCars(Vector3 focus) {
    if (Distance2D(focus.x, focus.z, SAND_CENTER_X, SAND_CENTER_Z) > 230.0f) return;
    static const Color paints[8] = {
        {215, 62, 55, 255}, {51, 112, 188, 255}, {236, 221, 181, 255},
        {42, 46, 55, 255}, {208, 150, 55, 255}, {83, 149, 107, 255},
        {139, 81, 161, 255}, {221, 222, 228, 255}
    };
    for (int i = 0; i < 8; i++) {
        Vehicle display = {0};
        display.position = (Vector3){SAND_CENTER_X - 84.0f + (float)(i%4)*56.0f,
                                    0.0f, SAND_CENTER_Z + 170.0f + (float)(i/4)*15.0f};
        display.yaw = PI_F;
        display.paint = paints[i];
        display.model = (i*3 + 1)%12;
        DrawVehicleModel(&display, false);
    }
}

static void InitTraffic(void) {
    static const Color paints[12] = {
        {219, 87, 70, 255}, {54, 120, 177, 255}, {224, 221, 208, 255},
        {51, 62, 73, 255}, {205, 165, 75, 255}, {93, 156, 119, 255},
        {123, 83, 160, 255}, {229, 229, 220, 255}, {231, 123, 52, 255},
        {39, 42, 51, 255}, {102, 187, 191, 255}, {170, 178, 187, 255}
    };

    for (int i = 0; i < TRAFFIC_COUNT; i++) {
        Vehicle *v = &gTraffic[i];
        int road = (i%15) - 7;
        float coord = (float)road*48.0f;
        v->speed = 6.0f + (float)(i%5)*1.55f;
        v->paint = paints[i%12];
        v->model = i%12;
        v->position.y = 0.0f;
        if ((i%2) == 0) {
            v->position.x = -CITY_HALF + 16.0f + (float)((i*73)%((int)(CITY_HALF*2.0f - 32.0f)));
            v->position.z = coord + ((i%4 < 2) ? -3.2f : 3.2f);
            v->yaw = (i%4 < 2) ? -PI_F*0.5f : PI_F*0.5f;
        } else {
            v->position.x = coord + ((i%4 < 2) ? -3.2f : 3.2f);
            v->position.z = -CITY_HALF + 16.0f + (float)((i*59)%((int)(CITY_HALF*2.0f - 32.0f)));
            v->yaw = (i%4 < 2) ? 0.0f : PI_F;
        }
    }
}

static void UpdateTraffic(float dt) {
    for (int i = 0; i < TRAFFIC_COUNT; i++) {
        Vehicle *v = &gTraffic[i];
        float dx = -sinf(v->yaw)*v->speed*dt;
        float dz = -cosf(v->yaw)*v->speed*dt;
        v->position.x += dx;
        v->position.z += dz;
        if (v->position.x > CITY_HALF + 8.0f) v->position.x = -CITY_HALF - 8.0f;
        if (v->position.x < -CITY_HALF - 8.0f) v->position.x = CITY_HALF + 8.0f;
        if (v->position.z > CITY_HALF + 8.0f) v->position.z = -CITY_HALF - 8.0f;
        if (v->position.z < -CITY_HALF - 8.0f) v->position.z = CITY_HALF + 8.0f;
    }
}

/* ---- People ------------------------------------------------------------ */
static void DrawHuman(Vector3 p, float yaw, float phase,
                      Color shirt, Color trousers, Color skin,
                      bool isPlayer, bool running) {
    rlPushMatrix();
    rlTranslatef(p.x, p.y, p.z);
    rlRotatef(yaw*RAD_TO_DEG, 0.0f, 1.0f, 0.0f);

    float swing = (running ? 32.0f : 20.0f)*sinf(phase);
    float bob = (running ? 0.075f : 0.035f)*fabsf(sinf(phase));
    Color shoe = (Color){ 34, 39, 46, 255 };
    Color hair = (Color){ 47, 39, 35, 255 };

    /* Torso, collar and belt. */
    DrawCube((Vector3){0.0f, 1.12f + bob, 0.0f}, 0.66f, 0.86f, 0.38f, shirt);
    DrawCube((Vector3){0.0f, 0.68f + bob, 0.0f}, 0.62f, 0.12f, 0.40f,
             (Color){ 54, 48, 47, 255 });
    DrawCube((Vector3){0.0f, 1.57f + bob, -0.01f}, 0.34f, 0.14f, 0.34f, skin);

    /* Head, hair and face. */
    DrawSphere((Vector3){0.0f, 1.86f + bob, 0.0f}, 0.31f, skin);
    DrawSphere((Vector3){0.0f, 2.04f + bob, 0.035f}, 0.285f, hair);
    DrawCube((Vector3){0.0f, 1.86f + bob, -0.274f}, 0.12f, 0.055f, 0.035f,
             (Color){ 44, 43, 43, 255 });

    /* Arms pivot from shoulders and swing while walking. */
    rlPushMatrix();
    rlTranslatef(-0.40f, 1.43f + bob, 0.0f);
    rlRotatef(swing, 1.0f, 0.0f, 0.0f);
    DrawCube((Vector3){0.0f, -0.34f, 0.0f}, 0.20f, 0.68f, 0.23f, shirt);
    DrawSphere((Vector3){0.0f, -0.72f, -0.01f}, 0.12f, skin);
    rlPopMatrix();

    rlPushMatrix();
    rlTranslatef(0.40f, 1.43f + bob, 0.0f);
    rlRotatef(-swing, 1.0f, 0.0f, 0.0f);
    DrawCube((Vector3){0.0f, -0.34f, 0.0f}, 0.20f, 0.68f, 0.23f, shirt);
    DrawSphere((Vector3){0.0f, -0.72f, -0.01f}, 0.12f, skin);
    rlPopMatrix();

    /* Legs and shoes. */
    rlPushMatrix();
    rlTranslatef(-0.18f, 0.67f + bob, 0.0f);
    rlRotatef(-swing*0.72f, 1.0f, 0.0f, 0.0f);
    DrawCube((Vector3){0.0f, -0.30f, 0.0f}, 0.25f, 0.60f, 0.29f, trousers);
    DrawCube((Vector3){0.0f, -0.59f, -0.10f}, 0.28f, 0.16f, 0.42f, shoe);
    rlPopMatrix();

    rlPushMatrix();
    rlTranslatef(0.18f, 0.67f + bob, 0.0f);
    rlRotatef(swing*0.72f, 1.0f, 0.0f, 0.0f);
    DrawCube((Vector3){0.0f, -0.30f, 0.0f}, 0.25f, 0.60f, 0.29f, trousers);
    DrawCube((Vector3){0.0f, -0.59f, -0.10f}, 0.28f, 0.16f, 0.42f, shoe);
    rlPopMatrix();

    if (isPlayer) {
        DrawCube((Vector3){0.0f, 1.12f + bob, 0.25f}, 0.42f, 0.48f, 0.20f,
                 (Color){ 42, 57, 80, 255 });
    }
    rlPopMatrix();
}

static void InitPedestrians(void) {
    static const Color shirts[] = {
        { 42, 116, 185, 255 }, { 195, 82, 73, 255 },
        { 68, 143, 104, 255 }, { 192, 146, 63, 255 },
        { 120, 91, 165, 255 }, { 220, 220, 213, 255 },
        { 56, 82, 105, 255 }
    };
    static const Color pants[] = {
        { 44, 55, 73, 255 }, { 84, 84, 80, 255 },
        { 90, 102, 116, 255 }, { 70, 64, 58, 255 }
    };

    for (int i = 0; i < WALKER_COUNT; i++) {
        Pedestrian *p = &gPedestrians[i];
        p->isStatic = (i >= 80);
        p->alongX = ((i%2) == 0);
        p->direction = ((i%3) == 0) ? -1 : 1;
        p->routeMin = -CITY_HALF + 24.0f;
        p->routeMax = CITY_HALF - 24.0f;
        p->speed = 1.0f + (float)(i%5)*0.27f;
        p->phase = (float)i*0.83f;
        p->shirt = shirts[i%7];
        p->trousers = pants[i%4];
        p->skin = (Color){
            (unsigned char)(183 + (i%4)*11),
            (unsigned char)(132 + (i%3)*12),
            (unsigned char)(101 + (i%4)*9), 255
        };

        if (p->isStatic) {
            int guest = i - 80;
            float a = (float)guest/16.0f*2.0f*PI_F;
            float radius = (guest%2 == 0) ? 91.0f : 99.0f;
            p->position = (Vector3){SAND_CENTER_X + cosf(a)*radius, 0.0f,
                                     SAND_CENTER_Z + sinf(a)*radius};
            p->yaw = -a + PI_F*0.5f;
            p->speed = 0.0f;
        } else if (p->alongX) {
            int road = (i/2)%15 - 7;
            float start = -CITY_HALF + 30.0f + (float)((i*41)%((int)(CITY_HALF*2.0f - 60.0f)));
            p->position = (Vector3){ start, 0.0f,
                                     (float)road*48.0f + ((i%4 < 2) ? -10.2f : 10.2f) };
            p->yaw = (p->direction > 0) ? -PI_F*0.5f : PI_F*0.5f;
        } else {
            int road = (i/2)%15 - 7;
            float start = -CITY_HALF + 30.0f + (float)((i*37)%((int)(CITY_HALF*2.0f - 60.0f)));
            p->position = (Vector3){ (float)road*48.0f + ((i%4 < 2) ? -10.2f : 10.2f),
                                     0.0f, start };
            p->yaw = (p->direction > 0) ? PI_F : 0.0f;
        }
    }
}

static void UpdatePedestrians(float dt) {
    for (int i = 0; i < WALKER_COUNT; i++) {
        Pedestrian *p = &gPedestrians[i];
        float movement = p->speed*(float)p->direction*dt;
        if (!p->isStatic) {
            if (p->alongX) p->position.x += movement;
            else p->position.z += movement;
        }
        p->phase += dt*7.0f;
        if (p->isStatic) continue;

        float value = p->alongX ? p->position.x : p->position.z;
        if (value > p->routeMax) {
            if (p->alongX) p->position.x = p->routeMin;
            else p->position.z = p->routeMin;
        } else if (value < p->routeMin) {
            if (p->alongX) p->position.x = p->routeMax;
            else p->position.z = p->routeMax;
        }
    }
}

static void DrawRainAround(Vector3 focus, float time, bool raining) {
    if (!raining) return;
    for (int i = 0; i < 72; i++) {
        float x = focus.x + (float)((i*37)%74) - 37.0f;
        float z = focus.z + (float)((i*53)%74) - 37.0f;
        float y = 5.0f + (float)((i*19)%17) + fmodf(time*29.0f + (float)i*1.7f, 8.0f);
        DrawLine3D((Vector3){x, y, z}, (Vector3){x - 0.30f, y - 1.25f, z + 0.10f},
                   Fade((Color){190, 212, 225, 255}, 0.48f));
    }
}

static const char *GetCarName(int model) {
    static const char *names[12] = {
        "CITY SEDAN", "SPORT COUPE", "OFFROAD SUV", "PICKUP",
        "POLICE UNIT", "CARGO VAN", "CLASSIC GT", "CITY TAXI",
        "RALLY HATCH", "MUSCLE GT", "ELECTRIC LUX", "DESERT BUGGY"
    };
    return names[model%12];
}

/* ---- Missions ---------------------------------------------------------- */
static const Vector3 gMissionPoints[MISSION_COUNT] = {
    {   45.0f, 0.0f,   11.0f }, {   98.0f, 0.0f,  -48.0f },
    {  -44.0f, 0.0f,   60.0f }, {  -96.0f, 0.0f,  -52.0f },
    {  145.0f, 0.0f, -105.0f }, { -145.0f, 0.0f,  144.0f },
    {  285.0f, 0.0f,  -90.0f }, { -286.0f, 0.0f, -150.0f },
    {  250.0f, 0.0f,  240.0f }, { -240.0f, 0.0f,  285.0f },
    {  565.0f, 0.0f,  110.0f }, {  690.0f, 0.0f,  230.0f }
};

static void DrawMissionMarker(Vector3 p, float time, bool pickup) {
    float pulse = 1.0f + 0.16f*sinf(time*3.0f);
    Color c = pickup ? C_MINT : C_GOLD;
    DrawCylinder((Vector3){p.x, 0.13f, p.z}, 1.35f*pulse, 1.35f*pulse,
                 0.18f, 24, Fade(c, 0.55f));
    DrawCylinderWires((Vector3){p.x, 0.15f, p.z}, 1.5f*pulse, 1.5f*pulse,
                      0.22f, 24, c);
    DrawCylinder((Vector3){p.x, 1.35f, p.z}, 0.12f, 0.12f, 2.2f,
                 12, Fade(c, 0.36f));
    DrawSphere((Vector3){p.x, 2.65f + 0.15f*sinf(time*2.0f), p.z},
               0.45f*pulse, c);
}

/* ---- Minimap and HUD --------------------------------------------------- */
static void DrawMinimap(Rectangle map, Vector3 actor, Vector3 target,
                        bool hasTarget, bool inCar) {
    DrawPanel(map, (Color){ 11, 19, 28, 226 }, (Color){ 153, 174, 188, 220 });
    Rectangle inner = {map.x + 9.0f, map.y + 9.0f, map.width - 18.0f, map.height - 18.0f};
    DrawRectangle((int)inner.x, (int)inner.y, (int)inner.width, (int)inner.height,
                  (Color){ 47, 78, 69, 255 });

    float sx = inner.width/(WORLD_HALF*2.0f);
    float sz = inner.height/(WORLD_HALF*2.0f);
    Rectangle desert = {
        inner.x + (SAND_X_MIN + WORLD_HALF)*sx,
        inner.y + (SAND_Z_MIN + WORLD_HALF)*sz,
        (SAND_X_MAX - SAND_X_MIN)*sx,
        (SAND_Z_MAX - SAND_Z_MIN)*sz
    };
    DrawRectangle((int)desert.x, (int)desert.y, (int)desert.width,
                  (int)desert.height, (Color){196, 157, 101, 255});

    for (int i = -4; i <= 4; i++) {
        float x = inner.x + (float)(i + 4)*(inner.width/8.0f);
        float y = inner.y + (float)(i + 4)*(inner.height/8.0f);
        DrawRectangle((int)(x - 1.0f), (int)inner.y, 2, (int)inner.height,
                      (Color){ 85, 91, 96, 255 });
        DrawRectangle((int)inner.x, (int)(y - 1.0f), (int)inner.width, 2,
                      (Color){ 85, 91, 96, 255 });
    }

    float px = inner.x + (actor.x + WORLD_HALF)*sx;
    float py = inner.y + (actor.z + WORLD_HALF)*sz;
    if (hasTarget) {
        float tx = inner.x + (target.x + WORLD_HALF)*sx;
        float ty = inner.y + (target.z + WORLD_HALF)*sz;
        DrawCircle((int)tx, (int)ty, 3.8f, C_GOLD);
    }
    DrawCircle((int)px, (int)py, inCar ? 4.6f : 3.5f, C_MINT);
    DrawCircleLines((int)px, (int)py, 6.0f, C_WHITE);
}

static void DrawGameHUD(int screenW, int screenH, int cash,
                        int delivered, int missionStep, float speed,
                        bool inCar, bool nearCar, bool nearMission,
                        Vector3 actor, Vector3 target, bool hasTarget,
                        MoveInput stick, float stickX, float stickY,
                        float time, bool radioEnabled, int radioTrack, int carModel,
                        float driftScore, int bestDrift, float nightAmount, bool raining) {
    DrawPanel((Rectangle){14.0f, 14.0f, 290.0f, 128.0f}, C_HUD, (Color){ 111, 142, 160, 220 });
    DrawText("NOVA BAY", 28, 21, 15, C_MINT);
    DrawText("CITY LIFE 3D", 28, 40, 23, C_WHITE);
    DrawText(GetDistrictName(actor), 28, 68, 12, Fade(C_WHITE, 0.84f));
    DrawText(TextFormat("CASH  $%d", cash), 28, 88, 17, C_GOLD);
    DrawText(TextFormat("DELIVERIES  %d / 6", delivered), 28, 113, 13, C_WHITE);

    Rectangle map = {(float)screenW - 160.0f, 14.0f, 146.0f, 146.0f};
    DrawMinimap(map, actor, target, hasTarget, inCar);

    DrawPanel((Rectangle){(float)screenW - 160.0f, 169.0f, 146.0f, 42.0f}, C_
