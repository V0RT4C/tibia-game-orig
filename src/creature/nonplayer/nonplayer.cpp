#include "cr.h"

static vector<Nonplayer*> NonplayerList(0, 10000, 1000, NULL);
static int FirstFreeNonplayer;

// Nonplayer
// =============================================================================
Nonplayer::Nonplayer(void) : TCreature() {
	this->State = SLEEPING;
}

Nonplayer::~Nonplayer(void){
	this->DelInList();
}

void Nonplayer::SetInList(void){
	TCreature::SetInCrList();
	*NonplayerList.at(FirstFreeNonplayer) = this;
	FirstFreeNonplayer += 1;
}

void Nonplayer::DelInList(void){
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
		error("Nonplayer::DelInList: Creature not found.\n");
	}
}

// Initialization
// =============================================================================
void init_nonplayer(void){
	print(1, "Initializing Nonplayers ...\n");
	FirstFreeNonplayer = 0;
	init_npcs();
	load_monsterhomes();
}

void exit_nonplayer(void){
	while(FirstFreeNonplayer > 0){
		// NOTE(fusion): Deleting a non player or any other creature will
		// automatically remove it from its related creature lists.
		Nonplayer *Nonplayer = *NonplayerList.at(0);
		delete Nonplayer;
	}

	// TODO(fusion): For any reason `BehaviourNodeTable` was originally a
	// pointer and it was deleted here. I doesn't really make a difference
	// because we'd usually exit after calling this function but if we wanted
	// to cleanup all memory, we'd need to add some `freeAll` method to the
	// `store` container.
}
