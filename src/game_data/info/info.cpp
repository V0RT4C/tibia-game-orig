#include "game_data/info/info.h"
#include "cr.h"
#include "magic.h"

const char *get_liquid_name(int LiquidType){
	const char *LiquidName;
	switch(LiquidType) {
		case LIQUID_NONE:		LiquidName = "nothing"; break;
		case LIQUID_WATER:		LiquidName = "water"; break;
		case LIQUID_WINE:		LiquidName = "wine"; break;
		case LIQUID_BEER:		LiquidName = "beer"; break;
		case LIQUID_MUD:		LiquidName = "mud"; break;
		case LIQUID_BLOOD:		LiquidName = "blood"; break;
		case LIQUID_SLIME:		LiquidName = "slime"; break;
		case LIQUID_OIL:		LiquidName = "oil"; break;
		case LIQUID_URINE:		LiquidName = "urine"; break;
		case LIQUID_MILK:		LiquidName = "milk"; break;
		case LIQUID_MANA:		LiquidName = "manafluid"; break;
		case LIQUID_LIFE:		LiquidName = "lifefluid"; break;
		case LIQUID_LEMONADE:	LiquidName = "lemonade"; break;
		default:{
			error("get_liquid_name: Invalid liquid type %d\n", LiquidType);
			LiquidName = "unknown";
			break;
		}
	}
	return LiquidName;
}

uint8 get_liquid_color(int LiquidType){
	uint8 LiquidColor;
	switch(LiquidType){
		case LIQUID_NONE:		LiquidColor = LIQUID_COLORLESS; break;
		case LIQUID_WATER:		LiquidColor = LIQUID_BLUE; break;
		case LIQUID_WINE:		LiquidColor = LIQUID_PURPLE; break;
		case LIQUID_BEER:		LiquidColor = LIQUID_BROWN; break;
		case LIQUID_MUD:		LiquidColor = LIQUID_BROWN; break;
		case LIQUID_BLOOD:		LiquidColor = LIQUID_RED; break;
		case LIQUID_SLIME:		LiquidColor = LIQUID_GREEN; break;
		case LIQUID_OIL:		LiquidColor = LIQUID_BROWN; break;
		case LIQUID_URINE:		LiquidColor = LIQUID_YELLOW; break;
		case LIQUID_MILK:		LiquidColor = LIQUID_WHITE; break;
		case LIQUID_MANA:		LiquidColor = LIQUID_PURPLE; break;
		case LIQUID_LIFE:		LiquidColor = LIQUID_RED; break;
		case LIQUID_LEMONADE:	LiquidColor = LIQUID_YELLOW; break;
		default:{
			error("get_liquid_color: Invalid liquid type %d\n", LiquidType);
			LiquidColor = LIQUID_COLORLESS;
			break;
		}
	}
	return LiquidColor;
}

const char *get_name(Object Obj){
	char ObjectName[50] = {};
	ObjectType ObjType = Obj.get_object_type();
	if(ObjType.is_creature_container()){
		TCreature *Creature = GetCreature(Obj);
		if(Creature != NULL){
			snprintf(ObjectName, sizeof(ObjectName), "%s", Creature->Name);
		}else{
			error("get_name: Creature %d does not exist.\n", Obj.get_creature_id());
		}
	}else{
		// IMPORTANT(fusion): `ObjectType::get_name` returns the same static buffer
		// from `Plural`.
		const char *TypeName = ObjType.get_name(1);
		if(TypeName != NULL){
			strcpy(ObjectName, TypeName);
		}

		int LiquidType = LIQUID_NONE;
		if(ObjType.get_flag(LIQUIDCONTAINER)){
			LiquidType = (int)Obj.get_attribute(CONTAINERLIQUIDTYPE);
		}else if(ObjType.get_flag(LIQUIDPOOL)){
			LiquidType = (int)Obj.get_attribute(POOLLIQUIDTYPE);
		}

		if(LiquidType != LIQUID_NONE){
			strcat(ObjectName, " of ");
			strcat(ObjectName, get_liquid_name(LiquidType));
		}
	}

	int Count = 1;
	if(ObjType.get_flag(CUMULATIVE)){
		Count = (int)Obj.get_attribute(AMOUNT);
	}

	// IMPORTANT(fusion): `Plural` will return a static buffer.
	return Plural(ObjectName, Count);
}

const char *get_info(Object Obj){
	if(!Obj.exists()){
		error("get_info: Passed object does not exist.\n");
		return NULL;
	}

	return Obj.get_object_type().get_description();
}

int get_weight(Object Obj, int Count){
	if(!Obj.exists()){
		error("get_weight: Passed object does not exist.\n");
		return 0;
	}

	// TODO(fusion): Why do some UNTAKE items have weight, and even worse, hardcoded?

	int Result = 0;
	ObjectType ObjType = Obj.get_object_type();
	if(ObjType.get_flag(TAKE)){
		int Weight = (int)ObjType.get_attribute(WEIGHT);
		if(!ObjType.get_flag(CUMULATIVE)){
			Count = 1;
		}else if(Count == -1){
			Count = (int)Obj.get_attribute(AMOUNT);
		}
		Result = Weight * Count;
	}else if(ObjType.TypeID == 2904){ // LARGE AMPHORA
		Result = 19500;
	}else if(ObjType.TypeID == 3458){ // ANVIL
		Result = 50000;
	}else if(ObjType.TypeID == 3510){ // COAL BASIN
		Result = 22800;
	}else if(ObjType.TypeID == 4311){ // DEAD HUMAN
		Result = 80000;
	}else{
		error("get_weight: Object type %d is not takeable.\n", ObjType.TypeID);
	}
	return Result;
}

int get_complete_weight(Object Obj){
	int Result = get_weight(Obj, -1);
	ObjectType ObjType = Obj.get_object_type();
	if(ObjType.get_flag(CONTAINER) || ObjType.get_flag(CHEST)){
		Object Help = get_first_container_object(Obj);
		Result += get_row_weight(Help);
	}
	return Result;
}

int get_row_weight(Object Obj){
	int Result = 0;
	while(Obj != NONE){
		// TODO(fusion): This is probably `get_complete_weight` inlined.
		Result += get_weight(Obj, -1);
		ObjectType ObjType = Obj.get_object_type();
		if(ObjType.get_flag(CONTAINER) || ObjType.get_flag(CHEST)){
			Object Help = get_first_container_object(Obj);
			Result += get_row_weight(Help);
		}
		Obj = Obj.get_next_object();
	}
	return Result;
}

uint32 get_object_creature_id(Object Obj){
	if(!Obj.exists()){
		error("get_object_creature_id: Passed object does not exist.\n");
		return 0;
	}

	uint32 CreatureID = 0;
	while(Obj != NONE){
		ObjectType ObjType = Obj.get_object_type();
		if(ObjType.is_creature_container()){
			CreatureID = Obj.get_creature_id();
			break;
		}else if(ObjType.is_map_container()){
			break;
		}
		Obj = Obj.get_container();
	}
	return CreatureID;
}

// TODO(fusion): I'm not sure about this one. Objects inside containers would
// also return the body position of the container even if it isn't actively
// equipped.
int get_object_body_position(Object Obj){
	if(!Obj.exists()){
		error("get_object_body_position: Passed object does not exist.\n");
		return 0;
	}

	int Position = 0;
	while(Obj != NONE){
		ObjectType ObjType = Obj.get_object_type();
		if(ObjType.is_body_container()){
			// NOTE(fusion): Body container type ids match inventory slots exactly.
			Position = ObjType.TypeID;
			break;
		}else if(ObjType.is_map_container()){
			break;
		}
		Obj = Obj.get_container();
	}
	return Position;
}

int get_object_rnum(Object Obj){
	if(!Obj.exists()){
		error("get_object_rnum: Passed object does not exist.\n");
		return 0;
	}

	ObjectType ObjType = Obj.get_object_type();
	if(ObjType.is_map_container()){
		error("get_object_rnum: Object is MapContainer.\n");
		return 0;
	}

	int Result = 0;
	Object Con = Obj.get_container();
	Object Help = get_first_container_object(Con);
	while(Help != NONE && Help != Obj){
		Result += 1;
		Help = Help.get_next_object();
	}

	if(Help != Obj){
		error("get_object_rnum: Object is not in container\n");
		Result = 0;
	}

	return Result;
}

bool object_in_range(uint32 CreatureID, Object Obj, int Range){
	TCreature *Creature = GetCreature(CreatureID);
	if(Creature == NULL){
		error("object_in_range: Invalid creature CreatureID=%d passed.\n", CreatureID);
		return false;
	}

	if(!Obj.exists()){
		error("object_in_range: Passed object does not exist.\n");
		return false;
	}

	int ObjX, ObjY, ObjZ;
	get_object_coordinates(Obj, &ObjX, &ObjY, &ObjZ);
	return Creature->posz == ObjZ
		&& std::abs(Creature->posx - ObjX) <= Range
		&& std::abs(Creature->posy - ObjY) <= Range;
}

bool object_accessible(uint32 CreatureID, Object Obj, int Range){
	if(!Obj.exists()){
		error("object_accessible: Passed object does not exist.\n");
		return false;
	}

	ObjectType ObjType = Obj.get_object_type();
	if(!ObjType.is_creature_container()){
		uint32 OwnerID = get_object_creature_id(Obj);
		if(OwnerID != 0){
			return OwnerID == CreatureID;
		}
	}

	if(ObjType.get_flag(HANG)){
		int ObjX, ObjY, ObjZ;
		get_object_coordinates(Obj, &ObjX, &ObjY, &ObjZ);

		bool HookSouth = coordinate_flag(ObjX, ObjY, ObjZ, HOOKSOUTH);
		bool HookEast = coordinate_flag(ObjX, ObjY, ObjZ, HOOKEAST);
		if(HookSouth || HookEast){
			TCreature *Creature = GetCreature(CreatureID);
			if(Creature == NULL){
				error("object_accessible: Creature does not exist.\n");
				return false;
			}

			if(HookSouth){
				if(Creature->posy < ObjY
						|| Creature->posy > (ObjY + Range)
						|| Creature->posx < (ObjX - Range)
						|| Creature->posx > (ObjX + Range)){
					return false;
				}
			}

			if(HookEast){
				if(Creature->posx < ObjX
						|| Creature->posx > (ObjX + Range)
						|| Creature->posy < (ObjY - Range)
						|| Creature->posy > (ObjY + Range)){
					return false;
				}
			}
		}
	}

	return object_in_range(CreatureID, Obj, Range);
}

int object_distance(Object Obj1, Object Obj2){
	if(!Obj1.exists() || !Obj2.exists()){
		error("object_distance: Passed objects do not exist.\n");
		return INT_MAX;
	}

	int Distance = INT_MAX;
	int ObjX1, ObjY1, ObjZ1;
	int ObjX2, ObjY2, ObjZ2;
	get_object_coordinates(Obj1, &ObjX1, &ObjY1, &ObjZ1);
	get_object_coordinates(Obj2, &ObjX2, &ObjY2, &ObjZ2);
	if(ObjZ1 == ObjZ2){
		Distance = std::max<int>(
				std::abs(ObjX1 - ObjX2),
				std::abs(ObjY1 - ObjY2));
	}
	return Distance;
}

Object get_body_container(uint32 CreatureID, int Position){
	if((Position < INVENTORY_FIRST || Position > INVENTORY_LAST)
	&& (Position < CONTAINER_FIRST || Position > CONTAINER_LAST)){
		error("get_body_container: invalid position: %d\n", Position);
		return NONE;
	}

	TCreature *Creature = GetCreature(CreatureID);
	if(Creature == NULL){
		error("get_body_container: Creature %d does not exist.\n", CreatureID);
		return NONE;
	}

	if(Position >= INVENTORY_FIRST && Position <= INVENTORY_LAST){
		if(!Creature->CrObject.exists()){
			error("get_body_container: Creature object of %s does not exist (Pos %d).\n",
					Creature->Name, Position);
			return NONE;
		}

		return get_container_object(Creature->CrObject, Position - INVENTORY_FIRST);
	}else{
		if(Creature->Type != PLAYER){
			error("get_body_container: Only players have open containers.\n");
			return NONE;
		}

		return ((TPlayer*)Creature)->GetOpenContainer(Position - CONTAINER_FIRST);
	}
}

Object get_body_object(uint32 CreatureID, int Position){
	if(Position < INVENTORY_FIRST || Position > INVENTORY_LAST){
		error("get_body_object: invalid position %d\n", Position);
		return NONE;
	}

	Object Obj = NONE;
	Object Con = get_body_container(CreatureID, Position);
	if(Con != NONE){
		Obj = get_first_container_object(Con);
	}
	return Obj;
}

Object get_top_object(int x, int y, int z, bool move){
	Object Obj = get_first_object(x, y, z);
	if(Obj != NONE){
		while(true){
			Object Next = Obj.get_next_object();
			if(Next == NONE){
				break;
			}

			ObjectType ObjType = Obj.get_object_type();
			if(!ObjType.get_flag(BANK)
					&& !ObjType.get_flag(CLIP)
					&& !ObjType.get_flag(BOTTOM)
					&& !ObjType.get_flag(TOP)
					&& (!move || !ObjType.is_creature_container())){
				break;
			}

			Obj = Next;
		}
	}
	return Obj;
}

Object get_container(uint32 CreatureID, int x, int y, int z){
	if(x == 0xFFFF){ // SPECIAL_COORDINATE ?
		return get_body_container(CreatureID, y);
	}else{
		return get_map_container(x, y, z);
	}
}

Object get_object(uint32 CreatureID, int x, int y, int z, int RNum, ObjectType Type){
	Object Obj = NONE;
	if(x == 0xFFFF){ // SPECIAL_COORDINATE ?
		if(y >= INVENTORY_FIRST && y <= INVENTORY_LAST){
			Obj = get_body_object(CreatureID, y);
		}else if(y >= CONTAINER_FIRST && y <= CONTAINER_LAST){
			Object Con = get_body_container(CreatureID, y);
			if(Con != NONE){
				Obj = get_container_object(Con, RNum);
			}
		}else if(y != INVENTORY_ANY){
			error("get_object: Invalid ContainerCode x=%d,y=%d,z=%d,RNum=%d,Type=%d.\n",
					x, y, z, RNum, Type.TypeID);
		}
	}else if(RNum != -1){
		Obj = get_first_object(x, y, z);
		while(Obj != NONE){
			if(Obj.get_object_type().get_disguise() == Type){
				break;
			}
			Obj = Obj.get_next_object();
		}
	}else{
		Obj = get_top_object(x, y, z, false);
	}

	// NOTE(fusion): `Type` can be a map container (TypeID = 0) as a wildcard for
	// any object found.
	if(Obj != NONE && !Type.is_map_container()
			&& Obj.get_object_type().get_disguise() != Type){
		Obj = NONE;
	}

	return Obj;
}

Object get_row_object(Object Obj, ObjectType Type, uint32 Value, bool Recurse){
	Object Result = NONE;
	while(Obj != NONE){
		ObjectType ObjType = Obj.get_object_type();
		if(Recurse && ObjType.get_flag(CONTAINER)){
			Object Help = get_first_container_object(Obj);
			Result = get_row_object(Help, Type, Value, Recurse);
			if(Result != NONE){
				break;
			}
		}

		if(ObjType == Type
				&& (!ObjType.get_flag(LIQUIDCONTAINER) || Obj.get_attribute(CONTAINERLIQUIDTYPE) == Value)
				&& (!ObjType.get_flag(KEY)             || Obj.get_attribute(KEYNUMBER)           == Value)){
			Result = Obj;
			break;
		}

		Obj = Obj.get_next_object();
	}
	return Result;
}

Object get_inventory_object(uint32 CreatureID, ObjectType Type, uint32 Value){
	TCreature *Creature = GetCreature(CreatureID);
	if(Creature == NULL){
		error("get_inventory_object: Creature %d does not exist.\n",CreatureID);
		return NONE;
	}

	Object Result = NONE;

	// NOTE(fusion): Search inventory containers.
	{
		Object BodyCon = get_first_container_object(Creature->CrObject);
		while(BodyCon != NONE){
			Object BodyObj = get_first_container_object(BodyCon);
			if(BodyObj != NONE && BodyObj.get_object_type().get_flag(CONTAINER)){
				Object Help = get_first_container_object(BodyObj);
				Result = get_row_object(Help, Type, Value, true);
				if(Result != NONE){
					break;
				}
			}
			BodyCon = BodyCon.get_next_object();
		}
	}

	// NOTE(fusion): Search inventory slots.
	if(Result == NONE){
		Object BodyCon = get_first_container_object(Creature->CrObject);
		while(BodyCon != NONE){
			Object BodyObj = get_first_container_object(BodyCon);
			Result = get_row_object(BodyObj, Type, Value, false);
			if(Result != NONE){
				break;
			}
			BodyCon = BodyCon.get_next_object();
		}
	}

	return Result;
}

bool is_held_by_container(Object Obj, Object Con){
	// TODO(fusion): Why do we check for map container in some loops?
	while(Obj != NONE && !Obj.get_object_type().is_map_container()){
		if(Obj == Con){
			return true;
		}
		Obj = Obj.get_container();
	}
	return false;
}

int count_objects_in_container(Object Con){
	if(!Con.exists()){
		error("count_objects_in_container: Container does not exist.\n");
		return 0;
	}

	int Count = 0;
	Object Obj = get_first_container_object(Con);
	while(Obj != NONE){
		Count += 1;
		Obj = Obj.get_next_object();
	}
	return Count;
}

int count_objects(Object Obj){
	if(!Obj.exists()){
		return 0;
	}

	int Count = 1;
	if(Obj.get_object_type().get_flag(CONTAINER)){
		Object Help = get_first_container_object(Obj);
		while(Help != NONE){
			Count += count_objects(Help);
			Help = Help.get_next_object();
		}
	}
	return Count;
}

int count_objects(Object Obj, ObjectType Type, uint32 Value){
	if(!Obj.exists()){
		return 0;
	}

	// TODO(fusion): This is different from the other `count_objects` as it does
	// check other objects in the chain.
	int Count = 0;
	while(Obj != NONE){
		ObjectType ObjType = Obj.get_object_type();
		if(ObjType.get_flag(CONTAINER)){
			Object Help = get_first_container_object(Obj);
			// BUG(fusion): We probably meant to pass `Value` down the the call stack?
			Count += count_objects(Help, Type, 0);
		}

		if(ObjType == Type
				&& (!ObjType.get_flag(LIQUIDCONTAINER) || Obj.get_attribute(CONTAINERLIQUIDTYPE) == Value)
				&& (!ObjType.get_flag(KEY)             || Obj.get_attribute(KEYNUMBER)           == Value)){
			if(ObjType.get_flag(CUMULATIVE)){
				Count += (int)Obj.get_attribute(AMOUNT);
			}else{
				Count += 1;
			}
		}

		Obj = Obj.get_next_object();
	}
	return Count;
}

int count_inventory_objects(uint32 CreatureID, ObjectType Type, uint32 Value){
	TCreature *Creature = GetCreature(CreatureID);
	if(Creature == NULL){
		error("count_inventory_objects: Creature %d does not exist; object type %d.\n",
				CreatureID, Type.TypeID);
		return 0;
	}

	if(Creature->CrObject == NONE){
		error("count_inventory_objects: Creature %s has no creature object.\n",
				Creature->Name);
		return 0;
	}

	Object Help = get_first_container_object(Creature->CrObject);
	return count_objects(Help, Type, Value);
}

int count_money(Object Obj){
	if(!Obj.exists()){
		return 0;
	}

	int Result = 0;
	while(Obj != NONE){
		ObjectType ObjType = Obj.get_object_type();
		if(ObjType.get_flag(CONTAINER)){
			Object Help = get_first_container_object(Obj);
			Result += count_money(Help);
		}

		// TODO(fusion): This was three different if statements but I don't think
		// we'd want the same object type to be all special money objects.
		if(ObjType == get_special_object(MONEY_ONE)){
			Result += (int)Obj.get_attribute(AMOUNT);
		}else if(ObjType == get_special_object(MONEY_HUNDRED)){
			Result += (int)Obj.get_attribute(AMOUNT) * 100;
		}else if(ObjType == get_special_object(MONEY_TENTHOUSAND)){
			Result += (int)Obj.get_attribute(AMOUNT) * 10000;
		}

		Obj = Obj.get_next_object();
	}
	return Result;
}

int count_inventory_money(uint32 CreatureID){
	TCreature *Creature = GetCreature(CreatureID);
	if(Creature == NULL){
		error("count_inventory_money: Creature %d does not exist.\n", CreatureID);
		return 0;
	}

	if(Creature->CrObject == NONE){
		error("count_inventory_money: Creature %s has no creature object.\n", Creature->Name);
		return 0;
	}

	Object Help = get_first_container_object(Creature->CrObject);
	return count_money(Help);
}

void calculate_change(int Amount, int *Gold, int *Platinum, int *Crystal){
	// TODO(fusion): This function is kind of a mess. We should simplify or
	// get rid of it entirely. It is used when removing gold from a creature
	// but doesnt doesn't report errors outside from the `error` logging. It
	// expects the caller to also check whether the amount of gold, platinum,
	// and crystal is sufficient? We'll see when it gets applied.

	int Go = *Gold;
	int Pl = *Platinum;
	int Cr = *Crystal;

	print(3, "Paying %d with %d/%d/%d coins...\n", Amount, Go, Pl, Cr);
	if((Cr * 10000 + Pl * 100 + Go) < Amount){
		error("calculate_change: %d/%d/%d coins are not enough to pay %d.\n",
				Go, Pl, Cr, Amount);
		return;
	}

	int AmountCr = Amount / 10000;
	int AmountRem = Amount % 10000;
	if((Pl * 100 + Go) < AmountRem){
		Cr = AmountCr + 1;
		Pl = (AmountRem - 10000) / 100;
		Go = (AmountRem - 10000) % 100;
	}else{
		if(Cr < AmountCr){
			AmountRem = Amount - Cr * 10000;
		}else{
			Cr = AmountCr;
		}

		int AmountPl = AmountRem / 100;
		int AmountGo = AmountRem % 100;
		if(Go < AmountGo){
			Pl = AmountPl + 1;
			Go = AmountGo - 100;
		}else if(Pl < AmountPl){
			Go = AmountRem - Pl * 100;
		}else{
			Pl = AmountPl;
			Go = AmountGo;
		}
	}

	print(3, "Using %d/%d/%d coins.\n", Go, Pl, Cr);
	*Gold = Go;
	*Platinum = Pl;
	*Crystal = Cr;

	if((Cr * 10000 + Pl * 100 + Go) != Amount){
		error("calculate_change: Incorrect calculation: %d/%d/%d coins for %d.\n",
				Go, Pl, Cr, Amount);
	}
}

int get_height(int x, int y, int z){
	int Result = 0;
	Object Obj = get_first_object(x, y, z);
	while(Obj != NONE){
		ObjectType ObjType = Obj.get_object_type();
		if(ObjType.get_flag(HEIGHT)){
			Result += (int)ObjType.get_attribute(ELEVATION);
		}
		Obj = Obj.get_next_object();
	}
	return Result;
}

bool jump_possible(int x, int y, int z, bool AvoidPlayers){
	bool HasBank = false;
	Object Obj = get_first_object(x, y, z);
	while(Obj != NONE){
		ObjectType ObjType = Obj.get_object_type();

		if(ObjType.get_flag(BANK)){
			HasBank = true;
		}

		if(ObjType.get_flag(UNPASS) && ObjType.get_flag(UNMOVE)){
			return false;
		}

		if(AvoidPlayers && ObjType.is_creature_container()){
			TCreature *Creature = GetCreature(Obj);
			if(Creature != NULL && Creature->Type == PLAYER){
				return false;
			}
		}

		Obj = Obj.get_next_object();
	}
	return HasBank;
}

bool field_possible(int x, int y, int z, int FieldType){
	Object Obj = get_first_object(x, y, z);

	// NOTE(fusion): Only the first object on a given coordinate can be a bank
	// on regular circumstances, so it makes sense to check it right away to
	// determine whether a bank is present.
	if(Obj == NONE || !Obj.get_object_type().get_flag(BANK)){
		return false;
	}

	while(Obj != NONE){
		ObjectType ObjType = Obj.get_object_type();
		if(!ObjType.is_creature_container()){
			if(ObjType.get_flag(UNPASS)){
				return false;
			}

			if(ObjType.get_flag(UNLAY)){
				return false;
			}
		}

		if(FieldType == FIELD_TYPE_MAGICWALL || FieldType == FIELD_TYPE_WILDGROWTH){
			if(ObjType.get_flag(UNPASS)){
				return false;
			}
		}

		Obj = Obj.get_next_object();
	}
	return true;
}

bool search_free_field(int *x, int *y, int *z, int Distance, uint16 HouseID, bool Jump){
	int OffsetX = 0;
	int OffsetY = 0;
	int CurrentDistance = 0;
	int CurrentDirection = DIRECTION_EAST;
	while(CurrentDistance <= Distance){
		int FieldX = *x + OffsetX;
		int FieldY = *y + OffsetY;
		int FieldZ = *z;

		// TODO(fusion): This is probably some form of the `TCreature::MovePossible`
		// function inlined.
		bool MovePossible;
		if(Jump){
			MovePossible = jump_possible(FieldX, FieldY, FieldZ, true);
		}else{
			MovePossible = coordinate_flag(FieldX, FieldY, FieldZ, BANK)
					&& !coordinate_flag(FieldX, FieldY, FieldZ, UNPASS);

			// TODO(fusion): This one I'm not so sure.
			if(MovePossible && coordinate_flag(FieldX, FieldY, FieldZ, AVOID)){
				MovePossible = coordinate_flag(FieldX, FieldY, FieldZ, BED);
			}
		}

		if(MovePossible){
			if(HouseID == 0xFFFF || !is_house(FieldX, FieldY, FieldZ)
			|| (HouseID != 0 && get_house_id(FieldX, FieldY, FieldZ) == HouseID)){
				*x = FieldX;
				*y = FieldY;
				return true;
			}
		}


		// NOTE(fusion): We're spiraling out from the initial coordinate.
		// TODO(fusion): This function used directions different from the ones
		// used by creatures and defined in `enums.hh` so I made it use them
		// instead, LOL.
		if(CurrentDirection == DIRECTION_NORTH){
			OffsetY -= 1;
			if(OffsetY <= -CurrentDistance){
				CurrentDirection = DIRECTION_WEST;
			}
		}else if(CurrentDirection == DIRECTION_WEST){
			OffsetX -= 1;
			if(OffsetX <= -CurrentDistance){
				CurrentDirection = DIRECTION_SOUTH;
			}
		}else if(CurrentDirection == DIRECTION_SOUTH){
			OffsetY += 1;
			if(OffsetY >= CurrentDistance){
				CurrentDirection = DIRECTION_EAST;
			}
		}else{
			ASSERT(CurrentDirection == DIRECTION_EAST);
			OffsetX += 1;
			if(OffsetX > CurrentDistance){
				CurrentDistance = OffsetX;
				CurrentDirection = DIRECTION_NORTH;
			}
		}
	}

	return false;
}

// NOTE(fusion): This is a helper function for `search_login_field` and improves
// the readability of an otherwise convoluted function.
static bool LoginPossible(int x, int y, int z, uint16 HouseID, bool Player){
	Object Obj = get_first_object(x, y, z);
	if(Obj == NONE){
		return false;
	}

	if(Player && is_no_logout_field(x, y, z)){
		return false;
	}

	if(is_house(x, y, z) && (HouseID == 0 || get_house_id(x, y, z) != HouseID)){
		return false;
	}

	bool HasBank = false;
	while(Obj != NONE){
		ObjectType ObjType = Obj.get_object_type();
		if(ObjType.get_flag(BANK)){
			HasBank = true;
		}

		if(ObjType.is_creature_container()){
			return false;
		}

		if(ObjType.get_flag(UNPASS) && ObjType.get_flag(UNMOVE)){
			return false;
		}

		if(!Player && ObjType.get_flag(AVOID) && ObjType.get_flag(UNMOVE)){
			return false;
		}

		Obj = Obj.get_next_object();
	}
	return HasBank;
}

bool search_login_field(int *x, int *y, int *z, int Distance, bool Player){
	uint16 HouseID = get_house_id(*x, *y, *z);
	if(search_free_field(x, y, z, Distance, HouseID, false)
			&& (!Player || !is_no_logout_field(*x, *y, *z))){
		return true;
	}

	int OffsetX = 0;
	int OffsetY = 0;
	int CurrentDistance = 0;
	int CurrentDirection = DIRECTION_EAST;
	while(CurrentDistance <= Distance){
		int FieldX = *x + OffsetX;
		int FieldY = *y + OffsetY;
		int FieldZ = *z;

		if(LoginPossible(FieldX, FieldY, FieldZ, HouseID, Player)){
			*x = FieldX;
			*y = FieldY;
			return true;
		}

		// NOTE(fusion): Same as `search_free_field`.
		if(CurrentDirection == DIRECTION_NORTH){
			OffsetY -= 1;
			if(OffsetY <= -CurrentDistance){
				CurrentDirection = DIRECTION_WEST;
			}
		}else if(CurrentDirection == DIRECTION_WEST){
			OffsetX -= 1;
			if(OffsetX <= -CurrentDistance){
				CurrentDirection = DIRECTION_SOUTH;
			}
		}else if(CurrentDirection == DIRECTION_SOUTH){
			OffsetY += 1;
			if(OffsetY >= CurrentDistance){
				CurrentDirection = DIRECTION_EAST;
			}
		}else{
			ASSERT(CurrentDirection == DIRECTION_EAST);
			OffsetX += 1;
			if(OffsetX > CurrentDistance){
				CurrentDistance = OffsetX;
				CurrentDirection = DIRECTION_NORTH;
			}
		}
	}
	return false;
}

bool search_spawn_field(int *x, int *y, int *z, int Distance, bool Player){
	// TODO(fusion): It seems `Distance` can be a negative number to do some
	// extended search?
	bool Minimize = true;
	if(Distance < 0){
		Minimize = false;
		Distance = -Distance;
	}

	uint16 HouseID = get_house_id(*x, *y, *z);
	matrix<int> Map(-Distance, Distance, -Distance, Distance, INT_MAX);
	*Map.at(0, 0) = 0;

	int BestX = 0;
	int BestY = 0;
	int BestTieBreaker = -1;
	int ExpansionPhase = 0;
	while(true){
		bool Found = false;
		bool Expanded = false;
		for(int OffsetY = -Distance; OffsetY <= Distance; OffsetY += 1)
		for(int OffsetX = -Distance; OffsetX <= Distance; OffsetX += 1){
			if(*Map.at(OffsetX, OffsetY) != ExpansionPhase){
				continue;
			}

			int FieldX = *x + OffsetX;
			int FieldY = *y + OffsetY;
			int FieldZ = *z;
			if(is_house(FieldX, FieldY, FieldZ) && (HouseID == 0 || get_house_id(FieldX, FieldY, FieldZ) != HouseID)){
				continue;
			}

			if(!Player && is_protection_zone(FieldX, FieldY, FieldZ)){
				continue;
			}

			Object Obj = get_first_object(FieldX, FieldY, FieldZ);
			if(Obj == NONE){
				continue;
			}

			bool ExpansionPossible = true;
			bool LoginPossible = true;
			bool LoginBad = false;
			while(Obj != NONE){
				ObjectType ObjType = Obj.get_object_type();
				if(ObjType.is_creature_container()){
					LoginPossible = false;
				}

				if(ObjType.get_flag(UNPASS)){
					if(ObjType.get_flag(UNMOVE)){
						ExpansionPossible = false;
						LoginPossible = false;
					}else{
						LoginBad = true;
					}
				}

				if(ObjType.get_flag(AVOID)){
					if(ObjType.get_flag(UNMOVE)){
						ExpansionPossible = false;
						LoginPossible = LoginPossible && !Player;
					}else{
						LoginBad = true;
					}
				}

				Obj = Obj.get_next_object();
			}

			if(ExpansionPossible || ExpansionPhase == 0){
				for(int NeighborOffsetY = OffsetY - 1; NeighborOffsetY <= OffsetY + 1; NeighborOffsetY += 1)
				for(int NeighborOffsetX = OffsetX - 1; NeighborOffsetX <= OffsetX + 1; NeighborOffsetX += 1){
					if(-Distance <= NeighborOffsetX && NeighborOffsetX <= Distance
					&& -Distance <= NeighborOffsetY && NeighborOffsetY <= Distance){
						int *NeighborPhase = Map.at(NeighborOffsetX, NeighborOffsetY);
						if(*NeighborPhase > ExpansionPhase){
							*NeighborPhase = ExpansionPhase
									+ std::abs(NeighborOffsetX - OffsetX)
									+ std::abs(NeighborOffsetY - OffsetY);
						}
					}
				}
				Expanded = true;
			}

			if(LoginPossible && (!Player || !is_no_logout_field(FieldX, FieldY, FieldZ))){
				int TieBreaker = random(0, 99);
				if(!LoginBad){
					TieBreaker += 100;
				}

				if(TieBreaker > BestTieBreaker){
					Found = true;
					BestX = FieldX;
					BestY = FieldY;
					BestTieBreaker = TieBreaker;
				}
			}
		}

		if((Found && Minimize) || !Expanded){
			break;
		}

		ExpansionPhase += 1;
	}

	bool Result = false;
	if(BestTieBreaker >= 0){
		*x = BestX;
		*y = BestY;
		Result = true;
	}

	return Result;
}

bool search_flight_field(uint32 FugitiveID, uint32 PursuerID, int *x, int *y, int *z){
	TCreature *Fugitive = GetCreature(FugitiveID);
	if(Fugitive == NULL){
		error("search_flight_field: Fugitive does not exist.\n");
		return false;
	}

	TCreature *Pursuer = GetCreature(PursuerID);
	if(Pursuer == NULL){
		error("search_flight_field: Pursuer does not exist.\n");
		return false;
	}

	if(Fugitive->posz != Pursuer->posz){
		error("search_flight_field: Fugitive and pursuer are on different levels.\n");
		return false;
	}

	int Dir[9];
	for(int i = 0; i < NARRAY(Dir); i += 1){
		Dir[i] = DIRECTION_NONE;
	}

	// IMPORTANT(fusion): This is more closely related to the original binary. If
	// you consider the offset to be the relative coordinate of the fugitive with
	// respect to the pursuer, and the conditions to be half-plane inequalities,
	// it shouldn't be too difficult to reason about it.

	int OffsetX = Fugitive->posx - Pursuer->posx;
	int OffsetY = Fugitive->posy - Pursuer->posy;
	int DistanceX = std::abs(OffsetX);
	int DistanceY = std::abs(OffsetY);

	// NOTE(fusion): Prefer axial direction away from the pursuer.
	if (DistanceX > DistanceY){
		Dir[0] = (OffsetX < 0) ? DIRECTION_WEST  : DIRECTION_EAST;
	}else if(DistanceX < DistanceY){
		Dir[0] = (OffsetY < 0) ? DIRECTION_NORTH : DIRECTION_SOUTH;
	}

	// NOTE(fusion): Fallback to random axial direction away from the pursuer.
	if(OffsetX >= 0) Dir[1] = DIRECTION_EAST;
	if(OffsetY <= 0) Dir[2] = DIRECTION_NORTH;
	if(OffsetX <= 0) Dir[3] = DIRECTION_WEST;
	if(OffsetY >= 0) Dir[4] = DIRECTION_SOUTH;
	RandomShuffle(&Dir[1], 4);

	// NOTE(fusion): Fallback to diagonal direction away from the pursuer.
	if(OffsetY <=  OffsetX) Dir[5] = DIRECTION_NORTHEAST;
	if(OffsetY <= -OffsetX) Dir[6] = DIRECTION_NORTHWEST;
	if(OffsetY >=  OffsetX) Dir[7] = DIRECTION_SOUTHWEST;
	if(OffsetY >= -OffsetX) Dir[8] = DIRECTION_SOUTHEAST;
	RandomShuffle(&Dir[5], 4);

	for(int i = 0; i < NARRAY(Dir); i += 1){
		if(Dir[i] == DIRECTION_NONE){
			continue;
		}

		int FieldX = Fugitive->posx;
		int FieldY = Fugitive->posy;
		int FieldZ = Fugitive->posz;
		switch(Dir[i]){
			case DIRECTION_NORTH:                  FieldY -= 1; break;
			case DIRECTION_EAST:      FieldX += 1;              break;
			case DIRECTION_SOUTH:                  FieldY += 1; break;
			case DIRECTION_WEST:      FieldX -= 1;              break;
			case DIRECTION_SOUTHWEST: FieldX -= 1; FieldY += 1; break;
			case DIRECTION_SOUTHEAST: FieldX += 1; FieldY += 1; break;
			case DIRECTION_NORTHWEST: FieldX -= 1; FieldY -= 1; break;
			case DIRECTION_NORTHEAST: FieldX += 1; FieldY -= 1; break;
			default:{
				error("search_flight_field: Invalid direction %d.\n", Dir[i]);
				return false;
			}
		}

		if(Fugitive->MovePossible(FieldX, FieldY, FieldZ, false, false)){
			*x = FieldX;
			*y = FieldY;
			*z = FieldZ;
			return true;
		}
	}

	return false;
}

bool search_summon_field(int *x, int *y, int *z, int Distance){
	int BestX = 0;
	int BestY = 0;
	int BestTieBreaker = -1;
	for(int OffsetY = -Distance; OffsetY <= Distance; OffsetY += 1)
	for(int OffsetX = -Distance; OffsetX <= Distance; OffsetX += 1){
		int TieBreaker = random(0, 99);
		if(TieBreaker <= BestTieBreaker){
			continue;
		}

		int FieldX = *x + OffsetX;
		int FieldY = *y + OffsetY;
		int FieldZ = *z;
		if(coordinate_flag(FieldX, FieldY, FieldZ, BANK)
				&& !coordinate_flag(FieldX, FieldY, FieldZ, UNPASS)
				&& !coordinate_flag(FieldX, FieldY, FieldZ, AVOID)
				&& !is_protection_zone(FieldX, FieldY, FieldZ)
				&& !is_house(FieldX, FieldY, FieldZ)
				&& throw_possible(*x, *y, *z, FieldX, FieldY, FieldZ, 0)){
			BestX = FieldX;
			BestY = FieldY;
			BestTieBreaker = TieBreaker;
		}
	}

	bool Result = false;
	if(BestTieBreaker >= 0){
		*x = BestX;
		*y = BestY;
		Result = true;
	}

	return Result;
}

bool throw_possible(int OrigX, int OrigY, int OrigZ,
			int DestX, int DestY, int DestZ, int Power){
	// NOTE(fusion): `MinZ` contains the highest floor we're able to throw. We'll
	// iterate from it towards the destination floor, checking the line between the
	// origin and destination until we find a valid throwing path (or not).
	int MinZ = std::max<int>(OrigZ - Power, 0);
	for(int CurZ = OrigZ - 1; CurZ >= MinZ; CurZ -= 1){
		Object Obj = get_first_object(OrigX, OrigY, CurZ);
		if(Obj != NONE && Obj.get_object_type().get_flag(BANK)){
			MinZ = CurZ + 1;
			break;
		}
	}

	// NOTE(fusion): I'm using `T` as the parameter for the line between the
	// origin and destination.
	int MaxT = std::max<int>(
			std::abs(DestX - OrigX),
			std::abs(DestY - OrigY));

	int StartT = 1;
	if((DestX < OrigX && coordinate_flag(OrigX, OrigY, OrigZ, HOOKEAST))
	|| (DestY < OrigY && coordinate_flag(OrigX, OrigY, OrigZ, HOOKSOUTH))){
		StartT = 0;
	}

	while(MinZ <= DestZ){
		int LastX = OrigX;
		int LastY = OrigY;
		if(DestX != OrigX || DestY != OrigY){
			// NOTE(fusion): Get the current coordinates with linear interpolation.
			for(int T = StartT; T <= MaxT; T += 1){
				int CurX = (OrigX * (MaxT - T) + DestX * T) / MaxT;
				int CurY = (OrigY * (MaxT - T) + DestY * T) / MaxT;
				int CurZ = MinZ;
				if(coordinate_flag(CurX, CurY, CurZ, UNTHROW)){
					break;
				}

				LastX = CurX;
				LastY = CurY;
			}
		}

		if(LastX == DestX && LastY == DestY){
			int LastZ = MinZ;
			for(; LastZ < DestZ; LastZ += 1){
				Object Obj = get_first_object(DestX, DestY, LastZ);
				if(Obj != NONE && Obj.get_object_type().get_flag(BANK)){
					break;
				}
			}

			if(LastZ == DestZ){
				return true;
			}
		}

		MinZ += 1;
	}

	return false;
}

void get_creature_light(uint32 CreatureID, int *Brightness, int *Color){
	TCreature *Creature = GetCreature(CreatureID);
	if(Creature == NULL){
		error("get_creature_light: Creature does not exist.\n");
		*Brightness = 0;
		*Color = 0;
		return;
	}

	int OutBrightness = 0;
	if(Creature->Skills[SKILL_LIGHT] != NULL){
		OutBrightness = Creature->Skills[SKILL_LIGHT]->TimerValue();
	}

	if(Creature->Type == PLAYER && check_right(CreatureID, ILLUMINATE)){
		if(OutBrightness < 7){
			OutBrightness = 7;
		}
	}

	int OutRed   = 5 * OutBrightness;
	int OutGreen = 5 * OutBrightness;
	int OutBlue  = 5 * OutBrightness;
	for(int Position = INVENTORY_FIRST;
			Position <= INVENTORY_LAST;
			Position += 1){
		Object Obj = get_body_object(CreatureID, Position);
		if(Obj == NONE){
			continue;
		}

		ObjectType ObjType = Obj.get_object_type();
		if(!ObjType.get_flag(LIGHT)){
			continue;
		}

		int ObjBrightness = (int)ObjType.get_attribute(BRIGHTNESS);
		int ObjColor      = (int)ObjType.get_attribute(LIGHTCOLOR);
		int ObjRed        = (ObjColor / 36)     * ObjBrightness;
		int ObjGreen      = (ObjColor % 36 / 6) * ObjBrightness;
		int ObjBlue       = (ObjColor % 36 % 6) * ObjBrightness;

		if(OutBrightness < ObjBrightness){
			OutBrightness = ObjBrightness;
		}

		if(OutRed < ObjRed){
			OutRed = ObjRed;
		}

		if(OutGreen < ObjGreen){
			OutGreen = ObjGreen;
		}

		if(OutBlue < ObjBlue){
			OutBlue = ObjBlue;
		}
	}

	int OutColor = 0;
	if(OutBrightness > 0){
		OutColor = (OutRed / OutBrightness) * 36
				+ (OutGreen / OutBrightness) * 6
				+ (OutBlue / OutBrightness);
	}

	*Brightness = OutBrightness;
	*Color      = OutColor;
}

int get_inventory_weight(uint32 CreatureID){
	TCreature *Creature = GetCreature(CreatureID);
	if(Creature == NULL){
		error("get_inventory_weight: Creature %d does not exist.\n", CreatureID);
		return 0;
	}

	Object Help = get_first_container_object(Creature->CrObject);
	return get_row_weight(Help);
}

bool check_right(uint32 CharacterID, RIGHT Right){
	TPlayer *Player = GetPlayer(CharacterID);
	if(Player == NULL){
		error("check_right: Player does not exist; Right=%d.\n", Right);
		return false;
	}

	if(!CheckBitIndex(NARRAY(Player->Rights), Right)){
		error("check_right: Invalid right number %d.\n", Right);
		return false;
	}

	return CheckBit(Player->Rights, Right);
}

bool check_banishment_right(uint32 CharacterID, int Reason, int Action){
	TPlayer *Player = GetPlayer(CharacterID);
	if(Player == NULL){
		error("check_banishment_right: Player does not exist.\n");
		return false;
	}

	// NOTE(fusion): Banishment rights range from 18 to 49 and match banishment
	// reasons exactly when subtracting 18.

	if(Reason < 0 || Reason > 31){
		error("check_banishment_right: Invalid ban reason %d from player %d.\n",
				Reason, CharacterID);
		return false;
	}

	if(Action < 0 || Action > 6){
		error("check_banishment_right: Invalid action %d from player %d.\n",
				Action, CharacterID);
		return false;
	}

	bool Result = false;
	if(check_right(CharacterID, (RIGHT)(Reason + 18))){
		bool Name = (Reason >= 0 && Reason <= 8);
		bool Statement = (Reason >= 9 && Reason <= 27) || Reason == 29;
		switch(Action){
			case 0:{
				Result = check_right(CharacterID, NOTATION);
				break;
			}

			case 1:{
				Result = Name && check_right(CharacterID, NAMELOCK);
				break;
			}

			case 2:{
				Result = !Name && check_right(CharacterID, BANISHMENT);
				break;
			}

			case 3:{
				Result = Name && check_right(CharacterID, NAMELOCK)
						&& check_right(CharacterID, BANISHMENT);
				break;
			}

			case 4:{
				Result = !Name && check_right(CharacterID, BANISHMENT)
						&& check_right(CharacterID, FINAL_WARNING);
				break;
			}

			case 5:{
				Result = Name && check_right(CharacterID, NAMELOCK)
						&& check_right(CharacterID, BANISHMENT)
						&& check_right(CharacterID, FINAL_WARNING);
				break;
			}

			case 6:{
				Result = Statement && check_right(CharacterID, STATEMENT_REPORT);
				break;
			}
		}
	}

	return Result;
}

const char *get_banishment_reason(int Reason){
	const char *Result = "";
	switch(Reason){
		case 0:  Result = "NAME_INSULTING"; break;
		case 1:  Result = "NAME_SENTENCE"; break;
		case 2:  Result = "NAME_NONSENSICAL_LETTERS"; break;
		case 3:  Result = "NAME_BADLY_FORMATTED"; break;
		case 4:  Result = "NAME_NO_PERSON"; break;
		case 5:  Result = "NAME_CELEBRITY"; break;
		case 6:  Result = "NAME_COUNTRY"; break;
		case 7:  Result = "NAME_FAKE_IDENTITY"; break;
		case 8:  Result = "NAME_FAKE_POSITION"; break;
		case 9:  Result = "STATEMENT_INSULTING"; break;
		case 10: Result = "STATEMENT_SPAMMING"; break;
		case 11: Result = "STATEMENT_ADVERT_OFFTOPIC"; break;
		case 12: Result = "STATEMENT_ADVERT_MONEY"; break;
		case 13: Result = "STATEMENT_NON_ENGLISH"; break;
		case 14: Result = "STATEMENT_CHANNEL_OFFTOPIC"; break;
		case 15: Result = "STATEMENT_VIOLATION_INCITING"; break;
		case 16: Result = "CHEATING_BUG_ABUSE"; break;
		case 17: Result = "CHEATING_GAME_WEAKNESS"; break;
		case 18: Result = "CHEATING_MACRO_USE"; break;
		case 19: Result = "CHEATING_MODIFIED_CLIENT"; break;
		case 20: Result = "CHEATING_HACKING"; break;
		case 21: Result = "CHEATING_MULTI_CLIENT"; break;
		case 22: Result = "CHEATING_ACCOUNT_TRADING"; break;
		case 23: Result = "CHEATING_ACCOUNT_SHARING"; break;
		case 24: Result = "GAMEMASTER_THREATENING"; break;
		case 25: Result = "GAMEMASTER_PRETENDING"; break;
		case 26: Result = "GAMEMASTER_INFLUENCE"; break;
		case 27: Result = "GAMEMASTER_FALSE_REPORTS"; break;
		case 28: Result = "KILLING_EXCESSIVE_UNJUSTIFIED"; break;
		case 29: Result = "DESTRUCTIVE_BEHAVIOUR"; break;
		case 30: Result = "SPOILING_AUCTION"; break;
		case 31: Result = "INVALID_PAYMENT"; break;
		default:{
			error("get_banishment_reason: Invalid banishment reason %d.\n", Reason);
			break;
		}
	}
	return Result;
}

void init_info(void){
	// no-op
}

void exit_info(void){
	// no-op
}
