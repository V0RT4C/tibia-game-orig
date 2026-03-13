#include "game_data/moveuse/moveuse.h"
#include "config.h"
#include "houses.h"
#include "info.h"
#include "magic.h"
#include "operate.h"
#include "reader.h"
#include "writer.h"

static vector<MoveUseCondition> MoveUseConditions(1, 1000, 1000);
static int NumberOfMoveUseConditions;

static vector<MoveUseAction> MoveUseActions(1, 1000, 1000);
static int NumberOfMoveUseActions;

static MoveUseDatabase MoveUseDatabases[5];

static vector<DelayedMail> DelayedMailArray(0, 10, 10);
static int DelayedMails;

// Coordinate Packing
// =============================================================================
int pack_absolute_coordinate(int x, int y, int z){
	// DOMAIN: [24576, 40959] x [24576, 40959] x [0, 15]
	int Packed = (((x - 24576) & 0x00003FFF) << 18)
				| (((y - 24576) & 0x00003FFF) << 4)
				| (z & 0x0000000F);
	return Packed;
}

void unpack_absolute_coordinate(int Packed, int *x, int *y, int *z){
	*x = ((Packed >> 18) & 0x00003FFF) + 24576;
	*y = ((Packed >>  4) & 0x00003FFF) + 24576;
	*z = ((Packed >>  0) & 0x0000000F);
}

int pack_relative_coordinate(int x, int y, int z){
	// DOMAIN: [-8192, 8191] x [-8192, 8191] x [-8, 7]
	int Packed = (((x + 8192) & 0x00003FFF) << 18)
				| (((y + 8192) & 0x00003FFF) << 4)
				| ((z + 8) & 0x0000000F);
	return Packed;
}

void unpack_relative_coordinate(int Packed, int *x, int *y, int *z){
	*x = ((Packed >> 18) & 0x00003FFF) - 8192;
	*y = ((Packed >>  4) & 0x00003FFF) - 8192;
	*z = ((Packed >>  0) & 0x0000000F) - 8;
}

// Event Execution
// =============================================================================
Object get_event_object(int Nr, Object User, Object Obj1, Object Obj2, Object Temp){
	Object Obj = NONE;
	switch(Nr){
		case 0: Obj = NONE; break;
		case 1: Obj = Obj1; break;
		case 2: Obj = Obj2; break;
		case 3: Obj = User; break;
		case 4: Obj = Temp; break;
		default:{
			error("get_event_object: Unknown number %d.\n", Nr);
			break;
		}
	}
	return Obj;
}

bool compare(int Value1, int Operator, int Value2){
	bool Result = false;
	switch(Operator){
		case '<': Result = (Value1 <  Value2); break;
		case '>': Result = (Value1 >  Value2); break;
		case '=': Result = (Value1 == Value2); break;
		case 'N': Result = (Value1 != Value2); break;
		case 'L': Result = (Value1 <= Value2); break;
		case 'G': Result = (Value1 >= Value2); break;
		default:{
			error("compare: Invalid operator %d.\n", Operator);
			break;
		}
	}
	return Result;
}

bool check_condition(MoveUseEventType EventType, MoveUseCondition *Condition,
		Object User, Object Obj1, Object Obj2, Object *Temp){
	bool Result = false;
	switch(Condition->Condition){
		case MOVEUSE_CONDITION_ISPOSITION:{
			int ObjX, ObjY, ObjZ, CoordX, CoordY, CoordZ;
			Object Obj = get_event_object(Condition->Parameters[0], User, Obj1, Obj2, *Temp);
			unpack_absolute_coordinate(Condition->Parameters[1], &CoordX, &CoordY, &CoordZ);
			get_object_coordinates(Obj, &ObjX, &ObjY, &ObjZ);
			Result = (ObjX == CoordX && ObjY == CoordY && ObjZ == CoordZ);
			break;
		}

		case MOVEUSE_CONDITION_ISTYPE:{
			Object Obj = get_event_object(Condition->Parameters[0], User, Obj1, Obj2, *Temp);
			Result = (Obj.get_object_type().TypeID == Condition->Parameters[1]);
			break;
		}

		case MOVEUSE_CONDITION_ISCREATURE:{
			Object Obj = get_event_object(Condition->Parameters[0], User, Obj1, Obj2, *Temp);
			Result = Obj.get_object_type().is_creature_container();
			break;
		}

		case MOVEUSE_CONDITION_ISPLAYER:{
			Object Obj = get_event_object(Condition->Parameters[0], User, Obj1, Obj2, *Temp);
			if(Obj.get_object_type().is_creature_container()){
				TCreature *Creature = GetCreature(Obj);
				Result = (Creature && Creature->Type == PLAYER);
			}
			break;
		}

		case MOVEUSE_CONDITION_HASFLAG:{
			Object Obj = get_event_object(Condition->Parameters[0], User, Obj1, Obj2, *Temp);
			Result = Obj.get_object_type().get_flag((FLAG)Condition->Parameters[1]);
			break;
		}

		case MOVEUSE_CONDITION_HASTYPEATTRIBUTE:{
			Object Obj = get_event_object(Condition->Parameters[0], User, Obj1, Obj2, *Temp);
			int Attribute = (int)Obj.get_object_type().get_attribute((TYPEATTRIBUTE)Condition->Parameters[1]);
			int Operator = Condition->Parameters[2];
			int Value = Condition->Parameters[3];
			Result = compare(Attribute, Operator, Value);
			break;
		}

		case MOVEUSE_CONDITION_HASINSTANCEATTRIBUTE:{
			Object Obj = get_event_object(Condition->Parameters[0], User, Obj1, Obj2, *Temp);
			int Attribute = (int)Obj.get_attribute((INSTANCEATTRIBUTE)Condition->Parameters[1]);
			int Operator = Condition->Parameters[2];
			int Value = Condition->Parameters[3];
			Result = compare(Attribute, Operator, Value);
			break;
		}

		case MOVEUSE_CONDITION_HASTEXT:{
			Object Obj = get_event_object(Condition->Parameters[0], User, Obj1, Obj2, *Temp);
			const char *Attribute = GetDynamicString(Obj.get_attribute(TEXTSTRING));
			const char *Text = GetDynamicString(Condition->Parameters[1]);
			Result = (strcmp(Attribute, Text) == 0);
			break;
		}

		case MOVEUSE_CONDITION_ISPEACEFUL:{
			Object Obj = get_event_object(Condition->Parameters[0], User, Obj1, Obj2, *Temp);
			if(Obj.get_object_type().is_creature_container()){
				TCreature *Creature = GetCreature(Obj);
				Result = (Creature && Creature->IsPeaceful());
			}
			break;
		}

		case MOVEUSE_CONDITION_MAYLOGOUT:{
			Object Obj = get_event_object(Condition->Parameters[0], User, Obj1, Obj2, *Temp);
			if(Obj.get_object_type().is_creature_container()){
				TCreature *Creature = GetCreature(Obj);
				Result = (Creature != NULL && Creature->Type == PLAYER
					&& Creature->LogoutPossible() == 0);
			}
			break;
		}

		case MOVEUSE_CONDITION_HASPROFESSION:{
			Object Obj = get_event_object(Condition->Parameters[0], User, Obj1, Obj2, *Temp);
			int Profession = Condition->Parameters[1];
			if(Obj.get_object_type().is_creature_container()){
				TCreature *Creature = GetCreature(Obj);
				Result = (Creature != NULL && Creature->Type == PLAYER
					&& ((TPlayer*)Creature)->GetEffectiveProfession() == Profession);
			}
			break;
		}

		case MOVEUSE_CONDITION_HASLEVEL:{
			Object Obj = get_event_object(Condition->Parameters[0], User, Obj1, Obj2, *Temp);
			int Operator = Condition->Parameters[1];
			int Value = Condition->Parameters[2];
			if(Obj.get_object_type().is_creature_container()){
				TCreature *Creature = GetCreature(Obj);
				if(Creature != NULL){
					int Level = Creature->Skills[SKILL_LEVEL]->Get();
					Result = compare(Level, Operator, Value);
				}
			}
			break;
		}

		case MOVEUSE_CONDITION_HASRIGHT:{
			Object Obj = get_event_object(Condition->Parameters[0], User, Obj1, Obj2, *Temp);
			RIGHT Right = (RIGHT)Condition->Parameters[1];
			if(Obj.get_object_type().is_creature_container()){
				TCreature *Creature = GetCreature(Obj);
				Result = (Creature != NULL && Creature->Type == PLAYER
					&& check_right(Creature->ID, Right));
			}
			break;
		}

		case MOVEUSE_CONDITION_HASQUESTVALUE:{
			Object Obj = get_event_object(Condition->Parameters[0], User, Obj1, Obj2, *Temp);
			int QuestNr = Condition->Parameters[1];
			int Operator = Condition->Parameters[2];
			int Value = Condition->Parameters[3];
			if(Obj.get_object_type().is_creature_container()){
				TCreature *Creature = GetCreature(Obj);
				if(Creature != NULL && Creature->Type == PLAYER){
					int QuestValue = ((TPlayer*)Creature)->GetQuestValue(QuestNr);
					Result = compare(QuestValue, Operator, Value);
				}
			}
			break;
		}

		case MOVEUSE_CONDITION_TESTSKILL:{
			Object Obj = get_event_object(Condition->Parameters[0], User, Obj1, Obj2, *Temp);
			int SkillNr = Condition->Parameters[1];
			int Difficulty = Condition->Parameters[2];
			int Probability = Condition->Parameters[3];
			if(Obj.get_object_type().is_creature_container()){
				TCreature *Creature = GetCreature(Obj);
				if(Creature != NULL){
					if(SkillNr >= 0 && SkillNr < NARRAY(Creature->Skills)){
						Result = Creature->Skills[SkillNr]->Probe(Difficulty, Probability, true);
					}else{
						error("check_condition (TestSkill): Invalid skill number %d.\n", SkillNr);
					}
				}
			}
			break;
		}

		// TODO(fusion): This also counts objects at the object's map location?
		case MOVEUSE_CONDITION_COUNTOBJECTS:{
			Object Obj = get_event_object(Condition->Parameters[0], User, Obj1, Obj2, *Temp);
			int Operator = Condition->Parameters[1];
			int Value = Condition->Parameters[2];
			Object MapCon = get_map_container(Obj);
			int ObjCount = count_objects_in_container(MapCon);
			Result = compare(ObjCount, Operator, Value);
			break;
		}

		case MOVEUSE_CONDITION_COUNTOBJECTSONMAP:{
			int CoordX, CoordY, CoordZ;
			unpack_absolute_coordinate(Condition->Parameters[0], &CoordX, &CoordY, &CoordZ);
			int Operator = Condition->Parameters[1];
			int Value = Condition->Parameters[2];
			Object MapCon = get_map_container(CoordX, CoordY, CoordZ);
			int ObjCount = count_objects_in_container(MapCon);
			Result = compare(ObjCount, Operator, Value);
			break;
		}

		case MOVEUSE_CONDITION_ISOBJECTTHERE:{
			int CoordX, CoordY, CoordZ;
			unpack_absolute_coordinate(Condition->Parameters[0], &CoordX, &CoordY, &CoordZ);
			ObjectType Type = Condition->Parameters[1];
			*Temp = get_first_spec_object(CoordX, CoordY, CoordZ, Type);
			Result = (*Temp != NONE);
			break;
		}

		case MOVEUSE_CONDITION_ISCREATURETHERE:{
			int CoordX, CoordY, CoordZ;
			unpack_absolute_coordinate(Condition->Parameters[0], &CoordX, &CoordY, &CoordZ);
			ObjectType Type = TYPEID_CREATURE_CONTAINER;
			*Temp = get_first_spec_object(CoordX, CoordY, CoordZ, Type);
			Result = (*Temp != NONE);
			break;
		}

		// TODO(fusion): I feel this one should iterate the field because there
		// could be more than one creature.
		case MOVEUSE_CONDITION_ISPLAYERTHERE:{
			int CoordX, CoordY, CoordZ;
			unpack_absolute_coordinate(Condition->Parameters[0], &CoordX, &CoordY, &CoordZ);
			ObjectType Type = TYPEID_CREATURE_CONTAINER;
			*Temp = get_first_spec_object(CoordX, CoordY, CoordZ, Type);
			if(*Temp != NONE){
				TCreature *Creature = GetCreature(*Temp);
				Result = Creature && Creature->Type == PLAYER;
			}
			break;
		}

		case MOVEUSE_CONDITION_ISOBJECTININVENTORY:{
			Object Obj = get_event_object(Condition->Parameters[0], User, Obj1, Obj2, *Temp);
			ObjectType Type = Condition->Parameters[1];
			int Count = Condition->Parameters[2];
			if(Obj.get_object_type().is_creature_container()){
				*Temp = get_inventory_object(Obj.get_creature_id(), Type, Count);
				Result = (*Temp != NONE);
			}
			break;
		}

		case MOVEUSE_CONDITION_ISPROTECTIONZONE:{
			int ObjX, ObjY, ObjZ;
			Object Obj = get_event_object(Condition->Parameters[0], User, Obj1, Obj2, *Temp);
			get_object_coordinates(Obj, &ObjX, &ObjY, &ObjZ);
			Result = is_protection_zone(ObjX, ObjY, ObjZ);
			break;
		}

		case MOVEUSE_CONDITION_ISHOUSE:{
			int ObjX, ObjY, ObjZ;
			Object Obj = get_event_object(Condition->Parameters[0], User, Obj1, Obj2, *Temp);
			get_object_coordinates(Obj, &ObjX, &ObjY, &ObjZ);
			Result = is_house(ObjX, ObjY, ObjZ);
			break;
		}

		case MOVEUSE_CONDITION_ISHOUSEOWNER:{
			Object Obj = get_event_object(Condition->Parameters[0], User, Obj1, Obj2, *Temp);
			Object Cr = get_event_object(Condition->Parameters[1], User, Obj1, Obj2, *Temp);
			if(Cr.get_object_type().is_creature_container()){
				TCreature *Creature = GetCreature(Cr);
				if(Creature != NULL && Creature->Type == PLAYER){
					int ObjX, ObjY, ObjZ;
					get_object_coordinates(Obj, &ObjX, &ObjY, &ObjZ);
					uint16 HouseID = get_house_id(ObjX, ObjY, ObjZ);
					Result = is_owner(HouseID, (TPlayer*)Creature);
				}
			}
			break;
		}

		case MOVEUSE_CONDITION_ISDRESSED:{
			Object Obj = get_event_object(Condition->Parameters[0], User, Obj1, Obj2, *Temp);
			ObjectType ObjType = Obj.get_object_type();
			if(ObjType.get_flag(CLOTHES)){
				int CurPosition = get_object_body_position(Obj);
				int ObjPosition = (int)ObjType.get_attribute(BODYPOSITION);
				Result = (CurPosition == ObjPosition);
			}else{
				error("check_condition (IsDressed): Object is not a clothing item.\n");
			}
			break;
		}

		case MOVEUSE_CONDITION_RANDOM:{
			Result = (random(1, 100) <= Condition->Parameters[0]);
			break;
		}

		default:{
			error("check_condition: Unknown condition %d.\n", Condition->Condition);
			return false;
		}
	}

	if(Condition->Modifier == MOVEUSE_MODIFIER_NORMAL){
		// no-op
	}else if(Condition->Modifier == MOVEUSE_MODIFIER_INVERT){
		Result = !Result;
	}else if(Condition->Modifier == MOVEUSE_MODIFIER_TRUE){
		Result = true;
	}else{
		// TODO(fusion): Error?
		Result = false;
	}

	return Result;
}

Object create_object(Object Con, ObjectType Type, uint32 Value){
	for(int Attempt = 0; true; Attempt += 1){
		try{
			Object Obj = NONE;
			ObjectType ConType = Con.get_object_type();
			if(ConType.is_body_container()){
				uint32 CreatureID = get_object_creature_id(Con);
				Obj = create_at_creature(CreatureID, Type, Value);
			}else{
				Obj = create(Con, Type, Value);
			}
			return Obj;
		}catch(RESULT r){
			if(Attempt == 0 && (r == NOTTAKABLE || r == TOOHEAVY || r == CONTAINERFULL)){
				// NOTE(fusion): Try to create object on the map, ONCE.
				Con = get_map_container(Con);
			}else{
				int ConX, ConY, ConZ;
				get_object_coordinates(Con, &ConX, &ConY, &ConZ);
				error("moveuse::create_object: Exception %d, object %d, position [%d,%d,%d].\n",
						r, Type.TypeID, ConX, ConY, ConZ);
				return NONE;
			}
		}
	}
}

void change_object(Object Obj, ObjectType NewType, uint32 Value){
	if(!Obj.exists()){
		error("change_object: Provided object does not exist (1, NewType=%d).\n", NewType.TypeID);
		return;
	}

	// NOTE(fusion): `change` requires CUMULATIVE objects to have AMOUNT
	// equal to ONE or it fails with `SPLITOBJECT`. This means might need
	// to split the object and the split destination may change depending
	// on how things go.
	Object SplitDest = Obj.get_container();
	for(int Attempt = 0; true; Attempt += 1){
		if(!Obj.exists()){
			error("change_object: Provided object does not exist (2, NewType=%d).\n", NewType.TypeID);
			return;
		}

		try{
			ObjectType ObjType = Obj.get_object_type();
			if(ObjType.get_flag(CUMULATIVE)){
				uint32 Amount = Obj.get_attribute(AMOUNT);
				if(Amount > 1){
					move(0, Obj, SplitDest, Amount - 1, true, NONE);
				}
			}

			change(Obj, NewType, Value);
			return;
		}catch(RESULT r){
			if(!Obj.exists()){
				error("change_object: Provided object does not exist (3, NewType=%d).\n", NewType.TypeID);
				return;
			}

			// TODO(fusion): I feel a couple of attempts should be enough?
			if(Attempt >= 2){
				throw;
			}

			if(r == CONTAINERFULL){
				// NOTE(fusion): Set `SplitDest` to map.
				SplitDest = get_map_container(Obj);
			}else if(r == TOOHEAVY){
				// NOTE(fusion): move object to map, keep `SplitDest`.
				Object MapCon = get_map_container(Obj);
				move(0, Obj, MapCon, -1, true, NONE);
			}else if(r == NOROOM
					&& Obj.get_object_type().get_flag(CUMULATIVE)
					&& Obj.get_attribute(AMOUNT) > 1){
				// NOTE(fusion): Split to map directly. Similar to setting `SplitDest`
				// to map, although failing here will throw us out of the function.
				uint32 Amount = Obj.get_attribute(AMOUNT);
				Object MapCon = get_map_container(SplitDest);
				move(0, Obj, MapCon, Amount - 1, true, NONE);
			}else{
				throw;
			}
		}
	}
}

void move_one_object(Object Obj, Object Con){
	if(!Obj.exists()){
		error("move_one_object: Provided object does not exist.\n");
		return;
	}

	if(!Con.exists()){
		error("move_one_object: Provided container does not exist.\n");
		return;
	}

	ObjectType ConType = Con.get_object_type();
	if(!ConType.get_flag(CONTAINER)){
		error("move_one_object: \"Con\" is not a container.\n");
		return;
	}

	ObjectType ObjType = Obj.get_object_type();
	if(ObjType.is_creature_container() && !ConType.is_map_container()){
		error("move_one_object: \"Con\" is not a map container.\n");
		return;
	}

	try{
		if(ObjType.get_flag(LIQUIDPOOL) || ObjType.get_flag(MAGICFIELD)){
			delete_op(Obj, -1);
		}else{
			move(0, Obj, Con, -1, true, NONE);
		}
	}catch(RESULT r){
		if(r != DESTROYED){
			int ConX, ConY, ConZ;
			get_object_coordinates(Con, &ConX, &ConY, &ConZ);
			error("move_one_object: Exception %d; coordinate [%d,%d,%d].\n",
					r, ConX, ConY, ConZ);
		}

		if(r == CONTAINERFULL){
			delete_op(Obj, -1);
		}
	}
}

void move_all_objects(Object Obj, Object Dest, Object Exclude, bool MoveUnmovable){
	if(Obj == NONE){
		return;
	}

	if(!Dest.get_object_type().is_map_container()){
		error("move_all_objects: \"Dest\" is not a map container.\n");
		return;
	}

	// TODO(fusion): We probably use recursion to move objects in reverse order?
	move_all_objects(Obj.get_next_object(), Dest, Exclude, MoveUnmovable);

	if(Obj != Exclude){
		ObjectType ObjType = Obj.get_object_type();
		try{
			if(ObjType.get_flag(LIQUIDPOOL) || ObjType.get_flag(MAGICFIELD)){
				delete_op(Obj, -1);
			}else if(MoveUnmovable || !ObjType.get_flag(UNMOVE)){
				move(0, Obj, Dest, -1, true, NONE);
			}
		}catch(RESULT r){
			if(r != DESTROYED){
				int DestX, DestY, DestZ;
				get_object_coordinates(Dest, &DestX, &DestY, &DestZ);
				error("move_all_objects: Exception %d; object %d, destination coordinate [%d,%d,%d].\n",
						r, ObjType.TypeID, DestX, DestY, DestZ);
			}
		}
	}
}

void delete_all_objects(Object Obj, Object Exclude, bool DeleteUnmovable){
	if(Obj == NONE){
		return;
	}

	// TODO(fusion): Same as `move_all_objects`.
	delete_all_objects(Obj.get_next_object(), Exclude, DeleteUnmovable);

	if(Obj != Exclude){
		ObjectType ObjType = Obj.get_object_type();
		if(ObjType.is_creature_container()){
			int ObjX, ObjY, ObjZ;
			get_object_coordinates(Obj, &ObjX, &ObjY, &ObjZ);
			error("delete_all_objects: A creature at position [%d,%d,%d] is about to be deleted.\n",
					ObjX, ObjY, ObjZ);
			return;
		}

		if(!DeleteUnmovable && ObjType.get_flag(UNMOVE)
				&& !ObjType.get_flag(LIQUIDPOOL)
				&& !ObjType.get_flag(MAGICFIELD)){
			return;
		}

		try{
			delete_op(Obj, -1);
		}catch(RESULT r){
			error("delete_all_objects: Exception %d.\n", r);
		}
	}
}

void clear_field(Object Obj, Object Exclude){
	// IMPORTANT(fusion): This function seems to be used when items transform
	// into `UNPASS` versions of themselves, like doors, which is why we skip
	// the actual object when calling `move_all_objects` at the end.
	//	The order we scan available fields may also be important which is why
	// I've kept the same ordering used by the original function.

	if(!Obj.exists()){
		error("clear_field: Object does not exist.\n");
		return;
	}

	int ObjX, ObjY, ObjZ;
	get_object_coordinates(Obj, &ObjX, &ObjY, &ObjZ);

	bool DestFound = false;
	int DestX, DestY, DestZ;
	int OffsetX[4] = { 1,  0, -1,  0};
	int OffsetY[4] = { 0,  1,  0, -1};

	// NOTE(fusion): First look for a field that's not blocked.
	for(int i = 0; i < 4; i += 1){
		DestX = ObjX + OffsetX[i];
		DestY = ObjY + OffsetY[i];
		DestZ = ObjZ;
		if(coordinate_flag(DestX, DestY, DestZ, BANK)
		&& !coordinate_flag(DestX, DestY, DestZ, UNPASS)){
			DestFound = true;
			break;
		}
	}

	// NOTE(fusion): Then look for a field where jumping is possible.
	if(!DestFound){
		for(int i = 0; i < 4; i += 1){
			DestX = ObjX + OffsetX[i];
			DestY = ObjY + OffsetY[i];
			DestZ = ObjZ;
			if(jump_possible(DestX, DestY, DestZ, false)){
				DestFound = true;
				break;
			}
		}
	}

	if(DestFound){
		Object Dest = get_map_container(DestX, DestY, DestZ);
		move_all_objects(Obj.get_next_object(), Dest, Exclude, true);
	}
}

void load_depot_box(uint32 CreatureID, int Nr, Object Con){
	TPlayer *Player = GetPlayer(CreatureID);
	if(Player == NULL){
		error("moveuse::load_depot_box: Creature not found.\n");
		return;
	}

	if(!Con.exists()){
		error("moveuse::load_depot_box: Provided container does not exist.\n");
		return;
	}

	if(!Con.get_object_type().get_flag(CONTAINER)){
		error("moveuse::load_depot_box: Provided object is not a container.\n");
		return;
	}

	// NOTE(fusion): Holy fuck. Objects are actually loaded into the depot box.
	LoadDepot(Player->PlayerData, Nr, Con);

	int DepotObjects = count_objects(Con) - 1;
	int DepotCapacity = get_depot_size(Nr, check_right(Player->ID, PREMIUM_ACCOUNT));
	int DepotSpace = DepotCapacity - DepotObjects;
	Player->Depot = Con;
	Player->DepotNr = Nr;
	Player->DepotSpace = DepotSpace;

	print(3, "Depot of %s has %d free slots.\n", Player->Name, DepotSpace);
	SendMessage(Player->Connection, TALK_STATUS_MESSAGE,
			"Your depot contains %d item%s.",
			DepotObjects, (DepotObjects != 1 ? "s" : ""));

	if(DepotSpace <= 0){
		SendMessage(Player->Connection, TALK_INFO_MESSAGE,
				"Your depot is full. Remove surplus items before storing new ones.");
	}
}

void save_depot_box(uint32 CreatureID, int Nr, Object Con){
	TPlayer *Player = GetPlayer(CreatureID);
	if(Player == NULL){
		error("moveuse::save_depot_box: Creature not found.\n");
		return;
	}

	if(!Con.exists()){
		error("moveuse::save_depot_box: Provided container does not exist.\n");
		return;
	}

	if(!Con.get_object_type().get_flag(CONTAINER)){
		error("moveuse::save_depot_box: Provided object is not a container.\n");
		return;
	}

	int DepotObjects = count_objects(Con) - 1;
	log_message("game", "Saving depot %d of %s ... depot size: %d.\n",
			Nr, Player->Name, DepotObjects);
	print(3, "Depot of %s: calculated free slots %d, actual objects %d.\n",
			Player->Name, Player->DepotSpace, DepotObjects);

	SaveDepot(Player->PlayerData, Nr, Con);
	Player->Depot = NONE;

	Object Obj = get_first_container_object(Con);
	while(Obj != NONE){
		Object Next = Obj.get_next_object();
		delete_op(Obj, -1);
		Obj = Next;
	}
}

// TODO(fusion): Maybe move this to `strings.cc` or `utils.cc`?
static int ReadLine(char *Dest, int DestCapacity, const char *Text, int ReadPos){
	ASSERT(DestCapacity > 0);
	int WritePos = 0;
	while(Text[ReadPos] != 0 && Text[ReadPos] != '\n'){
		if(WritePos < (DestCapacity - 1)){
			Dest[WritePos] = Text[ReadPos];
			WritePos += 1;
		}
		ReadPos += 1;
	}

	if(Text[ReadPos] == '\n'){
		ReadPos += 1;
	}

	Dest[WritePos] = 0;
	return ReadPos;
}

void send_mail(Object Obj){
	if(!Obj.exists()){
		error("send_mail: Provided object does not exist.\n");
		return;
	}

	ObjectType ObjType = Obj.get_object_type();
	if(ObjType != get_special_object(PARCEL_NEW)
	&& ObjType != get_special_object(LETTER_NEW)){
		return;
	}

	print(3, "Sending mail...\n");

	uint32 TextAttr = 0;
	if(ObjType == get_special_object(LETTER_NEW)){
		TextAttr = Obj.get_attribute(TEXTSTRING);
	}else{
		ObjectType LabelType = get_special_object(PARCEL_LABEL);
		Object Label = get_first_container_object(Obj);
		while(Label != NONE){
			if(Label.get_object_type() == LabelType){
				break;
			}
			Label = Label.get_next_object();
		}

		if(Label == NONE){
			return;
		}

		TextAttr = Label.get_attribute(TEXTSTRING);
	}

	const char *Text = GetDynamicString(TextAttr);
	if(Text == NULL || Text[0] == 0){
		return;
	}

	char Addressee[256];
	char Town[256];
	{
		int ReadPos = 0;
		ReadPos = ReadLine(Addressee, sizeof(Addressee), Text, ReadPos);
		ReadPos = ReadLine(Town,      sizeof(Town),      Text, ReadPos);
		Trim(Addressee);
		Trim(Town);
	}

	if(Addressee[0] == 0 || Town[0] == 0){
		return;
	}

	// TODO(fusion): This constant for the size of creature names showing up again.
	// Maybe `MAX_NAME_LENGTH`?
	if(strlen(Addressee) >= 30){
		return;
	}

	log_message("game", "Mail to %s in %s.\n", Addressee, Town);
	int DepotNr = get_depot_number(Town);
	if(DepotNr == -1){
		return;
	}

	TPlayer *Player;
	bool PlayerOnline;
	uint32 CharacterID;
	if(IdentifyPlayer(Addressee, true, true, &Player) == 0){
		PlayerOnline = true;
		CharacterID = Player->ID;
	}else{
		PlayerOnline = false;
		CharacterID = GetCharacterID(Addressee);
		if(CharacterID == 0){
			return;
		}
	}

	if(PlayerOnline && Player->Depot != NONE && Player->DepotNr == DepotNr){
		print(3, "Addressee is logged in and has depot open.\n");
		try{
			move(0, Obj, Player->Depot, -1, true, NONE);
			if(ObjType == get_special_object(LETTER_NEW)){
				change(Obj, get_special_object(LETTER_STAMPED), 0);
			}else{
				change(Obj, get_special_object(PARCEL_STAMPED), 0);
			}
			log_message("game", "Mail to %s in %s sent.\n", Addressee, Town);
			Player->DepotSpace -= count_objects(Obj);
			print(3, "Depot of %s now has %d free slots.\n", Player->Name, Player->DepotSpace);
			SendMessage(Player->Connection, TALK_INFO_MESSAGE, "New mail has arrived.");
			print(3, "Mail sent successfully.\n");
		}catch(RESULT r){
			if(r != CONTAINERFULL){
				error("send_mail (depot open): Exception %d.\n", r);
			}

			print(3, "Exception %d during mail delivery.\n", r);
		}
	}else{
		print(3, "Addressee is not logged in or has depot closed.\n");

		// TODO(fusion): The scope of this try block was unclear but I suppose
		// this is correct due to how `change` is also inside the try block in
		// the try block above, when the player is online.
		try{
			if(ObjType == get_special_object(LETTER_NEW)){
				change(Obj, get_special_object(LETTER_STAMPED), 0);
			}else{
				change(Obj, get_special_object(PARCEL_STAMPED), 0);
			}

			TDynamicWriteBuffer WriteBuffer(1024);
			save_objects(Obj, &WriteBuffer, true);
			delete_op(Obj, -1);

			DelayedMail *Mail = DelayedMailArray.at(DelayedMails);
			Mail->CharacterID = CharacterID;
			Mail->DepotNumber = DepotNr;
			Mail->PacketSize = WriteBuffer.Position;
			// TODO(fusion): Check if `WriteBuffer.Position` is not zero?
			Mail->Packet = new uint8[WriteBuffer.Position];
			memcpy(Mail->Packet, WriteBuffer.Data, WriteBuffer.Position);
			DelayedMails += 1;

			log_message("game", "Mail to %s in %s deferred.\n", Addressee, Town);
			print(3, "Mail deferred.\n");
			if(PlayerOnline){
				send_mails(Player->PlayerData);
			}else{
				load_character_order(CharacterID);
			}
		}catch(RESULT r){
			error("send_mail: Cannot delete parcel (%d).\n", r);
		}catch(const char *str){
			error("send_mail: Cannot write parcel (%s).\n", str);
		}
	}
}

void send_mails(TPlayerData *PlayerData){
	if(PlayerData == NULL){
		error("send_mails: PlayerData is NULL.\n");
		return;
	}

	if(PlayerData->CharacterID == 0){
		error("send_mails: Slot is not occupied.\n");
		return;
	}

	if(PlayerData->Locked != gettid()){
		error("send_mails: Slot is not locked by game thread.\n");
		return;
	}

	for(int i = 0; i < DelayedMails; i += 1){
		DelayedMail *Mail = DelayedMailArray.at(i);
		if(Mail->CharacterID != PlayerData->CharacterID){
			continue;
		}

		int DepotNr = Mail->DepotNumber;
		if(DepotNr < 0 || DepotNr >= MAX_DEPOTS){
			error("send_mails: Invalid depot number %d.\n", DepotNr);
			DepotNr = 0;
		}

		int NewDepotSize = 0;
		uint8 *NewDepot = NULL;
		if(PlayerData->Depot[DepotNr] != NULL){
			NewDepotSize = PlayerData->DepotSize[DepotNr] + Mail->PacketSize;
			NewDepot = new uint8[NewDepotSize];
			memcpy(&NewDepot[0], Mail->Packet, Mail->PacketSize);
			memcpy(&NewDepot[Mail->PacketSize],
					PlayerData->Depot[DepotNr],
					PlayerData->DepotSize[DepotNr]);
			delete[] PlayerData->Depot[DepotNr];
		}else{
			NewDepotSize = Mail->PacketSize + 5;
			NewDepot = new uint8[NewDepotSize];
			memcpy(&NewDepot[0], Mail->Packet, Mail->PacketSize);
			TWriteBuffer WriteBuffer(&NewDepot[Mail->PacketSize], 5);
			WriteBuffer.writeWord((uint16)get_special_object(DEPOT_CHEST).TypeID);
			WriteBuffer.writeByte(0xFF);
			WriteBuffer.writeWord(0xFFFF);
		}

		PlayerData->Depot[DepotNr] = NewDepot;
		PlayerData->DepotSize[DepotNr] = NewDepotSize;
		PlayerData->Dirty = true;

		log_message("game", "Mail to %s delivered.\n", PlayerData->Name);

		if(Mail->Packet != NULL){
			delete[] Mail->Packet;
		}

		// NOTE(fusion): A little swap and pop action.
		DelayedMails -= 1;
		*DelayedMailArray.at(i) = *DelayedMailArray.at(DelayedMails);
		*DelayedMailArray.at(DelayedMails) = DelayedMail{};

		// NOTE(fusion): Check current index again after removal.
		i -= 1;
	}
}

void text_effect(const char *Text, int x, int y, int z, int Radius){
	if(Text == NULL){
		error("text_effect: Text does not exist.\n");
		return;
	}

	TFindCreatures Search(Radius, Radius, x, y, FIND_PLAYERS);
	while(true){
		uint32 CharacterID = Search.getNext();
		if(CharacterID == 0){
			break;
		}

		TPlayer *Player = GetPlayer(CharacterID);
		if(Player == NULL || Player->Connection == NULL){
			continue;
		}

		int PlayerZ = Player->posz;
		if(PlayerZ == z || Radius > 30 || (Radius > 7 && PlayerZ <= 7 && z <= 7)){
			SendTalk(Player->Connection, 0, "", TALK_ANIMAL_LOW, x, y, z, Text);
		}
	}
}

void execute_action(MoveUseEventType EventType, MoveUseAction *Action,
		Object User, Object Obj1, Object Obj2, Object *Temp){
	try{
		switch(Action->Action){
			case MOVEUSE_ACTION_CREATEONMAP:{
				int CoordX, CoordY, CoordZ;
				unpack_absolute_coordinate(Action->Parameters[0], &CoordX, &CoordY, &CoordZ);
				ObjectType Type = Action->Parameters[1];
				int Value = Action->Parameters[2];
				Object MapCon = get_map_container(CoordX, CoordY, CoordZ);
				*Temp = create_object(MapCon, Type, Value);
				break;
			}

			case MOVEUSE_ACTION_CREATE:{
				Object Obj = get_event_object(Action->Parameters[0], User, Obj1, Obj2, *Temp);
				ObjectType Type = Action->Parameters[1];
				int Value = Action->Parameters[2];
				*Temp = create_object(Obj.get_container(), Type, Value);
				break;
			}

			case MOVEUSE_ACTION_MONSTERONMAP:{
				int CoordX, CoordY, CoordZ;
				unpack_absolute_coordinate(Action->Parameters[0], &CoordX, &CoordY, &CoordZ);
				int Race = Action->Parameters[1];
				CreateMonster(Race, CoordX, CoordY, CoordZ, 0, 0, true);
				break;
			}

			case MOVEUSE_ACTION_MONSTER:{
				int ObjX, ObjY, ObjZ;
				Object Obj = get_event_object(Action->Parameters[0], User, Obj1, Obj2, *Temp);
				int Race = Action->Parameters[1];
				get_object_coordinates(Obj, &ObjX, &ObjY, &ObjZ);
				CreateMonster(Race, ObjX, ObjY, ObjZ, 0, 0, true);
				break;
			}

			case MOVEUSE_ACTION_EFFECTONMAP:{
				int CoordX, CoordY, CoordZ;
				unpack_absolute_coordinate(Action->Parameters[0], &CoordX, &CoordY, &CoordZ);
				int Effect = Action->Parameters[1];
				graphical_effect(CoordX, CoordY, CoordZ, Effect);
				break;
			}

			case MOVEUSE_ACTION_EFFECT:{
				int ObjX, ObjY, ObjZ;
				Object Obj = get_event_object(Action->Parameters[0], User, Obj1, Obj2, *Temp);
				int Effect = Action->Parameters[1];
				get_object_coordinates(Obj, &ObjX, &ObjY, &ObjZ);
				graphical_effect(ObjX, ObjY, ObjZ, Effect);
				break;
			}

			case MOVEUSE_ACTION_TEXTONMAP:{
				int CoordX, CoordY, CoordZ;
				unpack_absolute_coordinate(Action->Parameters[0], &CoordX, &CoordY, &CoordZ);
				const char *Text = GetDynamicString(Action->Parameters[1]);
				int Radius = Action->Parameters[2];
				text_effect(Text, CoordX, CoordY, CoordZ, Radius);
				break;
			}

			case MOVEUSE_ACTION_TEXT:{
				int ObjX, ObjY, ObjZ;
				Object Obj = get_event_object(Action->Parameters[0], User, Obj1, Obj2, *Temp);
				const char *Text = GetDynamicString(Action->Parameters[1]);
				int Radius = Action->Parameters[2];
				get_object_coordinates(Obj, &ObjX, &ObjY, &ObjZ);
				text_effect(Text, ObjX, ObjY, ObjZ, Radius);
				break;
			}

			case MOVEUSE_ACTION_CHANGEONMAP:{
				int CoordX, CoordY, CoordZ;
				unpack_absolute_coordinate(Action->Parameters[0], &CoordX, &CoordY, &CoordZ);
				ObjectType OldType = Action->Parameters[1];
				ObjectType NewType = Action->Parameters[2];
				int NewValue = Action->Parameters[3];
				*Temp = get_first_spec_object(CoordX, CoordY, CoordZ, OldType);
				if(*Temp != NONE){
					change_object(*Temp, NewType, NewValue);
				}else{
					error("execute_action (CHANGEONMAP): No object %d at [%d,%d,%d].\n",
							OldType.TypeID, CoordX, CoordY, CoordZ);
				}
				break;
			}

			case MOVEUSE_ACTION_CHANGE:{
				Object Obj = get_event_object(Action->Parameters[0], User, Obj1, Obj2, *Temp);
				ObjectType NewType = Action->Parameters[1];
				int NewValue = Action->Parameters[2];
				change_object(Obj, NewType, NewValue);
				break;
			}

			case MOVEUSE_ACTION_CHANGEREL:{
				int ObjX, ObjY, ObjZ, RelX, RelY, RelZ;
				Object Obj = get_event_object(Action->Parameters[0], User, Obj1, Obj2, *Temp);
				unpack_relative_coordinate(Action->Parameters[1], &RelX, &RelY, &RelZ);
				ObjectType OldType = Action->Parameters[2];
				ObjectType NewType = Action->Parameters[3];
				int NewValue = Action->Parameters[4];
				get_object_coordinates(Obj, &ObjX, &ObjY, &ObjZ);
				*Temp = get_first_spec_object((ObjX + RelX), (ObjY + RelY), (ObjZ + RelZ), OldType);
				if(*Temp != NONE){
					change_object(*Temp, NewType, NewValue);
				}else{
					error("execute_action (CHANGEREL): No object %d at [%d,%d,%d].\n",
							OldType.TypeID, (ObjX + RelX), (ObjY + RelY), (ObjZ + RelZ));
				}
				break;
			}

			case MOVEUSE_ACTION_SETATTRIBUTE:{
				Object Obj = get_event_object(Action->Parameters[0], User, Obj1, Obj2, *Temp);
				INSTANCEATTRIBUTE Attribute = (INSTANCEATTRIBUTE)Action->Parameters[1];
				int Value = Action->Parameters[2];
				change(Obj, Attribute, Value);
				break;
			}

			case MOVEUSE_ACTION_CHANGEATTRIBUTE:{
				Object Obj = get_event_object(Action->Parameters[0], User, Obj1, Obj2, *Temp);
				INSTANCEATTRIBUTE Attribute = (INSTANCEATTRIBUTE)Action->Parameters[1];
				int Amount = Action->Parameters[2];
				// TODO(fusion): This is weird.
				int OldValue = (int)Obj.get_attribute(Attribute);
				int NewValue = OldValue + Amount;
				if(NewValue <= 0){
					if(OldValue == 0){
						NewValue = 0;
					}else{
						NewValue = 1;
					}
				}
				change(Obj, Attribute, NewValue);
				break;
			}

			case MOVEUSE_ACTION_SETQUESTVALUE:{
				Object Obj = get_event_object(Action->Parameters[0], User, Obj1, Obj2, *Temp);
				int QuestNr = Action->Parameters[1];
				int QuestValue = Action->Parameters[2];
				ObjectType ObjType = Obj.get_object_type();
				if(ObjType.is_creature_container()){
					TCreature *Creature = GetCreature(Obj);
					if(Creature != NULL && Creature->Type == PLAYER){
						((TPlayer*)Creature)->SetQuestValue(QuestNr, QuestValue);
					}else{
						error("execute_action (SETQUESTVALUE): Creature does not exist or is not a player.\n");
					}
				}else{
					error("execute_action (SETQUESTVALUE): Object is not a creature, but type %d.\n", ObjType.TypeID);
				}
				break;
			}

			case MOVEUSE_ACTION_DAMAGE:{
				Object AttackerObj = get_event_object(Action->Parameters[0], User, Obj1, Obj2, *Temp);
				Object VictimObj = get_event_object(Action->Parameters[1], User, Obj1, Obj2, *Temp);
				int DamageType = Action->Parameters[2];
				int Damage = Action->Parameters[3];

				TCreature *Attacker = NULL;
				if(AttackerObj != NONE){
					ObjectType AttackerType = AttackerObj.get_object_type();
					if(AttackerType.is_creature_container()){
						Attacker = GetCreature(AttackerObj);
					}else if(AttackerType.get_flag(MAGICFIELD)
							&& AttackerObj.get_attribute(RESPONSIBLE) != 0){
						int TotalExpireTime = (int)AttackerType.get_attribute(TOTALEXPIRETIME);
						int CurExpireTime = (int)cron_info(AttackerObj, false);
						if((TotalExpireTime - CurExpireTime) < 5){
							Attacker = GetCreature(AttackerObj.get_attribute(RESPONSIBLE));
						}
					}
				}

				ObjectType VictimType = VictimObj.get_object_type();
				if(VictimType.is_creature_container()){
					TCreature *Victim = GetCreature(VictimObj);
					if(Victim != NULL){
						Victim->Damage(Attacker, Damage, DamageType);
					}else{
						error("execute_action (DAMAGE): Creature does not exist.\n");
					}
				}else{
					error("execute_action (DAMAGE): Object is not a creature, but type %d.\n", VictimType.TypeID);
				}
				break;
			}

			case MOVEUSE_ACTION_SETSTART:{
				int StartX, StartY, StartZ;
				Object Obj = get_event_object(Action->Parameters[0], User, Obj1, Obj2, *Temp);
				unpack_absolute_coordinate(Action->Parameters[1], &StartX, &StartY, &StartZ);

				if(!Obj.get_object_type().is_creature_container()){
					throw ERROR;
				}

				TCreature *Creature = GetCreature(Obj);
				if(Creature == NULL || Creature->Type != PLAYER){
					throw ERROR;
				}

				Creature->startx = StartX;
				Creature->starty = StartY;
				Creature->startz = StartZ;
				((TPlayer*)Creature)->SaveData();
				break;
			}

			case MOVEUSE_ACTION_WRITENAME:{
				Object WriterObj = get_event_object(Action->Parameters[0], User, Obj1, Obj2, *Temp);
				const char *Format = GetDynamicString(Action->Parameters[1]);
				Object TargetObj = get_event_object(Action->Parameters[2], User, Obj1, Obj2, *Temp);

				if(WriterObj == NONE || !WriterObj.get_object_type().is_creature_container()
				|| TargetObj == NONE || !TargetObj.get_object_type().get_flag(TEXT)){
					throw ERROR;
				}

				TCreature *Writer = GetCreature(WriterObj);
				if(Writer == NULL){
					throw ERROR;
				}

				// TODO(fusion): Some helper function to format text?
				char Text[256] = {};
				{
					int ReadPos = 0;
					int WritePos = 0;
					int TextSize = (int)sizeof(Text);
					while(Format[ReadPos] != 0){
						if(Format[ReadPos] == '%' && Format[ReadPos + 1] == 'N'){
							// NOTE(fusion): Make sure we don't overflow the buffer.
							int NameLen = (int)strlen(Writer->Name);
							if(NameLen > 0){
								if((WritePos + NameLen) > TextSize){
									NameLen = TextSize - WritePos;
								}

								if(NameLen > 0){
									memcpy(&Text[WritePos], Writer->Name, NameLen);
									WritePos += NameLen;
								}
							}

							ReadPos += 2;
						}else{
							// NOTE(fusion): Make sure we don't overflow the buffer.
							if(WritePos < TextSize){
								Text[WritePos] = Format[ReadPos];
								WritePos += 1;
							}

							ReadPos += 1;
						}
					}

					if(WritePos >= TextSize){
						throw ERROR;
					}
				}

				DeleteDynamicString(TargetObj.get_attribute(TEXTSTRING));
				DeleteDynamicString(TargetObj.get_attribute(EDITOR));
				change(TargetObj, TEXTSTRING, AddDynamicString(Text));
				change(TargetObj, EDITOR, 0);
				break;
			}

			case MOVEUSE_ACTION_WRITETEXT:{
				const char *Text = GetDynamicString(Action->Parameters[0]);
				Object Obj = get_event_object(Action->Parameters[1], User, Obj1, Obj2, *Temp);

				if(Obj == NONE || !Obj.get_object_type().get_flag(TEXT)){
					throw ERROR;
				}

				DeleteDynamicString(Obj.get_attribute(TEXTSTRING));
				DeleteDynamicString(Obj.get_attribute(EDITOR));
				change(Obj, TEXTSTRING, AddDynamicString(Text));
				change(Obj, EDITOR, 0);
				break;
			}

			case MOVEUSE_ACTION_LOGOUT:{
				Object Obj = get_event_object(Action->Parameters[0], User, Obj1, Obj2, *Temp);

				if(Obj == NONE || !Obj.get_object_type().is_creature_container()){
					throw ERROR;
				}

				TCreature *Creature = GetCreature(Obj);
				if(Creature == NULL){
					throw ERROR;
				}

				Creature->StartLogout(true, true);
				break;
			}

			case MOVEUSE_ACTION_MOVEALLONMAP:{
				int OrigX, OrigY, OrigZ, DestX, DestY, DestZ;
				unpack_absolute_coordinate(Action->Parameters[0], &OrigX, &OrigY, &OrigZ);
				unpack_absolute_coordinate(Action->Parameters[1], &DestX, &DestY, &DestZ);
				Object First = get_first_object(OrigX, OrigY, OrigZ);
				Object Dest = get_map_container(DestX, DestY, DestZ);
				move_all_objects(First, Dest, NONE, false);
				break;
			}

			case MOVEUSE_ACTION_MOVEALL:{
				int DestX, DestY, DestZ;
				Object Obj = get_event_object(Action->Parameters[0], User, Obj1, Obj2, *Temp);
				unpack_absolute_coordinate(Action->Parameters[1], &DestX, &DestY, &DestZ);
				Object First = get_first_container_object(Obj.get_container());
				Object Dest = get_map_container(DestX, DestY, DestZ);
				// TODO(fusion): Why do we set the `Exclude` parameter to `First`?
				// Maybe we just don't want to check if it is `NONE` can call
				// `get_next_object()` like MOVETOPONMAP?
				move_all_objects(First, Dest, First, false);
				break;
			}

			case MOVEUSE_ACTION_MOVEALLREL:{
				int ObjX, ObjY, ObjZ, RelX, RelY, RelZ;
				Object Obj = get_event_object(Action->Parameters[0], User, Obj1, Obj2, *Temp);
				unpack_relative_coordinate(Action->Parameters[1], &RelX, &RelY, &RelZ);
				get_object_coordinates(Obj, &ObjX, &ObjY, &ObjZ);
				Object First = get_first_container_object(Obj.get_container());
				Object Dest = get_map_container((ObjX + RelX), (ObjY + RelY), (ObjZ + RelZ));
				// TODO(fusion): Same as above?
				move_all_objects(First, Dest, First, false);
				break;
			}

			case MOVEUSE_ACTION_MOVETOPONMAP:{
				int OrigX, OrigY, OrigZ, DestX, DestY, DestZ;
				unpack_absolute_coordinate(Action->Parameters[0], &OrigX, &OrigY, &OrigZ);
				ObjectType Type = Action->Parameters[1];
				unpack_absolute_coordinate(Action->Parameters[2], &DestX, &DestY, &DestZ);
				Object Obj = get_first_spec_object(OrigX, OrigY, OrigZ, Type);
				if(Obj != NONE){
					Object Dest = get_map_container(DestX, DestY, DestZ);
					move_all_objects(Obj.get_next_object(), Dest, NONE, true);
				}else{
					error("execute_action (MOVETOPONMAP): No object %d at [%d,%d,%d].\n",
							Type.TypeID, OrigX, OrigY, OrigZ);
				}
				break;
			}

			case MOVEUSE_ACTION_MOVETOP:{
				int DestX, DestY, DestZ;
				Object Obj = get_event_object(Action->Parameters[0], User, Obj1, Obj2, *Temp);
				unpack_absolute_coordinate(Action->Parameters[1], &DestX, &DestY, &DestZ);
				Object Dest = get_map_container(DestX, DestY, DestZ);
				move_all_objects(Obj.get_next_object(), Dest, NONE, true);
				break;
			}

			case MOVEUSE_ACTION_MOVETOPREL:{
				int ObjX, ObjY, ObjZ, RelX, RelY, RelZ;
				Object Obj = get_event_object(Action->Parameters[0], User, Obj1, Obj2, *Temp);
				unpack_relative_coordinate(Action->Parameters[1], &RelX, &RelY, &RelZ);
				get_object_coordinates(Obj, &ObjX, &ObjY, &ObjZ);
				Object Dest = get_map_container((ObjX + RelX), (ObjY + RelY), (ObjZ + RelZ));
				move_all_objects(Obj.get_next_object(), Dest, NONE, true);
				break;
			}

			case MOVEUSE_ACTION_MOVE:{
				int DestX, DestY, DestZ;
				Object Obj = get_event_object(Action->Parameters[0], User, Obj1, Obj2, *Temp);
				unpack_absolute_coordinate(Action->Parameters[1], &DestX, &DestY, &DestZ);
				Object Dest = get_map_container(DestX, DestY, DestZ);
				move_one_object(Obj, Dest);
				break;
			}

			case MOVEUSE_ACTION_MOVEREL:{
				int RefX, RefY, RefZ, RelX, RelY, RelZ;
				Object MovObj = get_event_object(Action->Parameters[0], User, Obj1, Obj2, *Temp);
				Object RefObj = get_event_object(Action->Parameters[1], User, Obj1, Obj2, *Temp);
				unpack_relative_coordinate(Action->Parameters[2], &RelX, &RelY, &RelZ);
				get_object_coordinates(RefObj, &RefX, &RefY, &RefZ);
				Object Dest = get_map_container((RefX + RelX), (RefY + RelY), (RefZ + RelZ));
				move_one_object(MovObj, Dest);
				break;
			}

			case MOVEUSE_ACTION_RETRIEVE:{
				int ObjX, ObjY, ObjZ, OrigRelX, OrigRelY, OrigRelZ, DestRelX, DestRelY, DestRelZ;
				Object Obj = get_event_object(Action->Parameters[0], User, Obj1, Obj2, *Temp);
				unpack_relative_coordinate(Action->Parameters[1], &OrigRelX, &OrigRelY, &OrigRelZ);
				unpack_relative_coordinate(Action->Parameters[2], &DestRelX, &DestRelY, &DestRelZ);
				get_object_coordinates(Obj, &ObjX, &ObjY, &ObjZ);
				Object Top = get_top_object((ObjX + OrigRelX), (ObjY + OrigRelY), (ObjZ + OrigRelZ), false);
				if(Top != NONE && !Top.get_object_type().get_flag(UNMOVE)){
					Object Dest = get_map_container((ObjX + DestRelX), (ObjY + DestRelY), (ObjZ + DestRelZ));
					move_one_object(Top, Dest);
				}
				break;
			}

			case MOVEUSE_ACTION_DELETEALLONMAP:{
				int CoordX, CoordY, CoordZ;
				unpack_absolute_coordinate(Action->Parameters[0], &CoordX, &CoordY, &CoordZ);
				Object First = get_first_object(CoordX, CoordY, CoordZ);
				// TODO(fusion): Same as `MOVEALL`.
				delete_all_objects(First, First, false);
				break;
			}

			case MOVEUSE_ACTION_DELETETOPONMAP:{
				int CoordX, CoordY, CoordZ;
				unpack_absolute_coordinate(Action->Parameters[0], &CoordX, &CoordY, &CoordZ);
				ObjectType Type = Action->Parameters[1];
				Object First = get_first_spec_object(CoordX, CoordY, CoordZ, Type);
				// TODO(fusion): Sometimes we throw, sometimes we print an error,
				// sometimes we ignore it, what a mess.
				if(First == NONE){
					throw ERROR;
				}
				delete_all_objects(First, First, true);
				break;
			}

			case MOVEUSE_ACTION_DELETEONMAP:{
				int CoordX, CoordY, CoordZ;
				unpack_absolute_coordinate(Action->Parameters[0], &CoordX, &CoordY, &CoordZ);
				ObjectType Type = Action->Parameters[1];
				Object Obj = get_first_spec_object(CoordX, CoordY, CoordZ, Type);
				if(Obj != NONE){
					bool IsUseEvent = (EventType == MOVEUSE_EVENT_USE
							|| EventType == MOVEUSE_EVENT_MULTIUSE);
					delete_op(Obj, (IsUseEvent ? 1 : -1));
				}else{
					error("execute_action (DELETEONMAP): No object %d at [%d,%d,%d].\n",
							Type.TypeID, CoordX, CoordY, CoordZ);
				}
				break;
			}

			case MOVEUSE_ACTION_DELETE:{
				Object Obj = get_event_object(Action->Parameters[0], User, Obj1, Obj2, *Temp);
				if(Obj == NONE){
					throw ERROR;
				}
				bool IsUseEvent = (EventType == MOVEUSE_EVENT_USE
						|| EventType == MOVEUSE_EVENT_MULTIUSE);
				delete_op(Obj, (IsUseEvent ? 1 : -1));
				break;
			}

			case MOVEUSE_ACTION_DELETEININVENTORY:{
				Object Obj = get_event_object(Action->Parameters[0], User, Obj1, Obj2, *Temp);
				ObjectType Type = Action->Parameters[1];
				int Value = Action->Parameters[2];
				if(Obj.get_object_type().is_creature_container()){
					delete_at_creature(Obj.get_creature_id(), Type, 1, Value);
				}else{
					throw ERROR;
				}
				break;
			}

			case MOVEUSE_ACTION_DESCRIPTION:{
				Object Obj = get_event_object(Action->Parameters[0], User, Obj1, Obj2, *Temp);
				Object Cr = get_event_object(Action->Parameters[1], User, Obj1, Obj2, *Temp);
				if(Cr.get_object_type().is_creature_container()){
					TCreature *Creature = GetCreature(Cr);
					if(Creature != NULL && Creature->Type == PLAYER){
						SendMessage(Creature->Connection, TALK_INFO_MESSAGE,
								"%s.", get_info(Obj));
					}
				}
				break;
			}

			case MOVEUSE_ACTION_LOADDEPOT:{
				int CoordX, CoordY, CoordZ;
				unpack_absolute_coordinate(Action->Parameters[0], &CoordX, &CoordY, &CoordZ);
				ObjectType Type = Action->Parameters[1];
				Object Cr = get_event_object(Action->Parameters[2], User, Obj1, Obj2, *Temp);
				int DepotNr = Action->Parameters[3];

				if(!Cr.get_object_type().is_creature_container()){
					throw ERROR;
				}

				Object Depot = get_first_spec_object(CoordX, CoordY, CoordZ, Type);
				if(Depot == NONE){
					throw ERROR;
				}

				load_depot_box(Cr.get_creature_id(), DepotNr, Depot);
				break;
			}

			case MOVEUSE_ACTION_SAVEDEPOT:{
				int CoordX, CoordY, CoordZ;
				unpack_absolute_coordinate(Action->Parameters[0], &CoordX, &CoordY, &CoordZ);
				ObjectType Type = Action->Parameters[1];
				Object Cr = get_event_object(Action->Parameters[2], User, Obj1, Obj2, *Temp);
				int DepotNr = Action->Parameters[3];

				if(!Cr.get_object_type().is_creature_container()){
					throw ERROR;
				}

				Object Depot = get_first_spec_object(CoordX, CoordY, CoordZ, Type);
				if(Depot == NONE){
					throw ERROR;
				}

				save_depot_box(Cr.get_creature_id(), DepotNr, Depot);
				break;
			}

			case MOVEUSE_ACTION_SENDMAIL:{
				Object Obj = get_event_object(Action->Parameters[0], User, Obj1, Obj2, *Temp);
				send_mail(Obj);
				break;
			}

			case MOVEUSE_ACTION_NOP:{
				// no-op
				break;
			}

			default:{
				error("execute_action: Unknown action %d.\n", Action->Action);
				break;
			}
		}
	}catch(RESULT r){
		error("execute_action: Exception %d (action %d).\n", r, Action->Action);
	}
}

bool handle_event(MoveUseEventType EventType, Object User, Object Obj1, Object Obj2){
	static int RecursionDepth = 0;

	if(RecursionDepth > 10){
		error("handle_event: Infinite loop suspected in move/use system.\n");
		return false;
	}

	bool Result = false;
	RecursionDepth += 1;
	MoveUseDatabase *DB = &MoveUseDatabases[EventType];
	for(int RuleNr = 1; RuleNr <= DB->NumberOfRules; RuleNr += 1){
		bool Execute = true;
		Object Temp = NONE;
		MoveUseRule *Rule = DB->Rules.at(RuleNr);
		for(int ConditionNr = Rule->FirstCondition;
				ConditionNr <= Rule->LastCondition;
				ConditionNr += 1){
			MoveUseCondition *Condition = MoveUseConditions.at(ConditionNr);
			if(!check_condition(EventType, Condition, User, Obj1, Obj2, &Temp)){
				Execute = false;
				break;
			}
		}

		if(Execute){
			for(int ActionNr = Rule->FirstAction;
					ActionNr <= Rule->LastAction;
					ActionNr += 1){
				MoveUseAction *Action = MoveUseActions.at(ActionNr);
				execute_action(EventType, Action, User, Obj1, Obj2, &Temp);
			}
			Result = true;
			break;
		}
	}
	RecursionDepth -= 1;
	return Result;
}

// Event Dispatching
// =============================================================================
void use_container(uint32 CreatureID, Object Con, int NextContainerNr){
	if(!Con.exists()){
		error("use_container: Provided object does not exist.\n");
		throw ERROR;
	}

	ObjectType ConType = Con.get_object_type();
	if(!ConType.get_flag(CONTAINER)){
		error("use_container: Provided object is not a container.\n");
		throw ERROR;
	}

	if(NextContainerNr < 0 || NextContainerNr >= NARRAY(TPlayer::OpenContainer)){
		error("use_container: Invalid window number %d.\n", NextContainerNr);
		throw ERROR;
	}

	if(ConType.is_creature_container()){
		throw NOTUSABLE;
	}

	TPlayer *Player = GetPlayer(CreatureID);
	if(Player == NULL){
		error("use_container: Player %d does not exist.\n", CreatureID);
		throw ERROR;
	}

	bool ContainerClosed = false;
	for(int ContainerNr = 0;
			ContainerNr < NARRAY(TPlayer::OpenContainer);
			ContainerNr += 1){
		if(Player->GetOpenContainer(ContainerNr) == Con){
			Player->SetOpenContainer(ContainerNr, NONE);
			SendCloseContainer(Player->Connection, ContainerNr);
			ContainerClosed = true;
			break;
		}
	}

	if(!ContainerClosed){
		Player->SetOpenContainer(NextContainerNr, Con);
		SendContainer(Player->Connection, NextContainerNr);
	}
}

void use_chest(uint32 CreatureID, Object Chest){
	if(!Chest.exists()){
		error("use_chest: Provided object does not exist.\n");
		throw ERROR;
	}

	ObjectType ChestType = Chest.get_object_type();
	if(!ChestType.get_flag(CHEST)){
		error("use_chest: Provided object is not a treasure chest.\n");
		throw ERROR;
	}

	int ChestX, ChestY, ChestZ;
	get_object_coordinates(Chest, &ChestX, &ChestY, &ChestZ);

	int QuestNr = (int)Chest.get_attribute(CHESTQUESTNUMBER);
	if(QuestNr < 0 || QuestNr >= NARRAY(TPlayer::QuestValues)){
		error("use_chest: Invalid number %d on treasure chest at position [%d,%d,%d].\n",
				QuestNr, ChestX, ChestY, ChestZ);
		throw ERROR;
	}

	if(count_objects_in_container(Chest) != 1){
		error("use_chest: Treasure chest at position [%d,%d,%d] does not contain exactly one object.\n",
				ChestX, ChestY, ChestZ);
		throw ERROR;
	}

	Object Treasure = get_first_container_object(Chest);
	ObjectType TreasureType = Treasure.get_object_type();
	if(TreasureType.get_flag(UNMOVE) || !TreasureType.get_flag(TAKE)){
		error("use_chest: Treasure at position [%d,%d,%d] is not takeable.\n",
				ChestX, ChestY, ChestZ);
		throw ERROR;
	}

	TPlayer *Player = GetPlayer(CreatureID);
	if(Player == NULL){
		error("use_chest: Player %u does not exist.\n", CreatureID);
		throw ERROR;
	}

	if(Player->GetQuestValue(QuestNr) != 0){
		print(3, "Treasure chest is already known.\n");
		SendMessage(Player->Connection, TALK_INFO_MESSAGE,
				"The %s is empty.", ChestType.get_name(-1));
		return;
	}

	if(!check_right(CreatureID, UNLIMITED_CAPACITY)){
		int TreasureWeight = get_complete_weight(Treasure);
		int InventoryWeight = get_inventory_weight(CreatureID);
		int MaxWeight = Player->Skills[SKILL_CARRY_STRENGTH]->Get() * 100;
		if((InventoryWeight + TreasureWeight) > MaxWeight
				|| check_right(CreatureID, ZERO_CAPACITY)){
			bool Multiple = TreasureType.get_flag(CUMULATIVE)
					&& Treasure.get_attribute(AMOUNT) > 1;
			SendMessage(Player->Connection, TALK_INFO_MESSAGE,
					"You have found %s. Weighing %d.%02d oz %s too heavy.",
					get_name(Treasure), (TreasureWeight / 100), (TreasureWeight % 100),
					(Multiple ? "they are" : "it is"));
			return;
		}
	}

	// NOTE(fusion): copy treasure object and attempt to move into the player's
	// inventory. The original version was somewhat confusing but this should
	// achieve the same end result.
	Treasure = copy(Chest, Treasure);
	bool CheckContainers = TreasureType.get_flag(MOVEMENTEVENT);
	bool TreasureMoved = false;
	for(int i = 0; i < 2 && !TreasureMoved; i += 1){
		for(int Position = INVENTORY_FIRST;
				Position <= INVENTORY_LAST && !TreasureMoved;
				Position += 1){
			try{
				Object BodyObj = get_body_object(CreatureID, Position);
				if(CheckContainers){
					if(BodyObj != NONE && BodyObj.get_object_type().get_flag(CONTAINER)){
						move(CreatureID, Treasure, BodyObj, -1, true, NONE);
						TreasureMoved = true;
					}
				}else{
					if(BodyObj == NONE){
						Object BodyCon = get_body_container(CreatureID, Position);
						move(CreatureID, Treasure, BodyCon, -1, true, NONE);
						TreasureMoved = true;
					}
				}
			}catch(RESULT r){
				// no-op
			}
		}
		CheckContainers = !CheckContainers;
	}

	if(!TreasureMoved){
		bool Multiple = TreasureType.get_flag(CUMULATIVE)
				&& Treasure.get_attribute(AMOUNT) > 1;
		SendMessage(Player->Connection, TALK_INFO_MESSAGE,
				"You have found %s, but you have no room to take %s.\n",
				get_name(Treasure), (Multiple ? "them" : "it"));
		delete_op(Treasure, -1);
		return;
	}

	SendMessage(Player->Connection, TALK_INFO_MESSAGE,
			"You have found %s.\n", get_name(Treasure));
	Player->SetQuestValue(QuestNr, 1);
}

void use_liquid_container(uint32 CreatureID, Object Obj, Object Dest){
	if(!Obj.exists()){
		error("use_liquid_container: Provided object Obj does not exist.\n");
		throw ERROR;
	}

	ObjectType ObjType = Obj.get_object_type();
	if(!Dest.exists()){
		error("use_liquid_container: Provided object Dest does not exist (Obj %d).\n",
				ObjType.TypeID);
		throw ERROR;
	}

	if(!ObjType.get_flag(LIQUIDCONTAINER)){
		error("use_liquid_container: Provided object is not a liquid container.\n");
		throw ERROR;
	}

	ObjectType DestType = Dest.get_object_type();
	int LiquidType = Obj.get_attribute(CONTAINERLIQUIDTYPE);

	// NOTE(fusion): Fill liquid container.
	if(DestType.get_flag(LIQUIDSOURCE) && LiquidType == LIQUID_NONE){
		int DestLiquidType = (int)DestType.get_attribute(SOURCELIQUIDTYPE);
		change(Obj, CONTAINERLIQUIDTYPE, DestLiquidType);
		return;
	}

	// NOTE(fusion): Transfer liquid to another container.
	if(DestType.get_flag(LIQUIDCONTAINER) && LiquidType != LIQUID_NONE
			&& Dest.get_attribute(CONTAINERLIQUIDTYPE) == LIQUID_NONE){
		change(Dest, CONTAINERLIQUIDTYPE, LiquidType);
		change(Obj, CONTAINERLIQUIDTYPE, LIQUID_NONE);
		return;
	}

	// NOTE(fusion): This is similar to the target picking logic for runes except
	// we want to prioritize the user drinking the liquid, instead of otherwise
	// throwing it on the ground.
	if(!DestType.is_creature_container()){
		Object Help = get_first_container_object(Dest.get_container());
		while(Help != NONE){
			ObjectType HelpType = Help.get_object_type();
			if(HelpType.is_creature_container() && Help.get_creature_id() == CreatureID){
				Dest = Help;
				DestType = HelpType;
				break;
			}
			Help = Help.get_next_object();
		}
	}

	// NOTE(fusion): Spill liquid.
	if(!DestType.is_creature_container() || Dest.get_creature_id() != CreatureID){
		if(LiquidType == LIQUID_NONE){
			throw NOTUSABLE;
		}

		int DestX, DestY, DestZ;
		get_object_coordinates(Dest, &DestX, &DestY, &DestZ);
		if(!coordinate_flag(DestX, DestY, DestZ, BANK)
		|| coordinate_flag(DestX, DestY, DestZ, UNLAY)){
			throw NOROOM;
		}

		change(Obj, CONTAINERLIQUIDTYPE, LIQUID_NONE);
		create_pool(get_map_container(DestX, DestY, DestZ),
				get_special_object(BLOOD_POOL), LiquidType);
		return;
	}

	// NOTE(fusion): Drink liquid.
	TPlayer *Player = GetPlayer(CreatureID);
	if(Player == NULL){
		error("use_liquid_container: Player %d does not exist.\n", CreatureID);
		throw ERROR;
	}

	switch(LiquidType){
		case LIQUID_NONE:{
			// no-op
			break;
		}

		case LIQUID_WINE:
		case LIQUID_BEER:{
			change(Obj, CONTAINERLIQUIDTYPE, LIQUID_NONE);
			int DrunkLevel = Player->Skills[SKILL_DRUNKEN]->TimerValue();
			if(DrunkLevel < 5){
				Player->SetTimer(SKILL_DRUNKEN, (DrunkLevel + 1), 120, 120, -1);
			}
			talk(CreatureID, TALK_SAY, NULL, "Aah...", false);
			break;
		}

		case LIQUID_SLIME:{
			change(Obj, CONTAINERLIQUIDTYPE, LIQUID_NONE);
			Player->Damage(NULL, 200, DAMAGE_POISON_PERIODIC);
			talk(CreatureID, TALK_SAY, NULL, "Urgh!", false);
			break;
		}

		case LIQUID_URINE:{
			change(Obj, CONTAINERLIQUIDTYPE, LIQUID_NONE);
			talk(CreatureID, TALK_SAY, NULL, "Urgh!", false);
			break;
		}

		case LIQUID_MANA:
		case LIQUID_LIFE:{
			drink_potion(CreatureID, Obj);
			talk(CreatureID, TALK_SAY, NULL, "Aaaah...", false);
			break;
		}

		case LIQUID_LEMONADE:{
			change(Obj, CONTAINERLIQUIDTYPE, LIQUID_NONE);
			talk(CreatureID, TALK_SAY, NULL, "Mmmh.", false);
			break;
		}

		default:{
			change(Obj, CONTAINERLIQUIDTYPE, LIQUID_NONE);
			talk(CreatureID, TALK_SAY, NULL, "Gulp.", false);
			break;
		}
	}
}

void use_food(uint32 CreatureID, Object Obj){
	if(!Obj.exists()){
		error("use_food: Provided object does not exist.\n");
		throw ERROR;
	}

	ObjectType ObjType = Obj.get_object_type();
	if(!ObjType.get_flag(FOOD)){
		error("use_food: Provided object is not a food item.\n");
		throw ERROR;
	}

	TPlayer *Player = GetPlayer(CreatureID);
	if(Player == NULL){
		error("use_food: Creature %u does not exist.\n", CreatureID);
		throw ERROR;
	}

	int ObjFoodTime = (int)ObjType.get_attribute(NUTRITION) * 12;
	int CurFoodTime = Player->Skills[SKILL_FED]->TimerValue();
	int MaxFoodTime = Player->Skills[SKILL_FED]->Max;
	if((CurFoodTime + ObjFoodTime) > MaxFoodTime){
		throw FEDUP;
	}

	Player->SetTimer(SKILL_FED, (CurFoodTime + ObjFoodTime), 0, 0, -1);
	delete_op(Obj, 1);
}

void use_text_object(uint32 CreatureID, Object Obj){
	if(!Obj.exists()){
		error("use_text_object: Provided object does not exist.\n");
		throw ERROR;
	}

	ObjectType ObjType = Obj.get_object_type();
	if(!ObjType.get_flag(TEXT)){
		error("use_text_object: Provided object is not a text item.\n");
		throw ERROR;
	}

	TPlayer *Player = GetPlayer(CreatureID);
	if(Player == NULL){
		error("use_text_object: Creature %u does not exist.\n", CreatureID);
		throw ERROR;
	}

	SendEditText(Player->Connection, Obj);
}

void use_announcer(uint32 CreatureID, Object Obj){
	if(!Obj.exists()){
		error("use_announcer: Provided object does not exist.\n");
		throw ERROR;
	}

	ObjectType ObjType = Obj.get_object_type();
	if(!ObjType.get_flag(INFORMATION)){
		error("use_announcer: Provided object does not provide information.\n");
		throw ERROR;
	}

	TPlayer *Player = GetPlayer(CreatureID);
	if(Player == NULL){
		error("use_announcer: Creature %d does not exist.\n", CreatureID);
		throw ERROR;
	}

	int Information = (int)ObjType.get_attribute(INFORMATIONTYPE);
	switch(Information){
		case 1:{ // INFORMATION_DATETIME
			int Year, Cycle, Day, Hour, Minute;
			GetDate(&Year, &Cycle, &Day);
			GetTime(&Hour, &Minute);
			SendMessage(Player->Connection, TALK_INFO_MESSAGE,
					"It is the %dth day of the %dth cycle in the year %d. The time is %d:%.2d.",
					Day, Cycle, Year, Hour, Minute);
			break;
		}

		case 2:{ // INFORMATION_TIME
			int Hour, Minute;
			GetTime(&Hour, &Minute);
			SendMessage(Player->Connection, TALK_INFO_MESSAGE,
					"The time is %d:%.2d.", Hour, Minute);
			break;
		}

		case 3:{ // INFORMATION_BLESSINGS
			int NumBlessings = 0;
			char Help[256] = {};
			strcpy(Help, "Received blessings:");

			if(Player->GetQuestValue(101) != 0){
				strcat(Help, "\nWisdom of Solitude");
				NumBlessings += 1;
			}

			if(Player->GetQuestValue(102) != 0){
				strcat(Help, "\nSpark of the Phoenix");
				NumBlessings += 1;
			}

			if(Player->GetQuestValue(103) != 0){
				strcat(Help, "\nFire of the Suns");
				NumBlessings += 1;
			}

			if(Player->GetQuestValue(104) != 0){
				strcat(Help, "\nSpiritual shielding");
				NumBlessings += 1;
			}

			if(Player->GetQuestValue(105) != 0){
				strcat(Help, "\nEmbrace of Tibia");
				NumBlessings += 1;
			}

			if(NumBlessings == 0){
				strcpy(Help, "No blessings received.");
			}

			SendMessage(Player->Connection, TALK_INFO_MESSAGE, "%s", Help);
			break;
		}

		case 4:{ // INFORMATION_SPELLBOOK
			SendEditText(Player->Connection, Obj);
			break;
		}

		default:{
			error("use_announcer: Invalid information type %d.\n", Information);
			break;
		}
	}
}

void use_key_door(uint32 CreatureID, Object Key, Object Door){
	if(!Key.exists()){
		error("use_key_door: Provided object Key does not exist.\n");
		throw ERROR;
	}

	if(!Door.exists()){
		error("use_key_door: Provided object Door does not exist.\n");
		throw ERROR;
	}

	ObjectType KeyType = Key.get_object_type();
	if(!KeyType.get_flag(KEY)){
		error("use_key_door: Provided object is not a key.\n");
		throw ERROR;
	}

	ObjectType DoorType = Door.get_object_type();
	if(!DoorType.get_flag(KEYDOOR)){
		throw NOTUSABLE;
	}

	int KeyNumber = (int)Key.get_attribute(KEYNUMBER);
	int KeyHoleNumber = (int)Door.get_attribute(KEYHOLENUMBER);
	if(KeyNumber == 0 || KeyHoleNumber == 0 || KeyNumber != KeyHoleNumber){
		throw NOKEYMATCH;
	}

	ObjectType KeyDoorTarget = (int)DoorType.get_attribute(KEYDOORTARGET);
	if(KeyDoorTarget.is_map_container()){
		error("use_key_door: Target door for door %d not specified.\n",
				DoorType.TypeID);
		throw ERROR;
	}

	if(KeyDoorTarget.get_flag(UNPASS)){
		clear_field(Door, NONE);
	}

	change(Door, KeyDoorTarget, 0);
}

void use_name_door(uint32 CreatureID, Object Door){
	if(!Door.exists()){
		error("use_name_door: Provided object Door does not exist.\n");
		throw ERROR;
	}

	ObjectType DoorType = Door.get_object_type();
	if(!DoorType.get_flag(NAMEDOOR)){
		error("use_name_door: Provided object Door is not a named door.\n");
		throw ERROR;
	}

	if(!DoorType.get_flag(TEXT)){
		error("use_name_door: Provided object Door has no text.\n");
		throw ERROR;
	}

	TPlayer *Player = GetPlayer(CreatureID);
	if(Player == NULL){
		error("use_name_door: Player does not exist.\n");
		throw ERROR;
	}

	int DoorX, DoorY, DoorZ;
	get_object_coordinates(Door, &DoorX, &DoorY, &DoorZ);

	uint16 HouseID = get_house_id(DoorX, DoorY, DoorZ);
	if(HouseID == 0){
		error("use_name_door: Coordinate [%d,%d,%d] does not belong to any house.\n",
				DoorX, DoorY, DoorZ);
		throw ERROR;
	}

	if(!is_owner(HouseID, Player)
			&& !check_right(CreatureID, OPEN_NAMEDOORS)
			&& !may_open_door(Door, Player)){
		throw NOTACCESSIBLE;
	}

	ObjectType NameDoorTarget = (int)DoorType.get_attribute(NAMEDOORTARGET);
	if(NameDoorTarget.is_map_container()){
		error("use_name_door: Target door for door %d not specified.\n",
				DoorType.TypeID);
		throw ERROR;
	}

	if(NameDoorTarget.get_flag(UNPASS)){
		clear_field(Door, NONE);
	}

	change(Door, NameDoorTarget, 0);
}

void use_level_door(uint32 CreatureID, Object Door){
	if(!Door.exists() || !Door.get_object_type().get_flag(LEVELDOOR)
			|| !Door.get_container().get_object_type().is_map_container()){
		error("use_level_door: Provided object Door is not a level door.\n");
		throw ERROR;
	}

	TPlayer *Player = GetPlayer(CreatureID);
	if(Player == NULL){
		error("use_level_door: Player does not exist.\n");
		throw ERROR;
	}

	ObjectType DoorType = Door.get_object_type();
	if(!DoorType.get_flag(UNPASS)){
		throw NOTUSABLE;
	}

	int DoorLevel = (int)Door.get_attribute(DOORLEVEL);
	int PlayerLevel = Player->Skills[SKILL_LEVEL]->Get();
	if(PlayerLevel < DoorLevel){
		SendMessage(Player->Connection, TALK_INFO_MESSAGE, "%s.", get_info(Door));
		return;
	}

	ObjectType LevelDoorTarget = (int)DoorType.get_attribute(LEVELDOORTARGET);
	if(LevelDoorTarget.is_map_container() || LevelDoorTarget.get_flag(UNPASS)){
		error("use_level_door: Target door for door %d not specified or not passable.\n",
				DoorType.TypeID);
		throw ERROR;
	}

	change(Door, LevelDoorTarget, 0);
	move(0, Player->CrObject, Door.get_container(), -1, false, NONE);
}

void use_quest_door(uint32 CreatureID, Object Door){
	if(!Door.exists() || !Door.get_object_type().get_flag(QUESTDOOR)
			|| !Door.get_container().get_object_type().is_map_container()){
		error("use_quest_door: Provided object Door is not a quest door.\n");
		throw ERROR;
	}

	TPlayer *Player = GetPlayer(CreatureID);
	if(Player == NULL){
		error("use_quest_door: Player does not exist.\n");
		throw ERROR;
	}

	ObjectType DoorType = Door.get_object_type();
	if(!DoorType.get_flag(UNPASS)){
		throw NOTUSABLE;
	}

	int QuestNr = Door.get_attribute(DOORQUESTNUMBER);
	int QuestValue = Door.get_attribute(DOORQUESTVALUE);
	if(Player->GetQuestValue(QuestNr) != QuestValue){
		SendMessage(Player->Connection, TALK_INFO_MESSAGE, "%s.", get_info(Door));
		return;
	}

	ObjectType QuestDoorTarget = (int)DoorType.get_attribute(QUESTDOORTARGET);
	if(QuestDoorTarget.is_map_container() || QuestDoorTarget.get_flag(UNPASS)){
		error("use_quest_door: Target door for door %d not specified or not passable.\n",
				DoorType.TypeID);
		throw ERROR;
	}

	change(Door, QuestDoorTarget, 0);
	move(0, Player->CrObject, Door.get_container(), -1, false, NONE);
}

void use_weapon(uint32 CreatureID, Object Weapon, Object Target){
	if(!Weapon.exists() || !Weapon.get_object_type().is_close_weapon()){
		error("use_weapon: Provided weapon does not exist or is not a weapon.\n");
		throw ERROR;
	}

	if(!Target.exists() || !Target.get_object_type().get_flag(DESTROY)){
		error("use_weapon: Provided target does not exist or is not destroyable.\n");
		throw ERROR;
	}

	TPlayer *Player = GetPlayer(CreatureID);
	if(Player == NULL){
		error("use_weapon: Player %d does not exist.\n", CreatureID);
		throw ERROR;
	}

	if(!Target.get_container().get_object_type().is_map_container()){
		throw NOTACCESSIBLE;
	}

	graphical_effect(Target, EFFECT_POFF);
	if(random(1, 3) == 1){
		// TODO(fusion): This is very similar to what is done in `process_cron_system`
		// and it is probably some inlined helper function to transform items that
		// might be containers.
		ObjectType TargetType = Target.get_object_type();
		ObjectType DestroyTarget = (int)TargetType.get_attribute(DESTROYTARGET);
		if(TargetType.get_flag(CONTAINER)){
			int Remainder = 0;
			if(!DestroyTarget.is_map_container() && DestroyTarget.get_flag(CONTAINER)){
				Remainder = (int)DestroyTarget.get_attribute(CAPACITY);
			}

			empty(Target, Remainder);
		}
		change(Target, DestroyTarget, 0);
	}
}

void use_change_object(uint32 CreatureID, Object Obj){
	if(!Obj.exists() || !Obj.get_object_type().get_flag(CHANGEUSE)){
		error("use_change_object: Object does not exist or is not a CHANGEUSE object.\n");
		throw ERROR;
	}

	TPlayer *Player = GetPlayer(CreatureID);
	if(Player == NULL){
		error("use_change_object: Player %d does not exist.\n", CreatureID);
		throw ERROR;
	}

	ObjectType ObjType = Obj.get_object_type();
	ObjectType ChangeTarget = (int)ObjType.get_attribute(CHANGETARGET);
	if(!ObjType.get_flag(UNPASS)
			&& !ChangeTarget.is_map_container()
			&& ChangeTarget.get_flag(UNPASS)){
		clear_field(Obj, NONE);
	}else if(!ObjType.get_flag(UNLAY)
			&& !ChangeTarget.is_map_container()
			&& ChangeTarget.get_flag(UNLAY)){
		// TODO(fusion): This is similar to `clear_field` except we look for an
		// field without `UNLAY`. It is probably an inlined function.
		int ObjX, ObjY, ObjZ;
		get_object_coordinates(Obj, &ObjX, &ObjY, &ObjZ);

		int OffsetX[4] = { 1,  0, -1,  0};
		int OffsetY[4] = { 0,  1,  0, -1};
		for(int i = 0; i < 4; i += 1){
			int DestX = ObjX + OffsetX[i];
			int DestY = ObjY + OffsetY[i];
			int DestZ = ObjZ;
			if(coordinate_flag(DestX, DestY, DestZ, BANK)
			&& !coordinate_flag(DestX, DestY, DestZ, UNPASS)){
				Object Dest = get_map_container(DestX, DestY, DestZ);
				move_all_objects(Obj.get_next_object(), Dest, NONE, true);
				break;
			}
		}
	}

	change(Obj, ChangeTarget, 0);
}

void use_object(uint32 CreatureID, Object Obj){
	if(!Obj.exists()){
		error("use_objects: Provided object does not exist.\n");
		throw ERROR;
	}

	if(!Obj.get_object_type().get_flag(USEEVENT)){
		error("use_objects: Provided object is not usable.\n");
		throw ERROR;
	}

	Object User = NONE;
	if(CreatureID != 0){
		TCreature *Creature = GetCreature(CreatureID);
		if(Creature != NULL){
			User = Creature->CrObject;
		}
	}

	if(!handle_event(MOVEUSE_EVENT_USE, User, Obj, NONE)){
		throw NOTUSABLE;
	}
}

void use_objects(uint32 CreatureID, Object Obj1, Object Obj2){
	if(!Obj1.exists()){
		error("use_objects: Provided object Obj1 does not exist.\n");
		throw ERROR;
	}

	if(!Obj2.exists()){
		error("use_objects: Provided object Obj2 does not exist.\n");
		throw ERROR;
	}

	if(!Obj1.get_object_type().get_flag(USEEVENT)){
		error("use_objects: Provided object Obj1 is not usable.\n");
		throw ERROR;
	}

	Object User = NONE;
	if(CreatureID != 0){
		TCreature *Creature = GetCreature(CreatureID);
		if(Creature != NULL){
			User = Creature->CrObject;
		}
	}

	if(!handle_event(MOVEUSE_EVENT_MULTIUSE, User, Obj1, Obj2)){
		throw NOTUSABLE;
	}
}

void movement_event(Object Obj, Object Start, Object Dest){
	if(!Obj.exists()){
		error("movement_event: Provided object does not exist.\n");
		throw ERROR;
	}

	ObjectType StartType = Start.get_object_type();
	if(!Start.exists() || (!StartType.get_flag(CONTAINER) && !StartType.get_flag(CHEST))){
		error("movement_event: \"Start\" is not a container.\n");
		throw ERROR;
	}

	ObjectType DestType = Dest.get_object_type();
	if(!Dest.exists() || (!DestType.get_flag(CONTAINER) && !DestType.get_flag(CHEST))){
		error("movement_event: \"Dest\" is not a container.\n");
		throw ERROR;
	}

	if(Obj.get_object_type().get_flag(MOVEMENTEVENT)){
		handle_event(MOVEUSE_EVENT_MOVEMENT, NONE, Obj, NONE);
		if(!Obj.exists()){
			throw DESTROYED;
		}
	}
}

void separation_event(Object Obj, Object Start){
	if(!Obj.exists()){
		error("separation_event: Provided object does not exist.\n");
		throw ERROR;
	}

	if(!Start.exists()){
		error("separation_event: Provided container does not exist.\n");
		throw ERROR;
	}

	ObjectType StartType = Start.get_object_type();
	if(!StartType.is_map_container()){
		return;
	}

	// NOTE(fusion): Separation event for `Obj` against objects at `Start`.
	ObjectType ObjType = Obj.get_object_type();
	if(ObjType.get_flag(SEPARATIONEVENT)){
		Object Help = get_first_container_object(Start);
		while(Help != NONE){
			Object Next = Help.get_next_object();
			if(Help != Obj){
				handle_event(MOVEUSE_EVENT_SEPARATION, NONE, Obj, Help);
				if(!Obj.exists()){
					throw DESTROYED;
				}
			}
			Help = Next;
		}
	}

	// NOTE(fusion): Separation event for objects at `Start` against `Obj`.
	Object Help = get_first_container_object(Start);
	while(Help != NONE){
		Object Next = Help.get_next_object();
		if(Help != Obj){
			ObjectType HelpType = Help.get_object_type();
			if(ObjType.is_creature_container()
					&& (HelpType.get_flag(LEVELDOOR) || HelpType.get_flag(QUESTDOOR))){
				ObjectType DoorTarget = HelpType.get_flag(LEVELDOOR)
						? (int)HelpType.get_attribute(LEVELDOORTARGET)
						: (int)HelpType.get_attribute(QUESTDOORTARGET);
				if(DoorTarget.is_map_container() || !DoorTarget.get_flag(UNPASS)){
					error("separation_event: Target door for door %d not specified or passable.\n",
							HelpType.TypeID);
					throw ERROR;
				}

				clear_field(Help, Obj);
				change(Help, DoorTarget, 0);
				break;
			}

			if(HelpType.get_flag(SEPARATIONEVENT)){
				handle_event(MOVEUSE_EVENT_SEPARATION, NONE, Help, Obj);
				if(!Obj.exists()){
					throw DESTROYED;
				}
			}
		}
		Help = Next;
	}
}

void collision_event(Object Obj, Object Dest){
	if(!Obj.exists()){
		error("collision_event: Provided object does not exist.\n");
		throw ERROR;
	}

	if(!Dest.exists()){
		error("collision_event: Provided container does not exist.\n");
		throw ERROR;
	}

	ObjectType DestType = Dest.get_object_type();
	if(!DestType.is_map_container()){
		return;
	}

	ObjectType ObjType = Obj.get_object_type();
	if(ObjType.get_flag(TELEPORTABSOLUTE) || ObjType.get_flag(TELEPORTRELATIVE)){
		int TeleportEffect;
		int TeleportDestX, TeleportDestY, TeleportDestZ;
		if(ObjType.get_flag(TELEPORTABSOLUTE)){
			unpack_absolute_coordinate((int)Obj.get_attribute(ABSTELEPORTDESTINATION),
									&TeleportDestX, &TeleportDestY, &TeleportDestZ);
			TeleportEffect = (int)ObjType.get_attribute(ABSTELEPORTEFFECT);
		}else{
			int DisplacementX, DisplacementY, DisplacementZ;
			get_object_coordinates(Obj, &TeleportDestX, &TeleportDestY, &TeleportDestZ);
			unpack_relative_coordinate((int)ObjType.get_attribute(RELTELEPORTDISPLACEMENT),
									&DisplacementX, &DisplacementY, &DisplacementZ);
			TeleportDestX += DisplacementX;
			TeleportDestY += DisplacementY;
			TeleportDestZ += DisplacementZ;
			TeleportEffect = (int)ObjType.get_attribute(RELTELEPORTEFFECT);
		}

		Object TeleportDest = get_map_container(TeleportDestX, TeleportDestY, TeleportDestZ);
		move_all_objects(Obj.get_next_object(), TeleportDest, NONE, true);
		graphical_effect(TeleportDestX, TeleportDestY, TeleportDestZ, TeleportEffect);

		// TODO(fusion): Is this even possible? Maybe `move_all_objects` could
		// trigger a separation event that destroys `Obj`?
		if(!Obj.exists()){
			throw DESTROYED;
		}
	}else{
		// NOTE(fusion): Collision event for `Obj` against objects at `Dest`.
		if(ObjType.get_flag(COLLISIONEVENT)){
			Object Help = get_first_container_object(Dest);
			while(Help != NONE){
				Object Next = Help.get_next_object();

				// IMPORTANT(fusion): Same as below.
				if(Next == Obj){
					Next = Next.get_next_object();
				}

				if(Help != Obj){
					handle_event(MOVEUSE_EVENT_COLLISION, NONE, Obj, Help);
					if(!Obj.exists()){
						throw DESTROYED;
					}
				}

				Help = Next;
			}
		}

		// NOTE(fusion): Collision event for objects at `Dest` against `Obj`.
		Object Help = get_first_container_object(Dest);
		while(Help != NONE){
			Object Next = Help.get_next_object();

			// IMPORTANT(fusion): This is very subtle but the collision event can
			// move or destroy either object, causing it to be removed from the
			// container list while we're still iterating it.
			//  It won't be a problem most of the time, but if Next happens to be
			// Obj, and it gets moved, we could end up iterating the remainder of
			// whatever list Obj now belongs to, causing side effects like double
			// collision effects, etc...
			//  Note that this cannot happen to Help simply because it is already
			// behind the Next pointer, so checking only for Obj here is enough.
			if(Next == Obj){
				Next = Next.get_next_object();
			}

			if(Help != Obj){
				ObjectType HelpType = Help.get_object_type();
				if(HelpType.get_flag(TELEPORTABSOLUTE) || HelpType.get_flag(TELEPORTRELATIVE)){
					int TeleportEffect;
					int TeleportDestX, TeleportDestY, TeleportDestZ;
					if(HelpType.get_flag(TELEPORTABSOLUTE)){
						unpack_absolute_coordinate((int)Help.get_attribute(ABSTELEPORTDESTINATION),
												&TeleportDestX, &TeleportDestY, &TeleportDestZ);
						TeleportEffect = (int)HelpType.get_attribute(ABSTELEPORTEFFECT);
					}else{
						int DisplacementX, DisplacementY, DisplacementZ;
						get_object_coordinates(Help, &TeleportDestX, &TeleportDestY, &TeleportDestZ);
						unpack_relative_coordinate((int)HelpType.get_attribute(RELTELEPORTDISPLACEMENT),
												&DisplacementX, &DisplacementY, &DisplacementZ);
						TeleportDestX += DisplacementX;
						TeleportDestY += DisplacementY;
						TeleportDestZ += DisplacementZ;
						TeleportEffect = (int)HelpType.get_attribute(RELTELEPORTEFFECT);
					}

					Object TeleportDest = get_map_container(TeleportDestX, TeleportDestY, TeleportDestZ);
					move_one_object(Obj, TeleportDest);
					graphical_effect(TeleportDestX, TeleportDestY, TeleportDestZ, TeleportEffect);
				}else if(HelpType.get_flag(COLLISIONEVENT)){
					handle_event(MOVEUSE_EVENT_COLLISION, NONE, Help, Obj);
				}

				if(!Obj.exists()){
					throw DESTROYED;
				}
			}

			Help = Next;
		}
	}
}

// Event Loading / Initialization
// =============================================================================
void load_parameters(ReadScriptFile *Script, int *Parameters, int NumberOfParameters, ...){
	if(NumberOfParameters > MOVEUSE_MAX_PARAMETERS){
		error("load_parameters: Too many parameters (%d).\n", NumberOfParameters);
		Script->error("too many parameters");
	}

	if(NumberOfParameters == 0){
		return;
	}

	if(Script->Token != SPECIAL || Script->get_special() != '('){
		Script->error("'(' expected");
	}

	va_list ap;
	va_start(ap, NumberOfParameters);
	for(int i = 0; i < NumberOfParameters; i += 1){
		if(i > 0){
			Script->read_symbol(',');
		}

		int ParamType = va_arg(ap, int);
		switch(ParamType){
			case MOVEUSE_PARAMETER_OBJECT:{
				const char *Object = Script->read_identifier();
				if(strcmp(Object, "null") == 0){
					Parameters[i] = 0; // MOVEUSE_OBJECT_NULL ?
				}else if(strcmp(Object, "obj1") == 0){
					Parameters[i] = 1; // MOVEUSE_OBJECT_1 ?
				}else if(strcmp(Object, "obj2") == 0){
					Parameters[i] = 2; // MOVEUSE_OBJECT_2 ?
				}else if(strcmp(Object, "user") == 0){
					Parameters[i] = 3; // MOVEUSE_OBJECT_USER ?
				}else if(strcmp(Object, "temp") == 0){
					Parameters[i] = 4; // MOVEUSE_OBJECT_TEMP ?
				}else{
					Script->error("Object expected");
				}
				break;
			}

			case MOVEUSE_PARAMETER_TYPE:{
				int TypeID = Script->read_number();
				Parameters[i] = TypeID;
				if(!object_type_exists(TypeID)){
					Script->error("Unknown object type");
				}
				break;
			}

			case MOVEUSE_PARAMETER_FLAG:{
				int Flag = get_flag_by_name(Script->read_identifier());
				Parameters[i] = Flag;
				if(Flag == -1){
					Script->error("Unknown flag");
				}
				break;
			}

			case MOVEUSE_PARAMETER_TYPEATTRIBUTE:{
				int TypeAttribute = get_type_attribute_by_name(Script->read_identifier());
				Parameters[i] = TypeAttribute;
				if(TypeAttribute == -1){
					Script->error("Unknown type attribute");
				}
				break;
			}

			case MOVEUSE_PARAMETER_INSTANCEATTRIBUTE:{
				int InstanceAttribute = get_instance_attribute_by_name(Script->read_identifier());
				Parameters[i] = InstanceAttribute;
				if(InstanceAttribute == -1){
					Script->error("Unknown instance attribute");
				}
				break;
			}

			case MOVEUSE_PARAMETER_COORDINATE:{
				int AbsX, AbsY, AbsZ;
				Script->read_coordinate(&AbsX, &AbsY, &AbsZ);
				Parameters[i] = pack_absolute_coordinate(AbsX, AbsY, AbsZ);
				break;
			}

			case MOVEUSE_PARAMETER_VECTOR:{
				int RelX, RelY, RelZ;
				Script->read_coordinate(&RelX, &RelY, &RelZ);
				Parameters[i] = pack_relative_coordinate(RelX, RelY, RelZ);
				break;
			}

			case MOVEUSE_PARAMETER_RIGHT:{
				const char *Right = Script->read_identifier();
				if(strcmp(Right, "premium_account") == 0){
					Parameters[i] = PREMIUM_ACCOUNT;
				}else if(strcmp(Right, "special_moveuse") == 0){
					Parameters[i] = SPECIAL_MOVEUSE;
				}else{
					Script->error("Unknown right");
				}
				break;
			}

			case MOVEUSE_PARAMETER_SKILL:{
				int SkillNr = GetSkillByName(Script->read_identifier());
				Parameters[i] = SkillNr;
				if(SkillNr == -1){
					Script->error("Unknown skill");
				}
				break;
			}

			case MOVEUSE_PARAMETER_NUMBER:{
				Parameters[i] = Script->read_number();
				break;
			}

			case MOVEUSE_PARAMETER_TEXT:{
				const char *Text = Script->read_string();
				Parameters[i] = AddDynamicString(Text);
				break;
			}

			case MOVEUSE_PARAMETER_COMPARISON:{
				int Operator = (int)Script->read_special();
				Parameters[i] = Operator;
				if(strchr("<=>GLN", Operator) == NULL){
					Script->error("Unknown comparison operator");
				}
				break;
			}
		}
	}
	va_end(ap);

	Script->read_symbol(')');
	Script->next_token();
}

void load_condition(ReadScriptFile *Script, MoveUseCondition *Condition){
	char Identifier[MAX_IDENT_LENGTH];
	strcpy(Identifier, Script->get_identifier());

	Script->next_token();
	if(strcmp(Identifier, "isposition") == 0){
		Condition->Condition = MOVEUSE_CONDITION_ISPOSITION;
		load_parameters(Script, Condition->Parameters, 2,
								MOVEUSE_PARAMETER_OBJECT,
								MOVEUSE_PARAMETER_COORDINATE);
	}else if(strcmp(Identifier, "istype") == 0){
		Condition->Condition = MOVEUSE_CONDITION_ISTYPE;
		load_parameters(Script, Condition->Parameters, 2,
								MOVEUSE_PARAMETER_OBJECT,
								MOVEUSE_PARAMETER_TYPE);
	}else if(strcmp(Identifier, "iscreature") == 0){
		Condition->Condition = MOVEUSE_CONDITION_ISCREATURE;
		load_parameters(Script, Condition->Parameters, 1,
								MOVEUSE_PARAMETER_OBJECT);
	}else if(strcmp(Identifier, "isplayer") == 0){
		Condition->Condition = MOVEUSE_CONDITION_ISPLAYER;
		load_parameters(Script, Condition->Parameters, 1,
								MOVEUSE_PARAMETER_OBJECT);
	}else if(strcmp(Identifier, "hasflag") == 0){
		Condition->Condition = MOVEUSE_CONDITION_HASFLAG;
		load_parameters(Script, Condition->Parameters, 2,
								MOVEUSE_PARAMETER_OBJECT,
								MOVEUSE_PARAMETER_FLAG);
	}else if(strcmp(Identifier, "hastypeattribute") == 0){
		Condition->Condition = MOVEUSE_CONDITION_HASTYPEATTRIBUTE;
		load_parameters(Script, Condition->Parameters, 4,
								MOVEUSE_PARAMETER_OBJECT,
								MOVEUSE_PARAMETER_TYPEATTRIBUTE,
								MOVEUSE_PARAMETER_COMPARISON,
								MOVEUSE_PARAMETER_NUMBER);
	}else if(strcmp(Identifier, "hasinstanceattribute") == 0){
		Condition->Condition = MOVEUSE_CONDITION_HASINSTANCEATTRIBUTE;
		load_parameters(Script, Condition->Parameters, 4,
								MOVEUSE_PARAMETER_OBJECT,
								MOVEUSE_PARAMETER_INSTANCEATTRIBUTE,
								MOVEUSE_PARAMETER_COMPARISON,
								MOVEUSE_PARAMETER_NUMBER);
	}else if(strcmp(Identifier, "hastext") == 0){
		Condition->Condition = MOVEUSE_CONDITION_HASTEXT;
		load_parameters(Script, Condition->Parameters, 2,
								MOVEUSE_PARAMETER_OBJECT,
								MOVEUSE_PARAMETER_TEXT);
	}else if(strcmp(Identifier, "ispeaceful") == 0){
		Condition->Condition = MOVEUSE_CONDITION_ISPEACEFUL;
		load_parameters(Script, Condition->Parameters, 1,
								MOVEUSE_PARAMETER_OBJECT);
	}else if(strcmp(Identifier, "maylogout") == 0){
		Condition->Condition = MOVEUSE_CONDITION_MAYLOGOUT;
		load_parameters(Script, Condition->Parameters, 1,
								MOVEUSE_PARAMETER_OBJECT);
	}else if(strcmp(Identifier, "hasprofession") == 0){
		Condition->Condition = MOVEUSE_CONDITION_HASPROFESSION;
		load_parameters(Script, Condition->Parameters, 2,
								MOVEUSE_PARAMETER_OBJECT,
								MOVEUSE_PARAMETER_NUMBER);
	}else if(strcmp(Identifier, "haslevel") == 0){
		Condition->Condition = MOVEUSE_CONDITION_HASLEVEL;
		load_parameters(Script, Condition->Parameters, 3,
								MOVEUSE_PARAMETER_OBJECT,
								MOVEUSE_PARAMETER_COMPARISON,
								MOVEUSE_PARAMETER_NUMBER);
	}else if(strcmp(Identifier, "hasright") == 0){
		Condition->Condition = MOVEUSE_CONDITION_HASRIGHT;
		load_parameters(Script, Condition->Parameters, 2,
								MOVEUSE_PARAMETER_OBJECT,
								MOVEUSE_PARAMETER_RIGHT);
	}else if(strcmp(Identifier, "hasquestvalue") == 0){
		Condition->Condition = MOVEUSE_CONDITION_HASQUESTVALUE;
		load_parameters(Script, Condition->Parameters, 4,
								MOVEUSE_PARAMETER_OBJECT,
								MOVEUSE_PARAMETER_NUMBER,
								MOVEUSE_PARAMETER_COMPARISON,
								MOVEUSE_PARAMETER_NUMBER);
	}else if(strcmp(Identifier, "testskill") == 0){
		Condition->Condition = MOVEUSE_CONDITION_TESTSKILL;
		load_parameters(Script, Condition->Parameters, 4,
								MOVEUSE_PARAMETER_OBJECT,
								MOVEUSE_PARAMETER_SKILL,
								MOVEUSE_PARAMETER_NUMBER,
								MOVEUSE_PARAMETER_NUMBER);
	}else if(strcmp(Identifier, "countobjects") == 0){
		Condition->Condition = MOVEUSE_CONDITION_COUNTOBJECTS;
		load_parameters(Script, Condition->Parameters, 3,
								MOVEUSE_PARAMETER_OBJECT,
								MOVEUSE_PARAMETER_COMPARISON,
								MOVEUSE_PARAMETER_NUMBER);
	}else if(strcmp(Identifier, "countobjectsonmap") == 0){
		Condition->Condition = MOVEUSE_CONDITION_COUNTOBJECTSONMAP;
		load_parameters(Script, Condition->Parameters, 3,
								MOVEUSE_PARAMETER_COORDINATE,
								MOVEUSE_PARAMETER_COMPARISON,
								MOVEUSE_PARAMETER_NUMBER);
	}else if(strcmp(Identifier, "isobjectthere") == 0){
		Condition->Condition = MOVEUSE_CONDITION_ISOBJECTTHERE;
		load_parameters(Script, Condition->Parameters, 2,
								MOVEUSE_PARAMETER_COORDINATE,
								MOVEUSE_PARAMETER_TYPE);
	}else if(strcmp(Identifier, "iscreaturethere") == 0){
		Condition->Condition = MOVEUSE_CONDITION_ISCREATURETHERE;
		load_parameters(Script, Condition->Parameters, 1,
								MOVEUSE_PARAMETER_COORDINATE);
	}else if(strcmp(Identifier, "isplayerthere") == 0){
		Condition->Condition = MOVEUSE_CONDITION_ISPLAYERTHERE;
		load_parameters(Script, Condition->Parameters, 1,
								MOVEUSE_PARAMETER_COORDINATE);
	}else if(strcmp(Identifier, "isobjectininventory") == 0){
		Condition->Condition = MOVEUSE_CONDITION_ISOBJECTININVENTORY;
		load_parameters(Script, Condition->Parameters, 3,
								MOVEUSE_PARAMETER_OBJECT,
								MOVEUSE_PARAMETER_TYPE,
								MOVEUSE_PARAMETER_NUMBER);
	}else if(strcmp(Identifier, "isprotectionzone") == 0){
		Condition->Condition = MOVEUSE_CONDITION_ISPROTECTIONZONE;
		load_parameters(Script, Condition->Parameters, 1,
								MOVEUSE_PARAMETER_OBJECT);
	}else if(strcmp(Identifier, "ishouse") == 0){
		Condition->Condition = MOVEUSE_CONDITION_ISHOUSE;
		load_parameters(Script, Condition->Parameters, 1,
								MOVEUSE_PARAMETER_OBJECT);
	}else if(strcmp(Identifier, "ishouseowner") == 0){
		Condition->Condition = MOVEUSE_CONDITION_ISHOUSEOWNER;
		load_parameters(Script, Condition->Parameters, 2,
								MOVEUSE_PARAMETER_OBJECT,
								MOVEUSE_PARAMETER_OBJECT);
	}else if(strcmp(Identifier, "isdressed") == 0){
		Condition->Condition = MOVEUSE_CONDITION_ISDRESSED;
		load_parameters(Script, Condition->Parameters, 1,
								MOVEUSE_PARAMETER_OBJECT);
	}else if(strcmp(Identifier, "random") == 0){
		Condition->Condition = MOVEUSE_CONDITION_RANDOM;
		load_parameters(Script, Condition->Parameters, 1,
								MOVEUSE_PARAMETER_NUMBER);
	}else{
		Script->error("invalid condition");
	}
}

void load_action(ReadScriptFile *Script, MoveUseAction *Action){
	char Identifier[MAX_IDENT_LENGTH];
	strcpy(Identifier, Script->get_identifier());

	Script->next_token();
	if(strcmp(Identifier, "createonmap") == 0){
		Action->Action = MOVEUSE_ACTION_CREATEONMAP;
		load_parameters(Script, Action->Parameters, 3,
								MOVEUSE_PARAMETER_COORDINATE,
								MOVEUSE_PARAMETER_TYPE,
								MOVEUSE_PARAMETER_NUMBER);
	}else if(strcmp(Identifier, "create") == 0){
		Action->Action = MOVEUSE_ACTION_CREATE;
		load_parameters(Script, Action->Parameters, 3,
								MOVEUSE_PARAMETER_OBJECT,
								MOVEUSE_PARAMETER_TYPE,
								MOVEUSE_PARAMETER_NUMBER);
	}else if(strcmp(Identifier, "monsteronmap") == 0){
		Action->Action = MOVEUSE_ACTION_MONSTERONMAP;
		load_parameters(Script, Action->Parameters, 2,
								MOVEUSE_PARAMETER_COORDINATE,
								MOVEUSE_PARAMETER_NUMBER);
	}else if(strcmp(Identifier, "monster") == 0){
		Action->Action = MOVEUSE_ACTION_MONSTER;
		load_parameters(Script, Action->Parameters, 2,
								MOVEUSE_PARAMETER_OBJECT,
								MOVEUSE_PARAMETER_NUMBER);
	}else if(strcmp(Identifier, "effectonmap") == 0){
		Action->Action = MOVEUSE_ACTION_EFFECTONMAP;
		load_parameters(Script, Action->Parameters, 2,
								MOVEUSE_PARAMETER_COORDINATE,
								MOVEUSE_PARAMETER_NUMBER);
	}else if(strcmp(Identifier, "effect") == 0){
		Action->Action = MOVEUSE_ACTION_EFFECT;
		load_parameters(Script, Action->Parameters, 2,
								MOVEUSE_PARAMETER_OBJECT,
								MOVEUSE_PARAMETER_NUMBER);
	}else if(strcmp(Identifier, "textonmap") == 0){
		Action->Action = MOVEUSE_ACTION_TEXTONMAP;
		load_parameters(Script, Action->Parameters, 3,
								MOVEUSE_PARAMETER_COORDINATE,
								MOVEUSE_PARAMETER_TEXT,
								MOVEUSE_PARAMETER_NUMBER);
	}else if(strcmp(Identifier, "text") == 0){
		Action->Action = MOVEUSE_ACTION_TEXT;
		load_parameters(Script, Action->Parameters, 3,
								MOVEUSE_PARAMETER_OBJECT,
								MOVEUSE_PARAMETER_TEXT,
								MOVEUSE_PARAMETER_NUMBER);
	}else if(strcmp(Identifier, "changeonmap") == 0){
		Action->Action = MOVEUSE_ACTION_CHANGEONMAP;
		load_parameters(Script, Action->Parameters, 4,
								MOVEUSE_PARAMETER_COORDINATE,
								MOVEUSE_PARAMETER_TYPE,
								MOVEUSE_PARAMETER_TYPE,
								MOVEUSE_PARAMETER_NUMBER);
	}else if(strcmp(Identifier, "change") == 0){
		Action->Action = MOVEUSE_ACTION_CHANGE;
		load_parameters(Script, Action->Parameters, 3,
								MOVEUSE_PARAMETER_OBJECT,
								MOVEUSE_PARAMETER_TYPE,
								MOVEUSE_PARAMETER_NUMBER);
	}else if(strcmp(Identifier, "changerel") == 0){
		Action->Action = MOVEUSE_ACTION_CHANGEREL;
		load_parameters(Script, Action->Parameters, 5,
								MOVEUSE_PARAMETER_OBJECT,
								MOVEUSE_PARAMETER_VECTOR,
								MOVEUSE_PARAMETER_TYPE,
								MOVEUSE_PARAMETER_TYPE,
								MOVEUSE_PARAMETER_NUMBER);
	}else if(strcmp(Identifier, "setattribute") == 0){
		Action->Action = MOVEUSE_ACTION_SETATTRIBUTE;
		load_parameters(Script, Action->Parameters, 3,
								MOVEUSE_PARAMETER_OBJECT,
								MOVEUSE_PARAMETER_INSTANCEATTRIBUTE,
								MOVEUSE_PARAMETER_NUMBER);
	}else if(strcmp(Identifier, "changeattribute") == 0){
		Action->Action = MOVEUSE_ACTION_CHANGEATTRIBUTE;
		load_parameters(Script, Action->Parameters, 3,
								MOVEUSE_PARAMETER_OBJECT,
								MOVEUSE_PARAMETER_INSTANCEATTRIBUTE,
								MOVEUSE_PARAMETER_NUMBER);
	}else if(strcmp(Identifier, "setquestvalue") == 0){
		Action->Action = MOVEUSE_ACTION_SETQUESTVALUE;
		load_parameters(Script, Action->Parameters, 3,
								MOVEUSE_PARAMETER_OBJECT,
								MOVEUSE_PARAMETER_NUMBER,
								MOVEUSE_PARAMETER_NUMBER);
	}else if(strcmp(Identifier, "damage") == 0){
		Action->Action = MOVEUSE_ACTION_DAMAGE;
		load_parameters(Script, Action->Parameters, 4,
								MOVEUSE_PARAMETER_OBJECT,
								MOVEUSE_PARAMETER_OBJECT,
								MOVEUSE_PARAMETER_NUMBER,
								MOVEUSE_PARAMETER_NUMBER);
	}else if(strcmp(Identifier, "setstart") == 0){
		Action->Action = MOVEUSE_ACTION_SETSTART;
		load_parameters(Script, Action->Parameters, 2,
								MOVEUSE_PARAMETER_OBJECT,
								MOVEUSE_PARAMETER_COORDINATE);
	}else if(strcmp(Identifier, "writename") == 0){
		Action->Action = MOVEUSE_ACTION_WRITENAME;
		load_parameters(Script, Action->Parameters, 3,
								MOVEUSE_PARAMETER_OBJECT,
								MOVEUSE_PARAMETER_TEXT,
								MOVEUSE_PARAMETER_OBJECT);
	}else if(strcmp(Identifier, "writetext") == 0){
		Action->Action = MOVEUSE_ACTION_WRITETEXT;
		load_parameters(Script, Action->Parameters, 2,
								MOVEUSE_PARAMETER_TEXT,
								MOVEUSE_PARAMETER_OBJECT);
	}else if(strcmp(Identifier, "logout") == 0){
		Action->Action = MOVEUSE_ACTION_LOGOUT;
		load_parameters(Script, Action->Parameters, 1,
								MOVEUSE_PARAMETER_OBJECT);
	}else if(strcmp(Identifier, "moveallonmap") == 0){
		Action->Action = MOVEUSE_ACTION_MOVEALLONMAP;
		load_parameters(Script, Action->Parameters, 2,
								MOVEUSE_PARAMETER_COORDINATE,
								MOVEUSE_PARAMETER_COORDINATE);
	}else if(strcmp(Identifier, "moveall") == 0){
		Action->Action = MOVEUSE_ACTION_MOVEALL;
		load_parameters(Script, Action->Parameters, 2,
								MOVEUSE_PARAMETER_OBJECT,
								MOVEUSE_PARAMETER_COORDINATE);
	}else if(strcmp(Identifier, "moveallrel") == 0){
		Action->Action = MOVEUSE_ACTION_MOVEALLREL;
		load_parameters(Script, Action->Parameters, 2,
								MOVEUSE_PARAMETER_OBJECT,
								MOVEUSE_PARAMETER_VECTOR);
	}else if(strcmp(Identifier, "movetoponmap") == 0){
		Action->Action = MOVEUSE_ACTION_MOVETOPONMAP;
		load_parameters(Script, Action->Parameters, 3,
								MOVEUSE_PARAMETER_COORDINATE,
								MOVEUSE_PARAMETER_TYPE,
								MOVEUSE_PARAMETER_COORDINATE);
	}else if(strcmp(Identifier, "movetop") == 0){
		Action->Action = MOVEUSE_ACTION_MOVETOP;
		load_parameters(Script, Action->Parameters, 2,
								MOVEUSE_PARAMETER_OBJECT,
								MOVEUSE_PARAMETER_COORDINATE);
	}else if(strcmp(Identifier, "movetoprel") == 0){
		Action->Action = MOVEUSE_ACTION_MOVETOPREL;
		load_parameters(Script, Action->Parameters, 2,
								MOVEUSE_PARAMETER_OBJECT,
								MOVEUSE_PARAMETER_VECTOR);
	}else if(strcmp(Identifier, "move") == 0){
		Action->Action = MOVEUSE_ACTION_MOVE;
		load_parameters(Script, Action->Parameters, 2,
								MOVEUSE_PARAMETER_OBJECT,
								MOVEUSE_PARAMETER_COORDINATE);
	}else if(strcmp(Identifier, "moverel") == 0){
		Action->Action = MOVEUSE_ACTION_MOVEREL;
		load_parameters(Script, Action->Parameters, 3,
								MOVEUSE_PARAMETER_OBJECT,
								MOVEUSE_PARAMETER_OBJECT,
								MOVEUSE_PARAMETER_VECTOR);
	}else if(strcmp(Identifier, "retrieve") == 0){
		Action->Action = MOVEUSE_ACTION_RETRIEVE;
		load_parameters(Script, Action->Parameters, 3,
								MOVEUSE_PARAMETER_OBJECT,
								MOVEUSE_PARAMETER_VECTOR,
								MOVEUSE_PARAMETER_VECTOR);
	}else if(strcmp(Identifier, "deleteallonmap") == 0){
		Action->Action = MOVEUSE_ACTION_DELETEALLONMAP;
		load_parameters(Script, Action->Parameters, 1,
								MOVEUSE_PARAMETER_COORDINATE);
	}else if(strcmp(Identifier, "deletetoponmap") == 0){
		Action->Action = MOVEUSE_ACTION_DELETETOPONMAP;
		load_parameters(Script, Action->Parameters, 2,
								MOVEUSE_PARAMETER_COORDINATE,
								MOVEUSE_PARAMETER_TYPE);
	}else if(strcmp(Identifier, "deleteonmap") == 0){
		Action->Action = MOVEUSE_ACTION_DELETEONMAP;
		load_parameters(Script, Action->Parameters, 2,
								MOVEUSE_PARAMETER_COORDINATE,
								MOVEUSE_PARAMETER_TYPE);
	}else if(strcmp(Identifier, "delete") == 0){
		Action->Action = MOVEUSE_ACTION_DELETE;
		load_parameters(Script, Action->Parameters, 1,
								MOVEUSE_PARAMETER_OBJECT);
	}else if(strcmp(Identifier, "deleteininventory") == 0){
		Action->Action = MOVEUSE_ACTION_DELETEININVENTORY;
		load_parameters(Script, Action->Parameters, 3,
								MOVEUSE_PARAMETER_OBJECT,
								MOVEUSE_PARAMETER_TYPE,
								MOVEUSE_PARAMETER_NUMBER);
	}else if(strcmp(Identifier, "description") == 0){
		Action->Action = MOVEUSE_ACTION_DESCRIPTION;
		load_parameters(Script, Action->Parameters, 2,
								MOVEUSE_PARAMETER_OBJECT,
								MOVEUSE_PARAMETER_OBJECT);
	}else if(strcmp(Identifier, "loaddepot") == 0){
		Action->Action = MOVEUSE_ACTION_LOADDEPOT;
		load_parameters(Script, Action->Parameters, 4,
								MOVEUSE_PARAMETER_COORDINATE,
								MOVEUSE_PARAMETER_TYPE,
								MOVEUSE_PARAMETER_OBJECT,
								MOVEUSE_PARAMETER_NUMBER);
	}else if(strcmp(Identifier, "savedepot") == 0){
		Action->Action = MOVEUSE_ACTION_SAVEDEPOT;
		load_parameters(Script, Action->Parameters, 4,
								MOVEUSE_PARAMETER_COORDINATE,
								MOVEUSE_PARAMETER_TYPE,
								MOVEUSE_PARAMETER_OBJECT,
								MOVEUSE_PARAMETER_NUMBER);
	}else if(strcmp(Identifier, "sendmail") == 0){
		Action->Action = MOVEUSE_ACTION_SENDMAIL;
		load_parameters(Script, Action->Parameters, 1,
								MOVEUSE_PARAMETER_OBJECT);
	}else if(strcmp(Identifier, "nop") == 0){
		Action->Action = MOVEUSE_ACTION_NOP;
		load_parameters(Script, Action->Parameters, 0);
	}else{
		Script->error("invalid action");
	}
}

void load_database(void){
	print(1, "Loading move/use database ...\n");

	char FileName[4096];
	snprintf(FileName, sizeof(FileName), "%s/moveuse.dat", DATAPATH);

	for(int i = 0; i < NARRAY(MoveUseDatabases); i += 1){
		MoveUseDatabases[i].NumberOfRules = 0;
	}

	ReadScriptFile Script;
	Script.open(FileName);
	Script.next_token();
	while(Script.Token != ENDOFFILE){
		const char *Identifier = Script.get_identifier();

		if(strcmp(Identifier, "begin") == 0){
			Script.read_string();
			Script.next_token();
			continue;
		}else if(strcmp(Identifier, "end") == 0){
			Script.next_token();
			continue;
		}

		// NOTE(fusion): `ReadScriptFile::error` will always throw but the
		// compiler might not be able to detect that and issue a warning.
		int EventType = 0;
		if(strcmp(Identifier, "use") == 0){
			EventType = MOVEUSE_EVENT_USE;
		}else if(strcmp(Identifier, "multiuse") == 0){
			EventType = MOVEUSE_EVENT_MULTIUSE;
		}else if(strcmp(Identifier, "movement") == 0){
			EventType = MOVEUSE_EVENT_MOVEMENT;
		}else if(strcmp(Identifier, "collision") == 0){
			EventType = MOVEUSE_EVENT_COLLISION;
		}else if(strcmp(Identifier, "separation") == 0){
			EventType = MOVEUSE_EVENT_SEPARATION;
		}else{
			Script.error("Unknown event type");
		}

		Script.read_symbol(',');

		MoveUseDatabases[EventType].NumberOfRules += 1;
		MoveUseRule *Rule = MoveUseDatabases[EventType].Rules.at(
					MoveUseDatabases[EventType].NumberOfRules);

		// NOTE(fusion): Conditions.
		Rule->FirstCondition = NumberOfMoveUseConditions + 1;
		while(true){
			Script.next_token();
			NumberOfMoveUseConditions += 1;
			MoveUseCondition *Condition = MoveUseConditions.at(NumberOfMoveUseConditions);
			Condition->Modifier = MOVEUSE_MODIFIER_NORMAL;
			if(Script.Token == SPECIAL){
				if(Script.get_special() == '!'){
					Script.next_token();
					Condition->Modifier = MOVEUSE_MODIFIER_INVERT;
				}else if(Script.get_special() == '~'){
					Script.next_token();
					Condition->Modifier = MOVEUSE_MODIFIER_TRUE;
				}
			}
			load_condition(&Script, Condition);

			if(Script.Token != SPECIAL || Script.get_special() != ','){
				break;
			}
		}
		Rule->LastCondition = NumberOfMoveUseConditions;

		if(Script.Token != SPECIAL || Script.get_special() != 'I'){
			Script.error("'->' expected");
		}

		// NOTE(fusion): Actions.
		Rule->FirstAction = NumberOfMoveUseActions + 1;
		while(true){
			Script.next_token();
			NumberOfMoveUseActions += 1;
			MoveUseAction *Action = MoveUseActions.at(NumberOfMoveUseActions);
			load_action(&Script, Action);

			if(Script.Token != SPECIAL || Script.get_special() != ','){
				break;
			}
		}
		Rule->LastAction = NumberOfMoveUseActions;

		// NOTE(fusion): Optional tag?
		if(Script.Token == STRING){
			Script.next_token();
		}
	}

	Script.close();

	print(1, "move/use database loaded with %d/%d/%d/%d/%d rules.\n",
			MoveUseDatabases[MOVEUSE_EVENT_USE].NumberOfRules,
			MoveUseDatabases[MOVEUSE_EVENT_MULTIUSE].NumberOfRules,
			MoveUseDatabases[MOVEUSE_EVENT_MOVEMENT].NumberOfRules,
			MoveUseDatabases[MOVEUSE_EVENT_COLLISION].NumberOfRules,
			MoveUseDatabases[MOVEUSE_EVENT_SEPARATION].NumberOfRules);
}

void init_move_use(void){
	NumberOfMoveUseConditions = 0;
	NumberOfMoveUseActions = 0;
	DelayedMails = 0;
	load_database();
}

void exit_move_use(void){
	for(int i = 0; i < DelayedMails; i += 1){
		error("exit_move_use: Parcel to %u was not delivered.\n",
				DelayedMailArray.at(i)->CharacterID);
	}
}
