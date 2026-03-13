#include "game_data/map/map.h"
#include "containers.h"
#include "config.h"
#include "enums.h"
#include "houses.h"
#include "script.h"

#include <dirent.h>

int SectorXMin;
int SectorXMax;
int SectorYMin;
int SectorYMax;
int SectorZMin;
int SectorZMax;
int RefreshedCylinders;
int NewbieStartPositionX;
int NewbieStartPositionY;
int NewbieStartPositionZ;
int VeteranStartPositionX;
int VeteranStartPositionY;
int VeteranStartPositionZ;

static int OBCount;
static matrix3d<Sector*> *SectorMatrix;
static ObjectBlock **ObjectBlockArray;
static MapObject *FirstFreeObject;
static MapObject **HashTableData;
static uint8 *HashTableType;
static uint32 HashTableSize;
static uint32 HashTableMask;
static uint32 HashTableFree;
static uint32 ObjectCounter;

static vector<CronEntry> CronEntryArray(0, 256, 256);
static int CronHashTable[2047];
static int CronEntries;

static vector<DepotInfo> DepotInfoArray(0, 4, 5);
static vector<Mark> MarkArray(0, 4, 5);
static int Marks;

static TDynamicWriteBuffer HelpBuffer(kb(64));

// Object
// =============================================================================
bool Object::exists(void){
	if(*this == NONE){
		return false;
	}

	uint32 EntryIndex = this->ObjectID & HashTableMask;
	if(HashTableType[EntryIndex] == STATUS_SWAPPED){
		unswap_sector((uintptr)HashTableData[EntryIndex]);
	}

	return HashTableType[EntryIndex] == STATUS_LOADED
		&& HashTableData[EntryIndex]->ObjectID == this->ObjectID;
}

ObjectType Object::get_object_type(void){
	return access_object(*this)->Type;
}

void Object::set_object_type(ObjectType Type){
	access_object(*this)->Type = Type;
}

Object Object::get_next_object(void){
	return access_object(*this)->NextObject;
}

void Object::set_next_object(Object NextObject){
	access_object(*this)->NextObject = NextObject;
}

Object Object::get_container(void){
	return access_object(*this)->Container;
}

void Object::set_container(Object Container){
	access_object(*this)->Container = Container;
}

uint32 Object::get_creature_id(void){
	// TODO(fusion): We call `access_object` once in `get_object_type` then again
	// after checking the TypeID, when we could call it once to check both type
	// and access `Attributes[1]`.

	if(!this->get_object_type().is_creature_container()){
		error("Object::get_creature_id: Object is not a creature.\n");
		return 0;
	}

	return access_object(*this)->Attributes[1];
}

uint32 Object::get_attribute(INSTANCEATTRIBUTE Attribute){
	ObjectType ObjType = this->get_object_type();
	int AttributeOffset = ObjType.get_attribute_offset(Attribute);
	if(AttributeOffset == -1){
		error("Object::get_attribute: Flag for attribute %d on object type %d not set.\n",
				Attribute, ObjType.TypeID);
		return 0;
	}

	if(AttributeOffset < 0 || AttributeOffset >= NARRAY(MapObject::Attributes)){
		error("Object::get_attribute: Invalid offset %d for attribute %d on object type %d.\n",
				AttributeOffset, Attribute, ObjType.TypeID);
		return 0;
	}

	return access_object(*this)->Attributes[AttributeOffset];
}

void Object::set_attribute(INSTANCEATTRIBUTE Attribute, uint32 Value){
	ObjectType ObjType = this->get_object_type();
	int AttributeOffset = ObjType.get_attribute_offset(Attribute);
	if(AttributeOffset == -1){
		error("Object::set_attribute: Flag for attribute %d on object type %d not set.\n",
				Attribute, ObjType.TypeID);
		return;
	}

	if(AttributeOffset < 0 || AttributeOffset >= NARRAY(MapObject::Attributes)){
		error("Object::set_attribute: Invalid offset %d for attribute %d on object type %d.\n",
				AttributeOffset, Attribute, ObjType.TypeID);
		return;
	}

	if(Value == 0){
		if(Attribute == AMOUNT || Attribute == POOLLIQUIDTYPE || Attribute == CHARGES){
			Value = 1;
		}else if(Attribute == REMAININGUSES){
			Value = ObjType.get_attribute(TOTALUSES);
		}
	}

	access_object(*this)->Attributes[AttributeOffset] = Value;
}

// Cron Management
// =============================================================================
static void CronMove(int Destination, int Source){
	CronEntry *DestEntry = CronEntryArray.at(Destination);
	*DestEntry = *CronEntryArray.at(Source);

	if(DestEntry->Next != -1){
		CronEntryArray.at(DestEntry->Next)->Previous = Destination;
	}

	if(DestEntry->Previous != -1){
		CronEntryArray.at(DestEntry->Previous)->Next = Destination;
	}else{
		CronHashTable[DestEntry->Obj.ObjectID % NARRAY(CronHashTable)] = Destination;
	}
}

static void CronHeapify(int Position){
	while(Position > 1){
		int Parent = Position / 2;
		CronEntry *CurrentEntry = CronEntryArray.at(Position);
		CronEntry *ParentEntry = CronEntryArray.at(Parent);
		if(ParentEntry->RoundNr <= CurrentEntry->RoundNr){
			break;
		}

		// NOTE(fusion): This is emulating a swap, using position 0 as the
		// temporary value. It is needed to maintain hash table links valid.
		CronMove(0, Position);
		CronMove(Position, Parent);
		CronMove(Parent, 0);

		Position = Parent;
	}

	// IMPORTANT(fusion): This is different from `priority_queue::deleteMin` as
	// the last element is still in the heap and needs to be considered.
	int Last = CronEntries;
	while(true){
		int Smallest = Position * 2;
		if(Smallest > Last){
			break;
		}

		CronEntry *SmallestEntry = CronEntryArray.at(Smallest);
		if((Smallest + 1) <= Last){
			CronEntry *OtherEntry = CronEntryArray.at(Smallest + 1);
			if(OtherEntry->RoundNr < SmallestEntry->RoundNr){
				SmallestEntry = OtherEntry;
				Smallest += 1;
			}
		}

		CronEntry *CurrentEntry = CronEntryArray.at(Position);
		if(CurrentEntry->RoundNr <= SmallestEntry->RoundNr){
			break;
		}

		// NOTE(fusion): Same in the first loop.
		CronMove(0, Position);
		CronMove(Position, Smallest);
		CronMove(Smallest, 0);

		Position = Smallest;
	}
}

static void CronSet(Object Obj, uint32 Delay){
	if(!Obj.exists()){
		error("CronSet: Provided object does not exist.\n");
		return;
	}

	// TODO(fusion): We don't check if the object is already in the table.

	CronEntries += 1;

	int Position = CronEntries;
	CronEntry *Entry = CronEntryArray.at(Position);
	Entry->Obj = Obj;
	Entry->RoundNr = RoundNr + Delay;
	Entry->Previous = -1;
	Entry->Next = CronHashTable[Obj.ObjectID % NARRAY(CronHashTable)];
	CronHashTable[Obj.ObjectID % NARRAY(CronHashTable)] = Position;
	if(Entry->Next != -1){
		CronEntryArray.at(Entry->Next)->Previous = Position;
	}

	CronHeapify(Position);
}

static void CronDelete(int Position){
	if(Position < 1 || Position > CronEntries){
		error("CronDelete: Invalid position %d.\n", Position);
		return;
	}

	CronEntry *Entry = CronEntryArray.at(Position);

	if(Entry->Next != -1){
		CronEntryArray.at(Entry->Next)->Previous = Entry->Previous;
	}

	if(Entry->Previous != -1){
		CronEntryArray.at(Entry->Previous)->Next = Entry->Next;
	}else{
		CronHashTable[Entry->Obj.ObjectID % NARRAY(CronHashTable)] = Entry->Next;
	}

	int Last = CronEntries;
	CronEntries -= 1;
	if(Position != Last){
		CronMove(Position, Last);
		CronHeapify(Position);
	}
}

Object cron_check(void){
	Object Obj = NONE;
	if(CronEntries != 0){
		CronEntry *Entry = CronEntryArray.at(1);
		if(Entry->RoundNr <= RoundNr){
			Obj = Entry->Obj;
		}
	}
	return Obj;
}

void cron_expire(Object Obj, int Delay){
	if(!Obj.exists()){
		error("cron_expire: Provided object does not exist.\n");
		return;
	}

	ObjectType ObjType = Obj.get_object_type();
	if(ObjType.get_flag(EXPIRE)){
		if(Delay == -1){
			CronSet(Obj, ObjType.get_attribute(TOTALEXPIRETIME));
		}else{
			CronSet(Obj, (uint32)Delay);
		}
	}
}

void cron_change(Object Obj, int NewDelay){
	if(!Obj.exists()){
		error("cron_change: Provided object does not exist.\n");
		return;
	}

	int Position = CronHashTable[Obj.ObjectID % NARRAY(CronHashTable)];
	while(Position != -1){
		CronEntry *Entry = CronEntryArray.at(Position);
		if(Entry->Obj == Obj){
			Entry->RoundNr = RoundNr + NewDelay;
			CronHeapify(Position);
			return;
		}
		Position = Entry->Next;
	}

	error("cron_change: Object is not registered in the cron system.\n");
}

uint32 cron_info(Object Obj, bool delete_op){
	if(!Obj.exists()){
		error("cron_info: Provided object does not exist.\n");
		return 0;
	}

	int Position = CronHashTable[Obj.ObjectID % NARRAY(CronHashTable)];
	while(Position != -1){
		CronEntry *Entry = CronEntryArray.at(Position);
		if(Entry->Obj == Obj){
			uint32 Remaining = 1;
			if(Entry->RoundNr > RoundNr){
				Remaining = Entry->RoundNr - RoundNr;
			}
			if(delete_op){
				CronDelete(Position);
			}
			return Remaining;
		}
		Position = Entry->Next;
	}

	error("cron_info: Object is not registered in the cron system.\n");
	return 0;
}

uint32 cron_stop(Object Obj){
	if(!Obj.exists()){
		error("cron_stop: Provided object does not exist.\n");
		return 0;
	}

	return cron_info(Obj, true);
}

// Map Management
// =============================================================================
static void ReadMapConfig(void){
	OBCount = 0xA0000;
	SectorXMin = 1000;
	SectorXMax = 1015;
	SectorYMin = 1000;
	SectorYMax = 1015;
	SectorZMin = 0;
	SectorZMax = 15;
	RefreshedCylinders = 1;
	NewbieStartPositionX = 0;
	NewbieStartPositionY = 0;
	NewbieStartPositionZ = 0;
	VeteranStartPositionX = 0;
	VeteranStartPositionY = 0;
	VeteranStartPositionZ = 0;
	HashTableSize = 0x100000;
	Marks = 0;

	char FileName[4096];
	snprintf(FileName, sizeof(FileName), "%s/map.dat", DATAPATH);

	ReadScriptFile Script;
	Script.open(FileName);
	while(true){
		Script.next_token();
		if(Script.Token == ENDOFFILE){
			Script.close();
			break;
		}

		char Identifier[MAX_IDENT_LENGTH];
		strcpy(Identifier, Script.get_identifier());
		Script.read_symbol('=');

		if(strcmp(Identifier, "sectorxmin") == 0){
			SectorXMin = Script.read_number();
		}else if(strcmp(Identifier, "sectorxmax") == 0){
			SectorXMax = Script.read_number();
		}else if(strcmp(Identifier, "sectorymin") == 0){
			SectorYMin = Script.read_number();
		}else if(strcmp(Identifier, "sectorymax") == 0){
			SectorYMax = Script.read_number();
		}else if(strcmp(Identifier, "sectorzmin") == 0){
			SectorZMin = Script.read_number();
		}else if(strcmp(Identifier, "sectorzmax") == 0){
			SectorZMax = Script.read_number();
		}else if(strcmp(Identifier, "refreshedcylinders") == 0){
			RefreshedCylinders = Script.read_number();
		}else if(strcmp(Identifier, "objects") == 0){
			HashTableSize = (uint32)Script.read_number();
		}else if(strcmp(Identifier, "cachesize") == 0){
			OBCount = Script.read_number();
		}else if(strcmp(Identifier, "depot") == 0){
			int DepotIndex = 0;
			DepotInfo TempInfo = {};
			Script.read_symbol('(');
			DepotIndex = Script.read_number();
			Script.read_symbol(',');
			const char *Town = Script.read_string();
			if(strlen(Town) >= NARRAY(TempInfo.Town)){
				Script.error("town name too long");
			}
			strcpy(TempInfo.Town, Town);
			Script.read_symbol(',');
			TempInfo.Size = Script.read_number();
			Script.read_symbol(')');
			*DepotInfoArray.at(DepotIndex) = TempInfo;
		}else if(strcmp(Identifier, "mark") == 0){
			Mark TempMark = {};
			Script.read_symbol('(');
			const char *Name = Script.read_string();
			if(strlen(Name) >= NARRAY(TempMark.Name)){
				Script.error("mark name too long");
			}
			strcpy(TempMark.Name, Name);
			Script.read_symbol(',');
			Script.read_coordinate(&TempMark.x, &TempMark.y, &TempMark.z);
			Script.read_symbol(')');
			*MarkArray.at(Marks) = TempMark;
			Marks += 1;
		}else if(strcmp(Identifier, "newbiestart") == 0){
			 Script.read_coordinate(
					&NewbieStartPositionX,
					&NewbieStartPositionY,
					&NewbieStartPositionZ);
		}else if(strcmp(Identifier, "veteranstart") == 0){
			 Script.read_coordinate(
					&VeteranStartPositionX,
					&VeteranStartPositionY,
					&VeteranStartPositionZ);
		}else{
			// TODO(fusion):
			//error("Unknown map configuration key \"%s\"", Identifier);
		}
	}

	// NOTE(fusion): If each sector is 32 x 32 tiles and the whole world is
	// 65535 x 65535 tiles, then the maximum number of sectors is 65535 / 32
	// which is the 2047 used below. We should probably define these contants.
	//	Notice that sectors at the edge of the XY-plane are also considered
	// invalid, probably to add some buffer to avoid wrapping or other types
	// of problems.

	if(SectorXMin <= 0){
		throw "illegal value for SectorXMin";
	}

	if(SectorXMax >= 2047){
		throw "illegal value for SectorXMax";
	}

	if(SectorYMin <= 0){
		throw "illegal value for SectorYMin";
	}

	if(SectorYMax >= 2047){
		throw "illegal value for SectorYMax";
	}

	if(SectorZMin < 0){
		throw "illegal value for SectorZMin";
	}

	if(SectorZMax > 15){
		throw "illegal value for SectorZMax";
	}

	if(SectorXMin > SectorXMax){
		throw "SectorXMin is greater than SectorXMax";
	}

	if(SectorYMin > SectorYMax){
		throw "SectorYMin is greater than SectorYMax";
	}

	if(SectorZMin > SectorZMax){
		throw "SectorZMin is greater than SectorZMax";
	}

	// TODO(fusion): Just align up from whatever value we got? And use `ObjectsPerBlock`?
	if(OBCount % 32768 != 0){
		throw "CacheSize must be a multiple of 32768";
	}

	if(OBCount <= 0){
		throw "illegal value for CacheSize";
	}

	OBCount /= 32768;

	if(HashTableSize <= 0){
		throw "illegal value for Objects";
	}

	if(!is_pow2(HashTableSize)){
		throw "Objects must be a power of 2";
	}

	if(NewbieStartPositionX == 0){
		throw "no start position for newbies specified";
	}

	if(VeteranStartPositionX == 0){
		throw "no start position for veterans specified";
	}
}

static void ResizeHashTable(void){
	uint32 OldSize = HashTableSize;
	uint32 NewSize = OldSize * 2;
	ASSERT(is_pow2(OldSize));
	ASSERT(NewSize > OldSize);

	// TODO(fusion): See note below.
	error("FATAL ERROR in ResizeHashTable: Resizing the object hash table is"
			" currently disabled. You may increase `Objects` in the map config"
			" from %d to %d, to achieve the same effect.", OldSize, NewSize);
	abort();

	error("INFO: Hash table too small. Size is being doubled to %d.\n", NewSize);

	uint32 NewMask = NewSize - 1;
	MapObject **NewData = (MapObject**)malloc(NewSize * sizeof(MapObject*));
	uint8 *NewType = (uint8*)malloc(NewSize * sizeof(uint8));
	memset(NewType, 0, NewSize * sizeof(uint8));

	// TODO(fusion): This rehash loop doesn't make a lot of sense. It wants to
	// access all existing objects but doing so would cause all sectors to be
	// swapped in at some time or another. This may be bad for performance but
	// the real problem is that `unswap_sector` may swap out some other sector
	// whose objects were already put into `NewType` and `NewData`, causing
	// multiple object ids to reference the same `MapObject`.
	//	Looking at some of the logs included with this executable, it doesn't
	// look like this function was ever called which explains why it wasn't fixed
	// earlier.
	//	It would be possible to do this rehashing without swapping anything out,
	// if we stored ObjectID somewhere (maybe `HashTableData`). Doubling the size
	// of the table means there are two possible indices for each previous entry,
	// and we can only determine which one to actually move it with the ObjectID.

	NewType[0] = STATUS_PERMANENT;
	NewData[0] = HashTableData[0];
	for(uint32 i = 1; i < OldSize; i += 1){
		if(HashTableType[i] != STATUS_FREE){
			if(HashTableType[i] == STATUS_SWAPPED){
				unswap_sector((uintptr)HashTableData[i]);
			}

			if(HashTableType[i] == STATUS_LOADED){
				MapObject *Entry = HashTableData[i];
				NewType[Entry->ObjectID & NewMask] = STATUS_LOADED;
				NewData[Entry->ObjectID & NewMask] = Entry;
			}else{
				error("ResizeHashTable: Error reorganizing the hash table.\n");
			}
		}
	}

	free(HashTableData);
	free(HashTableType);

	HashTableData = NewData;
	HashTableType = NewType;
	HashTableSize = NewSize;
	HashTableMask = NewMask;
	HashTableFree += (NewSize - OldSize);
}

static MapObject *GetFreeObjectSlot(void){
	if(FirstFreeObject == NULL){
		swap_sector();
	}

	MapObject *Entry = FirstFreeObject;
	if(Entry == NULL){
		error("GetFreeObjectSlot: No free slots remaining.\n");
		return NULL;
	}

	// NOTE(fusion): The next object pointer was originally stored in `Entry->NextObject.ObjectID`
	// which is a problem when compiling in 64 bits mode. For this reason, I've changed it to be
	// stored at the beggining of `MapObject`.
	FirstFreeObject = *((MapObject**)Entry);

	// TODO(fusion): Using `memset` here will trigger a compiler warning because `MapObject` contains
	// a few `Object`s and I've made them non PODs by adding a few constructors.
	//memset(Entry, 0, sizeof(MapObject));

	*Entry = MapObject{};
	return Entry;
}

static void PutFreeObjectSlot(MapObject *Entry){
	if(Entry == NULL){
		error("PutFreeObjectSlot: Entry is NULL.\n");
		return;
	}

	// NOTE(fusion): See note in `GetFreeObjectSlot`, just above.
	*((MapObject**)Entry) = FirstFreeObject;
	FirstFreeObject = Entry;
}

void swap_object(TWriteBinaryFile *File, Object Obj, uintptr FileNumber){
	ASSERT(Obj != NONE);

	// NOTE(fusion): Does it make sense to swap an object that isn't loaded? We
	// were originally calling `Object::exists` that would swap in the object's
	// sector if it was swapped out. We should probably have an assertion here.
	uint32 EntryIndex = Obj.ObjectID & HashTableMask;
	if(HashTableType[EntryIndex] != STATUS_LOADED){
		error("swap_object: Object doesn't exist or is not currently loaded.\n");
		return;
	}

	MapObject *Entry = HashTableData[EntryIndex];
	if(Entry->ObjectID != Obj.ObjectID){
		error("swap_object: Provided object does not exist.\n");
		return;
	}

	File->writeBytes((const uint8*)Entry, sizeof(MapObject));
	if(Entry->Type.get_flag(CONTAINER) || Entry->Type.get_flag(CHEST)){
		Object Current = Object(Obj.get_attribute(CONTENT));
		while(Current != NONE){
			Object Next = Current.get_next_object();
			swap_object(File, Current, FileNumber);
			Current = Next;
		}
	}

	PutFreeObjectSlot(Entry);
	HashTableType[EntryIndex] = STATUS_SWAPPED;
	HashTableData[EntryIndex] = (MapObject*)FileNumber;
}

void swap_sector(void){
	static uintptr FileNumber = 0;

	Sector *Oldest = NULL;
	int OldestSectorX = 0;
	int OldestSectorY = 0;
	int OldestSectorZ = 0;
	uint32 OldestTimeStamp = RoundNr + 1;

	ASSERT(SectorMatrix != NULL);
	for(int SectorZ = SectorZMin; SectorZ <= SectorZMax; SectorZ += 1)
	for(int SectorY = SectorYMin; SectorY <= SectorYMax; SectorY += 1)
	for(int SectorX = SectorXMin; SectorX <= SectorXMax; SectorX += 1){
		Sector *CurrentSector = *SectorMatrix->at(SectorX, SectorY, SectorZ);
		if(CurrentSector != NULL
				&& CurrentSector->Status == STATUS_LOADED
				&& CurrentSector->TimeStamp < OldestTimeStamp){
			Oldest = CurrentSector;
			OldestSectorX = SectorX;
			OldestSectorY = SectorY;
			OldestSectorZ = SectorZ;
			OldestTimeStamp = CurrentSector->TimeStamp;
		}
	}

	if(Oldest == NULL){
		error("FATAL ERROR in swap_sector: No sector can be swapped out.\n");
		abort();
	}

	char FileName[4096];
	do{
		FileNumber += 1;
		if(FileNumber > 99999999){
			FileNumber = 1;
		}
		snprintf(FileName, sizeof(FileName), "%s/%08u.swp", SAVEPATH, (uint32)FileNumber);
	}while(FileExists(FileName));

	TWriteBinaryFile File;
	try{
		if(!File.open(FileName)){
			error("FATAL ERROR in swap_sector: Cannot create file \"%s\".\n", FileName);
			abort();
		}
		print(2, "Swapping out sector %d/%d/%d...\n", OldestSectorX, OldestSectorY, OldestSectorZ);
		File.writeQuad((uint32)OldestSectorX);
		File.writeQuad((uint32)OldestSectorY);
		File.writeQuad((uint32)OldestSectorZ);
		for(int X = 0; X < 32; X += 1){
			for(int Y = 0; Y < 32; Y += 1){
				swap_object(&File, Oldest->MapCon[X][Y], FileNumber);
			}
		}
		Oldest->Status = STATUS_SWAPPED;
		File.close();
	}catch(const char *str){
		error("FATAL ERROR in swap_sector: Cannot create file \"%s\".\n", FileName);
		error("# Error: %s\n", str);
		abort();
	}
}

void unswap_sector(uintptr FileNumber){
	char FileName[4096];
	snprintf(FileName, sizeof(FileName), "%s/%08u.swp", SAVEPATH, (uint32)FileNumber);

	TReadBinaryFile File;
	try{
		if(!File.open(FileName)){
			error("FATAL ERROR in unswap_sector: Cannot open file \"%s\".\n", FileName);
			abort();
		}
		int SectorX = (int)File.readQuad();
		int SectorY = (int)File.readQuad();
		int SectorZ = (int)File.readQuad();
		print(2, "Swapping in sector %d/%d/%d...\n", SectorX, SectorY, SectorZ);

		ASSERT(SectorMatrix != NULL);
		Sector *LoadingSector = *SectorMatrix->at(SectorX, SectorY, SectorZ);
		if(LoadingSector == NULL){
			error("unswap_sector: Sector %d/%d/%d does not exist.\n", SectorX, SectorY, SectorZ);
			File.close();
			return;
		}

		if(LoadingSector->Status != STATUS_SWAPPED){
			error("unswap_sector: Sector %d/%d/%d is not swapped out.\n", SectorX, SectorY, SectorZ);
			File.close();
			return;
		}

		int Size = File.getSize();
		while(File.getPosition() < Size){
			MapObject Entry;
			File.readBytes((uint8*)&Entry, sizeof(MapObject));

			uint32 EntryIndex = Entry.ObjectID & HashTableMask;
			if(HashTableType[EntryIndex] == STATUS_SWAPPED){
				// NOTE(fusion): Make sure we only allocate the object if we confirm
				// its status. The original code would call `readBytes` on the result
				// from `GetFreeObjectSlot()` directly and would then leak it if the
				// entry status was not `STATUS_SWAPPED`.
				MapObject *EntryPointer = GetFreeObjectSlot();
				*EntryPointer = Entry;
				HashTableData[EntryIndex] = EntryPointer;
				HashTableType[EntryIndex] = STATUS_LOADED;
			}else{
				error("unswap_sector: Object %u already exists.\n", Entry.ObjectID);
			}
		}
		LoadingSector->Status = STATUS_LOADED;
		File.close();
		unlink(FileName);
	}catch(const char *str){
		error("FATAL ERROR in unswap_sector: Cannot read file \"%s\".\n", FileName);
		error("# Error: %s\n", str);
		abort();
	}
}

void delete_swapped_sectors(void){
	DIR *SwapDir = opendir(SAVEPATH);
	if(SwapDir == NULL){
		error("delete_swapped_sectors: Subdirectory %s not found\n", SAVEPATH);
		return;
	}

	char FileName[4096];
	while(dirent *DirEntry = readdir(SwapDir)){
		if(DirEntry->d_type != DT_REG){
			continue;
		}

		// NOTE(fusion): `DirEntry->d_name` will only contain the filename
		// so we need to assemble the actual file path (relative in this
		// case) to properly unlink it. Windows has the same behavior with
		// its `FindFirstFile`/`FindNextFile` API.
		const char *FileExt = findLast(DirEntry->d_name, '.');
		if(FileExt == NULL || strcmp(FileExt, ".swp") != 0){
			continue;
		}

		snprintf(FileName, sizeof(FileName), "%s/%s", SAVEPATH, DirEntry->d_name);
		unlink(FileName);
	}

	closedir(SwapDir);
}

void load_objects(ReadScriptFile *Script, TWriteStream *Stream, bool Skip){
	int Depth = 1;
	bool ProcessObjects = true;
	Script->read_symbol('{');
	Script->next_token();
	while(true){
		if(ProcessObjects){
			if(Script->Token != SPECIAL){
				int TypeID = Script->get_number();
				if(!object_type_exists(TypeID)){
					Script->error("unknown object type");
				}

				if(!Skip){
					Stream->writeWord((uint16)TypeID);
				}

				ProcessObjects = false;
			}else{
				char Special = Script->get_special();
				if(Special == '}'){
					if(!Skip){
						Stream->writeWord(0xFFFF);
					}

					Depth -= 1;
					ProcessObjects = false;
					if(Depth <= 0){
						break;
					}
				}else if(Special != ','){
					Script->error("expected comma");
				}
			}
			Script->next_token();
		}else{
			if(Script->Token != SPECIAL){
				int Attribute = get_instance_attribute_by_name(Script->get_identifier());
				if(Attribute == -1){
					Script->error("unknown attribute");
				}

				Script->read_symbol('=');
				if(!Skip){
					Stream->writeByte((uint8)Attribute);
				}

				if(Attribute == CONTENT){
					Script->read_symbol('{');
					Depth += 1;
					ProcessObjects = true;
				}else if(Attribute == TEXTSTRING || Attribute == EDITOR){
					const char *String = Script->read_string();
					if(!Skip){
						Stream->write_string(String);
					}
				}else{
					int Number = Script->read_number();
					if(!Skip){
						Stream->writeQuad((uint32)Number);
					}
				}

				Script->next_token();
			}else{
				// NOTE(fusion): Attributes are key-value pairs separated by whitespace.
				// If we find a special token (probably ',' or '}'), then we're done
				// parsing attributes for the current object.
				if(!Skip){
					Stream->writeByte(0xFF);
				}
				ProcessObjects = true;
			}
		}
	}
}

void load_objects(TReadStream *Stream, Object Con){
	int Depth = 1;
	Object Obj = NONE;
	while(true){
		if(Obj == NONE){
			int TypeID = (int)Stream->readWord();
			if(TypeID != 0xFFFF){
				ObjectCounter += 1;
				ObjectType ObjType(TypeID);

				if(ObjType.is_body_container()){
					Obj = get_container_object(Con, (TypeID - 1));
				}else{
					Obj = append_object(Con, ObjType);
				}
			}else{
				Obj = Con;
				Con = Con.get_container();
				Depth -= 1;
				if(Depth <= 0){
					break;
				}
			}
		}else{
			int Attribute = (int)Stream->readByte();
			if(Attribute != 0xFF){
				if(Attribute == CONTENT){
					Con = Obj;
					Obj = NONE;
					Depth += 1;
				}else if(Attribute == TEXTSTRING || Attribute == EDITOR){
					char String[4096];
					Stream->read_string(String, sizeof(String));
					Obj.set_attribute((INSTANCEATTRIBUTE)Attribute, AddDynamicString(String));
				}else if(Attribute == REMAININGEXPIRETIME){
					uint32 Value = Stream->readQuad();
					if(Value != 0){
						cron_change(Obj, (int)Value);
					}
				}else{
					uint32 Value = Stream->readQuad();
					Obj.set_attribute((INSTANCEATTRIBUTE)Attribute, Value);
				}
			}else{
				Obj = NONE;
			}
		}
	}
}

void init_sector(int SectorX, int SectorY, int SectorZ){
	ASSERT(SectorMatrix);
	if(*SectorMatrix->at(SectorX, SectorY, SectorZ) != NULL){
		error("init_sector: Sector %d/%d/%d already exists.\n", SectorX, SectorY, SectorZ);
		return;
	}

	Sector *NewSector = (Sector*)malloc(sizeof(Sector));
	for(int X = 0; X < 32; X += 1){
		for(int Y = 0; Y < 32; Y += 1){
			Object MapCon = create_object();
			// NOTE(fusion): `Attributes[0]` is probably the object id of the
			// first object in the container.
			access_object(MapCon)->Attributes[1] = SectorX * 32 + X;
			access_object(MapCon)->Attributes[2] = SectorY * 32 + Y;
			access_object(MapCon)->Attributes[3] = SectorZ;
			NewSector->MapCon[X][Y] = MapCon;
		}
	}
	NewSector->TimeStamp = RoundNr;
	NewSector->Status = STATUS_LOADED;
	NewSector->MapFlags = 0;

	*SectorMatrix->at(SectorX, SectorY, SectorZ) = NewSector;
}

void load_sector(const char *FileName, int SectorX, int SectorY, int SectorZ){
	if(SectorX < SectorXMin || SectorXMax < SectorX
			|| SectorY < SectorYMin || SectorYMax < SectorY
			|| SectorZ < SectorZMin || SectorZMax < SectorZ){
		return;
	}

	init_sector(SectorX, SectorY, SectorZ);

	ASSERT(SectorMatrix != NULL);
	Sector *LoadingSector = *SectorMatrix->at(SectorX, SectorY, SectorZ);
	ASSERT(LoadingSector != NULL);

	ReadScriptFile Script;
	try{
		Script.open(FileName);
		print(1, "Loading sector %d/%d/%d ...\n", SectorX, SectorY, SectorZ);

		int OffsetX = -1;
		int OffsetY = -1;
		while(true){
			Script.next_token();
			if(Script.Token == ENDOFFILE){
				Script.close();
				return;
			}

			if(Script.Token == SPECIAL && Script.get_special() == ','){
				continue;
			}

			if(Script.Token == BYTES){
				uint8 *SectorOffset = Script.get_bytesequence();
				OffsetX = (int)SectorOffset[0];
				OffsetY = (int)SectorOffset[1];
				Script.read_symbol(':');
				// TODO(fusion): Probably check if offsets are within bounds?
				continue;
			}

			if(Script.Token != IDENTIFIER){
				Script.error("next map point expected");
			}

			if(OffsetX == -1 || OffsetY == -1){
				Script.error("coordinate expected");
			}

			const char *Identifier = Script.get_identifier();
			if(strcmp(Identifier, "refresh") == 0){
				LoadingSector->MapFlags |= 1;
				access_object(LoadingSector->MapCon[OffsetX][OffsetY])->Attributes[3] |= 0x100;
			}else if(strcmp(Identifier, "nologout") == 0){
				LoadingSector->MapFlags |= 2;
				access_object(LoadingSector->MapCon[OffsetX][OffsetY])->Attributes[3] |= 0x200;
			}else if(strcmp(Identifier, "protectionzone") == 0){
				LoadingSector->MapFlags |= 4;
				access_object(LoadingSector->MapCon[OffsetX][OffsetY])->Attributes[3] |= 0x400;
			}else if(strcmp(Identifier, "content") == 0){
				Script.read_symbol('=');
				HelpBuffer.Position = 0;
				load_objects(&Script, &HelpBuffer, false);
				TReadBuffer ReadBuffer(HelpBuffer.Data, HelpBuffer.Position);
				load_objects(&ReadBuffer, LoadingSector->MapCon[OffsetX][OffsetY]);
			}else{
				Script.error("unknown map flag");
			}
		}
	}catch(const char *str){
		error("load_sector: Cannot read file \"%s\".\n", FileName);
		error("# Error: %s\n", str);
		throw "Cannot load sector";
	}
}

void load_map(void){
	DIR *MapDir = opendir(MAPPATH);
	if(MapDir == NULL){
		error("load_map: Subdirectory %s not found\n", MAPPATH);
		throw "Cannot load map";
	}

	print(1, "Loading map ...\n");
	ObjectCounter = 0;

	int SectorCounter = 0;
	char FileName[4096];
	while(dirent *DirEntry = readdir(MapDir)){
		if(DirEntry->d_type != DT_REG){
			continue;
		}

		// NOTE(fusion): See note in `delete_swapped_sectors`.
		const char *FileExt = findLast(DirEntry->d_name, '.');
		if(FileExt == NULL || strcmp(FileExt, ".sec") != 0){
			continue;
		}

		int SectorX, SectorY, SectorZ;
		if(sscanf(DirEntry->d_name, "%d-%d-%d.sec", &SectorX, &SectorY, &SectorZ) == 3){
			snprintf(FileName, sizeof(FileName), "%s/%s", MAPPATH, DirEntry->d_name);
			load_sector(FileName, SectorX, SectorY, SectorZ);
			SectorCounter += 1;
		}
	}

	closedir(MapDir);
	print(1, "%d sectors loaded.\n", SectorCounter);
	print(1, "%d objects loaded.\n", ObjectCounter);
}

void save_objects(Object Obj, TWriteStream *Stream, bool Stop){
	// TODO(fusion): Just use a recursive algorithm for both `save_objects` and
	// `load_objects`. Regardless of performance differences, this iterative version
	// is just unreadable and mostly disconnected.
	int Depth = 1;
	bool ProcessObjects = true;
	Object Prev = NONE;
	while(true){
		if(ProcessObjects){
			if(Obj != NONE){
				ObjectType ObjType = Obj.get_object_type();
				if(!ObjType.is_creature_container()){
					Stream->writeWord((uint16)ObjType.TypeID);
					ProcessObjects = false;
				}else{
					if(Stop && Depth == 1){
						break;
					}
					Prev = Obj;
					Obj = Obj.get_next_object();
				}
			}else{
				Stream->writeWord(0xFFFF);
				Depth -= 1;
				if(Depth <= 0){
					break;
				}

				Stream->writeByte(0xFF);
				ObjectCounter += 1;
				if(Stop && Depth == 1){
					break;
				}

				if(Prev == NONE){
					error("LastObj is NONE (1)\n");
				}

				Prev = Prev.get_container();
				if(Prev == NONE){
					error("LastObj is NONE (2)\n");
				}

				Obj = Prev.get_next_object();
			}
		}else{
			ASSERT(Obj != NONE);
			ObjectType ObjType = Obj.get_object_type();
			for(int Attribute = 1; Attribute <= 17; Attribute += 1){
				if(ObjType.get_attribute_offset((INSTANCEATTRIBUTE)Attribute) != -1){
					uint32 Value = 0;
					if(Attribute == REMAININGEXPIRETIME){
						Value = cron_info(Obj, false);
					}else{
						Value = Obj.get_attribute((INSTANCEATTRIBUTE)Attribute);
					}

					if(Value != 0){
						Stream->writeByte((uint8)Attribute);
						if(Attribute == TEXTSTRING || Attribute == EDITOR){
							Stream->write_string(GetDynamicString(Value));
						}else{
							Stream->writeQuad(Value);
						}
					}
				}
			}

			Object First = NONE;
			if(ObjType.get_attribute_offset(CONTENT) != -1){
				First = Object(Obj.get_attribute(CONTENT));
			}

			if(First != NONE){
				Depth += 1;
				Prev = NONE;
				Obj = First;
				Stream->writeByte((uint8)CONTENT);
			}else{
				Stream->writeByte(0xFF);
				ObjectCounter += 1;
				if(Stop && Depth == 1){
					break;
				}

				Prev = Obj;
				Obj = Obj.get_next_object();
			}

			ProcessObjects = true;
		}
	}
}

void save_objects(TReadStream *Stream, WriteScriptFile *Script){
	int Depth = 1;
	bool ProcessObjects = true;
	bool FirstObject = true;
	Script->write_text("{");
	while(true){
		if(ProcessObjects){
			int TypeID = (int)Stream->readWord();
			if(TypeID != 0xFFFF){
				if(!FirstObject){
					Script->write_text(", ");
				}

				Script->write_number(TypeID);
			}else{
				Script->write_text("}");
				Depth -= 1;
				if(Depth <= 0){
					break;
				}
			}

			ProcessObjects = false;
		}else{
			int Attribute = (int)Stream->readByte();
			if(Attribute != 0xFF){
				Script->write_text(" ");
				Script->write_text(get_instance_attribute_name(Attribute));
				Script->write_text("=");
				if(Attribute == CONTENT){
					Depth += 1;
					ProcessObjects = true;
					FirstObject = true;
					Script->write_text("{");
				}else if(Attribute == TEXTSTRING || Attribute == EDITOR){
					char String[4096];
					Stream->read_string(String, sizeof(String));
					Script->write_string(String);
				}else{
					Script->write_number((int)Stream->readQuad());
				}
			}else{
				ProcessObjects = true;
				FirstObject = false;
			}
		}
	}
}

void save_sector(char *FileName, int SectorX, int SectorY, int SectorZ){
	ASSERT(SectorMatrix);
	Sector *SavingSector = *SectorMatrix->at(SectorX, SectorY, SectorZ);
	if(!SavingSector){
		return;
	}

	bool empty = true;
	WriteScriptFile Script;
	try{
		Script.open(FileName);
		print(1, "Saving sector %d/%d/%d ...\n", SectorX, SectorY, SectorZ);

		Script.write_text("# Tibia - graphical Multi-User-Dungeon");
		Script.write_ln();
		Script.write_text("# Data for sector ");
		Script.write_number(SectorX);
		Script.write_text("/");
		Script.write_number(SectorY);
		Script.write_text("/");
		Script.write_number(SectorZ);
		Script.write_ln();
		Script.write_ln();

		for(int X = 0; X < 32; X += 1){
			for(int Y = 0; Y < 32; Y += 1){
				Object First = Object(SavingSector->MapCon[X][Y].get_attribute(CONTENT));
				uint8 Flags = get_map_container_flags(SavingSector->MapCon[X][Y]);
				if(First != NONE || Flags != 0){
					Script.write_number(X);
					Script.write_text("-");
					Script.write_number(Y);
					Script.write_text(": ");

					int AttrCount = 0;

					if(Flags & 1){
						if(AttrCount > 0){
							Script.write_text(", ");
						}
						Script.write_text("Refresh");
						AttrCount += 1;
					}

					if(Flags & 2){
						if(AttrCount > 0){
							Script.write_text(", ");
						}
						Script.write_text("NoLogout");
						AttrCount += 1;
					}

					if(Flags & 4){
						if(AttrCount > 0){
							Script.write_text(", ");
						}
						Script.write_text("ProtectionZone");
						AttrCount += 1;
					}

					if(First != NONE){
						if(AttrCount > 0){
							Script.write_text(", ");
						}
						Script.write_text("Content=");
						HelpBuffer.Position = 0;
						save_objects(First, &HelpBuffer, false);
						TReadBuffer ReadBuffer(HelpBuffer.Data, HelpBuffer.Position);
						save_objects(&ReadBuffer, &Script);
						AttrCount += 1;
					}

					Script.write_ln();
					empty = false;
				}
			}
		}

		Script.close();
		if(empty){
			error("save_sector: Sector %d/%d/%d is empty.\n", SectorX, SectorY, SectorZ);
			unlink(FileName);
		}
	}catch(const char *str){
		error("save_sector: Cannot write file %s.\n", FileName);
		error("# Error: %s\n", str);
	}
}

void save_map(void){
	// NOTE(fusion): I guess this could happen if we're already saving the map
	// and a signal causes `exit` to execute cleanup functions registered with
	// `atexit`, among which is `ExitAll` which may call `save_map` throught
	// `exit_map`.
	static bool SavingMap = false;
	if(SavingMap){
		error("save_map: Map is already being saved.\n");
		return;
	}

	SavingMap = true;
	print(1, "Saving map ...\n");
	ObjectCounter = 0;

	char FileName[4096];
	for(int SectorZ = SectorZMin; SectorZ <= SectorZMax; SectorZ += 1)
	for(int SectorY = SectorYMin; SectorY <= SectorYMax; SectorY += 1)
	for(int SectorX = SectorXMin; SectorX <= SectorXMax; SectorX += 1){
		snprintf(FileName, sizeof(FileName), "%s/%04d-%04d-%02d.sec",
				MAPPATH, SectorX, SectorY, SectorZ);
		save_sector(FileName, SectorX, SectorY, SectorZ);
	}

	print(1, "%d objects saved.\n", ObjectCounter);
	SavingMap = false;
}

void refresh_sector(int SectorX, int SectorY, int SectorZ, TReadStream *Stream){
	// NOTE(fusion): `matrix3d::at` would return the first entry if coordinates
	// are out of bounds which could be a problem here.
	if(SectorX < SectorXMin || SectorXMax < SectorX
			|| SectorY < SectorYMin || SectorYMax < SectorY
			|| SectorZ < SectorZMin || SectorZMax < SectorZ){
		error("refresh_sector: Sector %d/%d/%d is out of bounds.",
				SectorX, SectorY, SectorZ);
		return;
	}

	ASSERT(SectorMatrix);
	Sector *Sec = *SectorMatrix->at(SectorX, SectorY, SectorZ);
	if(Sec == NULL || (Sec->MapFlags & 0x01) == 0){
		return;
	}

	print(3, "Refreshing sector %d/%d/%d ...\n", SectorX, SectorY, SectorZ);
	try{
		while(!Stream->eof()){
			uint8 OffsetX = Stream->readByte();
			uint8 OffsetY = Stream->readByte();
			if(OffsetX < 32 && OffsetY < 32){
				Object Con = Sec->MapCon[OffsetX][OffsetY];

				// TODO(fusion): This loop was done a bit differently but I suppose
				// iterating it directly is clearer and as long as we don't access
				// the object after `delete_object`, it should work the same.
				Object Obj = Object(Con.get_attribute(CONTENT));
				while(Obj != NONE){
					Object Next = Obj.get_next_object();
					if(!Obj.get_object_type().is_creature_container()){
						delete_object(Obj);
					}
					Obj = Next;
				}

				load_objects(Stream, Con);
			}
		}
	}catch(const char *str){
		error("refresh_sector: Error reading the data (%s).\n", str);
	}
}

void patch_sector(int SectorX, int SectorY, int SectorZ, bool FullSector,
		ReadScriptFile *Script, bool SaveHouses){
	if(SectorX < SectorXMin || SectorXMax < SectorX
			|| SectorY < SectorYMin || SectorYMax < SectorY
			|| SectorZ < SectorZMin || SectorZMax < SectorZ){
		error("patch_sector: Sector %d/%d/%d is out of bounds.",
				SectorX, SectorY, SectorZ);
		return;
	}

	ASSERT(SectorMatrix);
	Sector *Sec = *SectorMatrix->at(SectorX, SectorY, SectorZ);
	bool NewSector = (Sec == NULL);
	if(NewSector){
		print(2, "Creating new sector %d/%d/%d.\n", SectorX, SectorY, SectorZ);
		init_sector(SectorX, SectorY, SectorZ);
		Sec = *SectorMatrix->at(SectorX, SectorY, SectorZ);
		ASSERT(Sec != NULL);
	}

	bool FieldTreated[32][32] = {};
	bool FieldPatched[32][32] = {};

	// NOTE(fusion): Step 1.
	//	Patch fields specified in the input script. House fields are NOT patched
	// if `SaveHouses` is set.
	{
		bool House = false;
		int OffsetX = -1;
		int OffsetY = -1;
		while(true){
			Script->next_token();
			if(Script->Token == ENDOFFILE){
				break;
			}

			if(Script->Token == SPECIAL && Script->get_special() == ','){
				continue;
			}

			if(Script->Token == BYTES){
				uint8 *SectorOffset = Script->get_bytesequence();
				OffsetX = (int)SectorOffset[0];
				OffsetY = (int)SectorOffset[1];
				Script->read_symbol(':');

				// TODO(fusion): Probably check if offsets are within bounds?
				FieldTreated[OffsetX][OffsetY] = true;
				int CoordX = SectorX * 32 + OffsetX;
				int CoordY = SectorY * 32 + OffsetY;
				int CoordZ = SectorZ;

				// TODO(fusion): Maybe some inlined function?
				House = is_house(CoordX, CoordY, CoordZ);
				if(!House && coordinate_flag(CoordX, CoordY, CoordZ, HOOKSOUTH)){
					House = is_house(CoordX - 1, CoordY + 1, CoordZ)
						||  is_house(CoordX,     CoordY + 1, CoordZ)
						||  is_house(CoordX + 1, CoordY + 1, CoordZ);
				}
				if(!House && coordinate_flag(CoordX, CoordY, CoordZ, HOOKEAST)){
					House = is_house(CoordX + 1, CoordY - 1, CoordZ)
						||  is_house(CoordX + 1, CoordY,     CoordZ)
						||  is_house(CoordX + 1, CoordY + 1, CoordZ);
				}

				if(House){
					if(SaveHouses){
						continue;
					}
					clean_house_field(CoordX, CoordY, CoordZ);
				}

				// NOTE(fusion): Similar to `refresh_sector`.
				Object Con = Sec->MapCon[OffsetX][OffsetY];
				Object Obj = Object(Con.get_attribute(CONTENT));
				while(Obj != NONE){
					Object Next = Obj.get_next_object();
					if(!Obj.get_object_type().is_creature_container()){
						delete_object(Obj);
					}
					Obj = Next;
				}
				// NOTE(fusion): Clear map container flags. See note in `get_object_coordinates`.
				access_object(Con)->Attributes[3] &= 0xFFFF00FF;
				FieldPatched[OffsetX][OffsetY] = true;
				continue;
			}

			if(Script->Token != IDENTIFIER){
				Script->error("next map point expected");
			}

			if(OffsetX == -1 || OffsetY == -1){
				Script->error("coordinate expected");
			}

			const char *Identifier = Script->get_identifier();
			if(strcmp(Identifier, "refresh") == 0){
				Sec->MapFlags |= 1;
				if(!House || !SaveHouses){
					access_object(Sec->MapCon[OffsetX][OffsetY])->Attributes[3] |= 0x100;
				}
			}else if(strcmp(Identifier, "nologout") == 0){
				Sec->MapFlags |= 2;
				if(!House || !SaveHouses){
					access_object(Sec->MapCon[OffsetX][OffsetY])->Attributes[3] |= 0x200;
				}
			}else if(strcmp(Identifier, "protectionzone") == 0){
				Sec->MapFlags |= 4;
				if(!House || !SaveHouses){
					access_object(Sec->MapCon[OffsetX][OffsetY])->Attributes[3] |= 0x400;
				}
			}else if(strcmp(Identifier, "content") == 0){
				Script->read_symbol('=');
				HelpBuffer.Position = 0;
				if(!House || !SaveHouses){
					load_objects(Script, &HelpBuffer, false);
					TReadBuffer ReadBuffer(HelpBuffer.Data, HelpBuffer.Position);
					load_objects(&ReadBuffer, Sec->MapCon[OffsetX][OffsetY]);
				}else{
					// NOTE(fusion): Skip content.
					load_objects(Script, &HelpBuffer, true);
				}
			}else{
				Script->error("unknown map flag");
			}
		}
	}

	// NOTE(fusion): Step 2.
	//	Patch fields not specified in the input script if `FullSector` is set.
	// Note that patching in this case is simply clearing the field. House fields
	// are NOT patched if `SaveHouses` is set.
	if(FullSector){
		for(int OffsetX = 0; OffsetX < 32; OffsetX += 1)
		for(int OffsetY = 0; OffsetY < 32; OffsetY += 1){
			if(FieldTreated[OffsetX][OffsetY]){
				continue;
			}

			int CoordX = SectorX * 32 + OffsetX;
			int CoordY = SectorY * 32 + OffsetY;
			int CoordZ = SectorZ;

			// TODO(fusion): Maybe some inlined function?
			bool House = is_house(CoordX, CoordY, CoordZ);
			if(!House && coordinate_flag(CoordX, CoordY, CoordZ, HOOKSOUTH)){
				House = is_house(CoordX - 1, CoordY + 1, CoordZ)
					||  is_house(CoordX,     CoordY + 1, CoordZ)
					||  is_house(CoordX + 1, CoordY + 1, CoordZ);
			}
			if(!House && coordinate_flag(CoordX, CoordY, CoordZ, HOOKEAST)){
				House = is_house(CoordX + 1, CoordY - 1, CoordZ)
					||  is_house(CoordX + 1, CoordY,     CoordZ)
					||  is_house(CoordX + 1, CoordY + 1, CoordZ);
			}

			if(House){
				if(SaveHouses){
					continue;
				}
				clean_house_field(CoordX, CoordY, CoordZ);
			}

			// NOTE(fusion): Same as in the parsing loop above.
			Object Con = Sec->MapCon[OffsetX][OffsetY];
			Object Obj = Object(Con.get_attribute(CONTENT));
			while(Obj != NONE){
				Object Next = Obj.get_next_object();
				if(!Obj.get_object_type().is_creature_container()){
					delete_object(Obj);
				}
				Obj = Next;
			}
			access_object(Con)->Attributes[3] &= 0xFFFF00FF;
			FieldPatched[OffsetX][OffsetY] = true;
		}
	}

	// NOTE(fusion): Step 3.
	//	Parse original sector file, transfering non patched fields to a new sector file.
	char FileName[4096];
	char FileNameBak[4096];
	snprintf(FileName, sizeof(FileName), "%s/%04d-%04d-%02d.sec",
			ORIGMAPPATH, SectorX, SectorY, SectorZ);
	snprintf(FileNameBak, sizeof(FileNameBak), "%s/%04d-%04d-%02d.sec~",
			ORIGMAPPATH, SectorX, SectorY, SectorZ);

	WriteScriptFile OUT;
	OUT.open(FileNameBak);
	OUT.write_text("# Tibia - graphical Multi-User-Dungeon");
	OUT.write_ln();
	OUT.write_text("# Data for sector ");
	OUT.write_number(SectorX);
	OUT.write_text("/");
	OUT.write_number(SectorY);
	OUT.write_text("/");
	OUT.write_number(SectorZ);
	OUT.write_ln();
	OUT.write_ln();

	if(!NewSector){
		ReadScriptFile IN;
		IN.open(FileName);

		int OffsetX = -1;
		int OffsetY = -1;
		int AttrCount = 0;
		while(true){
			IN.next_token();
			if(IN.Token == ENDOFFILE){
				IN.close();
				break;
			}

			if(IN.Token == SPECIAL && IN.get_special() == ','){
				continue;
			}

			if(IN.Token == BYTES){
				uint8 *SectorOffset = IN.get_bytesequence();
				OffsetX = (int)SectorOffset[0];
				OffsetY = (int)SectorOffset[1];
				IN.read_symbol(':');
				AttrCount = 0;
				// TODO(fusion): Probably check if offsets are within bounds?
				if(!FieldPatched[OffsetX][OffsetY]){
					OUT.write_number(OffsetX);
					OUT.write_text("-");
					OUT.write_number(OffsetY);
					OUT.write_text(": ");
				}
				continue;
			}

			if(IN.Token != IDENTIFIER){
				IN.error("next map point expected");
			}

			if(OffsetX == -1 || OffsetY == -1){
				IN.error("coordinate expected");
			}

			const char *Identifier = IN.get_identifier();
			if(strcmp(Identifier, "refresh") == 0){
				if(!FieldPatched[OffsetX][OffsetY]){
					if(AttrCount > 0){
						OUT.write_text(", ");
					}
					OUT.write_text("Refresh");
					AttrCount += 1;
				}
			}else if(strcmp(Identifier, "nologout") == 0){
				if(!FieldPatched[OffsetX][OffsetY]){
					if(AttrCount > 0){
						OUT.write_text(", ");
					}
					OUT.write_text("NoLogout");
					AttrCount += 1;
				}
			}else if(strcmp(Identifier, "protectionzone") == 0){
				if(!FieldPatched[OffsetX][OffsetY]){
					if(AttrCount > 0){
						OUT.write_text(", ");
					}
					OUT.write_text("ProtectionZone");
					AttrCount += 1;
				}
			}else if(strcmp(Identifier, "content") == 0){
				IN.read_symbol('=');
				HelpBuffer.Position = 0;
				load_objects(&IN, &HelpBuffer, false);
				if(!FieldPatched[OffsetX][OffsetY]){
					if(AttrCount > 0){
						OUT.write_text(", ");
					}
					OUT.write_text("Content=");
					TReadBuffer ReadBuffer(HelpBuffer.Data, HelpBuffer.Position);
					save_objects(&ReadBuffer, &OUT);
					AttrCount += 1;
				}
			}else{
				IN.error("unknown map flag");
			}
		}
	}

	// NOTE(fusion): Step 4.
	//	Transfer patched fields from memory into the new sector file.
	for(int OffsetX = 0; OffsetX < 32; OffsetX += 1)
	for(int OffsetY = 0; OffsetY < 32; OffsetY += 1){
		if(!FieldPatched[OffsetX][OffsetY]){
			continue;
		}

		Object Con = Sec->MapCon[OffsetX][OffsetY];
		Object First = Object(Con.get_attribute(CONTENT));
		uint8 Flags = get_map_container_flags(Con);
		if(First != NONE || Flags != 0){
			OUT.write_number(OffsetX);
			OUT.write_text("-");
			OUT.write_number(OffsetY);
			OUT.write_text(": ");

			int AttrCount = 0;

			if(Flags & 1){
				if(AttrCount > 0){
					OUT.write_text(", ");
				}
				OUT.write_text("Refresh");
				AttrCount += 1;
			}

			if(Flags & 2){
				if(AttrCount > 0){
					OUT.write_text(", ");
				}
				OUT.write_text("NoLogout");
				AttrCount += 1;
			}

			if(Flags & 4){
				if(AttrCount > 0){
					OUT.write_text(", ");
				}
				OUT.write_text("ProtectionZone");
				AttrCount += 1;
			}

			if(First != NONE){
				if(AttrCount > 0){
					OUT.write_text(", ");
				}
				OUT.write_text("Content=");
				HelpBuffer.Position = 0;
				save_objects(First, &HelpBuffer, false);
				TReadBuffer ReadBuffer(HelpBuffer.Data, HelpBuffer.Position);
				save_objects(&ReadBuffer, &OUT);
				AttrCount += 1;
			}

			OUT.write_ln();
		}
	}
	OUT.close();

	// NOTE(fusion): Step 5.
	//	Replace original sector file with the new one.
	if(!NewSector){
		unlink(FileName);
	}

	if(rename(FileNameBak, FileName) != 0){
		int ErrCode = errno;
		error("patch_sector: Error %d renaming %s.\n", ErrCode, FileNameBak);
		error("# Error %d: %s.\n", ErrCode, strerror(ErrCode));
		throw "cannot patch ORIGMAP";
	}
}

void init_map(void){
	ReadMapConfig();

	SectorMatrix = new matrix3d<Sector*>(SectorXMin, SectorXMax,
			SectorYMin, SectorYMax, SectorZMin, SectorZMax, NULL);

	delete_swapped_sectors();

	// NOTE(fusion): Object storage is FIXED and determined at startup.
	ObjectBlockArray = (ObjectBlock**)malloc(OBCount * sizeof(ObjectBlock*));
	for(int i = 0; i < OBCount; i += 1){
		ObjectBlockArray[i] = (ObjectBlock*)malloc(sizeof(ObjectBlock));
	}

	// NOTE(fusion): Setup free object list. See note in `GetFreeObjectSlot`.
	constexpr int ObjectsPerBlock = NARRAY(ObjectBlock::Object);
	for(int i = 0; i < OBCount; i += 1){
		for(int j = 0; j < ObjectsPerBlock; j += 1){
			*((MapObject**)&ObjectBlockArray[i]->Object[j]) = &ObjectBlockArray[i]->Object[j + 1];
		}

		if(i < (OBCount - 1)){
			// NOTE(fusion): Link last entry of this block, with the first entry of the next one.
			*((MapObject**)&ObjectBlockArray[i]->Object[ObjectsPerBlock - 1]) = &ObjectBlockArray[i + 1]->Object[0];
		}else{
			// NOTE(fusion): End of free object list.
			*((MapObject**)&ObjectBlockArray[i]->Object[ObjectsPerBlock - 1]) = NULL;
		}
	}
	FirstFreeObject = &ObjectBlockArray[0]->Object[0];

	// NOTE(fusion): Initialize object hash table.
	ASSERT(is_pow2(HashTableSize));
	HashTableMask = HashTableSize - 1;
	HashTableData = (MapObject**)malloc(HashTableSize * sizeof(MapObject*));
	HashTableType = (uint8*)malloc(HashTableSize * sizeof(uint8));
	memset(HashTableType, 0, HashTableSize * sizeof(uint8));
	HashTableFree = HashTableSize - 1;
	// NOTE(fusion): This is probably reserved for `NONE`.
	HashTableType[0] = STATUS_PERMANENT;
	HashTableData[0] = GetFreeObjectSlot();

	// NOTE(fusion): Initialize cron hash table (whatever that is).
	for(int i = 0; i < NARRAY(CronHashTable); i += 1){
		CronHashTable[i] = -1;
	}
	CronEntries = 0;

	load_map();
}

void exit_map(bool Save){
	if(Save){
		save_map();
	}

	free(HashTableData);
	free(HashTableType);

	for(int i = 0; i < OBCount; i += 1){
		free(ObjectBlockArray[i]);
	}
	free(ObjectBlockArray);

	if(SectorMatrix != NULL){
		for(int SectorZ = SectorZMin; SectorZ <= SectorZMax; SectorZ += 1)
		for(int SectorY = SectorYMin; SectorY <= SectorYMax; SectorY += 1)
		for(int SectorX = SectorXMin; SectorX <= SectorXMax; SectorX += 1){
			Sector *CurrentSector = *SectorMatrix->at(SectorX, SectorY, SectorZ);
			if(CurrentSector != NULL){
				free(CurrentSector);
			}
		}
		delete SectorMatrix;
	}

	delete_swapped_sectors();
}

// Object Functions
// =============================================================================
MapObject *access_object(Object Obj){
	if(Obj == NONE){
		error("access_object: Invalid object number zero.\n");
		return HashTableData[0];
	}

	uint32 EntryIndex = Obj.ObjectID & HashTableMask;
	if(HashTableType[EntryIndex] == STATUS_SWAPPED){
		unswap_sector((uintptr)HashTableData[EntryIndex]);
	}

	if(HashTableType[EntryIndex] == STATUS_LOADED
	&& HashTableData[EntryIndex]->ObjectID == Obj.ObjectID){
		return HashTableData[EntryIndex];
	}else{
		return HashTableData[0];
	}
}

Object create_object(void){
	static uint32 NextObjectID = 1;

	// NOTE(fusion): Load factor of 1/16.
	if(HashTableFree < (HashTableSize / 16)){
		ResizeHashTable();
	}

	// NOTE(fusion): If we properly manage the load factor and the number of free
	// entries, we should have no trouble finding an empty table entry here. Use
	// a bounded loop nevertheless, just to be safe.
	for(uint32 i = 0; i < HashTableSize; i += 1){
		if(HashTableType[NextObjectID & HashTableMask] == 0)
			break;
		NextObjectID += 1;
	}
	ASSERT(HashTableType[NextObjectID & HashTableMask] == 0);

	MapObject *Entry = GetFreeObjectSlot();
	if(Entry == NULL){
		error("create_object: Cannot create object.\n");
		return NONE;
	}

	Entry->ObjectID = NextObjectID;
	HashTableData[NextObjectID & HashTableMask] = Entry;
	HashTableType[NextObjectID & HashTableMask] = STATUS_LOADED;
	HashTableFree -= 1;
	IncrementObjectCounter();

	return Object(NextObjectID);
}

static void DestroyObject(Object Obj){
	if(!Obj.exists()){
		error("DestroyObject: Provided object does not exist.\n");
		return;
	}

	ObjectType ObjType = Obj.get_object_type();
	if(ObjType.get_flag(TEXT)){
		DeleteDynamicString(Obj.get_attribute(TEXTSTRING));
		DeleteDynamicString(Obj.get_attribute(EDITOR));
	}

	if(ObjType.get_flag(CONTAINER) || ObjType.get_flag(CHEST)){
		while(true){
			Object Inner = Object(Obj.get_attribute(CONTENT));
			if(Inner == NONE){
				break;
			}
			delete_object(Inner);
		}
	}

	if(ObjType.get_flag(EXPIRE)){
		cron_stop(Obj);
	}

	// TODO(fusion): I feel this should be checked up front?
	if(Obj == NONE){
		error("DestroyObject: Invalid object number %d.\n", NONE.ObjectID);
		return;
	}

	uint32 EntryIndex = Obj.ObjectID & HashTableMask;
	if(HashTableType[EntryIndex] != STATUS_LOADED){
		error("DestroyObject: Object is not in memory.\n");
		return;
	}

	PutFreeObjectSlot(HashTableData[EntryIndex]);
	HashTableType[EntryIndex] = STATUS_FREE;
	HashTableFree += 1;
	DecrementObjectCounter();
}

void delete_object(Object Obj){
	if(!Obj.exists()){
		error("delete_object: Provided object does not exist.\n");
		return;
	}

	cut_object(Obj);
	DestroyObject(Obj);
}

void change_object(Object Obj, ObjectType NewType){
	if(!Obj.exists()){
		error("change_object: Provided object does not exist.\n");
		return;
	}

	uint32 Amount = 0;
	uint32 SavedExpireTime = 0;
	int Delay = -1;

	ObjectType OldType = Obj.get_object_type();
	if(!OldType.is_map_container()){
		if(OldType.get_flag(CUMULATIVE)){
			Amount = Obj.get_attribute(AMOUNT);
		}

		if(OldType.get_flag(EXPIRE)){
			SavedExpireTime = cron_stop(Obj);
			if(NewType.get_flag(CUMULATIVE)){
				Amount = 1;
			}
		}

		if(OldType.get_flag(TEXT) && !NewType.get_flag(TEXT)){
			DeleteDynamicString(Obj.get_attribute(TEXTSTRING));
			Obj.set_attribute(TEXTSTRING, 0);

			DeleteDynamicString(Obj.get_attribute(EDITOR));
			Obj.set_attribute(EDITOR, 0);
		}

		if(OldType.get_flag(EXPIRESTOP)){
			if(Obj.get_attribute(SAVEDEXPIRETIME) != 0){
				Delay = (int)Obj.get_attribute(SAVEDEXPIRETIME);
			}
		}

		if(OldType.get_flag(MAGICFIELD)){
			Obj.set_attribute(RESPONSIBLE, 0);
		}
	}

	Obj.set_object_type(NewType);

	if(NewType.get_flag(CUMULATIVE)){
		if(Amount <= 0){
			Amount = 1;
		}
		Obj.set_attribute(AMOUNT, Amount);
	}

	if(NewType.get_flag(RUNE)){
		if(Obj.get_attribute(CHARGES) == 0){
			Obj.set_attribute(CHARGES, 1);
		}
	}

	if(NewType.get_flag(LIQUIDPOOL)){
		if(Obj.get_attribute(POOLLIQUIDTYPE) == 0){
			Obj.set_attribute(POOLLIQUIDTYPE, LIQUID_WATER);
		}
	}

	if(NewType.get_flag(WEAROUT)){
		if(Obj.get_attribute(REMAININGUSES) == 0){
			Obj.set_attribute(REMAININGUSES, NewType.get_attribute(TOTALUSES));
		}
	}

	if(NewType.get_flag(EXPIRESTOP)){
		Obj.set_attribute(SAVEDEXPIRETIME, SavedExpireTime);
	}

	cron_expire(Obj, Delay);
}

void change_object(Object Obj, INSTANCEATTRIBUTE Attribute, uint32 Value){
	if(!Obj.exists()){
		error("change_object: Provided object does not exist.\n");
		return;
	}

	Obj.set_attribute(Attribute, Value);
}

int get_object_priority(Object Obj){
	if(!Obj.exists()){
		error("get_object_priority: Provided object does not exist.\n");
		return -1;
	}

	int ObjPriority;
	ObjectType ObjType = Obj.get_object_type();
	if(ObjType.get_flag(BANK)){
		ObjPriority = PRIORITY_BANK;
	}else if(ObjType.get_flag(CLIP)){
		ObjPriority = PRIORITY_CLIP;
	}else if(ObjType.get_flag(BOTTOM)){
		ObjPriority = PRIORITY_BOTTOM;
	}else if(ObjType.get_flag(TOP)){
		ObjPriority = PRIORITY_TOP;
	}else if(ObjType.is_creature_container()){
		ObjPriority = PRIORITY_CREATURE;
	}else{
		ObjPriority = PRIORITY_LOW;
	}
	return ObjPriority;
}

void place_object(Object Obj, Object Con, bool Append){
	if(!Obj.exists()){
		error("place_object: Provided object does not exist.\n");
		return;
	}

	if(!Con.exists()){
		error("place_object: Provided container does not exist.\n");
		return;
	}

	ObjectType ConType = Con.get_object_type();
	if(!ConType.get_flag(CONTAINER) && !ConType.get_flag(CHEST)){
		error("place_object: Con (%d) is not a container.\n", ConType.TypeID);
		return;
	}

	Object Prev = NONE;
	Object Cur = Object(Con.get_attribute(CONTENT));
	if(ConType.is_map_container()){
		// TODO(fusion): Review. The loop below was a bit rough but it seems that
		// append is forced for non PRIORITY_CREATURE and PRIORITY_LOW.
		int ObjPriority = get_object_priority(Obj);
		Append = Append
			|| (ObjPriority != PRIORITY_CREATURE
				&& ObjPriority != PRIORITY_LOW);

		while(Cur != NONE){
			int CurPriority = get_object_priority(Cur);
			if(CurPriority == ObjPriority
					&& (ObjPriority == PRIORITY_BANK
						|| ObjPriority == PRIORITY_BOTTOM
						|| ObjPriority == PRIORITY_TOP)){
				// TODO(fusion): Replace item? I think the client might assert
				// if there are two of these objects on a single tile.
				const char *PriorityString = "";
				if(ObjPriority == PRIORITY_BANK){
					PriorityString = "BANK";
				}else if(ObjPriority == PRIORITY_BOTTOM){
					PriorityString = "BOTTOM";
				}else if(ObjPriority == PRIORITY_TOP){
					PriorityString = "TOP";
				}

				int CoordX, CoordY, CoordZ;
				get_object_coordinates(Con, &CoordX, &CoordY, &CoordZ);

				ObjectType ObjType = Obj.get_object_type();
				ObjectType CurType = Cur.get_object_type();
				error("place_object: Two %s objects (%d and %d) on field [%d,%d,%d].\n",
					PriorityString, ObjType.TypeID, CurType.TypeID, CoordX, CoordY, CoordZ);
			}

			if(Append && CurPriority > ObjPriority) break;
			if(!Append && CurPriority >= ObjPriority) break;

			Prev = Cur;
			Cur = Cur.get_next_object();
		}
	}else{
		if(Append){
			while(Cur != NONE){
				Prev = Cur;
				Cur = Cur.get_next_object();
			}
		}
	}

	if(Prev != NONE){
		Prev.set_next_object(Obj);
	}else{
		Con.set_attribute(CONTENT, Obj.ObjectID);
	}
	Obj.set_next_object(Cur);
	Obj.set_container(Con);
}

// NOTE(fusion): This is the opposite of `place_object`.
void cut_object(Object Obj){
	if(!Obj.exists()){
		error("cut_object: Provided object does not exist.\n");
		return;
	}

	Object Con = Obj.get_container();
	Object Cur = get_first_container_object(Con);
	if(Cur == Obj){
		Object Next = Obj.get_next_object();
		Con.set_attribute(CONTENT, Next.ObjectID);
	}else{
		Object Prev;
		do{
			Prev = Cur;
			Cur = Cur.get_next_object();
		}while(Cur != Obj);
		Prev.set_next_object(Cur.get_next_object());
	}

	Obj.set_next_object(NONE);
	Obj.set_container(NONE);
}

void move_object(Object Obj, Object Con){
	if(!Obj.exists()){
		error("move_object: Provided object does not exist.\n");
		return;
	}

	if(!Con.exists()){
		error("move_object: Provided destination container does not exist.\n");
		return;
	}

	cut_object(Obj);
	place_object(Obj, Con, false);
}

Object append_object(Object Con, ObjectType Type){
	if(!Con.exists()){
		error("append_object: Provided container does not exist.\n");
		return NONE;
	}

	Object Obj = create_object();
	change_object(Obj, Type);
	place_object(Obj, Con, true);
	return Obj;
}

Object set_object(Object Con, ObjectType Type, uint32 CreatureID){
	if(!Con.exists()){
		error("set_object: Provided container does not exist.\n");
		return NONE;
	}

	Object Obj = create_object();
	change_object(Obj, Type);
	place_object(Obj, Con, false);
	if(Type.is_creature_container()){
		if(CreatureID == 0){
			error("set_object: Invalid creature ID.\n");
		}
		access_object(Obj)->Attributes[1] = CreatureID;
	}
	return Obj;
}

Object copy_object(Object Con, Object Source){
	if(!Source.exists()){
		error("copy_object: Source does not exist.\n");
		return NONE;
	}

	if(!Con.exists()){
		error("copy_object: Destination container does not exist.\n");
		return NONE;
	}

	ObjectType SourceType = Source.get_object_type();
	if(SourceType.is_creature_container()){
		error("copy_object: Creatures must not be copied.\n");
		return NONE;
	}

	Object NewObj = set_object(Con, SourceType, 0);
	for(int i = 0; i < NARRAY(MapObject::Attributes); i += 1){
		access_object(NewObj)->Attributes[i] = access_object(Source)->Attributes[i];
	}

	if(SourceType.get_flag(CONTAINER) || SourceType.get_flag(CHEST)){
		NewObj.set_attribute(CONTENT, NONE.ObjectID);
	}

	if(SourceType.get_flag(TEXT)){
		// NOTE(fusion): Both `NewObj` and `Source` share the same strings. We
		// need to duplicate them so both objects can "own" and manage their own
		// strings separately.
		uint32 TextString = NewObj.get_attribute(TEXTSTRING);
		if(TextString != 0){
			TextString = AddDynamicString(GetDynamicString(TextString));
			NewObj.set_attribute(TEXTSTRING, TextString);
		}

		uint32 Editor = NewObj.get_attribute(EDITOR);
		if(Editor != 0){
			Editor = AddDynamicString(GetDynamicString(Editor));
			NewObj.set_attribute(EDITOR, Editor);
		}
	}

	return NewObj;
}

Object split_object(Object Obj, int Count){
	if(!Obj.exists()){
		error("split_object: Provided object does not exist.\n");
		return NONE;
	}

	ObjectType ObjType = Obj.get_object_type();
	if(!ObjType.get_flag(CUMULATIVE)){
		error("split_object: Object is not cumulative.\n");
		return Obj; // TODO(fusion): Probably return NONE?
	}

	uint32 Amount = Obj.get_attribute(AMOUNT);
	if(Count <= 0 || (uint32)Count > Amount){
		error("split_object: Invalid count %d.\n", Count);
		return Obj; // TODO(fusion): Probably return NONE?
	}

	Object Res = Obj;
	if((uint32)Count != Amount){
		Res = copy_object(Obj.get_container(), Obj);
		Res.set_attribute(AMOUNT, (uint32)Count);
		Obj.set_attribute(AMOUNT, Amount - (uint32)Count);
	}
	return Res;
}

void merge_objects(Object Obj, Object Dest){
	if(!Obj.exists()){
		error("merge_objects: Provided object does not exist.\n");
		return;
	}

	if(!Dest.exists()){
		error("merge_objects: Provided destination does not exist.\n");
		return;
	}

	ObjectType ObjType = Obj.get_object_type();
	ObjectType DestType = Dest.get_object_type();
	if(ObjType != DestType){
		error("merge_objects: Object types %d and %d are not identical.\n",
				ObjType.TypeID, DestType.TypeID);
		return;
	}

	if(!ObjType.get_flag(CUMULATIVE)){
		error("merge_objects: Object type %d is not cumulative.\n", ObjType.TypeID);
		return;
	}

	uint32 ObjAmount = Obj.get_attribute(AMOUNT);
	if(ObjAmount == 0){
		error("merge_objects: Object contains 0 parts.\n");
		ObjAmount = 1;
	}

	uint32 DestAmount = Dest.get_attribute(AMOUNT);
	if(DestAmount == 0){
		error("merge_objects: Destination object contains 0 parts.\n");
		DestAmount = 1;
	}

	DestAmount += ObjAmount;
	if(DestAmount > 100){
		error("merge_objects: New object contains more than 100 parts.\n");
		DestAmount = 100;
	}

	Dest.set_attribute(AMOUNT, DestAmount);
	delete_object(Obj);
}

Object get_first_container_object(Object Con){
	if(Con == NONE){
		error("get_first_container_object: Provided container does not exist.\n");
		return NONE;
	}

	ObjectType ConType = Con.get_object_type();
	if(!ConType.get_flag(CONTAINER) && !ConType.get_flag(CHEST)){
		error("get_first_container_object: Con (%d) is not a container.\n", ConType.TypeID);
		return NONE;
	}

	return Object(Con.get_attribute(CONTENT));
}

Object get_container_object(Object Con, int Index){
	if(Index < 0){
		error("get_container_object: Invalid index %d.\n", Index);
		return NONE;
	}

	if(!Con.exists()){
		error("get_container_object: Provided container does not exist.\n");
		return NONE;
	}

	Object Obj = get_first_container_object(Con);
	while(Index > 0 && Obj != NONE){
		Index -= 1;
		Obj = Obj.get_next_object();
	}

	return Obj;
}

Object get_map_container(int x, int y, int z){
	int SectorX = x / 32;
	int SectorY = y / 32;
	int SectorZ = z;

	if(SectorX < SectorXMin || SectorXMax < SectorX
			|| SectorY < SectorYMin || SectorYMax < SectorY
			|| SectorZ < SectorZMin || SectorZMax < SectorZ){
		return NONE;
	}

	ASSERT(SectorMatrix != NULL);
	Sector *ConSector = *SectorMatrix->at(SectorX, SectorY, SectorZ);
	if(ConSector == NULL){
		return NONE;
	}

	int OffsetX = x % 32;
	int OffsetY = y % 32;
	ConSector->TimeStamp = RoundNr;
	return ConSector->MapCon[OffsetX][OffsetY];
}

Object get_map_container(Object Obj){
	if(!Obj.exists()){
		error("get_map_container: Provided object does not exist\n");
		return NONE;
	}

	while(Obj != NONE){
		if(Obj.get_object_type().is_map_container())
			break;
		Obj = Obj.get_container();
	}

	return Obj;
}

Object get_first_object(int x, int y, int z){
	Object MapCon = get_map_container(x, y, z);
	if(MapCon != NONE){
		return Object(MapCon.get_attribute(CONTENT));
	}else{
		return NONE;
	}
}

Object get_first_spec_object(int x, int y, int z, ObjectType Type){
	Object Obj = get_first_object(x, y, z);
	while(Obj != NONE){
		if(Obj.get_object_type() == Type){
			break;
		}
		Obj = Obj.get_next_object();
	}
	return Obj;
}

uint8 get_map_container_flags(Object Obj){
	if(!Obj.exists() || !Obj.get_object_type().is_map_container()){
		error("get_map_container_flags: Object is not a MapContainer.\n");
		return 0;
	}

	// NOTE(fusion): See note in `get_object_coordinates`.
	return (uint8)(access_object(Obj)->Attributes[3] >> 8);
}

void get_object_coordinates(Object Obj, int *x, int *y, int *z){
	if(!Obj.exists()){
		error("get_object_coordinates: Provided object does not exist.\n");
		*x = 0;
		*y = 0;
		*z = 0;
		return;
	}

	// TODO(fusion): I'm not sure I like this approach with calling `access_object`
	// multiple times for the same object. Furthermore, using `Object::get_object_type`
	// (at least until now) is very weird when we could just get the same information
	// from the `MapObject` which we'll access ANYWAYS.

	while(true){
		if(Obj.get_object_type().is_map_container())
			break;
		Obj = Obj.get_container();
	}

	*x = access_object(Obj)->Attributes[1];
	*y = access_object(Obj)->Attributes[2];

	// NOTE(fusion): The first 8 bits of `Attributes[3]` holds the Z coordinate
	// of a map container. The next 8 bits holds its flags and the last 16 bits
	// holds its house id.
	*z = access_object(Obj)->Attributes[3] & 0xFF;
}

bool coordinate_flag(int x, int y, int z, FLAG Flag){
	bool Result = false;
	Object Obj = get_first_object(x, y, z);
	while(Obj != NONE){
		if(Obj.get_object_type().get_flag(Flag)){
			Result = true;
			break;
		}
		Obj = Obj.get_next_object();
	}
	return Result;
}

bool is_on_map(int x, int y, int z){
	int SectorX = x / 32;
	int SectorY = y / 32;
	int SectorZ = z;

	return SectorXMin <= SectorX && SectorX <= SectorXMax
		&& SectorYMin <= SectorY && SectorY <= SectorYMax
		&& SectorZMin <= SectorZ && SectorZ <= SectorZMax;
}

bool is_premium_area(int x, int y, int z){
	int SectorX = x / 32;
	int SectorY = y / 32;

	// TODO(fusion): This is checking if the position lies within certain bounding
	// boxes but it is difficult to unwind the condition due to optimizations. The
	// best way to understand the outline of the region is to plot this function.
	bool Result = true;
	if(SectorX < 0x40C
	&& (SectorX < 0x408 || SectorY > 0x3E6)
	&& (SectorX < 0x406 || SectorY < 0x3F0)
	&& (SectorX < 0x405 || SectorY < 0x3F1)
	&& (SectorX < 0x3FE || SectorY < 0x3F6)){
		Result = SectorY > 0x3F7;
	}
	return Result;
}

bool is_no_logout_field(int x, int y, int z){
	bool Result = false;
	Object Con = get_map_container(x, y, z);
	if(Con != NONE){
		Result = (get_map_container_flags(Con) & 0x02) != 0;
	}
	return Result;
}

bool is_protection_zone(int x, int y, int z){
	bool Result = false;
	Object Con = get_map_container(x, y, z);
	if(Con != NONE){
		Result = (get_map_container_flags(Con) & 0x04) != 0;
	}
	return Result;
}

bool is_house(int x, int y, int z){
	return get_house_id(x, y, z) != 0;
}

uint16 get_house_id(int x, int y, int z){
	Object Con = get_map_container(x, y, z);
	if(Con == NONE){
		return 0;
	}

	if(!Con.exists()){
		error("get_house_id: Map container for point [%d,%d,%d] does not exist.\n", x, y, z);
		return 0;
	}

	// NOTE(fusion): See note in `get_object_coordinates`.
	return (uint16)(access_object(Con)->Attributes[3] >> 16);
}

void set_house_id(int x, int y, int z, uint16 ID){
	if(!is_on_map(x, y, z)){
		return;
	}

	Object Con = get_map_container(x, y, z);
	if(Con == NONE || !Con.exists()){
		error("set_house_id: Map container for point [%d,%d,%d] does not exist.\n", x, y, z);
		return;
	}

	// NOTE(fusion): See note in `get_object_coordinates`.
	uint16 PrevID = (uint16)(access_object(Con)->Attributes[3] >> 16);
	if(PrevID != 0){
		error("set_house_id: Field [%d,%d,%d] already belongs to a house.\n", x, y, z);
		return;
	}

	access_object(Con)->Attributes[3] |= ((uint32)ID << 16);
}

int get_depot_number(const char *Town){
	if(Town == NULL){
		error("get_depot_number: Town is NULL.\n");
		return -1;
	}

	for(int DepotNumber = DepotInfoArray.min;
			DepotNumber <= DepotInfoArray.max;
			DepotNumber += 1){
		if(stricmp(DepotInfoArray.at(DepotNumber)->Town, Town) == 0){
			return DepotNumber;
		}
	}
	return -1;
}

const char *get_depot_name(int DepotNumber){
	if(DepotNumber < DepotInfoArray.min || DepotNumber > DepotInfoArray.max){
		error("get_depot_name: Invalid name for depot %d.\n", DepotNumber);
		return "unknown";
	}

	return DepotInfoArray.at(DepotNumber)->Town;
}

int get_depot_size(int DepotNumber, bool PremiumAccount){
	if(DepotNumber < DepotInfoArray.min || DepotNumber > DepotInfoArray.max){
		error("get_depot_size: Invalid depot number %d.\n", DepotNumber);
		return 1;
	}

	DepotInfo *Info = DepotInfoArray.at(DepotNumber);
	if(Info->Size < 1){
		error("get_depot_size: Invalid depot size %d for depot %d.\n", Info->Size, DepotNumber);
		Info->Size = 1;
	}

	int Size = Info->Size;
	if(PremiumAccount){
		Size *= 2;
	}

	return Size;
}

bool get_mark_position(const char *Name, int *x, int *y, int *z){
	bool Result = false;
	for(int i = 0; i < Marks; i += 1){
		Mark *MarkPointer = MarkArray.at(i);
		if(stricmp(MarkPointer->Name, Name) == 0){
			*x = MarkPointer->x;
			*y = MarkPointer->y;
			*z = MarkPointer->z;
			Result = true;
			break;
		}
	}
	return Result;
}

void get_start_position(int *x, int *y, int *z, bool Newbie){
	if(Newbie){
		*x = NewbieStartPositionX;
		*y = NewbieStartPositionY;
		*z = NewbieStartPositionZ;
	}else{
		*x = VeteranStartPositionX;
		*y = VeteranStartPositionY;
		*z = VeteranStartPositionZ;
	}
}
