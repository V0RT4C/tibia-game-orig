// gameserver/src/creature/npc/npc_action_handlers.cpp
#include "creature/npc/npc_action.h"
#include "creature/npc/npc.h"
#include "cr.h"
#include "operate.h"
#include "magic.h"
#include "writer.h"

// Variable setters
void HandleSetTopic(NpcActionContext *Ctx, BehaviourAction *Action) {
    int Value = Ctx->Database->evaluate(Ctx->Npc, Action->Expression, Ctx->Parameters);
    Ctx->Npc->Topic = Value;
}

void HandleSetPrice(NpcActionContext *Ctx, BehaviourAction *Action) {
    int Value = Ctx->Database->evaluate(Ctx->Npc, Action->Expression, Ctx->Parameters);
    Ctx->Npc->Price = Value;
}

void HandleSetAmount(NpcActionContext *Ctx, BehaviourAction *Action) {
    int Value = Ctx->Database->evaluate(Ctx->Npc, Action->Expression, Ctx->Parameters);
    Ctx->Npc->Amount = Value;
}

void HandleSetType(NpcActionContext *Ctx, BehaviourAction *Action) {
    int Value = Ctx->Database->evaluate(Ctx->Npc, Action->Expression, Ctx->Parameters);
    Ctx->Npc->TypeID = Value;
}

void HandleSetData(NpcActionContext *Ctx, BehaviourAction *Action) {
    int Value = Ctx->Database->evaluate(Ctx->Npc, Action->Expression, Ctx->Parameters);
    Ctx->Npc->Data = Value;
}

// Skill setters
void HandleSetHp(NpcActionContext *Ctx, BehaviourAction *Action) {
    int Value = Ctx->Database->evaluate(Ctx->Npc, Action->Expression, Ctx->Parameters);
    Ctx->Interlocutor->Skills[SKILL_HITPOINTS]->Set(Value);
    if (Ctx->Interlocutor->Skills[SKILL_HITPOINTS]->Get() <= 0) {
        error("HandleSetHp: NPC %s kills player.\n", Ctx->Npc->Name);
    }
}

void HandlePoison(NpcActionContext *Ctx, BehaviourAction *Action) {
    int Cycle = Ctx->Database->evaluate(Ctx->Npc, Action->Expression, Ctx->Parameters);
    int MaxCount = Ctx->Database->evaluate(Ctx->Npc, Action->Expression2, Ctx->Parameters);
    if (Cycle == 0 || Ctx->Interlocutor->Skills[SKILL_POISON]->TimerValue() < Cycle) {
        Ctx->Interlocutor->SetTimer(SKILL_POISON, Cycle, MaxCount, MaxCount, -1);
        Ctx->Interlocutor->PoisonDamageOrigin = 0;
    }
}

void HandleBurning(NpcActionContext *Ctx, BehaviourAction *Action) {
    int Cycle = Ctx->Database->evaluate(Ctx->Npc, Action->Expression, Ctx->Parameters);
    int MaxCount = Ctx->Database->evaluate(Ctx->Npc, Action->Expression2, Ctx->Parameters);
    if (Cycle == 0 || Ctx->Interlocutor->Skills[SKILL_BURNING]->TimerValue() < Cycle) {
        Ctx->Interlocutor->SetTimer(SKILL_BURNING, Cycle, MaxCount, MaxCount, -1);
        Ctx->Interlocutor->FireDamageOrigin = 0;
    }
}

// Effects
void HandleEffectMe(NpcActionContext *Ctx, BehaviourAction *Action) {
    int Param = Ctx->Database->evaluate(Ctx->Npc, Action->Expression, Ctx->Parameters);
    graphical_effect(Ctx->Npc->CrObject, Param);
}

void HandleEffectOpp(NpcActionContext *Ctx, BehaviourAction *Action) {
    int Param = Ctx->Database->evaluate(Ctx->Npc, Action->Expression, Ctx->Parameters);
    graphical_effect(Ctx->Interlocutor->CrObject, Param);
}

// Player modification
void HandleSetProfession(NpcActionContext *Ctx, BehaviourAction *Action) {
    int Param = Ctx->Database->evaluate(Ctx->Npc, Action->Expression, Ctx->Parameters);
    Ctx->Interlocutor->SetProfession(Param);
}

void HandleTeachSpell(NpcActionContext *Ctx, BehaviourAction *Action) {
    int Param = Ctx->Database->evaluate(Ctx->Npc, Action->Expression, Ctx->Parameters);
    Ctx->Interlocutor->LearnSpell(Param);
}

// World modification
void HandleSummon(NpcActionContext *Ctx, BehaviourAction *Action) {
    int Param = Ctx->Database->evaluate(Ctx->Npc, Action->Expression, Ctx->Parameters);
    create_monster(Param, Ctx->Npc->posx, Ctx->Npc->posy, Ctx->Npc->posz, 0, 0, true);
}

void HandleCreateItem(NpcActionContext *Ctx, BehaviourAction *Action) {
    int Param = Ctx->Database->evaluate(Ctx->Npc, Action->Expression, Ctx->Parameters);
    Ctx->Npc->GiveTo(Param, Ctx->Npc->Amount);
}

void HandleDeleteItem(NpcActionContext *Ctx, BehaviourAction *Action) {
    int Param = Ctx->Database->evaluate(Ctx->Npc, Action->Expression, Ctx->Parameters);
    Ctx->Npc->GetFrom(Param, Ctx->Npc->Amount);
}

// Money
void HandleCreateMoney(NpcActionContext *Ctx, BehaviourAction *Action) {
    Ctx->Npc->GiveMoney(Ctx->Npc->Price);
}

void HandleDeleteMoney(NpcActionContext *Ctx, BehaviourAction *Action) {
    Ctx->Npc->GetMoney(Ctx->Npc->Price);
}

// Queue
void HandleQueue(NpcActionContext *Ctx, BehaviourAction *Action) {
    if (Ctx->Situation == BUSY) {
        Ctx->Npc->Enqueue(Ctx->InterlocutorID, Ctx->Text);
    } else {
        error("HandleQueue: wrong situation for action \"Queue\".\n");
    }
}

// Quest
void HandleSetQuestValue(NpcActionContext *Ctx, BehaviourAction *Action) {
    int Param1 = Ctx->Database->evaluate(Ctx->Npc, Action->Expression, Ctx->Parameters);
    int Param2 = Ctx->Database->evaluate(Ctx->Npc, Action->Expression2, Ctx->Parameters);
    Ctx->Interlocutor->SetQuestValue(Param1, Param2);
}

// Movement
void HandleTeleport(NpcActionContext *Ctx, BehaviourAction *Action) {
    int x = Ctx->Database->evaluate(Ctx->Npc, Action->Expression, Ctx->Parameters);
    int y = Ctx->Database->evaluate(Ctx->Npc, Action->Expression2, Ctx->Parameters);
    int z = Ctx->Database->evaluate(Ctx->Npc, Action->Expression3, Ctx->Parameters);
    print(3, "NPC teleports interlocutor to [%d,%d,%d].\n", x, y, z);
    try {
        Object Dest = get_map_container(x, y, z);
        move(0, Ctx->Interlocutor->CrObject, Dest, -1, false, NONE);
    } catch (RESULT r) {
        error("HandleTeleport: Exception %d.\n", r);
    }
}

void HandleStartPosition(NpcActionContext *Ctx, BehaviourAction *Action) {
    int x = Ctx->Database->evaluate(Ctx->Npc, Action->Expression, Ctx->Parameters);
    int y = Ctx->Database->evaluate(Ctx->Npc, Action->Expression2, Ctx->Parameters);
    int z = Ctx->Database->evaluate(Ctx->Npc, Action->Expression3, Ctx->Parameters);
    print(3, "NPC sets start coordinate for interlocutor to [%d,%d,%d].\n", x, y, z);
    Ctx->Interlocutor->startx = x;
    Ctx->Interlocutor->starty = y;
    Ctx->Interlocutor->startz = z;
    Ctx->Interlocutor->SaveData();
}
