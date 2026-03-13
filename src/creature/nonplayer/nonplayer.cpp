#include "cr.h"

static vector<TNonplayer*> NonplayerList(0, 10000, 1000, NULL);
static int FirstFreeNonplayer;

// TNonplayer
// =============================================================================
TNonplayer::TNonplayer(void) : TCreature() {
	this->State = SLEEPING;
}

TNonplayer::~TNonplayer(void){
	this->DelInList();
}

void TNonplayer::SetInList(void){
	TCreature::SetInCrList();
	*NonplayerList.at(FirstFreeNonplayer) = this;
	FirstFreeNonplayer += 1;
}

void TNonplayer::DelInList(void){
	bool Removed = false;
	for(int i = 0; i < FirstFreeNonplayer; i += 1){
		if(*NonplayerList.at(i) == this){
			FirstFreeNonplayer -= 1;
			*NonplayerList.at(i) = *NonplayerList.at(FirstFreeNonplayer);
			*NonplayerList.at(FirstFreeNonplayer) = NULL;
			Removed = true;
			break;
		}
	}

	if(!Removed){
		error("TNonplayer::DelInList: Creature not found.\n");
	}
}

// Initialization
// =============================================================================
void InitNonplayer(void){
	print(1, "Initializing Nonplayers ...\n");
	FirstFreeNonplayer = 0;
	InitNPCs();
	LoadMonsterhomes();
}

void ExitNonplayer(void){
	while(FirstFreeNonplayer > 0){
		// NOTE(fusion): Deleting a non player or any other creature will
		// automatically remove it from its related creature lists.
		TNonplayer *Nonplayer = *NonplayerList.at(0);
		delete Nonplayer;
	}

	// TODO(fusion): For any reason `BehaviourNodeTable` was originally a
	// pointer and it was deleted here. I doesn't really make a difference
	// because we'd usually exit after calling this function but if we wanted
	// to cleanup all memory, we'd need to add some `freeAll` method to the
	// `store` container.
}
