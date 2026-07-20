/*
** b_move.cpp
**
** Movement/Roaming code for the bots
**
**---------------------------------------------------------------------------
**
** Copyright 1999 Martin Colberg
** Copyright 1999-2016 Marisa Heit
** Copyright 2005-2016 Christoph Oelckers
** Copyright 2017-2025 GZDoom Maintainers and Contributors
** Copyright 2025-2026 UZDoom Maintainers and Contributors
**
** SPDX-License-Identifier: GPL-3.0-or-later
**
**---------------------------------------------------------------------------
**
** Code written prior to 2026 is also licensed under:
**
** SPDX-License-Identifier: BSD-3-Clause
**
**---------------------------------------------------------------------------
**
*/

#include "actorinlines.h" // b_bot.h is defined in here via d_player.h.
#include "p_enemy.h"
#include "p_maputl.h"

static FRandom pr_bottrywalk("BotTryWalk");
static FRandom pr_botnewchasedir("BotNewChaseDir");

CVAR(Bool, sv_allowbotexit, false, CVAR_SERVERINFO | CVAR_NOSAVE)

bool P_CheckPosition(AActor* thing, const DVector2& pos, FCheckPosition& tm, bool actorsonly);

double DBot::GetJumpHeight() const
{
	// If playing in vanilla maps and jumping isn't explicitly set to enabled, avoid it
	// as it might break some maps.
	if (Level->maptype == MAPTYPE_DOOM && !(dmflags & DF_YES_JUMP))
		return 0.0;

    const double gravity = _player->mo->GetGravity();
    const double jumpZ = _player->mo->FloatVar(NAME_JumpZ);
    if (jumpZ < EQUAL_EPSILON || (_player->mo->flags & MF_NOGRAVITY) || (_player->mo->flags2 & MF2_FLY) || gravity < EQUAL_EPSILON
        || _player->mo->waterlevel >= 3 || !Level->IsJumpingAllowed())
    {
        return 0.0;
    }

    // Don't get the full height to account for bots not being exactly against the wall.
    return (jumpZ * jumpZ) / (2.0 * gravity) * 0.75;
}

// Make sure that bots can't pick up any items when checking their position since they're
// considered actual players and ZDoom throws every interaction inside of P_CheckPosition...
bool DBot::FakeCheckPosition(const DVector2& pos, FCheckPosition& tm, bool actorsOnly)
{
    const ActorFlags savedFlags = _player->mo->flags;
    _player->mo->flags &= ~MF_PICKUP;
    const bool res = P_CheckPosition(_player->mo, pos, tm, actorsOnly);
    _player->mo->flags = savedFlags;
    return res;
}

// Checks to see if the bot is capable of reaching a target actor taking
// level geometry into account. This is mostly for stepping up stairs and
// avoiding running directly into hazards, but won't take large dropoffs into account.
bool DBot::CanReach(AActor* mo, bool doJump)
{
    if (mo == nullptr)
        return false;

    if (_player->mo->flags & MF_NOCLIP)
        return true;

    if (mo->ceilingz - mo->floorz < _player->mo->Height)
        return false;

    // Intentionally ignore portals here.
    const double jumpHeight = doJump ? GetJumpHeight() : 0.0;
	const bool doHazardCheck = !_player->mo->Sector->IsDangerous(_player->mo->Pos(), _player->mo->Height, _player->mo->tid);
    constexpr int MaxBlocks = 3;

	const DVector3 dir = _player->mo->Vec3To(mo);
	DVector2 ofs = { 1.0, 0.0 };
	if (!dir.XY().isZero())
	{
		ofs = dir.XY();
		ofs.MakeUnit();
		ofs = { -ofs.Y, ofs.X };
	}
	ofs *= _player->mo->radius;

	for (int i = 0; i < 3; ++i)
	{
		DVector2 origin = _player->mo->Pos().XY();
		if (i == 1)
			origin += ofs;
		else if (i == 2)
			origin -= ofs;

		const DVector2 dest = origin + dir.XY();
		int blockCounter = 0;
		const sector_t* prevSec = Level->PointInSector(origin);
		double prevZ = _player->mo->floorz;
		const intercept_t* in = nullptr;
		FPathTraverse it = { Level, origin.X, origin.Y, dest.X, dest.Y, PT_ADDLINES | PT_ADDTHINGS };
		while ((in = it.Next()) != nullptr)
		{
			if (in->isaline)
			{
				const line_t* line = in->d.line;
				if (!(line->flags & ML_TWOSIDED) || (line->flags & (ML_BLOCKING | ML_BLOCKEVERYTHING | ML_BLOCK_PLAYERS)))
				{
					return false;
				}
				else
				{
					const DVector2 hitPos = it.InterceptPoint(in);
					const double hitZ = _player->mo->Z() + dir.Z * in->frac;

					//Determine if going to use backsector/frontsector.
					sector_t* sec = (line->backsector == prevSec) ? line->frontsector : line->backsector;
					if (doHazardCheck && sec->IsDangerous({ hitPos, hitZ }, _player->mo->Height, _player->mo->tid))
						return false;

					const double ceilZ = NextHighestCeilingAt(sec, hitPos.X, hitPos.Y, hitZ, hitZ + _player->mo->Height);
					const double floorZ = NextLowestFloorAt(sec, hitPos.Y, hitPos.Y, hitZ, 0, _player->mo->MaxStepHeight);

					if (floorZ <= prevZ + _player->mo->MaxStepHeight + jumpHeight
						&& ceilZ - floorZ >= _player->mo->Height)
					{
						prevZ = floorZ;
						prevSec = sec;
						continue;
					}

					// TODO: Check for doors here.

					return false;
				}
			}
			else
			{
				AActor* thing = in->d.thing;
				if (thing == _player->mo)
					continue;

				if (thing == mo)
					break;

				if (!(thing->flags & MF_SOLID))
					continue;

				// Ignore living things since they'll likely move out of the way.
				if (thing->player != nullptr || (thing->flags3 & MF3_ISMONSTER))
				{
					// Unless there's too many in the way.
					if (++blockCounter >= MaxBlocks)
						return false;

					continue;
				}

				return false;
			}
		}

		// This is done last in case there was a valid stairway leading up to the target.
		if (mo->Z() > prevZ + _player->mo->MaxStepHeight + jumpHeight)
			return false;
	}

	return true;
}

int P_CheckKeys(AActor* owner, int keynum, bool remote, bool quiet);

// Check to ensure the spot ahead of the bot is a valid place that can be walked. Tries
// to prevent walking over ledges and will automatically jump as well.
bool DBot::CheckMove(const DVector2& pos, bool* jumped, bool* interacted)
{
	if (jumped != nullptr)
		*jumped = false;
	if (interacted != nullptr)
		*interacted = false;

    // No jump check since the bot will just warp up ledges anyway.
    if (_player->mo->flags & MF_NOCLIP)
        return true;

    const double curZ = _player->mo->Z();
    const double jumpHeight = jumped != nullptr ? GetJumpHeight() : 0.0;
	const DVector2 usePos = _player->mo->Pos().XY() + _player->mo->Angles.Yaw.ToVector(_player->mo->FloatVar(NAME_UseRange));
    FCheckPosition tm = {};
    if (!FakeCheckPosition(pos, tm))
    {
		if (_player->mo->BlockingLine != nullptr)
		{
			if (interacted == nullptr)
				return false;

			auto line = _player->mo->BlockingLine;
			if (line->special == 0 || !(line->activation & (SPAC_Use | SPAC_UseThrough | SPAC_UseBack)))
				return false;

			const int side = P_PointOnLineSide(_player->mo->X(), _player->mo->Y(), line);
			const int useSide = P_PointOnLineSide(usePos.X, usePos.Y, line);
			// If the use action doesn't cross the line, they're probably not looking at it, so don't try
			// and open it.
			if (side == useSide)
				return false;

			if ((side == 1 && !(line->activation & SPAC_UseBack))
				|| (side != 1 && !(line->activation & (SPAC_Use | SPAC_UseThrough))))
			{
				return false;
			}

			if (line->locknumber)
			{
				if (!P_CheckKeys(_player->mo, line->locknumber, false, true))
					return false;
			}
			else
			{
				int lock = 0;
				if (line->special == 13 || line->special == 14)
					lock = line->args[3];
				else if (line->special == 83 || line->special == 85 || line->special == 202)
					lock = line->args[4];
				else if (line->special == 158)
					lock = line->args[2];

				if (lock && !P_CheckKeys(_player->mo, lock, false, true))
					return false;
			}

			*interacted = true;
			return false;
		}
		else
		{
			// Check for a thing that can be stepped up on.
			if (!(_player->mo->flags2 & MF2_PASSMOBJ) || (Level->i_compatflags & COMPATF_NO_PASSMOBJ))
				return false;

			// Don't walk on top of other players.
			AActor* blocking = _player->mo->BlockingMobj;
			if (blocking == nullptr || blocking->player != nullptr)
				return false;

			// Not enough room or too high to actually step up.
			const double top = blocking->Top();
			if (tm.ceilingz - top < _player->mo->Height
				|| (curZ + _player->mo->MaxStepHeight < top))
			{
				return false;
			}
		}
    }

    if (tm.ceilingz - tm.floorz < _player->mo->Height
        || tm.floorz > curZ + _player->mo->MaxStepHeight + jumpHeight
        || tm.ceilingz < _player->mo->Top()
        || (!(_player->mo->flags & (MF_DROPOFF | MF_FLOAT)) && tm.floorz - tm.dropoffz > _player->mo->MaxDropOffHeight)) 
    {
        return false;
    }

	// Only do a hazard check if we're not currently in a hazard zone.
	if (!_player->mo->Sector->IsDangerous(_player->mo->Pos(), _player->mo->Height, _player->mo->tid)
		&& tm.sector->IsDangerous(tm.pos, _player->mo->Height, _player->mo->tid))
	{
		return false;
	}

    // Check if it's jumpable.
    if (jumped != nullptr && tm.floorz > curZ + _player->mo->MaxStepHeight)
		*jumped = true;

    return true;
}

// Try and move the bot in its current movedir.
bool DBot::Move(bool running, bool doJump, bool doInteract, bool stuck)
{
	if (_player->mo->movedir >= DI_NODIR)
	{
		_player->mo->movedir = DI_NODIR;
		return false;
	}

	double moveCheck = _player->mo->radius - 1.0;
	if (moveCheck <= 0.0)
		moveCheck = 15.0;

    const DVector2 pos = { _player->mo->X() + moveCheck * xspeed[_player->mo->movedir],
                            _player->mo->Y() + moveCheck * yspeed[_player->mo->movedir] };

	bool jumped = false, interacted = false;
	const bool res = CheckMove(pos, doJump ? &jumped : nullptr, doInteract ? &interacted : nullptr);
	if (jumped)
		SetButtons(BT_JUMP, true);
	if (interacted)
		SetButtons(BT_USE, true);
	if (!res && !stuck)
		return false;

    constexpr double MinForward = 60.0;
    constexpr double MaxForward = 120.0;
    constexpr double MinSide = 30.0;
    constexpr double MaxSide = 150.0;

    const double delta = deltaangle(_player->mo->Angles.Yaw, DAngle45 * _player->mo->movedir).Degrees();
    const double absAng = fabs(delta);

    EBotMoveDirection forw = absAng <= MinForward || absAng >= MaxForward ? MDIR_FORWARDS : MDIR_NO_CHANGE;
    EBotMoveDirection side = absAng >= MinSide && absAng <= MaxSide ? MDIR_LEFT : MDIR_NO_CHANGE;
    if (side == MDIR_LEFT && delta < 0.0)
        side = MDIR_RIGHT;
    if (forw == MDIR_FORWARDS && absAng > 90.0)
        forw = MDIR_BACKWARDS;

    SetMove(forw, side, MDIR_NO_CHANGE, running);
    return true;
}

// Similar to Move() but will also set a cool down on the random turning if it could move.
// Only used when trying to pick a new direction to move.
bool DBot::TryWalk(bool running, bool doJump, bool doInteract, bool stuck)
{
    if (!Move(running, doJump, doInteract, stuck))
        return false;

    constexpr int CoolDown = TICRATE / 5;
    _player->mo->movecount = pr_bottrywalk() % CoolDown;
    return true;
}

void DBot::NewMoveDirection(AActor* goal, bool runAway, bool running, bool doJump, bool doInteract)
{
	int turnChance = 7;
	int facingDir = _player->mo->movedir;
	if (facingDir == DI_NODIR)
		facingDir = pr_botnewchasedir() & 7;

	int baseDir = facingDir;
    if (goal != nullptr)
    {
        constexpr double AngToDir = 1.0 / 45.0;

        double desired = _player->mo->AngleTo(goal).Degrees();
        while (desired < 0.0)
            desired += 360.0;

        baseDir = static_cast<int>(desired * AngToDir);
        if (runAway)
		{
			turnChance = 3;
            baseDir = (baseDir + 4) % 8;
			facingDir = (facingDir + 4) % 8;
		}
    }

    // Try and walk straight towards the goal, slowly shifting sides unless it needs to
    // turn around entirely.
    if (baseDir == facingDir && !(pr_botnewchasedir() & turnChance))
        baseDir = ((pr_botnewchasedir() & 1) * 2 - 1 + baseDir) % 8;

    _player->mo->movedir = baseDir;
    if (TryWalk(running, doJump, doInteract))
        return;

    _player->mo->movedir = (baseDir + 1) % 8;
    if (TryWalk(running, doJump, doInteract))
        return;

    _player->mo->movedir = (baseDir - 1) % 8;
    if (TryWalk(running, doJump, doInteract))
        return;

    _player->mo->movedir = (baseDir + 2) % 8;
    if (TryWalk(running, doJump, doInteract))
        return;

    _player->mo->movedir = (baseDir - 2) % 8;
    if (TryWalk(running, doJump, doInteract))
        return;

    _player->mo->movedir = (baseDir + 3) % 8;
    if (TryWalk(running, doJump, doInteract))
        return;

    _player->mo->movedir = (baseDir - 3) % 8;
    if (TryWalk(running, doJump, doInteract))
        return;

    _player->mo->movedir = (baseDir + 4) % 8;
    if (TryWalk(running, doJump, doInteract))
        return;

    // Couldn't move at all, so pick a random direction to see
	// if we can wiggle out.
    _player->mo->movedir = pr_botnewchasedir() & 7;
	TryWalk(running, doJump, doInteract, true);
}
