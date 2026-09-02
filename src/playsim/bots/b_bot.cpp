/*
** b_bot.cpp
**
** Cajun bot
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

#include "g_levellocals.h" // b_bot.h is defined in here via d_player.h.
#include "serializer_doom.h"
#include "serialize_obj.h"

extern int forwardmove[2], sidemove[2], flyspeed[2];

IMPLEMENT_CLASS(DBot, false, false)

// For Thinkers the default constructor isn't called at all, so these need to be initialized here.
void DBot::Construct(player_t* player, FName botID)
{
	_player = player;
	_botID = botID;
	Properties = { &DBotManager::BotDefinitions.CheckKey(botID)->GetProperties() };
}

// This has to be cleared manually since the destructor never gets called on Thinkers.
void DBot::OnDestroy()
{
	Properties.Clear();
	CoolDowns.Clear();
	Targets.Clear();

	Super::OnDestroy();
}

// The player pointer isn't serialized since bots will have their slots rearranged on load
// anyway to account for real players if needed. Instead their player pointer is reset then.
void DBot::Serialize(FSerializer& arc)
{
	Super::Serialize(arc);

	if (arc.isWriting())
	{
		FEntityProperties::PropertyDictionary& props = *const_cast<FEntityProperties::PropertyDictionary*>(&Properties.GetProperties());
		arc("botid", _botID)
			("aimpos", AimPos)
			("goalpos", GoalPos)
			("evadepos", EvadePos)
			("targetpos", TargetPos)
			("cooldowns", CoolDowns)
			("targets", Targets)
			("properties", props);
	}
	else
	{
		FEntityProperties::PropertyDictionary props = {};
		arc("botid", _botID)
			("aimpos", AimPos)
			("goalpos", GoalPos)
			("evadepos", EvadePos)
			("targetpos", TargetPos)
			("cooldowns", CoolDowns)
			("targets", Targets)
			("properties", props);

		const FBotDefinition* def = DBotManager::BotDefinitions.CheckKey(_botID);
		if (def == nullptr)
			Properties = { props }; // Keep its properties and key just in case it needs to be removed.
		else
			Properties = { props, &def->GetProperties() };
	}
}

// Called directly before its player's Think so commands can be properly set up
void DBot::CallBotThink()
{
	DBotManager::BotThinkCycles.Clock();

	IFVIRTUAL(DBot, BotThink)
		CallVM<void>(func, this);

	DBotManager::BotThinkCycles.Unclock();
}

short DBot::NormalizeSpeed(short cmd, int walkSpeed, int runSpeed, bool running, bool desiredRun)
{
	if (running && !desiredRun)
		cmd *= static_cast<double>(walkSpeed) / runSpeed;
	else if (!running && desiredRun)
		cmd *= static_cast<double>(runSpeed) / walkSpeed;
	return cmd;
}

void DBot::SetMove(EBotMoveDirection forward, EBotMoveDirection side, EBotMoveDirection up, bool running) const
{
	const bool curRunning = (_player->cmd.buttons & BT_RUN);
	if (forward != MDIR_NO_CHANGE)
		_player->cmd.forwardmove = forwardmove[running] * 256 * forward;
	else
		_player->cmd.forwardmove = NormalizeSpeed(_player->cmd.forwardmove, forwardmove[0], forwardmove[1], curRunning, running);

	if (side != MDIR_NO_CHANGE)
		_player->cmd.sidemove = sidemove[running] * 256 * side;
	else
		_player->cmd.sidemove = NormalizeSpeed(_player->cmd.sidemove, sidemove[0], sidemove[1], curRunning, running);

	if (up != MDIR_NO_CHANGE)
		_player->cmd.upmove = flyspeed[running] * up;
	else
		_player->cmd.upmove = NormalizeSpeed(_player->cmd.upmove, flyspeed[0], flyspeed[1], curRunning, running);

	SetButtons(BT_SPEED | BT_RUN, running);
}

void DBot::SetButtons(EButtonCodes cmds, bool set) const
{
	if (set)
		_player->cmd.buttons |= cmds;
	else
		_player->cmd.buttons &= ~cmds;
}

short DBot::GetAngleCommand(DAngle curAng, DAngle destAng)
{
	constexpr double AngToCmd = 65536.0 / 360.0;
	constexpr int MaxAngleCmd = 32768;

	const DAngle delta = deltaangle(curAng, destAng);
	int angleCmd = static_cast<int>(delta.Degrees() * AngToCmd);
	if (angleCmd >= MaxAngleCmd)
		angleCmd = MaxAngleCmd - 1;
	else if (angleCmd <= -MaxAngleCmd) // This usually has a special meaning, so avoid it.
		angleCmd = -MaxAngleCmd + 1;

	return angleCmd;
}

void DBot::SetAngle(DAngle dest, EBotAngleCmd type) const
{
	switch (type)
	{
		case ACMD_YAW:
			_player->cmd.yaw = GetAngleCommand(_player->mo->Angles.Yaw, dest);
			break;

		// This one is intentionally backwards due to how the pitch is applied.
		case ACMD_PITCH:
			_player->cmd.pitch = GetAngleCommand(dest, _player->mo->Angles.Pitch);
			break;

		case ACMD_ROLL:
			_player->cmd.roll = GetAngleCommand(_player->mo->Angles.Roll, dest);
			break;
	}
}
