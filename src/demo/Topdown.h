#pragma once
#define NOMINMAX
#include "../HalesiaEngine.h"

#include "../core/Scene.h"
#include "../core/Camera.h"
#include "../core/Object.h"

#include "../physics/RigidBody.h"
#include "../physics/Shapes.h"
#include "../physics/Physics.h"

#include "../renderer/Material.h"

#include "../io/SceneLoader.h"

#include "../glm.h"

#include "../system/Input.h"

static constexpr float fireRate = 1.0f / 4.0f * 1000.0f;

static constexpr glm::vec3 UnitXVector = glm::vec3(1, 0, 0);
static constexpr glm::vec3 UnitYVector = glm::vec3(0, 1, 0);
static constexpr glm::vec3 UnitZVector = glm::vec3(0, 0, 1);

class FollowCam : public Camera
{
public:
	Object* objToFollow = nullptr;

	void Update(Window* window, float delta) override;
};

class Ship : public Object
{
public:
	Object* baseBullet = nullptr;
	Window* mouse = nullptr;

	float timeSinceLastShot = 999.0f;
	int health = 3;

	void Start() override;
	void Update(float delta) override;

private:
	void SpawnBullet();
	glm::vec3 GetMousePosIn3D();
};

class Bullet : public Object
{
public:
	glm::vec3 forward = glm::vec3(0);
	float timeAlive = 0;

	void Update(float delta) override;
	void OnCollisionEnter(Object* object) override;
};

class Enemy : public Object
{
public:
	Object* player = nullptr;
	Object* baseBullet = nullptr;

	float timeSinceLastShot = 999.0f;
	int health = 3;

	void Update(float delta) override;

private:
	void SpawnBullet();
};

void FollowCam::Update(Window* window, float delta)
{
	position = objToFollow->transform.position;
	position.y = 8;
	position.z += 5;
	pitch = glm::radians(-45.0f);
	UpdateVectors();
}

void Enemy::Update(float delta)
{
	if (player == nullptr)
		return;

	if (!health)
		this->Free();

	if (Input::IsKeyPressed(VirtualKey::LeftMouseButton) && Input::IsKeyPressed(VirtualKey::LeftControl) && timeSinceLastShot > fireRate)
	{
		SpawnBullet();
		timeSinceLastShot = 0;
	}
	else timeSinceLastShot += delta;
}

void Enemy::SpawnBullet()
{
	Object* newBullet = GetParentScene()->DuplicateObject<Bullet>(baseBullet, "bullet");
	newBullet->name = std::to_string(newBullet->handle);
	Bullet* script = newBullet->GetScript<Bullet>();
	script->forward = glm::normalize(player->transform.position - transform.position);
	newBullet->transform.position = transform.position + glm::normalize(player->transform.position - transform.position) * 3.0f;
	newBullet->rigid.ForcePosition(newBullet->transform);
	newBullet->state = OBJECT_STATE_VISIBLE;
}

void Bullet::Update(float delta)
{
	timeAlive += delta;
	if (timeAlive > 3000)
	{
		Free();
		return;
	}

	glm::vec3 forward2D = glm::normalize(glm::vec3(forward.x, 0, forward.z));
	transform.rotation.x = glm::degrees(-asin(glm::dot(forward, UnitYVector)));
	transform.rotation.y = glm::degrees(acos(glm::dot(forward2D, UnitXVector))) + 90;
	transform.rotation.z = 0;
	if (glm::dot(forward, UnitZVector) > 0)
		transform.rotation.y = 360 - transform.rotation.y;

	transform.position += forward2D * delta * 0.01f;
	rigid.MovePosition(transform);
}

void Bullet::OnCollisionEnter(Object* object)
{
	if (object->name != "box") return;
	object->GetScript<Enemy>()->health--;
	Free();
}

void Ship::Start()
{
	transform.scale = glm::vec3(0.75f);
	mouse = HalesiaEngine::GetInstance()->GetEngineCore().window;
}

void Ship::Update(float delta)
{
	glm::vec3 dest = GetMousePosIn3D();
	glm::vec3 forward = glm::normalize(glm::vec3(dest.x, 0, dest.z) - glm::vec3(transform.position.x, 0, transform.position.z));

	transform.rotation = glm::vec3(0, glm::degrees(acos(glm::dot(forward, UnitXVector))), 0);
	if (glm::dot(forward, UnitZVector) > 0)
		transform.rotation.y = 270 - transform.rotation.y;
	else transform.rotation.y -= 90;

	if (!Input::IsKeyPressed(VirtualKey::LeftControl))
		return;

	if (Input::IsKeyPressed(VirtualKey::W))
		transform.position.z -= delta * 0.01f;
	if (Input::IsKeyPressed(VirtualKey::S))
		transform.position.z += delta * 0.01f;
	if (Input::IsKeyPressed(VirtualKey::A))
		transform.position.x -= delta * 0.01f;
	if (Input::IsKeyPressed(VirtualKey::D))
		transform.position.x += delta * 0.01f;

	if (Input::IsKeyPressed(VirtualKey::LeftMouseButton) && timeSinceLastShot > fireRate)
	{
		SpawnBullet();
		timeSinceLastShot = 0;
	}
	else timeSinceLastShot += delta;

	//rigid.MovePosition(transform);
}

void Ship::SpawnBullet()
{
	Object* newBullet = GetParentScene()->DuplicateObject<Bullet>(baseBullet, "bullet");
	newBullet->name = std::to_string(newBullet->handle);
	Bullet* script = newBullet->GetScript<Bullet>();
	script->forward = transform.GetForward();
	newBullet->transform.position = transform.position + transform.GetForward() * 2.0f;
	newBullet->rigid.ForcePosition(newBullet->transform);
	newBullet->state = OBJECT_STATE_VISIBLE;
}

glm::vec3 Ship::GetMousePosIn3D()
{
	int x, y;
	mouse->GetAbsoluteCursorPosition(x, y);
	glm::vec2 inUv = glm::vec2(x, y) / glm::vec2((float)mouse->GetWidth(), (float)mouse->GetHeight());
	glm::vec2 uv = inUv * 2.0f - 1.0f;

	glm::mat4 invView = glm::inverse(GetParentScene()->camera->GetViewMatrix());
	glm::vec4 target = glm::inverse(GetParentScene()->camera->GetProjectionMatrix()) * glm::vec4(uv.x, uv.y, 1, 1);
	glm::vec3 origin = invView * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
	glm::vec3 dir = invView * glm::vec4(glm::normalize(glm::vec3(target.x, target.y, target.z)), 0);

	Physics::RayHitInfo hitInfo;
	Physics::CastRay(origin, dir, 100000.0f, hitInfo);

	return origin + dir * hitInfo.distance;
}

class Topdown : public Scene
{
	void Start() override
	{
		WriteScene();
	}
	/*void ReadScene()
	{
		camera = AddCustomCamera<FollowCam>();

		MaterialCreateInfo lightInfo{};
		lightInfo.albedo = "textures/glockAlbedo.png";
		lightInfo.isLight = true;
		Material lightMat = Material::Create(lightInfo);

		SceneLoader loader("scene.hsf");
		loader.LoadScene();
		for (MaterialCreationData& data : loader.materials)
			Mesh::materials.push_back(Material::Create(data));
		for (ObjectCreationData& data : loader.objects)
		{
			if (data.name == "ship")
			{
				Object* ship = AddCustomObject<Ship>(data);
				camera->GetScript<FollowCam>()->objToFollow = ship;
			}
			else if (data.name == "light")
			{
				Object* light = AddStaticObject(data);
				light->AwaitGeneration();
				light->meshes[0].SetMaterial(lightMat);
			}
			else if (data.name == "box")
			{
				Object* box = AddCustomObject<Enemy>(data);
			}
			else AddStaticObject(data)->AwaitGeneration();
		}
	}*/

	Object* baseBullet;
	void WriteScene()
	{
		camera = AddCustomCamera<FollowCam>();

		Object* ship = AddObject<Ship>(GenericLoader::LoadObjectFile("stdObj/ship.obj"));
		ship->name = "ship";
		ship->AwaitGeneration();
		Shape shape = Box(ship->mesh.extents);
		//ship->AddRigidBody(RIGID_BODY_KINEMATIC, shape);
		camera->GetScript<FollowCam>()->objToFollow = ship;

		Object* floor = AddObject(GenericLoader::LoadObjectFile("stdObj/cube.obj"));
		floor->AwaitGeneration();
		floor->name = "floor";
		floor->transform.scale = glm::vec3(20, 1, 20);
		floor->transform.position.y = -3;

		Shape floorShape = Box(glm::vec3(20, 1, 20));
		floor->SetRigidBody(RigidBody::Type::Static, floorShape);
		floor->rigid.ForcePosition(floor->transform);

		Object* light = DuplicateObject(floor, "light");
		light->transform.position.y = 10;

		MaterialCreateInfo lightInfo{};
		lightInfo.albedo = "textures/glockAlbedo.png";
		lightInfo.isLight = true;
		Material lightMat = Material::Create(lightInfo);
		light->mesh.SetMaterial(lightMat);

		MaterialCreateInfo boxInfo{};
		boxInfo.albedo = "textures/red.png";
		boxInfo.metallic = "textures/white.png";
		boxInfo.roughness = "textures/black.png";
		Material boxMat = Material::Create(boxInfo);
		boxMat.AwaitGeneration();

		Object* box = DuplicateObject<Enemy>(floor, "box");
		box->AwaitGeneration();
		box->transform.scale = glm::vec3(1, 1, 1);
		box->transform.position = glm::vec3(5, 0, 0);
		box->mesh.SetMaterial(boxMat);

		MaterialCreateInfo bulletInfo{};
		bulletInfo.albedo = "textures/uv.png";
		bulletInfo.isLight = true;
		Material bulletMat = Material::Create(bulletInfo);

		baseBullet = AddObject<Bullet>(GenericLoader::LoadObjectFile("stdObj/bullet.obj"));
		baseBullet->name = "bullet";
		baseBullet->SetRigidBody(RigidBody::Type::Kinematic, Box(baseBullet->mesh.extents));
		baseBullet->state = OBJECT_STATE_DISABLED;
		baseBullet->transform.position.y = -5;
		baseBullet->mesh.SetMaterial(bulletMat);
		baseBullet->rigid.ForcePosition(baseBullet->transform);

		ship->GetScript<Ship>()->baseBullet = baseBullet;
		box->GetScript<Enemy>()->baseBullet = baseBullet;
		box->GetScript<Enemy>()->player = ship;

		box->SetRigidBody(RigidBody::Type::Kinematic, Box(box->mesh.extents));

		//HSFWriter::WriteHSFScene(this, "scene.hsf");
	}

	void Update(float delta) override
	{

	}
};