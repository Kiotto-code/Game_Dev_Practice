#include <entt.hpp>
#include <raylib.h>

struct Transform2D{
	Vector2 position;
};

struct Collider{
	Vector2 size;
};

struct Velocity{
	Vector2 speed;
};

struct TorpedoTag{};
struct WallTag{};

void UpdateCollisions(entt::registry& registry) {
    // Get all entities that are Torpedoes and have a Transform2D and Collider
    auto torpedoView = registry.view<TorpedoTag, Transform2D, Collider>();
    // Get all entities that are Walls and have a Transform2D and Collider
    auto wallView = registry.view<WallTag, Transform2D, Collider>();

    // We use a separate vector to store entities to destroy 
    // to avoid modifying the registry while iterating through it.
    std::vector<entt::entity> toDestroy;

    for (auto torpedoEntity : torpedoView) {
        const auto& tTransform2D = torpedoView.get<Transform2D>(torpedoEntity);
        const auto& tCollider = torpedoView.get<Collider>(torpedoEntity);
        
        Rectangle tRect = { tTransform2D.position.x, tTransform2D.position.y, tCollider.size.x, tCollider.size.y };

        for (auto wallEntity : wallView) {
            const auto& wTransform2D = wallView.get<Transform2D>(wallEntity);
            const auto& wCollider = wallView.get<Collider>(wallEntity);
            
            Rectangle wRect = { wTransform2D.position.x, wTransform2D.position.y, wCollider.size.x, wCollider.size.y };

            // Raylib's built-in AABB collision check
            if (CheckCollisionRecs(tRect, wRect)) {
                toDestroy.push_back(torpedoEntity);
                toDestroy.push_back(wallEntity);
                break; // Stop checking this torpedo against other walls
            }
        }
    }

    // Safely destroy the entities
    for (auto entity : toDestroy) {
        if (registry.valid(entity)) {
            registry.destroy(entity);
        }
    }
}

int main(){
	InitWindow(800, 600, "Test");
	SetTargetFPS(60);	
	entt::registry registry;

	auto wall = registry.create();
	registry.emplace<WallTag>(wall);
	registry.emplace<Transform2D>(wall, Vector2{600.0f, 250.0f});
	registry.emplace<Collider>(wall, Vector2{50.0f, 100.0f});

	auto torpedo = registry.create();
	registry.emplace<TorpedoTag>(torpedo);
	registry.emplace<Collider>(torpedo, Vector2{30.0f, 15.0f});
	registry.emplace<Velocity>(torpedo, Vector2{5.0f, 0.0f});
	registry.emplace<Transform2D>(torpedo, Vector2{100.0f, 250.0f});

	while(!WindowShouldClose()){
		auto moveView = registry.view<TorpedoTag, Transform2D, Velocity>();
		for (auto entity : moveView) {
            auto& transform2D = moveView.get<Transform2D>(entity);
            const auto& velocity = moveView.get<Velocity>(entity);
            transform2D.position.x += velocity.speed.x;
            transform2D.position.y += velocity.speed.y;
        }

		UpdateCollisions(registry);

		// 2. DRAW LOGIC
        BeginDrawing();
        ClearBackground(RAYWHITE);

        // Draw Walls
        auto drawWalls = registry.view<WallTag, Transform2D, Collider>();
        for (auto entity : drawWalls) {
            const auto& t = drawWalls.get<Transform2D>(entity);
            const auto& c = drawWalls.get<Collider>(entity);
            DrawRectangleV(t.position, c.size, DARKGRAY);
        }

        // Draw Torpedoes
        auto drawTorpedoes = registry.view<TorpedoTag, Transform2D, Collider>();
        for (auto entity : drawTorpedoes) {
            const auto& t = drawTorpedoes.get<Transform2D>(entity);
            const auto& c = drawTorpedoes.get<Collider>(entity);
            DrawRectangleV(t.position, c.size, RED);
        }

        DrawText("Torpedo is moving towards the wall...", 10, 10, 20, DARKGRAY);

        EndDrawing();
    }

    CloseWindow();
    return 0;
};