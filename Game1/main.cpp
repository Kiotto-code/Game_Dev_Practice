#include <raylib.h>
#include <entt/entt.hpp>
#include <iostream>

struct Position {
    float x;
    float y;
};

struct Velocity {
    float dx;
    float dy;
};

int main() {
    // Initialize Raylib
    InitWindow(800, 450, "raylib + EnTT");
    SetTargetFPS(60);

    // Initialize EnTT Registry
    entt::registry registry;

    // Create an Entity
    auto player = registry.create();
    registry.emplace<Position>(player, 400.0f, 225.0f);
    registry.emplace<Velocity>(player, 2.0f, 1.5f);

    while (!WindowShouldClose()) {
        // 1. Update System (ECS)
        auto view = registry.view<Position, Velocity>();
        for (auto entity : view) {
            auto &pos = view.get<Position>(entity);
            auto &vel = view.get<Velocity>(entity);
            
            pos.x += vel.dx;
            pos.y += vel.dy;

            // Simple Screen Bounce
            if (pos.x <= 0 || pos.x >= 800) vel.dx *= -1;
            if (pos.y <= 0 || pos.y >= 450) vel.dy *= -1;
        }

        // 2. Render System
        BeginDrawing();
        ClearBackground(RAYWHITE);
        
        auto renderView = registry.view<Position>();
        for (auto entity : renderView) {
            auto &pos = renderView.get<Position>(entity);
            DrawCircle((int)pos.x, (int)pos.y, 20, MAROON);
        }

        DrawText("EnTT Entity moving with Raylib!", 10, 10, 20, DARKGRAY);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}