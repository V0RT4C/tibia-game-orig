#include "cr.h"
#include "config.h"
#include "enums.h"
#include "info.h"
#include "writer.h"

#include <dirent.h>

// Race
// =============================================================================
TRaceData::TRaceData(void) :
		Skill(1, 5, 5),
		Talk(1, 5, 5),
		Item(1, 5, 5),
		Spell(1, 5, 5)
{
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

bool IsRaceValid(int Race){
	return Race >= 1 && Race < MAX_RACES;
}

int GetRaceByName(const char *RaceName){
	int Result = 0;
	for(int Race = 1; Race < MAX_RACES; Race += 1){
		if(stricmp(RaceName, RaceData[Race].Name) == 0){
			Result = Race;
			break;
		}
	}
	return Result;
}

const char *GetRaceName(int Race){
	if(!IsRaceValid(Race)){
		error("GetRaceName: Invalid race number %d.\n", Race);
		return NULL;
	}

	return RaceData[Race].Name;
}

TOutfit GetRaceOutfit(int Race){
	if(!IsRaceValid(Race)){
		error("GetRaceOutfit: Invalid race number %d.\n", Race);
		return RaceData[1].Outfit;
	}

	return RaceData[Race].Outfit;
}

bool GetRaceNoSummon(int Race){
	if(!IsRaceValid(Race)){
		error("GetRaceNoSummon: Invalid race number %d.\n", Race);
		return true;
	}

	return RaceData[Race].NoSummon;
}

bool GetRaceNoConvince(int Race){
	if(!IsRaceValid(Race)){
		error("GetRaceNoConvince: Invalid race number %d.\n", Race);
		return true;
	}

	return RaceData[Race].NoConvince;
}

bool GetRaceNoIllusion(int Race){
	if(!IsRaceValid(Race)){
		error("GetRaceNoIllusion: Invalid race number %d.\n", Race);
		return true;
	}

	return RaceData[Race].NoIllusion;
}

bool GetRaceNoParalyze(int Race){
	if(!IsRaceValid(Race)){
		error("GetRaceNoParalyze: Invalid race number %d.\n", Race);
		return true;
	}

	return RaceData[Race].NoParalyze;
}

int GetRaceSummonCost(int Race){
	if(!IsRaceValid(Race)){
		error("GetRaceSummonCost: Invalid race number %d.\n", Race);
		return 0;
	}

	return RaceData[Race].SummonCost;
}

int GetRacePoison(int Race){
	if(!IsRaceValid(Race)){
		error("GetRacePoison: Invalid race number %d.\n", Race);
		return 0;
	}

	return RaceData[Race].Poison;
}

bool GetRaceUnpushable(int Race){
	if(!IsRaceValid(Race)){
		error("GetRaceUnpushable: Invalid race number %d.\n", Race);
		return true;
	}

	return RaceData[Race].Unpushable;
}

// TODO(fusion): Probably move this somewhere else?
TOutfit ReadOutfit(ReadScriptFile *Script){
	TOutfit Outfit = {};
	Script->read_symbol('(');
	Outfit.OutfitID = Script->read_number();
	Script->read_symbol(',');
	if(Outfit.OutfitID == 0){
		Outfit.ObjectType = Script->read_number();
	}else{
		memcpy(Outfit.Colors, Script->read_bytesequence(), sizeof(Outfit.Colors));
	}
	Script->read_symbol(')');
	return Outfit;
}

// TODO(fusion): Probably move this somewhere else?
void WriteOutfit(WriteScriptFile *Script, TOutfit Outfit){
	Script->write_text("(");
	Script->write_number(Outfit.OutfitID);
	Script->write_text(",");
	if(Outfit.OutfitID == 0){
		Script->write_number(Outfit.ObjectType);
	}else{
		Script->write_bytesequence(Outfit.Colors, sizeof(Outfit.Colors));
	}
	Script->write_text(")");
}

void LoadRace(const char *FileName){
	ReadScriptFile Script;

	Script.open(FileName);

	// NOTE(fusion): It seems we expect `RaceNumber` to be the first attribute
	// declared in a race file.
	if(strcmp(Script.read_identifier(), "racenumber") != 0){
		Script.error("race number expected");
	}

	Script.read_symbol('=');

	int RaceNumber = Script.read_number();
	if(!IsRaceValid(RaceNumber)){
		Script.error("illegal race number");
	}

	TRaceData *Race = &RaceData[RaceNumber];
	if(Race->Name[0] != 0){
		Script.error("race already defined");
	}

	Race->Outfit = TOutfit{};
	Race->Outfit.OutfitID = RaceNumber;

	while(true){
		Script.next_token();
		if(Script.Token == ENDOFFILE){
			Script.close();
			break;
		}

		char Identifier[MAX_IDENT_LENGTH];
		strcpy(Identifier, Script.get_identifier());
		Script.read_symbol('=');

		if(strcmp(Identifier, "name") == 0){
			strcpy(Race->Name, Script.read_string());
		}else if(strcmp(Identifier, "article") == 0){
			strcpy(Race->Article, Script.read_string());
		}else if(strcmp(Identifier, "outfit") == 0){
			Race->Outfit = ReadOutfit(&Script);
		}else if(strcmp(Identifier, "corpse") == 0){
			int CorpseTypeID = Script.read_number();
			Race->MaleCorpse = CorpseTypeID;
			Race->FemaleCorpse = CorpseTypeID;
		}else if(strcmp(Identifier, "corpses") == 0){
			Race->MaleCorpse = Script.read_number();
			Script.read_symbol(',');
			Race->FemaleCorpse = Script.read_number();
		}else if(strcmp(Identifier, "blood") == 0){
			const char *Blood = Script.read_identifier();
			if(strcmp(Blood, "blood") == 0){
				Race->Blood = BT_BLOOD;
			}else if(strcmp(Blood, "slime") == 0){
				Race->Blood = BT_SLIME;
			}else if(strcmp(Blood, "bones") == 0){
				Race->Blood = BT_BONES;
			}else if(strcmp(Blood, "fire") == 0){
				Race->Blood = BT_FIRE;
			}else if(strcmp(Blood, "energy") == 0){
				Race->Blood = BT_ENERGY;
			}else{
				Script.error("unknown blood type");
			}
		}else if(strcmp(Identifier, "experience") == 0){
			Race->ExperiencePoints = Script.read_number();
		}else if(strcmp(Identifier, "summoncost") == 0){
			Race->SummonCost = Script.read_number();
		}else if(strcmp(Identifier, "fleethreshold") == 0){
			Race->FleeThreshold = Script.read_number();
		}else if(strcmp(Identifier, "attack") == 0){
			Race->Attack = Script.read_number();
		}else if(strcmp(Identifier, "defend") == 0){
			Race->Defend = Script.read_number();
		}else if(strcmp(Identifier, "armor") == 0){
			Race->Armor = Script.read_number();
		}else if(strcmp(Identifier, "poison") == 0){
			Race->Poison = Script.read_number();
		}else if(strcmp(Identifier, "losetarget") == 0){
			Race->LoseTarget = Script.read_number();
		}else if(strcmp(Identifier, "strategy") == 0){
			Script.read_symbol('(');
			Race->Strategy[0] = Script.read_number();
			Script.read_symbol(',');
			Race->Strategy[1] = Script.read_number();
			Script.read_symbol(',');
			Race->Strategy[2] = Script.read_number();
			Script.read_symbol(',');
			Race->Strategy[3] = Script.read_number();
			Script.read_symbol(')');
		}else if(strcmp(Identifier, "flags") == 0){
			Script.read_symbol('{');
			do{
				const char *Flag = Script.read_identifier();
				if(strcmp(Flag, "kickboxes") == 0){
					Race->KickBoxes = true;
				}else if(strcmp(Flag, "kickcreatures") == 0){
					Race->KickCreatures = true;
				}else if(strcmp(Flag, "seeinvisible") == 0){
					Race->SeeInvisible = true;
				}else if(strcmp(Flag, "unpushable") == 0){
					Race->Unpushable = true;
				}else if(strcmp(Flag, "distancefighting") == 0){
					Race->DistanceFighting = true;
				}else if(strcmp(Flag, "nosummon") == 0){
					Race->NoSummon = true;
				}else if(strcmp(Flag, "noconvince") == 0){
					Race->NoConvince = true;
				}else if(strcmp(Flag, "noillusion") == 0){
					Race->NoIllusion = true;
				}else if(strcmp(Flag, "noburning") == 0){
					Race->NoBurning = true;
				}else if(strcmp(Flag, "nopoison") == 0){
					Race->NoPoison = true;
				}else if(strcmp(Flag, "noenergy") == 0){
					Race->NoEnergy = true;
				}else if(strcmp(Flag, "nohit") == 0){
					Race->NoHit = true;
				}else if(strcmp(Flag, "nolifedrain") == 0){
					Race->NoLifeDrain = true;
				}else if(strcmp(Flag, "noparalyze") == 0){
					Race->NoParalyze = true;
				}else{
					Script.error("unknown flag");
				}
			}while(Script.read_special() != '}');
		}else if(strcmp(Identifier, "skills") == 0){
			Script.read_symbol('{');
			do{
				Script.read_symbol('(');
				int SkillNr = GetSkillByName(Script.read_identifier());
				if(SkillNr == -1){
					Script.error("unknown skill name");
				}

				// NOTE(fusion): Skills are indexed from 1.
				Race->Skills += 1;
				TSkillData *SkillData = Race->Skill.at(Race->Skills);
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
			}while(Script.read_special() != '}');
		}else if(strcmp(Identifier, "talk") == 0){
			Script.read_symbol('{');
			do{
				// NOTE(fusion): Talks are indexed from 1.
				Race->Talks += 1;
				*Race->Talk.at(Race->Talks) = AddDynamicString(Script.read_string());
			}while(Script.read_special() != '}');
		}else if(strcmp(Identifier, "inventory") == 0){
			Script.read_symbol('{');
			do{
				// NOTE(fusion): Items are indexed from 1.
				Race->Items += 1;
				TItemData *ItemData = Race->Item.at(Race->Items);
				Script.read_symbol('(');
				ItemData->Type = Script.read_number();
				Script.read_symbol(',');
				ItemData->Maximum = Script.read_number();
				Script.read_symbol(',');
				ItemData->Probability = Script.read_number();
				Script.read_symbol(')');
			}while(Script.read_special() != '}');
		}else if(strcmp(Identifier, "spells") == 0){
			Script.read_symbol('{');
			do{
				// NOTE(fusion): Spells are indexed from 1.
				Race->Spells += 1;
				TSpellData *SpellData = Race->Spell.at(Race->Spells);

				// NOTE(fusion): Spell shape.
				{
					const char *SpellShape = Script.read_identifier();
					if(strcmp(SpellShape, "actor") == 0){
						SpellData->Shape = SHAPE_ACTOR;
						Script.read_symbol('(');
						SpellData->ShapeParam1 = Script.read_number();
						Script.read_symbol(')');
					}else if(strcmp(SpellShape, "victim") == 0){
						SpellData->Shape = SHAPE_VICTIM;
						Script.read_symbol('(');
						SpellData->ShapeParam1 = Script.read_number();
						Script.read_symbol(',');
						SpellData->ShapeParam2 = Script.read_number();
						Script.read_symbol(',');
						SpellData->ShapeParam3 = Script.read_number();
						Script.read_symbol(')');
					}else if(strcmp(SpellShape, "origin") == 0){
						SpellData->Shape = SHAPE_ORIGIN;
						Script.read_symbol('(');
						SpellData->ShapeParam1 = Script.read_number();
						Script.read_symbol(',');
						SpellData->ShapeParam2 = Script.read_number();
						Script.read_symbol(')');
					}else if(strcmp(SpellShape, "destination") == 0){
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
					}else if(strcmp(SpellShape, "angle") == 0){
						SpellData->Shape = SHAPE_ANGLE;
						Script.read_symbol('(');
						SpellData->ShapeParam1 = Script.read_number();
						Script.read_symbol(',');
						SpellData->ShapeParam2 = Script.read_number();
						Script.read_symbol(',');
						SpellData->ShapeParam3 = Script.read_number();
						Script.read_symbol(')');
					}else{
						Script.error("unknown spell shape");
					}
				}

				Script.read_symbol('I');

				// NOTE(fusion): Spell impact.
				{
					const char *SpellImpact = Script.read_identifier();
					if(strcmp(SpellImpact, "damage") == 0){
						SpellData->Impact = IMPACT_DAMAGE;
						Script.read_symbol('(');
						SpellData->ImpactParam1 = Script.read_number();
						Script.read_symbol(',');
						SpellData->ImpactParam2 = Script.read_number();
						Script.read_symbol(',');
						SpellData->ImpactParam3 = Script.read_number();
						Script.read_symbol(')');
					}else if(strcmp(SpellImpact, "field") == 0){
						SpellData->Impact = IMPACT_FIELD;
						Script.read_symbol('(');
						SpellData->ImpactParam1 = Script.read_number();
						Script.read_symbol(')');
					}else if(strcmp(SpellImpact, "healing") == 0){
						SpellData->Impact = IMPACT_HEALING;
						Script.read_symbol('(');
						SpellData->ImpactParam1 = Script.read_number();
						Script.read_symbol(',');
						SpellData->ImpactParam2 = Script.read_number();
						Script.read_symbol(')');
					}else if(strcmp(SpellImpact, "speed") == 0){
						SpellData->Impact = IMPACT_SPEED;
						Script.read_symbol('(');
						SpellData->ImpactParam1 = Script.read_number();
						Script.read_symbol(',');
						SpellData->ImpactParam2 = Script.read_number();
						Script.read_symbol(',');
						SpellData->ImpactParam3 = Script.read_number();
						Script.read_symbol(')');
					}else if(strcmp(SpellImpact, "drunken") == 0){
						SpellData->Impact = IMPACT_DRUNKEN;
						Script.read_symbol('(');
						SpellData->ImpactParam1 = Script.read_number();
						Script.read_symbol(',');
						SpellData->ImpactParam2 = Script.read_number();
						Script.read_symbol(',');
						SpellData->ImpactParam3 = Script.read_number();
						Script.read_symbol(')');
					}else if(strcmp(SpellImpact, "strength") == 0){
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
					}else if(strcmp(SpellImpact, "outfit") == 0){
						SpellData->Impact = IMPACT_OUTFIT;
						Script.read_symbol('(');
						TOutfit Outfit = ReadOutfit(&Script);
						SpellData->ImpactParam1 = Outfit.OutfitID;
						SpellData->ImpactParam2 = Outfit.ObjectType;
						Script.read_symbol(',');
						SpellData->ImpactParam3 = Script.read_number();
						Script.read_symbol(')');
					}else if(strcmp(SpellImpact, "summon") == 0){
						SpellData->Impact = IMPACT_SUMMON;
						Script.read_symbol('(');
						SpellData->ImpactParam1 = Script.read_number();
						Script.read_symbol(',');
						SpellData->ImpactParam2 = Script.read_number();
						Script.read_symbol(')');
					}else{
						Script.error("unknown spell impact");
					}
				}

				Script.read_symbol(':');

				// NOTE(fusion): Spell delay.
				{
					SpellData->Delay = Script.read_number();
					if(SpellData->Delay == 0){
						Script.error("zero spell delay");
					}
				}
			}while(Script.read_special() != '}');
		}else{
			Script.error("unknown race property");
		}
	}
}

void LoadRaces(void){
	// TODO(fusion): It is possible to leak `MonsterDir` if `LoadRace` throws on
	// errors (which it does). This is usually not a problem because failing here
	// means we're cascading back to `InitAll` which will report the error and
	// exit, but it is something to keep in mind for any functions that uses
	// exceptions with non-RAII resources.

	DIR *MonsterDir = opendir(MONSTERPATH);
	if(MonsterDir == NULL){
		error("LoadRaces: Subdirectory %s not found\n", MONSTERPATH);
		throw "Cannot load races";
	}

	char FileName[4096];
	while(dirent *DirEntry = readdir(MonsterDir)){
		if(DirEntry->d_type != DT_REG){
			continue;
		}

		const char *FileExt = findLast(DirEntry->d_name, '.');
		if(FileExt == NULL || strcmp(FileExt, ".mon") != 0){
			continue;
		}

		snprintf(FileName, sizeof(FileName), "%s/%s", MONSTERPATH, DirEntry->d_name);
		LoadRace(FileName);
	}

	closedir(MonsterDir);
}
