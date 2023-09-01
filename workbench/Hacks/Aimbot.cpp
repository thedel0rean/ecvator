#include "../Config.h"
#include "../Interfaces.h"
#include "../Memory.h"

#include "../GameData.h"
#include "../Memory.h"
#include "../SDK/Angle.h"
#include "../SDK/ConVar.h"
#include "../SDK/GlobalVars.h"
#include "../SDK/PhysicsSurfaceProps.h"
#include "../SDK/Entity.h"
#include "../SDK/UserCmd.h"
#include "../SDK/Utils.h"
#include "../SDK/Vector.h"
#include "../SDK/WeaponId.h"
#include "../SDK/WeaponData.h"
#include "../SDK/GlobalVars.h"
#include "../SDK/LocalPlayer.h"
#include "../SDK/ModelInfo.h"

#include "Animations.h"
#include "Backtrack.h"
#include "Aimbot.h"
#include "EnginePrediction.h"
#include "Resolver.h"
#include "Tickbase.h"
#include "Misc.h"

#include <DirectXMath.h>
#include <iostream>
#include <cstdlib>
#include <vector>
#include "../SDK/Angle.h"
#include <omp.h>

static bool keyPressed = false;

int maxThreadNum = (std::thread::hardware_concurrency());

Vector Aimbot::calculateRelativeAngle(const Vector& source, const Vector& destination, const Vector& viewAngles) noexcept
{
	return ((destination - source).toAngle() - viewAngles).normalize();
}

namespace aim_utils
{
	float segmentToSegment(const Vector& s1, const Vector& s2, const Vector& k1, const Vector& k2) noexcept
	{
		static auto constexpr epsilon = 0.00000001f;

		auto u = s2 - s1;
		auto v = k2 - k1;
		auto w = s1 - k1;

		auto a = u.dotProduct(u); //-V525
		auto b = u.dotProduct(v);
		auto c = v.dotProduct(v);
		auto d = u.dotProduct(w);
		auto e = v.dotProduct(w);
		auto D = a * c - b * b;

		auto sn = 0.0f, sd = D;
		auto tn = 0.0f, td = D;

		if (D < epsilon)
		{
			sn = 0.0f;
			sd = 1.0f;
			tn = e;
			td = c;
		}
		else
		{
			sn = b * e - c * d;
			tn = a * e - b * d;

			if (sn < 0.0f)
			{
				sn = 0.0f;
				tn = e;
				td = c;
			}
			else if (sn > sd)
			{
				sn = sd;
				tn = e + b;
				td = c;
			}
		}

		if (tn < 0.0f)
		{
			tn = 0.0f;

			if (-d < 0.0f)
				sn = 0.0f;
			else if (-d > a)
				sn = sd;
			else
			{
				sn = -d;
				sd = a;
			}
		}
		else if (tn > td)
		{
			tn = td;

			if (-d + b < 0.0f)
				sn = 0.0f;
			else if (-d + b > a)
				sn = sd;
			else
			{
				sn = -d + b;
				sd = a;
			}
		}

		auto sc = fabs(sn) < epsilon ? 0.0f : sn / sd;
		auto tc = fabs(tn) < epsilon ? 0.0f : tn / td;

		auto dp = w + u * sc - v * tc;
		return dp.length();
	}

	bool intersectLineWithBb(Vector& start, Vector& end, Vector& min, Vector& max) noexcept
	{
		float d1, d2, f;
		auto start_solid = true;
		auto t1 = -1.0f, t2 = 1.0f;

		const float s[3] = { start.x, start.y, start.z };
		const float e[3] = { end.x, end.y, end.z };
		const float mi[3] = { min.x, min.y, min.z };
		const float ma[3] = { max.x, max.y, max.z };

		bool result = start_solid || (t1 < t2 && t1 >= 0.0f);
#pragma omp parallel for num_threads(maxThreadNum)
		for (auto i = 0; i < 6; ++i)
		{
			if (i >= 3)
			{
				const auto j = i - 3;

				d1 = s[j] - ma[j];
				d2 = d1 + e[j];
			}
			else
			{
				d1 = -s[i] + mi[i];
				d2 = d1 - e[i];
			}

			if (d1 > 0.0f && d2 > 0.0f)
				result = false;

			if (d1 <= 0.0f && d2 <= 0.0f)
				continue;

			if (d1 > 0)
				start_solid = false;

			if (d1 > d2)
			{
				f = d1;
				if (f < 0.0f)
					f = 0.0f;

				f /= d1 - d2;
				if (f > t1)
					t1 = f;
			}
			else
			{
				f = d1 / (d1 - d2);
				if (f < t2)
					t2 = f;
			}
		}

		return result;
	}

	void inline sinCos(float radians, float* sine, float* cosine)
	{
		*sine = sin(radians);
		*cosine = cos(radians);
	}

	Vector vectorRotate(Vector& in1, Vector& in2) noexcept
	{
		auto vector_rotate = [](const Vector& in1, const matrix3x4& in2)
		{
			return Vector(in1.dotProduct(in2[0]), in1.dotProduct(in2[1]), in1.dotProduct(in2[2]));
		};

		auto angleMatrix = [](const Vector& angles, matrix3x4& matrix)
		{
			float sr, sp, sy, cr, cp, cy;

			sinCos(Helpers::deg2rad(angles[1]), &sy, &cy);
			sinCos(Helpers::deg2rad(angles[0]), &sp, &cp);
			sinCos(Helpers::deg2rad(angles[2]), &sr, &cr);

			// matrix = (YAW * PITCH) * ROLL
			matrix[0][0] = cp * cy;
			matrix[1][0] = cp * sy;
			matrix[2][0] = -sp;

			float crcy = cr * cy;
			float crsy = cr * sy;
			float srcy = sr * cy;
			float srsy = sr * sy;
			matrix[0][1] = sp * srcy - crsy;
			matrix[1][1] = sp * srsy + crcy;
			matrix[2][1] = sr * cp;

			matrix[0][2] = (sp * crcy + srsy);
			matrix[1][2] = (sp * crsy - srcy);
			matrix[2][2] = cr * cp;

			matrix[0][3] = 0.0f;
			matrix[1][3] = 0.0f;
			matrix[2][3] = 0.0f;
		};

		matrix3x4 m;
		angleMatrix(in2, m);
		return vector_rotate(in1, m);
	}

	void vectorITransform(const Vector& in1, const matrix3x4& in2, Vector& out) noexcept
	{
		out.x = (in1.x - in2[0][3]) * in2[0][0] + (in1.y - in2[1][3]) * in2[1][0] + (in1.z - in2[2][3]) * in2[2][0];
		out.y = (in1.x - in2[0][3]) * in2[0][1] + (in1.y - in2[1][3]) * in2[1][1] + (in1.z - in2[2][3]) * in2[2][1];
		out.z = (in1.x - in2[0][3]) * in2[0][2] + (in1.y - in2[1][3]) * in2[1][2] + (in1.z - in2[2][3]) * in2[2][2];
	}

	void vectorIRotate(Vector in1, matrix3x4 in2, Vector& out) noexcept
	{
		out.x = in1.x * in2[0][0] + in1.y * in2[1][0] + in1.z * in2[2][0];
		out.y = in1.x * in2[0][1] + in1.y * in2[1][1] + in1.z * in2[2][1];
		out.z = in1.x * in2[0][2] + in1.y * in2[1][2] + in1.z * in2[2][2];
	}

	static int ClipRayToHitbox(const Ray& ray, StudioBbox* hitbox, matrix3x4& matrix, Trace& trace)
	{
		static auto fn = Memory::findPattern(c_xor("client.dll"), c_xor("\x55\x8B\xEC\x83\xE4\xF8\xF3\x0F\x10\x42"), true);

		trace.fraction = 1.0f;
		trace.startSolid = false;

		return reinterpret_cast <int(__fastcall*)(const Ray&, StudioBbox*, matrix3x4&, Trace&)> (fn)(ray, hitbox, matrix, trace);
	}

	bool canHitHitbox(const matrix3x4 matrix[MAXSTUDIOBONES], int iHitbox, StudioHitboxSet* set, const Vector& start, const Vector& end)
	{
		auto VectorTransform_Wrapper = [](const Vector& in1, const matrix3x4 in2, Vector& out)
		{
			auto VectorTransform = [](const float* in1, const matrix3x4 in2, float* out)
			{
				auto DotProducts = [](const float* v1, const float* v2)
				{
					return v1[0] * v2[0] + v1[1] * v2[1] + v1[2] * v2[2];
				};
				out[0] = DotProducts(in1, in2[0]) + in2[0][3];
				out[1] = DotProducts(in1, in2[1]) + in2[1][3];
				out[2] = DotProducts(in1, in2[2]) + in2[2][3];
			};
			VectorTransform(&in1.x, in2, &out.x);
		};

		StudioBbox* hitbox = set->getHitbox(iHitbox);
		if (!hitbox)
			return false;

		if (hitbox->capsuleRadius == -1.f)
			return false;

		Vector mins, maxs;
		const auto isCapsule = hitbox->capsuleRadius != -1.f;

		if (isCapsule)
		{
			VectorTransform_Wrapper(hitbox->bbMin, matrix[hitbox->bone], mins);
			VectorTransform_Wrapper(hitbox->bbMax, matrix[hitbox->bone], maxs);
			const auto dist = segmentToSegment(start, end, mins, maxs);

			if (dist < hitbox->capsuleRadius)
				return true;
		}
		else
		{
			VectorTransform_Wrapper(vectorRotate(hitbox->bbMin, hitbox->offsetOrientation), matrix[hitbox->bone], mins);
			VectorTransform_Wrapper(vectorRotate(hitbox->bbMax, hitbox->offsetOrientation), matrix[hitbox->bone], maxs);

			vectorITransform(start, matrix[hitbox->bone], mins);
			vectorIRotate(end, matrix[hitbox->bone], maxs);

			if (intersectLineWithBb(mins, maxs, hitbox->bbMin, hitbox->bbMax))
				return true;
		}

		return false;
	}

	bool Hitchance(Entity* localPlayer, Entity* entity, StudioHitboxSet* set, const matrix3x4 matrix[MAXSTUDIOBONES], Entity* activeWeapon, const Vector& destination, UserCmd* cmd, const int hitChance)
	{
		static auto isSpreadEnabled = interfaces->cvar->findVar(c_xor("weapon_accuracy_nospread"));
		if (!hitChance || isSpreadEnabled->getInt() >= 1)
			return true;

		constexpr int maxSeed = 255;

		const Angle angles(destination + cmd->viewangles);

		int hits = 0;
		const int hitsNeed = static_cast<int>(static_cast<float>(maxSeed) * (static_cast<float>(hitChance) / 100.f));

		const auto weapSpread = activeWeapon->getSpread();
		const auto weapInaccuracy = activeWeapon->getInaccuracy();
		const auto localEyePosition = localPlayer->getEyePosition();
		const auto range = activeWeapon->getWeaponData()->range;
		bool plz_hit_my_ass = false;

#pragma omp parallel for num_threads(maxThreadNum)
		for (int i = 1; i < maxSeed; ++i) //use openmp
		{
			memory->randomSeed(i + 1 + omp_get_thread_num());
			const float spreadX = memory->randomFloat(0.f, 2.f * static_cast<float>(M_PI));
			const float spreadY = memory->randomFloat(0.f, 2.f * static_cast<float>(M_PI));

			auto inaccuracy = weapInaccuracy * memory->randomFloat(0.f, 1.f);
			if (inaccuracy == 0)
				inaccuracy = 0.0000001f;

			auto spread = weapSpread * memory->randomFloat(0.f, 1.f);

			Vector spreadView{ (cosf(spreadX) * inaccuracy) + (cosf(spreadY) * spread), (sinf(spreadX) * inaccuracy) + (sinf(spreadY) * spread) };
			Vector direction{ (angles.forward + (angles.right * spreadView.x) + (angles.up * spreadView.y)) * range };

			for (int hitbox = 0; hitbox < Hitboxes::Max; hitbox++)
			{
				if (canHitHitbox(matrix, hitbox, set, localEyePosition, localEyePosition + direction))
				{
					// шобы стрелять в аирах со скаутом в ебучку как бог.
					if (localPlayer->getActiveWeapon()->itemDefinitionIndex2() == WeaponId::Ssg08 && !(localPlayer->flags() & FL_ONGROUND))
						if (inaccuracy < 0.009f)
							return false;

					hits++;
					break;
				}
			}

			if (hits >= hitsNeed)
				plz_hit_my_ass = true;

			if ((maxSeed - i + hits) < hitsNeed)
				return false;
		}

		return plz_hit_my_ass;
	}

	std::vector<Vector> multiPoint(Entity* entity, const matrix3x4 matrix[MAXSTUDIOBONES], StudioBbox* hitbox, Vector localEyePos, int _hitbox)
	{
		auto VectorTransformWrapper = [](const Vector& in1, const matrix3x4 in2, Vector& out)
		{
			auto VectorTransform = [](const float* in1, const matrix3x4 in2, float* out)
			{
				auto dotProducts = [](const float* v1, const float* v2)
				{
					return v1[0] * v2[0] + v1[1] * v2[1] + v1[2] * v2[2];
				};
				out[0] = dotProducts(in1, in2[0]) + in2[0][3];
				out[1] = dotProducts(in1, in2[1]) + in2[1][3];
				out[2] = dotProducts(in1, in2[2]) + in2[2][3];
			};
			VectorTransform(&in1.x, in2, &out.x);
		};

		Vector min, max, center;
		VectorTransformWrapper(hitbox->bbMin, matrix[hitbox->bone], min);
		VectorTransformWrapper(hitbox->bbMax, matrix[hitbox->bone], max);
		center = (min + max) * 0.5f;

		std::vector<Vector> vecArray;

		if (config->rageBot[RcurrentCategory].multiPointScale <= 0)
		{
			vecArray.emplace_back(center);
			return vecArray;
		}

		vecArray.emplace_back(center);
		Vector currentAngles = Aimbot::calculateRelativeAngle(center, localEyePos, Vector{});

		Vector forward;
		Vector::fromAngle(currentAngles, &forward);

		Vector right = forward.cross(Vector{ 0, 0, 1 });
		Vector left = Vector{ -right.x, -right.y, right.z };

		Vector top = Vector{ 0, 0, 1 };
		Vector bottom = Vector{ 0, 0, -1 };

		float multiPointScale = (min(config->rageBot[RcurrentCategory].multiPointScale, 95)) * 0.01f;

		switch (_hitbox)
		{
		case Hitboxes::Head:
			for (auto i = 0; i < 4; ++i)
				vecArray.emplace_back(center);

			vecArray[1] += top * (hitbox->capsuleRadius * multiPointScale);
			vecArray[2] += right * (hitbox->capsuleRadius * multiPointScale);
			vecArray[3] += left * (hitbox->capsuleRadius * multiPointScale);
			break;
		default: //reset
			for (auto i = 0; i < 3; ++i)
				vecArray.emplace_back(center);

			vecArray[1] += right * (hitbox->capsuleRadius * multiPointScale);
			vecArray[2] += left * (hitbox->capsuleRadius * multiPointScale);
			break;
		}

		return vecArray;
	}
}

namespace damage_utils
{
	static bool traceToExit(const Trace& enterTrace, const Vector& start, const Vector& direction, Vector& end, Trace& exitTrace, float range = 90.f, float step = 4.0f)
	{
		float distance{ 0.0f };
		int previousContents{ 0 };
		int first_contents = 0;

		while (distance <= range)
		{
			distance += step;
			Vector origin{ start + direction * distance };

			if (!previousContents)
				previousContents = interfaces->engineTrace->getPointContents(origin, 0x4600400B);

			const int currentContents = interfaces->engineTrace->getPointContents(origin, 0x4600400B);

			if (!(currentContents & 0x600400B) || (currentContents & 0x40000000 && currentContents != previousContents))
			{
				const Vector destination{ origin - (direction * step) };

				if (interfaces->engineTrace->traceRay({ origin, destination }, 0x4600400B, nullptr, exitTrace); exitTrace.startSolid && exitTrace.surface.flags & 0x8000)
				{
					if (interfaces->engineTrace->traceRay({ origin, start }, 0x600400B, { exitTrace.entity }, exitTrace); exitTrace.didHit() && !exitTrace.startSolid)
						return true;

					continue;
				}

				if (exitTrace.didHit() && !exitTrace.startSolid)
				{
					if (memory->isBreakableEntity(enterTrace.entity) && memory->isBreakableEntity(exitTrace.entity))
						return true;

					if (enterTrace.surface.flags & 0x0080 || (!(exitTrace.surface.flags & 0x0080) && exitTrace.plane.normal.dotProduct(direction) <= 1.0f))
						return true;

					continue;
				}
				else
				{
					if (enterTrace.entity && enterTrace.entity->index() != 0 && memory->isBreakableEntity(enterTrace.entity))
						return true;

					continue;
				}
			}
		}

		return false;
	}

	static float handleBulletPenetration(SurfaceData* enterSurfaceData, const Trace& enterTrace, const Vector& direction, Vector& result, float penetration, float damage) noexcept
	{
		Vector end;
		Trace exitTrace;

		if (!traceToExit(enterTrace, enterTrace.endpos, direction, end, exitTrace))
			return -1.0f;

		SurfaceData* exitSurfaceData = interfaces->physicsSurfaceProps->getSurfaceData(exitTrace.surface.surfaceProps);

		float damageModifier = 0.16f;
		float penetrationModifier = (enterSurfaceData->penetrationmodifier + exitSurfaceData->penetrationmodifier) / 2.0f;

		if (enterSurfaceData->material == 71 || enterSurfaceData->material == 89)
		{
			damageModifier = 0.05f;
			penetrationModifier = 3.0f;
		}
		else if (enterTrace.contents >> 3 & 1 || enterTrace.surface.flags >> 7 & 1)
			penetrationModifier = 1.0f;

		if (enterSurfaceData->material == exitSurfaceData->material)
		{
			if (exitSurfaceData->material == 85 || exitSurfaceData->material == 87)
				penetrationModifier = 3.0f;
			else if (exitSurfaceData->material == 76)
				penetrationModifier = 2.0f;
		}

		damage -= 11.25f / penetration / penetrationModifier + damage * damageModifier + (exitTrace.endpos - enterTrace.endpos).squareLength() / 24.0f / penetrationModifier;

		result = exitTrace.endpos;
		return damage;
	}

	void calculateArmorDamage(float armorRatio, int armorValue, bool hasHeavyArmor, float& damage) noexcept
	{
		auto armorScale = 1.0f;
		auto armorBonusRatio = 0.5f;

		if (hasHeavyArmor)
		{
			armorRatio *= 0.2f;
			armorBonusRatio = 0.33f;
			armorScale = 0.25f;
		}

		auto newDamage = damage * armorRatio;
		const auto estiminated_damage = (damage - damage * armorRatio) * armorScale * armorBonusRatio;

		if (estiminated_damage > armorValue)
			newDamage = damage - armorValue / armorBonusRatio;

		damage = newDamage;
	}

	bool getSafePoints(Entity* entity, const matrix3x4 matrix[MAXSTUDIOBONES], StudioHitboxSet* set, Vector start_position, Vector end_position, int hitbox) noexcept
	{
		if (aim_utils::canHitHitbox(matrix, hitbox, set, start_position, end_position))
			return true;

		return false;
	}

	float getScanDamage(Entity* entity, const Vector& destination, const WeaponInfo* weaponData, int minDamage) noexcept
	{
		if (!localPlayer)
			return 0.f;

		float damage{ static_cast<float>(weaponData->damage) };

		Vector start{ localPlayer->getEyePosition() };
		Vector direction{ destination - start };
		float maxDistance{ direction.length() };
		float curDistance{ 0.0f };
		direction /= maxDistance;

		int hitsLeft = 4;

		while (damage >= 1.0f && hitsLeft)
		{
			Trace trace;
			interfaces->engineTrace->traceRay({ start, destination }, 0x4600400B, localPlayer.get(), trace);

			if (trace.entity && trace.entity->isPlayer() && !localPlayer->isOtherEnemy(trace.entity))
				return 0.f;

			if (trace.fraction == 1.0f)
				break;

			curDistance += trace.fraction * (maxDistance - curDistance);
			damage *= std::pow(weaponData->rangeModifier, curDistance / 500.0f);

			if (trace.entity == entity && trace.hitgroup > HitGroup::Generic && trace.hitgroup <= HitGroup::RightLeg)
			{
				damage *= HitGroup::getDamageMultiplier(trace.hitgroup, weaponData, trace.entity->hasHeavyArmor(), static_cast<int>(trace.entity->getTeamNumber()));

				if (HitGroup::isArmored(trace.hitgroup, trace.entity->hasHelmet(), trace.entity->armor(), trace.entity->hasHeavyArmor()))
					calculateArmorDamage(weaponData->armorRatio / 2.0f, trace.entity->armor(), trace.entity->hasHeavyArmor(), damage);

				if (damage >= minDamage)
					return damage;

				return 0.f;
			}

			const auto surfaceData = interfaces->physicsSurfaceProps->getSurfaceData(trace.surface.surfaceProps);

			if (surfaceData->penetrationmodifier < 0.1f)
				break;

			damage = handleBulletPenetration(surfaceData, trace, direction, start, weaponData->penetration, damage);
			hitsLeft--;
		}
		return 0.f;
	}
}

void Aimbot::updateInput() noexcept
{
	config->ragebotKey.handleToggle();
	config->hitchanceOverride.handleToggle();
	config->minDamageOverrideKey.handleToggle();
	config->forceBaim.handleToggle();
}

void setupAimbot(UserCmd* cmd, Entity* entity, matrix3x4* matrix, Aimbot::Enemies target, std::array<bool, Hitboxes::Max> hitbox, Entity* activeWeapon, Vector localPlayerEyePosition, Vector aimPunch, int minDamage, float& damageDiff, Vector& bestAngle, Vector& bestTarget) noexcept
{
	// damageDiff = FLT_MAX;

	const Model* model = entity->getModel();
	if (!model)
		return;

	StudioHdr* hdr = interfaces->modelInfo->getStudioModel(model);
	if (!hdr)
		return;

	StudioHitboxSet* set = hdr->getHitboxSet(0);
	if (!set)
		return;

	for (size_t i = 0; i < hitbox.size(); i++)
	{
		if (!hitbox[i])
			continue;

		StudioBbox* hitbox = set->getHitbox(i);
		if (!hitbox)
			continue;

		for (auto& bonePosition : aim_utils::multiPoint(entity, matrix, hitbox, localPlayerEyePosition, i))
		{
			auto angleView{ Aimbot::calculateRelativeAngle(localPlayerEyePosition, bonePosition, cmd->viewangles + aimPunch) };
			auto fov{ angleView.length2D() };

			if (fov > config->ragebot.fov)
				continue;

			auto damage = damage_utils::getScanDamage(entity, bonePosition, activeWeapon->getWeaponData(), minDamage);
			damage = std::clamp(damage, 0.0f, (float)entity->maxHealth());

			if (damage <= 0.1f)
				continue;

			if (Tickbase::isPeeking = true)
			{
				if ((config->rageBot[RcurrentCategory].autostop_mode & 1 << 2) == 1 << 2 && localPlayer->flags() & FL_ONGROUND && !(cmd->buttons & UserCmd::IN_JUMP))
				{
					static auto isSpreadEnabled = interfaces->cvar->findVar(skCrypt("weapon_accuracy_nospread"));
					if (isSpreadEnabled->getInt() >= 1)
						continue;

					auto activeWeapon = localPlayer->getActiveWeapon();
					auto weaponData = activeWeapon->getWeaponData();

					const auto velocity = EnginePrediction::getVelocity();
					const auto speed = velocity.length2D();

					const float maxSpeed = (localPlayer->isScoped() ? weaponData->maxSpeedAlt : weaponData->maxSpeed) / 3;

					if (speed >= maxSpeed)
					{
						Vector direction = velocity.toAngle();
						direction.y = cmd->viewangles.y - direction.y;

						const auto negatedDirection = Vector::fromAngle(direction) * -speed;
						cmd->forwardmove = negatedDirection.x;
						cmd->sidemove = negatedDirection.y;
					}
				}

				if ((config->rageBot[RcurrentCategory].autostop_mode & 1 << 1) == 1 << 1 && memory->globalVars->serverTime() < localPlayer->getActiveWeapon()->nextPrimaryAttack())
					continue;
			}
			else
				Tickbase::isPeeking = false;

			if (std::fabsf((float)target.health - damage) <= damageDiff)
			{
				bestAngle = angleView;
				damageDiff = std::fabsf((float)target.health - damage);
				bestTarget = bonePosition;
			}
		}
	}

	if (bestTarget.notNull())
	{
		if (!aim_utils::Hitchance(localPlayer.get(), entity, set, matrix, activeWeapon, bestAngle, cmd, config->rageBot[RcurrentCategory].hitChance))
		{
			bestAngle = Vector{ };
			damageDiff = FLT_MAX;
			bestTarget = Vector{ };
		}
	}
}

void Aimbot::run(UserCmd* cmd) noexcept
{
	if (!config->ragebot.enabled)
		return;

	if (!config->ragebotKey.isActive())
		return;

	//if (!localPlayer || localPlayer->nextAttack() > memory->globalVars->serverTime() || localPlayer->isDefusing() || localPlayer->waitForNoAttack())
	//	return;

	const auto activeWeapon = localPlayer->getActiveWeapon();
	if (!activeWeapon || !activeWeapon->clip())
		return;

	if (activeWeapon->isKnife() || activeWeapon->isBomb() || activeWeapon->isGrenade())
		return;

	if (config->rageBot[1].enabled && (activeWeapon->itemDefinitionIndex2() == WeaponId::Elite || activeWeapon->itemDefinitionIndex2() == WeaponId::Hkp2000 || activeWeapon->itemDefinitionIndex2() == WeaponId::P250 || activeWeapon->itemDefinitionIndex2() == WeaponId::Usp_s || activeWeapon->itemDefinitionIndex2() == WeaponId::Cz75a || activeWeapon->itemDefinitionIndex2() == WeaponId::Tec9 || activeWeapon->itemDefinitionIndex2() == WeaponId::Fiveseven || activeWeapon->itemDefinitionIndex2() == WeaponId::Glock))
		RcurrentCategory = 1;
	else if (config->rageBot[2].enabled && activeWeapon->itemDefinitionIndex2() == WeaponId::Deagle)
		RcurrentCategory = 2;
	else if (config->rageBot[3].enabled && activeWeapon->itemDefinitionIndex2() == WeaponId::Revolver)
		RcurrentCategory = 3;
	else if (config->rageBot[4].enabled && (activeWeapon->itemDefinitionIndex2() == WeaponId::Mac10 || activeWeapon->itemDefinitionIndex2() == WeaponId::Mp9 || activeWeapon->itemDefinitionIndex2() == WeaponId::Mp7 || activeWeapon->itemDefinitionIndex2() == WeaponId::Mp5sd || activeWeapon->itemDefinitionIndex2() == WeaponId::Ump45 || activeWeapon->itemDefinitionIndex2() == WeaponId::P90 || activeWeapon->itemDefinitionIndex2() == WeaponId::Bizon))
		RcurrentCategory = 4;
	else if (config->rageBot[5].enabled && (activeWeapon->itemDefinitionIndex2() == WeaponId::Ak47 || activeWeapon->itemDefinitionIndex2() == WeaponId::M4A1 || activeWeapon->itemDefinitionIndex2() == WeaponId::M4a1_s || activeWeapon->itemDefinitionIndex2() == WeaponId::GalilAr || activeWeapon->itemDefinitionIndex2() == WeaponId::Aug || activeWeapon->itemDefinitionIndex2() == WeaponId::Sg553 || activeWeapon->itemDefinitionIndex2() == WeaponId::Famas))
		RcurrentCategory = 5;
	else if (config->rageBot[6].enabled && activeWeapon->itemDefinitionIndex2() == WeaponId::Ssg08)
		RcurrentCategory = 6;
	else if (config->rageBot[7].enabled && activeWeapon->itemDefinitionIndex2() == WeaponId::Awp)
		RcurrentCategory = 7;
	else if (config->rageBot[8].enabled && (activeWeapon->itemDefinitionIndex2() == WeaponId::G3SG1 || activeWeapon->itemDefinitionIndex2() == WeaponId::Scar20))
		RcurrentCategory = 8;
	else if (config->rageBot[9].enabled && activeWeapon->itemDefinitionIndex2() == WeaponId::Taser)
		RcurrentCategory = 9;
	else
		RcurrentCategory = 0;

	//if (localPlayer->shotsFired() > 0 && !activeWeapon->isFullAuto())
	//	return;

	if (!(config->ragebot.enabled && (cmd->buttons & UserCmd::IN_ATTACK || config->ragebot.autoShot)))
		return;

	float bestSimulationTime = 0;
	float damageDiff = FLT_MAX;

	int bestIndex{ -1 };

	Vector bestTarget{ };
	Vector bestAngle{ };

	const auto localPlayerEyePosition = localPlayer->getEyePosition();
	const auto aimPunch = (!activeWeapon->isKnife() && !activeWeapon->isBomb() && !activeWeapon->isGrenade()) ? localPlayer->getAimPunch() : Vector{};

	std::vector<Aimbot::Enemies> enemies;
	std::array<bool, Hitboxes::Max> hitbox{ false };

	if (config->rageBot[RcurrentCategory].preferBodyAim)
	{
		// Chest
		hitbox[Hitboxes::UpperChest] = (config->rageBot[RcurrentCategory].hitboxes & 1 << 1) == 1 << 1;
		hitbox[Hitboxes::Thorax] = (config->rageBot[RcurrentCategory].hitboxes & 1 << 1) == 1 << 1;
		hitbox[Hitboxes::LowerChest] = (config->rageBot[RcurrentCategory].hitboxes & 1 << 1) == 1 << 1;

		// Stomach
		hitbox[Hitboxes::Belly] = (config->rageBot[RcurrentCategory].hitboxes & 1 << 2) == 1 << 2;
	}
	else
	{
		// Head
		hitbox[Hitboxes::Head] = (config->rageBot[RcurrentCategory].hitboxes & 1 << 0) == 1 << 0;

		// Chest
		hitbox[Hitboxes::UpperChest] = (config->rageBot[RcurrentCategory].hitboxes & 1 << 1) == 1 << 1;
		hitbox[Hitboxes::Thorax] = (config->rageBot[RcurrentCategory].hitboxes & 1 << 1) == 1 << 1;
		hitbox[Hitboxes::LowerChest] = (config->rageBot[RcurrentCategory].hitboxes & 1 << 1) == 1 << 1;

		// Stomach
		hitbox[Hitboxes::Belly] = (config->rageBot[RcurrentCategory].hitboxes & 1 << 2) == 1 << 2;

		//Arms
		hitbox[Hitboxes::RightUpperArm] = (config->rageBot[RcurrentCategory].hitboxes & 1 << 3) == 1 << 3;
		hitbox[Hitboxes::RightForearm] = (config->rageBot[RcurrentCategory].hitboxes & 1 << 3) == 1 << 3;
		hitbox[Hitboxes::LeftUpperArm] = (config->rageBot[RcurrentCategory].hitboxes & 1 << 3) == 1 << 3;
		hitbox[Hitboxes::LeftForearm] = (config->rageBot[RcurrentCategory].hitboxes & 1 << 3) == 1 << 3;

		// Legs
		hitbox[Hitboxes::RightCalf] = (config->rageBot[RcurrentCategory].hitboxes & 1 << 4) == 1 << 4;
		hitbox[Hitboxes::RightThigh] = (config->rageBot[RcurrentCategory].hitboxes & 1 << 4) == 1 << 4;
		hitbox[Hitboxes::LeftCalf] = (config->rageBot[RcurrentCategory].hitboxes & 1 << 4) == 1 << 4;
		hitbox[Hitboxes::LeftThigh] = (config->rageBot[RcurrentCategory].hitboxes & 1 << 4) == 1 << 4;

		// feet
		hitbox[Hitboxes::RightFoot] = (config->rageBot[RcurrentCategory].hitboxes & 1 << 5) == 1 << 5;
		hitbox[Hitboxes::LeftFoot] = (config->rageBot[RcurrentCategory].hitboxes & 1 << 5) == 1 << 5;
	}

	const auto& localPlayerOrigin{ localPlayer->getAbsOrigin() };

	for (int i = 1; i <= interfaces->engine->getMaxClients(); ++i)
	{
		const auto player = Animations::getPlayer(i);
		if (!player.gotMatrix)
			continue;

		const auto entity{ interfaces->entityList->getEntity(i) };
		if (!entity || entity == localPlayer.get() || entity->isDormant() || !entity->isAlive() || !entity->isOtherEnemy(localPlayer.get()) || entity->gunGameImmunity())
			continue;

		Vector angle{ Aimbot::calculateRelativeAngle(localPlayerEyePosition, player.matrix[8].origin(), cmd->viewangles + aimPunch) };
		const auto& origin{ entity->getAbsOrigin() };

		const auto fov{ angle.length2D() }; // fov
		const auto health{ entity->health() }; // health
		const auto distance{ localPlayerOrigin.distTo(origin) }; // distance    

		enemies.emplace_back(i, health, distance, fov);
	}

	if (enemies.empty())
		return;

	std::sort(enemies.begin(), enemies.end(), distanceSort);

	for (const auto& target : enemies)
	{
		auto entity{ interfaces->entityList->getEntity(target.id) };
		auto player = Animations::getPlayer(target.id);

		int minDamage = std::clamp(config->minDamageOverrideKey.isActive() ? config->rageBot[RcurrentCategory].minDamageOverride : config->rageBot[RcurrentCategory].minDamage, 0, target.health + 1);

		matrix3x4* backupBoneCache = entity->getBoneCache().memory;
		Vector backupMins = entity->getCollideable()->obbMins();
		Vector backupMaxs = entity->getCollideable()->obbMaxs();
		Vector backupOrigin = entity->getAbsOrigin();
		Vector backupAbsAngle = entity->getAbsAngle();

		for (size_t i = 0; i < 2; i++)
		{
			float currentSimulationTime = -1.0f;

			if (config->backtrack.enabled)
			{
				const auto records = Animations::getBacktrackRecords(entity->index());
				if (!records || records->empty())
					continue;

				int bestTick = -1;

				if (i == 0)
				{
					for (size_t i = 0; i < records->size(); i++)
					{
						if (Backtrack::valid(records->at(i).simulationTime))
						{
							bestTick = static_cast<int>(i);
							break;
						}
					}
				}
				else
				{
					for (int i = static_cast<int>(records->size() - 1U); i >= 0; i--)
					{
						if (Backtrack::valid(records->at(i).simulationTime))
						{
							bestTick = i;
							break;
						}
					}
				}

				if (bestTick <= -1)
					continue;

				memcpy(entity->getBoneCache().memory, records->at(bestTick).matrix, std::clamp(entity->getBoneCache().size, 0, MAXSTUDIOBONES) * sizeof(matrix3x4));
				memory->setAbsOrigin(entity, records->at(bestTick).origin);
				memory->setAbsAngle(entity, Vector{ 0.f, records->at(bestTick).absAngle.y, 0.f });
				memory->setCollisionBounds(entity->getCollideable(), records->at(bestTick).mins, records->at(bestTick).maxs);

				currentSimulationTime = records->at(bestTick).simulationTime;
			}
			else
			{
				//We skip backtrack
				if (i == 1)
					continue;

				memcpy(entity->getBoneCache().memory, player.matrix.data(), std::clamp(entity->getBoneCache().size, 0, MAXSTUDIOBONES) * sizeof(matrix3x4));
				memory->setAbsOrigin(entity, player.origin);
				memory->setAbsAngle(entity, Vector{ 0.f, player.absAngle.y, 0.f });
				memory->setCollisionBounds(entity->getCollideable(), player.mins, player.maxs);

				currentSimulationTime = player.simulationTime;
			}

			setupAimbot(cmd, entity, entity->getBoneCache().memory, target, hitbox, activeWeapon, localPlayerEyePosition, aimPunch, minDamage, damageDiff, bestAngle, bestTarget);
			resetMatrix(entity, backupBoneCache, backupOrigin, backupAbsAngle, backupMins, backupMaxs);

			if (bestTarget.notNull())
			{
				bestSimulationTime = currentSimulationTime;
				bestIndex = target.id;
				break;
			}
		}

		if (bestTarget.notNull())
			break;
	}

	if (bestTarget.notNull())
	{
		static Vector lastAngles{ cmd->viewangles };
		static int lastCommand{ };

		if (lastCommand == cmd->commandNumber - 1 && lastAngles.notNull() && config->ragebot.silent)
			cmd->viewangles = lastAngles;

		Vector angle = Aimbot::calculateRelativeAngle(localPlayerEyePosition, bestTarget, cmd->viewangles + aimPunch);
		bool clamped{ false };

		if (std::abs(angle.x) > 255 || std::abs(angle.y) > 255)
		{
			angle.x = std::clamp(angle.x, -255.f, 255.f);
			angle.y = std::clamp(angle.y, -255.f, 255.f);
			clamped = true;
		}

		if (activeWeapon->nextPrimaryAttack() <= memory->globalVars->serverTime())
		{
			cmd->viewangles += angle;

			if (!config->ragebot.silent)
				interfaces->engine->setViewAngles(cmd->viewangles);

			if (config->ragebot.autoShot && !clamped)
				cmd->buttons |= UserCmd::IN_ATTACK;

			if (config->ragebot.autoScope && activeWeapon->isSniperRifle() && !localPlayer->isScoped() && !activeWeapon->zoomLevel())
				cmd->buttons |= UserCmd::IN_ZOOM;
		}

		if (clamped)
			cmd->buttons &= ~UserCmd::IN_ATTACK;

		if (cmd->buttons & UserCmd::IN_ATTACK)
		{
			cmd->tickCount = timeToTicks(bestSimulationTime + Backtrack::getLerp());
			Resolver::saveRecord(bestIndex, bestSimulationTime);
		}

		if (clamped)
			lastAngles = cmd->viewangles;
		else
			lastAngles = Vector{ };

		lastCommand = cmd->commandNumber;
	}
}