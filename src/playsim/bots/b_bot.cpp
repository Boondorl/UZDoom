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

extern int forwardmove[2], sidemove[2], flyspeed[2];

IMPLEMENT_CLASS(DBot, false, false)

// For Thinkers the default constructor isn't called at all, so these need to be initialized here.
void DBot::Construct(player_t* const player, const FName& botID)
{
	_player = player;
	_botID = botID;
	Properties = { &DBotManager::BotDefinitions.CheckKey(botID)->GetProperties() };
}

// This has to be cleared manually since the destructor never gets called on Thinkers.
void DBot::OnDestroy()
{
	Super::OnDestroy();

	Properties.Clear();
}

// The player pointer isn't serialized since bots will have their slots rearranged on load
// anyway to account for real players if needed. Instead their player pointer is reset then.
void DBot::Serialize(FSerializer &arc)
{
	Super::Serialize(arc);

	if (arc.isWriting())
	{
		TMap<FName, FString> props = Properties.GetProperties();
		arc("botid", _botID)
			("properties", props);
	}
	else
	{
		TMap<FName, FString> props = {};
		arc("botid", _botID)
			("properties", props);

		const FBotDefinition* const def = DBotManager::BotDefinitions.CheckKey(_botID);
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
	{
		VMValue params[] = { this };
		VMCall(func, params, 1, nullptr, 0);
	}

	DBotManager::BotThinkCycles.Unclock();
}

void DBot::NormalizeSpeed(short& cmd, const int* const speeds, const bool running)
{
	const bool curRunning = _player->cmd.ucmd.buttons & (BT_SPEED | BT_RUN);
	if (curRunning && !running)
		cmd *= static_cast<double>(speeds[0]) / speeds[1];
	else if (!curRunning && running)
		cmd *= static_cast<double>(speeds[1]) / speeds[0];
}

void DBot::SetMove(const EBotMoveDirection forward, const EBotMoveDirection side, const EBotMoveDirection up, const bool running)
{
	if (forward != MDIR_NO_CHANGE)
		_player->cmd.ucmd.forwardmove = forwardmove[running] * 256 * forward;
	else
		NormalizeSpeed(_player->cmd.ucmd.forwardmove, forwardmove, running);

	if (side != MDIR_NO_CHANGE)
		_player->cmd.ucmd.sidemove = sidemove[running] * 256 * side;
	else
		NormalizeSpeed(_player->cmd.ucmd.sidemove, sidemove, running);

	if (up != MDIR_NO_CHANGE)
		_player->cmd.ucmd.upmove = flyspeed[running] * up;
	else
		NormalizeSpeed(_player->cmd.ucmd.upmove, flyspeed, running);

	SetButtons(BT_SPEED | BT_RUN, running);
}

void DBot::SetButtons(const int cmds, const bool set)
{
	if (set)
		_player->cmd.ucmd.buttons |= cmds;
	else
		_player->cmd.ucmd.buttons &= ~cmds;
};

void DBot::SetAngleCommand(short& cmd, const DAngle& curAng, const DAngle& destAng)
{
	constexpr double AngToCmd = 65536.0 / 360.0;
	constexpr int MaxAngleCmd = 32768;

	const DAngle delta = deltaangle(curAng, destAng);
	int angleCmd = static_cast<int>(delta.Degrees() * AngToCmd);
	if (angleCmd >= MaxAngleCmd)
		angleCmd = MaxAngleCmd - 1;
	else if (angleCmd <= -MaxAngleCmd) // This usually has a special meaning, so avoid it.
		angleCmd = -MaxAngleCmd + 1;


	cmd = angleCmd;
}

void DBot::SetAngle(const DAngle& dest, const EBotAngleCmd type)
{
	switch (type)
	{
		case ACMD_YAW:
			SetAngleCommand(_player->cmd.ucmd.yaw, _player->mo->Angles.Yaw, dest);
			break;

		case ACMD_PITCH:
			SetAngleCommand(_player->cmd.ucmd.pitch, _player->mo->Angles.Pitch, dest);
			break;

		case ACMD_ROLL:
			SetAngleCommand(_player->cmd.ucmd.roll, _player->mo->Angles.Roll, dest);
			break;
	}
}
