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

#include "b_bot.h"
#include "serializer_doom.h"
#include "gi.h"

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

// Player is serialized via player slot. Bots always refresh their player struct on load so saving it is not needed.
// If a player currently occupies its desired slot, its player pointer will be null and an attempt
// will be made to see if a new slot exists for it to be moved to.
void DBot::Serialize(FSerializer &arc)
{
	Super::Serialize(arc);

	if (arc.isWriting())
	{
		int32_t pNum = Level->PlayerNum(_player);
		TMap<FName, FString> props = Properties.GetProperties();
		arc("player", pNum)
			("botid", _botID)
			("properties", props);
	}
	else
	{
		int32_t pNum = 0;
		TMap<FName, FString> props = {};
		arc("player", pNum)
			("botid", _botID)
			("properties", props);

		const FBotDefinition* const def = DBotManager::BotDefinitions.CheckKey(_botID);
		if (def == nullptr)
			Properties = { props }; // Keep its properties and key just in case it needs to be removed.
		else
			Properties = { props, &def->GetProperties() };

		// Make sure the player slot is actually the bot. If not, the bot will
		// attempt to find a new slot if its player pointer is still null
		// after loading. If no slot open, it'll just boot itself.
		if (Level->Players[pNum]->Bot == this)
			_player = Level->Players[pNum];
	}
}

constexpr player_t* DBot::GetPlayer() const
{
	return _player;
}

constexpr const FName& DBot::GetBotID() const
{
	return _botID;
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

void DBot::SetMove(const EBotMoveDirection forward, const EBotMoveDirection side, const bool running)
{
	if (forward != MDIR_NO_CHANGE)
		_player->cmd.ucmd.forwardmove = static_cast<int>(gameinfo.normforwardmove[running] * 256.0 * forward);
	if (side != MDIR_NO_CHANGE)
		_player->cmd.ucmd.sidemove = static_cast<int>(gameinfo.normsidemove[running] * 256.0 * side);

	SetButtons(BT_SPEED | BT_RUN, running);
}

void DBot::SetButtons(const int cmds, const bool set)
{
	if (set)
		_player->cmd.ucmd.buttons |= cmds;
	else
		_player->cmd.ucmd.buttons &= ~cmds;
};
