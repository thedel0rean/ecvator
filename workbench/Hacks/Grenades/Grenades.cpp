#include "Grenades.h"

bool CGrenade::checkGrenades(Entity* ent) //to check if we already added this grenade
{
	for (Grenade_t grenade : grenades)
	{
		if (grenade.entity == ent)
			return false;
	}

	return true;
}

static bool worldToScreen(const Vector& in, Vector& out) noexcept
{
	const auto& matrix = interfaces->engine->worldToScreenMatrix();
	float w = matrix._41 * in.x + matrix._42 * in.y + matrix._43 * in.z + matrix._44;

	if (w > 0.001f) {
		const auto [width, height] = interfaces->surface->getScreenSize();
		out.x = width / 2 * (1 + (matrix._11 * in.x + matrix._12 * in.y + matrix._13 * in.z + matrix._14) / w);
		out.y = height / 2 * (1 - (matrix._21 * in.x + matrix._22 * in.y + matrix._23 * in.z + matrix._24) / w);
		out.z = 0.0f;
		return true;
	}
	return false;
}

void CGrenade::addGrenade(Grenade_t grenade)
{
	grenades.push_back(grenade);
}

void CGrenade::updatePosition(Entity* ent, Vector position)
{
	grenades.at(findGrenade(ent)).positions.push_back(position);
}

void CGrenade::draw(Color color)
{
	for (size_t i = 0; i < grenades.size(); i++)
	{
		if (grenades.at(i).addTime + 20.f < memory->globalVars->realtime)
		{
			continue;
		}
		if (grenades.at(i).addTime + 2.5f < memory->globalVars->realtime)
		{
			if (grenades.at(i).positions.size() < 1) continue;

			grenades.at(i).positions.erase(grenades.at(i).positions.begin());
		}

		for (size_t j = 1; j < grenades.at(i).positions.size(); j++)
		{
			Vector sPosition;
			Vector sLastPosition;
			if (worldToScreen(grenades.at(i).positions.at(j), sPosition) && worldToScreen(grenades.at(i).positions.at(j - 1), sLastPosition))
			{
				interfaces->surface->setDrawColor(color.r(), color.g(), color.b(), color.a());
				interfaces->surface->drawLine(sPosition.x, sPosition.y, sLastPosition.x, sLastPosition.y);
			}
		}
	}
}

int CGrenade::findGrenade(Entity* ent)
{
	for (size_t i = 0; i < grenades.size(); i++)
	{
		if (grenades.at(i).entity == ent)
			return i;
	}

	return 0;
}