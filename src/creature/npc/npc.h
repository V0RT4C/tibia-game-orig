#ifndef TIBIA_CREATURE_NPC_H_
#define TIBIA_CREATURE_NPC_H_ 1

#include "creature/nonplayer/nonplayer.h"

struct TNPC; // forward declaration for TBehaviourDatabase

// TNPC
// =============================================================================
struct TBehaviourNode {
	int Type;
	int Data;
	TBehaviourNode *Left;
	TBehaviourNode *Right;
};

struct TBehaviourCondition {
	bool set(int Type, void *Data);
	void clear(void);

	// DATA
	// =================
	int Type;
	uint32 Text;
	TBehaviourNode *Expression;
	int Property;
	int Number;
};

struct TBehaviourAction {
	bool set(int Type, void *Data, void *Data2, void *Data3, void *Data4);
	void clear(void);

	// DATA
	// =================
	int Type;
	uint32 Text;
	int Number;
	TBehaviourNode *Expression;
	TBehaviourNode *Expression2;
	TBehaviourNode *Expression3;
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
	vector<TBehaviourCondition> Condition;
	vector<TBehaviourAction> Action;
	int Conditions;
	int Actions;
};

struct TBehaviourDatabase {
	TBehaviourDatabase(ReadScriptFile *Script);

	// TODO(fusion): These could/should be standalone functions.
	TBehaviourNode *readValue(ReadScriptFile *Script);
	TBehaviourNode *readFactor(ReadScriptFile *Script);
	TBehaviourNode *readTerm(ReadScriptFile *Script);
	int evaluate(TNPC *Npc, TBehaviourNode *Node, int *Parameters);

	void react(TNPC *Npc, const char *Text, SITUATION Situation);

	// DATA
	// =================
	vector<TBehaviour> Behaviour;
	int Behaviours;
};

struct TNPC: TNonplayer {
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
	//TNonplayer super_TNonplayer; 	// IMPLICIT
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
	TBehaviourDatabase *Behaviour;
};

void ChangeNPCState(TCreature *Npc, int NewState, bool Stimulus);
void InitNPCs(void);

#endif // TIBIA_CREATURE_NPC_H_
