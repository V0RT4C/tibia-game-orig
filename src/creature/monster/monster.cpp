#include "cr.h"
#include "config.h"
#include "containers.h"
#include "info.h"
#include "magic.h"
#include "operate.h"
#include "writer.h"

static vector<TMonsterhome> Monsterhome(1, 5000, 1000);
static int Monsterhomes;

// Monster Homes
// =============================================================================
void StartMonsterhomeTimer(int Nr){
	if(Nr < 1 || Nr > Monsterhomes){
		error("StartMonsterhomeTimer: Invalid monsterhome number %d.\n", Nr);
		return;
	}

	TMonsterhome *MH = Monsterhome.at(Nr);
	if(MH->Timer > 0){
		error("StartMonsterhomeTimer: Timer is already running.\n");
		return;
	}

	if(MH->ActMonsters >= MH->MaxMonsters){
		error("StartMonsterhomeTimer: Maximum monster count already reached.\n");
		error("# Monsterhome with race %d at [%d,%d,%d]\n", MH->Race, MH->x, MH->y, MH->z);
		return;
	}

	int MaxTimer = MH->RegenerationTime;
	int NumPlayers = GetNumberOfPlayers();
	if(NumPlayers > 800){
		MaxTimer = (MaxTimer * 2) / 5;
	}else if(NumPlayers > 200){
		MaxTimer = (MaxTimer * 200) / ((NumPlayers / 2) + 100);
	}

	MH->Timer = random(MaxTimer / 2, MaxTimer);
}

void LoadMonsterhomes(void){
	print(1, "Initializing Monsterhomes ...\n");

	char FileName[4096];
	snprintf(FileName, sizeof(FileName), "%s/monster.db", DATAPATH);

	ReadScriptFile Script;
	Script.open(FileName);

	Monsterhomes = 0;
	while(true){
		int Race = Script.read_number();
		if(Race == 0){
			break;
		}

		Monsterhomes += 1;
		TMonsterhome *MH = Monsterhome.at(Monsterhomes);
		MH->Race = Race;
		MH->x = Script.read_number();
		MH->y = Script.read_number();
		MH->z = Script.read_number();
		MH->Radius = Script.read_number();
		MH->MaxMonsters = Script.read_number();
		MH->ActMonsters = 0;
		MH->RegenerationTime = Script.read_number();
		MH->Timer = 0;

		if(!is_on_map(MH->x, MH->y, MH->z)){
			print(1, "WARNING: Monsterhome [%d,%d,%d] is outside the map.\n", MH->x, MH->y, MH->z);
		}
	}

	print(1, "%d Monsterhomes loaded.\n", Monsterhomes);
	Script.close();

	for(int i = 0; i < Monsterhomes; i += 1){
		TMonsterhome *MH = Monsterhome.at(i);
		for (int j = 0; j < MH->MaxMonsters; j += 1){
			int SpawnX = MH->x;
			int SpawnY = MH->y;
			int SpawnZ = MH->z;
			int SpawnRadius = MH->Radius;

			if(j == 0){
				if(SpawnRadius > 1){
					SpawnRadius = 1;
				}
			}else{
				if(SpawnRadius > 10){
					SpawnRadius = 10;
				}

				// NOTE(fusion): `search_spawn_field` performs an extended search
				// if the radius is negative.
				SpawnRadius = -SpawnRadius;
			}

			if(search_spawn_field(&SpawnX, &SpawnY, &SpawnZ, SpawnRadius, false)){
				CreateMonster(MH->Race, SpawnX, SpawnY, SpawnZ, i, 0, false);
				MH->ActMonsters += 1;
			}
		}

		if(MH->Timer > 0){
			error("LoadMonsterhomes: Timer is already running (race %d at [%d,%d,%d]).\n",
					MH->Race, MH->x, MH->y, MH->z);
		}else if(MH->ActMonsters < MH->MaxMonsters){
			StartMonsterhomeTimer(i);
		}
	}
}

void ProcessMonsterhomes(void){
	for(int i = 1; i <= Monsterhomes; i += 1){
		TMonsterhome *MH = Monsterhome.at(i);

		ASSERT(MH->Timer >= 0);
		if(MH->Timer == 0){
			continue;
		}

		MH->Timer -= 1;
		if(MH->Timer > 0){
			continue;
		}

		int MaxRadius = MH->Radius;
		if(MaxRadius > 10){
			MaxRadius = 10;
		}

		TFindCreatures Search(MaxRadius + 9, MaxRadius + 7, MH->x, MH->y, FIND_PLAYERS);
		while(true){
			uint32 CharacterID = Search.getNext();
			if(CharacterID == 0){
				break;
			}

			TPlayer *Player = GetPlayer(CharacterID);
			if(Player == NULL){
				error("ProcessMonsterhomes: Creature does not exist.\n");
				break;
			}

			if(!Player->CanSeeFloor(MH->z)){
				continue;
			}

			int DistanceX = std::abs(Player->posx - MH->x);
			int DistanceY = std::abs(Player->posy - MH->y);
			int Radius = std::max<int>(DistanceX - 9, DistanceY - 7);
			if(Radius < MaxRadius){
				MaxRadius = Radius;
			}
		}

		if(MaxRadius >= 0){
			int SpawnX = MH->x;
			int SpawnY = MH->y;
			int SpawnZ = MH->z;
			int SpawnRadius = MaxRadius;

			if(MH->ActMonsters == 0){
				if(SpawnRadius > 1){
					SpawnRadius = 1;
				}
			}else{
				// NOTE(fusion): `search_spawn_field` performs an extended search
				// if the radius is negative.
				SpawnRadius = -SpawnRadius;
			}

			if(search_spawn_field(&SpawnX, &SpawnY, &SpawnZ, SpawnRadius, false)){
				CreateMonster(MH->Race, SpawnX, SpawnY, SpawnZ, i, 0, false);
				MH->ActMonsters += 1;

				// TODO(fusion): Not sure why this check is here.
				if(MH->Timer > 0){
					error("ProcessMonsterhomes: Timer is already running (race %d at [%d,%d,%d]).\n",
							MH->Race, SpawnX, SpawnY, SpawnZ);
				}
			}
		}

		if(MH->ActMonsters < MH->MaxMonsters){
			StartMonsterhomeTimer(i);
		}
	}
}

void NotifyMonsterhomeOfDeath(int Nr){
	if(Nr < 1 || Nr > Monsterhomes){
		error("NotifyMonsterhomeOfDeath: Invalid monsterhome number %d.\n", Nr);
		return;
	}

	TMonsterhome *MH = Monsterhome.at(Nr);
	if(MH->ActMonsters < 1){
		error("NotifyMonsterhomeOfDeath: Monsterhome has no living creatures.\n");
		return;
	}

	MH->ActMonsters -= 1;
	if(MH->ActMonsters >= MH->MaxMonsters){
		error("NotifyMonsterhomeOfDeath: Monsterhome %d had too many monsters (%d instead of %d).\n",
				Nr, (MH->ActMonsters + 1), MH->MaxMonsters);
		return;
	}

	if(MH->Timer == 0){
		StartMonsterhomeTimer(Nr);
	}
}

bool MonsterhomeInRange(int Nr, int x, int y, int z){
	if(Nr == 0){
		return true;
	}

	if(Nr < 1 || Nr > Monsterhomes){
		error("MonsterhomeInRange: Invalid monsterhome number %d.\n", Nr);
		return false;
	}

	TMonsterhome *MH = Monsterhome.at(Nr);
	return std::abs(z - MH->z) <= 2
		&& std::abs(x - MH->x) <= MH->Radius
		&& std::abs(y - MH->y) <= MH->Radius;
}

// TMonster
// =============================================================================
TMonster::TMonster(int Race, int x, int y, int z, int Home, uint32 MasterID) :
		TNonplayer()
{
	this->Type = MONSTER;
	this->Race = Race;
	this->startx = x;
	this->starty = y;
	this->startz = z;
	this->posx = x;
	this->posx = y;
	this->posx = z;
	this->State = IDLE;
	this->Home = Home;
	this->Master = MasterID;
	this->Target = 0;

	while(this->Master != 0){
		TCreature *Master = GetCreature(this->Master);
		// TODO(fusion): Do we actually want to return here?
		if(Master == NULL){
			error("TMonster::TMonster: Master does not exist.\n");
			return;
		}

		if(Master->Type != MONSTER || ((TMonster*)Master)->Master == 0){
			this->LifeEndRound = Master->LifeEndRound;
			Master->SummonedCreatures += 1;
			break;
		}

		error("TMonster::TMonster: Children may not create their own children.\n");
		this->Master = ((TMonster*)Master)->Master;
	}

	if(RaceData[Race].Article[0] == 0){
		strcpy(this->Name, RaceData[Race].Name);
	}else{
		snprintf(this->Name, sizeof(this->Name), "%s %s",
				RaceData[Race].Article, RaceData[Race].Name);
	}

	this->SetSkills(Race);
	this->OrgOutfit = RaceData[Race].Outfit;
	this->Outfit = RaceData[Race].Outfit;
	this->SetID(0);
	this->SetInList();
	if(!this->SetOnMap()){
		error("TMonster::TMonster: Cannot place monster on the map.\n");
		return;
	}

	// TODO(fusion): This part is very similar to `ProcessMonsterRaids` when
	// adding extra items to spawned monsters.
	try{
		if(this->Master == 0 && RaceData[Race].Items > 0){
			Object Bag = create(get_body_container(this->ID, INVENTORY_BAG),
								get_special_object(DEFAULT_CONTAINER),
								0);
			for(int i = 1; i <= RaceData[Race].Items; i += 1){
				TItemData *ItemData = RaceData[Race].Item.at(i);
				if(random(0, 999) > ItemData->Probability){
					continue;
				}

				ObjectType ItemType = ItemData->Type;
				int Amount = random(1, ItemData->Maximum);
				int Repeat = 1;
				if(!ItemType.get_flag(CUMULATIVE)){
					Repeat = Amount;
					Amount = 0;
				}

				for(int j = 0; j < Repeat; j += 1){
					Object Item = NONE;
					try{
						if(ItemType.get_flag(WEAPON)
								|| ItemType.get_flag(SHIELD)
								|| ItemType.get_flag(BOW)
								|| ItemType.get_flag(THROW)
								|| ItemType.get_flag(WAND)
								|| ItemType.get_flag(WEAROUT)
								|| ItemType.get_flag(EXPIRE)
								|| ItemType.get_flag(EXPIRESTOP)){
							Item = create(Bag, ItemType, 0);
						}else{
							Item = create_at_creature(this->ID, ItemType, Amount);
						}
					}catch(RESULT r){
						error("TMonster::TMonster: Exception %d for race %d, consider"
								" increasing CarryStrength.\n", r, Race);
						break;
					}

					// NOTE(fusion): Prevent items from being dropped onto the map.
					if(Item.get_container().get_object_type().is_map_container()){
						error("TMonster::TMonster: Object falls on the map."
								" Increase CarryStrength for race %d.\n", Race);
						delete_op(Item, -1);
					}
				}
			}

			// NOTE(fusion): `Bag` could be empty if we failed to add any
			// items to it in the loop above.
			if(get_first_container_object(Bag) == NONE){
				delete_op(Bag, -1);
			}
		}
	}catch(RESULT r){
		error("TMonster::TMonster: Exception %d for race %d.\n", r, Race);
	}

	this->Combat.CheckCombatValues();
	this->Combat.SetAttackMode(ATTACK_MODE_BALANCED);
	this->ToDoYield();
}

TMonster::~TMonster(void){
	if(!this->IsDead){
		graphical_effect(this->posx, this->posy, this->posz, EFFECT_POFF);
	}else if(this->Master == 0){
		this->Combat.DistributeExperiencePoints(RaceData[this->Race].ExperiencePoints);
	}

	if(this->Master != 0){
		TCreature *Master = GetCreature(this->Master);
		if(Master != NULL && Master->SummonedCreatures > 0){
			Master->SummonedCreatures -= 1;
		}
	}

	if(this->Home != 0){
		NotifyMonsterhomeOfDeath(this->Home);
	}
}

bool TMonster::MovePossible(int x, int y, int z, bool Execute, bool Jump){
	if(this->posz != z){
		error("TMonster::MovePossible: Check across floors ([%d,%d,%d] -> [%d,%d,%d]).\n",
				this->posx, this->posy, this->posz, x, y, z);
		return false;
	}

	if(this->State != ATTACKING && this->State != PANIC){
		if(!MonsterhomeInRange(this->Home, x, y, z)){
			return false;
		}

		// NOTE(fusion): Max distance.
		int Distance = std::max<int>(
				std::abs(x - this->posx),
				std::abs(y - this->posy));
		if(Distance > this->Radius){
			return false;
		}
	}

	if(this->Skills[SKILL_GO_STRENGTH]->Act < 0){
		error("TMonster::MovePossible: Monster %s at [%d,%d,%d] is not allowed to move.\n",
				this->Name, this->posx, this->posy, this->posz);
		return false;
	}

	if(is_protection_zone(x, y, z) || is_house(x, y, z)){
		return false;
	}

	if(Execute && this->Master != 0 && this->State != ATTACKING && this->State != PANIC){
		TCreature *Master = GetCreature(this->Master);
		if(Master != NULL && Master->posz == this->posz){
			// NOTE(fusion): Manhattan distance.
			if((std::abs(Master->posx - this->posx) + std::abs(Master->posy - this->posy)) >  1
			&& (std::abs(Master->posx -          x) + std::abs(Master->posy -          y)) <= 1){
				return false;
			}
		}
	}

	// NOTE(fusion): Check destination and retry while we keep kicking blocking
	// objects away.
	for(int Attempt = 0; Attempt < 100; Attempt += 1){
		Object Obj = get_first_object(x, y, z);
		if(Obj == NONE || !Obj.get_object_type().get_flag(BANK)){
			return false;
		}

		while(Obj != NONE){
			ObjectType ObjType = Obj.get_object_type();
			if(ObjType.is_creature_container()){
				if(this->State != ATTACKING && this->State != PANIC){
					return false;
				}

				if(this->Target == 0){
					return false;
				}

				if(!RaceData[this->Race].KickCreatures){
					return false;
				}

				TCreature *Creature = GetCreature(Obj);
				if(Creature == NULL){
					error("TMonster::MovePossible: Cannot identify blocking creature.\n");
					return false;
				}

				if(Creature->ID == this->Target || Creature->ID == this->Master){
					return false;
				}

				if(RaceData[Creature->Race].Unpushable){
					return false;
				}

				// TODO(fusion): Why?
				if(!RaceData[this->Race].SeeInvisible && Creature->IsInvisible()){
					return false;
				}

				if(Creature->Type == NPC){
					return false;
				}

				if(Creature->Type == PLAYER){
					if(this->Master != 0 || check_right(Creature->ID, IGNORED_BY_MONSTERS)){
						return false;
					}
				}

				if(Execute){
					if(Creature->Type == PLAYER){
						this->Target = 0;
						throw EXHAUSTED;
					}

					if(!this->KickCreature(Creature)){
						throw EXHAUSTED;
					}

					// NOTE(fusion): Break from the inner loop and retry.
					break;
				}
			}else{
				if(ObjType.get_flag(UNPASS)){
					if(ObjType.get_flag(UNMOVE) || !this->CanKickBoxes()){
						return false;
					}

					if(Execute){
						this->KickBoxes(Obj);

						// NOTE(fusion): Break from the inner loop and retry.
						break;
					}
				}

				if(ObjType.get_flag(AVOID)){
					int AvoidDamageTypes = (int)ObjType.get_attribute(AVOIDDAMAGETYPES);
					bool IgnoreHazard = (this->State == PANIC && AvoidDamageTypes != 0)
						|| (RaceData[this->Race].NoPoison  && AvoidDamageTypes == DAMAGE_POISON)
						|| (RaceData[this->Race].NoBurning && AvoidDamageTypes == DAMAGE_FIRE)
						|| (RaceData[this->Race].NoEnergy  && AvoidDamageTypes == DAMAGE_ENERGY);
					if(!IgnoreHazard){
						if(ObjType.get_flag(UNMOVE) || !this->CanKickBoxes()){
							return false;
						}

						if(Execute){
							this->KickBoxes(Obj);

							// NOTE(fusion): Break from the inner loop and retry.
							break;
						}
					}
				}
			}
			Obj = Obj.get_next_object();
		}

		if(Obj == NONE){
			return true;
		}
	}

	error("TMonster::MovePossible: Infinite loop suspected for %s at [%d,%d,%d].\n",
			this->Name, x, y, z);
	return false;
}

bool TMonster::IsPeaceful(void){
	return this->Master != 0
		&& IsCreaturePlayer(this->Master);
}

uint32 TMonster::GetMaster(void){
	return this->Master;
}

void TMonster::DamageStimulus(uint32 AttackerID, int Damage, int DamageType){
	if(AttackerID != 0 && Damage != 0){
		if(this->State == SLEEPING){
			if(this->Target == 0){
				this->State = PANIC;
			}else{
				this->State = UNDERATTACK;
			}
			this->ToDoYield();
		}else{
			if(this->Target == 0){
				this->State = PANIC;
			}else if(this->State == IDLE){
				this->State = UNDERATTACK;
			}
		}
	}
}

void TMonster::IdleStimulus(void){
	if(this->LockToDo || this->LoggingOut){
		return;
	}

	if(this->LifeEndRound != 0 && this->LifeEndRound <= RoundNr){
		print(3, "Lifetime for %s has expired.\n", this->Name);
		this->StartLogout(true, true);
		this->State = SLEEPING;
		return;
	}

	if(this->Master != 0){
		TCreature *Master = GetCreature(this->Master);
		bool MasterIsPlayer = IsCreaturePlayer(this->Master);
		bool ShouldDespawn = false;
		if(Master == NULL){
			if(MasterIsPlayer){
				print(3, "Creature %s loses its player master.\n", this->Name);
			}else{
				print(3, "Creature %s loses its parent monster.\n", this->Name);
			}
			ShouldDespawn = true;
		}else if(MasterIsPlayer && Master->SummonedCreatures == 0){
			print(3, "Player master has just logged in again.\n");
			ShouldDespawn = true;
		}else if(!MasterIsPlayer && Master->posz != this->posz){
			print(3, "Parent monster is too far away.\n");
			ShouldDespawn = true;
		}else if(std::abs(Master->posz - this->posz) > 1
				|| std::abs(Master->posx - this->posx) > 30
				|| std::abs(Master->posy - this->posy) > 30){
			if(MasterIsPlayer){
				print(3, "Player master is too far away.\n");
			}else{
				print(3, "Parent monster is too far away.\n");
			}
			ShouldDespawn = true;
		}

		if(ShouldDespawn){
			if(MasterIsPlayer){
				this->StartLogout(true, true);
			}else{
				this->Kill();
			}
			this->State = SLEEPING;
			return;
		}

		if(Master->Combat.Following){
			this->Target = 0;
		}else{
			this->Target = Master->Combat.AttackDest;
		}

		if(this->Target == 0 || this->Target == this->ID){
			this->Target = this->Master;
		}

	}else{
		if(!MonsterhomeInRange(this->Home, this->posx, this->posy, this->posz)){
			TMonsterhome *MH = Monsterhome.at(this->Home);
			print(3, "%s at [%d,%d,%d] too far from [%d,%d,%d].\n",
					this->Name, this->posx, this->posy, this->posz, MH->x, MH->y, MH->z);
			this->StartLogout(true, true);
			this->State = SLEEPING;
			return;
		}
	}

	if(this->Target != 0){
		TCreature *Target = GetCreature(this->Target);
		bool LoseTarget = (Target == NULL)
			// RANGE CHECK
			|| Target->posz != this->posz
			|| std::abs(Target->posx - this->posx) > 10
			|| std::abs(Target->posy - this->posy) > 10
			// FIELD CHECK
			|| is_protection_zone(Target->posx, Target->posy, Target->posz)
			|| is_house(Target->posx, Target->posy, Target->posz)
			// INVISIBILITY CHECK
			|| (Target->IsInvisible() && !RaceData[this->Race].SeeInvisible)
			// LOSETARGET CHECK
			|| (this->Master == 0 && random(0, 99) < RaceData[this->Race].LoseTarget);
		if(LoseTarget){
			this->Target = 0;
		}
	}

	if(this->State != PANIC && this->State != UNDERATTACK){
		this->State = IDLE;
	}

	// NOTE(fusion): TALKING.
	if(RaceData[this->Race].Talks > 0 && (rand() % 50) == 0){
		int TalkNr = random(1, RaceData[this->Race].Talks);
		const char *Text = GetDynamicString(*RaceData[this->Race].Talk.at(TalkNr));
		if(Text != 0 && Text[0] != 0){
			// TODO(fusion): We were only checking for a `#` but it could be
			// problematic for any two character nul terminated string, since
			// adding 3 would make it point out of bounds. Poor string management
			// is a recurring theme.
			int Mode = TALK_ANIMAL_LOW;
			if(Text[0] == '#' && (Text[1] == 'y' || Text[1] == 'Y') && Text[2] == ' '){
				Mode = TALK_ANIMAL_LOUD;
				Text += 3;
			}

			try{
				talk(this->ID, Mode, NULL, Text, false);
			}catch(RESULT r){
				error("TMonster::IdleStimulus: Exception %d during Talk.\n", r);
			}
		}else{
			// TODO(fusion): The original wouldn't check if `Text` is actually
			// valid which could also be a problem.
			error("TMonster::IdleStimulus: Race %d talk entry %d is invalid.",
					this->Race, TalkNr);
		}
	}

	// NOTE(fusion): TARGETING.
	if(this->Target == 0){
		bool ShouldSleep = true;
		if(this->Master == 0){
			// IMPORTANT(fusion): We don't iterate over the last strategy on purpose.
			int Strategy = 0;
			int StrategyRoll = random(0, 99);
			while(Strategy < (NARRAY(TRaceData::Strategy) - 1)){
				if(StrategyRoll < RaceData[this->Race].Strategy[Strategy]){
					break;
				}
				StrategyRoll -= RaceData[this->Race].Strategy[Strategy];
				Strategy += 1;
			}

			int BestStrategyParam = INT_MIN;
			int BestTieBreaker = 0;
			TFindCreatures Search(12, 12, this->ID, FIND_PLAYERS | FIND_MONSTERS);
			while(true){
				uint32 TargetID = Search.getNext();
				if(TargetID == 0){
					break;
				}

				TCreature *Target = GetCreature(TargetID);
				if(Target == NULL){
					error("TMonster::IdleStimulus: Creature does not exist.\n");
					continue;
				}

				if(Target->Type == MONSTER && !((TMonster*)Target)->IsPlayerControlled()){
					continue;
				}

				if(Target->CanSeeFloor(this->posz)){
					ShouldSleep = false;
				}

				// TODO(fusion): This is quite similar to the conditions for losing
				// the target. Perhaps there is some common inlined function.
				int DistanceX = std::abs(Target->posx - this->posx);
				int DistanceY = std::abs(Target->posy - this->posy);
				if((Target->posz != this->posz || DistanceX > 10 || DistanceY > 10)
						||(Target->Type == PLAYER && check_right(Target->ID, IGNORED_BY_MONSTERS))
						|| (Target->IsInvisible() && !RaceData[this->Race].SeeInvisible)
						|| is_protection_zone(Target->posx, Target->posy, Target->posz)
						|| is_house(Target->posx, Target->posy, Target->posz)
						|| std::abs(Target->posx - this->posx) > 10
						|| std::abs(Target->posy - this->posy) > 10){
					continue;
				}

				int StrategyParam = 0;
				if(Strategy == 0){ // STRATEGY_NEAREST
					StrategyParam = -(DistanceX + DistanceY);
				}else if(Strategy == 1){ // STRATEGY_LOWEST_HEALTH
					StrategyParam = -Target->Skills[SKILL_HITPOINTS]->Get();
				}else if(Strategy == 2){ // STRATEGY_MOST_DAMAGE
					StrategyParam = this->Combat.GetDamageByCreature(TargetID);
				}else if(Strategy == 3){ // STRATEGY_RANDOM
					// no-op
				}else{
					error("TMonster::IdleStimulus: Unknown strategy %d.\n", Strategy);
				}

				// NOTE(fusion): We're looking for the creature that maximizes
				// the strategy parameter.
				int TieBreaker = random(0, 99);
				if(StrategyParam >= BestStrategyParam
				||(StrategyParam == BestStrategyParam && TieBreaker > BestTieBreaker)){
					this->Target = TargetID;
					BestTieBreaker = TieBreaker;
					BestStrategyParam = StrategyParam;
				}
			}
		}

		if(ShouldSleep && this->Target == 0
				&& this->State != UNDERATTACK
				&& this->State != PANIC){
			if(this->Master == 0){
				this->State = SLEEPING;
			}else{
				this->ToDoWait(1000);
				this->ToDoStart();
			}
			return;
		}

		if(this->State == PANIC && this->Target == 0){
			this->State = IDLE;
		}

		if(this->State == UNDERATTACK){
			this->State = IDLE;
		}
	}

	// NOTE(fusion): CASTING.
	// TODO(fusion): It is highly unlikely but we could cast all spells at once.
	// I'm not sure this is correct but I can see it happening.
	if(RaceData[this->Race].Spells > 0){
		TCreature *Target = GetCreature(this->Target);
		for(int SpellNr = 1;
				SpellNr <= RaceData[this->Race].Spells;
				SpellNr += 1){
			TSpellData *SpellData = RaceData[this->Race].Spell.at(SpellNr);
			if((rand() % SpellData->Delay) != 0){
				continue;
			}

			if(this->IsFleeing() && random(1, 3) != 1){
				continue;
			}

			Impact *Impact = NULL;
			switch(SpellData->Impact){
				case IMPACT_DAMAGE:{
					int DamageType = SpellData->ImpactParam1;
					int Damage = SpellData->ImpactParam2;
					int Variation = SpellData->ImpactParam3;
					Damage = compute_damage(this, 0, Damage, Variation);
					Impact = new DamageImpact(this, DamageType, Damage, true);
					break;
				}

				case IMPACT_FIELD:{
					int FieldType = SpellData->ImpactParam1;
					Impact = new FieldImpact(this, FieldType);
					break;
				}

				case IMPACT_HEALING:{
					int Amount = SpellData->ImpactParam1;
					int Variation = SpellData->ImpactParam2;
					Amount = compute_damage(this, 0, Amount, Variation);
					Impact = new HealingImpact(this, Amount);
					break;
				}

				case IMPACT_SPEED:{
					int Percent = SpellData->ImpactParam1;
					int Variation = SpellData->ImpactParam2;
					int Duration = SpellData->ImpactParam3;
					Percent = compute_damage(this, 0, Percent, Variation);
					Impact = new SpeedImpact(this, Percent, Duration);
					break;
				}

				case IMPACT_DRUNKEN:{
					int Strength = SpellData->ImpactParam1;
					int Variation = SpellData->ImpactParam2;
					int Duration = SpellData->ImpactParam3;
					Strength = compute_damage(this, 0, Strength, Variation);
					Impact = new DrunkenImpact(this, Strength, Duration);
					break;
				}

				case IMPACT_STRENGTH:{
					int Skills = SpellData->ImpactParam1;
					int Percent = SpellData->ImpactParam2;
					int Variation = SpellData->ImpactParam3;
					int Duration = SpellData->ImpactParam4;
					Percent = compute_damage(this, 0, Percent, Variation);
					Impact = new StrengthImpact(this, Skills, Percent, Duration);
					break;
				}

				case IMPACT_OUTFIT:{
					TOutfit Outfit = {};
					Outfit.OutfitID = SpellData->ImpactParam1;
					Outfit.ObjectType = SpellData->ImpactParam2;
					int Duration = SpellData->ImpactParam3;
					Impact = new OutfitImpact(this, Outfit, Duration);
					break;
				}

				case IMPACT_SUMMON:{
					if(this->Master == 0){
						int SummonRace = SpellData->ImpactParam1;
						int MaxSummons = SpellData->ImpactParam2;
						Impact = new SummonImpact(this, SummonRace, MaxSummons);
					}
					break;
				}
			}

			if(Impact != NULL){
				if(!Impact->isAggressive() || (this->Target != 0 && this->Target != this->Master)){
					switch(SpellData->Shape){
						case SHAPE_ACTOR:{
							int Effect = SpellData->ShapeParam1;
							actor_shape_spell(this, Impact, Effect);
							break;
						}

						case SHAPE_VICTIM:{
							if(Target != NULL){
								this->Rotate(Target);

								int Range = SpellData->ShapeParam1;
								int Animation = SpellData->ShapeParam2;
								int Effect = SpellData->ShapeParam3;
								victim_shape_spell(this, Target, Range,
										Animation, Impact, Effect);
							}
							break;
						}

						case SHAPE_ORIGIN:{
							int Radius = SpellData->ShapeParam1;
							int Effect = SpellData->ShapeParam2;
							origin_shape_spell(this, Radius, Impact, Effect);
							break;
						}

						case SHAPE_DESTINATION:{
							if(Target != NULL){
								this->Rotate(Target);

								int Range = SpellData->ShapeParam1;
								int Animation = SpellData->ShapeParam2;
								int Radius = SpellData->ShapeParam3;
								int Effect  = SpellData->ShapeParam4;
								destination_shape_spell(this, Target, Range,
										Animation, Radius, Impact, Effect);
							}
							break;
						}

						case SHAPE_ANGLE:{
							if(Target != NULL){
								this->Rotate(Target);
							}

							int Angle = SpellData->ShapeParam1;
							int Range = SpellData->ShapeParam2;
							int Effect = SpellData->ShapeParam3;
							angle_shape_spell(this, Angle, Range, Impact, Effect);
							break;
						}
					}
				}

				delete Impact;
			}
		}
	}

	// NOTE(fusion): WALKING. What was already bad got even worse.
	TCreature *Target = GetCreature(this->Target);
	if(this->Target != 0 && Target == NULL){
		error("TMonster::IdleStimulus: Creature does not exist.\n");
		this->Target = 0;
	}

	try{
		if(Target != NULL){
			if(this->IsFleeing()){
				int DestX, DestY, DestZ;
				if(search_flight_field(this->ID, this->Target, &DestX, &DestY, &DestZ)){
					this->ToDoGo(DestX, DestY, DestZ, true, INT_MAX);
					this->ToDoStart();
					return;
				}
			}else if(this->Target == this->Master){
				// NOTE(fusion): Manhattan distance.
				int DistanceX = std::abs(Target->posx - this->posx);
				int DistanceY = std::abs(Target->posy - this->posy);
				int Distance = DistanceX + DistanceY;
				if(Distance > 1){
					if(Distance == 2){
						this->ToDoWait(1000);
					}else{
						if(Distance == 3){
							this->ToDoWait(1000);
						}
						this->ToDoGo(Target->posx, Target->posy, Target->posz, false, 3);
					}
					this->ToDoStart();
					return;
				}
			}else{
				if(this->Skills[SKILL_FIST]->Get() > 0 && this->State != PANIC){
					this->State = ATTACKING;
				}

				if(this->State == ATTACKING || this->State == PANIC){
					this->Combat.SetAttackDest(this->Target, false);
					this->Combat.SetChaseMode(CHASE_MODE_NONE);
				}

				// TODO(fusion): We're doing something similar to `TCombat::CanToDoAttack`
				// with added random steps around the attacking position. This function is
				// too convoluted and we should definitely split it into smaller ones. I
				// don't have a problem with large functions but this it too much.

				// NOTE(fusion): Max distance.
				int Distance = std::max<int>(
						std::abs(Target->posx - this->posx),
						std::abs(Target->posy - this->posy));
				if(!RaceData[this->Race].DistanceFighting
						|| !throw_possible(this->posx, this->posy, this->posz,
								Target->posx, Target->posy, Target->posz, 0)){
					this->Combat.SetChaseMode(CHASE_MODE_CLOSE);
					if(Distance > 1){
						// TODO(fusion): Not sure what is going on here. I think
						// it's because `ToDoAttack` with `CHASE_MODE_CLOSE` will
						// already take care of walking to the target?
						if(this->State != ATTACKING && this->State != PANIC){
							this->ToDoGo(Target->posx, Target->posy, Target->posz, false, 3);
						}
					}else{
						int DestX = this->posx;
						int DestY = this->posy;
						int DestZ = this->posz;
						switch(rand() % 5){
							case 0: DestX -= 1; break;
							case 1: DestX += 1; break;
							case 2: DestY -= 1; break;
							case 3: DestY += 1; break;
							case 4: break; // no step
						}

						int DestDistance = std::max<int>(
								std::abs(Target->posx - DestX),
								std::abs(Target->posy - DestY));
						if(DestDistance == 1 && this->MovePossible(DestX, DestY, DestZ, true, false)){
							this->ToDoGo(DestX, DestY, DestZ, true, INT_MAX);
						}

						if(this->State == PANIC){
							this->State = ATTACKING;
						}
					}
				}else{
					if(Distance < 4){
						int DestX, DestY, DestZ;
						if(search_flight_field(this->ID, this->Target, &DestX, &DestY, &DestZ)){
							this->ToDoGo(DestX, DestY, DestZ, true, INT_MAX);
						}else{
							this->ToDoWait(1000);
						}
					}else if(Distance > 4){
						this->ToDoGo(Target->posx, Target->posy, Target->posz, false, Distance - 4);
					}else{
						int DestX = this->posx;
						int DestY = this->posy;
						int DestZ = this->posz;
						switch(rand() % 5){
							case 0: DestX -= 1; break;
							case 1: DestX += 1; break;
							case 2: DestY -= 1; break;
							case 3: DestY += 1; break;
							case 4: break; // no step
						}

						int DestDistance = std::max<int>(
								std::abs(Target->posx - DestX),
								std::abs(Target->posy - DestY));
						if(DestDistance == 4 && this->MovePossible(DestX, DestY, DestZ, true, false)){
							this->ToDoGo(DestX, DestY, DestZ, true, INT_MAX);
						}

						this->ToDoWait(1000);
					}
				}

				if(this->State == ATTACKING || this->State == PANIC){
					this->Rotate(Target);
					if(this->Master != this->Target){
						this->ToDoAttack();
					}else{
						error("TMonster::IdleStimulus: %s attacks master %u (St=%d, T: %u).\n",
								this->Name, this->Master, this->State, this->Target);
					}
				}else{
					this->ToDoWait(1000);
				}

				this->ToDoStart();
				return;
			}
		}
	}catch(RESULT r){
		this->Target = 0;
		this->ToDoClear();
		if(r == EXHAUSTED){
			this->ToDoWait(1000);
			this->ToDoStart();
			return;
		}
	}

	// TODO(fusion): This part is similar to `TNPC::IdleStimulus`. Perhaps there
	// is a common function in `TNonplayer` that does this?
	bool FoundDest = false;
	int DestX, DestY, DestZ;
	for(int i = 0; i < 10; i += 1){
		DestX = this->posx;
		DestY = this->posy;
		DestZ = this->posz;
		switch(rand() % 4){
			case 0: DestX -= 1; break;
			case 1: DestX += 1; break;
			case 2: DestY -= 1; break;
			case 3: DestY += 1; break;
		}

		try{
			if(this->MovePossible(DestX, DestY, DestZ, true, false)){
				FoundDest = true;
				break;
			}
		}catch(RESULT r){
			// no-op
		}
	}

	if(FoundDest){
		try{
			this->ToDoGo(DestX, DestY, DestZ, true, INT_MAX);
			this->ToDoWait(1000);
			this->ToDoStart();
		}catch(RESULT r){
			error("TMonster::IdleStimulus: Exception %d.\n", r);
		}
	}else{
		this->ToDoWait(1000);
		this->ToDoStart();
	}
}

void TMonster::CreatureMoveStimulus(uint32 CreatureID, int Type){
	if(CreatureID == this->ID){
		if(this->State == SLEEPING && Type != OBJECT_DELETED){
			this->State = IDLE;
			this->ToDoYield();
		}
		return;
	}

	if(this->State == SLEEPING && Type != OBJECT_DELETED){
		TCreature *Creature = GetCreature(CreatureID);
		if(Creature == NULL){
			error("TMonster::CreatureMoveStimulus: Creature %u does not exist.\n", CreatureID);
			return;
		}

		if(Creature->Type == NPC){
			return;
		}

		if(Creature->Type == MONSTER && !((TMonster*)Creature)->IsPlayerControlled()){
			return;
		}

		this->State = IDLE;
		this->ToDoYield();
	}

	TCreature::CreatureMoveStimulus(CreatureID, Type);
}

bool TMonster::CanKickBoxes(void){
	bool Result = RaceData[this->Race].KickBoxes;
	if(!Result && this->Master != 0){
		TCreature *Master = GetCreature(this->Master);
		Result = (Master != NULL && Master->Type == MONSTER
					&& ((TMonster*)Master)->CanKickBoxes());
	}
	return Result;
}

void TMonster::KickBoxes(Object Obj){
	if(!Obj.exists()){
		error("TMonster::KickBoxes: Provided object does not exist.\n");
		return;
	}

	int ObjX, ObjY, ObjZ;
	get_object_coordinates(Obj, &ObjX, &ObjY, &ObjZ);

	try{
		bool ObjMoved = false;
		int OffsetX[4] = { 0,  0, -1,  1};
		int OffsetY[4] = {-1,  1,  0,  0};
		for(int i = 0; i < 4; i += 1){
			int DestX = ObjX + OffsetX[i];
			int DestY = ObjY + OffsetY[i];
			int DestZ = ObjZ;
			if(DestX == this->posx && DestY == this->posy){
				continue;
			}

			if(coordinate_flag(DestX, DestY, DestZ, BANK)
			&& !coordinate_flag(DestX, DestY, DestZ, UNPASS)){
				Object Dest = get_map_container(DestX, DestY, DestZ);
				::move(this->ID, Obj, Dest, -1, false, NONE);
				ObjMoved = true;
				break;
			}
		}

		if(!ObjMoved){
			graphical_effect(Obj, EFFECT_BLOCK_HIT);
			delete_op(Obj, -1);
		}
	}catch(RESULT r){
		error("TMonster::KickBoxes: Exception %d, Object %d.\n",
				r, Obj.get_object_type().TypeID);
		error("# own position: [%d,%d,%d] - object position: [%d,%d,%d]\n",
				this->posx, this->posy, this->posz, ObjX, ObjY, ObjZ);
	}
}

bool TMonster::KickCreature(TCreature *Creature){
	if(Creature == NULL){
		error("TMonster::KickCreature: Provided creature does not exist.\n");
		return false;
	}

	if(Creature->Type != MONSTER){
		error("TMonster::KickCreature: Creature to be displaced is not a monster.\n");
		return false;
	}

	print(3, "%s displaces %s.\n", this->Name, Creature->Name);

	// TODO(fusion): Declare these here so they can be used within the catch
	// block to print out what's on the last position we tried to move the
	// creature to.
	int DestX = Creature->posx;
	int DestY = Creature->posy;
	int DestZ = Creature->posz;
	bool CreatureMoved = false;
	try{
		int OffsetX[4] = { 0,  0, -1,  1};
		int OffsetY[4] = {-1,  1,  0,  0};
		for(int i = 0; i < 4; i += 1){
			DestX = Creature->posx + OffsetX[i];
			DestY = Creature->posy + OffsetY[i];
			if(DestX == this->posx && DestY == this->posy){
				continue;
			}

			if(Creature->MovePossible(DestX, DestY, DestZ, true, false)
					&& !coordinate_flag(DestX, DestY, DestZ, AVOID)){
				Object Dest = get_map_container(DestX, DestY, DestZ);
				::move(this->ID, Creature->CrObject, Dest, -1, false, NONE);
				CreatureMoved = true;
				break;
			}
		}

		if(!CreatureMoved){
			print(3, "No space to displace => killing.\n");
			graphical_effect(Creature->CrObject, EFFECT_BLOCK_HIT);
			Creature->Combat.AddDamageToCombatList(this->ID,
					Creature->Skills[SKILL_HITPOINTS]->Get());
			Creature->Kill();
		}
	}catch(RESULT r){
		error("TMonster::KickCreature: Exception %d, Creature %s.\n",
				r, Creature->Name);
		error("# own position: [%d,%d,%d] - obstacle position: [%d,%d,%d]\n",
				this->posx, this->posy, this->posz,
				Creature->posx, Creature->posy, Creature->posz);

		error("# Objects on destination field [%d,%d,%d]:\n", DestX, DestY, DestZ);
		Object Obj = get_first_object(DestX, DestY, DestZ);
		while(Obj != NONE){
			error("# %d\n", Obj.get_object_type().TypeID);
			Obj = Obj.get_next_object();
		}
	}

	return CreatureMoved;
}

void TMonster::Convince(TCreature *NewMaster){
	if(NewMaster == NULL){
		error("TMonster::Convince: NewMaster is NULL.");
		return;
	}

	if(this->Home != 0){
		NotifyMonsterhomeOfDeath(this->Home);
	}

	this->Home = 0;
	if(this->Master != 0){
		TCreature *OldMaster = GetCreature(this->Master);
		if(OldMaster != NULL){
			OldMaster->SummonedCreatures -= 1;
		}
	}

	this->Master = NewMaster->ID;
	NewMaster->SummonedCreatures += 1;

	this->ToDoClear();
	this->ToDoWait(100);
	this->ToDoStart();
}

void TMonster::SetTarget(TCreature *NewTarget){
	if(NewTarget == NULL){
		error("TMonster::SetTarget: NewTarget is NULL.\n");
		return;
	}

	if(this->Master == 0){
		this->Target = NewTarget->ID;
		this->Rotate(NewTarget);
		this->ToDoYield();
	}
}

bool TMonster::IsPlayerControlled(void){
	bool Result = false;
	if(this->Master != 0){
		TCreature *Master = GetCreature(this->Master);
		Result = (Master && Master->Type == PLAYER);
	}
	return Result;
}

bool TMonster::IsFleeing(void){
	bool Result = false;
	if(this->Master == 0){
		int HitPoints = this->Skills[SKILL_HITPOINTS]->Get();
		int FleeThreshold = RaceData[this->Race].FleeThreshold;
		Result = HitPoints <= FleeThreshold;
	}
	return Result;
}

TCreature *CreateMonster(int Race, int x, int y, int z, int Home, uint32 MasterID, bool ShowEffect){
	if(!IsRaceValid(Race)){
		error("CreateMonster: Invalid race number %d.\n", Race);
		return NULL;
	}

	if(RaceData[Race].Name[0] == 0){
		error("CreateMonster: Data for race %d not defined.\n", Race);
		return NULL;
	}

	search_free_field(&x, &y, &z, 2, 0, false);
	TMonster *Monster = new TMonster(Race, x, y, z, Home, MasterID);
	if(ShowEffect){
		graphical_effect(x, y, z, EFFECT_ENERGY);
	}
	return Monster;
}

void ConvinceMonster(TCreature *Master, TCreature *Slave){
	if(Master == NULL){
		error("ConvinceMonster: Master does not exist.\n");
		return;
	}

	if(Slave == NULL){
		error("ConvinceMonster: Slave does not exist.\n");
		return;
	}

	if(Slave->Type != MONSTER){
		error("ConvinceMonster: Slave is not a monster.\n");
		return;
	}

	((TMonster*)Slave)->Convince(Master);
}

void ChallengeMonster(TCreature *Challenger, TCreature *Monster){
	if(Challenger == NULL){
		error("ChallengeMonster: Challenger does not exist.\n");
		return;
	}

	if(Monster == NULL){
		error("ChallengeMonster: Monster does not exist.\n");
		return;
	}

	if(Monster->Type != MONSTER){
		error("ChallengeMonster: Monster is not a monster.\n");
		return;
	}

	((TMonster*)Monster)->Target = Challenger->ID;
	Monster->Rotate(Challenger);
	Monster->ToDoYield();
}
