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

#include "b_bot.h"
#include "info.h"

// FEntityProperties

static void GetString(FEntityProperties* self, int key, const FString* def, FString* res)
{
	*res = self->GetString(ENamedName(key), *def);
}

DEFINE_ACTION_FUNCTION_NATIVE(FEntityProperties, GetString, GetString)
{
	PARAM_SELF_STRUCT_PROLOGUE(FEntityProperties);
	PARAM_INT(key);
	PARAM_STRING(def);

	FString res = {};
	GetString(self, key, &def, &res);
	ACTION_RETURN_STRING(res);
}

static void GetDefaultString(FEntityProperties *self, int key, const FString *def, FString *res)
{
	*res = self->GetDefaultString(ENamedName(key), *def);
}

DEFINE_ACTION_FUNCTION_NATIVE(FEntityProperties, GetDefaultString, GetDefaultString)
{
	PARAM_SELF_STRUCT_PROLOGUE(FEntityProperties);
	PARAM_INT(key);
	PARAM_STRING(def);

	FString res = {};
	GetDefaultString(self, key, &def, &res);
	ACTION_RETURN_STRING(res);
}

static int GetInt(FEntityProperties* self, int key, int def)
{
	return self->GetInt(ENamedName(key), def);
}

DEFINE_ACTION_FUNCTION_NATIVE(FEntityProperties, GetInt, GetInt)
{
	PARAM_SELF_STRUCT_PROLOGUE(FEntityProperties);
	PARAM_INT(key);
	PARAM_INT(def);

	ACTION_RETURN_INT(GetInt(self, key, def));
}

static int GetDefaultInt(FEntityProperties *self, int key, int def)
{
	return self->GetDefaultInt(ENamedName(key), def);
}

DEFINE_ACTION_FUNCTION_NATIVE(FEntityProperties, GetDefaultInt, GetDefaultInt)
{
	PARAM_SELF_STRUCT_PROLOGUE(FEntityProperties);
	PARAM_INT(key);
	PARAM_INT(def);

	ACTION_RETURN_INT(GetDefaultInt(self, key, def));
}

static int GetBool(FEntityProperties* self, int key, bool def)
{
	return self->GetBool(ENamedName(key), def);
}

DEFINE_ACTION_FUNCTION_NATIVE(FEntityProperties, GetBool, GetBool)
{
	PARAM_SELF_STRUCT_PROLOGUE(FEntityProperties);
	PARAM_INT(key);
	PARAM_BOOL(def);

	ACTION_RETURN_BOOL(GetBool(self, key, def));
}

static int GetDefaultBool(FEntityProperties *self, int key, bool def)
{
	return self->GetDefaultBool(ENamedName(key), def);
}

DEFINE_ACTION_FUNCTION_NATIVE(FEntityProperties, GetDefaultBool, GetDefaultBool)
{
	PARAM_SELF_STRUCT_PROLOGUE(FEntityProperties);
	PARAM_INT(key);
	PARAM_BOOL(def);

	ACTION_RETURN_BOOL(GetDefaultBool(self, key, def));
}

static double GetDouble(FEntityProperties* self, int key, double def)
{
	return self->GetDouble(ENamedName(key), def);
}

DEFINE_ACTION_FUNCTION_NATIVE(FEntityProperties, GetDouble, GetDouble)
{
	PARAM_SELF_STRUCT_PROLOGUE(FEntityProperties);
	PARAM_INT(key);
	PARAM_FLOAT(def);

	ACTION_RETURN_FLOAT(GetDouble(self, key, def));
}

static double GetDefaultDouble(FEntityProperties *self, int key, double def)
{
	return self->GetDefaultDouble(ENamedName(key), def);
}

DEFINE_ACTION_FUNCTION_NATIVE(FEntityProperties, GetDefaultDouble, GetDefaultDouble)
{
	PARAM_SELF_STRUCT_PROLOGUE(FEntityProperties);
	PARAM_INT(key);
	PARAM_FLOAT(def);

	ACTION_RETURN_FLOAT(GetDefaultDouble(self, key, def));
}

static int GetWholeNumber(FEntityProperties* self, int key, int def)
{
	return self->GetWholeNumber(ENamedName(key), def);
}

DEFINE_ACTION_FUNCTION_NATIVE(FEntityProperties, GetWholeNumber, GetWholeNumber)
{
	PARAM_SELF_STRUCT_PROLOGUE(FEntityProperties);
	PARAM_INT(key);
	PARAM_INT(def);

	ACTION_RETURN_FLOAT(GetWholeNumber(self, key, def));
}

static int GetDefaultWholeNumber(FEntityProperties *self, int key, int def)
{
	return self->GetDefaultWholeNumber(ENamedName(key), def);
}

DEFINE_ACTION_FUNCTION_NATIVE(FEntityProperties, GetDefaultWholeNumber, GetDefaultWholeNumber)
{
	PARAM_SELF_STRUCT_PROLOGUE(FEntityProperties);
	PARAM_INT(key);
	PARAM_INT(def);

	ACTION_RETURN_FLOAT(GetDefaultWholeNumber(self, key, def));
}

static double GetRealNumber(FEntityProperties* self, int key, double def)
{
	return self->GetRealNumber(ENamedName(key), def);
}

DEFINE_ACTION_FUNCTION_NATIVE(FEntityProperties, GetRealNumber, GetRealNumber)
{
	PARAM_SELF_STRUCT_PROLOGUE(FEntityProperties);
	PARAM_INT(key);
	PARAM_FLOAT(def);

	ACTION_RETURN_FLOAT(GetRealNumber(self, key, def));
}

static double GetDefaultRealNumber(FEntityProperties *self, int key, double def)
{
	return self->GetDefaultRealNumber(ENamedName(key), def);
}

DEFINE_ACTION_FUNCTION_NATIVE(FEntityProperties, GetDefaultRealNumber, GetDefaultRealNumber)
{
	PARAM_SELF_STRUCT_PROLOGUE(FEntityProperties);
	PARAM_INT(key);
	PARAM_FLOAT(def);

	ACTION_RETURN_FLOAT(GetDefaultRealNumber(self, key, def));
}

static void SetString(FEntityProperties* self, int key, const FString* value)
{
	self->SetString(ENamedName(key), *value);
}

DEFINE_ACTION_FUNCTION_NATIVE(FEntityProperties, SetString, SetString)
{
	PARAM_SELF_STRUCT_PROLOGUE(FEntityProperties);
	PARAM_INT(key);
	PARAM_STRING(value);

	SetString(self, key, &value);
	return 0;
}

static void SetInt(FEntityProperties* self, int key, int value)
{
	self->SetInt(ENamedName(key), value);
}

DEFINE_ACTION_FUNCTION_NATIVE(FEntityProperties, SetInt, SetInt)
{
	PARAM_SELF_STRUCT_PROLOGUE(FEntityProperties);
	PARAM_INT(key);
	PARAM_INT(value);

	SetInt(self, key, value);
	return 0;
}

static void SetBool(FEntityProperties* self, int key, bool value)
{
	self->SetBool(ENamedName(key), value);
}

DEFINE_ACTION_FUNCTION_NATIVE(FEntityProperties, SetBool, SetBool)
{
	PARAM_SELF_STRUCT_PROLOGUE(FEntityProperties);
	PARAM_INT(key);
	PARAM_BOOL(value);

	SetBool(self, key, value);
	return 0;
}

static void SetDouble(FEntityProperties* self, int key, double value)
{
	self->SetDouble(ENamedName(key), value);
}

DEFINE_ACTION_FUNCTION_NATIVE(FEntityProperties, SetDouble, SetDouble)
{
	PARAM_SELF_STRUCT_PROLOGUE(FEntityProperties);
	PARAM_INT(key);
	PARAM_FLOAT(value);

	SetDouble(self, key, value);
	return 0;
}

static int HasProperty(FEntityProperties* self, int key)
{
	return self->HasProperty(ENamedName(key));
}

DEFINE_ACTION_FUNCTION_NATIVE(FEntityProperties, HasProperty, HasProperty)
{
	PARAM_SELF_STRUCT_PROLOGUE(FEntityProperties);
	PARAM_INT(key);

	ACTION_RETURN_BOOL(HasProperty(self, key));
}

static void RemoveProperty(FEntityProperties* self, int key)
{
	self->RemoveProperty(ENamedName(key));
}

DEFINE_ACTION_FUNCTION_NATIVE(FEntityProperties, RemoveProperty, RemoveProperty)
{
	PARAM_SELF_STRUCT_PROLOGUE(FEntityProperties);
	PARAM_INT(key);

	RemoveProperty(self, key);
	return 0;
}

static void ResetProperty(FEntityProperties* self, int key)
{
	self->ResetProperty(ENamedName(key));
}

DEFINE_ACTION_FUNCTION_NATIVE(FEntityProperties, ResetProperty, ResetProperty)
{
	PARAM_SELF_STRUCT_PROLOGUE(FEntityProperties);
	PARAM_INT(key);

	ResetProperty(self, key);
	return 0;
}

static void ResetAllProperties(FEntityProperties* self)
{
	self->ResetAllProperties();
}

DEFINE_ACTION_FUNCTION_NATIVE(FEntityProperties, ResetAllProperties, ResetAllProperties)
{
	PARAM_SELF_STRUCT_PROLOGUE(FEntityProperties);

	ResetAllProperties(self);
	return 0;
}

// DBot

DEFINE_FIELD(DBot, Properties)
DEFINE_FIELD(DBot, AimPos)
DEFINE_FIELD(DBot, MovePos)
DEFINE_FIELD_NAMED(DBot, CoolDowns, _coolDowns)
DEFINE_FIELD_NAMED(DBot, Targets, _targets)

static FEntityProperties* GetEntityInfo(int ent, int base)
{
	return EntityDefManager::GetEntityInfo(ENamedName(ent), ENamedName(base));
}

DEFINE_ACTION_FUNCTION_NATIVE(FLevelLocals, GetEntityInfo, GetEntityInfo)
{
	PARAM_PROLOGUE;
	PARAM_INT(ent);
	PARAM_INT(base);

	ACTION_RETURN_POINTER(GetEntityInfo(ent, base));
}

static int GetBotCount()
{
	return DBotManager::CountBots();
}

DEFINE_ACTION_FUNCTION_NATIVE(FLevelLocals, GetBotCount, GetBotCount)
{
	PARAM_PROLOGUE;

	ACTION_RETURN_INT(GetBotCount());
}

static player_t* GetPlayer(DBot* self)
{
	return self->GetPlayer();
}

DEFINE_ACTION_FUNCTION_NATIVE(DBot, GetPlayer, GetPlayer)
{
	PARAM_SELF_PROLOGUE(DBot);

	ACTION_RETURN_POINTER(GetPlayer(self));
}

static int GetBotID(DBot* self)
{
	return self->GetBotID().GetIndex();
}

DEFINE_ACTION_FUNCTION_NATIVE(DBot, GetBotID, GetBotID)
{
	PARAM_SELF_PROLOGUE(DBot);

	ACTION_RETURN_INT(GetBotID(self));
}

static void SetMove(DBot* self, int forw, int side, int up, bool running)
{
	self->SetMove(static_cast<EBotMoveDirection>(forw), static_cast<EBotMoveDirection>(side), static_cast<EBotMoveDirection>(up), running);
}

DEFINE_ACTION_FUNCTION_NATIVE(DBot, SetMove, SetMove)
{
	PARAM_SELF_PROLOGUE(DBot);
	PARAM_INT(forw);
	PARAM_INT(side);
	PARAM_INT(up);
	PARAM_BOOL(running);

	SetMove(self, forw, side, up, running);
	return 0;
}

static void SetButtons(DBot* self, unsigned buttons, bool set)
{
	self->SetButtons(EButtonCodes::FromInt(buttons), set);
}

DEFINE_ACTION_FUNCTION_NATIVE(DBot, SetButtons, SetButtons)
{
	PARAM_SELF_PROLOGUE(DBot);
	PARAM_UINT(buttons);
	PARAM_BOOL(set);

	SetButtons(self, buttons, set);
	return 0;
}

static void SetAngle(DBot* self, double destAng)
{
	self->SetAngle(DAngle::fromDeg(destAng), ACMD_YAW);
}

DEFINE_ACTION_FUNCTION_NATIVE(DBot, SetAngle, SetAngle)
{
	PARAM_SELF_PROLOGUE(DBot);
	PARAM_FLOAT(yaw);

	SetAngle(self, yaw);
	return 0;
}

static void SetPitch(DBot* self, double destAng)
{
	self->SetAngle(DAngle::fromDeg(destAng), ACMD_PITCH);
}

DEFINE_ACTION_FUNCTION_NATIVE(DBot, SetPitch, SetPitch)
{
	PARAM_SELF_PROLOGUE(DBot);
	PARAM_FLOAT(pitch);

	SetPitch(self, pitch);
	return 0;
}

static void SetRoll(DBot* self, double destAng)
{
	self->SetAngle(DAngle::fromDeg(destAng), ACMD_ROLL);
}

DEFINE_ACTION_FUNCTION_NATIVE(DBot, SetRoll, SetRoll)
{
	PARAM_SELF_PROLOGUE(DBot);
	PARAM_FLOAT(roll);

	SetRoll(self, roll);
	return 0;
}

static int IsActorInView(DBot* self, AActor* mo, double fov)
{
	return self->IsActorInView(mo, DAngle::fromDeg(fov));
}

DEFINE_ACTION_FUNCTION_NATIVE(DBot, IsActorInView, IsActorInView)
{
	PARAM_SELF_PROLOGUE(DBot);
	PARAM_POINTER(mo, AActor);
	PARAM_FLOAT(fov);

	ACTION_RETURN_BOOL(IsActorInView(self, mo, fov));
}

static int CheckShotPath(DBot* self, double x, double y, double z, int projType, double minDistance)
{
	return self->CheckShotPath({ x, y, z }, ENamedName(projType), minDistance);
}

DEFINE_ACTION_FUNCTION_NATIVE(DBot, CheckShotPath, CheckShotPath)
{
	PARAM_SELF_PROLOGUE(DBot);
	PARAM_FLOAT(x);
	PARAM_FLOAT(y);
	PARAM_FLOAT(z);
	PARAM_INT(projType);
	PARAM_FLOAT(minDist);

	ACTION_RETURN_BOOL(CheckShotPath(self, x, y, z, projType, minDist));
}

static AActor* FindTarget(DBot* self, double fov)
{
	return self->FindTarget(DAngle::fromDeg(fov));
}

DEFINE_ACTION_FUNCTION_NATIVE(DBot, FindTarget, FindTarget)
{
	PARAM_SELF_PROLOGUE(DBot);
	PARAM_FLOAT(fov);

	ACTION_RETURN_POINTER(FindTarget(self, fov));
}

static AActor* FindPartner(DBot* self)
{
	return self->FindPartner();
}

DEFINE_ACTION_FUNCTION_NATIVE(DBot, FindPartner, FindPartner)
{
	PARAM_SELF_PROLOGUE(DBot);

	ACTION_RETURN_POINTER(FindPartner(self));
}

static int IsValidItem(DBot* self, AActor* item)
{
	return self->IsValidItem(item);
}

DEFINE_ACTION_FUNCTION_NATIVE(DBot, IsValidItem, IsValidItem)
{
	PARAM_SELF_PROLOGUE(DBot);
	PARAM_POINTER(item, AActor);

	ACTION_RETURN_BOOL(IsValidItem(self, item));
}

static int TestPosition(DBot* self, double x, double y, FCheckPosition* tm, bool actorsOnly)
{
	int res = 0;
	if (tm == nullptr)
	{
		FCheckPosition temp = {};
		res = self->TestPosition({ x, y }, temp, actorsOnly);
	}
	else
	{
		res = self->TestPosition({ x, y }, *tm, actorsOnly);
	}

	return res;
}

DEFINE_ACTION_FUNCTION_NATIVE(DBot, TestPosition, TestPosition)
{
	PARAM_SELF_PROLOGUE(DBot);
	PARAM_FLOAT(x);
	PARAM_FLOAT(y);
	PARAM_POINTER(tm, FCheckPosition);
	PARAM_BOOL(actorsOnly);

	ACTION_RETURN_BOOL(TestPosition(self, x, y, tm, actorsOnly));
}

static double GetJumpHeight(DBot* self)
{
	return self->GetJumpHeight();
}

DEFINE_ACTION_FUNCTION_NATIVE(DBot, GetJumpHeight, GetJumpHeight)
{
	PARAM_SELF_PROLOGUE(DBot);

	ACTION_RETURN_FLOAT(GetJumpHeight(self));
}

static int CanReach(DBot* self, AActor* mo, unsigned cmds)
{
	return self->CanReach(mo, EBotCmds::FromInt(cmds));
}

DEFINE_ACTION_FUNCTION_NATIVE(DBot, CanReach, CanReach)
{
	PARAM_SELF_PROLOGUE(DBot);
	PARAM_POINTER(mo, AActor);
	PARAM_UINT(cmds);

	ACTION_RETURN_BOOL(CanReach(self, mo, cmds));
}

static int CheckMove(DBot* self, double x, double y, unsigned cmds, unsigned* desiredCmds)
{
	EBotCmds desired = 0;
	bool res = self->CheckMove({ x, y }, EBotCmds::FromInt(cmds), &desired);
	if (desiredCmds != nullptr)
		*desiredCmds = desired;
	return res;
}

DEFINE_ACTION_FUNCTION_NATIVE(DBot, CheckMove, CheckMove)
{
	PARAM_SELF_PROLOGUE(DBot);
	PARAM_FLOAT(x);
	PARAM_FLOAT(y);
	PARAM_UINT(cmds);

	unsigned desiredCmds = 0;
	bool res = CheckMove(self, x, y, cmds, &desiredCmds);
	if (numret > 0)
		ret[0].SetInt(res);
	if (numret > 1)
		ret[1].SetInt(desiredCmds);
	return numret;
}

static int Move(DBot* self, unsigned cmds)
{
	return self->Move(EBotCmds::FromInt(cmds));
}

DEFINE_ACTION_FUNCTION_NATIVE(DBot, Move, Move)
{
	PARAM_SELF_PROLOGUE(DBot);
	PARAM_UINT(cmds);

	ACTION_RETURN_BOOL(Move(self, cmds));
}

static void NewMoveDirection(DBot* self, double x, double y, bool runAway, unsigned cmds)
{
	self->NewMoveDirection({x, y}, runAway, EBotCmds::FromInt(cmds));
}

DEFINE_ACTION_FUNCTION_NATIVE(DBot, NewMoveDirection, NewMoveDirection)
{
	PARAM_SELF_PROLOGUE(DBot);
	PARAM_FLOAT(x);
	PARAM_FLOAT(y);
	PARAM_BOOL(runAway);
	PARAM_UINT(cmds);

	NewMoveDirection(self, x, y, runAway, cmds);
	return 0;
}
