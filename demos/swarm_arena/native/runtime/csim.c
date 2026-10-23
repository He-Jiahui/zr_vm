#include "csim.h"
#include "runtime.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Keep expressions and traversal order aligned with the ZR rules. CMake
 * compiles this translation unit with -ffp-contract=off, without fast math.
 * Dense native records are reference storage; swaps copy every field. */
typedef struct Enemy {
    double x, y, vx, vy, ax, ay, radius, speed;
    int64_t id, hp;
    int kind, boss;
} Enemy;

typedef struct Motion { double x, y, vx, vy; int life; } Motion;
typedef struct Bullet {
    Motion motion;
    int damage, pierce, hit_count;
    int64_t history[8];
} Bullet;
typedef struct Pickup { double x, y; int value; } Pickup;
typedef enum EventTag { HIT, KILLED, PLAYER_HIT } EventTag;
typedef struct Event {
    EventTag tag;
    union {
        struct { int64_t id; int damage; } hit;
        struct { int64_t id; int kind, boss; double x, y; } killed;
        int player_damage;
    } data;
} Event;

typedef struct Player {
    double x, y, magnet_radius;
    int hp, immune_until, collected, level, xp, threshold;
    int damage, pierce, aimed_count, radial_count, next_burst;
    int64_t upgrades_hash;
} Player;

typedef struct Director {
    int index, start, next_spawn, transitions, boss_waves, burst_spawns, total_spawned;
} Director;

struct ZrgCSim {
    const char *scenario;
    uint32_t seed, frames;
    int enemy_cap, bullet_cap, pickup_cap, particle_cap;
    int base_aimed, ring_radius, wave_frames, particles_per_kill, reserve;
    Enemy *enemies;
    Bullet *bullets;
    Pickup *pickups;
    Motion *particles;
    int enemy_count, bullet_count, pickup_count, particle_count;
    int enemies_max, bullets_max, particles_max;
    int64_t next_id, rng, hash, hash60, shots, hits, kills, score, drops;
    int *id_slots;
    size_t id_capacity;
    Event *events;
    int event_head, event_count, event_cap;
    int counts[4096], starts[4097], cursor[4096], *slots;
    double circle_x[256], circle_y[256], radial_x[64], radial_y[64];
    double *bot_x, *bot_y;
    Player player;
    Director director;
    int frames_done, sprites;
};

static int64_t fail(const char *message) {
    fprintf(stderr, "SWARM_ERROR c lane %s\n", message);
    return -1;
}

static int64_t next_rng(ZrgCSim *s) {
    s->rng = s->rng * INT64_C(48271) % INT64_C(2147483647);
    return s->rng;
}

static int64_t mix(int64_t hash, int64_t value) {
    int64_t q = value % INT64_C(1000000007);
    if (q < 0) q += INT64_C(1000000007);
    return (hash * 31 + q) % INT64_C(1000000007);
}

static int64_t mix_position(int64_t hash, double value) {
    return mix(hash, (int64_t)(value * 16.0));
}

static double clamp_position(double value) {
    if (value < 0.0) value = 0.0;
    if (value > 4095.0) value = 4095.0;
    return value;
}

void zrg_csim_free(ZrgCSim *s) {
    if (!s) return;
    free(s->enemies); free(s->bullets); free(s->pickups); free(s->particles);
    free(s->events); free(s->slots); free(s->id_slots);
    free(s->bot_x); free(s->bot_y); free(s);
}

ZrgCSim *zrg_csim_new(const char *scenario, uint32_t seed, uint32_t frames) {
    ZrgCSim *s = calloc(1, sizeof(*s));
    if (!s) { fail("allocation failed"); return NULL; }
    s->scenario = scenario; s->seed = seed; s->frames = frames;
    s->enemy_cap = 2000; s->base_aimed = 10; s->ring_radius = 1024;
    s->wave_frames = 300; s->particles_per_kill = 4; s->particle_cap = 4096;
    if (!strcmp(scenario, "smoke")) {
        s->enemy_cap = 200; s->base_aimed = 2; s->ring_radius = 192;
        s->wave_frames = 40; s->particle_cap = 512;
    } else if (!strcmp(scenario, "swarm_10k")) {
        s->enemy_cap = 10000; s->ring_radius = 1600;
    } else if (!strcmp(scenario, "bullet_storm")) {
        s->enemy_cap = 1000; s->base_aimed = 100; s->ring_radius = 384; s->particle_cap = 8192;
    } else if (!strcmp(scenario, "particle_burst")) {
        s->ring_radius = 512; s->particles_per_kill = 64; s->particle_cap = 16384;
    } else if (strcmp(scenario, "arena_2k")) {
        fail("invalid scenario"); zrg_csim_free(s); return NULL;
    }
    uint32_t limit = !strcmp(scenario, "smoke") ? 120 : 1800;
    if (frames < 1 || frames > limit || seed < 1 || seed > 2147483646) {
        fail("invalid frames or seed"); zrg_csim_free(s); return NULL;
    }
    s->bullet_cap = (s->base_aimed + 8) * 90 + 64;
    s->pickup_cap = 2 * s->enemy_cap + 256;
    s->event_cap = 8 * s->bullet_cap + 64;
    s->reserve = s->enemy_cap / 20;
    if (s->reserve > 32) s->reserve = 32;
    s->enemies = calloc((size_t)s->enemy_cap, sizeof(*s->enemies));
    s->bullets = calloc((size_t)s->bullet_cap, sizeof(*s->bullets));
    s->pickups = calloc((size_t)s->pickup_cap, sizeof(*s->pickups));
    s->particles = calloc((size_t)s->particle_cap, sizeof(*s->particles));
    s->events = calloc((size_t)s->event_cap, sizeof(*s->events));
    s->slots = calloc((size_t)s->enemy_cap, sizeof(*s->slots));
    s->bot_x = calloc(frames, sizeof(*s->bot_x));
    s->bot_y = calloc(frames, sizeof(*s->bot_y));
    if (!s->enemies || !s->bullets || !s->pickups || !s->particles || !s->events ||
        !s->slots || !s->bot_x || !s->bot_y) {
        fail("allocation failed"); zrg_csim_free(s); return NULL;
    }
    s->next_id = 1; s->rng = seed; s->hash = 1;
    s->player = (Player){.x = 2048.0, .y = 2048.0, .hp = 100, .level = 1,
        .threshold = 4, .damage = 1, .aimed_count = s->base_aimed,
        .radial_count = 8, .magnet_radius = 160.0, .upgrades_hash = 1};
    for (int i = 0; i < 256; ++i) {
        double angle = (double)i * 6.283185307179586 / 256.0;
        s->circle_x[i] = cos(angle); s->circle_y[i] = sin(angle);
    }
    for (int i = 0; i < 64; ++i) {
        double angle = (double)i * 6.283185307179586 / 64.0;
        s->radial_x[i] = cos(angle); s->radial_y[i] = sin(angle);
    }
    for (uint32_t i = 1; i <= frames; ++i) {
        s->bot_x[i - 1] = 2048.0 + 128.0 * sin((double)i * 6.283185307179586 / 240.0);
        s->bot_y[i - 1] = 2048.0 + 96.0 * sin((double)i * 6.283185307179586 / 180.0);
    }
    return s;
}

static int nearest(const ZrgCSim *s) {
    int slot = -1;
    double best = 100000000.0;
    for (int i = 0; i < s->enemy_count; ++i) {
        double dx = s->enemies[i].x - s->player.x, dy = s->enemies[i].y - s->player.y;
        double d2 = dx * dx + dy * dy;
        if (d2 < best) { best = d2; slot = i; }
    }
    return slot;
}

static void input(ZrgCSim *s, int frame) {
    Player *p = &s->player;
    p->x = s->bot_x[frame]; p->y = s->bot_y[frame];
    int slot = nearest(s);
    if (slot < 0) return;
    double dx = p->x - s->enemies[slot].x, dy = p->y - s->enemies[slot].y;
    double d2 = dx * dx + dy * dy;
    if (d2 >= 2304.0) return;
    double distance = sqrt(d2);
    if (distance <= 0.000001) p->x += 24.0;
    else {
        double amount = 24.0 * (1.0 - distance / 48.0);
        p->x += -dy / distance * amount; p->y += dx / distance * amount;
    }
}

static bool spawn(ZrgCSim *s, double x, double y, int kind, int boss) {
    if (s->enemy_count == s->enemy_cap) { fail("enemy capacity exceeded"); return false; }
    if ((uint64_t)s->next_id >= s->id_capacity) {
        size_t capacity = s->id_capacity ? s->id_capacity * 2 : 256;
        int *slots = realloc(s->id_slots, capacity * sizeof(*slots));
        if (!slots) { fail("ID lookup allocation failed"); return false; }
        for (size_t i = s->id_capacity; i < capacity; ++i) slots[i] = -1;
        s->id_slots = slots; s->id_capacity = capacity;
    }
    Enemy enemy = {.x = x, .y = y, .id = s->next_id, .kind = kind, .boss = boss,
        .hp = 2, .radius = 10.0, .speed = 72.0};
    if (kind == 1) { enemy.hp = 1; enemy.radius = 7.0; enemy.speed = 120.0; }
    if (kind == 2) { enemy.hp = 8; enemy.radius = 16.0; enemy.speed = 42.0; }
    if (boss) enemy.hp = 40;
    s->id_slots[s->next_id] = s->enemy_count;
    s->enemies[s->enemy_count++] = enemy; ++s->next_id;
    return true;
}

static bool direct_and_spawn(ZrgCSim *s, int frame) {
    static const int intervals[] = {6, 5, 4, 4, 3, 3}, amounts[] = {4, 6, 8, 10, 12, 14};
    static const int grunts[] = {60, 50, 40, 45, 35, 25}, runners[] = {90, 85, 80, 85, 80, 75};
    static const int offsets[] = {0, 64, 0, -64, 32, -128};
    Director *d = &s->director;
    bool boss_wave = false;
    while (frame >= d->start + s->wave_frames) {
        d->start += s->wave_frames; d->index = (d->index + 1) % 6;
        d->next_spawn = frame; ++d->transitions;
        if (d->index == 2 || d->index == 5) { boss_wave = true; ++d->boss_waves; }
    }
    if (frame < d->next_spawn && !boss_wave) return true;
    d->next_spawn = frame + intervals[d->index];
    int limit = s->enemy_cap - s->reserve;
    int amount = amounts[d->index] * (s->enemy_cap / 200);
    if (frame == 0) amount = limit;
    if (boss_wave) { amount = s->reserve; limit = s->enemy_cap; }
    if (amount > limit - s->enemy_count) amount = limit - s->enemy_count;
    if (amount < 0) amount = 0;
    /* Requests have no side effects on the player or RNG between generation
     * and application, so append each request directly in its original order. */
    for (int i = 0; i < amount; ++i) {
        int angle = (int)(next_rng(s) % 256);
        int radius = s->ring_radius + offsets[d->index] + (int)(next_rng(s) % 257);
        int selection = (int)(next_rng(s) % 100), kind = 2;
        if (selection < grunts[d->index]) kind = 0;
        else if (selection < runners[d->index]) kind = 1;
        double x = clamp_position(s->player.x + (double)radius * s->circle_x[angle]);
        double y = clamp_position(s->player.y + (double)radius * s->circle_y[angle]);
        int boss = boss_wave && i == 0;
        if (!spawn(s, x, y, boss ? 2 : kind, boss)) return false;
    }
    d->total_spawned += amount;
    if (boss_wave) d->burst_spawns += amount;
    return true;
}

static int cell(double x, double y) {
    int cx = (int)(x / 64.0), cy = (int)(y / 64.0);
    if (cx < 0) cx = 0;
    if (cx > 63) cx = 63;
    if (cy < 0) cy = 0;
    if (cy > 63) cy = 63;
    return cy * 64 + cx;
}

static void build_grid(ZrgCSim *s) {
    memset(s->counts, 0, sizeof(s->counts));
    for (int i = 0; i < s->enemy_count; ++i) ++s->counts[cell(s->enemies[i].x, s->enemies[i].y)];
    int sum = 0;
    for (int i = 0; i < 4096; ++i) {
        s->starts[i] = sum; s->cursor[i] = sum; sum += s->counts[i];
    }
    s->starts[4096] = sum;
    for (int i = 0; i < s->enemy_count; ++i) {
        int c = cell(s->enemies[i].x, s->enemies[i].y);
        s->slots[s->cursor[c]++] = i;
    }
}

static void steer(ZrgCSim *s) {
    for (int i = 0; i < s->enemy_count; ++i) {
        Enemy *e = &s->enemies[i];
        double dx = s->player.x - e->x, dy = s->player.y - e->y;
        double distance = sqrt(dx * dx + dy * dy), tx = 0.0, ty = 0.0;
        if (distance > 0.000001) { tx = dx / distance * e->speed; ty = dy / distance * e->speed; }
        int first = cell(e->x - 24.0, e->y - 24.0), last = cell(e->x + 24.0, e->y + 24.0);
        for (int row = first / 64; row <= last / 64; ++row) {
            for (int col = first % 64; col <= last % 64; ++col) {
                int c = row * 64 + col;
                for (int j = s->starts[c]; j < s->starts[c + 1]; ++j) {
                    int other = s->slots[j];
                    if (other == i) continue;
                    double sx = e->x - s->enemies[other].x, sy = e->y - s->enemies[other].y;
                    double d2 = sx * sx + sy * sy;
                    if (d2 <= 0.000001) {
                        if (e->id < s->enemies[other].id) tx -= 24.0;
                        else tx += 24.0;
                    } else if (d2 <= 576.0) {
                        double weight = (576.0 - d2) / 576.0 * 600.0;
                        tx += sx / d2 * weight; ty += sy / d2 * weight;
                    }
                }
            }
        }
        e->ax = (tx - e->vx) * 6.0; e->ay = (ty - e->vy) * 6.0;
    }
}

static void integrate(ZrgCSim *s) {
    const double dt = 1.0 / 60.0;
    for (int i = 0; i < s->enemy_count; ++i) {
        Enemy *e = &s->enemies[i];
        double x = e->vx + e->ax * dt, y = e->vy + e->ay * dt;
        double speed = sqrt(x * x + y * y);
        if (speed > e->speed) { x = x / speed * e->speed; y = y / speed * e->speed; }
        e->vx = x; e->vy = y;
        e->x = clamp_position(e->x + x * dt); e->y = clamp_position(e->y + y * dt);
    }
}

static bool add_bullet(ZrgCSim *s, double vx, double vy, int pierce) {
    if (s->bullet_count == s->bullet_cap) { fail("bullet capacity exceeded"); return false; }
    s->bullets[s->bullet_count++] = (Bullet){
        .motion = {s->player.x, s->player.y, vx, vy, 90}, .damage = s->player.damage, .pierce = pierce};
    ++s->shots;
    return true;
}

static bool fire(ZrgCSim *s, int frame) {
    Player *p = &s->player;
    int slot = nearest(s);
    if (slot < 0) return true;
    double dx = s->enemies[slot].x - p->x, dy = s->enemies[slot].y - p->y;
    double distance = sqrt(dx * dx + dy * dy), vx = 480.0, vy = 0.0;
    if (distance > 0.000001) { vx = dx / distance * 480.0; vy = dy / distance * 480.0; }
    int hits = 2 + p->pierce;
    if (hits > 8) hits = 8;
    for (int i = 0; i < p->aimed_count; ++i) if (!add_bullet(s, vx, vy, hits)) return false;
    if (frame >= p->next_burst) {
        hits = 3 + p->pierce;
        if (hits > 8) hits = 8;
        for (int i = 0; i < p->radial_count; ++i) {
            int direction = i * 64 / p->radial_count;
            if (!add_bullet(s, s->radial_x[direction] * 480.0, s->radial_y[direction] * 480.0, hits)) return false;
        }
        p->next_burst = frame + 60;
    }
    return true;
}

static void move_motion(Motion *m) {
    const double dt = 1.0 / 60.0;
    m->x = m->x + m->vx * dt; m->y = m->y + m->vy * dt;
    --m->life;
}

static void move_bullets(ZrgCSim *s) {
    int i = 0;
    while (i < s->bullet_count) {
        move_motion(&s->bullets[i].motion);
        if (s->bullets[i].motion.life <= 0) s->bullets[i] = s->bullets[--s->bullet_count];
        else ++i;
    }
}

static bool enqueue(ZrgCSim *s, Event event) {
    if (s->event_count == s->event_cap) { fail("event queue capacity exceeded"); return false; }
    s->events[(s->event_head + s->event_count) % s->event_cap] = event;
    ++s->event_count;
    return true;
}

static bool already_hit(const Bullet *b, int64_t id) {
    for (int j = 0; j < b->hit_count; ++j) if (b->history[j] == id) return true;
    return false;
}

static bool collide(ZrgCSim *s, int frame) {
    int i = 0;
    while (i < s->bullet_count) {
        Bullet *b = &s->bullets[i];
        int first = cell(b->motion.x - 20.0, b->motion.y - 20.0);
        int last = cell(b->motion.x + 20.0, b->motion.y + 20.0);
        /* Grid membership is still the phase-4 snapshot; distances use the
         * integrated positions. No enemy swaps occur until events drain. */
        for (int row = first / 64; row <= last / 64 && b->pierce > 0; ++row) {
            for (int col = first % 64; col <= last % 64 && b->pierce > 0; ++col) {
                int c = row * 64 + col;
                for (int j = s->starts[c]; j < s->starts[c + 1] && b->pierce > 0; ++j) {
                    Enemy *e = &s->enemies[s->slots[j]];
                    double dx = e->x - b->motion.x, dy = e->y - b->motion.y;
                    double radius = e->radius + 2.0;
                    if (dx * dx + dy * dy <= radius * radius && !already_hit(b, e->id)) {
                        Event event = {.tag = HIT, .data.hit = {e->id, b->damage}};
                        if (!enqueue(s, event)) return false;
                        b->history[b->hit_count++] = e->id; --b->pierce;
                    }
                }
            }
        }
        if (b->pierce <= 0) s->bullets[i] = s->bullets[--s->bullet_count];
        else ++i;
    }
    Player *p = &s->player;
    for (i = 0; i < s->enemy_count && frame >= p->immune_until; ++i) {
        Enemy *e = &s->enemies[i];
        double dx = e->x - p->x, dy = e->y - p->y, radius = e->radius + 10.0;
        if (dx * dx + dy * dy <= radius * radius) {
            Event event = {.tag = PLAYER_HIT, .data.player_damage = 1};
            if (!enqueue(s, event)) return false;
            p->immune_until = frame + 30;
        }
    }
    return true;
}

static void add_particle(ZrgCSim *s, Motion particle) {
    if (s->particle_count < s->particle_cap) s->particles[s->particle_count++] = particle;
}

static bool particle_burst(ZrgCSim *s, double x, double y) {
    if (s->particles_per_kill == 64) {
        /* Native counterpart of the temporary burst request, separate from
         * the retained pool. Cosmetic overflow changes neither RNG nor hash. */
        Motion *request = malloc(64 * sizeof(*request));
        if (!request) { fail("particle request allocation failed"); return false; }
        for (int i = 0; i < 64; ++i)
            request[i] = (Motion){x, y, s->radial_x[i] * 24.0, s->radial_y[i] * 24.0, 24};
        for (int i = 0; i < 64; ++i) add_particle(s, request[i]);
        free(request);
    } else {
        for (int i = 0; i < s->particles_per_kill; ++i) {
            double vx = 0.0, vy = 0.0;
            if (i == 0) vx = 24.0;
            else if (i == 1) vx = -24.0;
            else if (i == 2) vy = 24.0;
            else vy = -24.0;
            add_particle(s, (Motion){x, y, vx, vy, 24});
        }
    }
    return true;
}

static bool drain(ZrgCSim *s) {
    while (s->event_count) {
        /* Copy before appending a death: the ring may reuse the popped slot. */
        Event event = s->events[s->event_head];
        s->event_head = (s->event_head + 1) % s->event_cap; --s->event_count;
        if (event.tag == HIT) {
            int64_t id = event.data.hit.id;
            int slot = id >= 0 && (uint64_t)id < s->id_capacity ? s->id_slots[id] : -1;
            if (slot < 0) continue;
            Enemy *e = &s->enemies[slot];
            e->hp -= event.data.hit.damage; ++s->hits;
            if (e->hp <= 0) {
                Event death = {.tag = KILLED, .data.killed = {e->id, e->kind, e->boss, e->x, e->y}};
                if (!enqueue(s, death)) return false;
                s->id_slots[e->id] = -1;
                int last = --s->enemy_count;
                s->enemies[slot] = s->enemies[last];
                if (slot < last) s->id_slots[s->enemies[slot].id] = slot;
            }
        } else if (event.tag == KILLED) {
            ++s->kills; s->score += 10;
            if (!particle_burst(s, event.data.killed.x, event.data.killed.y)) return false;
            int64_t draw = next_rng(s);
            if (draw % 2 == 0 || event.data.killed.boss) {
                int value = 1;
                if (event.data.killed.kind == 1) value = 2;
                else if (event.data.killed.kind == 2) value = 4;
                if (event.data.killed.boss) value = 8;
                if (s->pickup_count == s->pickup_cap) { fail("pickup capacity exceeded"); return false; }
                s->pickups[s->pickup_count++] = (Pickup){event.data.killed.x, event.data.killed.y, value};
                ++s->drops;
            }
        } else if (event.tag == PLAYER_HIT) {
            s->player.hp -= event.data.player_damage;
            if (s->player.hp < 0) s->player.hp = 0;
        }
    }
    return true;
}

static void move_particles(ZrgCSim *s) {
    int i = 0;
    while (i < s->particle_count) {
        move_motion(&s->particles[i]);
        if (s->particles[i].life <= 0) s->particles[i] = s->particles[--s->particle_count];
        else ++i;
    }
}

static void gain(ZrgCSim *s, int value) {
    Player *p = &s->player;
    p->xp += value;
    while (p->xp >= p->threshold) {
        p->xp -= p->threshold; ++p->level; p->threshold += 2;
        int upgrade = (int)(next_rng(s) % 5);
        p->upgrades_hash = mix(p->upgrades_hash, upgrade);
        if (upgrade == 0 && p->damage < 8) ++p->damage;
        else if (upgrade == 1 && p->pierce < 6) ++p->pierce;
        else if (upgrade == 2 && p->aimed_count < s->base_aimed + 8) ++p->aimed_count;
        else if (upgrade == 3 && p->magnet_radius < 384.0) {
            p->magnet_radius += 24.0;
            if (p->magnet_radius > 384.0) p->magnet_radius = 384.0;
        } else if (upgrade == 4 && p->radial_count < 32) p->radial_count += 2;
    }
}

static void magnet(ZrgCSim *s) {
    Player *p = &s->player;
    int i = 0;
    while (i < s->pickup_count) {
        Pickup *pickup = &s->pickups[i];
        double dx = p->x - pickup->x, dy = p->y - pickup->y, d2 = dx * dx + dy * dy;
        if (d2 <= p->magnet_radius * p->magnet_radius && d2 > 0.000001) {
            double distance = sqrt(d2), step = 240.0 / 60.0;
            if (step > distance) step = distance;
            pickup->x = pickup->x + dx / distance * step; pickup->y = pickup->y + dy / distance * step;
            dx = p->x - pickup->x; dy = p->y - pickup->y; d2 = dx * dx + dy * dy;
        }
        if (d2 <= 256.0) {
            ++p->collected; gain(s, pickup->value);
            s->pickups[i] = s->pickups[--s->pickup_count];
        } else ++i;
    }
}

static int64_t cheap_hash(const ZrgCSim *s, int frame) {
    const Player *p = &s->player;
    const Director *d = &s->director;
    int64_t h = mix(s->hash, frame);
    const int64_t fields[] = {s->rng, s->next_id, s->enemy_count, s->bullet_count,
        p->hp, p->immune_until, s->shots, s->hits, s->kills, s->score, s->drops,
        p->collected, p->level, p->xp, p->threshold, p->damage, p->pierce,
        p->aimed_count, p->radial_count};
    for (size_t i = 0; i < sizeof(fields) / sizeof(fields[0]); ++i) h = mix(h, fields[i]);
    h = mix_position(h, p->magnet_radius);
    const int64_t tail[] = {p->next_burst, p->upgrades_hash, d->index, d->start, d->next_spawn,
        d->transitions, d->boss_waves, d->burst_spawns, d->total_spawned, s->pickup_count};
    for (size_t i = 0; i < sizeof(tail) / sizeof(tail[0]); ++i) h = mix(h, tail[i]);
    return h;
}

static int64_t positions_hash(const ZrgCSim *s, int64_t h) {
    h = mix_position(h, s->player.x); h = mix_position(h, s->player.y);
    for (int i = 0; i < s->enemy_count; ++i) {
        const Enemy *e = &s->enemies[i];
        h = mix(h, e->id); h = mix(h, e->kind); h = mix(h, e->boss); h = mix(h, e->hp);
        h = mix_position(h, e->x); h = mix_position(h, e->y);
        h = mix_position(h, e->vx); h = mix_position(h, e->vy);
        h = mix_position(h, e->ax); h = mix_position(h, e->ay);
    }
    for (int i = 0; i < s->bullet_count; ++i) {
        const Bullet *b = &s->bullets[i];
        h = mix_position(h, b->motion.x); h = mix_position(h, b->motion.y);
        h = mix_position(h, b->motion.vx); h = mix_position(h, b->motion.vy);
        h = mix(h, b->motion.life); h = mix(h, b->damage); h = mix(h, b->pierce); h = mix(h, b->hit_count);
        for (int j = 0; j < b->hit_count; ++j) h = mix(h, b->history[j]);
    }
    for (int i = 0; i < s->pickup_count; ++i) {
        h = mix_position(h, s->pickups[i].x); h = mix_position(h, s->pickups[i].y);
        h = mix(h, s->pickups[i].value);
    }
    return h;
}

int64_t zrg_csim_full_hash(const ZrgCSim *s) { return positions_hash(s, s->hash); }

int64_t zrg_csim_tick(ZrgCSim *s, int64_t frame) {
    if (frame != s->frames_done || frame < 0 || (uint64_t)frame >= s->frames)
        return fail("tick requires the next configured frame");
    input(s, (int)frame);                                         /* 1 input */
    if (!direct_and_spawn(s, (int)frame)) return -1;               /* 2-3 requests/spawn */
    build_grid(s);                                               /* 4 grid */
    steer(s);                                                    /* 5 steering snapshot */
    integrate(s);                                                /* 6 integrate */
    if (!fire(s, (int)frame)) return -1;                           /* 7 weapons */
    move_bullets(s);                                             /* 8 bullets */
    if (!collide(s, (int)frame) || !drain(s)) return -1;            /* 9-10 collision/events */
    move_particles(s);                                           /* 11 particles */
    magnet(s);                                                   /* 12 pickups/progression */
    s->hash = cheap_hash(s, (int)frame + 1);                       /* 13 canonical hash */
    if ((frame + 1) % 60 == 0) s->hash = positions_hash(s, s->hash);
    if (frame == 59) s->hash60 = s->hash;
    ++s->frames_done;
    if (s->enemy_count > s->enemies_max) s->enemies_max = s->enemy_count;
    if (s->bullet_count > s->bullets_max) s->bullets_max = s->bullet_count;
    if (s->particle_count > s->particles_max) s->particles_max = s->particle_count;
    return s->hash;
}

static bool render_sprite(const ZrgCSim *s, double x, double y, double size, uint32_t color) {
    const Player *p = &s->player;
    /* Cull in f64 before the same f32 conversion as gfx.submitSoA. */
    if (x + size * 0.5 >= p->x - 640.0 && x - size * 0.5 <= p->x + 640.0 &&
        y + size * 0.5 >= p->y - 360.0 && y - size * 0.5 <= p->y + 360.0)
        return zrg_sprite((ZrgSprite){(float)x, (float)y, (float)size, 0.0f, color});
    return true;
}

int64_t zrg_csim_render(ZrgCSim *s, int64_t frame) {
    if (frame < 0 || frame + 1 != s->frames_done) return fail("render requires the completed tick");
    zrg_begin_frame(frame);
    if (!zrg_camera((float)s->player.x, (float)s->player.y) ||
        !render_sprite(s, s->player.x, s->player.y, 20.0, UINT32_C(1717987071)))
        return fail("render batch failed");
    for (int i = 0; i < s->enemy_count; ++i) {
        const Enemy *e = &s->enemies[i];
        if (!render_sprite(s, e->x, e->y, e->radius * 2.0, UINT32_C(4284901119)))
            return fail("render batch failed");
    }
    for (int i = 0; i < s->bullet_count; ++i) {
        const Motion *m = &s->bullets[i].motion;
        if (!render_sprite(s, m->x, m->y, 4.0, UINT32_C(4294967295))) return fail("render batch failed");
    }
    for (int i = 0; i < s->particle_count; ++i) {
        const Motion *m = &s->particles[i];
        if (!render_sprite(s, m->x, m->y, 3.0, UINT32_C(4290773247))) return fail("render batch failed");
    }
    for (int i = 0; i < s->pickup_count; ++i) {
        const Pickup *p = &s->pickups[i];
        if (!render_sprite(s, p->x, p->y, 8.0, UINT32_C(1717960959))) return fail("render batch failed");
    }
    s->sprites = (int)zrg_batch()->count;
    return s->sprites;
}

int64_t zrg_csim_entity_max(const ZrgCSim *s, int64_t kind) {
    if (kind == 0) return s->enemies_max;
    if (kind == 1) return s->bullets_max;
    if (kind == 2) return s->particles_max;
    return fail("unknown entity kind");
}

bool zrg_csim_summary(const ZrgCSim *s, char *summary, size_t capacity) {
    if (!summary || !capacity) return true;
    const Player *p = &s->player;
    const Director *d = &s->director;
    int length = snprintf(summary, capacity,
        "SWARM_OK scenario=%s lane=c mode=host frames=%d hash=%" PRId64 " seed=%u hash60=%" PRId64
        " enemies=%d bullets=%d particles=%d shots=%" PRId64 " hits=%" PRId64 " kills=%" PRId64
        " score=%" PRId64 " pickups=%d drops=%" PRId64 " collected=%d level=%d xp=%d"
        " damage=%d pierce_upgrade=%d aimed_count=%d radial_count=%d magnet_radius=%d upgrades_hash=%" PRId64
        " wave=%d boss_waves=%d burst_spawns=%d player_hp=%d sprites=%d render_hash=%u",
        s->scenario, s->frames_done, zrg_csim_full_hash(s), s->seed, s->hash60,
        s->enemy_count, s->bullet_count, s->particle_count, s->shots, s->hits, s->kills,
        s->score, s->pickup_count, s->drops, p->collected, p->level, p->xp,
        p->damage, p->pierce, p->aimed_count, p->radial_count, (int)p->magnet_radius, p->upgrades_hash,
        d->index, d->boss_waves, d->burst_spawns, p->hp, s->sprites, zrg_snapshot(""));
    if (length < 0 || (size_t)length >= capacity) { fail("summary capacity exceeded"); return false; }
    return true;
}
