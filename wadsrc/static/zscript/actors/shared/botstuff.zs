/*
** botstuff.zs
**
**
**
**---------------------------------------------------------------------------
**
** Copyright 1993-1996 id Software
** Copyright 1999-2016 Marisa Heit
** Copyright 2006-2016 Christoph Oelckers
** Copyright 2017-2025 GZDoom Maintainers and Contributors
** Copyright 2025-2026 UZDoom Maintainers and Contributors
**
** SPDX-License-Identifier: GPL-3.0-or-later
**
**---------------------------------------------------------------------------
**
*/

struct EntityProperties native
{
	// Boon TODO: Return a list of properties? Is that even useful?
	//       	  Default getters?
	native void SetString(Name key, string value);
	native void SetBool(Name key, bool value);
	native void SetInt(Name key, int value);
	native void SetDouble(Name key, double value);

	native clearscope string GetString(Name key, string def = "") const;
	native clearscope bool GetBool(Name key, bool def = false) const;
	native clearscope int GetInt(Name key, int def = 0) const;
	native clearscope double GetDouble(Name key, double def = 0.0) const;
	native clearscope int GetWholeNumber(Name key, int def = 0) const;
	native clearscope double GetRealNumber(Name key, double def = 0.0) const;

	native void RemoveProperty(Name key);
	native void ResetProperty(Name key);
	native void ResetAllProperties();
	native clearscope bool HasProperty(Name key) const;
}

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
}

class Bot : Thinker native
{
	const DEF_REACTION_TICS = int(0.25 * TICRATE);
	const DEF_SIGHT_FOV = 60.0;
	const DEF_ITEM_RANGE = 480.0;
	const DARKNESS_THRESHOLD = 50;
	const MIN_RESPAWN_TIME = int(0.5 * TICRATE);
	const MAX_RESPAWN_TIME = int(1.5 * TICRATE);
	const SIGHT_COOL_DOWN_TICS = int(2.0 * TICRATE);
	const TARGET_COOL_DOWN_TICS = int(3.0 * TICRATE);
	const EVADE_COOL_DOWN_TICS = int(1.0 * TICRATE);
	const GOAL_COOL_DOWN_TICS = int(4.0 * TICRATE);
	const BURST_DELAY_TICS = int(0.15 * TICRATE);
	const PARTNER_WALK_RANGE_SQ = 640.0 * 640.0;
	const PARTNER_BACK_OFF_RANGE_SQ = 128.0 * 128.0;
	const MAX_FIRE_TRACKING_RANGE = 256.0;
	const MAX_ANGLE = 180.0;
	const MAX_TURN_SPEED = 4.0;
	const MAX_TURN_SPEED_BONUS = MAX_TURN_SPEED * 3.0;
	const BASE_IMPRECISION = 7.5;

	native @EntityProperties Properties;
	native Actor Evade;

	protected native Vector3 AimPos;

	private native Map<Name, int> _coolDowns;

	native clearscope PlayerInfo GetPlayer() const;
	native void SetMove(EBotMoveDirection forward = MDIR_NO_CHANGE, EBotMoveDirection side = MDIR_NO_CHANGE, EBotMoveDirection up = MDIR_NO_CHANGE, bool running = true);
	native void SetButtons(EButtons cmd, bool set);
	native void SetAngle(double destAngle);
	native void SetPitch(double destPitch);
	native void SetRoll(double destRoll);

	native bool IsActorInView(Actor mo, double fov = DEF_SIGHT_FOV);
	native bool CheckShotPath(Vector3 dest, Name projectileType = 'None', double minDistance = 0.0);
	native Actor FindTarget(double fov = DEF_SIGHT_FOV);
	native uint FindPartner();
	native bool IsValidItem(Inventory item);

	native clearscope double GetJumpHeight() const;
	native bool FakeCheckPosition(Vector2 dest, out FCheckPosition tm = null, bool actorsOnly = false);
	native bool CanReach(Actor mo, bool jump = true);
	native bool, bool, bool CheckMove(Vector2 dest, bool jump = true, bool allowInteract = true);
	native bool Move(bool running = true, bool jump = true, bool allowInteract = true);
	native void NewMoveDirection(Actor goal = null, bool runAway = false, bool running = true, bool jump = true, bool allowInteract = true);

	clearscope PlayerPawn GetPawn() const
	{
		return GetPlayer().Mo;
	}

	clearscope Actor GetTarget() const
	{
		return GetPawn().Target;
	}

	clearscope PlayerPawn GetPartner() const
	{
		let pawn = GetPawn();
		let partner = pawn.FriendPlayer > 0u && pawn.FriendPlayer <= MAXPLAYERS ? Players[pawn.FriendPlayer - 1u].Mo : null;
		if (partner == pawn)
			partner = null;

		return partner;
	}

	clearscope Actor GetGoal() const
	{
		return GetPawn().Goal;
	}

	clearscope bool IsTargetValid(Actor target) const
	{
		let pawn = GetPawn();
		return target && target != pawn && target.Health > 0 && target.bShootable && pawn.IsHostile(target);
	}

	// Buddha is intentionally ignored here.
	clearscope bool IsTargetDamageable(Actor target) const
	{
		return target && !target.bNonshootable && !target.bInvulnerable && !target.bNoDamage
				&& (!target.Player || !(target.Player.Cheats & (CF_GODMODE | CF_GODMODE2)));
	}

	clearscope bool IsPartnerValid(Actor partner) const
	{
		let pawn = GetPawn();
		return partner && partner != pawn && partner.Health > 0 && pawn.IsFriend(partner);
	}

	clearscope EntityProperties GetWeaponInfo(Weapon weap) const
	{
		return weap ? Level.GetEntityInfo(weap.GetClassName(), 'Weapon') : null;
	}

	clearscope double GetRange(EntityProperties props, Name prop, double propDef = 0.0, Name mod = 'None', double modDef = 1.0) const
	{
		double value = props && prop != 'None' ? props.GetRealNumber(prop, propDef) : propDef;
		value *= mod != 'None' ? Properties.GetRealNumber(mod, modDef) : modDef;
		return value;
	}

	clearscope double GetRangeSquared(EntityProperties props, Name prop, double propDef = 0.0, Name mod = 'None', double modDef = 1.0) const
	{
		double range = GetRange(props, prop, propDef, mod, modDef);
		return range * range;
	}

	void SetCoolDown(Name key, int time)
	{
		_coolDowns.Insert(key, Level.Time + time);
	}

	clearscope bool IsOnCoolDown(Name key) const
	{
		let [val, exists] = _coolDowns.CheckValue(key);
		return exists ? val > Level.Time : false;
	}

	void ResetCoolDowns()
	{
		_coolDowns.Clear();
	}

	void SetTarget(Actor target)
	{
		GetPawn().Target = target;
	}

	void SetPartner(uint pNum)
	{
		let pawn = GetPawn();
		if (!pNum || pNum > MAXPLAYERS || !PlayerInGame[pNum - 1u])
			pawn.FriendPlayer = pawn.PlayerNumber() + 1;
		else
			pawn.FriendPlayer = pNum;
	}

	void SetGoal(Actor goal)
	{
		GetPawn().Goal = goal;
	}

	virtual void BotThink()
	{
		if (GetPlayer().PlayerState == PST_DEAD)
		{
			BotDeathThink();
			return;
		}

		SearchForPartner();
		SearchForTarget();
		CheckEvade();
		UpdateGoal();
		UpdateInventory();
		
		AdjustAngles();
		HandleMovement();
		TryFire();
	}

	virtual void BotDeathThink()
	{
		// Bots will automatically respawn in deathmatch but for coop
		// play they still need to hit the use button.
		if (GetPlayer().Respawn_Time <= Level.Time)
			SetButtons(BT_USE, true);
	}

	virtual void SearchForPartner()
	{
		let partner = GetPartner();
		if (partner)
		{
			if (IsPartnerValid(partner))
				return;

			if (partner == GetGoal())
				SetGoal(null);
			if (partner == Evade)
				Evade = null;

			SetPartner(0u);
		}

		if (!deathmatch || teamplay)
			SetPartner(FindPartner());
	}

	virtual void SearchForTarget()
	{
		if (!IsOnCoolDown('LastSeen'))
			SetCoolDown('Fire', Properties.GetWholeNumber('ReactionTime', DEF_REACTION_TICS));

		let pawn = GetPawn();
		Actor target = GetTarget();
		double viewFOV = Properties.GetRealNumber('ViewFOV', DEF_SIGHT_FOV);
		if (target
			&& (!IsTargetValid(target) || (!IsOnCoolDown('LastSeen') && !IsActorInView(target, viewFOV))))
		{
			if (target == GetGoal())
				SetGoal(null);
			if (target == Evade)
				Evade = null;

			SetTarget(null);
			SetCoolDown('Target', 0);
		}

		target = GetTarget();
		if (target && IsActorInView(target, viewFOV))
			SetCoolDown('LastSeen', SIGHT_COOL_DOWN_TICS);

		let player = GetPlayer();
		Actor attacker = IsTargetValid(player.Attacker) ? player.Attacker : null;
		if (!target || (!IsOnCoolDown('Target') && (attacker || (deathmatch && target.Player))) || !IsTargetDamageable(target))
		{
			if (attacker)
				target = attacker;
			else
				target = FindTarget(viewFOV);

			if (target)
			{
				SetTarget(target);
				SetCoolDown('Target', TARGET_COOL_DOWN_TICS);
				SetCoolDown('LastSeen', SIGHT_COOL_DOWN_TICS);
				SetCoolDown('Fire', 0);
			}
		}

		player.Attacker = null;
	}

	virtual void CheckEvade()
	{
		let player = GetPlayer();
		let pawn = GetPawn();
		let partner = GetPartner();
		Actor target = GetTarget();

		double evasiveness = Properties.GetRealNumber('Evasiveness', 1.0);
		if (Evade)
		{
			if (Evade == target)
			{
				double minRange = GetRangeSquared(GetWeaponInfo(player.ReadyWeapon), 'MinCombatRange', mod: 'Timidness');
				
				let weapInfo = target.Player ? GetWeaponInfo(target.Player.ReadyWeapon) : null;
				double runRange = GetRangeSquared(weapInfo, 'FrightenRange', modDef: evasiveness);

				double dist = pawn.Distance3DSquared(target);
				if ((minRange <= 0.0 || dist > minRange) && (runRange <= 0.0 || dist > runRange))
					Evade = null;
			}
			else if (Evade == partner)
			{
				if (pawn.Distance3DSquared(partner) > PARTNER_BACK_OFF_RANGE_SQ)
					Evade = null;
			}
			else
			{
				double runRange = GetRangeSquared(Level.GetEntityInfo(Evade.GetClassName()), 'FrightenRange', modDef: evasiveness);
				if (runRange <= 0.0 || pawn.Distance3DSquared(Evade) > runRange)
					Evade = null;
			}
		}

		if (!Evade)
			SetCoolDown('Evade', 0);

		if (evasiveness > 0.0 && !IsOnCoolDown('Evade'))
		{
			Actor mo;
			Actor closest;
			double closestDist = double.infinity;
			double viewFOV = Properties.GetRealNumber('ViewFOV', DEF_SIGHT_FOV);
			let it = ThinkerIterator.Create("Actor", STAT_DEFAULT);
			while (mo = Actor(it.Next()))
			{
				if ((!mo.bIsMonster && !mo.bMissile)
					|| ((mo.bIsMonster && pawn.IsFriend(mo)) || (mo.bMissile && mo.Target && (mo.Target == pawn || pawn.IsFriend(mo.Target)))))
				{
					continue;
				}

				double runRange = GetRangeSquared(Level.GetEntityInfo(mo.GetClassName()), 'FrightenRange', modDef: evasiveness);
				if (runRange <= 0.0)
					continue;

				double dist = pawn.Distance3DSquared(mo);
				if (dist < closestDist && dist <= runRange && IsActorInView(mo, viewFOV))
				{
					closestDist = dist;
					closest = mo;
				}
			}

			if (closest)
			{
				Evade = closest;
				SetCoolDown('Evade', EVADE_COOL_DOWN_TICS);
				NewMoveDirection(Evade, true);
			}
		}

		if (Evade)
			return;

		if (target)
		{
			double minRange = GetRangeSquared(GetWeaponInfo(player.ReadyWeapon), 'MinCombatRange', mod: 'Timidness');
				
			let weapInfo = target.Player ? GetWeaponInfo(target.Player.ReadyWeapon) : null;
			double runRange = GetRangeSquared(weapInfo, 'FrightenRange', modDef: evasiveness);

			double dist = pawn.Distance3DSquared(target);
			if ((minRange > 0.0 && dist <= minRange) || (runRange > 0.0 && dist <= runRange))
			{
				Evade = target;
				SetCoolDown('Evade', 0);
				NewMoveDirection(Evade, true);
				return;
			}
		}

		if (partner && pawn.Distance3DSquared(partner) <= PARTNER_BACK_OFF_RANGE_SQ)
		{
			Evade = partner;
			SetCoolDown('Evade', 0);
			NewMoveDirection(Evade, true, false);
		}
	}

	virtual void UpdateGoal()
	{
		let curItem = Inventory(GetGoal());
		if (curItem && !IsValidItem(curItem))
		{
			curItem = null;
			SetGoal(null);
		}

		let player = GetPlayer();
		let pawn = GetPawn();
		bool isLowHealth = player.Health <= int(0.25 * pawn.GetMaxHealth(true));
		if (curItem && curItem.bIsHealth && isLowHealth)
			return;

		Actor target = GetTarget();
		if (target && player.ReadyWeapon)
		{
			let weapInfo = GetWeaponInfo(player.ReadyWeapon);
			double chaseRange = GetRangeSquared(weapInfo, 'ChaseRange', mod: 'Aggressiveness');
			double combatRange = GetRangeSquared(weapInfo, 'MaxCombatRange', mod: 'Confidence');

			double dist = pawn.Distance3DSquared(target);
			if ((combatRange > 0.0 && dist > combatRange)
				|| (chaseRange > 0.0 && dist <= chaseRange && CanReach(target)))
			{
				SetGoal(target);
				SetCoolDown('Goal', 0);
				return;
			}
		}

		if (target && target == GetGoal())
			SetGoal(null);

		if (!GetGoal())
			SetCoolDown('Goal', 0);

		// Bots won't steal items as much in coop.
		double itemRange = GetRange(Properties, 'ScavengeRange', DEF_ITEM_RANGE, 'Timidness');
		if (itemRange > 0.0 && !IsOnCoolDown('Goal') && (deathmatch || !Random[BotItem]()))
		{
			Inventory closest;
			double closestDist = double.infinity;
			double viewFOV = Properties.GetRealNumber('ViewFOV', DEF_SIGHT_FOV);
			double rangeSq = itemRange * itemRange;
			let it = BlockThingsIterator.Create(pawn, itemRange);
			while (it.Next())
			{
				Inventory item = Inventory(it.thing);
				if (!item)
					continue;

				double dist = pawn.Distance3DSquared(item);
				if (item.bBigPowerup || (item.bIsHealth && isLowHealth))
					dist *= 0.5625; // 0.75 * 0.75

				if (dist <= rangeSq && dist < closestDist && IsValidItem(item)
					&& IsActorInView(item, viewFOV) && CanReach(item))
				{
					closestDist = dist;
					closest = item;
				}
			}

			if (closest)
			{
				SetGoal(closest);
				SetCoolDown('Goal', GOAL_COOL_DOWN_TICS);
				return;
			}
		}
		
		let partner = GetPartner();
		Actor goal = GetGoal();
		if (partner && (!goal || partner == goal))
		{
			if (pawn.Distance3DSquared(partner) > PARTNER_WALK_RANGE_SQ
				&& pawn.CheckSight(partner, SF_IGNOREVISIBILITY|SF_SEEPASTSHOOTABLELINES|SF_IGNOREWATERBOUNDARY)
				&& CanReach(partner))
			{
				SetGoal(partner);
			}
			else
			{
				SetGoal(null);
			}

			SetCoolDown('Goal', 0);
		}
	}

	clearscope double GetWeaponWeighting(Weapon weap, double targetDist = -1.0, bool hasStrength = false) const
	{
		if (!weap)
			return 0.0;

		let def = GetWeaponInfo(weap);
		if (targetDist >= 0.0)
		{
			if (def)
			{
				double chase = GetRange(def, 'ChaseRange', mod: 'Aggressiveness');
				if (chase > 0.0 && chase < targetDist)
					return 0.0;
			}
			else if (weap.bMeleeWeapon && DEFMELEERANGE * 2 < targetDist)
			{
				return 0.0;
			}
		}
		
		double weight;
		let ammo1 = weap.Ammo1;
		let ammo2 = weap.Ammo2;
		if (!ammo1 && !ammo2)
			weight = 0.5;
		else if (sv_infiniteammo || GetPawn().FindInventory("PowerInfiniteAmmo", true))
			weight = 1.0;
		else if (ammo1 && !ammo2)
			weight = double(ammo1.Amount) / ammo1.MaxAmount;
		else if (!ammo1 && ammo2)
			weight = double(ammo2.Amount) / ammo2.MaxAmount;
		else
			weight = (double(ammo1.Amount) / ammo1.MaxAmount) * 0.5 + (double(ammo2.Amount) / ammo2.MaxAmount) * 0.5;
		
		weight *= 0.01;
		if (def)
		{
			weight *= def.GetWholeNumber('Priority', 10);
			if (hasStrength)
				weight *= def.GetRealNumber('StrengthBonus', 1.0);
			if (targetDist >= 0.0)
			{
				double maxRange = GetRange(def, 'MaxCombatRange', mod: 'Confidence');
				if (maxRange > 0.0 && targetDist > maxRange)
					weight *= maxRange / targetDist;
				double minRange = GetRange(def, 'MinCombatRange', mod: 'Timidness');
				if (minRange > 0.0 && targetDist < minRange)
					weight *= targetDist / minRange;
			}
		}
		else
		{
			bool ignoreWimpy = weap.bMeleeWeapon && weap.bWimpy_Weapon && hasStrength;
			weight *= 20 + (30 * (!weap.bWimpy_Weapon || ignoreWimpy));
		}

		// Prefer not to select melee weapons.
		if (weap.bMeleeWeapon)
			weight *= 0.7;
		
		return weight;
	}

	virtual void UpdateInventory()
	{
		let player = GetPlayer();
		let pawn = GetPawn();
		let target = GetTarget();

		if (player.PendingWeapon != WP_NOCHANGE || !(player.WeaponState & WF_WEAPONSWITCHOK) || player.IsTotallyFrozen())
			return;

		bool strong = pawn.FindInventory("PowerStrength") != null;
		double dist = target ? pawn.Distance3D(target) : -1.0;
		Weapon weap = player.ReadyWeapon;

		double switchChance;
		bool mustChange;
		if (!weap || !weap.CheckAmmo(Weapon.EitherFire, false))
		{
			mustChange = true;
		}
		else
		{
			double switchWeight = GetWeaponWeighting(weap, dist, strong);
			if (switchWeight <= 0.0)
				switchChance = 0.01;
			else
				switchChance = (1.0 / switchWeight) * 0.0001;
			// Don't switch as much if just idling.
			if (!target && !weap.bMeleeWeapon)
				switchChance *= 0.1;
		}

		if (!mustChange && FRandom[BotSwitch](0.0, 1.0) >= switchChance)
			return;

		bool tomed = pawn.FindInventory("PowerWeaponLevel2", true) != null;
		double maxWeight, maxOptimalWeight;
		Array<double> allWeights, optimalWeights;
		Array<Weapon> allWeapons, optimalWeapons;
		for (int i; i < PlayerPawn.NUM_WEAPON_SLOTS; ++i)
		{
			for (int j; j < player.Weapons.SlotSize(i); ++j)
			{
				let w = Weapon(pawn.FindInventory(player.Weapons.GetWeapon(i, j)));
				if (!w || w == weap)
					continue;
				if (tomed && w.SisterWeapon && w.SisterWeapon.bPowered_Up)
					continue;
				if (!tomed && w.bPowered_Up)
					continue;
				// Don't select melee weapons while idle.
				if (!target && w.bMeleeWeapon)
					continue;
				if (!w.CheckAmmo(Weapon.EitherFire, false))
					continue;
				
				double weight = GetWeaponWeighting(w, dist, strong);
				if (weap)
				{
					// Weight towards ranged weapons over melee since bots are probably
					// just going to die in those ranges anyway.
					if (!weap.bMeleeWeapon && w.bMeleeWeapon)
						weight *= 0.5;
					else if (weap.bMeleeWeapon && !w.bMeleeWeapon)
						weight *= 2.0;
				}

				if (weight <= 0.0)
					continue;

				maxWeight += weight;
				allWeights.Push(weight);
				allWeapons.Push(w);

				bool ignoreWimpy;
				if (strong)
				{
					let def = GetWeaponInfo(w);
					if (def)
						ignoreWimpy = def.GetRealNumber('StrengthBonus') > 1.0;
					else
						ignoreWimpy = w.bMeleeWeapon && w.bWimpy_Weapon;
				}

				// Don't ever intentionally select wimpy weapons unless it's
				// a ranged weapon we're swapping to from melee.
				if (weap && weap.bMeleeWeapon && !w.bMeleeWeapon)
					ignoreWimpy = true;

				if (w.bWimpy_Weapon && !ignoreWimpy)
					continue;

				maxOptimalWeight += weight;
				optimalWeights.Push(weight);
				optimalWeapons.Push(w);
			}
		}
		
		Weapon change = WP_NOCHANGE;
		// If only bad weapons were available but we have to change, just
		// pick anything.
		if (!optimalWeapons.Size() && mustChange)
		{
			double r = FRandom[BotSwitch](0.0, maxWeight);
			int i;
			double cur;
			for (; i < allWeapons.Size(); ++i)
			{
				cur += allWeights[i];
				if (cur >= r)
					break;
			}
			if (i < allWeapons.Size())
				change = allWeapons[i];
		}
		else
		{
			double r = FRandom[BotSwitch](0.0, maxOptimalWeight);
			int i;
			double cur;
			for (; i < optimalWeapons.Size(); ++i)
			{
				cur += optimalWeights[i];
				if (cur >= r)
					break;
			}
			if (i < optimalWeapons.Size())
				change = optimalWeapons[i];
		}

		// Normally weapon switching is handled via networked inputs which bots won't have access to, so
		// just use the weapon directly to swap.
		if (change != WP_NOCHANGE && change != weap)
			change.Use(false);
	}

	virtual void AdjustAngles()
	{
		let player = GetPlayer();
		let pawn = GetPawn();

		bool aimingAtTarget;
		Vector3 viewPos = (pawn.Pos.XY, player.ViewZ);

		Actor target = GetTarget();
		Actor goal = GetGoal();
		if (target && (IsOnCoolDown('LastSeen') || IsActorInView(target, Properties.GetRealNumber('ViewFOV', DEF_SIGHT_FOV))))
		{
			aimingAtTarget = true;
			AimPos = target.Pos.PlusZ(target.Height * 0.75 - target.FloorClip);

			class<Actor> proj;
			let weapInfo = GetWeaponInfo(player.ReadyWeapon);
			if (weapInfo)
				proj = weapInfo.GetString('ProjectileType');

			double dist = Level.Vec3Diff(viewPos, AimPos).Length();
			double maxTrackingRange = MAX_FIRE_TRACKING_RANGE * Properties.GetRealNumber('Predictiveness', 1.0);
			if (proj && maxTrackingRange > 0.0 && dist < maxTrackingRange)
			{
				let def = GetDefaultByType(proj);
				if (def.Speed > 0.0)
				{
					int tics = int(dist / def.Speed);
					double multi = 1.0 - dist / maxTrackingRange;
					AimPos.XY = Level.Vec2Offset(AimPos.XY, target.Vel.XY * tics * multi);
					AimPos.Z += target.Vel.Z * 0.2 * multi;
				}
			}
		}
		else if (goal is "Inventory")
		{
			AimPos = goal.Pos.PlusZ(goal.Height * 0.5 - goal.FloorClip);
		}
		else
		{
			double moveAng = pawn.MoveDir < 8 ? pawn.MoveDir * 45.0 : pawn.Angle;
			AimPos = viewPos + (moveAng.ToVector(), 0.0);
		}

		Vector3 diff = Level.Vec3Diff(viewPos, AimPos);
		if (!(diff ~== (0.0, 0.0, 0.0)))
		{
			double speed = Max(pawn.Speed, 1.0);
			double turnMulti = 1.0;
			if (player.AttackDown)
				turnMulti *= 0.5;
			if (aimingAtTarget)
			{
				if (target.bShadow)
					turnMulti *= 0.5;
				if (target.CurSector.GetLightLevel() <= DARKNESS_THRESHOLD)
					turnMulti *= 0.5;
			}

			// Turning is allowed to be faster if the angle is wider.
			double delta = Actor.DeltaAngle(pawn.Angle, diff.XY.Angle());
			double maxTurn = (MAX_TURN_SPEED + MAX_TURN_SPEED_BONUS * Abs(delta) / MAX_ANGLE) * speed * turnMulti;
			double turn = Clamp(delta, -maxTurn, maxTurn);
			SetAngle(pawn.Angle + turn);

			delta = Actor.DeltaAngle(pawn.Pitch, -Atan2(diff.Z, diff.XY.Length()));
			maxTurn = (MAX_TURN_SPEED + MAX_TURN_SPEED_BONUS * Abs(delta) / MAX_ANGLE) * speed * turnMulti;
			turn = Clamp(delta, -maxTurn, maxTurn);
			SetPitch(pawn.Pitch + turn);
		}
	}

	virtual void HandleMovement()
	{
		Actor goal = GetGoal();

		bool running = Evade || goal || GetTarget();
		if (--GetPawn().MoveCount < 0 || !Move(running))
		{
			bool avoidingPartner = Evade == GetPartner();
			if (Evade && (!avoidingPartner || !goal))
				NewMoveDirection(Evade, true, !avoidingPartner);
			else
				NewMoveDirection(goal, running: running);
		}
	}

	virtual bool TryFire()
	{
		if (IsOnCoolDown('Fire'))
			return false;

		let player = GetPlayer();
		Actor target = GetTarget();
		if (!target || !player.ReadyWeapon || player.PendingWeapon != WP_NOCHANGE || !IsActorInView(target, Properties.GetRealNumber('ViewFOV', DEF_SIGHT_FOV)))
			return false;

		let pawn = GetPawn();
		Vector3 origPos = pawn.Pos.PlusZ(pawn.ViewHeight - pawn.FloorClip);
		Vector3 dir = Level.Vec3Diff(origPos, AimPos);
		double dist = dir.Length();

		double minRange, maxRange;
		class<Actor> proj;
		int maxRefire = -1;
		let weapInfo = GetWeaponInfo(player.ReadyWeapon);
		if (weapInfo)
		{
			maxRefire = int(weapInfo.GetWholeNumber('MaxRefire', -1) * Properties.GetRealNumber('Aggressiveness', 1.0));
			minRange = weapInfo.GetRealNumber('ExplosiveRange');
			maxRange = weapInfo.GetRealNumber('MaxCombatRange') * Properties.GetRealNumber('Confidence', 1.0);
			proj = weapInfo.GetString('ProjectileType');
		}

		if (maxRefire >= 0 && player.Refire >= maxRefire)
		{
			SetCoolDown('Fire', BURST_DELAY_TICS);
			return false;
		}

		if ((minRange > 0.0 && dist <= minRange) || (maxRange > 0.0 && dist > maxRange))
			return false;

		if (!(dist ~== 0.0))
		{
			// By making this value larger, bots will start shooting before they've aligned
			// their shots, causing them to miss. They're also more likely to keep spraying
			// causing accuracy loss from refiring.
			double imprecisionMulti = 1.0;
			if (target.CurSector.GetLightLevel() <= DARKNESS_THRESHOLD)
				imprecisionMulti += 1.0;
			if (target.bShadow)
				imprecisionMulti += 2.0;

			Vector3 facing = (pawn.Angle.ToVector() * Cos(pawn.Pitch), -Sin(pawn.Pitch));
			if (facing dot (dir / dist) < Cos((BASE_IMPRECISION + Properties.GetRealNumber('Imprecision')) * imprecisionMulti))
				return false;
		}

		if (!CheckShotPath(AimPos, proj ? proj.GetClassName() : 'None', minRange))
			return false;

		SetButtons(BT_ATTACK, true);
		return true;
	}

	virtual string ModifySpawnProperty(Name property, string value)
	{
		return value;
	}

	virtual void BotRespawned()
	{
		SetTarget(null);

		let player = GetPlayer();
		let pawn = GetPawn();

		player.ReadyWeapon = null;
		player.PendingWeapon = pawn.PickNewWeapon(null);

		pawn.MoveDir = int(pawn.Angle / 45.0);
	}

	virtual int BotDamaged(Actor inflictor, Actor source, int damage, Name damageType, EDmgFlags flags = 0, double angle = 0.0)
	{
		if (source == GetTarget())
			SetCoolDown('LastSeen', SIGHT_COOL_DOWN_TICS);

		return damage;
	}

	virtual void BotDied(Actor source, Actor inflictor, EDmgFlags dmgFlags = 0, Name meansOfDeath = 'None')
	{
		AimPos = (0.0, 0.0, 0.0);
		ResetCoolDowns();
		Evade = null;
		SetGoal(null);
		SetPartner(0u);
		GetPlayer().Respawn_Time += Random[BotRespawn](MIN_RESPAWN_TIME, MAX_RESPAWN_TIME);
	}

	virtual void BotKilled(Actor victim) {}
	virtual void AddedInventory(Inventory item) {}
	virtual void RemovedInventory(Inventory item) {}
	virtual void UsedInventory(Inventory item, bool useFailed) {}
	virtual void Morphed() {}
	virtual void Unmorphed() {}
	virtual void FiredWeapon(bool altFire) {}
}

// Unused; only here for backwards compat

class CajunBodyNode : Actor
{
	Default
	{
		+NOSECTOR
		+NOGRAVITY
		+INVISIBLE
	}
}

class CajunTrace : Actor
{
	Default
	{
		Speed 12;
		Radius 6;
		Height 8;
		+NOBLOCKMAP
		+DROPOFF
		+MISSILE
		+NOGRAVITY
		+NOTELEPORT
	}
}
