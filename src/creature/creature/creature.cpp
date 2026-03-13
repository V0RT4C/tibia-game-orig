#include "cr.h"
#include "communication.h"
#include "config.h"
#include "info.h"
#include "operate.h"
#include "writer.h"

// TCreature
// =============================================================================
TCreature::TCreature(void) :
		SkillBase(),
		Combat(),
		ToDoList(0, 20, 10)
{
	this->Combat.Master = this;
	this->ID = 0;
	this->NextHashEntry = NULL;
	this->NextChainCreature =  0;
	this->Name[0] = 0;
	this->Murderer[0] = 0;
	this->OrgOutfit = {};
	this->Outfit = {};
	this->startx = 0;
	this->starty = 0;
	this->startz = 0;
	this->posx = 0;
	this->posy = 0;
	this->posz = 0;
	this->Sex = 1;
	this->Race = 0;
	this->Direction = DIRECTION_SOUTH;
	this->Radius = INT_MAX;
	this->IsDead = false;
	this->LoseInventory = LOSE_INVENTORY_ALL;
	this->LoggingOut = false;
	this->LogoutAllowed = false;
	this->EarliestLogoutRound = 0;
	this->EarliestProtectionZoneRound = 0;
	this->EarliestYellRound = 0;
	this->EarliestTradeChannelRound = 0;
	this->EarliestSpellTime = 0;
	this->EarliestMultiuseTime = 0;
	this->EarliestWalkTime = 0;
	this->LifeEndRound = 0;
	this->FirstKnowingConnection = NULL;
	this->SummonedCreatures = 0;
	this->FireDamageOrigin = 0;
	this->PoisonDamageOrigin = 0;
	this->EnergyDamageOrigin = 0;
	this->CrObject = NONE;
	this->ActToDo = 0;
	this->NrToDo = 0;
	this->NextWakeup = 0;
	this->Stop = false;
	this->LockToDo = false;
	this->Connection = NULL;

	for(int i = 0; i < NARRAY(this->Skills); i += 1){
		this->NewSkill((uint16)i, this);
	}
}

TCreature::~TCreature(void){
	// TODO(fusion): Bruuhh... these exceptions...
	if(this->IsDead){
		int Race = this->Race;
		int PoolLiquid = LIQUID_NONE;
		if(race_data[Race].Blood == BT_BLOOD){
			PoolLiquid = LIQUID_BLOOD;
		}else if(race_data[Race].Blood == BT_SLIME){
			PoolLiquid = LIQUID_SLIME;
		}

		if(PoolLiquid != LIQUID_NONE){
			try{
				create_pool(get_map_container(this->CrObject),
						get_special_object(BLOOD_POOL),
						PoolLiquid);
			}catch(RESULT r){
				if(r != NOROOM && r != DESTROYED){
					error("TCreature::~TCreature: Cannot place blood pool (Exc %d, Pos [%d,%d,%d]).\n",
							r, this->posx, this->posy, this->posz);
				}
			}
		}

		ObjectType CorpseType = (this->Sex == 1) // MALE ?
				? race_data[Race].MaleCorpse
				: race_data[Race].FemaleCorpse;

		if(CorpseType.get_flag(MAGICFIELD)){
			Object Obj = get_first_object(this->posx, this->posy, this->posz);
			while(Obj != NONE){
				Object Next = Obj.get_next_object();
				if(Obj.get_object_type().get_flag(MAGICFIELD)){
					try{
						delete_op(Obj, -1);
					}catch(RESULT r){
						error("TCreature::~TCreature: Exception %d when deleting a field.\n", r);
					}
				}
				Obj = Next;
			}
		}

		try{
			Object Con = get_map_container(this->posx, this->posy, this->posz);
			Object Corpse = create(Con, CorpseType, 0);
			log_message("game", "Death of %s: LoseInventory=%d.\n", this->Name, this->LoseInventory);

			if(this->Type == PLAYER){
				char Help[128];
				sprintf(Help, "You recognize %s", this->Name);
				if(this->Murderer[0] != 0){
					if(this->Sex == 1){ // MALE ?
						strcat(Help, ". He was killed by ");
					}else{
						strcat(Help, ". She was killed by ");
					}
					strcat(Help, this->Murderer);
				}
				change(Corpse, TEXTSTRING, AddDynamicString(Help));
			}

			if(this->LoseInventory != LOSE_INVENTORY_NONE){
				for(int Position = INVENTORY_FIRST;
						Position <= INVENTORY_LAST;
						Position += 1){
					Object Item = get_body_object(this->ID, Position);
					if(Item == NONE){
						continue;
					}

					if(this->LoseInventory == LOSE_INVENTORY_ALL
							|| Item.get_object_type().get_flag(CONTAINER)
							|| random(0, 9) == 0){
						::move(0, Item, Corpse, -1, false, NONE);
					}
				}
			}

			if(this->Type == PLAYER && this->LoseInventory != LOSE_INVENTORY_ALL){
				((TPlayer*)this)->SaveInventory();
			}
		}catch(RESULT r){
			error("TCreature::~TCreature: Cannot place corpse/inventory (Exc %d, Pos [%d,%d,%d], %s).\n",
					r, this->posx, this->posy, this->posz, this->Name);
		}
	}

	if(this->CrObject != NONE && this->CrObject.exists()){
		this->DelOnMap();
	}

	this->ToDoClear();

	if(this->Type == PLAYER && this->Connection != NULL){
		this->Connection->Logout(30, true);
	}

	this->DelInCrList();

	if(this->ID != 0){
		this->DelID();
	}

	for(TKnownCreature *KnownCreature = this->FirstKnowingConnection;
			KnownCreature != NULL;
			KnownCreature = KnownCreature->Next){
		if(KnownCreature->CreatureID != this->ID){
			error("TCreature::~TCreature: Chain link error for creature %u.\n", this->ID);
		}
		KnownCreature->State = KNOWNCREATURE_FREE;
	}
}

void TCreature::StartLogout(bool Force, bool StopFight){
	this->LoggingOut = true;
	if(Force || LagDetected()){
		this->LogoutAllowed = true;
	}

	if(this->Type == PLAYER && this->Connection != NULL){
		this->Connection->Logout(0, true);
	}

	this->Combat.StopAttack(StopFight ? 0 : 60);
}

int TCreature::LogoutPossible(void){
	if(!this->LogoutAllowed && !this->IsDead && !GameEnding()){
		if(this->EarliestLogoutRound > RoundNr && !LagDetected()){
			return 1; // LOGOUT_COMBAT ?
		}

		if(is_no_logout_field(this->posx, this->posy, this->posz)){
			return 2; // LOGOUT_FIELD ?
		}

		this->LogoutAllowed = true;
	}

	return 0; // LOGOUT_OK ?
}

void TCreature::BlockLogout(int Delay, bool BlockProtectionZone){
	if(WorldType == NON_PVP){
		BlockProtectionZone = false;
	}

	if(this->Type == PLAYER && !check_right(this->ID, NO_LOGOUT_BLOCK)){
		if(BlockProtectionZone || this->EarliestProtectionZoneRound > RoundNr){
			uint32 EarliestProtectionZoneRound = RoundNr + Delay;
			if(this->EarliestProtectionZoneRound < EarliestProtectionZoneRound){
				this->EarliestProtectionZoneRound = EarliestProtectionZoneRound;
			}
		}else if(this->Connection == NULL){
			// NOTE(fusion): This is a failsafe to avoid extending the earliest
			// logout round of a player that got disconnected in combat.
			return;
		}

		uint32 EarliestLogoutRound = RoundNr + Delay;
		if(this->EarliestLogoutRound < EarliestLogoutRound){
			this->EarliestLogoutRound = EarliestLogoutRound;
		}

		((TPlayer*)this)->CheckState();
	}
}

int TCreature::GetHealth(void){
	int MaxHitPoints = this->Skills[SKILL_HITPOINTS]->Max;
	if(MaxHitPoints <= 0){
		if(!this->IsDead){
			error("TCreature::GetHealth: MaxHitpoints of %s is %d, even though it is not dead.\n",
					this->Name, MaxHitPoints);
		}
		return 0;
	}

	int CurrentHitPoints = this->Skills[SKILL_HITPOINTS]->Get();
	int Health = CurrentHitPoints * 100 / MaxHitPoints;
	if(Health <= 0){
		Health = (int)(CurrentHitPoints != 0);
	}
	return Health;
}

int TCreature::GetSpeed(void){
	TSkill *GoStrength = this->Skills[SKILL_GO_STRENGTH];
	if(GoStrength == NULL){
		error("TCreature::GetSpeed: No skill GOSTRENGTH present.\n");
		return 0;
	}

	return GoStrength->Get() * 2 + 80;
}

int TCreature::Damage(TCreature *Attacker, int Damage, int DamageType){
	if(this->IsDead || this->Type == NPC){
		return 0;
	}

	if(Attacker != NULL && this->Type == PLAYER){
		if(this->Connection != NULL){
			SendMarkCreature(this->Connection, Attacker->ID, COLOR_BLACK);
		}

		if(Attacker->Type == PLAYER
				&& DamageType != DAMAGE_POISON_PERIODIC
				&& DamageType != DAMAGE_FIRE_PERIODIC
				&& DamageType != DAMAGE_ENERGY_PERIODIC){
			Damage = (Damage + 1) / 2;
		}
	}

	// NOTE(fusion): `Responsible` is either the attacker or its master. It is
	// always valid if `Attacker` is not NULL.
	uint32 AttackerID = 0;
	TCreature *Responsible = Attacker;
	if(Attacker != NULL){
		AttackerID = Attacker->ID;

		uint32 MasterID = Attacker->GetMaster();
		if(MasterID != 0){
			// NOTE(fusion): This is very subtle but we could hit a case where the
			// master logs out or dies but the summon has a ToDoAttack queued, in
			// which case it would try to attack before checking whether it should
			// despawn in `TMonster::IdleStimulus`, causing `Responsible` to be NULL
			// even though `Attacker` is not.
			TCreature *Master = get_creature(MasterID);
			if(Master != NULL){
				Responsible = Master;
			}
		}

		Attacker->BlockLogout(60, this->Type == PLAYER);
		if(Responsible != Attacker){
			Responsible->BlockLogout(60, this->Type == PLAYER);
		}

		if(this->Type == PLAYER && Responsible->Type == PLAYER){
			((TPlayer*)Responsible)->RecordAttack(this->ID);
		}
	}

	if(this->Type == PLAYER){
		if(check_right(this->ID, INVULNERABLE)){
			Damage = 0;
		}

		for(int Position = INVENTORY_FIRST;
				Position <= INVENTORY_LAST && Damage > 0;
				Position += 1){
			Object Obj = get_body_object(this->ID, Position);
			if(Obj == NONE){
				continue;
			}

			ObjectType ObjType = Obj.get_object_type();
			if(ObjType.get_flag(PROTECTION) && ObjType.get_flag(CLOTHES)
					&& (int)ObjType.get_attribute(BODYPOSITION) == Position
					&& (ObjType.get_attribute(PROTECTIONDAMAGETYPES) & DamageType) != 0){
				int DamageReduction = ObjType.get_attribute(DAMAGEREDUCTION);
				Damage = (Damage * (100 - DamageReduction)) / 100;
				if(ObjType.get_flag(WEAROUT)){
					// TODO(fusion): Ugh... The try..catch block might be used only
					// when changing the object's type.
					try{
						uint32 RemainingUses = Obj.get_attribute(REMAININGUSES);
						if(RemainingUses > 1){
							change(Obj, REMAININGUSES, RemainingUses - 1);
						}else{
							ObjectType WearOutType = (int)ObjType.get_attribute(WEAROUTTARGET);
							change(Obj, WearOutType, 0);
						}
					}catch(RESULT r){
						error("TCreature::Damage: Exception %d when wearing out object %d.\n",
								r, ObjType.TypeID);
					}
				}
			}else if(ObjType.get_flag(PROTECTION) && !ObjType.get_flag(CLOTHES)){
				error("TCreature::Damage: Object %d has PROTECTION, but not CLOTHES.\n",
						ObjType.TypeID);
			}
		}
	}

	if(Damage <= 0){
		graphical_effect(this->CrObject, EFFECT_POFF);
		return 0;
	}

	if(DamageType == DAMAGE_POISON_PERIODIC){
		if(race_data[this->Race].NoPoison){
			return 0;
		}

		if(Damage > this->Skills[SKILL_POISON]->TimerValue()){
			this->PoisonDamageOrigin = AttackerID;
			this->SetTimer(SKILL_POISON, Damage, 3, 3, -1);
		}

		this->DamageStimulus(AttackerID, Damage, DamageType);
		return Damage;
	}else if(DamageType == DAMAGE_FIRE_PERIODIC){
		if(race_data[this->Race].NoBurning){
			return 0;
		}

		this->FireDamageOrigin = AttackerID;
		this->SetTimer(SKILL_BURNING, Damage / 10, 8, 8, -1);
		this->DamageStimulus(AttackerID, Damage, DamageType);
		return Damage;
	}else if(DamageType == DAMAGE_ENERGY_PERIODIC){
		if(race_data[this->Race].NoEnergy){
			return 0;
		}

		// TODO(fusion): Shouldn't we use `Damage / 25` here?
		this->EnergyDamageOrigin = AttackerID;
		this->SetTimer(SKILL_ENERGY, Damage / 20, 10, 10, -1);
		this->DamageStimulus(AttackerID, Damage, DamageType);
		return Damage;
	}

	if((DamageType == DAMAGE_PHYSICAL && race_data[this->Race].NoHit)
	|| (DamageType == DAMAGE_POISON && race_data[this->Race].NoPoison)
	|| (DamageType == DAMAGE_FIRE && race_data[this->Race].NoBurning)
	|| (DamageType == DAMAGE_ENERGY && race_data[this->Race].NoEnergy)
	|| (DamageType == DAMAGE_LIFEDRAIN && race_data[this->Race].NoLifeDrain)){
		graphical_effect(this->CrObject, EFFECT_BLOCK_HIT);
		return 0;
	}

	if(DamageType == DAMAGE_PHYSICAL){
		Damage -= this->Combat.GetArmorStrength();
		if(Damage <= 0){
			graphical_effect(this->CrObject, EFFECT_BLOCK_HIT);
			return 0;
		}
	}

	this->DamageStimulus(AttackerID, Damage, DamageType);

	// NOTE(fusion): Remove non-player invisibility. Might as well be an inlined
	// function.
	if(this->Type != PLAYER && this->IsInvisible()){
		this->SetTimer(SKILL_ILLUSION, 0, 0, 0, -1);
		this->Outfit = this->OrgOutfit;
		announce_changed_creature(this->ID, CREATURE_OUTFIT_CHANGED);
		notify_all_creatures(this->CrObject, OBJECT_CHANGED, NONE);
	}

	if(DamageType == DAMAGE_MANADRAIN){
		int ManaPoints = this->Skills[SKILL_MANA]->Get();
		if(Damage > ManaPoints){
			Damage = ManaPoints;
		}

		if(Damage > 0){
			this->Skills[SKILL_MANA]->Change(-Damage);
			if(this->Type == PLAYER && this->Connection != NULL){
				SendMessage(this->Connection, TALK_STATUS_MESSAGE,
						"You lose %d mana.", Damage);
			}
			graphical_effect(this->CrObject, EFFECT_MAGIC_RED);
			textual_effect(this->CrObject, COLOR_BLUE, "%d", Damage);
		}

		return Damage;
	}

	if(this->Skills[SKILL_MANASHIELD]->TimerValue() > 0
			|| this->Skills[SKILL_MANASHIELD]->Get() > 0){
		// NOTE(fusion): We only send these if the attack was fully absorbed,
		// else it'd be overwritten by whatever effect and messages we send
		// next, when the victim's hitpoints are actually touched.
		int ManaPoints = this->Skills[SKILL_MANA]->Get();
		if(Damage <= ManaPoints){
			this->Skills[SKILL_MANA]->Change(-Damage);
			graphical_effect(this->CrObject, EFFECT_MANA_HIT);
			textual_effect(this->CrObject, COLOR_BLUE, "%d", Damage);
			if(this->Type == PLAYER && this->Connection != NULL){
				if(Attacker != NULL){
					SendMessage(this->Connection, TALK_STATUS_MESSAGE,
							"You lose %d mana blocking an attack by %s.",
							Damage, Attacker->Name);
				}else{
					SendMessage(this->Connection, TALK_STATUS_MESSAGE,
							"You lose %d mana.", Damage);
				}
				SendPlayerData(this->Connection);
			}
			return Damage;
		}

		this->Skills[SKILL_MANA]->Set(0);
		Damage -= ManaPoints;
	}

	int HitPoints = this->Skills[SKILL_HITPOINTS]->Get();
	if(Damage > HitPoints){
		Damage = HitPoints;
	}

	this->Skills[SKILL_HITPOINTS]->Change(-Damage);
	if(Attacker != NULL){
		ASSERT(Responsible != NULL);
		if(Responsible == Attacker){
			this->Combat.AddDamageToCombatList(Attacker->ID, Damage);
		}else{
			this->Combat.AddDamageToCombatList(Attacker->ID, Damage / 2);
			this->Combat.AddDamageToCombatList(Responsible->ID, Damage / 2);
		}
	}

	int HitEffect = EFFECT_NONE;
	int TextColor = COLOR_BLACK;
	int SplashLiquid = LIQUID_NONE;
	if(DamageType == DAMAGE_PHYSICAL){
		switch(race_data[this->Race].Blood){
			case BT_BLOOD:{
				HitEffect = EFFECT_BLOOD_HIT;
				TextColor = COLOR_RED;
				SplashLiquid = LIQUID_BLOOD;
				break;
			}
			case BT_SLIME:{
				HitEffect = EFFECT_POISON_HIT;
				TextColor = COLOR_LIGHTGREEN;
				SplashLiquid = LIQUID_SLIME;
				break;
			}
			case BT_BONES:{
				HitEffect = EFFECT_BONE_HIT;
				TextColor = COLOR_LIGHTGRAY;
				break;
			}
			case BT_FIRE:{
				HitEffect = EFFECT_BLOOD_HIT;
				TextColor = COLOR_ORANGE;
				break;
			}
			case BT_ENERGY:{
				HitEffect = EFFECT_ENERGY_HIT;
				TextColor = COLOR_LIGHTBLUE;
				break;
			}
			default:{
				error("TCreature::Damage: Invalid blood type %d for race %d.\n",
						race_data[this->Race].Blood, this->Race);
				break;
			}
		}
	}else if(DamageType == DAMAGE_POISON){
		HitEffect = EFFECT_POISON;
		TextColor = COLOR_LIGHTGREEN;
	}else if(DamageType == DAMAGE_FIRE){
		HitEffect = EFFECT_FIRE;
		TextColor = COLOR_ORANGE;
	}else if(DamageType == DAMAGE_ENERGY){
		HitEffect = EFFECT_ENERGY_HIT;
		TextColor = COLOR_LIGHTBLUE;
	}else if(DamageType == DAMAGE_LIFEDRAIN){
		HitEffect = EFFECT_MAGIC_RED;
		TextColor = COLOR_RED;
	}else{
		// TODO(fusion): The original decompiled function would return here but
		// I don't think it's a good idea because it would skip death handling.
		error("TCreature::Damage: Invalid damage type %d.\n", DamageType);
		//return Damage;
	}

	if(HitEffect != EFFECT_NONE){
		graphical_effect(this->CrObject, HitEffect);
		textual_effect(this->CrObject, TextColor, "%d", Damage);
		if(SplashLiquid != LIQUID_NONE){
			try{
				create_pool(get_map_container(this->CrObject),
							get_special_object(BLOOD_SPLASH),
							SplashLiquid);
			}catch(RESULT r){
				// TODO(fusion): Ignore?
			}
		}
	}

	if(this->Type == PLAYER && this->Connection != NULL){
		if(Attacker != NULL){
			SendMessage(this->Connection, TALK_STATUS_MESSAGE,
					"You lose %d hitpoint%s due to an attack by %s.",
					Damage, (Damage != 1 ? "s" : ""), Attacker->Name);
		}else{
			SendMessage(this->Connection, TALK_STATUS_MESSAGE,
					"You lose %d hitpoint%s.",
					Damage, (Damage != 1 ? "s" : ""));
		}
		SendPlayerData(this->Connection);
	}

	if(Damage == HitPoints){
		if(this->Type == PLAYER){
			for(int Position = INVENTORY_FIRST;
					Position <= INVENTORY_LAST;
					Position += 1){
				Object Obj = get_body_object(this->ID, Position);
				if(Obj == NONE){
					continue;
				}

				ObjectType ObjType = Obj.get_object_type();
				if(ObjType.get_flag(CLOTHES) && (int)ObjType.get_attribute(BODYPOSITION) == Position){
					ObjectType AmuletOfLossType = get_new_object_type(77, 12);
					if(ObjType == AmuletOfLossType){
						log_message("game", "%s dies with Amulet of Loss.\n", this->Name);
						this->LoseInventory = LOSE_INVENTORY_NONE;
						try{
							delete_op(Obj, -1);
							// TODO(fusion): Shouldn't we break here? We could also
							// just check if there is an amulet of loss in the necklace
							// container instead of iterating over all inventory.
						}catch(RESULT r){
							error("TCreature::Damage: Exception %d when deleting object %d.\n",
									r, AmuletOfLossType.TypeID);
						}
					}
				}
			}
		}

		int OldLevel = this->Skills[SKILL_LEVEL]->Get();
		this->Death();
		if(Attacker != NULL && this->Type == PLAYER){
			Attacker->BlockLogout(900, true);
			if(Responsible != Attacker){
				Responsible->BlockLogout(900, true);
			}
		}

		uint32 MurdererID = 0;
		char Remark[30] = {};
		if(Attacker == NULL){
			add_kill_statistics(0, this->Race);
			if(DamageType == DAMAGE_PHYSICAL){
				strcpy(Remark, "a hit");
			}else if(DamageType == DAMAGE_POISON){
				strcpy(Remark, "poison");
			}else if(DamageType == DAMAGE_FIRE){
				strcpy(Remark, "fire");
			}else if(DamageType == DAMAGE_ENERGY){
				strcpy(Remark, "energy");
			}else{
				// NOTE(fusion): We probably don't expect any other damage type
				// as the cause of death when there is no attacker.
				error("TCreature::Damage: Invalid damage type %d as cause of death.\n", DamageType);
			}
		}else{
			add_kill_statistics(Attacker->Race, this->Race);
			strcpy(this->Murderer, Attacker->Name);

			if(Responsible->Type == PLAYER){
				MurdererID = Responsible->ID;
			}

			if(Attacker->Type != PLAYER){
				strcpy(Remark, Attacker->Name);
			}
		}

		if(this->Type == PLAYER){
			((TPlayer*)this)->RecordDeath(MurdererID, OldLevel, Remark);

			uint32 MostDangerousID = this->Combat.GetMostDangerousAttacker();
			if(MostDangerousID != 0
					&& MostDangerousID != MurdererID
					&& is_creature_player(MostDangerousID)){
				// TODO(fusion): The original function is confusing at this point
				// but it seems correct that the remark is included only with the
				// murderer.
				((TPlayer*)this)->RecordDeath(MostDangerousID, OldLevel, "");
			}
		}
	}

	announce_changed_creature(this->ID, CREATURE_HEALTH_CHANGED);
	return Damage;
}

void TCreature::Death(void){
	this->IsDead = true;
	this->LoggingOut = true;
}

bool TCreature::MovePossible(int x, int y, int z, bool Execute, bool Jump){
	bool Result;

	if(Jump){
		Result = jump_possible(x, y, z, false);
	}else{
		Result = coordinate_flag(x, y, z, BANK)
			&& !coordinate_flag(x, y, z, UNPASS);
	}

	if(Result && !Execute && coordinate_flag(x, y, z, AVOID)){
		Result = false;
	}

	return Result;
}

bool TCreature::IsPeaceful(void){
	return true;
}

uint32 TCreature::GetMaster(void){
	return 0;
}

void TCreature::TalkStimulus(uint32 SpeakerID, const char *Text){
	// no-op
}

void TCreature::DamageStimulus(uint32 AttackerID, int Damage, int DamageType){
	// no-op
}

void TCreature::IdleStimulus(void){
	// no-op
}

void TCreature::CreatureMoveStimulus(uint32 CreatureID, int Type){
	if(CreatureID == 0 || CreatureID == this->ID
			|| this->IsDead
			|| this->Combat.AttackDest != CreatureID
			|| this->Combat.ChaseMode != CHASE_MODE_CLOSE
			|| this->Combat.EarliestAttackTime <= (ServerMilliseconds + 200)){
		return;
	}

	if(Type != OBJECT_CHANGED
			|| !this->LockToDo
			|| this->ActToDo >= this->NrToDo
			|| this->ToDoList.at(this->ActToDo)->Code != TDAttack){
		return;
	}

	TCreature *Target = get_creature(this->Combat.AttackDest);
	if(Target == NULL){
		return;
	}

	int Distance = object_distance(this->CrObject, Target->CrObject);
	if(Distance <= 1){
		return;
	}

	// TODO(fusion): Review this.
	try{
		if(this->ToDoClear() && this->Type == PLAYER){
			SendSnapback(this->Connection);
		}
		this->ToDoWait(200);
		this->ToDoAttack();
		this->ToDoStart();
	}catch(RESULT r){
		if(this->Type == PLAYER){
			SendResult(this->Connection, r);
		}
		this->ToDoClear();
		this->ToDoWaitUntil(this->Combat.EarliestAttackTime);
		this->ToDoStart();
	}
}

void TCreature::AttackStimulus(uint32 AttackerID){
	// no-op
}

// Creature Management
// =============================================================================
bool is_creature_player(uint32 CreatureID){
	return CreatureID < 0x40000000;
}
