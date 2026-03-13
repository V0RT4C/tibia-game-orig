#include "config.h"
#include "cr.h"
#include "enums.h"
#include "info.h"
#include "writer.h"

#include <dirent.h>

// Race
// =============================================================================
RaceData::RaceData(void) : Skill(1, 5, 5), Talk(1, 5, 5), Item(1, 5, 5), Spell(1, 5, 5) {
	this->Name[0] = 0;
	this->Article[0] = 0;
	this->Outfit = TOutfit{};
	this->Blood = BT_BLOOD;
	this->ExperiencePoints = 0;
	this->FleeThreshold = 0;
	this->Attack = 0;
	this->Defend = 0;
	this->Armor = 0;
	this->Poison = 0;
	this->SummonCost = 0;
	this->LoseTarget = 0;
	this->Strategy[0] = 100;
	this->Strategy[1] = 0;
	this->Strategy[2] = 0;
	this->Strategy[3] = 0;
	this->KickBoxes = false;
	this->KickCreatures = false;
	this->SeeInvisible = false;
	this->Unpushable = false;
	this->DistanceFighting = false;
	this->NoSummon = false;
	this->NoIllusion = false;
	this->NoConvince = false;
	this->NoBurning = false;
	this->NoPoison = false;
	this->NoEnergy = false;
	this->NoHit = false;
	this->NoLifeDrain = false;
	this->NoParalyze = false;
	this->Skills = 0;
	this->Talks = 0;
	this->Items = 0;
	this->Spells = 0;
}

bool is_race_valid(int Race) {
	return Race >= 1 && Race < MAX_RACES;
}

int get_race_by_name(const char *RaceName) {
	int Result = 0;
	for (int Race = 1; Race < MAX_RACES; Race += 1) {
		if (stricmp(RaceName, race_data[Race].Name) == 0) {
			Result = Race;
			break;
		}
	}
	return Result;
}

const char *get_race_name(int Race) {
	if (!is_race_valid(Race)) {
		error("get_race_name: Invalid race number %d.\n", Race);
		return NULL;
	}

	return race_data[Race].Name;
}

TOutfit get_race_outfit(int Race) {
	if (!is_race_valid(Race)) {
		error("get_race_outfit: Invalid race number %d.\n", Race);
		return race_data[1].Outfit;
	}

	return race_data[Race].Outfit;
}

bool get_race_no_summon(int Race) {
	if (!is_race_valid(Race)) {
		error("get_race_no_summon: Invalid race number %d.\n", Race);
		return true;
	}

	return race_data[Race].NoSummon;
}

bool get_race_no_convince(int Race) {
	if (!is_race_valid(Race)) {
		error("get_race_no_convince: Invalid race number %d.\n", Race);
		return true;
	}

	return race_data[Race].NoConvince;
}

bool get_race_no_illusion(int Race) {
	if (!is_race_valid(Race)) {
		error("get_race_no_illusion: Invalid race number %d.\n", Race);
		return true;
	}

	return race_data[Race].NoIllusion;
}

bool get_race_no_paralyze(int Race) {
	if (!is_race_valid(Race)) {
		error("get_race_no_paralyze: Invalid race number %d.\n", Race);
		return true;
	}

	return race_data[Race].NoParalyze;
}

int get_race_summon_cost(int Race) {
	if (!is_race_valid(Race)) {
		error("get_race_summon_cost: Invalid race number %d.\n", Race);
		return 0;
	}

	return race_data[Race].SummonCost;
}

int get_race_poison(int Race) {
	if (!is_race_valid(Race)) {
		error("get_race_poison: Invalid race number %d.\n", Race);
		return 0;
	}

	return race_data[Race].Poison;
}

bool get_race_unpushable(int Race) {
	if (!is_race_valid(Race)) {
		error("get_race_unpushable: Invalid race number %d.\n", Race);
		return true;
	}

	return race_data[Race].Unpushable;
}

// TODO(fusion): Probably move this somewhere else?
TOutfit read_outfit(ReadScriptFile *Script) {
	TOutfit Outfit = {};
	Script->read_symbol('(');
	Outfit.OutfitID = Script->read_number();
	Script->read_symbol(',');
	if (Outfit.OutfitID == 0) {
		Outfit.ObjectType = Script->read_number();
	} else {
		memcpy(Outfit.Colors, Script->read_bytesequence(), sizeof(Outfit.Colors));
	}
	Script->read_symbol(')');
	return Outfit;
}

// TODO(fusion): Probably move this somewhere else?
void write_outfit(WriteScriptFile *Script, TOutfit Outfit) {
	Script->write_text("(");
	Script->write_number(Outfit.OutfitID);
	Script->write_text(",");
	if (Outfit.OutfitID == 0) {
		Script->write_number(Outfit.ObjectType);
	} else {
		Script->write_bytesequence(Outfit.Colors, sizeof(Outfit.Colors));
	}
	Script->write_text(")");
}

void load_race(const char *FileName) {
	ReadScriptFile Script;

	Script.open(FileName);

	// NOTE(fusion): It seems we expect `RaceNumber` to be the first attribute
	// declared in a race file.
	if (strcmp(Script.read_identifier(), "racenumber") != 0) {
		Script.error("race number expected");
	}

	Script.read_symbol('=');

	int RaceNumber = Script.read_number();
	if (!is_race_valid(RaceNumber)) {
		Script.error("illegal race number");
	}

	RaceData *Race = &race_data[RaceNumber];
	if (Race->Name[0] != 0) {
		Script.error("race already defined");
	}

	Race->Outfit = TOutfit{};
	Race->Outfit.OutfitID = RaceNumber;

	while (true) {
		Script.next_token();
		if (Script.Token == ENDOFFILE) {
			Script.close();
			break;
		}

		char Identifier[MAX_IDENT_LENGTH];
		strcpy(Identifier, Script.get_identifier());
		Script.read_symbol('=');

		if (strcmp(Identifier, "name") == 0) {
			strcpy(Race->Name, Script.read_string());
		} else if (strcmp(Identifier, "article") == 0) {
			strcpy(Race->Article, Script.read_string());
		} else if (strcmp(Identifier, "outfit") == 0) {
			Race->Outfit = read_outfit(&Script);
		} else if (strcmp(Identifier, "corpse") == 0) {
			int CorpseTypeID = Script.read_number();
			Race->MaleCorpse = CorpseTypeID;
			Race->FemaleCorpse = CorpseTypeID;
		} else if (strcmp(Identifier, "corpses") == 0) {
			Race->MaleCorpse = Script.read_number();
			Script.read_symbol(',');
			Race->FemaleCorpse = Script.read_number();
		} else if (strcmp(Identifier, "blood") == 0) {
			const char *Blood = Script.read_identifier();
			if (strcmp(Blood, "blood") == 0) {
				Race->Blood = BT_BLOOD;
			} else if (strcmp(Blood, "slime") == 0) {
				Race->Blood = BT_SLIME;
			} else if (strcmp(Blood, "bones") == 0) {
				Race->Blood = BT_BONES;
			} else if (strcmp(Blood, "fire") == 0) {
				Race->Blood = BT_FIRE;
			} else if (strcmp(Blood, "energy") == 0) {
				Race->Blood = BT_ENERGY;
			} else {
				Script.error("unknown blood type");
			}
		} else if (strcmp(Identifier, "experience") == 0) {
			Race->ExperiencePoints = Script.read_number();
		} else if (strcmp(Identifier, "summoncost") == 0) {
			Race->SummonCost = Script.read_number();
		} else if (strcmp(Identifier, "fleethreshold") == 0) {
			Race->FleeThreshold = Script.read_number();
		} else if (strcmp(Identifier, "attack") == 0) {
			Race->Attack = Script.read_number();
		} else if (strcmp(Identifier, "defend") == 0) {
			Race->Defend = Script.read_number();
		} else if (strcmp(Identifier, "armor") == 0) {
			Race->Armor = Script.read_number();
		} else if (strcmp(Identifier, "poison") == 0) {
			Race->Poison = Script.read_number();
		} else if (strcmp(Identifier, "losetarget") == 0) {
			Race->LoseTarget = Script.read_number();
		} else if (strcmp(Identifier, "strategy") == 0) {
			Script.read_symbol('(');
			Race->Strategy[0] = Script.read_number();
			Script.read_symbol(',');
			Race->Strategy[1] = Script.read_number();
			Script.read_symbol(',');
			Race->Strategy[2] = Script.read_number();
			Script.read_symbol(',');
			Race->Strategy[3] = Script.read_number();
			Script.read_symbol(')');
		} else if (strcmp(Identifier, "flags") == 0) {
			Script.read_symbol('{');
			do {
				const char *Flag = Script.read_identifier();
				if (strcmp(Flag, "kickboxes") == 0) {
					Race->KickBoxes = true;
				} else if (strcmp(Flag, "kickcreatures") == 0) {
					Race->KickCreatures = true;
				} else if (strcmp(Flag, "seeinvisible") == 0) {
					Race->SeeInvisible = true;
				} else if (strcmp(Flag, "unpushable") == 0) {
					Race->Unpushable = true;
				} else if (strcmp(Flag, "distancefighting") == 0) {
					Race->DistanceFighting = true;
				} else if (strcmp(Flag, "nosummon") == 0) {
					Race->NoSummon = true;
				} else if (strcmp(Flag, "noconvince") == 0) {
					Race->NoConvince = true;
				} else if (strcmp(Flag, "noillusion") == 0) {
					Race->NoIllusion = true;
				} else if (strcmp(Flag, "noburning") == 0) {
					Race->NoBurning = true;
				} else if (strcmp(Flag, "nopoison") == 0) {
					Race->NoPoison = true;
				} else if (strcmp(Flag, "noenergy") == 0) {
					Race->NoEnergy = true;
				} else if (strcmp(Flag, "nohit") == 0) {
					Race->NoHit = true;
				} else if (strcmp(Flag, "nolifedrain") == 0) {
					Race->NoLifeDrain = true;
				} else if (strcmp(Flag, "noparalyze") == 0) {
					Race->NoParalyze = true;
				} else {
					Script.error("unknown flag");
				}
			} while (Script.read_special() != '}');
		} else if (strcmp(Identifier, "skills") == 0) {
			Script.read_symbol('{');
			do {
				Script.read_symbol('(');
				int SkillNr = get_skill_by_name(Script.read_identifier());
				if (SkillNr == -1) {
					Script.error("unknown skill name");
				}

				// NOTE(fusion): Skills are indexed from 1.
				Race->Skills += 1;
				SkillData *SkillData = Race->Skill.at(Race->Skills);
				SkillData->Nr = SkillNr;
				Script.read_symbol(',');
				SkillData->Actual = Script.read_number();
				Script.read_symbol(',');
				SkillData->Minimum = Script.read_number();
				Script.read_symbol(',');
				SkillData->Maximum = Script.read_number();
				Script.read_symbol(',');
				SkillData->NextLevel = Script.read_number();
				Script.read_symbol(',');
				SkillData->FactorPercent = Script.read_number();
				Script.read_symbol(',');
				SkillData->AddLevel = Script.read_number();
				Script.read_symbol(')');
			} while (Script.read_special() != '}');
		} else if (strcmp(Identifier, "talk") == 0) {
			Script.read_symbol('{');
			do {
				// NOTE(fusion): Talks are indexed from 1.
				Race->Talks += 1;
				*Race->Talk.at(Race->Talks) = AddDynamicString(Script.read_string());
			} while (Script.read_special() != '}');
		} else if (strcmp(Identifier, "inventory") == 0) {
			Script.read_symbol('{');
			do {
				// NOTE(fusion): Items are indexed from 1.
				Race->Items += 1;
				ItemData *ItemData = Race->Item.at(Race->Items);
				Script.read_symbol('(');
				ItemData->Type = Script.read_number();
				Script.read_symbol(',');
				ItemData->Maximum = Script.read_number();
				Script.read_symbol(',');
				ItemData->Probability = Script.read_number();
				Script.read_symbol(')');
			} while (Script.read_special() != '}');
		} else if (strcmp(Identifier, "spells") == 0) {
			Script.read_symbol('{');
			do {
				// NOTE(fusion): Spells are indexed from 1.
				Race->Spells += 1;
				SpellData *SpellData = Race->Spell.at(Race->Spells);

				// NOTE(fusion): Spell shape.
				{
					const char *SpellShape = Script.read_identifier();
					if (strcmp(SpellShape, "actor") == 0) {
						SpellData->Shape = SHAPE_ACTOR;
						Script.read_symbol('(');
						SpellData->ShapeParam1 = Script.read_number();
						Script.read_symbol(')');
					} else if (strcmp(SpellShape, "victim") == 0) {
						SpellData->Shape = SHAPE_VICTIM;
						Script.read_symbol('(');
						SpellData->ShapeParam1 = Script.read_number();
						Script.read_symbol(',');
						SpellData->ShapeParam2 = Script.read_number();
						Script.read_symbol(',');
						SpellData->ShapeParam3 = Script.read_number();
						Script.read_symbol(')');
					} else if (strcmp(SpellShape, "origin") == 0) {
						SpellData->Shape = SHAPE_ORIGIN;
						Script.read_symbol('(');
						SpellData->ShapeParam1 = Script.read_number();
						Script.read_symbol(',');
						SpellData->ShapeParam2 = Script.read_number();
						Script.read_symbol(')');
					} else if (strcmp(SpellShape, "destination") == 0) {
						SpellData->Shape = SHAPE_DESTINATION;
						Script.read_symbol('(');
						SpellData->ShapeParam1 = Script.read_number();
						Script.read_symbol(',');
						SpellData->ShapeParam2 = Script.read_number();
						Script.read_symbol(',');
						SpellData->ShapeParam3 = Script.read_number();
						Script.read_symbol(',');
						SpellData->ShapeParam4 = Script.read_number();
						Script.read_symbol(')');
					} else if (strcmp(SpellShape, "angle") == 0) {
						SpellData->Shape = SHAPE_ANGLE;
						Script.read_symbol('(');
						SpellData->ShapeParam1 = Script.read_number();
						Script.read_symbol(',');
						SpellData->ShapeParam2 = Script.read_number();
						Script.read_symbol(',');
						SpellData->ShapeParam3 = Script.read_number();
						Script.read_symbol(')');
					} else {
						Script.error("unknown spell shape");
					}
				}

				Script.read_symbol('I');

				// NOTE(fusion): Spell impact.
				{
					const char *SpellImpact = Script.read_identifier();
					if (strcmp(SpellImpact, "damage") == 0) {
						SpellData->Impact = IMPACT_DAMAGE;
						Script.read_symbol('(');
						SpellData->ImpactParam1 = Script.read_number();
						Script.read_symbol(',');
						SpellData->ImpactParam2 = Script.read_number();
						Script.read_symbol(',');
						SpellData->ImpactParam3 = Script.read_number();
						Script.read_symbol(')');
					} else if (strcmp(SpellImpact, "field") == 0) {
						SpellData->Impact = IMPACT_FIELD;
						Script.read_symbol('(');
						SpellData->ImpactParam1 = Script.read_number();
						Script.read_symbol(')');
					} else if (strcmp(SpellImpact, "healing") == 0) {
						SpellData->Impact = IMPACT_HEALING;
						Script.read_symbol('(');
						SpellData->ImpactParam1 = Script.read_number();
						Script.read_symbol(',');
						SpellData->ImpactParam2 = Script.read_number();
						Script.read_symbol(')');
					} else if (strcmp(SpellImpact, "speed") == 0) {
						SpellData->Impact = IMPACT_SPEED;
						Script.read_symbol('(');
						SpellData->ImpactParam1 = Script.read_number();
						Script.read_symbol(',');
						SpellData->ImpactParam2 = Script.read_number();
						Script.read_symbol(',');
						SpellData->ImpactParam3 = Script.read_number();
						Script.read_symbol(')');
					} else if (strcmp(SpellImpact, "drunken") == 0) {
						SpellData->Impact = IMPACT_DRUNKEN;
						Script.read_symbol('(');
						SpellData->ImpactParam1 = Script.read_number();
						Script.read_symbol(',');
						SpellData->ImpactParam2 = Script.read_number();
						Script.read_symbol(',');
						SpellData->ImpactParam3 = Script.read_number();
						Script.read_symbol(')');
					} else if (strcmp(SpellImpact, "strength") == 0) {
						SpellData->Impact = IMPACT_STRENGTH;
						Script.read_symbol('(');
						SpellData->ImpactParam1 = Script.read_number();
						Script.read_symbol(',');
						SpellData->ImpactParam2 = Script.read_number();
						Script.read_symbol(',');
						SpellData->ImpactParam3 = Script.read_number();
						Script.read_symbol(',');
						SpellData->ImpactParam4 = Script.read_number();
						Script.read_symbol(')');
					} else if (strcmp(SpellImpact, "outfit") == 0) {
						SpellData->Impact = IMPACT_OUTFIT;
						Script.read_symbol('(');
						TOutfit Outfit = read_outfit(&Script);
						SpellData->ImpactParam1 = Outfit.OutfitID;
						SpellData->ImpactParam2 = Outfit.ObjectType;
						Script.read_symbol(',');
						SpellData->ImpactParam3 = Script.read_number();
						Script.read_symbol(')');
					} else if (strcmp(SpellImpact, "summon") == 0) {
						SpellData->Impact = IMPACT_SUMMON;
						Script.read_symbol('(');
						SpellData->ImpactParam1 = Script.read_number();
						Script.read_symbol(',');
						SpellData->ImpactParam2 = Script.read_number();
						Script.read_symbol(')');
					} else {
						Script.error("unknown spell impact");
					}
				}

				Script.read_symbol(':');

				// NOTE(fusion): Spell delay.
				{
					SpellData->Delay = Script.read_number();
					if (SpellData->Delay == 0) {
						Script.error("zero spell delay");
					}
				}
			} while (Script.read_special() != '}');
		} else {
			Script.error("unknown race property");
		}
	}
}

void load_races(void) {
	// TODO(fusion): It is possible to leak `MonsterDir` if `load_race` throws on
	// errors (which it does). This is usually not a problem because failing here
	// means we're cascading back to `InitAll` which will report the error and
	// exit, but it is something to keep in mind for any functions that uses
	// exceptions with non-RAII resources.

	DIR *MonsterDir = opendir(MONSTERPATH);
	if (MonsterDir == NULL) {
		error("load_races: Subdirectory %s not found\n", MONSTERPATH);
		throw "Cannot load races";
	}

	char FileName[4096];
	while (dirent *DirEntry = readdir(MonsterDir)) {
		if (DirEntry->d_type != DT_REG) {
			continue;
		}

		const char *FileExt = findLast(DirEntry->d_name, '.');
		if (FileExt == NULL || strcmp(FileExt, ".mon") != 0) {
			continue;
		}

		snprintf(FileName, sizeof(FileName), "%s/%s", MONSTERPATH, DirEntry->d_name);
		load_race(FileName);
	}

	closedir(MonsterDir);
}
