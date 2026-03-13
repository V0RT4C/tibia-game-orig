#include "config.h"
#include "containers.h"
#include "cr.h"
#include "info.h"
#include "magic.h"
#include "operate.h"
#include "writer.h"

#include <dirent.h>

static store<BehaviourNode, 256> BehaviourNodeTable;

// Behaviour Database
// =============================================================================
static BehaviourNode *NewBehaviourNode(int Type, BehaviourNode *Left, BehaviourNode *Right) {
	BehaviourNode *Node = BehaviourNodeTable.getFreeItem();
	Node->Type = Type;
	Node->Data = 0;
	Node->Left = Left;
	Node->Right = Right;
	return Node;
}

static BehaviourNode *NewBehaviourNode(int Type, int Data) {
	BehaviourNode *Node = BehaviourNodeTable.getFreeItem();
	Node->Type = Type;
	Node->Data = Data;
	Node->Left = NULL;
	Node->Right = NULL;
	return Node;
}

static void DeleteBehaviourNode(BehaviourNode *Node) {
	if (Node != NULL) {
		DeleteBehaviourNode(Node->Left);
		DeleteBehaviourNode(Node->Right);
		BehaviourNodeTable.putFreeItem(Node);
	}
}

bool BehaviourCondition::set(int Type, void *Data) {
	if (Type != BEHAVIOUR_CONDITION_SHORTCIRCUIT && Data == NULL) {
		error("BehaviourCondition::set: Data is NULL.\n");
		this->Type = BEHAVIOUR_CONDITION_NONE;
		return false;
	}

	this->Type = Type;
	switch (Type) {
	case BEHAVIOUR_CONDITION_TEXT: {
		this->Text = AddDynamicString((const char *)Data);
		break;
	}

	case BEHAVIOUR_CONDITION_PROPERTY: {
		const char *Property = (const char *)Data;
		this->Property = BEHAVIOUR_PROPERTY_NONE;
		if (strcmp(Property, "address") == 0) {
			this->Property = BEHAVIOUR_PROPERTY_ADDRESS;
		} else if (strcmp(Property, "busy") == 0) {
			this->Property = BEHAVIOUR_PROPERTY_BUSY;
		} else if (strcmp(Property, "vanish") == 0) {
			this->Property = BEHAVIOUR_PROPERTY_VANISH;
		} else if (strcmp(Property, "male") == 0) {
			this->Property = BEHAVIOUR_PROPERTY_MALE;
		} else if (strcmp(Property, "female") == 0) {
			this->Property = BEHAVIOUR_PROPERTY_FEMALE;
		} else if (strcmp(Property, "knight") == 0) {
			this->Property = BEHAVIOUR_PROPERTY_KNIGHT;
		} else if (strcmp(Property, "paladin") == 0) {
			this->Property = BEHAVIOUR_PROPERTY_PALADIN;
		} else if (strcmp(Property, "sorcerer") == 0) {
			this->Property = BEHAVIOUR_PROPERTY_SORCERER;
		} else if (strcmp(Property, "druid") == 0) {
			this->Property = BEHAVIOUR_PROPERTY_DRUID;
		} else if (strcmp(Property, "premium") == 0) {
			this->Property = BEHAVIOUR_PROPERTY_PREMIUM;
		} else if (strcmp(Property, "promoted") == 0) {
			this->Property = BEHAVIOUR_PROPERTY_PROMOTED;
		} else if (strcmp(Property, "pzblock") == 0) {
			this->Property = BEHAVIOUR_PROPERTY_PZBLOCK;
		} else if (strcmp(Property, "nonpvp") == 0) {
			this->Property = BEHAVIOUR_PROPERTY_NONPVP;
		} else if (strcmp(Property, "pvpenforced") == 0) {
			this->Property = BEHAVIOUR_PROPERTY_PVPENFORCED;
		}

		if (this->Property == BEHAVIOUR_PROPERTY_NONE) {
			return false;
		}
		break;
	}

	case BEHAVIOUR_CONDITION_PARAMETER: {
		this->Number = *(int *)Data;
		break;
	}

	case BEHAVIOUR_CONDITION_EXPRESSION: {
		this->Expression = (BehaviourNode *)Data;
		break;
	}

	case BEHAVIOUR_CONDITION_SHORTCIRCUIT: {
		// no-op
		break;
	}

	default: {
		error("BehaviourCondition::set: Invalid condition type %d\n", Type);
		return false;
	}
	}

	return true;
}

void BehaviourCondition::clear(void) {
	if (this->Type == BEHAVIOUR_CONDITION_TEXT) {
		DeleteDynamicString(this->Text);
	} else if (this->Type == BEHAVIOUR_CONDITION_EXPRESSION) {
		DeleteBehaviourNode(this->Expression);
	}

	this->Type = BEHAVIOUR_CONDITION_NONE;
}

bool BehaviourAction::set(int Type, void *Data, void *Data2, void *Data3, void *Data4) {
	this->Type = Type;
	switch (Type) {
	case BEHAVIOUR_ACTION_REPLY: {
		this->Text = AddDynamicString((const char *)Data);
		break;
	}

	case BEHAVIOUR_ACTION_SET_VARIABLE:
	case BEHAVIOUR_ACTION_SET_SKILL:
	case BEHAVIOUR_ACTION_FUNCTION1: {
		this->Number = *(int *)Data;
		this->Expression = (BehaviourNode *)Data2;
		break;
	}

	case BEHAVIOUR_ACTION_FUNCTION2:
	case BEHAVIOUR_ACTION_SET_SKILL_TIMER: {
		this->Number = *(int *)Data;
		this->Expression = (BehaviourNode *)Data2;
		this->Expression2 = (BehaviourNode *)Data3;
		break;
	}

	case BEHAVIOUR_ACTION_FUNCTION0:
	case BEHAVIOUR_ACTION_CHANGESTATE: {
		this->Number = *(int *)Data;
		break;
	}

	case BEHAVIOUR_ACTION_REPEAT: {
		// no-op
		break;
	}

	case BEHAVIOUR_ACTION_FUNCTION3: {
		this->Number = *(int *)Data;
		this->Expression = (BehaviourNode *)Data2;
		this->Expression2 = (BehaviourNode *)Data3;
		this->Expression3 = (BehaviourNode *)Data4;
		break;
	}

	default: {
		error("TAction::set: Invalid action type %d\n", Type);
		return false;
	}
	}

	return true;
}

void BehaviourAction::clear(void) {
	switch (this->Type) {
	case BEHAVIOUR_ACTION_REPLY: {
		DeleteDynamicString(this->Text);
		break;
	}

	case BEHAVIOUR_ACTION_SET_VARIABLE:
	case BEHAVIOUR_ACTION_SET_SKILL:
	case BEHAVIOUR_ACTION_FUNCTION1: {
		DeleteBehaviourNode(this->Expression);
		break;
	}

	case BEHAVIOUR_ACTION_FUNCTION2:
	case BEHAVIOUR_ACTION_SET_SKILL_TIMER: {
		DeleteBehaviourNode(this->Expression);
		DeleteBehaviourNode(this->Expression2);
		break;
	}

	case BEHAVIOUR_ACTION_FUNCTION3: {
		DeleteBehaviourNode(this->Expression);
		DeleteBehaviourNode(this->Expression2);
		DeleteBehaviourNode(this->Expression3);
		break;
	}

	default: {
		break;
	}
	}

	this->Type = BEHAVIOUR_ACTION_NONE;
}

TBehaviour::TBehaviour(void) : Condition(0, 5, 5), Action(0, 5, 5) {
	this->Conditions = 0;
	this->Actions = 0;
}

TBehaviour::~TBehaviour(void) {
	for (int i = 0; i < this->Conditions; i += 1) {
		this->Condition.at(i)->clear();
	}

	for (int i = 0; i < this->Actions; i += 1) {
		this->Action.at(i)->clear();
	}
}

bool TBehaviour::addCondition(int Type, void *Data) {
	bool Result = this->Condition.at(this->Conditions)->set(Type, Data);
	if (Result) {
		this->Conditions += 1;
	}
	return Result;
}

bool TBehaviour::addAction(int Type, void *Data, void *Data2, void *Data3, void *Data4) {
	bool Result = this->Action.at(this->Actions)->set(Type, Data, Data2, Data3, Data4);
	if (Result) {
		this->Actions += 1;
	}
	return Result;
}

TBehaviour::TBehaviour(const TBehaviour &Other) : TBehaviour() {
	this->operator=(Other);
}

void TBehaviour::operator=(const TBehaviour &Other) {
	this->Conditions = Other.Conditions;
	for (int i = 0; i < Other.Conditions; i += 1) {
		*this->Condition.at(i) = Other.Condition.copyAt(i);
	}

	this->Actions = Other.Actions;
	for (int i = 0; i < Other.Actions; i += 1) {
		*this->Action.at(i) = Other.Action.copyAt(i);
	}
}

BehaviourDatabase::BehaviourDatabase(ReadScriptFile *Script) : Behaviour(0, 50, 25) {
	this->Behaviours = 0;
	Script->read_symbol('{');
	Script->next_token();
	while (Script->Token != SPECIAL || Script->get_special() != '}') {
		TBehaviour *Behaviour = this->Behaviour.at(this->Behaviours);

		// NOTE(fusion): Optional conditions.
		if (Script->Token != SPECIAL || Script->get_special() != 'I') {
			while (true) {
				bool Ok = false;
				if (Script->Token == STRING) {
					Ok = Behaviour->addCondition(BEHAVIOUR_CONDITION_TEXT, Script->get_string());
				} else if (Script->Token == IDENTIFIER) {
					Ok = Behaviour->addCondition(BEHAVIOUR_CONDITION_PROPERTY, Script->get_identifier());
				} else if (Script->Token == SPECIAL) {
					if (Script->get_special() == '!') {
						Ok = Behaviour->addCondition(BEHAVIOUR_CONDITION_SHORTCIRCUIT, NULL);
					} else if (Script->get_special() == '%') {
						int Parameter = Script->read_number();
						if (Parameter != 1 && Parameter != 2) {
							Script->error("illegal ordinal number");
						}
						Ok = Behaviour->addCondition(BEHAVIOUR_CONDITION_PARAMETER, &Parameter);
					}
				}

				if (!Ok) {
					BehaviourNode *Left = this->readTerm(Script);
					if (Script->Token != SPECIAL) {
						Script->error("relational operator expected");
					}

					int Operator = BEHAVIOUR_NODE_NONE;
					switch (Script->get_special()) {
					case '<':
						Operator = BEHAVIOUR_NODE_CMP_LT;
						break;
					case '>':
						Operator = BEHAVIOUR_NODE_CMP_GT;
						break;
					case '=':
						Operator = BEHAVIOUR_NODE_CMP_EQ;
						break;
					case 'N':
						Operator = BEHAVIOUR_NODE_CMP_NEQ;
						break;
					case 'L':
						Operator = BEHAVIOUR_NODE_CMP_LE;
						break;
					case 'G':
						Operator = BEHAVIOUR_NODE_CMP_GE;
						break;
					default: {
						Script->error("relational operator expected");
						break;
					}
					}

					Script->next_token();
					BehaviourNode *Right = this->readTerm(Script);
					Behaviour->addCondition(BEHAVIOUR_CONDITION_EXPRESSION, NewBehaviourNode(Operator, Left, Right));
				} else {
					Script->next_token();
				}

				if (Script->Token == SPECIAL && Script->get_special() == ',') {
					Script->next_token();
				} else {
					break;
				}
			}
		}

		if (Script->Token != SPECIAL || Script->get_special() != 'I') {
			Script->error("'->' expected");
		}

		// NOTE(fusion): Required Actions.
		Script->next_token();
		while (true) {
			if (Script->Token == STRING) {
				Behaviour->addAction(BEHAVIOUR_ACTION_REPLY, Script->get_string(), NULL, NULL, NULL);
				Script->next_token();
			} else if (Script->Token == IDENTIFIER) {
				int Type = -1;
				int Data = 0;
				const char *Identifier = Script->get_identifier();
				if (strcmp(Identifier, "topic") == 0) {
					Type = BEHAVIOUR_ACTION_SET_VARIABLE;
					Data = 1; // BEHAVIOUR_VARIABLE_TOPIC ?
				} else if (strcmp(Identifier, "price") == 0) {
					Type = BEHAVIOUR_ACTION_SET_VARIABLE;
					Data = 2; // BEHAVIOUR_VARIABLE_PRICE ?
				} else if (strcmp(Identifier, "amount") == 0) {
					Type = BEHAVIOUR_ACTION_SET_VARIABLE;
					Data = 3; // BEHAVIOUR_VARIABLE_AMOUNT ?
				} else if (strcmp(Identifier, "type") == 0) {
					Type = BEHAVIOUR_ACTION_SET_VARIABLE;
					Data = 4; // BEHAVIOUR_VARIABLE_TYPE
				} else if (strcmp(Identifier, "data") == 0) {
					Type = BEHAVIOUR_ACTION_SET_VARIABLE;
					Data = 6; // BEHAVIOUR_VARIABLE_DATA
				} else if (strcmp(Identifier, "hp") == 0) {
					Type = BEHAVIOUR_ACTION_SET_SKILL;
					Data = SKILL_HITPOINTS;
				} else if (strcmp(Identifier, "poison") == 0) {
					Type = BEHAVIOUR_ACTION_SET_SKILL_TIMER;
					Data = SKILL_POISON;
				} else if (strcmp(Identifier, "burning") == 0) {
					Type = BEHAVIOUR_ACTION_SET_SKILL_TIMER;
					Data = SKILL_BURNING;
				} else if (strcmp(Identifier, "setquestvalue") == 0) {
					Type = BEHAVIOUR_ACTION_FUNCTION2;
					Data = 3; // BEHAVIOUR_FUNCTION2_SETQUESTVALUE ?
				} else if (strcmp(Identifier, "effectme") == 0) {
					Type = BEHAVIOUR_ACTION_FUNCTION1;
					Data = 1; // BEHAVIOUR_FUNCTION1_EFFECTME ?
				} else if (strcmp(Identifier, "effectopp") == 0) {
					Type = BEHAVIOUR_ACTION_FUNCTION1;
					Data = 2; // BEHAVIOUR_FUNCTION1_EFFECTOPP ?
				} else if (strcmp(Identifier, "profession") == 0) {
					Type = BEHAVIOUR_ACTION_FUNCTION1;
					Data = 3; // BEHAVIOUR_FUNCTION1_SETPROFESSION ?
				} else if (strcmp(Identifier, "teachspell") == 0) {
					Type = BEHAVIOUR_ACTION_FUNCTION1;
					Data = 4; // BEHAVIOUR_FUNCTION1_TEACHSPELL ?
				} else if (strcmp(Identifier, "summon") == 0) {
					Type = BEHAVIOUR_ACTION_FUNCTION1;
					Data = 5; // BEHAVIOUR_FUNCTION1_SUMMON ?
				} else if (strcmp(Identifier, "create") == 0) {
					Type = BEHAVIOUR_ACTION_FUNCTION1;
					Data = 6; // BEHAVIOUR_FUNCTION1_CREATE ?
				} else if (strcmp(Identifier, "delete") == 0) {
					Type = BEHAVIOUR_ACTION_FUNCTION1;
					Data = 7; // BEHAVIOUR_FUNCTION1_DELETE ?
				} else if (strcmp(Identifier, "createmoney") == 0) {
					Type = BEHAVIOUR_ACTION_FUNCTION0;
					Data = 1; // BEHAVIOUR_FUNCTION0_CREATEMONEY ?
				} else if (strcmp(Identifier, "deletemoney") == 0) {
					Type = BEHAVIOUR_ACTION_FUNCTION0;
					Data = 2; // BEHAVIOUR_FUNCTION0_DELETEMONEY ?
				} else if (strcmp(Identifier, "queue") == 0) {
					Type = BEHAVIOUR_ACTION_FUNCTION0;
					Data = 3; // BEHAVIOUR_FUNCTION0_ENQUEUE ?
				} else if (strcmp(Identifier, "teleport") == 0) {
					Type = BEHAVIOUR_ACTION_FUNCTION3;
					Data = 1; // BEHAVIOUR_FUNCTION3_TELEPORT ?
				} else if (strcmp(Identifier, "startposition") == 0) {
					Type = BEHAVIOUR_ACTION_FUNCTION3;
					Data = 2; // BEHAVIOUR_FUNCTION3_SETSTART ?
				} else if (strcmp(Identifier, "idle") == 0) {
					Type = BEHAVIOUR_ACTION_CHANGESTATE;
					Data = IDLE;
				} else if (strcmp(Identifier, "nop") == 0) {
					Type = BEHAVIOUR_ACTION_NONE;
					Data = 0;
				}

				switch (Type) {
				case BEHAVIOUR_ACTION_NONE: {
					Script->next_token();
					break;
				}

				case BEHAVIOUR_ACTION_SET_VARIABLE:
				case BEHAVIOUR_ACTION_SET_SKILL: {
					Script->read_symbol('=');
					Script->next_token();
					BehaviourNode *Value = this->readTerm(Script);
					Behaviour->addAction(Type, &Data, Value, NULL, NULL);
					break;
				}

				case BEHAVIOUR_ACTION_FUNCTION1: {
					Script->read_symbol('(');
					Script->next_token();
					BehaviourNode *Param = this->readTerm(Script);
					if (Script->Token != SPECIAL || Script->get_special() != ')') {
						Script->error(") expected");
					}
					Script->next_token();
					Behaviour->addAction(Type, &Data, Param, NULL, NULL);
					break;
				}

				case BEHAVIOUR_ACTION_FUNCTION2:
				case BEHAVIOUR_ACTION_SET_SKILL_TIMER: {
					Script->read_symbol('(');
					Script->next_token();
					BehaviourNode *Param1 = this->readTerm(Script);
					if (Script->Token != SPECIAL || Script->get_special() != ',') {
						Script->error(", expected");
					}
					Script->next_token();
					BehaviourNode *Param2 = this->readTerm(Script);
					if (Script->Token != SPECIAL || Script->get_special() != ')') {
						Script->error(") expected");
					}
					Script->next_token();
					Behaviour->addAction(Type, &Data, Param1, Param2, NULL);
					break;
				}

				case BEHAVIOUR_ACTION_FUNCTION0:
				case BEHAVIOUR_ACTION_CHANGESTATE: {
					Script->next_token();
					Behaviour->addAction(Type, &Data, NULL, NULL, NULL);
					break;
				}

				case BEHAVIOUR_ACTION_FUNCTION3: {
					Script->read_symbol('(');
					Script->next_token();
					BehaviourNode *Param1 = this->readTerm(Script);
					if (Script->Token != SPECIAL || Script->get_special() != ',') {
						Script->error(", expected");
					}
					Script->next_token();
					BehaviourNode *Param2 = this->readTerm(Script);
					if (Script->Token != SPECIAL || Script->get_special() != ',') {
						Script->error(", expected");
					}
					Script->next_token();
					BehaviourNode *Param3 = this->readTerm(Script);
					if (Script->Token != SPECIAL || Script->get_special() != ')') {
						Script->error(") expected");
					}
					Script->next_token();
					Behaviour->addAction(Type, &Data, Param1, Param2, Param3);
					break;
				}

				default: {
					Script->error("unknown identifier");
					break;
				}
				}

			} else if (Script->Token == SPECIAL && Script->get_special() == '*') {
				if (this->Behaviours == 0) {
					Script->error("no previous pattern");
				}
				Script->next_token();
				Behaviour->addAction(BEHAVIOUR_ACTION_REPEAT, NULL, NULL, NULL, NULL);
			} else {
				Script->error("illegal action");
			}

			if (Script->Token == SPECIAL && Script->get_special() == ',') {
				Script->next_token();
			} else {
				break;
			}
		}

		this->Behaviours += 1;
	}
}

BehaviourNode *BehaviourDatabase::readValue(ReadScriptFile *Script) {
	BehaviourNode *Node = NULL;
	if (Script->Token == NUMBER) {
		Node = NewBehaviourNode(BEHAVIOUR_NODE_NUMBER, Script->get_number());
	} else if (Script->Token == IDENTIFIER) {
		const char *Identifier = Script->get_identifier();
		if (strcmp(Identifier, "topic") == 0) {
			Node = NewBehaviourNode(BEHAVIOUR_NODE_TOPIC, 0);
		} else if (strcmp(Identifier, "price") == 0) {
			Node = NewBehaviourNode(BEHAVIOUR_NODE_PRICE, 0);
		} else if (strcmp(Identifier, "amount") == 0) {
			Node = NewBehaviourNode(BEHAVIOUR_NODE_AMOUNT, 0);
		} else if (strcmp(Identifier, "level") == 0) {
			Node = NewBehaviourNode(BEHAVIOUR_NODE_SKILL, SKILL_LEVEL);
		} else if (strcmp(Identifier, "magiclevel") == 0) {
			Node = NewBehaviourNode(BEHAVIOUR_NODE_SKILL, SKILL_MAGIC_LEVEL);
		} else if (strcmp(Identifier, "hp") == 0) {
			Node = NewBehaviourNode(BEHAVIOUR_NODE_SKILL, SKILL_HITPOINTS);
		} else if (strcmp(Identifier, "poison") == 0) {
			Node = NewBehaviourNode(BEHAVIOUR_NODE_SKILL, SKILL_POISON);
		} else if (strcmp(Identifier, "burning") == 0) {
			Node = NewBehaviourNode(BEHAVIOUR_NODE_SKILL, SKILL_BURNING);
		} else if (strcmp(Identifier, "count") == 0) {
			Script->read_symbol('(');
			Script->next_token();
			BehaviourNode *Left = this->readTerm(Script);
			if (Script->Token != SPECIAL || Script->get_special() != ')') {
				Script->error(") expected");
			}

			Node = NewBehaviourNode(BEHAVIOUR_NODE_COUNT, Left, NULL);
		} else if (strcmp(Identifier, "countmoney") == 0) {
			Node = NewBehaviourNode(BEHAVIOUR_NODE_COUNTMONEY, 0);
		} else if (strcmp(Identifier, "type") == 0) {
			Node = NewBehaviourNode(BEHAVIOUR_NODE_TYPE, 0);
		} else if (strcmp(Identifier, "data") == 0) {
			Node = NewBehaviourNode(BEHAVIOUR_NODE_DATA, 0);
		} else if (strcmp(Identifier, "spellknown") == 0) {
			Script->read_symbol('(');
			Script->next_token();
			BehaviourNode *Left = this->readTerm(Script);
			if (Script->Token != SPECIAL || Script->get_special() != ')') {
				Script->error(") expected");
			}

			Node = NewBehaviourNode(BEHAVIOUR_NODE_SPELLKNOWN, Left, NULL);
		} else if (strcmp(Identifier, "spelllevel") == 0) {
			Script->read_symbol('(');
			Script->next_token();
			BehaviourNode *Left = this->readTerm(Script);
			if (Script->Token != SPECIAL || Script->get_special() != ')') {
				Script->error(") expected");
			}

			Node = NewBehaviourNode(BEHAVIOUR_NODE_SPELLLEVEL, Left, NULL);
		} else if (strcmp(Identifier, "random") == 0) {
			Script->read_symbol('(');

			Script->next_token();
			BehaviourNode *Left = this->readTerm(Script);
			if (Script->Token != SPECIAL || Script->get_special() != ',') {
				Script->error(", expected");
			}

			Script->next_token();
			BehaviourNode *Right = this->readTerm(Script);
			if (Script->Token != SPECIAL || Script->get_special() != ')') {
				Script->error(") expected");
			}

			Node = NewBehaviourNode(BEHAVIOUR_NODE_RANDOM, Left, Right);
		} else if (strcmp(Identifier, "questvalue") == 0) {
			Script->read_symbol('(');
			Script->next_token();
			BehaviourNode *Left = this->readTerm(Script);
			if (Script->Token != SPECIAL || Script->get_special() != ')') {
				Script->error(") expected");
			}

			Node = NewBehaviourNode(BEHAVIOUR_NODE_QUESTVALUE, Left, NULL);
		}
	} else if (Script->Token == SPECIAL) {
		if (Script->get_special() != '%') {
			Script->error("illegal character");
		}
		Node = NewBehaviourNode(BEHAVIOUR_NODE_PARAMETER, Script->read_number());
	} else {
		Script->error("illegal value");
	}

	if (Node == NULL) {
		Script->error("unknown value");
	}

	Script->next_token();
	return Node;
}

BehaviourNode *BehaviourDatabase::readFactor(ReadScriptFile *Script) {
	BehaviourNode *Node = this->readValue(Script);
	while (Script->Token == SPECIAL && Script->get_special() == '*') {
		Script->next_token();
		Node = NewBehaviourNode(BEHAVIOUR_NODE_MUL, Node, this->readValue(Script));
	}
	return Node;
}

BehaviourNode *BehaviourDatabase::readTerm(ReadScriptFile *Script) {
	BehaviourNode *Node = this->readFactor(Script);
	while (Script->Token == SPECIAL) {
		int Type = BEHAVIOUR_NODE_NONE;
		if (Script->get_special() == '+') {
			Type = BEHAVIOUR_NODE_ADD;
		} else if (Script->get_special() == '-') {
			Type = BEHAVIOUR_NODE_SUB;
		} else {
			break;
		}

		Script->next_token();
		Node = NewBehaviourNode(Type, Node, this->readFactor(Script));
	}
	return Node;
}

int BehaviourDatabase::evaluate(TNPC *Npc, BehaviourNode *Node, int *Parameters) {
	if (Npc == NULL) {
		error("BehaviourDatabase::evaluate: NPC does not exist.\n");
		return 0;
	}

	if (Node == NULL) {
		error("BehaviourDatabase::evaluate: Node does not exist.\n");
		return 0;
	}

	if (Parameters == NULL) {
		error("BehaviourDatabase::evaluate: Parameters do not exist.\n");
		return 0;
	}

	uint32 InterlocutorID = Npc->Interlocutor;
	TPlayer *Interlocutor = get_player(InterlocutorID);
	if (Interlocutor == NULL) {
		error("BehaviourDatabase::evaluate: Interlocutor does not exist.\n");
		return 0;
	}

	int Result = 0;
	switch (Node->Type) {
	case BEHAVIOUR_NODE_CMP_LT: {
		int Left = this->evaluate(Npc, Node->Left, Parameters);
		int Right = this->evaluate(Npc, Node->Right, Parameters);
		Result = Left < Right;
		break;
	}

	case BEHAVIOUR_NODE_CMP_GT: {
		int Left = this->evaluate(Npc, Node->Left, Parameters);
		int Right = this->evaluate(Npc, Node->Right, Parameters);
		Result = Left > Right;
		break;
	}

	case BEHAVIOUR_NODE_CMP_EQ: {
		int Left = this->evaluate(Npc, Node->Left, Parameters);
		int Right = this->evaluate(Npc, Node->Right, Parameters);
		Result = Left == Right;
		break;
	}

	case BEHAVIOUR_NODE_CMP_NEQ: {
		int Left = this->evaluate(Npc, Node->Left, Parameters);
		int Right = this->evaluate(Npc, Node->Right, Parameters);
		Result = Left != Right;
		break;
	}

	case BEHAVIOUR_NODE_CMP_LE: {
		int Left = this->evaluate(Npc, Node->Left, Parameters);
		int Right = this->evaluate(Npc, Node->Right, Parameters);
		Result = Left <= Right;
		break;
	}

	case BEHAVIOUR_NODE_CMP_GE: {
		int Left = this->evaluate(Npc, Node->Left, Parameters);
		int Right = this->evaluate(Npc, Node->Right, Parameters);
		Result = Left >= Right;
		break;
	}

	case BEHAVIOUR_NODE_ADD: {
		int Left = this->evaluate(Npc, Node->Left, Parameters);
		int Right = this->evaluate(Npc, Node->Right, Parameters);
		Result = Left + Right;
		break;
	}

	case BEHAVIOUR_NODE_SUB: {
		int Left = this->evaluate(Npc, Node->Left, Parameters);
		int Right = this->evaluate(Npc, Node->Right, Parameters);
		Result = Left - Right;
		break;
	}

	case BEHAVIOUR_NODE_MUL: {
		int Left = this->evaluate(Npc, Node->Left, Parameters);
		int Right = this->evaluate(Npc, Node->Right, Parameters);
		Result = Left * Right;
		break;
	}

	case BEHAVIOUR_NODE_PARAMETER: {
		if (Node->Data != 1 && Node->Data != 2) {
			error("BehaviourDatabase::evaluate: Invalid number parameter %d.\n", Node->Data);
		} else if (Parameters[Node->Data - 1] < 0) {
			error("BehaviourDatabase::evaluate: Number parameter %d not assigned.\n", Node->Data);
		} else {
			Result = Parameters[Node->Data - 1];
		}
		break;
	}

	case BEHAVIOUR_NODE_NUMBER: {
		Result = Node->Data;
		break;
	}

	case BEHAVIOUR_NODE_TOPIC: {
		Result = Npc->Topic;
		break;
	}

	case BEHAVIOUR_NODE_PRICE: {
		Result = Npc->Price;
		break;
	}

	case BEHAVIOUR_NODE_AMOUNT: {
		Result = Npc->Amount;
		break;
	}

	case BEHAVIOUR_NODE_SKILL: {
		int SkillNr = Node->Data;
		if (SkillNr == SKILL_LEVEL || SkillNr == SKILL_MAGIC_LEVEL || SkillNr == SKILL_HITPOINTS) {
			Result = Interlocutor->Skills[SkillNr]->Get();
		} else if (SkillNr == SKILL_POISON || SkillNr == SKILL_BURNING) {
			Result = Interlocutor->Skills[SkillNr]->TimerValue();
		} else {
			error("BehaviourDatabase::evaluate: Invalid skill %d.\n", SkillNr);
		}
		break;
	}

	case BEHAVIOUR_NODE_COUNT: {
		int TypeID = this->evaluate(Npc, Node->Left, Parameters);
		Result = count_inventory_objects(InterlocutorID, TypeID, Npc->Data);
		break;
	}

	case BEHAVIOUR_NODE_COUNTMONEY: {
		Result = count_inventory_money(InterlocutorID);
		break;
	}

	case BEHAVIOUR_NODE_TYPE: {
		Result = Npc->TypeID;
		break;
	}

	case BEHAVIOUR_NODE_DATA: {
		Result = Npc->Data;
		break;
	}

	case BEHAVIOUR_NODE_SPELLKNOWN: {
		int SpellNr = this->evaluate(Npc, Node->Left, Parameters);
		Result = Interlocutor->SpellKnown(SpellNr);
		break;
	}

	case BEHAVIOUR_NODE_SPELLLEVEL: {
		int SpellNr = this->evaluate(Npc, Node->Left, Parameters);
		Result = get_spell_level(SpellNr);
		break;
	}

	case BEHAVIOUR_NODE_RANDOM: {
		int Left = this->evaluate(Npc, Node->Left, Parameters);
		int Right = this->evaluate(Npc, Node->Right, Parameters);
		Result = random(Left, Right);
		break;
	}

	case BEHAVIOUR_NODE_QUESTVALUE: {
		int QuestNr = this->evaluate(Npc, Node->Left, Parameters);
		Result = Interlocutor->GetQuestValue(QuestNr);
		break;
	}

	default: {
		error("BehaviourDatabase::evaluate: Invalid node type %d.\n", Node->Type);
		break;
	}
	}
	return Result;
}

// NOTE(fusion): These smaller functions were inside `BehaviourDatabase::react`
// but I figured it would be better to pull them out for readability.
static bool CheckBehaviourProperty(int Property, SITUATION Situation, TPlayer *Interlocutor) {
	if (Interlocutor == NULL) {
		error("CheckBehaviourProperty: Interlocutor is NULL.");
		return false;
	}

	bool Result = false;
	switch (Property) {
	case BEHAVIOUR_PROPERTY_ADDRESS:
		Result = (Situation == ADDRESS || Situation == ADDRESSQUEUE);
		break;
	case BEHAVIOUR_PROPERTY_BUSY:
		Result = (Situation == BUSY);
		break;
	case BEHAVIOUR_PROPERTY_VANISH:
		Result = (Situation == VANISH);
		break;
	case BEHAVIOUR_PROPERTY_MALE:
		Result = (Interlocutor->Sex == 1);
		break;
	case BEHAVIOUR_PROPERTY_FEMALE:
		Result = (Interlocutor->Sex == 2);
		break;
	case BEHAVIOUR_PROPERTY_KNIGHT:
		Result = (Interlocutor->GetEffectiveProfession() == PROFESSION_KNIGHT);
		break;
	case BEHAVIOUR_PROPERTY_PALADIN:
		Result = (Interlocutor->GetEffectiveProfession() == PROFESSION_PALADIN);
		break;
	case BEHAVIOUR_PROPERTY_SORCERER:
		Result = (Interlocutor->GetEffectiveProfession() == PROFESSION_SORCERER);
		break;
	case BEHAVIOUR_PROPERTY_DRUID:
		Result = (Interlocutor->GetEffectiveProfession() == PROFESSION_DRUID);
		break;
	case BEHAVIOUR_PROPERTY_PREMIUM:
		Result = check_right(Interlocutor->ID, PREMIUM_ACCOUNT);
		break;
	case BEHAVIOUR_PROPERTY_PROMOTED:
		Result = Interlocutor->GetActivePromotion();
		break;
	case BEHAVIOUR_PROPERTY_PZBLOCK:
		Result = (Interlocutor->EarliestProtectionZoneRound > RoundNr);
		break;
	case BEHAVIOUR_PROPERTY_NONPVP:
		Result = (WorldType == NON_PVP);
		break;
	case BEHAVIOUR_PROPERTY_PVPENFORCED:
		Result = (WorldType == PVP_ENFORCED);
		break;
	}

	return Result;
}

static bool FormatNpcResponse(char *Buffer, int BufferSize, const char *Template, TNPC *Npc, TPlayer *Interlocutor) {
	if (Buffer == NULL || BufferSize <= 0) {
		error("FormatNpcResponse: Invalid response buffer.");
		return false;
	}

	if (Template == NULL || Template[0] == 0) {
		error("FormatNpcResponse: Template is NULL or empty.");
		return false;
	}

	if (Npc == NULL) {
		error("FormatNpcResponse: Npc is NULL.\n");
		return false;
	}

	if (Interlocutor == NULL) {
		error("FormatNpcResponse: Interlocutor is NULL.\n");
		return false;
	}

	int WritePos = 0;
	int ReadPos = 0;
	while (Template[ReadPos] != 0) {
		if (Template[ReadPos] == '%' && Template[ReadPos + 1] != 0) {
			char Help[50] = {};
			switch (Template[ReadPos + 1]) {
			case 'N':
				strcpy(Help, Interlocutor->Name);
				break;
			case 'A':
				sprintf(Help, "%d", Npc->Amount);
				break;
			case 'P':
				sprintf(Help, "%d", Npc->Price);
				break;
			case 'T': {
				int Hour, Minute;
				GetTime(&Hour, &Minute);
				if (Hour < 12) {
					sprintf(Help, "%d:%.2d am", Hour, Minute);
				} else {
					sprintf(Help, "%d:%.2d pm", (Hour - 12), Minute);
				}
				break;
			}
			}

			// NOTE(fusion): Make sure we don't overflow the buffer.
			int HelpLen = strlen(Help);
			if (HelpLen > 0) {
				if ((WritePos + HelpLen) > BufferSize) {
					HelpLen = BufferSize - WritePos;
				}

				if (HelpLen > 0) {
					memcpy(&Buffer[WritePos], Help, HelpLen);
					WritePos += HelpLen;
				}
			}

			ReadPos += 2;
		} else {
			// NOTE(fusion): Make sure we don't overflow the buffer.
			if (WritePos < BufferSize) {
				Buffer[WritePos] = Template[ReadPos];
				WritePos += 1;
			}

			ReadPos += 1;
		}
	}

	if (WritePos < BufferSize) {
		Buffer[WritePos] = 0;
		return true;
	} else {
		Buffer[BufferSize - 1] = 0;
		return false;
	}
}

void BehaviourDatabase::react(TNPC *Npc, const char *Text, SITUATION Situation) {
	if (Npc == NULL) {
		error("BehaviourDatabase::react: NPC does not exist.\n");
		return;
	}

	if (Text == NULL) {
		error("BehaviourDatabase::react: Provided text does not exist.\n");
		return;
	}

	uint32 InterlocutorID = Npc->Interlocutor;
	TPlayer *Interlocutor = get_player(InterlocutorID);
	if (Interlocutor == NULL) {
		error("BehaviourDatabase::react: Interlocutor does not exist"
			  " (Text=%s, Situation=%d).\n",
			  Text, Situation);
		return;
	}

	int Parameters[2] = {-1, -1};
	int BestMatch = -1;
	int MaxConditions = -1;
	for (int BehaviourNr = 0; BehaviourNr < this->Behaviours; BehaviourNr += 1) {
		TBehaviour *Behaviour = this->Behaviour.at(BehaviourNr);
		bool Match = true;
		bool ShortCircuit = false;
		const char *TextPtr = Text;
		for (int ConditionNr = 0; ConditionNr < Behaviour->Conditions && Match; ConditionNr += 1) {
			BehaviourCondition *Condition = Behaviour->Condition.at(ConditionNr);
			if (Condition->Type == BEHAVIOUR_CONDITION_SHORTCIRCUIT) {
				ShortCircuit = true;
				break;
			}

			switch (Condition->Type) {
			case BEHAVIOUR_CONDITION_TEXT: {
				const char *Pattern = GetDynamicString(Condition->Text);
				const char *Word = SearchForWord(Pattern, TextPtr);
				if (Word != NULL) {
					TextPtr = Word + strlen(Pattern);
				} else {
					Match = false;
				}
				break;
			}

			case BEHAVIOUR_CONDITION_PROPERTY: {
				if (!CheckBehaviourProperty(Condition->Property, Situation, Interlocutor)) {
					Match = false;
				}
				break;
			}

			case BEHAVIOUR_CONDITION_PARAMETER: {
				// TODO(fusion): The original function wouldn't check before
				// writing to `Parameters` which could be a problem.
				const char *Parameter = SearchForNumber(Condition->Number, TextPtr);
				if (Parameter != NULL && Condition->Number >= 1 && Condition->Number <= NARRAY(Parameters)) {
					Parameters[Condition->Number - 1] = atoi(Parameter);
					if (Parameters[Condition->Number - 1] > 500) {
						Parameters[Condition->Number - 1] = 500;
					}

					// TODO(fusion): This could be problematic if the number
					// has more than one digit.
					TextPtr = Parameter + 1;
				} else {
					Match = false;
				}
				break;
			}

			case BEHAVIOUR_CONDITION_EXPRESSION: {
				if (this->evaluate(Npc, Condition->Expression, Parameters) == 0) {
					Match = false;
				}
				break;
			}

			case BEHAVIOUR_CONDITION_SHORTCIRCUIT: {
				// TODO(fusion): Select current and stop loop.
				break;
			}
			}
		}

		if (ShortCircuit || (Match && Behaviour->Conditions > MaxConditions)) {
			BestMatch = BehaviourNr;
			MaxConditions = Behaviour->Conditions;
			if (ShortCircuit) {
				break;
			}
		}
	}

	if (BestMatch == -1) {
		return;
	}

	if (Situation != BUSY) {
		Npc->Topic = 0;
	}

	bool Repeat = false;
	bool StartToDo = false;
	int TalkDelay = 1000;
	int BehaviourNr = BestMatch;
	do {
		Repeat = false;
		TBehaviour *Behaviour = this->Behaviour.at(BehaviourNr);
		for (int ActionNr = 0; ActionNr < Behaviour->Actions; ActionNr += 1) {
			BehaviourAction *Action = Behaviour->Action.at(ActionNr);
			if (Action->Type == BEHAVIOUR_ACTION_REPEAT) {
				if (BehaviourNr > 0) {
					BehaviourNr -= 1;
					Repeat = true;
				} else {
					error("BehaviourDatabase::react (9): No previous pattern.\n");
				}
				break;
			}

			switch (Action->Type) {
			case BEHAVIOUR_ACTION_REPLY: {
				char Response[256] = {};
				const char *Template = GetDynamicString(Action->Text);
				if (FormatNpcResponse(Response, sizeof(Response), Template, Npc, Interlocutor)) {
					Npc->ToDoWait(TalkDelay);
					Npc->ToDoTalk(TALK_SAY, NULL, Response, false);
					TalkDelay += 3100 + (int)(strlen(Response) / 2) * 100;
					StartToDo = true;
				} else {
					Response[20] = 0;
					error("BehaviourDatabase::react: Text from NPC %s is too long (%s...).\n", Npc->Name, Response);
				}
				break;
			}

			case BEHAVIOUR_ACTION_SET_VARIABLE: {
				int Variable = Action->Number;
				int Value = this->evaluate(Npc, Action->Expression, Parameters);
				switch (Variable) {
				case 1:
					Npc->Topic = Value;
					break;
				case 2:
					Npc->Price = Value;
					break;
				case 3:
					Npc->Amount = Value;
					break;
				case 4:
					Npc->TypeID = Value;
					break;
				case 6:
					Npc->Data = Value;
					break;
				default: {
					error("BehaviourDatabase::react: Invalid variable.\n");
					break;
				}
				}
				break;
			}

			case BEHAVIOUR_ACTION_SET_SKILL: {
				int SkillNr = Action->Number;
				int Value = this->evaluate(Npc, Action->Expression, Parameters);
				if (SkillNr == SKILL_HITPOINTS) {
					Interlocutor->Skills[SKILL_HITPOINTS]->Set(Value);
					if (Interlocutor->Skills[SKILL_HITPOINTS]->Get() <= 0) {
						error("BehaviourDatabase::react: NPC %s kills player.\n", Npc->Name);
					}
				} else {
					error("BehaviourDatabase::react: Invalid skill.\n");
				}
				break;
			}

			case BEHAVIOUR_ACTION_FUNCTION2: {
				int FunctionNr = Action->Number;
				int Param1 = this->evaluate(Npc, Action->Expression, Parameters);
				int Param2 = this->evaluate(Npc, Action->Expression2, Parameters);
				switch (FunctionNr) {
				case 3:
					Interlocutor->SetQuestValue(Param1, Param2);
					break;
				default: {
					error("BehaviourDatabase::react (4): Invalid function number.\n");
					break;
				}
				}
				break;
			}

			case BEHAVIOUR_ACTION_FUNCTION1: {
				int FunctionNr = Action->Number;
				int Param = this->evaluate(Npc, Action->Expression, Parameters);
				switch (FunctionNr) {
				case 1:
					graphical_effect(Npc->CrObject, Param);
					break;
				case 2:
					graphical_effect(Interlocutor->CrObject, Param);
					break;
				case 3:
					Interlocutor->SetProfession(Param);
					break;
				case 4:
					Interlocutor->LearnSpell(Param);
					break;
				case 5:
					create_monster(Param, Npc->posx, Npc->posy, Npc->posz, 0, 0, true);
					break;
				case 6:
					Npc->GiveTo(Param, Npc->Amount);
					break;
				case 7:
					Npc->GetFrom(Param, Npc->Amount);
					break;
				default: {
					error("BehaviourDatabase::react (5): Invalid function number.\n");
					break;
				}
				}
				break;
			}

			case BEHAVIOUR_ACTION_FUNCTION0: {
				int FunctionNr = Action->Number;
				switch (FunctionNr) {
				case 1:
					Npc->GiveMoney(Npc->Price);
					break;
				case 2:
					Npc->GetMoney(Npc->Price);
					break;
				case 3: {
					if (Situation == BUSY) {
						Npc->Enqueue(InterlocutorID, Text);
					} else {
						error("BehaviourDatabase::react (6): wrong situation for action \"Queue\".\n");
					}
					break;
				}
				default: {
					error("BehaviourDatabase::react (6): Invalid function number.\n");
					break;
				}
				}
				break;
			}

			case BEHAVIOUR_ACTION_CHANGESTATE: {
				int NewState = Action->Number;
				if (!StartToDo) {
					Npc->ChangeState((STATE)NewState, false);
					if (Situation != ADDRESSQUEUE) {
						StartToDo = true;
					} else {
						error("BehaviourDatabase::react: NPC %s does not respond to address %s.\n", Npc->Name, Text);
					}
				} else {
					if (NewState == IDLE) {
						Npc->ChangeState(LEAVING, false);
					}
					Npc->ToDoChangeState(NewState);
				}
				break;
			}

			case BEHAVIOUR_ACTION_SET_SKILL_TIMER: {
				int SkillNr = Action->Number;
				int Cycle = this->evaluate(Npc, Action->Expression, Parameters);
				int MaxCount = this->evaluate(Npc, Action->Expression2, Parameters);
				if (SkillNr == SKILL_POISON || SkillNr == SKILL_BURNING) {
					if (Cycle == 0 || Interlocutor->Skills[SkillNr]->TimerValue() < Cycle) {
						Interlocutor->SetTimer(SkillNr, Cycle, MaxCount, MaxCount, -1);
						if (SkillNr == SKILL_POISON) {
							Interlocutor->PoisonDamageOrigin = 0;
						} else {
							Interlocutor->FireDamageOrigin = 0;
						}
					}
				} else {
					error("BehaviourDatabase::react (8): Invalid skill.\n");
				}
				break;
			}

			case BEHAVIOUR_ACTION_FUNCTION3: {
				int FunctionNr = Action->Number;
				int Param1 = this->evaluate(Npc, Action->Expression, Parameters);
				int Param2 = this->evaluate(Npc, Action->Expression2, Parameters);
				int Param3 = this->evaluate(Npc, Action->Expression3, Parameters);
				switch (FunctionNr) {
				case 1: {
					print(3, "NPC teleports interlocutor to [%d,%d,%d].\n", Param1, Param2, Param3);
					try {
						Object Dest = get_map_container(Param1, Param2, Param3);
						move(0, Interlocutor->CrObject, Dest, -1, false, NONE);
					} catch (RESULT r) {
						error("BehaviourDatabase::react (10): Exception %d.\n", r);
					}
					break;
				}

				case 2: {
					print(3, "NPC sets start coordinate for interlocutor to [%d,%d,%d].\n", Param1, Param2, Param3);
					Interlocutor->startx = Param1;
					Interlocutor->starty = Param2;
					Interlocutor->startz = Param3;
					Interlocutor->SaveData();
					break;
				}

				default: {
					error("BehaviourDatabase::react (10): Invalid sub-number.\n");
					break;
				}
				}
				break;
			}
			}
		}
	} while (Repeat);

	if (StartToDo) {
		Npc->ToDoWait(TalkDelay);
		Npc->ToDoStart();
		if (Situation != BUSY) {
			Npc->LastTalk = (TalkDelay / 1000) + RoundNr;
		}
	}
}

// TNPC
// =============================================================================
TNPC::TNPC(const char *FileName) : Nonplayer(), QueuedPlayers(0, 9, 10), QueuedAddresses(0, 9, 10) {
	this->Type = NPC;
	this->posz = 255;
	this->Interlocutor = 0;
	this->Topic = 0;
	this->Price = 0;
	this->Amount = 1;
	this->TypeID = 0;
	this->Data = 0;
	this->LastTalk = 0;
	this->QueueLength = 0;
	this->Behaviour = NULL;

	ReadScriptFile Script;
	Script.open(FileName);
	while (true) {
		Script.next_token();
		if (Script.Token == ENDOFFILE) {
			Script.close();
			break;
		}

		if (Script.Token != IDENTIFIER) {
			Script.error("identifier expected");
		}

		char Identifier[MAX_IDENT_LENGTH];
		strcpy(Identifier, Script.get_identifier());
		Script.read_symbol('=');

		if (strcmp(Identifier, "name") == 0) {
			strcpy(this->Name, Script.read_string());
		} else if (strcmp(Identifier, "sex") == 0) {
			const char *Sex = Script.read_identifier();
			if (strcmp(Sex, "male") == 0) {
				this->Sex = 1;
			} else if (strcmp(Sex, "female") == 0) {
				this->Sex = 2;
			} else {
				Script.error("unknown constant");
			}
		} else if (strcmp(Identifier, "race") == 0) {
			this->Race = Script.read_number();
			this->SetSkills(this->Race);
		} else if (strcmp(Identifier, "outfit") == 0) {
			this->OrgOutfit = read_outfit(&Script);
			this->Outfit = this->OrgOutfit;
		} else if (strcmp(Identifier, "home") == 0) {
			Script.read_coordinate(&this->startx, &this->starty, &this->startz);
			this->posx = this->startx;
			this->posy = this->starty;
			this->posz = this->startz;
		} else if (strcmp(Identifier, "radius") == 0) {
			this->Radius = Script.read_number();
		} else if (strcmp(Identifier, "gostrength") == 0) {
			if (this->Race == 0) {
				Script.error("gostrength before race in npc-script-file");
			}
			int GoStrength = Script.read_number();
			this->Skills[SKILL_GO_STRENGTH]->Act = GoStrength;
			this->Skills[SKILL_GO_STRENGTH]->Max = GoStrength;
		} else if (strcmp(Identifier, "behaviour") == 0) {
			if (this->Behaviour != NULL) {
				Script.error("behaviour database specified twice for NPC");
			}
			this->Behaviour = new BehaviourDatabase(&Script);
		} else {
			Script.error("unknown npc property");
		}
	}

	if (this->Name[0] == 0) {
		throw "no name specified for NPC";
	}

	if (this->posz == 255) {
		throw "no startpoint specified for NPC";
	}

	if (this->Behaviour == NULL) {
		throw "no behaviour database specified for NPC";
	}

	if (!is_on_map(this->posx, this->posy, this->posz)) {
		print(1, "WARNING: NPC '%s' is outside the map.\n", this->Name);
		return;
	}

	this->SetID(0);
	this->SetInList();
	if (!this->SetOnMap()) {
		print(1, "WARNING: Cannot place NPC \'%s\'.\n", this->Name);
		return;
	}

	this->ToDoYield();
}

TNPC::~TNPC(void) {
	delete this->Behaviour;
}

bool TNPC::MovePossible(int x, int y, int z, bool Execute, bool Jump) {
	return coordinate_flag(x, y, z, BANK) && !coordinate_flag(x, y, z, UNPASS) && !coordinate_flag(x, y, z, AVOID) &&
		   z == this->startz && std::abs(x - this->startx) <= this->Radius &&
		   std::abs(y - this->starty) <= this->Radius && !is_house(x, y, z);
}

void TNPC::TalkStimulus(uint32 SpeakerID, const char *Text) {
	if (SpeakerID == this->ID) {
		return;
	}

	if (Text == NULL) {
		error("TNPC::TalkStimulus: Provided text does not exist.\n");
		return;
	}

	if (this->State == TALKING || this->QueueLength != 0) {
		uint32 InterlocutorID = this->Interlocutor;
		if (SpeakerID == InterlocutorID) {
			this->LastTalk = RoundNr;
			this->Behaviour->react(this, Text, DEFAULT);
		} else {
			this->Interlocutor = SpeakerID;
			this->Behaviour->react(this, Text, BUSY);
			this->Interlocutor = InterlocutorID;
		}
	} else {
		this->ToDoClear();
		this->ChangeState(TALKING, false);
		this->Interlocutor = SpeakerID;
		this->LastTalk = RoundNr;
		this->Behaviour->react(this, Text, ADDRESS);
		if (this->State == TALKING) {
			this->TurnToInterlocutor();
		}
	}
}

void TNPC::DamageStimulus(uint32 AttackerID, int Damage, int DamageType) {
	// no-op
}

void TNPC::IdleStimulus(void) {
	if (this->State == TALKING) {
		if ((this->LastTalk + 30) > RoundNr) {
			this->ToDoWait(2000);
			this->ToDoStart();
			return;
		}

		this->Behaviour->react(this, "", VANISH);
		this->ChangeState(IDLE, false);
	}

	if (this->State == IDLE) {
		while (this->QueueLength > 0) {
			uint32 InterlocutorID = *this->QueuedPlayers.at(0);
			uint32 Text = *this->QueuedAddresses.at(0);

			// NOTE(fusion): Ordered removal.
			this->QueueLength -= 1;
			for (int i = 0; i < this->QueueLength; i += 1) {
				*this->QueuedPlayers.at(i) = *this->QueuedPlayers.at(i + 1);
				*this->QueuedAddresses.at(i) = *this->QueuedAddresses.at(i + 1);
			}

			this->ToDoClear();

			TCreature *Interlocutor = get_creature(InterlocutorID);
			if (Interlocutor != NULL) {
				this->ChangeState(TALKING, false);
				this->Interlocutor = InterlocutorID;
				this->Behaviour->react(this, GetDynamicString(Text), ADDRESSQUEUE);
			} else {
				error("TNPC::IdleStimulus: Interlocutor does not exist.\n");
			}

			DeleteDynamicString(Text);
			if (this->State == TALKING) {
				this->TurnToInterlocutor();
				this->LastTalk = RoundNr;
				return;
			}
		}

		if (!this->LockToDo) {
			FindCreatures Search(10, 10, this->ID, FIND_PLAYERS);
			if (Search.getNext() == 0) {
				this->ChangeState(SLEEPING, false);
				return;
			}

			bool DestFound = false;
			int DestX, DestY, DestZ;
			for (int i = 0; i < 10; i += 1) {
				DestX = this->posx;
				DestY = this->posy;
				DestZ = this->posz;
				switch (rand() % 4) {
				case 0:
					DestX -= 1;
					break;
				case 1:
					DestX += 1;
					break;
				case 2:
					DestY -= 1;
					break;
				case 3:
					DestY += 1;
					break;
				}

				if (this->MovePossible(DestX, DestY, DestZ, true, false)) {
					DestFound = true;
					break;
				}
			}

			if (DestFound) {
				try {
					this->ToDoGo(DestX, DestY, DestZ, true, INT_MAX);
					this->ToDoWait(2000);
					this->ToDoStart();
				} catch (RESULT r) {
					error("TNPC::IdleStimulus: Exception %d!\n", r);
				}
			} else {
				this->ToDoWait(2000);
				this->ToDoStart();
			}
		}
	}
}

void TNPC::CreatureMoveStimulus(uint32 CreatureID, int Type) {
	for (int i = 0; i < this->QueueLength; i += 1) {
		uint32 QueuedPlayerID = *this->QueuedPlayers.at(i);
		if (CreatureID != QueuedPlayerID && CreatureID != this->ID) {
			continue;
		}

		TCreature *QueuedPlayer = get_creature(QueuedPlayerID);
		if (Type != OBJECT_DELETED && QueuedPlayer->posz == this->posz &&
			std::abs(QueuedPlayer->posx - this->posx) < 5 && std::abs(QueuedPlayer->posy - this->posy) < 4) {
			continue;
		}

		// NOTE(fusion): Ordered removal.
		this->QueueLength -= 1;
		for (int j = i; j < this->QueueLength; j += 1) {
			*this->QueuedPlayers.at(j) = *this->QueuedPlayers.at(j + 1);
			*this->QueuedAddresses.at(j) = *this->QueuedAddresses.at(j + 1);
		}

		// NOTE(fusion): The ordered removal has shifted queued players back one
		// place so we need to check the current queue position again.
		i -= 1;
	}

	if (this->State == TALKING || this->State == LEAVING) {
		if (CreatureID == this->Interlocutor || CreatureID == this->ID) {
			TCreature *Interlocutor = get_creature(this->Interlocutor);
			if (Interlocutor == NULL) {
				error("TNPC::CreatureMoveStimulus: Interlocutor does not exist.\n");
				this->ChangeState(IDLE, true);
				return;
			}

			if (Type != OBJECT_DELETED) {
				this->Rotate(Interlocutor);
			}

			if (this->State == TALKING) {
				if (Type == OBJECT_DELETED || Interlocutor->posz != this->posz ||
					std::abs(Interlocutor->posx - this->posx) >= 5 || std::abs(Interlocutor->posy - this->posy) >= 4) {
					this->Behaviour->react(this, "", VANISH);
					this->ChangeState(IDLE, true);
				}
			}
		}
	}

	if (CreatureID != this->ID && this->State == SLEEPING && Type != OBJECT_DELETED) {
		this->ChangeState(IDLE, true);
	}
}

void TNPC::GiveTo(ObjectType Type, int Amount) {
	if (Amount == 0) {
		return;
	}

	if (Type.is_map_container() || Type.get_name(1) == NULL) {
		error("TNPC::GiveTo: %s wants to create objects of type %d.\n", this->Name, Type.TypeID);
		return;
	}

	log_message("npc", "%s -> %u: %d %s\n", this->Name, this->Interlocutor, Amount, Type.get_name(1));

	if (Type.get_flag(CUMULATIVE)) {
		while (Amount > 0) {
			int StackAmount = std::min<int>(Amount, 100);
			create_at_creature(this->Interlocutor, Type, StackAmount);
			Amount -= StackAmount;
		}
	} else {
		while (Amount > 0) {
			create_at_creature(this->Interlocutor, Type, this->Data);
			Amount -= 1;
		}
	}
}

void TNPC::GetFrom(ObjectType Type, int Amount) {
	if (Amount > 0) {
		log_message("npc", "%s <- %u: %d %s\n", this->Name, this->Interlocutor, Amount, Type.get_name(1));
		delete_at_creature(this->Interlocutor, Type, Amount, this->Data);
	}
}

void TNPC::GiveMoney(int Amount) {
	int Crystal = (Amount / 10000);
	int Platinum = (Amount % 10000) / 100;
	int Gold = (Amount % 10000) % 100;
	this->GiveTo(get_special_object(MONEY_TENTHOUSAND), Crystal);
	this->GiveTo(get_special_object(MONEY_HUNDRED), Platinum);
	this->GiveTo(get_special_object(MONEY_ONE), Gold);
}

void TNPC::GetMoney(int Amount) {
	int Crystal = count_inventory_objects(this->Interlocutor, get_special_object(MONEY_TENTHOUSAND), 0);
	int Platinum = count_inventory_objects(this->Interlocutor, get_special_object(MONEY_HUNDRED), 0);
	int Gold = count_inventory_objects(this->Interlocutor, get_special_object(MONEY_ONE), 0);
	calculate_change(Amount, &Gold, &Platinum, &Crystal);

	if (Gold > 0) {
		this->GetFrom(get_special_object(MONEY_ONE), Gold);
	} else if (Gold < 0) {
		this->GiveTo(get_special_object(MONEY_ONE), -Gold);
	}

	if (Platinum > 0) {
		this->GetFrom(get_special_object(MONEY_HUNDRED), Platinum);
	} else if (Platinum < 0) {
		this->GiveTo(get_special_object(MONEY_HUNDRED), -Platinum);
	}

	ASSERT(Crystal >= 0);
	if (Crystal > 0) {
		this->GetFrom(get_special_object(MONEY_TENTHOUSAND), Crystal);
	}
}

void TNPC::Enqueue(uint32 InterlocutorID, const char *Text) {
	if (InterlocutorID == 0) {
		error("TNPC::Enqueue: Interlocutor is null.\n");
		return;
	}

	if (Text == NULL) {
		error("TNPC::Enqueue: Text is NULL.\n");
		return;
	}

	bool AlreadyQueued = false;
	for (int i = 0; i < this->QueueLength; i += 1) {
		if (*this->QueuedPlayers.at(i) == InterlocutorID) {
			AlreadyQueued = true;
			break;
		}
	}

	if (!AlreadyQueued) {
		*this->QueuedPlayers.at(this->QueueLength) = InterlocutorID;
		*this->QueuedAddresses.at(this->QueueLength) = AddDynamicString(Text);
		this->QueueLength += 1;
	}
}

void TNPC::TurnToInterlocutor(void) {
	TCreature *Interlocutor = get_creature(this->Interlocutor);
	if (Interlocutor == NULL) {
		error("TNPC::TurnToInterlocutor: Interlocutor does not exist.\n");
		return;
	}

	this->Rotate(Interlocutor);
}

void TNPC::ChangeState(STATE NewState, bool Stimulus) {
	this->State = NewState;
	if (Stimulus) {
		this->ToDoYield();
	}
}

void change_npc_state(TCreature *Npc, int NewState, bool Stimulus) {
	if (Npc == NULL) {
		error("change_npc_state: npc is NULL.\n");
		return;
	}

	if (Npc->Type != NPC) {
		error("change_npc_state: npc is not an NPC.\n");
		return;
	}

	((TNPC *)Npc)->ChangeState((STATE)NewState, Stimulus);
}

// Initialization
// =============================================================================
void init_npcs(void) {
	DIR *NpcDir = opendir(NPCPATH);
	if (NpcDir == NULL) {
		error("init_npcs: Subdirectory %s not found\n", NPCPATH);
		throw "Cannot init NPCs";
	}

	char FileName[4096];
	while (dirent *DirEntry = readdir(NpcDir)) {
		if (DirEntry->d_type != DT_REG) {
			continue;
		}

		const char *FileExt = findLast(DirEntry->d_name, '.');
		if (FileExt == NULL || strcmp(FileExt, ".npc") != 0) {
			continue;
		}

		snprintf(FileName, sizeof(FileName), "%s/%s", NPCPATH, DirEntry->d_name);
		new TNPC(FileName);
	}

	closedir(NpcDir);
}
