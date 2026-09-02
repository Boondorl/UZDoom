/*
** b_bot.h
**
** Used with all b_*
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

#ifndef __B_BOT_H__
#define __B_BOT_H__

#include "r_defs.h"
#include "dthinker.h"
#include "stats.h"
#include "c_cvars.h"
#include "d_event.h"
#include "vm.h"
#include "m_random.h"
#include "p_checkposition.h"
#include "maps.h"
#include <variant>

EXTERN_CVAR(Int, bot_next_color) // Unused.

enum EBotMoveDirection
{
	MDIR_BACKWARDS = -1,
	MDIR_NONE,
	MDIR_FORWARDS,
	MDIR_NO_CHANGE,

	MDIR_RIGHT = MDIR_FORWARDS,
	MDIR_LEFT = MDIR_BACKWARDS,

	MDIR_UP = MDIR_FORWARDS,
	MDIR_DOWN = MDIR_BACKWARDS
};

enum EBotAngleCmd
{
	ACMD_YAW,
	ACMD_PITCH,
	ACMD_ROLL
};

enum EBotCmd : unsigned
{
	BCMD_JUMP	= 1,
	BCMD_RUN	= 1 << 1,
	BCMD_USE	= 1 << 2,
};

typedef TFlags<EBotCmd> EBotCmds;
DEFINE_TFLAGS_OPERATORS(EBotCmds)

// Allow for modders to set up any custom properties they want in BOTDEF. Includes wrapper functionality
// for getting simple data types (others like vectors can be implemented ZScript side). Name is more generic since
// this can technically be used for anything for any reason.
struct FEntityProperties
{
	using Property = std::variant<bool, int, double, FString>;
	using PropertyDictionary = TMap<FName, Property>;

private:
	PropertyDictionary _properties = {};
	const FEntityProperties* _default = nullptr; // Similar to a PClass's defaults.

public:
	FEntityProperties() = default;
	FEntityProperties(const PropertyDictionary& _properties) : _properties(_properties) {}
	FEntityProperties(const FEntityProperties* _default) : _default(_default) { ResetAllProperties(); }
	FEntityProperties(const PropertyDictionary& _properties, const FEntityProperties* _default) : _properties(_properties), _default(_default) {}

	// Needed for when the bot is destroyed.
	void Clear()
	{
		_properties.Clear();
		_default = nullptr;
	}

	void ResetProperty(FName key)
	{
		if (_default != nullptr && _default->HasProperty(key))
			SetValue(key, *_default->GetValue(key));
		else
			RemoveProperty(key);
	}

	void ResetAllProperties()
	{
		if (_default != nullptr)
			_properties = _default->_properties;
		else
			_properties.Clear();
	}

	// This is important so that properties can be properly reset to use defaults
	// specified by the client. Even having an empty string is not the same as the value not
	// existing.
	void RemoveProperty(FName key)
	{
		_properties.Remove(key);
	}

	const PropertyDictionary& GetProperties() const
	{
		return _properties;
	}

	bool HasProperty(FName key) const
	{
		return _properties.CheckKey(key) != nullptr;
	}

	void SetValue(FName key, const Property& value)
	{
		_properties[key] = value;
	}

	void SetString(FName key, const FString& value)
	{
		_properties[key] = value;
	}

	void SetBool(FName key, bool value)
	{
		_properties[key] = value;
	}

	void SetInt(FName key, int value)
	{
		_properties[key] = value;
	}

	void SetDouble(FName key, double value)
	{
		_properties[key] = value;
	}

	const Property* GetValue(FName key) const
	{
		return _properties.CheckKey(key);
	}

	const FString& GetString(FName key, const FString& def = {}) const
	{
		auto value = _properties.CheckKey(key);
		return value != nullptr && std::holds_alternative<FString>(*value) ? std::get<FString>(*value) : def;
	}

	bool GetBool(FName key, bool def = false) const
	{
		auto value = _properties.CheckKey(key);
		return value != nullptr && std::holds_alternative<bool>(*value) ? std::get<bool>(*value) : def;
	}

	int GetInt(FName key, int def = 0) const
	{
		auto value = _properties.CheckKey(key);
		return value != nullptr && std::holds_alternative<int>(*value) ? std::get<int>(*value) : def;
	}

	double GetDouble(FName key, double def = 0.0) const
	{
		auto value = _properties.CheckKey(key);
		return value != nullptr && std::holds_alternative<double>(*value) ? std::get<double>(*value) : def;
	}

	int GetWholeNumber(FName key, int def = 0) const
	{
		auto value = _properties.CheckKey(key);
		if (value == nullptr)
			return def;

		if (std::holds_alternative<int>(*value))
			return std::get<int>(*value);
		else if (std::holds_alternative<double>(*value))
			return static_cast<int>(std::get<double>(*value));

		return def;
	}

	double GetRealNumber(FName key, double def = 0.0) const
	{
		auto value = _properties.CheckKey(key);
		if (value == nullptr)
			return def;

		if (std::holds_alternative<int>(*value))
			return std::get<int>(*value);
		else if (std::holds_alternative<double>(*value))
			return std::get<double>(*value);

		return def;
	}

	const FString& GetDefaultString(FName key, const FString &def = {}) const
	{
		return _default != nullptr ? _default->GetString(key, def) : def;
	}

	bool GetDefaultBool(FName key, bool def = false) const
	{
		return _default != nullptr ? _default->GetBool(key, def) : def;
	}

	int GetDefaultInt(FName key, int def = 0) const
	{
		return _default != nullptr ? _default->GetInt(key, def) : def;
	}

	double GetDefaultDouble(FName key, double def = 0.0) const
	{
		return _default != nullptr ? _default->GetDouble(key, def) : def;
	}

	int GetDefaultWholeNumber(FName key, int def = 0) const
	{
		return _default != nullptr ? _default->GetWholeNumber(key, def) : def;
	}

	double GetDefaultRealNumber(FName key, double def = 0.0) const
	{
		return _default != nullptr ? _default->GetRealNumber(key, def) : def;
	}

	FString AsString(FName key, const FString& def = {}) const
	{
		auto value = _properties.CheckKey(key);
		if (value == nullptr)
			return def;

		switch (value->index())
		{
		case 0:
			return std::get<bool>(*value) ? "true" : "false";
		case 1:
			return FStringf("%d", std::get<int>(*value));
		case 2:
			return FStringf("%f", std::get<double>(*value));
		}

		return std::get<FString>(*value);
	}
};

class DBot : public DThinker
{
	DECLARE_CLASS(DBot, DThinker)

private:
	player_t* _player;	// Player info for the bot. This gets reset every time the game loads into a new map.
	FName _botID;		// Tracks which bot definition it's tied to.

	static short NormalizeSpeed(short cmd, int walkSpeed, int runSpeed, bool running, bool desiredRun);	// Ensure that speeds adhere to running properly.
	static short GetAngleCommand(DAngle curAng, DAngle destAng);						// Convert a delta angle into a valid turn command.
	bool TryWalk(EBotCmds cmds = BCMD_JUMP | BCMD_RUN | BCMD_USE, bool stuck = false) const;	// Same as Move but also sets a turn cool down when moving.

public:
	static const int DEFAULT_STAT = STAT_BOT;

	FEntityProperties Properties; // Stores current information about the bot. Uses the properties from its bot ID as defaults.
	DVector3 AimPos;
	DVector2 MovePos;
	ZSMap<FName, int> CoolDowns;
	ZSMap<FName, AActor*> Targets;

	void Construct(player_t* player, FName index);	// Set the default values of the class fields when the Thinker is created.
	void OnDestroy() override;						// Clear the Properties map.
	void Serialize(FSerializer& arc) override;		// Only serialize the bot id and properties.

	inline player_t* GetPlayer() const { return _player; }
	inline FName GetBotID() const { return _botID; }
	inline void ResetPlayer(player_t* player) { _player = player; } // This should only be called when deserializing.

	void CallBotThink(); // Handles overall thinking logic. Called directly before PlayerThink.

	bool IsActorInView(AActor* mo, DAngle fov = DAngle90) const;	// Check if the bot has sight of the Actor within a view cone.
	bool CanReach(AActor* mo, EBotCmds cmds = BCMD_JUMP | BCMD_USE) const; // Checks to see if a valid movement can be made towards the target.
	bool CheckShotPath(const DVector3& dest, FName projectileType = NAME_None, double minDistance = 0.0) const; // Checks if anything is blocking the ReadyWeapon missile's path.
	AActor* FindTarget(DAngle fov = DAngle90) const;					// Tries to find a target.
	AActor* FindPartner() const;											// Looks for a player to stick near, bot or real.
	bool IsValidItem(AActor* item) const;								// Checks to see if the item is able to be picked up.
	double GetJumpHeight() const;
	bool TestPosition(const DVector2& pos, FCheckPosition& tm, bool actorsOnly = false) const;	// Same as CheckPosition but prevent picking up items.
	bool CheckMove(const DVector2& pos, EBotCmds cmds, EBotCmds* desiredCmds) const;					// Check if a valid movement can be made to the given position. Also jumps if needed if that move is valid.
	bool Move(EBotCmds cmds = BCMD_JUMP | BCMD_RUN | BCMD_USE, bool stuck = false) const;				// Check to see if a movement is valid in the current moveDir.
	void NewMoveDirection(const DVector2& goalPos, bool runAway = false, EBotCmds cmds = BCMD_JUMP | BCMD_RUN | BCMD_USE) const; // Attempts to get a new direction to move towards the bot's goal.
	void SetMove(EBotMoveDirection forward = MDIR_NO_CHANGE, EBotMoveDirection side = MDIR_NO_CHANGE, EBotMoveDirection up = MDIR_NO_CHANGE, bool running = true) const; // Sets the move commands.
	void SetButtons(EButtonCodes cmd, bool set) const;			// Sets the button commands.
	void SetAngle(DAngle dest, EBotAngleCmd type) const;			// Sets the angle commands.
};

FString D_EscapeUserInfo(const char* str);

// Info about bots in the BOTDEF files.
struct FBotDefinition
{
private:
	FEntityProperties _properties = {};
	FString _userInfo = {};

public:
	// Just in case.
	void Clear()
	{
		_properties.Clear();
		_userInfo = FString{};
	}

	// This is needed so that the player's userinfo values get set correctly.
	TArrayView<uint8_t> GenerateUserInfo(const TMap<FName, FBaseCVar*>& info, DBot* bot)
	{
		// Reset it if it's being regenerated.
		_userInfo = FString{};

		if (bot == nullptr)
			return nullptr;

		FString value = {};
		IFVIRTUALPTR(bot, DBot, ModifySpawnProperty);
		VMValue params[] = { bot, {}, &value };
		VMReturn ret[] = { &value };

		TMap<FName, FBaseCVar*>::ConstPair* pair = nullptr;
		TMap<FName, FBaseCVar*>::ConstIterator it = { info };
		while (it.NextPair(pair))
		{
			value = _properties.AsString(pair->Key);
			if (func != nullptr)
			{
				params[1] = pair->Key.GetIndex();
				VMCall(func, params, 3, ret, 1);
			}

			if (value.IsNotEmpty())
			{
				value = D_EscapeUserInfo(value.GetChars());
				_userInfo.AppendFormat("\\%s\\%s", pair->Key.GetChars(), value.GetChars());
			}
		}

		return { const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(_userInfo.GetChars())), (unsigned)_userInfo.Len() + 1 };
	}

	const FEntityProperties& GetProperties() const
	{
		return _properties;
	}

	void SetValue(FName key, const FEntityProperties::Property& value)
	{
		_properties.SetValue(key, value);
	}

	void SetString(FName key, const FString& value)
	{
		_properties.SetString(key, value);
	}

	void SetBool(FName key, bool value)
	{
		_properties.SetBool(key, value);
	}

	void SetInt(FName key, int value)
	{
		_properties.SetInt(key, value);
	}

	void SetDouble(FName key, double value)
	{
		_properties.SetDouble(key, value);
	}

	const FEntityProperties::Property* GetValue(FName key) const
	{
		return _properties.GetValue(key);
	}

	const FString& GetString(FName key, const FString& def = {}) const
	{
		return _properties.GetString(key, def);
	}

	bool GetBool(FName key, bool def = false) const
	{
		return _properties.GetBool(key, def);
	}

	int GetInt(FName key, int def = 0) const
	{
		return _properties.GetInt(key, def);
	}

	double GetDouble(FName key, double def = 0.0) const
	{
		return _properties.GetDouble(key, def);
	}
};

// Used to keep all the globally needed variables and functions in order. A namespace isn't used
// here in order to prevent certain functionality from being accessed globally.
// Boon TODO: Now that shit is organized, this can probably be turned into a namespace proper
class DBotManager final
{
private:
	static inline TArray<FName> _botNameArgs = {};					// Bot names given when the host launched the game with the "-bots" arg.
	static inline TMap<FName, FName> _botReplacements = {};			// Replacement IDs when fetching a bot's info.

	static FBotDefinition& ParseBot(FScanner& sc, FBotDefinition& def);				// Function that parses a bot block in BOTDEFS.

public:
	DBotManager() = delete;

	static inline cycle_t BotThinkCycles = {};							// For tracking think time of bots specifically.
	static inline TMap<FName, FBotDefinition> BotDefinitions = {};		// Default properties and userinfo to give when spawning a bot. Stored by bot ID.

	static void ParseBotDefinitions();								// Parses the BOTDEF lumps.
	static void ParseZCajun();										// This is only for backwards compat, it shouldn't be used otherwise.
	static void SetNamedBots(const FString* args, int argCount);	// Parses the "-bots" arg for the names of the bots.
	static void SpawnNamedBots();				// Spawns any named bots. Only the host can do this. Triggers on level load.
	static int CountBots(FLevelLocals* level = nullptr);			// Counts the number of bots in the game.

	static FBotDefinition* GetBot(FName botName);
	static bool SpawnBot(FName name = NAME_None);					// Spawns a bot over the network. If no name is passed, spawns a random one.
	static bool TryAddBot(FLevelLocals* level, unsigned playerIndex, FName botID);		// Tries to add a bot to the game.
	static void RemoveBot(unsigned botNum);						// Removes the bot and makes it emulate a player leaving the game.
	static void RemoveAllBots();										// Removes all bots from the game.
};

class EntityDefManager final
{
private:
	static inline TMap<FName, FName> _entityReplacements = {};	// Replacement IDs when fetching an entity's info.

	static FEntityProperties& ParseEntity(FScanner& sc, FEntityProperties& props);	// Function that parses an entity block in ENTDEFS.

public:
	EntityDefManager() = delete;

	static inline TMap<FName, FEntityProperties> EntityInfo = {};	// Key information about how bots should react to entities. Stored by entity ID.

	static void ParseEntityDefinitions();								// Parses the ENTDEFS lumps.
	static FEntityProperties* GetEntityInfo(FName ent, FName baseClass = NAME_Actor);	// If the ID isn't found, try and use baseClass to find a parent entity.
};

#endif
