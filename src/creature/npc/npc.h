#ifndef TIBIA_CREATURE_NPC_H_
#define TIBIA_CREATURE_NPC_H_ 1

#include "creature/nonplayer/nonplayer.h"

struct TNPC; // forward declaration for BehaviourDatabase

// TNPC
// =============================================================================
struct BehaviourNode {
	int Type;
	int Data;
	BehaviourNode *Left;
	BehaviourNode *Right;
};

struct BehaviourCondition {
	bool set(int Type, void *Data);
	void clear(void);

	// DATA
	// =================
	int Type;
	uint32 Text;
	BehaviourNode *Expression;
	int Property;
	int Number;
};

struct BehaviourAction {
	bool set(int Type, void *Data, void *Data2, void *Data3, void *Data4);
	void clear(void);

	// DATA
	// =================
	int Type;
	uint32 Text;
	int Number;
	BehaviourNode *Expression;
	BehaviourNode *Expression2;
	BehaviourNode *Expression3;
};

struct TBehaviour {
	TBehaviour(void);
	~TBehaviour(void);
	bool addCondition(int Type, void *Data);
	bool addAction(int Type, void *Data, void *Data2, void *Data3, void *Data4);

	// TODO(fusion): Same as `Channel` in `operate.hh`.
	TBehaviour(const TBehaviour &Other);
	void operator=(const TBehaviour &Other);

	// DATA
	// =================
	vector<BehaviourCondition> Condition;
	vector<BehaviourAction> Action;
	int Conditions;
	int Actions;
};

struct BehaviourDatabase {
	BehaviourDatabase(ReadScriptFile *Script);

	// TODO(fusion): These could/should be standalone functions.
	BehaviourNode *readValue(ReadScriptFile *Script);
	BehaviourNode *readFactor(ReadScriptFile *Script);
	BehaviourNode *readTerm(ReadScriptFile *Script);
	int evaluate(TNPC *Npc, BehaviourNode *Node, int *Parameters);

	void react(TNPC *Npc, const char *Text, SITUATION Situation);

	// DATA
	// =================
	vector<TBehaviour> Behaviour;
	int Behaviours;
};

struct TNPC: Nonplayer {
	TNPC(const char *FileName);
	void GiveTo(ObjectType Type, int Amount);
	void GetFrom(ObjectType Type, int Amount);
	void GiveMoney(int Amount);
	void GetMoney(int Amount);
	void Enqueue(uint32 InterlocutorID, const char *Text);
	void TurnToInterlocutor(void);
	void ChangeState(STATE NewState, bool Stimulus);

	// VIRTUAL FUNCTIONS
	// =================
	~TNPC(void) override;
	bool MovePossible(int x, int y, int z, bool Execute, bool Jump) override;
	void TalkStimulus(uint32 SpeakerID, const char *Text) override;
	void DamageStimulus(uint32 AttackerID, int Damage, int DamageType) override;
	void IdleStimulus(void) override;
	void CreatureMoveStimulus(uint32 CreatureID, int Type) override;

	// DATA
	// =================
	//Nonplayer super_TNonplayer; 	// IMPLICIT
	uint32 Interlocutor;
	int Topic;
	int Price;
	int Amount;
	int TypeID;
	uint32 Data;
	uint32 LastTalk;
	vector<uint32> QueuedPlayers;
	vector<uint32> QueuedAddresses;
	int QueueLength;
	BehaviourDatabase *Behaviour;
};

void change_npc_state(TCreature *Npc, int NewState, bool Stimulus);
void init_npcs(void);

#endif // TIBIA_CREATURE_NPC_H_
