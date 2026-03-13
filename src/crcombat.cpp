#include "cr.h"
#include "config.h"
#include "info.h"
#include "magic.h"
#include "operate.h"

TCombat::TCombat(void){
	this->Master = NULL;
	this->EarliestAttackTime = 0;
	this->EarliestDefendTime = 0;
	this->LastDefendTime = 0;
	this->LatestAttackTime = 0;
	this->AttackMode = ATTACK_MODE_BALANCED;
	this->ChaseMode = CHASE_MODE_NONE;
	this->SecureMode = SECURE_MODE_ENABLED;
	this->AttackDest = 0;
	this->Following = false;
	this->Shield = NONE;
	this->Close = NONE;
	this->Missile = NONE;
	this->Throw = NONE;
	this->Wand = NONE;
	this->Ammo = NONE;
	this->Fist = false;
	this->CombatDamage = 0;
	this->ActCombatEntry = 0;
	for(int i = 0; i < NARRAY(this->CombatList); i += 1){
		this->CombatList[i].ID = 0;
		this->CombatList[i].Damage = 0;
		this->CombatList[i].TimeStamp = 0;
	}
	this->LearningPoints = 0;
}

void TCombat::GetWeapon(void){
	this->Shield = NONE;
	this->Close = NONE;
	this->Missile = NONE;
	this->Throw = NONE;
	this->Wand = NONE;
	this->Ammo = NONE;
	this->Fist = true;

	// TODO(fusion): Check if `Master` is NULL?
	TCreature *Master = this->Master;
	if(!Master->CrObject.exists()){
		error("TCombat::GetWeapon: Creature object does not exist.\n");
		return;
	}

	for(int Position = INVENTORY_HAND_FIRST;
			Position <= INVENTORY_HAND_LAST;
			Position += 1){
		Object Obj = get_body_object(Master->ID, Position);
		if(Obj == NONE){
			continue;
		}

		ObjectType ObjType = Obj.get_object_type();

		if(ObjType.get_flag(RESTRICTLEVEL)){
			int CurrentLevel = Master->Skills[SKILL_LEVEL]->Get();
			int MinimumLevel = (int)ObjType.get_attribute(MINIMUMLEVEL);
			if(CurrentLevel < MinimumLevel){
				continue;
			}
		}

		if(ObjType.get_flag(RESTRICTPROFESSION) && Master->Type == PLAYER){
			uint32 ProfessionMask = ObjType.get_attribute(PROFESSIONS);
			uint8 Profession = ((TPlayer*)Master)->GetEffectiveProfession();
			if((ProfessionMask & (1 << Profession)) == 0){
				continue;
			}
		}

		if(ObjType.get_flag(SHIELD)){
			this->Shield = Obj;
		}

		if(ObjType.get_flag(WEAPON)){
			this->Close = Obj;
			this->Fist = false;
		}

		if(ObjType.get_flag(BOW)){
			this->Missile = Obj;
			this->Fist = false;
		}

		if(ObjType.get_flag(THROW)){
			this->Throw = Obj;
			this->Fist = false;
		}

		if(ObjType.get_flag(WAND)){
			this->Wand = Obj;
			this->Fist = false;
		}
	}
}

void TCombat::GetAmmo(void){
	if(this->Missile == NONE){
		if(this->Throw != NONE){
			this->Ammo = this->Throw;
		}else if(this->Wand != NONE){
			this->Ammo = this->Wand;
		}
		return;
	}

	// TODO(fusion): Check if `Master` is NULL?
	Object Ammo = get_body_object(this->Master->ID, INVENTORY_AMMO);
	this->Ammo = NONE;
	if(Ammo != NONE){
		ObjectType AmmoType = Ammo.get_object_type();
		if(AmmoType.get_flag(AMMO)){
			ObjectType BowType = this->Missile.get_object_type();
			if(AmmoType.get_attribute(AMMOTYPE) == BowType.get_attribute(BOWAMMOTYPE)){
				this->Ammo = Ammo;
			}
		}
	}
}

void TCombat::CheckCombatValues(void){
	Object OldShield = this->Shield;
	Object OldClose = this->Close;
	Object OldMissile = this->Missile;
	Object OldThrow = this->Throw;
	Object OldWand = this->Wand;
	Object OldAmmo = this->Ammo;
	bool OldFist = this->Fist;

	this->GetWeapon();
	this->GetAmmo();
	if(OldShield != this->Shield
			|| OldClose != this->Close
			|| OldMissile != this->Missile
			|| OldThrow != this->Throw
			|| OldWand != this->Wand
			|| OldAmmo != this->Ammo
			|| OldFist != this->Fist){
		this->DelayAttack(2000);
	}
}

static int WeaponTypeToSkill(int WeaponType){
	int Result = SKILL_FIST;
	switch(WeaponType){
		case WEAPON_NONE:		Result = SKILL_FIST; break;
		case WEAPON_SWORD:		Result = SKILL_SWORD; break;
		case WEAPON_CLUB:		Result = SKILL_CLUB; break;
		case WEAPON_AXE:		Result = SKILL_AXE; break;
		case WEAPON_SHIELD:		Result = SKILL_SHIELDING; break;
		case WEAPON_AMMO:		ATTR_FALLTHROUGH;
		case WEAPON_THROW:		Result = SKILL_DISTANCE; break;
	}
	return Result;
}

void TCombat::GetAttackValue(int *Value, int *SkillNr){
	int AttackValue;
	int WeaponType;
	if(this->Close != NONE){
		ObjectType CloseType = this->Close.get_object_type();
		AttackValue = (int)CloseType.get_attribute(WEAPONATTACKVALUE);
		WeaponType = (int)CloseType.get_attribute(WEAPONTYPE);
	}else if(this->Missile != NONE){
		ObjectType AmmoType = this->Ammo.get_object_type();
		AttackValue = (int)AmmoType.get_attribute(AMMOATTACKVALUE);
		WeaponType = WEAPON_AMMO;
	}else if(this->Throw != NONE){
		ObjectType ThrowType = this->Throw.get_object_type();
		AttackValue = (int)ThrowType.get_attribute(THROWATTACKVALUE);
		WeaponType = WEAPON_THROW;
	}else if(this->Wand != NONE){
		AttackValue = 0;
		WeaponType = WEAPON_NONE;
	}else{
		AttackValue = RaceData[this->Master->Race].Attack;
		WeaponType = WEAPON_NONE;
	}

	*Value = AttackValue;
	*SkillNr = WeaponTypeToSkill(WeaponType);
}

void TCombat::GetDefendValue(int *Value, int *SkillNr){
	int DefenseValue;
	int WeaponType;
	if(this->Shield != NONE){
		ObjectType ShieldType = this->Shield.get_object_type();
		DefenseValue = (int)ShieldType.get_attribute(SHIELDDEFENDVALUE);
		WeaponType = WEAPON_SHIELD;
	}else if(this->Close != NONE){
		ObjectType CloseType = this->Close.get_object_type();
		DefenseValue = (int)CloseType.get_attribute(WEAPONDEFENDVALUE);
		WeaponType = (int)CloseType.get_attribute(WEAPONTYPE);
	}else if(this->Throw != NONE){
		ObjectType ThrowType = this->Throw.get_object_type();
		DefenseValue = (int)ThrowType.get_attribute(THROWDEFENDVALUE);
		WeaponType = WEAPON_THROW;
	}else if(this->Missile != NONE){
		// TODO(fusion): This might be correct. Having a bow equipped will reduce
		// your defense to zero but having a wand equipped will give you base defense.
		DefenseValue = 0;
		WeaponType = WEAPON_AMMO;
	}else{
		DefenseValue = RaceData[this->Master->Race].Defend;
		WeaponType = WEAPON_NONE;
	}

	*Value = DefenseValue;
	*SkillNr = WeaponTypeToSkill(WeaponType);
}

int TCombat::GetAttackDamage(void){
	int MaxValue, SkillNr;
	this->GetAttackValue(&MaxValue, &SkillNr);
	if(this->AttackMode == ATTACK_MODE_OFFENSIVE){
		MaxValue += (MaxValue * 2) / 10;
	}else if(this->AttackMode == ATTACK_MODE_DEFENSIVE){
		MaxValue -= (MaxValue * 4) / 10;
	}

	TSkill *Skill = this->Master->Skills[SkillNr];
	int Result = Skill->ProbeValue(MaxValue, this->LearningPoints > 0);
	if(this->LearningPoints > 0){
		this->LearningPoints -= 1;
	}
	return Result;
}

int TCombat::GetDefendDamage(void){
	if(this->EarliestDefendTime > ServerMilliseconds){
		return 0;
	}

	this->EarliestDefendTime = this->LastDefendTime + 2000;
	this->LastDefendTime = ServerMilliseconds;

	int AttackMode = this->AttackMode;
	if(this->Following || this->AttackDest == 0){
		AttackMode = ATTACK_MODE_DEFENSIVE;
	}

	int MaxValue, SkillNr;
	this->GetDefendValue(&MaxValue, &SkillNr);
	if(AttackMode == ATTACK_MODE_OFFENSIVE){
		MaxValue -= (MaxValue * 4) / 10;
	}else if(AttackMode == ATTACK_MODE_DEFENSIVE){
		MaxValue += (MaxValue * 8) / 10;
	}

	TSkill *Skill = this->Master->Skills[SkillNr];
	bool Increase = (this->Shield != NONE && this->LearningPoints > 0);
	int Result = Skill->ProbeValue(MaxValue, Increase);
	if(Increase){
		this->LearningPoints -= 1;
	}

	Object Shield = this->Shield;
	if(Shield != NONE){
		ObjectType ShieldType = Shield.get_object_type();
		if(ShieldType.get_flag(WEAROUT)){
			uint32 RemainingUses = Shield.get_attribute(REMAININGUSES);
			if(RemainingUses > 1){
				change(Shield, REMAININGUSES, RemainingUses - 1);
			}else{
				ObjectType WearOutType = (int)ShieldType.get_attribute(WEAROUTTARGET);
				change(Shield, WearOutType, 0);
				// TODO(fusion): Probably just check anyways, to be sure.
				if(!Shield.exists() || !WearOutType.get_flag(SHIELD)){
					this->CheckCombatValues();
				}
			}
		}
	}

	return Result;
}

int TCombat::GetArmorStrength(void){
	int Armor = 0;
	TCreature *Master = this->Master;
	for(int Position = INVENTORY_FIRST;
			Position <= INVENTORY_LAST;
			Position += 1){
		Object Obj = get_body_object(Master->ID, Position);
		if(Obj.exists()){
			ObjectType ObjType = Obj.get_object_type();
			if(ObjType.get_flag(CLOTHES) && ObjType.get_flag(ARMOR)
			&& (int)ObjType.get_attribute(BODYPOSITION) == Position){
				Armor += (int)ObjType.get_attribute(ARMORVALUE);
			}
		}
	}

	Armor += RaceData[Master->Race].Armor;
	if(Armor >= 2){
		Armor = (Armor / 2) + rand() % (Armor / 2);
	}
	return Armor;
}

int TCombat::GetDistance(void){
	int Distance = 0;
	if(this->Close != NONE || this->Fist){
		Distance = 1; // DISTANCE_CLOSE ?
	}else if(this->Throw != NONE){
		Distance = 2; // DISTANCE_THROW ?
	}else if(this->Missile != NONE || this->Wand != NONE){
		Distance = 3; // DISTANCE_RANGE ?
	}
	return Distance;
}

void TCombat::ActivateLearning(void){
	this->LearningPoints = 30;
}

void TCombat::SetAttackMode(uint8 AttackMode){
	if(AttackMode != ATTACK_MODE_OFFENSIVE
			&& AttackMode != ATTACK_MODE_BALANCED
			&& AttackMode != ATTACK_MODE_DEFENSIVE){
		error("TCombat::SetAttackMode: Invalid attack mode %d.\n", AttackMode);
		return;
	}

	if(this->AttackMode != AttackMode){
		this->DelayAttack(2000);
		this->AttackMode = AttackMode;
	}
}

void TCombat::SetChaseMode(uint8 ChaseMode){
	if(ChaseMode != CHASE_MODE_NONE && ChaseMode != CHASE_MODE_CLOSE){
		error("TCombat::SetChaseMode: Invalid chase mode %d.\n", ChaseMode);
		return;
	}

	this->ChaseMode = ChaseMode;
}

void TCombat::SetSecureMode(uint8 SecureMode){
	if(SecureMode != SECURE_MODE_DISABLED && SecureMode != SECURE_MODE_ENABLED){
		error("TCombat::SetSecureMode: Invalid secure mode %d.\n", SecureMode);
		return;
	}

	this->SecureMode = SecureMode;
}

void TCombat::SetAttackDest(uint32 TargetID, bool Follow){
	if(this->AttackDest == TargetID && this->Following == Follow){
		return;
	}

	TCreature *Master = this->Master;
	if(TargetID == 0 || TargetID == Master->ID){
		this->StopAttack(0);
		return;
	}

	TCreature *Target = GetCreature(TargetID);
	if(Target == NULL){
		this->StopAttack(0);
		return;
	}

	if(!Follow){
		if(Master->Type == PLAYER && Target->Type == PLAYER){
			if(this->SecureMode == SECURE_MODE_ENABLED && WorldType == NORMAL
					&& !((TPlayer*)Master)->IsAttackJustified(TargetID)){
				this->StopAttack(0);
				throw SECUREMODE;
			}
		}

		if(Master->Type == PLAYER && !check_right(Master->ID, ATTACK_EVERYWHERE)){
			if(is_protection_zone(Master->posx, Master->posy, Master->posz)
			|| is_protection_zone(Target->posx, Target->posy, Target->posz)){
				this->StopAttack(0);
				throw PROTECTIONZONE;
			}
		}

		if(Master->Type == PLAYER && check_right(Master->ID, NO_ATTACK)){
			this->StopAttack(0);
			throw ATTACKNOTALLOWED;
		}

		if(Master->Type == PLAYER && Target->Type == PLAYER){
			if(((TPlayer*)Master)->GetRealProfession() == PROFESSION_NONE
					&& !check_right(Master->ID, ATTACK_EVERYWHERE)){
				this->StopAttack(0);
				throw ATTACKNOTALLOWED;
			}
		}

		if(Target->Type == NPC){
			this->StopAttack(0);
			throw ATTACKNOTALLOWED;
		}

		if(WorldType == NON_PVP && Master->IsPeaceful() && Target->IsPeaceful()){
			if(Master->Type != PLAYER || !check_right(Master->ID, ATTACK_EVERYWHERE)){
				this->StopAttack(0);
				throw ATTACKNOTALLOWED;
			}
		}
	}

	if(Master->Type == PLAYER && Target->Type != PLAYER){
		if(Target->IsInvisible()){
			this->StopAttack(0);
			throw TARGETLOST;
		}
	}

	int Distance = object_distance(Master->CrObject, Target->CrObject);
	if(Distance > 8){
		this->StopAttack(0);
		throw TARGETLOST;
	}

	this->AttackDest = TargetID;
	this->Following = Follow;
	if(!Follow){
		Target->AttackStimulus(Master->ID);
		Master->BlockLogout(60, Target->Type == PLAYER);
		if(Master->Type == PLAYER && Target->Type == PLAYER){
			((TPlayer*)Master)->RecordAttack(TargetID);
		}
		this->LatestAttackTime = 0;
	}
}

void TCombat::CanToDoAttack(void){
	if(this->AttackDest == 0){
		return;
	}

	// TODO(fusion): There is some `CanAttack` function inlined here.
	TCreature *Master = this->Master;
	if(Master == NULL){
		error("TCombat::CanAttack: No master set!\n");
		throw ERROR;
	}

	TCreature *Target = GetCreature(this->AttackDest);
	if(Target == NULL){
		this->StopAttack(0);
		throw TARGETLOST;
	}

	if(Master->Type == PLAYER && Target->Type != PLAYER){
		if(Target->IsInvisible()){
			this->StopAttack(0);
			throw TARGETLOST;
		}
	}

	if(!this->Following){
		if(Master->Type == PLAYER && Target->Type == PLAYER){
			if(this->SecureMode == SECURE_MODE_ENABLED && WorldType == NORMAL
					&& !((TPlayer*)Master)->IsAttackJustified(Target->ID)){
				this->StopAttack(0);
				throw SECUREMODE;
			}
		}

		if(WorldType == NON_PVP){
			if(Master->IsPeaceful() && Target->IsPeaceful()){
				if(Master->Type != PLAYER || !check_right(Master->ID, ATTACK_EVERYWHERE)){
					this->StopAttack(0);
					throw ATTACKNOTALLOWED;
				}
			}
		}
	}

	int Distance = object_distance(Master->CrObject, Target->CrObject);
	if(Distance > 8){
		this->StopAttack(0);
		throw TARGETLOST;
	}

	int ChaseMode = this->ChaseMode;
	if(this->Following){
		ChaseMode = CHASE_MODE_CLOSE;
	}

	if(ChaseMode == CHASE_MODE_CLOSE){
		if(Distance > 1){
			Master->ToDoGo(Target->posx, Target->posy, Target->posz, false, 3);
		}
	}else if(ChaseMode == CHASE_MODE_RANGE){
		if(Distance > 4){
			Master->ToDoGo(Target->posx, Target->posy, Target->posz, false, Distance - 4);
		}else if(Distance < 4){
			int DestX, DestY, DestZ;
			if(search_flight_field(Master->ID, Target->ID, &DestX, &DestY, &DestZ)){
				Master->ToDoGo(DestX, DestY, DestZ, true, -1);
			}
		}
	}
}

void TCombat::StopAttack(int Delay){
	if(Delay == 0){
		this->AttackDest = 0;
		if(this->Master->Type == PLAYER){
			SendClearTarget(this->Master->Connection);
		}
	}else{
		this->LatestAttackTime = RoundNr + Delay;
	}
}

void TCombat::DelayAttack(int Milliseconds){
	uint32 EarliestAttackTime = ServerMilliseconds + Milliseconds;
	if(this->EarliestAttackTime < EarliestAttackTime){
		this->EarliestAttackTime = EarliestAttackTime;
	}
}

void TCombat::Attack(void){
	if(this->AttackDest == 0 || this->Following){
		return;
	}

	// TODO(fusion): There is some `CanAttack` function inlined here.
	TCreature *Master = this->Master;
	if(Master == NULL){
		error("TCombat::CanAttack: No master set!\n");
		throw ERROR;
	}

	TCreature *Target = GetCreature(this->AttackDest);
	if(Target == NULL){
		this->StopAttack(0);
		throw TARGETLOST;
	}

	// NOTE(fusion): This probably has something to do with calling `StopAttack`
	// with a non zero delay.
	if(this->LatestAttackTime != 0 && this->LatestAttackTime < RoundNr){
		this->StopAttack(0);
		return;
	}

	if(Master->Type == PLAYER && Target->Type != PLAYER){
		if(Target->IsInvisible()){
			this->StopAttack(0);
			throw TARGETLOST;
		}
	}

	if(Master->Type == PLAYER && Target->Type == PLAYER){
		if(this->SecureMode == SECURE_MODE_ENABLED && WorldType == NORMAL
				&& !((TPlayer*)Master)->IsAttackJustified(Target->ID)){
			this->StopAttack(0);
			throw SECUREMODE;
		}
	}

	// TODO(fusion): It is weird that max attack distance is hardcoded.
	// Actually, it is related to the maximum visible distance on a
	// creature's viewport.
	int Distance = object_distance(Master->CrObject, Target->CrObject);
	if(Distance > 8){
		this->StopAttack(0);
		throw TARGETLOST;
	}

	if(Master->Type == PLAYER && Target->Type == PLAYER){
		if(((TPlayer*)Master)->GetRealProfession() == PROFESSION_NONE
				&& !check_right(Master->ID, ATTACK_EVERYWHERE)){
			this->StopAttack(0);
			throw ATTACKNOTALLOWED;
		}
	}

	if(Master->Type == PLAYER){
		if(check_right(Master->ID, NO_ATTACK)){
			this->StopAttack(0);
			throw ATTACKNOTALLOWED;
		}
	}

	if(is_protection_zone(Master->posx, Master->posy, Master->posz)
	|| is_protection_zone(Target->posx, Target->posy, Target->posz)){
		this->StopAttack(0);
		throw PROTECTIONZONE;
	}

	Master->BlockLogout(60, Target->Type == PLAYER);
	Target->BlockLogout(60, false);

	if(Master->Type == PLAYER && Target->Type == PLAYER){
		((TPlayer*)Master)->RecordAttack(Target->ID);
	}

	this->DelayAttack(200);

	// TODO(fusion): This `Range` value doesn't make a lot of sense.
	int Range = this->GetDistance();
	if(Range == 1){
		if(Distance > 1){
			throw TARGETOUTOFRANGE;
		}
		this->CloseAttack(Target);
	}else{
		if(Range < 2 || Range > 3){
			throw ERROR;
		}

		// TODO(fusion): These are related to the maximum visible distance on
		// each coordinate.
		if(std::abs(Master->posx - Target->posx) > 7
		|| std::abs(Master->posy - Target->posy) > 5){
			throw TARGETOUTOFRANGE;
		}

		// NOTE(fusion): Originally, this was a single function `RangeAttack`
		// and I quickly realized that readability would be massively improved
		// by splitting it into `DistanceAttack` and `WandAttack`.
		if(this->Missile != NONE || this->Throw != NONE){
			this->DistanceAttack(Target);
		}else if(this->Wand != NONE){
			this->WandAttack(Target);
		}else{
			throw ERROR;
		}
	}

	this->DelayAttack(2000);

	if(Target->IsDead){
		this->StopAttack(0);
	}
}

void TCombat::CloseAttack(TCreature *Target){
	int Attack = this->GetAttackDamage();
	int Defense = Target->Combat.GetDefendDamage();
	int Damage = Attack - Defense;
	if(Damage < 0){
		Damage = 0;
	}

	int DamageDone = Target->Damage(Master, Damage, DAMAGE_PHYSICAL);
	if(DamageDone > 0){
		this->ActivateLearning();
	}

	int Poison = GetRacePoison(Master->Race);
	if(Poison != 0){
		if(DamageDone > 0 || (Attack > Defense && (rand() % 5) == 0)){
			int PoisonDamage = random(Poison / 2, Poison);
			if(PoisonDamage > 0){
				Target->Damage(Master, PoisonDamage, DAMAGE_POISON_PERIODIC);
				if(Target->Type == PLAYER){
					SendMessage(Target->Connection, TALK_STATUS_MESSAGE, "You are poisoned.");
				}
			}
		}
	}

	Object Close = this->Close;
	if(Close != NONE){
		ObjectType CloseType = Close.get_object_type();
		if(CloseType.get_flag(WEAROUT)){
			uint32 RemainingUses = Close.get_attribute(REMAININGUSES);
			if(RemainingUses > 1){
				change(Close, REMAININGUSES, RemainingUses - 1);
			}else{
				ObjectType WearOutType = (int)CloseType.get_attribute(WEAROUTTARGET);
				change(Close, WearOutType, 0);
				this->CheckCombatValues();
			}
		}
	}
}

void TCombat::WandAttack(TCreature *Target){
	ASSERT(this->Wand != NONE);

	TCreature *Master = this->Master;
	int Distance = std::max<int>(
			std::abs(Master->posx - Target->posx),
			std::abs(Master->posy - Target->posy));

	ObjectType WandType = this->Wand.get_object_type();
	if(Distance > (int)WandType.get_attribute(WANDRANGE)){
		throw TARGETOUTOFRANGE;
	}

	if(!throw_possible(Master->posx, Master->posy, Master->posz,
				Target->posx, Target->posy, Target->posz, 0)){
		throw TARGETHIDDEN;
	}

	int DamageType = WandType.get_attribute(WANDDAMAGETYPE);
	int AnimType = WandType.get_attribute(WANDMISSILE);
	int ManaConsumption = WandType.get_attribute(WANDMANACONSUMPTION);
	int AttackStrength = WandType.get_attribute(WANDATTACKSTRENGTH);
	int AttackVariation = (int)WandType.get_attribute(WANDATTACKVARIATION);

	try{
		check_mana(Master, ManaConsumption, 0, 0);
	}catch(RESULT r){
		if(r == NOTENOUGHMANA){
			throw OUTOFAMMO;
		}else{
			throw r;
		}
	}

	int Damage = AttackStrength + random(-AttackVariation, AttackVariation);
	if(Target->Damage(Master, Damage, DamageType) > 0){
		this->ActivateLearning();
	}

	::missile(Master->CrObject, Target->CrObject, AnimType);
}

void TCombat::DistanceAttack(TCreature *Target){
	ASSERT(this->Missile != NONE || this->Throw != NONE);

	int HitChance;
	int DamageType;
	int AnimType;
	int Fragility;
	int SpecialEffect;
	int EffectStrength;

	TCreature *Master = this->Master;
	int DistanceX = std::abs(Master->posx - Target->posx);
	int DistanceY = std::abs(Master->posy - Target->posy);
	int Distance = std::max<int>(DistanceX, DistanceY);

	if(this->Missile != NONE){
		this->GetAmmo();
		if(this->Ammo == NONE || !this->Ammo.exists()){
			throw OUTOFAMMO;
		}

		ObjectType BowType = this->Missile.get_object_type();
		ObjectType AmmoType = this->Ammo.get_object_type();
		if(Distance > (int)BowType.get_attribute(BOWRANGE)){
			throw TARGETOUTOFRANGE;
		}

		HitChance = 90;
		DamageType = DAMAGE_PHYSICAL;
		AnimType = AmmoType.get_attribute(AMMOMISSILE);
		Fragility = 100;
		SpecialEffect = AmmoType.get_attribute(AMMOSPECIALEFFECT);
		EffectStrength = AmmoType.get_attribute(AMMOEFFECTSTRENGTH);
	}else{
		ASSERT(this->Throw != NONE);
		ObjectType ThrowType = this->Throw.get_object_type();
		if(Distance > (int)ThrowType.get_attribute(THROWRANGE)){
			throw TARGETOUTOFRANGE;
		}

		HitChance = 75;
		DamageType = DAMAGE_PHYSICAL;
		AnimType = ThrowType.get_attribute(THROWMISSILE);
		Fragility = ThrowType.get_attribute(THROWFRAGILITY);
		SpecialEffect = ThrowType.get_attribute(THROWSPECIALEFFECT);
		EffectStrength = ThrowType.get_attribute(THROWEFFECTSTRENGTH);
	}

	if(!throw_possible(Master->posx, Master->posy, Master->posz,
				Target->posx, Target->posy, Target->posz, 0)){
		throw TARGETHIDDEN;
	}

	int Difficulty = (Distance >= 2) ? Distance : 5;
	bool Hit = Master->Skills[SKILL_DISTANCE]->Probe(
			Difficulty * 15, HitChance, this->LearningPoints > 0);
	if(this->LearningPoints > 0){
		this->LearningPoints -= 1;
	}

	int DropX = Target->posx;
	int DropY = Target->posy;
	int DropZ = Target->posz;
	if(Hit){
		int Attack = this->GetAttackDamage();

		// TODO(fusion): This doesn't make any sense. The disassembly looks
		// correct but I feel we're missing something here. Or maybe defense
		// doesn't work with ranged attacks but worn them out? But then only
		// if the attacker is wearing a shield? This is a probably a bug.
		if(this->Shield != NONE){
			Target->Combat.GetDefendDamage();
		}

		if(Target->Damage(Master, Attack, DamageType) > 0){
			this->ActivateLearning();
		}
	}else{
		if(DistanceX > 1 || DistanceY > 1){
			DropX += (rand() % 3) - 1;
			DropY += (rand() % 3) - 1;
		}

		if(!coordinate_flag(DropX, DropY, DropZ, BANK)
		|| coordinate_flag(DropX, DropY, DropZ, UNLAY)
		|| !throw_possible(Master->posx, Master->posy, Master->posz, DropX, DropY, DropZ, 0)){
			DropX = Target->posx;
			DropY = Target->posy;
		}
	}

	Object DropCon = get_map_container(DropX, DropY, DropZ);
	::missile(Master->CrObject, DropCon, AnimType);

	if(SpecialEffect == 1){ // POISON ARROW
		if(Hit){
			Target->Damage(Master, EffectStrength, DAMAGE_POISON_PERIODIC);
		}
	}else if(SpecialEffect == 2){ // BURST ARROW
		int Damage = compute_damage(Master, 0, EffectStrength, EffectStrength);
		DamageImpact Impact(Master, DAMAGE_PHYSICAL, Damage, false);
		circle_shape_spell(Master, DropX, DropY, DropZ, INT_MAX,
				ANIMATION_NONE, 2, &Impact, EFFECT_FIRE_BURST);
	}

	try{
		if(random(0, 99) < Fragility){
			delete_op(this->Ammo, 1);
		}else{
			move(0, this->Ammo, DropCon, 1, false, NONE);
		}
	}catch(RESULT r){
		if(r != DESTROYED){
			error("TCombat::RangeAttack: Could not move/delete ammo"
					" (Exception %d, [%d,%d,%d]).\n", r, DropX, DropY, DropZ);
		}
	}

	if(!Hit){
		graphical_effect(DropX, DropY, DropZ, EFFECT_POFF);
	}
}

void TCombat::AddDamageToCombatList(uint32 Attacker, uint32 Damage){
	this->CombatDamage += Damage;
	for(int i = 0; i < NARRAY(this->CombatList); i += 1){
		if(this->CombatList[i].ID == Attacker){
			this->CombatList[i].Damage += Damage;
			this->CombatList[i].TimeStamp = RoundNr;
			return;
		}
	}

	int NextEntryIndex = this->ActCombatEntry;
	this->CombatList[NextEntryIndex].ID = Attacker;
	this->CombatList[NextEntryIndex].Damage = Damage;
	this->CombatList[NextEntryIndex].TimeStamp = RoundNr;

	NextEntryIndex += 1;
	if(NextEntryIndex >= NARRAY(this->CombatList)){
		NextEntryIndex = 0;
	}
	this->ActCombatEntry = NextEntryIndex;
}

uint32 TCombat::GetDamageByCreature(uint32 CreatureID){
	uint32 Damage = 0;
	for(int i = 0; i < NARRAY(this->CombatList); i += 1){
		if(this->CombatList[i].ID == CreatureID){
			Damage = this->CombatList[i].Damage;
			break;
		}
	}
	return Damage;
}

uint32 TCombat::GetMostDangerousAttacker(void){
	uint32 Attacker = 0;
	uint32 MaxDamage = 0;
	for(int i = 0; i < NARRAY(this->CombatList); i += 1){
		if((RoundNr - this->CombatList[i].TimeStamp) < 60
				&& this->CombatList[i].Damage > MaxDamage){
			Attacker = this->CombatList[i].ID;
			MaxDamage = this->CombatList[i].Damage;
		}
	}
	return Attacker;
}

void TCombat::DistributeExperiencePoints(uint32 Exp){
	TCreature *Master = this->Master;
	print(3, "%s has died. Distributing %u EXP...\n", Master->Name, Exp);
	if(this->CombatDamage == 0){
		return;
	}

	for(int i = 0; i < NARRAY(this->CombatList); i += 1){
		TCreature *Attacker = GetCreature(this->CombatList[i].ID);
		if(Attacker == NULL || Attacker->IsDead){
			continue;
		}

		int Amount = (int)((Exp * this->CombatList[i].Damage) / this->CombatDamage);
		if(Master->Type == PLAYER && Attacker->Type == PLAYER){
			if(((TPlayer*)Master)->InPartyWith((TPlayer*)Attacker, true)){
				continue;
			}

			int MasterLevel = Master->Skills[SKILL_LEVEL]->Get();
			int AttackerLevel = Attacker->Skills[SKILL_LEVEL]->Get();
			int MaxLevel = (MasterLevel * 11) / 10;
			if(AttackerLevel >= MaxLevel){
				continue;
			}

			Amount = ((MaxLevel - AttackerLevel) * Amount) / MasterLevel;
			print(3, "%s receives %d EXP.\n", Attacker->Name, Amount);
		}

		if(Amount > 0){
			if(Attacker->Type == PLAYER){
				// NOTE(fusion): Enable soul regeneration.
				int AttackerLevel = Attacker->Skills[SKILL_LEVEL]->Get();
				if(Amount >= AttackerLevel){
					int Interval = 120;
					if(((TPlayer*)Attacker)->GetActivePromotion()){
						Interval = 15;
					}

					int Count = Attacker->Skills[SKILL_SOUL]->TimerValue() % Interval;
					if(Count == 0){
						Count = Interval;
					}

					Attacker->SetTimer(SKILL_SOUL, (240 / Interval), Count, Interval, -1);
				}
			}

			Attacker->Skills[SKILL_LEVEL]->Increase(Amount);
			textual_effect(Attacker->CrObject, COLOR_WHITE, "%d", Amount);
		}
	}
}


