/*
**
**
**---------------------------------------------------------------------------
** Copyright 1999 Martin Colberg
** Copyright 1999-2016 Randy Heit
** Copyright 2005-2016 Christoph Oelckers
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/

#include "g_levellocals.h" // b_bot.h is defined in here via d_player.h.
#include "c_dispatch.h"
#include "teaminfo.h"
#include "d_net.h"
#include "events.h"

// This needs to be client-sided since it'll be called outside of the server loop.
static FCRandom pr_botspawn("BotSpawn");

extern void G_DoPlayerPop(int playernum);

// Sends out a network message telling clients to spawn a bot. Name
// is optional and, if not specified, a random one is chosen. This
// can include duplicates. This is really only meant to be called
// from the console command, but is network safe.
bool DBotManager::SpawnBot(FName name)
{
	if (!players[consoleplayer].settings_controller)
	{
		Printf("Only settings controllers are allowed to spawn bots\n");
		return false;
	}

	if (gamestate != GS_LEVEL)
	{
		Printf("Bots cannot be added when not in a game\n");
		return false;
	}

	if (!BotDefinitions.CountUsed())
	{
		Printf("There are no bots available to spawn\n");
		return false;
	}

	unsigned playerIndex = 0u;
	for (; playerIndex < MAXPLAYERS; ++playerIndex)
	{
		if (!playeringame[playerIndex])
			break;
	}

	if (playerIndex >= MAXPLAYERS)
	{
		Printf("The maximum number of players has already been reached, bot could not be added\n");
		return false;
	}

	FName key = name;
	if (key != NAME_None)
	{
		if (BotDefinitions.CheckKey(name) == nullptr)
		{
   		 	Printf("Couldn't find bot %s\n", name.GetChars());
			return false;
		}
	}
	else
	{
		// Spawn a random bot if no name given.
		int r = pr_botspawn() % BotDefinitions.CountUsed();
		TMap<FName, FBotDefinition>::ConstPair* pair = nullptr;
		TMap<FName, FBotDefinition>::ConstIterator it = { BotDefinitions };
		while (it.NextPair(pair) && r-- > 0) {}
		key = pair->Key;
	}

	// Send off the player slot + bot id.
	Net_WriteInt8(DEM_ADDBOT);
	Net_WriteInt8(playerIndex);
	Net_WriteString(key.GetChars());
	return true;
}

// Attempts to add a bot to the given level. This is really only meant to be called when
// receiving the network message to spawn a bot. SpawnBot should be used instead if network
// safety is needed.
bool DBotManager::TryAddBot(FLevelLocals* level, unsigned playerIndex, FName botID)
{
	if (gamestate != GS_LEVEL)
	{
		Printf("Bots cannot be added when not in a game\n");
		return false;
	}

	if (playeringame[playerIndex])
	{
		Printf("Couldn't add bot to slot %u, already occupied\n", playerIndex);
		return false;
	}

	auto bot = GetBot(botID);
	if (bot == nullptr)
	{
		Printf("Bot %s does not exist\n", botID.GetChars());
		return false;
	}

	static PClass* DefaultClass = PClass::FindClass(NAME_Bot);

	auto& cls = bot->GetString("BotClass", "Bot");
	auto botClass = PClass::FindClass(cls);
	if (botClass == nullptr || !botClass->IsDescendantOf(DefaultClass))
	{
		Printf(TEXTCOLOR_RED "Bot class %s is invalid, defaulting to Bot\n", cls.GetChars());
		botClass = DefaultClass;
	}

	multiplayer = true; // Count this as multiplayer, even if it's not a netgame.
	playeringame[playerIndex] = true;
	players[playerIndex].Bot = level->CreateThinker<DBot>(&players[playerIndex], botID);

	auto stream = bot->GenerateUserInfo(players[playerIndex].userinfo, players[playerIndex].Bot);
	if (stream.Size() > 0)
		D_ReadUserInfoStrings(playerIndex, stream, false);

	if (teamplay)
		Printf("%s joined the %s team\n", players[playerIndex].userinfo.GetName(), Teams[players[playerIndex].userinfo.GetTeam()].GetName());
	else
		Printf("%s joined the game\n", players[playerIndex].userinfo.GetName());

	// PlayerSpawned needs to be called before PlayerEntered
	players[playerIndex].playerstate = PST_ENTER;
	level->DoReborn(playerIndex);
	level->localEventManager->PlayerEntered(playerIndex, false);
	return true;
}

void DBotManager::RemoveBot(unsigned botNum)
{
	if (players[botNum].Bot == nullptr)
		return;

	// Make sure they stay on the same team next time they're added again.
	auto bot = BotDefinitions.CheckKey(players[botNum].Bot->GetBotID());
	int team = players[botNum].userinfo.GetTeam();
	if (!FTeam::IsValid(team))
		team = bot->GetInt("Team", TEAM_NONE);

	bot->SetInt("Team", team);
	players[botNum].playerstate = PST_GONE;
	G_DoPlayerPop(botNum);
}

void DBotManager::RemoveAllBots()
{
	for (unsigned i = 0u; i < MAXPLAYERS; ++i)
	{
		if (players[i].Bot != nullptr)
			RemoveBot(i);
	}
}

int DBotManager::CountBots(FLevelLocals* level)
{
	int bots = 0;
	if (level == nullptr)
	{
		for (auto lev : AllLevels())
		{
			for (unsigned i = 0u; i < MAXPLAYERS; ++i)
			{
				if (lev->PlayerInGame(i) && lev->Players[i]->Bot != nullptr)
					++bots;
			}
		}
	}
	else
	{
		for (unsigned i = 0u; i < MAXPLAYERS; ++i)
		{
			if (level->PlayerInGame(i) && level->Players[i]->Bot != nullptr)
				++bots;
		}
	}

	return bots;
}

FEntityProperties* DBotManager::GetEntityInfo(FName ent, FName baseClass)
{
	FName key = ent;
	auto replacement = _botEntityReplacements.CheckKey(key);
	if (replacement != nullptr)
		key = *replacement;

	FEntityProperties* entInfo = BotEntityInfo.CheckKey(key);
	if (entInfo != nullptr)
		return entInfo;

	// Maybe a class it inherits from has one?
	auto classType = PClass::FindClass(ent);
	if (classType == nullptr || !classType->IsDescendantOf(baseClass))
		return nullptr;

	auto parent = classType->ParentClass;
	while (parent != nullptr && parent->IsDescendantOf(baseClass))
	{
		entInfo = BotEntityInfo.CheckKey(parent->TypeName);
		if (entInfo != nullptr)
			return entInfo;

		parent = parent->ParentClass;
	}

	// If it's an actor, what about replacements?
	auto actor = PClassActor::FindActor(ent);
	if (actor == nullptr)
		return nullptr;

	// Like the actor it replaces.
	auto info = actor->ActorInfo();
	if (info->Replacee != nullptr)
	{
		entInfo = BotEntityInfo.CheckKey(info->Replacee->TypeName);
		if (entInfo != nullptr)
			return entInfo;
	}

	// Or the actor replacing it.
	if (info->Replacement != nullptr)
	{
		entInfo = BotEntityInfo.CheckKey(info->Replacement->TypeName);
		if (entInfo != nullptr)
			return entInfo;
	}

	return nullptr;
}

FBotDefinition* DBotManager::GetBot(FName botName)
{
	FName id = botName;
	auto replacement = _botReplacements.CheckKey(botName);
	if (replacement != nullptr)
		id = *replacement;

	return BotDefinitions.CheckKey(id);
}

void DBotManager::SetNamedBots(const FString* args, int argCount)
{
	_botNameArgs.Clear();
	if (consoleplayer != Net_Arbitrator)
		return;

	for (int i = 0; i < argCount; ++i)
		_botNameArgs.Push(args[i]);
}

void DBotManager::SpawnNamedBots()
{
	if (gamestate != GS_LEVEL || consoleplayer != Net_Arbitrator)
		return;

	for (auto& name : _botNameArgs)
		SpawnBot(name);

	_botNameArgs.Clear();
}

// BOTDEF parsing

static bool GetEntityDef(FName cls, FName base)
{
	auto clsDef = DBotManager::BotEntityInfo.CheckKey(cls);
	if (clsDef == nullptr)
		return false;

	auto baseDef = DBotManager::BotEntityInfo.CheckKey(base);
	if (base == nullptr)
		return false;

	FEntityProperties::PropertyDictionary::ConstIterator it = { baseDef->GetProperties() };
	FEntityProperties::PropertyDictionary::ConstPair* pair = nullptr;
	while (it.NextPair(pair))
	{
		if (!clsDef->HasProperty(pair->Key))
			clsDef->SetValue(pair->Key, pair->Value);
	}

	return true;
}

static bool GetBotDef(FName cls, FName base)
{
	auto clsDef = DBotManager::BotDefinitions.CheckKey(cls);
	if (clsDef == nullptr)
		return false;

	auto baseDef = DBotManager::BotDefinitions.CheckKey(base);
	if (base == nullptr)
		return false;

	auto& props = clsDef->GetProperties();
	FEntityProperties::PropertyDictionary::ConstIterator it = { baseDef->GetProperties().GetProperties() };
	FEntityProperties::PropertyDictionary::ConstPair* pair = nullptr;
	while (it.NextPair(pair))
	{
		if (!props.HasProperty(pair->Key))
			clsDef->SetValue(pair->Key, pair->Value);
	}

	return true;
}

class FInheritenceError
{
private:
	FString _error = {};

public:
	FInheritenceError() = default;
	FInheritenceError(const FString& error) : _error(error) {}

	const char* GetError() const
	{
		return _error.GetChars();
	}

	bool HasError() const
	{
		return _error.IsNotEmpty();
	}
};

struct FTreeNode
{
private:
	bool _bVisited = false;
	FName _parent = NAME_None;
	FName _cls = NAME_None;
	bool (*_nodeAction)(FName, FName) = nullptr;
	TArray<FName> _children = {};

public:
	FTreeNode(FName _cls, FName _parent, bool (*_nodeAction)(FName, FName)) : _cls(_cls), _parent(_parent), _nodeAction(_nodeAction) {}

	FName GetParent() const
	{
		return _parent;
	}

	bool CouldPotentiallyLoop() const
	{
		return _parent != NAME_None && _children.Size() && !WasVisited();
	}

	bool WasVisited() const
	{
		return _bVisited;
	}

	void SetVisited()
	{
		_bVisited = true;
	}

	bool IsInvalid() const
	{
		return _cls == NAME_None || _nodeAction == nullptr;
	}

	void Validate(FName cls, FName parent, bool (*action)(FName, FName))
	{
		if (!IsInvalid())
			return;

		if (cls != NAME_None && action != nullptr)
		{
			_cls = cls;
			_parent = parent;
			_nodeAction = action;
		}
	}

	void AddChildNode(FName child)
	{
		if (child != NAME_None && _children.Find(child) >= _children.Size())
			_children.Push(child);
	}

	FInheritenceError ParseChildren(TMap<FName, FTreeNode>& nodes)
	{
		if (_parent != NAME_None)
		{
			if (_nodeAction == nullptr)
				return { "Class node had no function definition for how to inherit." };

			bool res = _nodeAction(_cls, _parent);
			if (!res)
				return { FStringf("%s failed to inherit from %s.", _cls.GetChars(), _parent.GetChars()) };
		}

		for (auto& cls : _children)
		{
			auto node = nodes.CheckKey(cls);
			if (node == nullptr)
				continue;

			FInheritenceError error = node->ParseChildren(nodes);
			if (error.HasError())
				return error;
		}

		return {};
	}
};

struct FInheritenceTree
{
private:
	TArray<FName> _roots = {};
	TMap<FName, FTreeNode> _nodeList = {};

public:
	bool IsInvalidClass(FName cls) const
	{
		auto node = _nodeList.CheckKey(cls);
		return node != nullptr && !node->IsInvalid();
	}

	// Make sure no loops exist. These can't be childless or parentless
	// by definition, so those nodes can be skipped. Nodes that were already
	// visited are also safe.
	FInheritenceError ValidateNodes()
	{
		TMap<FName, FTreeNode>::Iterator it = { _nodeList };
		TMap<FName, FTreeNode>::Pair* pair = nullptr;
		while (it.NextPair(pair))
		{
			// Class that never got defined.
			if (pair->Value.IsInvalid())
				return { FStringf("Class %s was inherited from but never defined.", pair->Key.GetChars()) };

			if (pair->Value.CouldPotentiallyLoop())
			{
				TArray<FName> traversed = {};
				traversed.Push(pair->Key);

				FName parent = pair->Value.GetParent();
				while (parent != NAME_None)
				{
					// Recursion detected.
					if (traversed.Find(parent) < traversed.Size())
						return { FStringf("Class %s has no root class (inherits recursively).", pair->Key.GetChars()) };

					auto node = _nodeList.CheckKey(parent);
					// Must have been found in a valid path previously.
					if (node->WasVisited())
						break;

					traversed.Push(parent);
					node->SetVisited();
					parent = node->GetParent();
				}
			}

			pair->Value.SetVisited();
		}

		return {};
	}

	FInheritenceError GenerateData()
	{
		FInheritenceError error = ValidateNodes();
		if (error.HasError())
			return error;

		for (auto& cls : _roots)
		{
			auto node = _nodeList.CheckKey(cls);
			if (node == nullptr)
				continue;

			// Something went wrong in the class definition updating.
			FInheritenceError e = node->ParseChildren(_nodeList);
			if (e.HasError())
				return e;
		}

		return {};
	}

	void InsertNode(FName cls, FName parent, bool (*action)(FName, FName))
	{
		if (cls == NAME_None)
			return;

		const auto existing = _nodeList.CheckKey(cls);
		if (existing)
		{
			existing->Validate(cls, parent, action);
			if (parent != NAME_None)
				_roots.Delete(_roots.Find(parent));
		}
		else
		{
			_nodeList.Insert(cls, { (action != nullptr ? cls : NAME_None), parent, action });
		}

		if (parent == NAME_None)
		{
			_roots.Push(cls);
		}
		else
		{
			auto node = _nodeList.CheckKey(parent);
			if (node == nullptr)
			{
				// Make a temporary root for now. If the actual class is found,
				// it'll be reorganized.
				InsertNode(parent, NAME_None, nullptr);
				node = _nodeList.CheckKey(parent);
			}

			node->AddChildNode(cls);
		}
	}
};

// Boon TODO: Add backwards compat for zcajun bots
void DBotManager::ParseBotDefinitions()
{
	BotDefinitions.Clear();
	BotEntityInfo.Clear();

	constexpr int NoLump = -1;
	constexpr char LumpName[] = "BOTDEF";
	constexpr char BotDef[] = "Bot";
	constexpr char EntityDef[] = "Entity";
	constexpr char Replaces[] = "Replaces";
	constexpr char Abstract[] = "Abstract";

	// TODO: #include support

	TArray<FName> abstractEnts = {}, abstractBots = {};
	FInheritenceTree entTree = {}, botTree = {};

	int lump = NoLump;
	int lastLump = 0;
	while ((lump = fileSystem.FindLump(LumpName, &lastLump)) != NoLump)
	{
		FScanner sc = { lump };
		sc.SetCMode(true);
		while (sc.GetString())
		{
			const bool isAbstract = sc.Compare(Abstract);
			if (isAbstract)
				sc.MustGetString();

			const bool isBot = sc.Compare(BotDef);
			if (!isBot && !sc.Compare(EntityDef))
				sc.ScriptError("Expected '%s' or '%s', got '%s'.", BotDef, EntityDef, sc.String);

			sc.MustGetString();
			const FName key = sc.String;

			FName parent = NAME_None;
			sc.MustGetString();
			if (sc.Compare(":"))
			{
				sc.MustGetString();
				parent = sc.String;
				if (parent == key)
					sc.ScriptError("Definition '%s' tried to inherit from itself.", key.GetChars());

				sc.MustGetString();
			}

			FName replacement = NAME_None;
			if (sc.Compare(Replaces))
			{
				if (isAbstract)
					sc.ScriptError("Abstract definitions cannot replace other definitions.");

				sc.MustGetString();
				replacement = sc.String;
				sc.MustGetString();
			}

			if (!sc.Compare("{"))
				sc.ScriptError("Expected '{', got '%s'.", sc.String);

			if(isBot)
			{
				if (botTree.IsInvalidClass(key))
					sc.ScriptError("Bot '%s' already exists.", key.GetChars());

				FBotDefinition def = {};
				BotDefinitions.Insert(key, ParseBot(sc, def));
				botTree.InsertNode(key, parent, GetBotDef);
				if (replacement != NAME_None)
					_botReplacements.Insert(replacement, key);

				if (isAbstract)
					abstractBots.Push(key);
			}
			else
			{
				if (entTree.IsInvalidClass(key))
					sc.ScriptError("Weapon '%s' already exists.", key.GetChars());

				FEntityProperties props = {};
				BotEntityInfo.Insert(key, ParseEntity(sc, props));
				entTree.InsertNode(key, parent, GetEntityDef);
				if (replacement != NAME_None)
					_botEntityReplacements.Insert(replacement, key);

				if (isAbstract)
					abstractEnts.Push(key);
			}
		}
	}

	// Boon TODO: Error handling definitely needs to be reworked.
	// Now that we've done an initial pass, start inheriting.
	FInheritenceError error = entTree.GenerateData();
	if (error.HasError())
		I_Error("BOTDEF - %s\n", error.GetError());

	error = botTree.GenerateData();
	if (error.HasError())
		I_Error("BOTDEF - %s\n", error.GetError());

	for (auto& def : abstractBots)
	{
		BotDefinitions.Remove(def);
		_botReplacements.Remove(def); // Just in case someone decided to try replacing it.
	}
	for (auto& def : abstractEnts)
	{
		BotEntityInfo.Remove(def);
		_botEntityReplacements.Remove(def);
	}
}

FBotDefinition& DBotManager::ParseBot(FScanner& sc, FBotDefinition& def)
{
	while (sc.GetString())
	{
		if (sc.Compare("}"))
			break;

		const FName key = sc.String;
		if (sc.CheckBoolToken())
		{
			def.SetBool(key, sc.Number);
		}
		else if (sc.CheckFloat())
		{
			def.SetDouble(key, sc.Float);
		}
		else if (sc.CheckNumber())
		{
			def.SetInt(key, sc.Number);
		}
		else
		{
			sc.MustGetString();
			def.SetString(key, sc.String);
		}
	}

	if (def.GetString("PlayerClass").IsEmpty())
		def.SetString("PlayerClass", "Random");

	if (!FTeam::IsValid(def.GetInt("Team", UINT_MAX)))
		def.SetInt("Team", TEAM_NONE);

	return def;
}

FEntityProperties& DBotManager::ParseEntity(FScanner& sc, FEntityProperties& props)
{
	while (sc.GetString())
	{
		if (sc.Compare("}"))
			break;

		const FName key = sc.String;
		if (sc.CheckBoolToken())
		{
			props.SetBool(key, sc.Number);
		}
		else if (sc.CheckFloat())
		{
			props.SetDouble(key, sc.Float);
		}
		else if (sc.CheckNumber())
		{
			props.SetInt(key, sc.Number);
		}
		else
		{
			sc.MustGetString();
			props.SetString(key, sc.String);
		}
	}

	return props;
}

CVAR(Int, bot_next_color, 0, 0)

ADD_STAT(bots)
{
	FString out = {};
	out.Format("think = %04.2f ms", DBotManager::BotThinkCycles.TimeMS());
	return out;
}

CCMD(addbot)
{
	if (argv.argc() > 2)
	{
		Printf("addbot [botname] : add a bot to the game\n");
		return;
	}

	DBotManager::SpawnBot(argv.argc() > 1 ? FName(argv[1]) : NAME_None);
}

CCMD(removebots)
{
	if (!players[consoleplayer].settings_controller)
	{
		Printf("Only settings controllers can remove bots\n");
		return;
	}

	Net_WriteInt8(DEM_KILLBOTS);
}

CCMD(removebot)
{
	if (argv.argc() != 2)
	{
		Printf("removebot [botname] : remove a bot from the game\n");
		return;
	}

	if (!players[consoleplayer].settings_controller)
	{
		Printf("Only settings controllers can remove bots\n");
		return;
	}

	const FName id = argv[1];
	unsigned i = 0u;
	for (; i < MAXPLAYERS; ++i)
	{
		if (playeringame[i] && players[i].Bot != nullptr && players[i].Bot->GetBotID() == id)
			break;
	}

	if (i >= MAXPLAYERS)
	{
		Printf("Bot %s is not currently in the game\n", argv[1]);
		return;
	}

	Net_WriteInt8(DEM_KILLBOT);
	Net_WriteInt8(i);
}

CCMD(listbots)
{
	TMap<FName, FBotDefinition>::ConstIterator it = { DBotManager::BotDefinitions };
	TMap<FName, FBotDefinition>::ConstPair* pair = nullptr;
	while (it.NextPair(pair))
	{
		unsigned i = 0u;
		for (; i < MAXPLAYERS; ++i)
		{
			if (playeringame[i] && players[i].Bot != nullptr && players[i].Bot->GetBotID() == pair->Key)
				break;
		}

		Printf("%s%s\n", pair->Key.GetChars(), i < MAXPLAYERS ? " (active)" : "");
	}

	Printf("> %d bots\n> %d bots active\n", DBotManager::BotDefinitions.CountUsed(), DBotManager::CountBots());
}
