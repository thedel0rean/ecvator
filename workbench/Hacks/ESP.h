#pragma once
#include "../Config.h"
#include "../fnv.h"
#include "../GameData.h"
#include "../Helpers.h"
#include "../Memory.h"
#include "../SDK/Engine.h"
#include "../SDK/GlobalVars.h"
#include "../SDK/Surface.h"
#include "../Hooks.h"
#include "../GameData.h"
#include "../xor.h"
#include "../render.hpp"
#include "../Localize.h"
#include <iostream>

namespace warning
{
	void draw_circle(int x, int y, int points, int radius, Color color);
};

namespace ESP
{
	void render() noexcept;
	inline bool reset{ false };
}