#pragma once

#include "c_cvars.h"
#include "v_palette.h"

#include "r_utility.h"

struct Colormap;

EXTERN_CVAR(Int, r_extralight)

inline int hw_ClampLight(int lightlevel, bool addExtra = true)
{
	return clamp(lightlevel + r_extralight * addExtra, 0, 255);
}

EXTERN_CVAR(Int, gl_weaponlight);

inline	int getExtraLight()
{
	return r_viewpoint.extralight * gl_weaponlight;
}

