#include "raylib.h"
#include <raymath.h>
#include <entt.hpp>
#include <vector>
#include <cmath>
#include <unordered_map>
#include <algorithm> 

#define FPS 120

// ============================================================================
// 1. DATA-DRIVEN CONFIGURATION
// ============================================================================
struct GameConfig {
    float fixedDt = 1.0f / 60;

    struct {
        int maxHp = 100;
        float maxSpeed = 18.0f;
        float acceleration = 80.0f;
        float friction = 8.0f;
        float verticalFriction = 8.0f;
        float fireCooldown = 0.2f;
        float torpedoDamage = 25.0f;
        float dashSpeedMult = 4.0f;
        float dashDuration = 0.2f;
        float dashCooldown = 1.5f;
    } player;

    struct {
        int maxHp = 1500;
        float speedPhase1 = 6.0f;
        float speedPhase2 = 12.0f;
        float attackRatePhase1 = 2.0f;
        float attackRatePhase2 = 0.8f;
        float bodyDamage = 15.0f;
        int segmentCount = 15;
        float segmentDist = 2.5f;
    } monster1;

    struct {
        float inkDamage = 15.0f;
    } combat;
};

// ============================================================================
// 2. COMPONENTS & TAGS
// ============================================================================
struct Transform3D { Vector3 position; float yaw; };
struct Velocity { Vector3 value; };
struct Lifetime { float remaining; };

// Tags
struct TorpedoTag {};
struct InkTag {};
struct BubbleTag {};
struct WallTag {};
struct ReticleTag {}; 
struct WallRepelTag {}; 
struct PendingDestroy {}; 

// Mechanics Components
struct BossSegment { 
    entt::entity leader; 
    float followDistance; 
};

struct DestructibleWall { int hp; };
struct OceanCurrent { Vector3 force; };

enum ColliderType { COL_SPHERE, COL_BOX };
struct Collider { 
    ColliderType type;
    float radius;          
    Vector3 size;          
    BoundingBox cachedBox; 
};

// Gameplay Data
struct PlayerData { 
    float fireTimer; 
    int hp; 
    float hitShake; 
    float iFrameTimer; 
    float dashTimer;
    float dashCooldownTimer;
};

struct Monster1Data {
    int hp;
    int maxHp;
    float attackTimer;
    int phase; 
};

struct WeaponData { float forwardOffset; };

struct CameraState {
    float angle = 0.0f;
    float pitch = 0.6f;
    float distance = 20.0f;
};

// ============================================================================
// 3. GRAPHICS ABSTRACTION
// ============================================================================
enum class AssetID { 
    PlayerHull, Monster1Head, Monster1Body, Torpedo, InkBlast, Bubble, SeaWall, Muzzle, Reticle, CurrentZone, Debris
};

struct MeshRenderer { 
    AssetID assetId;
    Vector3 scale;     
    Color tint; 
};

// ============================================================================
// 4. SPATIAL GRID SYSTEM 
// ============================================================================
class SpatialGrid {
private:
    float cellSize;
    std::unordered_map<uint64_t, std::vector<entt::entity>> cells;

    uint64_t GetHash(int gridX, int gridZ) {
        return ((uint64_t)(uint32_t)gridX << 32) | (uint32_t)gridZ;
    }

public:
    SpatialGrid(float size) : cellSize(size) {}
    void Clear() { cells.clear(); }

    void InsertBox(entt::entity entity, BoundingBox box) {
        int minX = (int)std::floor(box.min.x / cellSize);
        int minZ = (int)std::floor(box.min.z / cellSize);
        int maxX = (int)std::floor(box.max.x / cellSize);
        int maxZ = (int)std::floor(box.max.z / cellSize);

        for (int x = minX; x <= maxX; x++) {
            for (int z = minZ; z <= maxZ; z++) {
                cells[GetHash(x, z)].push_back(entity);
            }
        }
    }

    std::vector<entt::entity> GetNearby(Vector3 position) {
        std::vector<entt::entity> result;
        int gridX = (int)std::floor(position.x / cellSize);
        int gridZ = (int)std::floor(position.z / cellSize);

        for (int x = -1; x <= 1; x++) {
            for (int z = -1; z <= 1; z++) {
                uint64_t hash = GetHash(gridX + x, gridZ + z);
                if (cells.count(hash)) {
                    for (auto e : cells[hash]) {
                        if (std::find(result.begin(), result.end(), e) == result.end()) {
                            result.push_back(e);
                        }
                    }
                }
            }
        }
        return result;
    }
};

// ============================================================================
// 5. SYSTEMS 
// ============================================================================

void player_input_system(entt::registry& registry) {
    const auto& config = registry.ctx().get<GameConfig>();
    const float dt = registry.ctx().get<float>();
    const auto& camState = registry.ctx().get<CameraState>();

    auto view = registry.view<Velocity, PlayerData>();

    Vector3 forward = { -sinf(camState.angle), 0.0f, -cosf(camState.angle) };
    Vector3 right   = {  cosf(camState.angle), 0.0f, -sinf(camState.angle) };

    view.each([&](auto& vel, auto& player) {
        
        // Dash Logic
        if (player.dashCooldownTimer > 0) player.dashCooldownTimer -= dt;
        if (player.dashTimer > 0) player.dashTimer -= dt;

        if (IsKeyPressed(KEY_LEFT_SHIFT) && player.dashCooldownTimer <= 0) {
            player.dashTimer = config.player.dashDuration;
            player.dashCooldownTimer = config.player.dashCooldown;
            player.iFrameTimer = config.player.dashDuration; // Invincible during dash!
        }

        float currentAccel = config.player.acceleration;
        float currentMaxSpeed = config.player.maxSpeed;

        if (player.dashTimer > 0) {
            currentAccel *= config.player.dashSpeedMult;
            currentMaxSpeed *= config.player.dashSpeedMult;
        }

        Vector3 horizontalMove = { 0, 0, 0 };
        if (IsKeyDown(KEY_W)) horizontalMove = Vector3Add(horizontalMove, forward);
        if (IsKeyDown(KEY_S)) horizontalMove = Vector3Subtract(horizontalMove, forward);
        if (IsKeyDown(KEY_D)) horizontalMove = Vector3Add(horizontalMove, right);
        if (IsKeyDown(KEY_A)) horizontalMove = Vector3Subtract(horizontalMove, right);

        float moveLenSqr = Vector3LengthSqr(horizontalMove);
        if (moveLenSqr > 0.0f) {
            horizontalMove = Vector3Scale(horizontalMove, 1.0f / sqrtf(moveLenSqr)); 
            Vector3 accelVec = Vector3Scale(horizontalMove, currentAccel * dt);
            vel.value.x += accelVec.x;
            vel.value.z += accelVec.z;
        }

        float verticalMove = 0.0f;
        if (IsKeyDown(KEY_SPACE)) verticalMove += 1.0f;
        if (IsKeyDown(KEY_LEFT_CONTROL))     verticalMove -= 1.0f; // Moved from SHIFT to C

        if (verticalMove != 0.0f) vel.value.y += verticalMove * currentAccel * dt;

        float horizFrictionMult = fmaxf(0.0f, 1.0f - (config.player.friction * dt));
        float vertFrictionMult  = fmaxf(0.0f, 1.0f - (config.player.verticalFriction * dt)); 

        vel.value.x *= horizFrictionMult;
        vel.value.z *= horizFrictionMult;
        vel.value.y *= vertFrictionMult;      

        float currentSpeedSqr = Vector3LengthSqr(vel.value);
        if (currentSpeedSqr > currentMaxSpeed * currentMaxSpeed) {
            vel.value = Vector3Scale(Vector3Normalize(vel.value), currentMaxSpeed);
        }
        
        if (Vector3LengthSqr(vel.value) < 0.0001f) vel.value = { 0, 0, 0 };
    });
}

void targeting_system(entt::registry& registry, const Camera3D& camera) {
    Ray mouseRay = GetMouseRay(GetMousePosition(), camera);
    Vector3 targetPos = {0, 0, 0};
    bool hitSomething = false;
    float closestDist = 9999.0f;

    registry.view<Transform3D, Collider>().each([&](const auto& trans, const auto& col) {
        if (col.radius < 1.5f && col.type == COL_SPHERE) return; 

        RayCollision hit = { 0 };
        if (col.type == COL_SPHERE) hit = GetRayCollisionSphere(mouseRay, trans.position, col.radius);
        else if (col.type == COL_BOX) hit = GetRayCollisionBox(mouseRay, col.cachedBox);
        
        if (hit.hit && hit.distance < closestDist) {
            closestDist = hit.distance;
            targetPos = hit.point;
            hitSomething = true;
        }
    });

    if (!hitSomething) targetPos = Vector3Add(mouseRay.position, Vector3Scale(mouseRay.direction, 100.0f));

    registry.view<Transform3D, ReticleTag>().each([&](auto& transform) { transform.position = targetPos; });

    registry.view<Transform3D, PlayerData>().each([&](auto& p_trans, const auto&) {
        Vector3 dir = Vector3Subtract(targetPos, p_trans.position);
        p_trans.yaw = atan2f(-dir.x, -dir.z); 
    });
}

void monster1_ai_system(entt::registry& registry) {
    const auto& config = registry.ctx().get<GameConfig>();
    const float dt = registry.ctx().get<float>();
    auto wallsView = registry.view<Collider, WallTag>(); 

    Vector3 playerPos = {0, 0, 0};
    registry.view<Transform3D, PlayerData>().each([&](const auto& p_trans, const auto&) {
        playerPos = p_trans.position;
    });

    // 1. Head Logic
    registry.view<Transform3D, Velocity, Monster1Data>().each([&](auto& transform, auto& vel, auto& monster1) {
        if (monster1.hp < monster1.maxHp / 2 && monster1.phase == 1) monster1.phase = 2;

        Vector3 dir = Vector3Subtract(playerPos, transform.position);
        float distSqr = Vector3LengthSqr(dir);
        
        if (distSqr > 0.0f && distSqr < 10000.0f) { // Aggro Radius
            float speed = (monster1.phase == 1) ? config.monster1.speedPhase1 : config.monster1.speedPhase2;
            vel.value = Vector3Scale(dir, speed / sqrtf(distSqr)); 
        } else {
            vel.value = {0,0,0};
        }

        monster1.attackTimer -= dt;
        if (monster1.attackTimer <= 0.0f && distSqr < 10000.0f) {
            monster1.attackTimer = (monster1.phase == 1) ? config.monster1.attackRatePhase1 : config.monster1.attackRatePhase2; 
            Vector3 shootDir = Vector3Scale(dir, 1.0f / sqrtf(distSqr)); 
            Ray sightRay = { transform.position, shootDir };
            bool hasLineOfSight = true;

            wallsView.each([&](const auto& w_col) {
                RayCollision hit = GetRayCollisionBox(sightRay, w_col.cachedBox);
                if (hit.hit && hit.distance < sqrtf(distSqr)) hasLineOfSight = false; 
            });

            if (hasLineOfSight) {
                int projectiles = (monster1.phase == 1) ? 1 : 12;
                float angleStep = (2.0f * PI) / projectiles;
                
                for (int i = 0; i < projectiles; i++) {
                    Vector3 novaDir = (projectiles == 1) ? shootDir : Vector3Normalize({ cosf(i * angleStep), shootDir.y, sinf(i * angleStep) });
                    auto ink = registry.create();
                    registry.emplace<Transform3D>(ink, Vector3Add(transform.position, Vector3Scale(novaDir, 6.0f)), 0.0f);
                    registry.emplace<Velocity>(ink, Vector3Scale(novaDir, projectiles == 1 ? 25.0f : 12.0f));
                    registry.emplace<MeshRenderer>(ink, AssetID::InkBlast, Vector3{1,1,1}, projectiles == 1 ? BLACK : RED);
                    registry.emplace<Collider>(ink, COL_SPHERE, 1.0f, Vector3{0,0,0}, BoundingBox{});
                    registry.emplace<Lifetime>(ink, 5.0f);
                    registry.emplace<InkTag>(ink);
                }
            }
        }
    });

    // 2. Segment Follow-The-Leader Logic
    registry.view<Transform3D, BossSegment>().each([&](auto& trans, const auto& seg) {
        if (!registry.valid(seg.leader)) return;
        auto leaderTrans = registry.get<Transform3D>(seg.leader);
        
        Vector3 dir = Vector3Subtract(leaderTrans.position, trans.position);
        float dist = Vector3Length(dir);
        
        if (dist > seg.followDistance) {
            Vector3 moveDir = Vector3Normalize(dir);
            // Snap to exact follow distance behind leader
            trans.position = Vector3Subtract(leaderTrans.position, Vector3Scale(moveDir, seg.followDistance));
        }
    });
}

void player_combat_system(entt::registry& registry) {
    const auto& config = registry.ctx().get<GameConfig>();
    const float dt = registry.ctx().get<float>();

    Vector3 targetPos = {0, 0, 0};
    registry.view<Transform3D, ReticleTag>().each([&](const auto& trans) { targetPos = trans.position; });

    registry.view<Transform3D, Velocity, PlayerData, MeshRenderer, WeaponData>().each(
        [&](const auto& p_trans, const auto& p_vel, auto& p_data, auto& p_renderer, const auto& p_weapon) {
        
        if (p_data.iFrameTimer > 0.0f) {
            p_data.iFrameTimer -= dt;
            p_renderer.tint = ((int)(p_data.iFrameTimer * 15) % 2 == 0) ? Fade(YELLOW, 0.1f) : YELLOW;
        } else p_renderer.tint = YELLOW;

        if (p_data.fireTimer > 0.0f) p_data.fireTimer -= dt;

        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && p_data.fireTimer <= 0.0f) {
            p_data.fireTimer = config.player.fireCooldown;
            Vector3 shootDir = Vector3Normalize(Vector3Subtract(targetPos, p_trans.position));
            
            Vector3 spawnPos = {
                p_trans.position.x - sinf(p_trans.yaw) * p_weapon.forwardOffset,
                p_trans.position.y,
                p_trans.position.z - cosf(p_trans.yaw) * p_weapon.forwardOffset
            };

            auto torpedo = registry.create();
            registry.emplace<Transform3D>(torpedo, spawnPos, 0.0f); 
            registry.emplace<Velocity>(torpedo, Vector3Add(Vector3Scale(shootDir, 100.0f), p_vel.value)); 
            registry.emplace<MeshRenderer>(torpedo, AssetID::Torpedo, Vector3{0.4f,0.4f,0.4f}, ORANGE);
            registry.emplace<Collider>(torpedo, COL_SPHERE, 0.4f, Vector3{0,0,0}, BoundingBox{});
            registry.emplace<Lifetime>(torpedo, 3.0f);
            registry.emplace<TorpedoTag>(torpedo);
        }
    });
}

void environment_system(entt::registry& registry) {
    const float dt = registry.ctx().get<float>();
    
    // Bubbles
    static float bubbleTimer = 0.0f;
    bubbleTimer -= dt;
    if (bubbleTimer <= 0.0f) {
        bubbleTimer = 0.1f; 
        auto bubble = registry.create();
        float scale = (float)GetRandomValue(1, 4) / 10.0f;
        registry.emplace<Transform3D>(bubble, Vector3{(float)GetRandomValue(-50, 50), (float)GetRandomValue(0, 5), (float)GetRandomValue(-50, 50)}, 0.0f);
        registry.emplace<Velocity>(bubble, Vector3{0, (float)GetRandomValue(2, 6), 0}); 
        registry.emplace<MeshRenderer>(bubble, AssetID::Bubble, Vector3{scale, scale, scale}, Fade(RAYWHITE, 0.4f));
        registry.emplace<Lifetime>(bubble, 10.0f);
    }
}

void physics_system(entt::registry& registry) {
    const float dt = registry.ctx().get<float>();
    
    // Apply Ocean Currents
    registry.view<Transform3D, Velocity>().each([&](const auto& trans, auto& vel) {
        registry.view<Collider, OceanCurrent>().each([&](const auto& col, const auto& current) {
            if (CheckCollisionBoxSphere(col.cachedBox, trans.position, 0.5f)) {
                vel.value = Vector3Add(vel.value, Vector3Scale(current.force, dt));
            }
        });
    });

    auto dynamicBodies = registry.group<Transform3D, Velocity>();
    for (auto entity : dynamicBodies) {
        auto& transform = dynamicBodies.get<Transform3D>(entity);
        const auto& vel = dynamicBodies.get<Velocity>(entity);
        transform.position = Vector3Add(transform.position, Vector3Scale(vel.value, dt));
    }

    registry.view<Transform3D, PlayerData>().each([](auto& transform, const auto&) {
        if (transform.position.y < 1.0f) transform.position.y = 1.0f;
    });

    registry.view<Transform3D, Monster1Data>().each([](auto& transform, const auto&) {
        if (transform.position.y < 5.0f) transform.position.y = 5.0f;
    });
}

void collision_system(entt::registry& registry) {
    const auto& config = registry.ctx().get<GameConfig>();
    
    auto players = registry.view<Transform3D, Collider, PlayerData>();
    auto bossHeads = registry.view<Transform3D, Collider, Monster1Data>();
    auto bossSegments = registry.view<Transform3D, Collider, BossSegment>();
    auto walls = registry.view<Collider, DestructibleWall>(); 
    auto dynamicSolids = registry.view<Transform3D, Collider, WallRepelTag>();

    static SpatialGrid wallGrid(10.0f);
    wallGrid.Clear();
    walls.each([&](auto entity, const auto& w_col, const auto&) { wallGrid.InsertBox(entity, w_col.cachedBox); });

    // 1. Solid Repel Logic & Destructible Cover
    dynamicSolids.each([&](auto solid_entity, auto& trans, const auto& col) {
        bool isEnragedBoss = registry.all_of<Monster1Data>(solid_entity) && registry.get<Monster1Data>(solid_entity).phase == 2;

        for (auto wallEntity : wallGrid.GetNearby(trans.position)) {
            const auto& w_col = registry.get<Collider>(wallEntity);
            auto& wallData = registry.get<DestructibleWall>(wallEntity);

            Vector3 closest = { 
                Clamp(trans.position.x, w_col.cachedBox.min.x, w_col.cachedBox.max.x), 
                Clamp(trans.position.y, w_col.cachedBox.min.y, w_col.cachedBox.max.y), 
                Clamp(trans.position.z, w_col.cachedBox.min.z, w_col.cachedBox.max.z) 
            };
            
            float distSqr = Vector3DistanceSqr(trans.position, closest);
            float radSqr = col.radius * col.radius;
            
            if (distSqr < radSqr) {
                // If Enraged Boss hits wall, destroy wall!
                if (isEnragedBoss) {
                    wallData.hp -= 100;
                    if (wallData.hp <= 0) {
                        registry.emplace_or_replace<PendingDestroy>(wallEntity);
                        
                        // Spawn Debris
                        for (int i = 0; i < 5; i++) {
                            auto debris = registry.create();
                            registry.emplace<Transform3D>(debris, closest, 0.0f);
                            registry.emplace<Velocity>(debris, Vector3{(float)GetRandomValue(-15,15), (float)GetRandomValue(5,20), (float)GetRandomValue(-15,15)});
                            registry.emplace<MeshRenderer>(debris, AssetID::Debris, Vector3{2,2,2}, DARKGRAY);
                            registry.emplace<Lifetime>(debris, 2.0f);
                        }
                    }
                } else {
                    // Standard Repel
                    if (distSqr > 0.0001f) {
                        float dist = sqrtf(distSqr);
                        Vector3 pushDir = Vector3Scale(Vector3Subtract(trans.position, closest), 1.0f / dist);
                        trans.position = Vector3Add(trans.position, Vector3Scale(pushDir, col.radius - dist));
                    } else trans.position.y += 0.01f; 
                }
            }           
        }
    });

    // 2. Torpedoes vs Boss/Walls
    registry.view<Transform3D, Collider, TorpedoTag>().each([&](auto t_entity, const auto& t_trans, const auto& t_col) {
        if (t_trans.position.y - t_col.radius <= 0.0f) {
            registry.emplace_or_replace<PendingDestroy>(t_entity); return;
        }
        
        bool destroyed = false;
        
        // Check Boss Head
        bossHeads.each([&](const auto& b_trans, const auto& b_col, auto& b_data) {
            if (!destroyed && CheckCollisionSpheres(t_trans.position, t_col.radius, b_trans.position, b_col.radius)) {
                b_data.hp -= config.player.torpedoDamage;
                registry.emplace_or_replace<PendingDestroy>(t_entity);
                destroyed = true; 
            }
        });

        // Check Boss Segments
        bossSegments.each([&](auto seg_ent, const auto& s_trans, const auto& s_col, const auto&) {
            if (!destroyed && CheckCollisionSpheres(t_trans.position, t_col.radius, s_trans.position, s_col.radius)) {
                // Hitting the body still damages the main boss pool
                registry.view<Monster1Data>().each([&](auto& b_data) { b_data.hp -= config.player.torpedoDamage * 0.5f; });
                registry.emplace_or_replace<PendingDestroy>(t_entity);
                
                // Visual feedback for hitting segment
                registry.get<MeshRenderer>(seg_ent).tint = RED; 
                destroyed = true; 
            }
        });

        if (destroyed) return;

        for (auto wallEntity : wallGrid.GetNearby(t_trans.position)) {
            if (destroyed) break;
            const auto& w_col = registry.get<Collider>(wallEntity);
            if (CheckCollisionBoxSphere(w_col.cachedBox, t_trans.position, t_col.radius)) {
                registry.emplace_or_replace<PendingDestroy>(t_entity);
                destroyed = true;
            }
        }
    });

    // 3. Ink vs Player/Walls
    registry.view<Transform3D, Collider, InkTag>().each([&](auto i_entity, const auto& i_trans, const auto& i_col) {
        if (i_trans.position.y - i_col.radius <= 0.0f) {
            registry.emplace_or_replace<PendingDestroy>(i_entity); return;
        }
        
        bool destroyed = false;
        players.each([&](const auto& p_trans, const auto& p_col, auto& p_data) {
            if (!destroyed && CheckCollisionSpheres(i_trans.position, i_col.radius, p_trans.position, p_col.radius)) {
                if (p_data.iFrameTimer <= 0.0f) {
                    p_data.hp -= config.combat.inkDamage;
                    p_data.hitShake = 0.4f; 
                    p_data.iFrameTimer = 1.0f; 
                }
                registry.emplace_or_replace<PendingDestroy>(i_entity);
                destroyed = true; 
            }
        });
        if (destroyed) return;

        for (auto wallEntity : wallGrid.GetNearby(i_trans.position)) {
            if (destroyed) break;
            const auto& w_col = registry.get<Collider>(wallEntity);
            if (CheckCollisionBoxSphere(w_col.cachedBox, i_trans.position, i_col.radius)) {
                registry.emplace_or_replace<PendingDestroy>(i_entity);
                destroyed = true;
            }
        }
    });

    // 4. Boss Body vs Player
    players.each([&](const auto& p_trans, const auto& p_col, auto& p_data) {
        if (p_data.iFrameTimer > 0.0f) return;
        
        // Head
        bossHeads.each([&](const auto& b_trans, const auto& b_col, const auto&) {
            if (CheckCollisionSpheres(p_trans.position, p_col.radius, b_trans.position, b_col.radius)) {
                p_data.hp -= config.monster1.bodyDamage; p_data.hitShake = 0.6f; p_data.iFrameTimer = 1.0f; 
            }
        });

        // Segments
        bossSegments.each([&](const auto& s_trans, const auto& s_col, const auto&) {
            if (CheckCollisionSpheres(p_trans.position, p_col.radius, s_trans.position, s_col.radius)) {
                p_data.hp -= config.monster1.bodyDamage; p_data.hitShake = 0.6f; p_data.iFrameTimer = 1.0f; 
            }
        });
    });
}

void lifetime_system(entt::registry& registry) {
    const float dt = registry.ctx().get<float>();
    registry.view<Lifetime>().each([&](auto entity, auto& life) {
        life.remaining -= dt;
        if (life.remaining <= 0.0f) registry.emplace_or_replace<PendingDestroy>(entity);
    });
}

void cleanup_system(entt::registry& registry) {
    auto view = registry.view<PendingDestroy>();
    registry.destroy(view.begin(), view.end());
}

void game_state_system(entt::registry& registry, int& gameState) {
    if (gameState != 0) return;

    bool monster1Alive = false;
    registry.view<Monster1Data>().each([&](auto entity, const auto& monster1Data) {
        monster1Alive = true;
        if (monster1Data.hp <= 0) {
            registry.emplace_or_replace<PendingDestroy>(entity);
            // Destroy all segments when head dies
            registry.view<BossSegment>().each([&](auto seg_ent, const auto&) { registry.emplace_or_replace<PendingDestroy>(seg_ent); });
            gameState = 1; 
        }
    });

    if (!monster1Alive) gameState = 1;

    registry.view<PlayerData>().each([&](const auto& pData) {
        if (pData.hp <= 0) gameState = 2; 
    });
}

void camera_system(entt::registry& registry, Camera3D& camera) {
    auto& camState = registry.ctx().get<CameraState>();
    float dt = GetFrameTime(); 

    if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
        Vector2 mouseDelta = GetMouseDelta();
        camState.angle -= mouseDelta.x * 0.005f; 
        camState.pitch += mouseDelta.y * 0.005f;
        camState.pitch = Clamp(camState.pitch, -1.48f, 1.48f);
    }

    float scroll = GetMouseWheelMove();
    if (scroll != 0) {
        camState.distance -= scroll * 2.0f;
        camState.distance = Clamp(camState.distance, 5.0f, 50.0f);
    }

    Vector3 targetPos = {0, 0, 0};
    registry.view<Transform3D, PlayerData>().each([&](const auto& trans, const auto&) { targetPos = trans.position; });

    Vector3 dirToCamera = { sinf(camState.angle) * cosf(camState.pitch), sinf(camState.pitch), cosf(camState.angle) * cosf(camState.pitch) };
    Ray camRay = { targetPos, dirToCamera };
    float actualDistance = camState.distance; 

    registry.view<Collider, DestructibleWall>().each([&](const auto& w_col, const auto&) {
        RayCollision hit = GetRayCollisionBox(camRay, w_col.cachedBox);
        if (hit.hit && hit.distance < actualDistance) actualDistance = hit.distance - 0.5f;
    });

    if (actualDistance < 2.0f) actualDistance = 2.0f;

    float hDist = actualDistance * cosf(camState.pitch); 
    camera.position = { targetPos.x + sinf(camState.angle) * hDist, targetPos.y + sinf(camState.pitch) * actualDistance, targetPos.z + cosf(camState.angle) * hDist };
    camera.target = targetPos; 

    registry.view<PlayerData>().each([&](auto& pData) {
        if (pData.hitShake > 0.0f) {
            pData.hitShake -= dt;
            float intensity = pData.hitShake * 2.0f; 
            camera.target.x += (float)GetRandomValue(-10, 10) / 15.0f * intensity;
            camera.position.y += (float)GetRandomValue(-10, 10) / 15.0f * intensity;
        }
    });
}

// ============================================================================
// 6. RENDER & UI SYSTEMS
// ============================================================================

void render_system(entt::registry& registry, int gameState) {
    if (gameState == 0) {
        registry.view<Transform3D, ReticleTag>().each([](const auto& transform) {
            DrawCircle3D(transform.position, 1.2f, Vector3{1, 0, 0}, 90.0f, Fade(RED, 0.5f));
            DrawLine3D(transform.position, Vector3{transform.position.x, 1.0f, transform.position.z}, Fade(RED, 0.3f));
            DrawCircle3D(Vector3{transform.position.x, 1.1f, transform.position.z}, 0.5f, Vector3{1, 0, 0}, 90.0f, Fade(DARKGRAY, 0.8f));
        });
    }

    // Reset Boss Segment tint back to normal (if it flashed red from damage)
    registry.view<MeshRenderer, BossSegment>().each([](auto& renderer, const auto&) { renderer.tint = PURPLE; });

    registry.view<Transform3D, MeshRenderer>().each([](const auto& trans, const auto& renderer) {
        switch (renderer.assetId) {
            case AssetID::PlayerHull:
            case AssetID::Debris:
                DrawCube(trans.position, renderer.scale.x, renderer.scale.y, renderer.scale.z, renderer.tint);
                DrawCubeWires(trans.position, renderer.scale.x, renderer.scale.y, renderer.scale.z, BLACK);
                break;
            case AssetID::Monster1Head:
            case AssetID::Monster1Body:
                DrawSphere(trans.position, renderer.scale.x, renderer.tint);
                DrawSphereWires(trans.position, renderer.scale.x * 1.05f, 16, 16, BLACK); 
                break;
            case AssetID::Torpedo:
            case AssetID::InkBlast:
            case AssetID::Bubble:
                DrawSphere(trans.position, renderer.scale.x, renderer.tint);
                break;
            case AssetID::SeaWall:
            case AssetID::CurrentZone:
                DrawCube(trans.position, renderer.scale.x, renderer.scale.y, renderer.scale.z, renderer.tint);
                DrawCubeWires(trans.position, renderer.scale.x, renderer.scale.y, renderer.scale.z, Fade(BLACK, 0.5f));
                break;
            default: break;
        }
    });

    registry.view<Transform3D, WeaponData>().each([](const auto& trans, const auto& weapon) {
        Vector3 muzzlePos = { trans.position.x - sinf(trans.yaw) * weapon.forwardOffset, trans.position.y, trans.position.z - cosf(trans.yaw) * weapon.forwardOffset };
        DrawCube(muzzlePos, 0.8f, 0.6f, 0.8f, DARKGRAY);
        DrawCubeWires(muzzlePos, 0.8f, 0.6f, 0.8f, BLACK);
    });
}

void ui_system(entt::registry& registry, entt::entity player, int gameState) {
    if (gameState == 0) {
        auto& p_data = registry.get<PlayerData>(player);
        DrawText(TextFormat("HULL INTEGRITY: %d%%", p_data.hp), 20, 20, 20, (p_data.hp > 30) ? GREEN : RED);
        DrawText("WASD: Move | SPACE/C: Up/Down | L-SHIFT: Dash", 20, 50, 20, LIGHTGRAY);
        DrawText("LEFT CLICK: Fire | RIGHT CLICK: Spin Camera | SCROLL: Zoom", 20, 75, 20, ORANGE);

        // Dash Cooldown UI
        if (p_data.dashCooldownTimer > 0) DrawRectangle(20, 100, (int)(100 * (1.0f - p_data.dashCooldownTimer/1.5f)), 10, BLUE);
        else DrawRectangle(20, 100, 100, 10, SKYBLUE);

        registry.view<Monster1Data>().each([](const auto& monster1Data) {
            float healthPct = (float)monster1Data.hp / monster1Data.maxHp;
            int barWidth = 600;
            int barX = GetScreenWidth() / 2 - barWidth / 2;
            DrawText(monster1Data.phase == 1 ? "LEVIATHAN" : "ENRAGED LEVIATHAN", barX, GetScreenHeight() - 60, 20, monster1Data.phase == 1 ? PURPLE : RED);
            DrawRectangle(barX, GetScreenHeight() - 30, barWidth, 20, DARKGRAY);
            DrawRectangle(barX, GetScreenHeight() - 30, barWidth * healthPct, 20, monster1Data.phase == 1 ? PURPLE : RED);
        });
    } 
    else if (gameState == 1) DrawText("LEVIATHAN DEFEATED!", GetScreenWidth()/2 - 250, GetScreenHeight()/2, 40, GREEN);
    else if (gameState == 2) DrawText("HULL BREACH. YOU SANK.", GetScreenWidth()/2 - 280, GetScreenHeight()/2, 40, RED);
}

// ============================================================================
// 7. MAIN LOOP
// ============================================================================
int main() {
    InitWindow(1024, 768, "Abyssal Leviathan - Professional Refactor");
    SetTargetFPS(FPS);

    entt::registry registry;
    GameConfig config; 
    registry.ctx().emplace<GameConfig>(config);
    registry.ctx().emplace<CameraState>();
    registry.ctx().emplace<float>(GetFrameTime()); 

    Camera3D camera = { 0 };
    camera.position = (Vector3){ 0.0f, 20.0f, 25.0f };
    camera.target = (Vector3){ 0.0f, 0.0f, 0.0f };
    camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.fovy = 60.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    // --- SETUP PLAYER ---
    auto player = registry.create();
    registry.emplace<Transform3D>(player, Vector3{0, 10.0f, 15.0f}, 0.0f);
    registry.emplace<Velocity>(player, Vector3{0, 0, 0});
    registry.emplace<MeshRenderer>(player, AssetID::PlayerHull, Vector3{2.0f, 2.0f, 2.0f}, YELLOW);
    registry.emplace<Collider>(player, COL_SPHERE, 1.0f, Vector3{0,0,0}, BoundingBox{});
    registry.emplace<PlayerData>(player, 0.0f, config.player.maxHp, 0.0f, 0.0f, 0.0f, 0.0f); 
    registry.emplace<WeaponData>(player, 1.2f);
    registry.emplace<WallRepelTag>(player);

    // --- SETUP SEGMENTED BOSS ---
    auto monsterHead = registry.create();
    registry.emplace<Transform3D>(monsterHead, Vector3{0, 5.0f, -20.0f}, 0.0f);
    registry.emplace<Velocity>(monsterHead, Vector3{0, 0, 0});
    registry.emplace<MeshRenderer>(monsterHead, AssetID::Monster1Head, Vector3{4.0f, 4.0f, 4.0f}, PURPLE);
    registry.emplace<Collider>(monsterHead, COL_SPHERE, 4.0f, Vector3{0,0,0}, BoundingBox{});
    registry.emplace<Monster1Data>(monsterHead, config.monster1.maxHp, config.monster1.maxHp, config.monster1.attackRatePhase1, 1);
    registry.emplace<WallRepelTag>(monsterHead);

    entt::entity previousSegment = monsterHead;
    for (int i = 0; i < config.monster1.segmentCount; i++) {
        auto segment = registry.create();
        float scale = 3.5f - (i * 0.15f); // Tail gets smaller
        registry.emplace<Transform3D>(segment, Vector3{0, 5.0f, -20.0f + (i * 2.0f)}, 0.0f);
        registry.emplace<MeshRenderer>(segment, AssetID::Monster1Body, Vector3{scale, scale, scale}, PURPLE);
        registry.emplace<Collider>(segment, COL_SPHERE, scale, Vector3{0,0,0}, BoundingBox{});
        registry.emplace<BossSegment>(segment, previousSegment, config.monster1.segmentDist);
        previousSegment = segment;
    }

    // --- SETUP RETICLE ---
    auto reticle = registry.create();
    registry.emplace<Transform3D>(reticle, Vector3{0, 0, 0}, 0.0f);
    registry.emplace<ReticleTag>(reticle);

    // --- SETUP WALLS ---
    Vector3 wallPositions[] = { {-35.0f, 5.0f, 0.0f}, {35.0f, 5.0f, 0.0f}, {0.0f, 5.0f, -35.0f}, {0.0f, 5.0f, 35.0f} };
    Vector3 wallSizes[] = { {10.0f, 20.0f, 80.0f}, {10.0f, 20.0f, 80.0f}, {80.0f, 20.0f, 10.0f}, {80.0f, 20.0f, 10.0f} };
    for (int i = 0; i < 4; i++) {
        auto wall = registry.create();
        registry.emplace<Transform3D>(wall, wallPositions[i], 0.0f);
        registry.emplace<DestructibleWall>(wall, 100); // 100 HP!
        registry.emplace<MeshRenderer>(wall, AssetID::SeaWall, wallSizes[i], Fade(DARKGRAY, 0.3f));
        BoundingBox cachedBox = {
            { wallPositions[i].x - wallSizes[i].x/2, wallPositions[i].y - wallSizes[i].y/2, wallPositions[i].z - wallSizes[i].z/2 },
            { wallPositions[i].x + wallSizes[i].x/2, wallPositions[i].y + wallSizes[i].y/2, wallPositions[i].z + wallSizes[i].z/2 }
        };
        registry.emplace<Collider>(wall, COL_BOX, 0.0f, wallSizes[i], cachedBox);
    }

    // --- SETUP CURRENT ZONE ---
    auto currentZone = registry.create();
    Vector3 cPos = { -15.0f, 5.0f, -15.0f };
    Vector3 cSize = { 15.0f, 20.0f, 15.0f };
    registry.emplace<Transform3D>(currentZone, cPos, 0.0f);
    registry.emplace<OceanCurrent>(currentZone, Vector3{50.0f, 0.0f, 50.0f}); // Pushes diagonally
    registry.emplace<MeshRenderer>(currentZone, AssetID::CurrentZone, cSize, Fade(BLUE, 0.1f));
    BoundingBox cBox = { { cPos.x - cSize.x/2, cPos.y - cSize.y/2, cPos.z - cSize.z/2 }, { cPos.x + cSize.x/2, cPos.y + cSize.y/2, cPos.z + cSize.z/2 } };
    registry.emplace<Collider>(currentZone, COL_BOX, 0.0f, cSize, cBox);

    int gameState = 0; 
    // float timeAccumulator = 0.0f;

    while (!WindowShouldClose()) {
        float frameDt = GetFrameTime();
        // timeAccumulator += frameDt;

        if (gameState == 0) {
            player_input_system(registry);
            targeting_system(registry, camera); 
            player_combat_system(registry);
            camera_system(registry, camera);
            

                
			monster1_ai_system(registry);
			environment_system(registry);
			physics_system(registry);
			collision_system(registry);
			lifetime_system(registry);
                

            registry.ctx().get<float>() = frameDt; // Restore dynamic dt for rendering

            game_state_system(registry, gameState);
            cleanup_system(registry);
        }

        BeginDrawing();
        ClearBackground(Color{ 5, 15, 25, 255 }); 

        BeginMode3D(camera);
        DrawPlane(Vector3{0, 0, 0}, Vector2{200, 200}, Color{ 5, 30, 30, 255 }); 
        render_system(registry, gameState); 
        EndMode3D();

        ui_system(registry, player, gameState);

        EndDrawing();
    }
    
    CloseWindow();
    return 0;
}