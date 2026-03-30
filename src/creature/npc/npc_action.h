// gameserver/src/creature/npc/npc_action.h
#ifndef TIBIA_CREATURE_NPC_ACTION_H_
#define TIBIA_CREATURE_NPC_ACTION_H_ 1

#include "common/types/types.h"

struct TNPC;
struct TPlayer;
struct BehaviourAction;
struct BehaviourDatabase;

enum NpcActionType: int {
    NPC_ACTION_NONE = 0,
    NPC_ACTION_REPLY,
    NPC_ACTION_SET_TOPIC,
    NPC_ACTION_SET_PRICE,
    NPC_ACTION_SET_AMOUNT,
    NPC_ACTION_SET_TYPE,
    NPC_ACTION_SET_DATA,
    NPC_ACTION_SET_HP,
    NPC_ACTION_POISON,
    NPC_ACTION_BURNING,
    NPC_ACTION_EFFECT_ME,
    NPC_ACTION_EFFECT_OPP,
    NPC_ACTION_SET_PROFESSION,
    NPC_ACTION_TEACH_SPELL,
    NPC_ACTION_SUMMON,
    NPC_ACTION_CREATE_ITEM,
    NPC_ACTION_DELETE_ITEM,
    NPC_ACTION_CREATE_MONEY,
    NPC_ACTION_DELETE_MONEY,
    NPC_ACTION_QUEUE,
    NPC_ACTION_SET_QUEST_VALUE,
    NPC_ACTION_TELEPORT,
    NPC_ACTION_START_POSITION,
    NPC_ACTION_CHANGE_STATE,
    NPC_ACTION_REPEAT,
};

enum SITUATION: int;

struct NpcActionContext {
    TNPC *Npc;
    TPlayer *Interlocutor;
    uint32 InterlocutorID;
    const char *Text;
    SITUATION Situation;
    int *Parameters;
    BehaviourDatabase *Database;
    int *TalkDelay;
    bool *StartToDo;
};

typedef void (*NpcActionHandler)(NpcActionContext *Ctx, BehaviourAction *Action);

struct NpcActionDef {
    const char *Name;
    NpcActionType Type;
    int ParamCount;          // -1 = "= expr", 0/1/2/3 = parenthesized params
    NpcActionHandler Execute;
};

NpcActionDef *FindActionByName(const char *Name);
NpcActionDef *FindActionByType(NpcActionType Type);

#endif // TIBIA_CREATURE_NPC_ACTION_H_
