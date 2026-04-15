// #include <entt.hpp>
// #include <raylib.h>
// #include <raymath.h>
// #include <rcamera.h>
// #include <vector>
// #include <cmath>

// struct GameConfig{

// };

// struct CameraState{

// };

// struct TransForm3D{
	
// };

// struct MeshRenderer{

// };

// struct Collider{
// 	AssetID 	type;
// 	float		radius;
// 	Vector3 	size;
// 	BoundingBox	cachedBox;
// };

// enum class AssetID{
// 	PlayerHull,

// };

// enum ColliderType{
// 	COLSPHERE,
// 	COLBOX
// }

// void camera_config(Camera3D camera){
// 	camera.position = (Vector3){0, 0, 10};
// 	camera.target = (Vector3){0, 0, 0}; // facing to the negative z-axis
// 	camera.up = (Vector3){0, 1, 0};
// 	camera.fovy = 45.0f;
// 	camera.projection = CAMERA_PERSPECTIVE;
// }

// void create_player(entt::registry& registry){
// 	auto player = registry.create();
// 	registry.emplace<TransForm3D>(player, (Vector3){0, 0, 0});
// 	registry.emplace<Vector3>(player, Vector3{0});
// 	// registry.emplace<MeshRenderer>(player, )
// 	registry.emplace<MeshRenderer>(player, AssetID::PlayerHull, (Vector3){10, 10, 10}, YELLOW);
// 	registry.emplace<Collider>(player, COLSPHERE, )

// }

// int main(){
// 	InitWindow(1024, 768, "GAME");
// 	SetTargetFPS(120);

// 	entt::registry registry;
// 	GameConfig config;
// 	registry.ctx().emplace<GameConfig>(config);
// 	registry.ctx().emplace<CameraState>();
// 	registry.ctx().emplace<float>(GetTimeFrame());

// 	auto player = registry.create();
// 	create_player(registry);

// 	Camera3D camera = {0};
// 	camera_config(camera)
// }