#pragma once
#include <iostream>
#include "../../SDK/Engine.h"
#include "../../SDK/Entity.h"
#include "../../SDK/Surface.h"
#include "../../color.h"
struct Grenade_t
{
	Entity* entity;
	std::vector<Vector> positions;
	float addTime;
};

namespace CGrenade
{
	bool checkGrenades(Entity* ent);
	void addGrenade(Grenade_t grenade);
	void updatePosition(Entity* ent, Vector position);
	void draw(Color color);
	std::vector<Grenade_t> grenades;
	int findGrenade(Entity* ent);
};