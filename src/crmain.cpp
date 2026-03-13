#include "cr.h"
#include "communication.h"
#include "config.h"
#include "enums.h"
#include "info.h"
#include "operate.h"
#include "writer.h"

#include <dirent.h>

TRaceData RaceData[MAX_RACES];
priority_queue<uint32, uint32> ToDoQueue(5000, 1000);

static TCreature *HashList[1000];
static matrix<uint32> *FirstChainCreature;
static vector<TCreature*> CreatureList(0, 10000, 1000, NULL);
static int FirstFreeCreature;
static uint32 NextCreatureID;

static int KilledCreatures[MAX_RACES];
static int KilledPlayers[MAX_RACES];

static priority_queue<uint32, TAttackWave*> AttackWaveQueue(100, 100);

// TFindCreatures
// =============================================================================
TFindCreatures::TFindCreatures(int RadiusX, int RadiusY, int CenterX, int CenterY, int Mask){
	this->initSearch(RadiusX, RadiusY, CenterX, CenterY, Mask);
}

TFindCreatures::TFindCreatures(int RadiusX, int RadiusY, uint32 CreatureID, int Mask){
	TCreature *Creature = GetCreature(CreatureID);
	if(Creature == NULL){
		error("TFindCreatures::TFindCreatures: Creature does not exist.\n");
		this->finished = true;
		return;
	}

	this->initSearch(RadiusX, RadiusY, Creature->posx, Creature->posy, Mask);
	this->SkipID = Creature->ID;
}

TFindCreatures::TFindCreatures(int RadiusX, int RadiusY, Object Obj, int Mask){
	if(!Obj.exists()){
		error("TFindCreatures::TFindCreatures: Passed object does not exist.\n");
		this->finished = true;
		return;
	}

	int ObjX, ObjY, ObjZ;
	get_object_coordinates(Obj, &ObjX, &ObjY, &ObjZ);
	this->initSearch(RadiusX, RadiusY, ObjX, ObjY, Mask);
}

void TFindCreatures::initSearch(int RadiusX, int RadiusY, int CenterX, int CenterY, int Mask){
	this->startx = CenterX - RadiusX;
	this->starty = CenterY - RadiusY;
	this->endx = CenterX + RadiusX;
	this->endy = CenterY + RadiusY;
	// NOTE(fusion): See `TFindCreatures::getNext` for an explanation on the -1.
	this->blockx = (this->startx / 16) - 1;
	this->blocky = (this->starty / 16);
	this->ActID = 0;
	this->SkipID = 0;
	this->Mask = Mask;
	this->finished = false;
}

uint32 TFindCreatures::getNext(void){
	if(this->finished){
		return 0;
	}

	int StartBlockX = this->startx / 16;
	int EndBlockX = this->endx / 16;
	int EndBlockY = this->endy / 16;
	while(true){
		while(this->ActID == 0){
			this->blockx += 1;
			if(this->blockx > EndBlockX){
				this->blockx = StartBlockX;
				this->blocky += 1;
				if(this->blocky > EndBlockY){
					this->finished = true;
					return 0;
				}
			}

			uint32 *FirstID = FirstChainCreature->boundedAt(this->blockx, this->blocky);
			if(FirstID != NULL){
				this->ActID = *FirstID;
			}else{
				this->ActID = 0;
			}
		}

		TCreature *Creature = GetCreature(this->ActID);
		if(Creature == NULL){
			error("TFindCreatures::getNext: Creature does not exist.\n");
			this->ActID = 0;
			continue;
		}

		this->ActID = Creature->NextChainCreature;
		if(Creature->ID == this->SkipID
				|| Creature->posx < this->startx || Creature->posx > this->endx
				|| Creature->posy < this->starty || Creature->posy > this->endy
				|| (Creature->Type == PLAYER  && (this->Mask & 0x01) == 0)
				|| (Creature->Type == NPC     && (this->Mask & 0x02) == 0)
				|| (Creature->Type == MONSTER && (this->Mask & 0x04) == 0)){
			continue;
		}

		return Creature->ID;
	}
}

// TCreature
// =============================================================================
void TCreature::SetID(uint32 CharacterID){
	if(this->ID != 0){
		error("TCreature::SetID: ID is already set.\n");
	}

	uint32 CreatureID = 0;
	if(CharacterID == 0){
		bool Found = false;
		for(int Attempts = 0; Attempts < 16; Attempts += 1){
			CreatureID = NextCreatureID++;
			if(GetCreature(CreatureID) == NULL){
				Found = true;
				break;
			}
		}

		if(!Found){
			error("TCreature::SetID: 16x consecutive duplicate ID."
					" Now using duplicate ID %d\n", CreatureID);
		}
	}else{
		CreatureID = CharacterID;
		if(GetCreature(CreatureID) != NULL){
			error("TCreature::SetID: Duplicate character ID %d found.\n", CharacterID);
		}
	}

	uint32 ListIndex = CreatureID % NARRAY(HashList);
	this->ID = CreatureID;
	this->NextHashEntry = HashList[ListIndex];
	HashList[ListIndex] = this;
}

void TCreature::DelID(void){
	uint32 ListIndex = this->ID % NARRAY(HashList);
	TCreature *First = HashList[ListIndex];
	if(First == NULL){
		error("TCreature::DelID: Hash entry not found id = %d\n", this->ID);
		return;
	}

	if(First->ID == this->ID){
		HashList[ListIndex] = this->NextHashEntry;
	}else{
		TCreature *Prev = First;
		TCreature *Current = First->NextHashEntry;
		while(true){
			if(Current == NULL){
				error("TCreature::DelID: id=%d not found.\n", this->ID);
				return;
			}

			if(Current->ID == this->ID){
				Prev->NextHashEntry = this->NextHashEntry;
				break;
			}

			Prev = Current;
			Current = Current->NextHashEntry;
		}
	}
}

void TCreature::SetInCrList(void){
	*CreatureList.at(FirstFreeCreature) = this;
	FirstFreeCreature += 1;
}

void TCreature::DelInCrList(void){
	// TODO(fusion): See note in `ProcessCreatures`.
	for(int Index = 0; Index < FirstFreeCreature; Index += 1){
		TCreature **Current = CreatureList.at(Index);
		if(*Current == this){
			TCreature **Last = CreatureList.at(FirstFreeCreature - 1);
			*Current = *Last;
			*Last = NULL;
			FirstFreeCreature -= 1;

			// TODO(fusion): The original function wouldn't break here. Maybe it
			// is possible to have duplicates in `CreatureList`?
			//break;
		}
	}
}

// Creature Management
// =============================================================================
TCreature *GetCreature(uint32 CreatureID){
	if(CreatureID == 0){
		return NULL;
	}

	TCreature *Creature = HashList[CreatureID % NARRAY(HashList)];
	while(Creature != NULL && Creature->ID != CreatureID){
		Creature = Creature->NextHashEntry;
	}

	return Creature;
}

TCreature *GetCreature(Object Obj){
	return GetCreature(Obj.get_creature_id());
}

void InsertChainCreature(TCreature *Creature, int CoordX, int CoordY){
	if(Creature == NULL){
		// TODO(fusion): Maybe a typo on the name of the function? I thought it
		// could be some type of macro because there was no function name mismatch
		// until now.
		error("DeleteChainCreature: Passed creature does not exist.\n");
		return;
	}

	if(CoordX == 0){
		CoordX = Creature->posx;
	}

	if(CoordY == 0){
		CoordY = Creature->posy;
	}

	int ChainX = CoordX / 16;
	int ChainY = CoordY / 16;
	uint32 *FirstID = FirstChainCreature->at(ChainX, ChainY);
	Creature->NextChainCreature = *FirstID;
	*FirstID = Creature->ID;
}

void DeleteChainCreature(TCreature *Creature){
	if(Creature == NULL){
		error("DeleteChainCreature: Passed creature does not exist.\n");
		return;
	}

	// NOTE(fusion): All creatures in each 16x16 region form a creature linked
	// list, despite its current floor. Whether that is a good idea is a whole
	// other matter.
	int ChainX = Creature->posx / 16;
	int ChainY = Creature->posy / 16;
	uint32 *FirstID = FirstChainCreature->at(ChainX, ChainY);

	if(*FirstID == Creature->ID){
		*FirstID = Creature->NextChainCreature;
	}else{
		uint32 CurrentID = *FirstID;
		while(true){
			if(CurrentID == 0){
				error("DeleteChainCreature: Creature not found.\n");
				return;
			}

			TCreature *Current = GetCreature(CurrentID);
			if(Current == NULL){
				error("DeleteChainCreature: Creature does not exist.\n");
				return;
			}

			if(Current->NextChainCreature == Creature->ID){
				Current->NextChainCreature = Creature->NextChainCreature;
				break;
			}

			CurrentID = Current->NextChainCreature;
		}
	}
}

void MoveChainCreature(TCreature *Creature, int CoordX, int CoordY){
	if(Creature == NULL){
		error("DeleteChainCreature: Passed creature does not exist.\n");
		return;
	}

	int NewChainX = CoordX / 16;
	int NewChainY = CoordY / 16;
	int OldChainX = Creature->posx / 16;
	int OldChainY = Creature->posy / 16;

	if(NewChainX != OldChainX || NewChainY != OldChainY){
		DeleteChainCreature(Creature);
		InsertChainCreature(Creature, CoordX, CoordY);
	}
}

void ProcessCreatures(void){
	for(int Index = 0; Index < FirstFreeCreature; Index += 1){
		TCreature *Creature = *CreatureList.at(Index);
		if(Creature == NULL){
			error("ProcessCreatures: Creature %d does not exist.\n", Index);
			continue;
		}

		// TODO(fusion): I'm almost sure this is processing ITEM regen rather
		// FOOD regen. It wouldn't make a lot of sense to have this plus what
		// happens in `TSkillFed::Event` if we didn't consider things like the
		// life ring, etc...
		int RegenInterval = Creature->Skills[SKILL_FED]->Get();
		if(RegenInterval > 0 && (RoundNr % RegenInterval) == 0 && !Creature->IsDead
				&& !is_protection_zone(Creature->posx, Creature->posy, Creature->posz)){
			Creature->Skills[SKILL_HITPOINTS]->Change(1);
			Creature->Skills[SKILL_MANA]->Change(4);
			if(Creature->Type == PLAYER){
				SendPlayerData(Creature->Connection);
			}
		}

		if(Creature->Type == PLAYER){
			if(Creature->Connection != NULL){
				((TPlayer*)Creature)->CheckState();
			}

			if(Creature->EarliestLogoutRound != 0 && Creature->EarliestLogoutRound <= RoundNr){
				((TPlayer*)Creature)->ClearPlayerkillingMarks();
				Creature->EarliestLogoutRound = 0;
			}
		}

		if(!Creature->IsDead && Creature->Skills[SKILL_HITPOINTS]->Get() <= 0){
			error("ProcessCreatures: Creature %s is not dead, even though it has no HP left.\n", Creature->Name);
			Creature->Death();
		}

		if(Creature->LoggingOut && Creature->LogoutPossible() == 0){ // LOGOUT_POSSIBLE ?
			if(Creature->IsDead && Creature->Skills[SKILL_HITPOINTS]->Get() > 0){
				error("ProcessCreatures: Creature %s has HP, even though it is dead.\n", Creature->Name);
				Creature->Skills[SKILL_HITPOINTS]->Set(0);
			}

			// TODO(fusion): Creatures are removed from `CreatureList` with a swap
			// and pop. Since we're iterating it RIGHT NOW, we need to process the
			// the current index AGAIN because it'll now contain the creature that
			// was previously at the end of the list. The annoying part here is that
			// this removal occurs implicitly in the creature's destructor.
			delete Creature;
			Index -= 1;
		}
	}
}

void ProcessSkills(void){
	for(int Index = 0; Index < FirstFreeCreature; Index += 1){
		TCreature *Creature = *CreatureList.at(Index);
		if(Creature == NULL){
			error("ProcessSkills: Creature %d does not exist.\n", Index);
			continue;
		}

		Creature->ProcessSkills();
	}
}

void MoveCreatures(int Delay){
	ServerMilliseconds += Delay;
	while(ToDoQueue.Entries > 0){
		auto Entry = *ToDoQueue.Entry->at(1);
		uint32 ExecutionTime = Entry.Key;
		uint32 CreatureID = Entry.Data;
		if(ExecutionTime > ServerMilliseconds){
			break;
		}

		ToDoQueue.deleteMin();
		TCreature *Creature = GetCreature(CreatureID);
		if(Creature != NULL){
			Creature->Execute();
		}
	}
}

// Kill Statistics
// =============================================================================
void AddKillStatistics(int AttackerRace, int DefenderRace){
	// NOTE(fusion): I think the race name can be "human" only for players,
	// which means we're probably tracking how many creatures are killed by
	// players with `KilledCreatures`, and how many players are killed by
	// creatures with `KilledPlayers`.

	if(strcmp(RaceData[AttackerRace].Name, "human") == 0){
		KilledCreatures[DefenderRace] += 1;
	}

	if(strcmp(RaceData[DefenderRace].Name, "human") == 0){
		KilledPlayers[AttackerRace] += 1;
	}
}

void WriteKillStatistics(void){
	// TODO(fusion): Using the same names with local and global variables is
	// trash. I'd personally have all globals with a `g_` prefix but I'm trying
	// to not change the original code too much.

	// TODO(fusion): The way we manage race names here is a disaster but could
	// make sense if there is a constant `MAX_RACE_NAME` somewhere. I've also
	// seen the length of 30 often used with name strings so it could also be
	// a general constant `MAX_NAME`.

	int NumberOfRaces = 0;
	char *RaceNames = new char[MAX_RACES * 30];
	int *KilledPlayers = new int[MAX_RACES];
	int *KilledCreatures = new int[MAX_RACES];
	for(int Race = 0; Race < MAX_RACES; Race += 1){
		if(::KilledCreatures[Race] == 0 && ::KilledPlayers[Race] == 0){
			continue;
		}

		char *Name = &RaceNames[NumberOfRaces * 30];
		if(Race == 0){
			strcpy(Name, "(fire/poison/energy)");
		}else{
			sprintf(Name, "%s %s", RaceData[Race].Article, RaceData[Race].Name);
			// TODO(fusion): The original function had a call to `Plural` and
			// `Capitals` but didn't seem to put the results back into `Name`.
		}

		KilledPlayers[NumberOfRaces] = ::KilledPlayers[Race];
		KilledCreatures[NumberOfRaces] = ::KilledCreatures[Race];
		NumberOfRaces += 1;
	}

	kill_statistics_order(NumberOfRaces, RaceNames, KilledPlayers, KilledCreatures);
	InitKillStatistics();
}

void InitKillStatistics(void){
	for(int i = 0; i < MAX_RACES; i += 1){
		KilledCreatures[i] = 0;
		KilledPlayers[i] = 0;
	}
}

void ExitKillStatistics(void){
	WriteKillStatistics();
}

// Monster Raid
// =============================================================================
TAttackWave::TAttackWave(void) :
		ExtraItem(1, 5, 5)
{
	this->x = 0;
	this->y = 0;
	this->z = 0;
	this->Spread = 0;
	this->Race = -1;
	this->MinCount = 0;
	this->MaxCount = 0;
	this->Radius = INT_MAX;
	this->Lifetime = 0;
	this->Message = 0;
	this->ExtraItems = 0;
}

TAttackWave::~TAttackWave(void){
	if(this->Message != 0){
		DeleteDynamicString(this->Message);
	}
}

void LoadMonsterRaid(const char *FileName, int Start,
		bool *Type, int *Date, int *Interval, int *Duration){
	if(FileName == NULL){
		error("LoadMonsterRaid: Filename is NULL.\n");
		throw "cannot load monster raid";
	}

	// TODO(fusion): The original function would only write to these output
	// variables when `Start` was negative. Instead, we assign dummy variables
	// if any of the pointers is NULL to allow for any reads and writes while
	// loading the raid.
	bool DummyType;
	int DummyDate;
	int DummyInterval;
	int DummyDuration;
	if(Type == NULL)		{ Type = &DummyType; }
	if(Date == NULL)		{ Date = &DummyDate; }
	if(Interval == NULL)	{ Interval = &DummyInterval; }
	if(Duration == NULL)	{ Duration = &DummyDuration; }

	*Type = false;
	*Date = 0;
	*Interval = 0;
	*Duration = 0;

	if(Start >= 0){
		print(1, "Scheduling raid %s for round %d.\n", FileName, Start);
	}

	// NOTE(fusion): We expect the first few attributes describing the raid to
	// be in an exact order, followed by attack waves. Attack wave attributes
	// don't need to be in any specific order but specifying `Delay` will wrap
	// the previous wave (if any) and start a new one.

	ReadScriptFile Script;
	Script.open(FileName);

	// NOTE(fusion): Optional `Description` attribute.
	Script.next_token();
	if(strcmp(Script.get_identifier(), "description") == 0){
		Script.read_symbol('=');
		Script.read_string();
		Script.next_token();
	}

	if(strcmp(Script.get_identifier(), "type") == 0){
		// NOTE(fusion): The type can be either "BigRaid" or "SmallRaid" so it uses
		// a boolean for `Type` to tell whether it is a big raid or not. It could be
		// renamed to `BigRaid` or something.
		Script.read_symbol('=');
		*Type = (strcmp(Script.read_identifier(), "bigraid") == 0);
	}else{
		Script.error("type expected");
	}

	Script.next_token();
	if(strcmp(Script.get_identifier(), "date") == 0){
		Script.read_symbol('=');
		*Date = Script.read_number();
	}else if(strcmp(Script.get_identifier(), "interval") == 0){
		Script.read_symbol('=');
		*Interval = Script.read_number();
	}else{
		Script.error("date or interval expected");
	}

	Script.next_token();
	while(Script.Token != ENDOFFILE){
		if(strcmp(Script.get_identifier(), "delay") != 0){
			Script.error("delay expected");
		}

		Script.read_symbol('=');
		int Delay = Script.read_number();
		TAttackWave *Wave = new TAttackWave;
		while(true){
			Script.next_token();
			if(Script.Token == ENDOFFILE){
				break;
			}

			if(strcmp(Script.get_identifier(), "delay") == 0){
				break;
			}

			char Identifier[MAX_IDENT_LENGTH];
			strcpy(Identifier, Script.get_identifier());
			Script.read_symbol('=');
			if(strcmp(Identifier, "location") == 0){
				Script.read_string();
			}else if(strcmp(Identifier, "position") == 0){
				Script.read_coordinate(&Wave->x, &Wave->y, &Wave->z);
			}else if(strcmp(Identifier, "spread") == 0){
				Wave->Spread = Script.read_number();
			}else if(strcmp(Identifier, "race") == 0){
				Wave->Race = Script.read_number();
				if(!IsRaceValid(Wave->Race)){
					Script.error("illegal race number");
				}
			}else if(strcmp(Identifier, "count") == 0){
				Script.read_symbol('(');
				Wave->MinCount = Script.read_number();
				Script.read_symbol(',');
				Wave->MaxCount = Script.read_number();
				Script.read_symbol(')');

				if(Wave->MaxCount < Wave->MinCount){
					Script.error("mincount greater than maxcount");
				}

				if(Wave->MinCount < 0 || Wave->MaxCount < 1){
					Script.error("illegal number of monsters");
				}
			}else if(strcmp(Identifier, "radius") == 0){
				Wave->Radius = Script.read_number();
			}else if(strcmp(Identifier, "lifetime") == 0){
				Wave->Lifetime = Script.read_number();
			}else if(strcmp(Identifier, "message") == 0){
				Wave->Message = AddDynamicString(Script.read_string());
			}else if(strcmp(Identifier, "inventory") == 0){
				Script.read_symbol('{');
				do{
					// NOTE(fusion): Items are indexed from 1.
					Wave->ExtraItems += 1;
					TItemData *ItemData = Wave->ExtraItem.at(Wave->ExtraItems);
					Script.read_symbol('(');
					ItemData->Type = Script.read_number();
					Script.read_symbol(',');
					ItemData->Maximum = Script.read_number();
					Script.read_symbol(',');
					ItemData->Probability = Script.read_number();
					Script.read_symbol(')');
				}while(Script.read_special() != '}');
			}else{
				Script.error("unknown attack wave property");
			}
		}

		if(Wave->x == 0){
			Script.error("position expected");
		}

		if(Wave->Race == -1){
			Script.error("race expected");
		}

		if(Wave->MaxCount == 0){
			Script.error("count expected");
		}

		int WaveEnd = Delay + (Wave->Lifetime != 0 ? Wave->Lifetime : 3600);
		if(*Duration < WaveEnd){
			*Duration = WaveEnd;
		}

		if(Start >= 0){
			AttackWaveQueue.insert(Start + Delay, Wave);
		}else{
			delete Wave;
		}
	}

	Script.close();
}

void LoadMonsterRaids(void){
	DIR *MonsterDir = opendir(MONSTERPATH);
	if(MonsterDir == NULL){
		error("LoadMonsterRaids: Subdirectory %s not found.\n", MONSTERPATH);
		throw "Cannot load monster raids";
	}

	// TODO(fusion): The `Date` attribute used by raid files is a unix timestamp
	// which is usually what `time(NULL)` returns. This should work fine until
	// 2038 when the timestamp value exceeds the capacity of a 32-bit signed
	// integer.
	int Now = (int)std::min<time_t>(time(NULL), INT_MAX);

	int Hour, Minute;
	GetRealTime(&Hour, &Minute);

	int SecondsToReboot = (RebootTime - (Hour * 60 + Minute)) * 60;
	if(SecondsToReboot < 0){
		SecondsToReboot += 86400;
	}

	char BigRaidName[4096] = {};
	int BigRaidDuration = 0;
	int BigRaidTieBreaker = -1;

	char FileName[4096];
	while(dirent *DirEntry = readdir(MonsterDir)){
		if(DirEntry->d_type != DT_REG){
			continue;
		}

		const char *FileExt = findLast(DirEntry->d_name, '.');
		if(FileExt == NULL || strcmp(FileExt, ".evt") != 0){
			continue;
		}

		bool BigRaid;
		int Date, Interval, Duration;
		snprintf(FileName, sizeof(FileName), "%s/%s", MONSTERPATH, DirEntry->d_name);
		LoadMonsterRaid(FileName, -1, &BigRaid, &Date, &Interval, &Duration);
		print(1, "Raid %s: Date %d, Interval %d, Duration %d\n",
				FileName, Date, Interval, Duration);

		if(Date > 0){
			if(Now <= Date && Date <= (Now + SecondsToReboot)){
				int Start = RoundNr + (Date - Now);
				LoadMonsterRaid(FileName, Start, NULL, NULL, NULL, NULL);
				if(BigRaid){
					BigRaidName[0] = 0;
					BigRaidDuration = 0;
					BigRaidTieBreaker = 100;
				}
			}
		}else if(Duration <= SecondsToReboot){
			// NOTE(fusion): `Interval` specifies an average raid interval
			// in seconds. With this function being called at startup, usually
			// after a reboot, we can expect `SecondsToReboot` to be close to
			// a day very regularly. Meaning we can approximate the condition
			// below to `random(1, AverageIntervalDays) == 1` which hopefully
			// makes more sense.
			if(random(0, Interval - 1) < SecondsToReboot){
				if(!BigRaid){
					int Start = RoundNr + random(0, SecondsToReboot - Duration);
					LoadMonsterRaid(FileName, Start, NULL, NULL, NULL, NULL);
				}else{
					int TieBreaker = random(0, 99);
					if(TieBreaker > BigRaidTieBreaker){
						strcpy(BigRaidName, FileName);
						BigRaidDuration = Duration;
						BigRaidTieBreaker = TieBreaker;
					}
				}
			}
		}
	}

	closedir(MonsterDir);

	if(BigRaidName[0] != 0){
		int Start = RoundNr + random(0, SecondsToReboot - BigRaidDuration);
		LoadMonsterRaid(BigRaidName, Start, NULL, NULL, NULL, NULL);
	}
}

void ProcessMonsterRaids(void){
	while(AttackWaveQueue.Entries > 0){
		auto Entry = *AttackWaveQueue.Entry->at(1);
		uint32 ExecutionRound = Entry.Key;
		TAttackWave *Wave = Entry.Data;
		if(ExecutionRound > RoundNr){
			break;
		}

		AttackWaveQueue.deleteMin();
		print(2, "Attack by monsters of race %d.\n", Wave->Race);
		if(Wave->Message != 0){
			BroadcastMessage(TALK_EVENT_MESSAGE, "%s", GetDynamicString(Wave->Message));
		}

		int NumSpawned = 0;
		TCreature *Spawned[64] = {};

		// NOTE(fusion): The original function would use `alloca` to allocate the
		// buffer for spawned creatures. Inspecting some raid files, the maximum
		// count I could find was 40. I don't think it is realistic for this number
		// to get much higher, mostly because you can always split a large wave
		// into multiple smaller ones, and because going crazy with alloca could
		// cause the stack to blow up.
		int Count = random(Wave->MinCount, Wave->MaxCount);
		if(Count > NARRAY(Spawned)){
			Count = NARRAY(Spawned);
		}

		for(int i = 0; i < Count; i += 1){
			int SpawnX = Wave->x + random(-Wave->Spread, Wave->Spread);
			int SpawnY = Wave->y + random(-Wave->Spread, Wave->Spread);
			int SpawnZ = Wave->z;
			if(!search_free_field(&SpawnX, &SpawnY, &SpawnZ, 1, 0, false)
					|| is_protection_zone(SpawnX, SpawnY, SpawnZ)){
				continue;
			}

			TCreature *Creature = CreateMonster(Wave->Race, SpawnX, SpawnY, SpawnZ, 0, 0, true);
			if(Creature != NULL){
				Creature->Radius = Wave->Radius;
				if(Wave->Lifetime != 0){
					Creature->LifeEndRound = RoundNr + Wave->Lifetime;
				}

				Spawned[NumSpawned] = Creature;
				NumSpawned += 1;
			}
		}

		if(NumSpawned > 0){
			int ExtraItems = Wave->ExtraItems;
			for(int i = 1; i <= ExtraItems; i += 1){
				TItemData *ItemData = Wave->ExtraItem.at(i);
				if(random(0, 999) > ItemData->Probability){
					continue;
				}

				TCreature *Creature = Spawned[random(0, NumSpawned - 1)];
				Object Bag = get_body_object(Creature->ID, INVENTORY_BAG);
				if(Bag == NONE){
					try{
						Bag = create(get_body_container(Creature->ID, INVENTORY_BAG),
								get_special_object(DEFAULT_CONTAINER),
								0);
					}catch(RESULT r){
						error("ProcessMonsterRaids: Exception %d for race %d"
								" when creating the backpack.\n", r, Wave->Race);
						continue;
					}
				}

				ObjectType ItemType = ItemData->Type;
				int Amount = random(1, ItemData->Maximum);
				int Repeat = 1;
				if(!ItemType.get_flag(CUMULATIVE)){
					Repeat = Amount;
					Amount = 0;
				}

				print(2, "Distributing %d objects of type %d.\n", Amount, ItemType.TypeID);
				for(int j = 0; j < Repeat; j += 1){
					// TODO(fusion): What's the difference between using `create`
					// and `create_at_creature` here? Maybe it checks carry strength,
					// but then why? Don't they have some check to make sure items
					// are not dropped onto the map? This is very confusing and
					// using exception handlers all over the place doesn't help
					// either.
					Object Item = NONE;
					try{
						// TODO(fusion): Possibly an inlined function to check for
						// weapons or equipment?
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
							Item = create_at_creature(Creature->ID, ItemType, Amount);
						}
					}catch(RESULT r){
						error("ProcessMonsterRaids: Exception %d for race %d, consider"
							" increasing CarryStrength.\n", r, Wave->Race);
						break;
					}

					if(Item.get_container().get_object_type().is_map_container()){
						error("ProcessMonsterRaids: Object falls on the map."
								" Increase CarryStrength for race %d.\n", Wave->Race);
						delete_op(Item, -1);
						// TODO(fusion): Should probably stop this inner loop here.
					}
				}

				// NOTE(fusion): `Bag` could be empty if we failed to add any
				// items to it in the loop above.
				if(get_first_container_object(Bag) == NONE){
					delete_op(Bag, -1);
				}
			}
		}

		delete Wave;
	}
}

// Initialization
// =============================================================================
void InitCr(void){
	NextCreatureID = 0x40000000;
	FirstFreeCreature = 0;
	FirstChainCreature = new matrix<uint32>(
				SectorXMin * 2, SectorXMax * 2 + 1,
				SectorYMin * 2, SectorYMax * 2 + 1,
				0);

	LoadRaces();
	LoadMonsterRaids();
	InitCrskill();
	InitPlayer();
	InitNonplayer();
	InitKillStatistics();
}

void ExitCr(void){
	ExitKillStatistics();
	ExitPlayer();
	ExitNonplayer();
	ExitCrskill();

	delete FirstChainCreature;
}
