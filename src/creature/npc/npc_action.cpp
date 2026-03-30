// gameserver/src/creature/npc/npc_action.cpp
#include "creature/npc/npc_action.h"

#include <cstring>

// Forward declarations of handler functions (defined in npc_action_handlers.cpp)
void HandleSetTopic(NpcActionContext *Ctx, BehaviourAction *Action);
void HandleSetPrice(NpcActionContext *Ctx, BehaviourAction *Action);
void HandleSetAmount(NpcActionContext *Ctx, BehaviourAction *Action);
void HandleSetType(NpcActionContext *Ctx, BehaviourAction *Action);
void HandleSetData(NpcActionContext *Ctx, BehaviourAction *Action);
void HandleSetHp(NpcActionContext *Ctx, BehaviourAction *Action);
void HandlePoison(NpcActionContext *Ctx, BehaviourAction *Action);
void HandleBurning(NpcActionContext *Ctx, BehaviourAction *Action);
void HandleEffectMe(NpcActionContext *Ctx, BehaviourAction *Action);
void HandleEffectOpp(NpcActionContext *Ctx, BehaviourAction *Action);
void HandleSetProfession(NpcActionContext *Ctx, BehaviourAction *Action);
void HandleTeachSpell(NpcActionContext *Ctx, BehaviourAction *Action);
void HandleSummon(NpcActionContext *Ctx, BehaviourAction *Action);
void HandleCreateItem(NpcActionContext *Ctx, BehaviourAction *Action);
void HandleDeleteItem(NpcActionContext *Ctx, BehaviourAction *Action);
void HandleCreateMoney(NpcActionContext *Ctx, BehaviourAction *Action);
void HandleDeleteMoney(NpcActionContext *Ctx, BehaviourAction *Action);
void HandleQueue(NpcActionContext *Ctx, BehaviourAction *Action);
void HandleSetQuestValue(NpcActionContext *Ctx, BehaviourAction *Action);
void HandleTeleport(NpcActionContext *Ctx, BehaviourAction *Action);
void HandleStartPosition(NpcActionContext *Ctx, BehaviourAction *Action);

static NpcActionDef ActionRegistry[] = {
    // ParamCount -1 = "= expr" parsing, 0/1/2/3 = parenthesized params
    { "topic",          NPC_ACTION_SET_TOPIC,         -1, HandleSetTopic },
    { "price",          NPC_ACTION_SET_PRICE,         -1, HandleSetPrice },
    { "amount",         NPC_ACTION_SET_AMOUNT,        -1, HandleSetAmount },
    { "type",           NPC_ACTION_SET_TYPE,          -1, HandleSetType },
    { "data",           NPC_ACTION_SET_DATA,          -1, HandleSetData },
    { "hp",             NPC_ACTION_SET_HP,            -1, HandleSetHp },
    { "poison",         NPC_ACTION_POISON,             2, HandlePoison },
    { "burning",        NPC_ACTION_BURNING,            2, HandleBurning },
    { "effectme",       NPC_ACTION_EFFECT_ME,          1, HandleEffectMe },
    { "effectopp",      NPC_ACTION_EFFECT_OPP,         1, HandleEffectOpp },
    { "profession",     NPC_ACTION_SET_PROFESSION,     1, HandleSetProfession },
    { "teachspell",     NPC_ACTION_TEACH_SPELL,        1, HandleTeachSpell },
    { "summon",         NPC_ACTION_SUMMON,              1, HandleSummon },
    { "create",         NPC_ACTION_CREATE_ITEM,         1, HandleCreateItem },
    { "delete",         NPC_ACTION_DELETE_ITEM,         1, HandleDeleteItem },
    { "createmoney",    NPC_ACTION_CREATE_MONEY,        0, HandleCreateMoney },
    { "deletemoney",    NPC_ACTION_DELETE_MONEY,        0, HandleDeleteMoney },
    { "queue",          NPC_ACTION_QUEUE,               0, HandleQueue },
    { "setquestvalue",  NPC_ACTION_SET_QUEST_VALUE,    2, HandleSetQuestValue },
    { "teleport",       NPC_ACTION_TELEPORT,            3, HandleTeleport },
    { "startposition",  NPC_ACTION_START_POSITION,      3, HandleStartPosition },
};

static const int ActionRegistrySize = sizeof(ActionRegistry) / sizeof(ActionRegistry[0]);

NpcActionDef *FindActionByName(const char *Name) {
    for (int i = 0; i < ActionRegistrySize; i++) {
        if (strcmp(ActionRegistry[i].Name, Name) == 0) {
            return &ActionRegistry[i];
        }
    }
    return nullptr;
}

NpcActionDef *FindActionByType(NpcActionType Type) {
    for (int i = 0; i < ActionRegistrySize; i++) {
        if (ActionRegistry[i].Type == Type) {
            return &ActionRegistry[i];
        }
    }
    return nullptr;
}
