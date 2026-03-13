#ifndef TIBIA_CREATURE_NONPLAYER_H_
#define TIBIA_CREATURE_NONPLAYER_H_ 1

#include "creature/creature/creature.h"

struct Nonplayer : TCreature {
	Nonplayer(void);
	~Nonplayer(void) override;
	void SetInList(void);
	void DelInList(void);

	// DATA
	// =================
	// TCreature super_TCreature;	// IMPLICIT
	STATE State;
};

void init_nonplayer(void);
void exit_nonplayer(void);

#endif // TIBIA_CREATURE_NONPLAYER_H_
