#ifndef TIBIA_CREATURE_NONPLAYER_H_
#define TIBIA_CREATURE_NONPLAYER_H_ 1

#include "creature/creature/creature.h"

struct TNonplayer: TCreature {
	TNonplayer(void);
	~TNonplayer(void) override;
	void SetInList(void);
	void DelInList(void);

	// DATA
	// =================
	//TCreature super_TCreature;	// IMPLICIT
	STATE State;
};

void InitNonplayer(void);
void ExitNonplayer(void);

#endif // TIBIA_CREATURE_NONPLAYER_H_
