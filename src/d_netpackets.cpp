//-----------------------------------------------------------------------------
//
// Copyright 1993-1996 id Software
// Copyright 1999-2016 Randy Heit
// Copyright 2002-2016 Christoph Oelckers
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//

#include "d_net.h"
#include "s_music.h"
#include "screenjob.h"
#include "vm.h"
#include "actor.h"
#include "d_player.h"
#include "i_interface.h"
#include "gi.h"
#include "m_cheat.h"
#include "g_levellocals.h"
#include "gstrings.h"
#include "p_lnspec.h"

extern TMap<uint8_t, std::unique_ptr<NetPacket>(*)()> NetPacketFactory;

void InitializeDoomPackets()
{
	REGISTER_NETPACKET(SayPacket);
	REGISTER_NETPACKET(ChangeMusicPacket);
	REGISTER_NETPACKET(NetCommandPacket);
	REGISTER_NETPACKET(RevertCameraPacket);
}

EDemoCommand GetPacketType(const ReadStream& stream)
{
	return stream.EndOfStream() ? DEM_INVALID : stream.PeekValue<EDemoCommand>();
}

//==========================================================================
//
// Packet functionality
//
//==========================================================================

static bool ValidClass(const FString& cls, FName type = NAME_Actor)
{
	auto clss = PClass::FindActor(cls);
	return clss != nullptr && clss->IsDescendantOf(type);
}

NETPACKET_EXECUTE(WarpPacket)
{
	P_TeleportMove(players[player].mo, DVector3(_x, _y, _z), true);
	return true;
}

NETPACKET_CONDITION(GenericCheatPacket)
{
	return CheckCheatmode(player == consoleplayer);
}

NETPACKET_EXECUTE(GenericCheatPacket)
{
	cht_DoCheat(&players[player], Value);
	return true;
}

NETPACKET_CONDITION(ScriptCallPacket)
{
	return _scriptNum;
}

NETPACKET_EXECUTE(ScriptCallPacket)
{
	P_StartScript(players[player].mo->Level, players[player].mo, nullptr, abs(_scriptNum), players[player].mo->Level->MapName.GetChars(), Args, std::size(Args), ACS_NET | ((_scriptNum < 0) * ACS_ALWAYS));
}

// Lock this one down to the host to prevent anything weird from happening.
NETPACKET_CONDITION(EndScreenRunnerPacket)
{
	return player == Net_Arbitrator;
}

NETPACKET_EXECUTE(EndScreenRunnerPacket)
{
	EndScreenJob();
	return true;
}

NETPACKET_EXECUTE(PlayerReadyPacket)
{
	Net_PlayerReadiedUp(player);
	return true;
}

NETPACKET_EXECUTE(RevertCameraPacket)
{
	players[player].camera = players[player].mo;
	return true;
}

NETPACKET_EXECUTE(UseAllPacket)
{
	if (gamestate == GS_LEVEL && !WorldPaused(false)
		&& players[player].playerstate != PST_DEAD)
	{
		AActor* item = players[player].mo->Inventory;
		while (item != nullptr)
		{
			AActor* next = item->Inventory;
			IFVIRTUALPTRNAME(item, NAME_Inventory, UseAll)
				CallVM<void>(func, item, players[player].mo);
			item = next;
		}
	}
	return true;
}

NETPACKET_EXECUTE(ChangeMusicPacket)
{
	S_ChangeMusic(Value.GetChars());
	return true;
}

NETPACKET_CONDITION(CheatPacket)
{
	if (!ValidClass(ItemCls, NAME_Inventory))
	{
		Printf("No Inventory of type %s exists", ItemCls.GetChars());
		return false;
	}
	return true;
}

NETPACKET_EXECUTE(GiveCheatPacket)
{
	if (!ValidClass(ItemCls, NAME_Inventory))
	{
		Printf("%s [%d] spawned invalid item %s", players[player].userinfo.GetName(), player, ItemCls.GetChars());
		return true; // Non-fatal, just means someone is about to desync.
	}

	cht_Give(&players[player], ItemCls, Amount);
	if (player != consoleplayer)
	{
		FString message = GStrings.GetString("TXT_X_CHEATS");
		message.Substitute("%s", players[player].userinfo.GetName());
		Printf("%s: give %s\n", message.GetChars(), ItemCls.GetChars());
	}
	return true;
}

NETPACKET_EXECUTE(TakeCheatPacket)
{
	if (!ValidClass(ItemCls, NAME_Inventory))
	{
		Printf("%s [%d] took invalid item %s", players[player].userinfo.GetName(), player, ItemCls.GetChars());
		return true; // Non-fatal, just means someone is about to desync.
	}

	cht_Take(&players[player], ItemCls, Amount);
	return true;
}

NETPACKET_EXECUTE(SetCheatPacket)
{
	if (!ValidClass(ItemCls, NAME_Inventory))
	{
		Printf("%s [%d] set invalid item %s", players[player].userinfo.GetName(), player, ItemCls.GetChars());
		return true; // Non-fatal, just means someone is about to desync.
	}

	cht_SetInv(&players[player], ItemCls, Amount, _bPastMax);
	return true;
}

NETPACKET_CONDITION(SayPacket)
{
	return _message.IsNotEmpty();
}

NETPACKET_EXECUTE(SayPacket)
{
	if (cl_showchat == CHAT_DISABLED || (MutedClients & ((uint64_t)1u << player)))
		return true;

	const char* name = players[player].userinfo.GetName();
	if (!(_flags & MSG_TEAM))
	{
		if (cl_showchat < CHAT_GLOBAL)
			return true;

		// Said to everyone
		if (deathmatch && teamplay)
			Printf(PRINT_CHAT, "(All) ");
		if ((_flags & MSG_BOLD) && !cl_noboldchat)
			Printf(PRINT_CHAT, TEXTCOLOR_BOLD "* %s [%d]" TEXTCOLOR_BOLD "%s" TEXTCOLOR_BOLD "\n", name, player, _message.GetChars());
		else
			Printf(PRINT_CHAT, "%s [%d]" TEXTCOLOR_CHAT ": %s" TEXTCOLOR_CHAT "\n", name, player, _message.GetChars());

		if (!cl_nochatsound)
			S_Sound(CHAN_VOICE, CHANF_UI, gameinfo.chatSound, 1.0f, ATTN_NONE);
	}
	else if (!deathmatch || players[player].userinfo.GetTeam() == players[consoleplayer].userinfo.GetTeam())
	{
		if (cl_showchat < CHAT_TEAM_ONLY)
			return;

		// Said only to members of the player's team
		if (deathmatch && teamplay)
			Printf(PRINT_TEAMCHAT, "(Team) ");
		if ((_flags & MSG_BOLD) && !cl_noboldchat)
			Printf(PRINT_TEAMCHAT, TEXTCOLOR_BOLD "* %s [%d]" TEXTCOLOR_BOLD "%s" TEXTCOLOR_BOLD "\n", name, player, _message.GetChars());
		else
			Printf(PRINT_TEAMCHAT, "%s [%d]" TEXTCOLOR_TEAMCHAT ": %s" TEXTCOLOR_TEAMCHAT "\n", name, player, _message.GetChars());

		if (!cl_nochatsound)
			S_Sound(CHAN_VOICE, CHANF_UI, gameinfo.chatSound, 1.0f, ATTN_NONE);
	}
	return true;
}

NETPACKET_CONDITION(RunSpecialPacket)
{
	return _special > 0;
}

NETPACKET_EXECUTE(RunSpecialPacket)
{
	if (!CheckCheatmode(player == consoleplayer, false))
		P_ExecuteSpecial(players[player].mo->Level, _special, nullptr, players[player].mo, false, Args[0], Args[1], Args[2], Args[3], Args[4]);
	return true;
}

NETPACKET_EXECUTE(FOVPacket)
{
	if (Value != players[player].DesiredFOV)
		Printf("FOV%s set to %g\n", player == Net_Arbitrator ? " for everyone" : "", Value);

	player_t *p = nullptr;
	while ((p = player_t::GetNextPlayer(p)) != nullptr)
		p->DesiredFOV = Value;

	return true;
}

NETPACKET_EXECUTE(MyFOVPacket)
{
	players[player].DesiredFOV = Value;
	return true;
}

NETPACKET_EXECUTE(PausePacket)
{
	if (gamestate != GS_LEVEL)
		return true;

	if (paused)
	{
		paused = 0;
		S_ResumeSound(false);
	}
	else
	{
		paused = player + 1;
		S_PauseSound(false, false);
	}
	return true;
}

NETPACKET_CONDITION(SummonBasePacket)
{
	if (CheckCheatmode())
		return false;
	if (players[player].mo == nullptr)
		return false;
	const PClassActor *aType = PClass::FindActor(Cls);
	if (aType == nullptr)
	{
		const PClass *vType = PClass::FindClass(Cls);
		if (vType == nullptr || !vType->IsDescendantOf(NAME_VisualThinker))
		{
			Printf("No Actor or VisualThinker of type %s found\n", Cls.GetChars());
			return false;
		}
	}
	return true;
}

static void SummonClass(int player, const FString& cls, EFriendlyType friendly, bool specialSummon = false, DAngle angle = nullAngle, int tid = 0, int special = 0, int *args = nullptr)
{
	AActor *source = players[player].mo;
	PClassActor *aInfo = PClass::FindActor(cls);
	if (aInfo != nullptr)
	{
		if (GetDefaultByType(aInfo)->flags & MF_MISSILE)
		{
			P_SpawnPlayerMissile(source, 0, 0, 0, aInfo, source->Angles.Yaw);
		}
		else
		{
			DVector3 spawnPos = source->Vec3Angle(GetDefaultByType(aInfo)->radius * 2.0 + source->radius, source->Angles.Yaw, 8.0);
			AActor *spawned = Spawn(primaryLevel, aInfo, spawnPos, ALLOW_REPLACE);
			if (spawned != nullptr)
			{
				spawned->SpawnFlags |= MTF_CONSOLETHING;
				if (friendly == FT_FRIEND || friendly == FT_MBF)
				{
					spawned->ClearCounters();
					spawned->FriendPlayer = player + 1;
					spawned->flags |= MF_FRIENDLY;
					spawned->LastHeard = players[player].mo;
					spawned->health = spawned->SpawnHealth();
					if (friendly == FT_MBF)
						spawned->flags3 |= MF3_NOBLOCKMONST;
				}
				else if (friendly == FT_FOE)
				{
					spawned->FriendPlayer = 0;
					spawned->flags &= ~MF_FRIENDLY;
					spawned->health = spawned->SpawnHealth();
				}

				if (specialSummon)
				{
					spawned->Angles.Yaw = source->Angles.Yaw - angle;
					spawned->special = special;
					if (tid)
						spawned->SetTID(tid);
					if (args != nullptr)
					{
						for (size_t i = 0u; i < std::size(spawned->args); ++i)
							spawned->args[i] = args[i];
					}
				}
			}
		}
	}
	else
	{
		PClass *vType = PClass::FindClass(cls);
		if (vType != nullptr && vType->IsDescendantOf("VisualThinker"))
		{
			auto vt = DVisualThinker::NewVisualThinker(source->Level, vType, false);
			if (vt != nullptr)
			{
				vt->PT.Pos = source->Vec3Angle(source->radius * 4.0, source->Angles.Yaw, 8.0);
				vt->UpdateSector();
			}
		}
	}
}

NETPACKET_EXECUTE(SummonPacket)
{
	SummonClass(player, Cls, static_cast<EFriendlyType>(FriendlyType));
	return true;
}

NETPACKET_EXECUTE(Summon2Packet)
{
	SummonClass(player, Cls, static_cast<EFriendlyType>(FriendlyType), true, DAngle::fromBam(_angle), _tid, _special, _args);
	return true;
}
