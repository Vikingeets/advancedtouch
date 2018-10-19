
#include <cassert>
#include <algorithm>
#include <iostream>
#include <map>
#include "craft.h"
#include "levels.h"

using namespace std;
using actionResult = craft::actionResult;

// chance == 70 means 70% success and so on
inline bool craft::rollPercent(int chance, bool canUseSteadyHand) const
{
	if (canUseSteadyHand)
	{
		if (steadyHandTime > 0) chance += 20;
		else if (steadyHand2Time > 0) chance += 30;
	}

	if (chance >= 100) return true;
	else return rng.generateInt(99) < chance;
}

void craft::calculateFactors()
{
	int unmodifiedLevelDifference = crafter.rLevel - recipe.rLevel;
	int levelDifference = unmodifiedLevelDifference;

	if (ingenuityTime > 0)
	{
		if (levelDifference < 0 && recipe.rLevel >= 390)
		{
			int levelCap = abs(unmodifiedLevelDifference) <= 100 ? -5 : -20;
			levelDifference = max(levelDifference + recipe.rLevel / 8, levelCap);
		}
		else
		{
			if (recipe.rLevel >= 390)
				levelDifference = (2 * recipe.rLevel) / 43;
			else
			{
				if (recipe.rLevel == 290)
					levelDifference += 10;
				else if (recipe.rLevel == 300)
					levelDifference += 9;
				else if (recipe.rLevel >= 120)
					levelDifference += 11;
				else
					levelDifference += 5;
			}
			levelDifference = max(levelDifference, -1 * getStars(recipe.rLevel));
		}
	}
	else if(ingenuity2Time > 0)
	{
		if (levelDifference < 0 && recipe.rLevel >= 390)
		{
			int levelCap = -20;
			if (abs(unmodifiedLevelDifference) <= 100)
				levelCap = -4;
			else if (abs(unmodifiedLevelDifference) < 110)
				levelCap = -9;
			levelDifference = max(levelDifference + recipe.rLevel / 7, levelCap);
		}
		else
		{
			if (recipe.rLevel >= 390)
				levelDifference = recipe.rLevel / 21;
			else
			{
				if (recipe.rLevel == 290)
					levelDifference += 11;
				else if (recipe.rLevel == 300)
					levelDifference += 10;
				else if (recipe.rLevel >= 120)
					levelDifference += 12;
				else
					levelDifference += 6;
			}
			levelDifference = max(levelDifference, -1 * (getStars(recipe.rLevel)) - 1);
		}
	}

	progressFactor = getProgressFactor(levelDifference);
	qualityFactor = getQualityFactor(levelDifference);
}

void craft::increaseProgress(int efficiency)
{
	if (whistleStacks > 0 && whistleStacks % 3 == 0)
		efficiency += efficiency / 2;

	float baseProgress = (crafter.craftsmanship * 21.f) / 100.f + 2.f;
	float suggestionMod = (crafter.craftsmanship + 10000.f) / (recipe.suggestedCraftsmanship + 10000.f);
	float differenceMod = progressFactor / 100.f;

	int progressIncrease = static_cast<int>(baseProgress * suggestionMod * differenceMod);

	progress += (progressIncrease * efficiency) / 100;

	return;
}

void craft::increaseQuality(int efficiency)
{
	if (greatStridesTime > 0)
	{
		efficiency *= 2;
		greatStridesTime = 0;
	}

	int control = crafter.control + min(
		(max(0, innerQuietStacks - 1) * crafter.control) / 5
		+
		(innovationTime > 0 ? crafter.control / 2 : 0),
		3000);
	
	float baseQuality = (control * 35.f) / 100.f + 35.f;
	float suggestionMod = (control + 10000.f) / (recipe.suggestedControl + 10000.f);
	float differenceMod = qualityFactor / 100.f;
	float conditionMod;

	switch (cond)
	{
	case condition::poor:
		conditionMod = 0.5f;
		break;
	case condition::normal:
		conditionMod = 1.0f;
		break;
	case condition::good:
		conditionMod = 1.5f;
		break;
	case condition::excellent:
		conditionMod = 4.f;
		break;
	}

	int qualityIncrease = static_cast<int>(baseQuality * suggestionMod * differenceMod * conditionMod);

	quality += (qualityIncrease * efficiency) / 100;

	if (innerQuietStacks > 0 && innerQuietStacks < 11) innerQuietStacks++;

	return;
}

// Negative amount to deduct, positive to add. Returns false if not enough CP
bool craft::changeCP(int amount)
{
	if (amount >= 0)
	{
		CP = min(CP + amount, crafter.CP + (strokeOfGeniusActive ? 15 : 0));
		return true;
	}

	if (-amount > CP) return false;

	// https://redd.it/7s4ilp
	// the reddit post says it's a 20% chance to proc, but my own experimentation came out to 30%
	if (initialPreparationsActive && rollPercent(30, false))
		amount -= (amount * 3) / 10;

	CP += amount;
	return true;
}

craft::condition craft::getNextCondition(condition current)
{
	if (normalLock) return condition::normal;
	
	switch (current)
	{
	case condition::poor:
		return condition::normal;
	case condition::normal:
		break;
	case condition::good:
	case condition::excellent:
		if (whistleStacks > 0)
		{
			whistleStacks--;
			if (whistleStacks == 0) finishingTouches();
		}
		return cond == condition::good ? condition::normal : condition::poor;
	}

	bool qualityAssurance = crafter.level >= 63;
	int excellentChance, goodChance;
	
	if (recipe.rLevel >= 430)		// 80*+: 89/10/1
	{
		excellentChance = 1;
		goodChance = qualityAssurance ? 11 : 10;
	}
	else if (recipe.rLevel >= 406)	// 75+: 83/15/2
	{
		excellentChance = 2;
		goodChance = qualityAssurance ? 17 : 15;
	}
	else if (recipe.rLevel >= 385)	// 71+: 78/20/2
	{
		excellentChance = 2;
		goodChance = qualityAssurance ? 22 : 20;
	}
	else if (recipe.rLevel >= 300)		// 70*+: 89/10/1
	{
		excellentChance = 1;
		goodChance = qualityAssurance ? 11 : 10;
	}
	else if (recipe.rLevel >= 276)	// 65+: 83/15/2
	{
		excellentChance = 2;
		goodChance = qualityAssurance ? 17 : 15;
	}
	else if (recipe.rLevel >= 255)	// 61+: 78/20/2
	{
		excellentChance = 2;
		goodChance = qualityAssurance ? 22 : 20;
	}
	else if (recipe.rLevel >= 150)	// 60+: 89/10/1
	{
		excellentChance = 1;
		goodChance = qualityAssurance ? 11 : 10;
	}
	else if (recipe.rLevel >= 136)	// 55+: 83/15/2
	{
		excellentChance = 2;
		goodChance = qualityAssurance ? 17 : 15;
	}
	else							// 1+: 73/25/2
	{
		excellentChance = 2;
		goodChance = qualityAssurance ? 27 : 25;
	}

	if (heartTime > 0) goodChance *= 2;

	int roll = rng.generateInt(99);

	if (roll >= 100 - excellentChance) return condition::excellent;
	else if (roll >= 100 - excellentChance - goodChance) return condition::good;
	else return condition::normal;
}

void craft::endStep()
{
	if(comfortZoneTime > 0)
	{
		changeCP(8);
		comfortZoneTime--;
	}
	wasteNotTime--;
	if (manipulationTime > 0)
	{
		changeDurability(10);
		manipulationTime--;
	}
	if(manipulation2Time > 0)
	{
		changeDurability(5);
		manipulation2Time--;
	}
	steadyHandTime--;
	steadyHand2Time--;
	if (ingenuityTime > 0)
	{
		ingenuityTime--;
		if (ingenuityTime == 0) calculateFactors();
	}
	if (ingenuity2Time > 0)
	{ 
		ingenuity2Time--;
		if (ingenuity2Time == 0) calculateFactors();
	}
	greatStridesTime--;
	innovationTime--;
	makersMarkTime--;
	nameOfTheElementsTime--;
	heartTime--;
	observeCombo = false;
	cond = getNextCondition(cond);

	if (step == 1 && crafter.level >= 70 && (crafter.specialist || rollPercent(30, false)))
	{
		strokeOfGeniusActive = true;
		changeCP(15);
	}

	step++;

	return;
}

actionResult craft::brandOfTheElements()
{
	int efficiency = 100;
	if (nameOfTheElementsTime > 0)
		efficiency += 2 * (100 - 100 * progress / recipe.difficulty);

	return commonSynth(-6, efficiency, 90);
}

actionResult craft::nameOfTheElements()
{
	if (nameOfTheElementsUsed) return actionResult::failHardUnavailable;
	if (!changeCP(-15)) return actionResult::failNoCP;

	nameOfTheElementsUsed = true;
	return actionResult::success;
}

void craft::nameOfTheElementsPost()
{
	nameOfTheElementsTime = 5;
}

void craft::finishingTouches()
{
	commonSynth(0, 200, 50);
}

const map<int, int> qualityPercentToHQPercent = {
	{0, 1}, {5, 2}, {9, 3}, {13, 4}, {17, 5}, {21, 6}, {25, 7}, {29, 8},
	{32, 9}, {35, 10}, {38, 11}, {41, 12}, {44, 13}, {47, 14}, {50, 15}, {53, 16},
	{55, 17}, {58, 18}, {61, 19}, {63, 20}, {65, 21}, {66, 22}, {67, 23}, {68, 24},
	{69, 26}, {70, 28}, {71, 31}, {72, 34}, {73, 38}, {74, 42}, {75, 47}, {76, 52},
	{77, 58}, {78, 64}, {79, 68}, {80, 71}, {81, 74}, {82, 76}, {83, 78}, {84, 80},
	{85, 81}, {86, 82}, {87, 83}, {88, 84}, {89, 85}, {90, 86}, {91, 87}, {92, 88},
	{93, 89}, {94, 90}, {95, 91}, {96, 92}, {97, 94}, {98, 96}, {99, 98}, {100, 100}
};

int craft::hqPercentFromQuality(int qualityPercent)
{
	auto it = qualityPercentToHQPercent.upper_bound(qualityPercent);
	if (it == qualityPercentToHQPercent.cbegin())
	{
		assert(false);
		return 1;
	}
	--it;
	return it->second;
}

bool craft::checkBuffsValid() const
{
	return 
//		comfortZoneTime >= 0 &&
		wasteNotTime >= 0 &&
		manipulationTime >= 0 &&
		manipulation2Time >= 0 &&
		!(manipulationTime > 0 && manipulation2Time > 0) &&
		innerQuietStacks >= 0 &&
		innerQuietStacks <= 11 &&
//		steadyHandTime >= 0 &&
//		steadyHand2Time >= 0 &&
		!(steadyHandTime > 0 && steadyHand2Time > 0) &&
		ingenuityTime >= 0 &&
		ingenuity2Time >= 0 &&
		!(ingenuityTime > 0 && ingenuity2Time > 0) &&
//		greatStridesTime >= 0 &&
//		innovationTime >= 0 &&
//		makersMarkTime >= 0 &&
//		nameOfTheElementsTime >= 0 &&
		whistleStacks >= 0;
}

string stateBuffList(const string& name, int variable, bool* anybuffs)
{
	if (variable <= 0) return string();
	*anybuffs = true;
	return name + to_string(variable) + "; ";
}

string craft::getState() const
{
	string output;
	output += "Progress: " + to_string(progress) + '/' + to_string(recipe.difficulty);
	output += ", Quality: " + to_string(quality);
	if(recipe.quality > 0) output += '/' + to_string(recipe.nominalQuality);
	output += ", Durability: " + to_string(durability) + '/' + to_string(recipe.durability);
	output += ", CP: " + to_string(CP) + '/' + to_string(crafter.CP + (strokeOfGeniusActive ? 15 : 0)) + '\n';

	if (!normalLock)
	{
		output += "Condition: ";
		switch (cond)
		{
		case condition::poor:
			output += "Poor, ";
			break;
		case condition::normal:
			output += "Normal, ";
			break;
		case condition::good:
			output += "Good, ";
			break;
		case condition::excellent:
			output += "Excellent, ";
			break;
		}
	}

	output += "Buffs: ";
	bool anybuffs = false;
	output += stateBuffList("Comfort Zone: ", comfortZoneTime, &anybuffs);
	output += stateBuffList("Waste Not: ", wasteNotTime, &anybuffs);
	output += stateBuffList("Manipulation: ", manipulationTime, &anybuffs);
	output += stateBuffList("Manipulation 2: ", manipulation2Time, &anybuffs);
	output += stateBuffList("Inner Quiet: ", innerQuietStacks, &anybuffs);
	output += stateBuffList("Steady Hand: ", steadyHandTime, &anybuffs);
	output += stateBuffList("Steady Hand 2: ", steadyHand2Time, &anybuffs);
	output += stateBuffList("Ingenuity: ", ingenuityTime, &anybuffs);
	output += stateBuffList("Ingenuity 2: ", ingenuity2Time, &anybuffs);
	output += stateBuffList("Great Strides: ", greatStridesTime, &anybuffs);
	output += stateBuffList("Innovation: ", innovationTime, &anybuffs);
	output += stateBuffList("Maker's Mark: ", makersMarkTime, &anybuffs);
	if (initialPreparationsActive)
	{
		anybuffs = true;
		output += "Initial Preparations; ";
	}
	if (reclaimActive)
	{
		anybuffs = true;
		output += "Reclaim; ";
	}
	if (reuseActive)
	{
		anybuffs = true;
		output += "Reuse; ";
	}
	if (strokeOfGeniusActive && step == 1)
	{
		anybuffs = true;
		output += "Stroke of Genius; ";
	}
	output += stateBuffList("Name of Elements: ", nameOfTheElementsTime, &anybuffs);
	output += stateBuffList("Whistle While You Work: ", whistleStacks, &anybuffs);
	if (observeCombo)
	{
		anybuffs = true;
		output += "Observe; ";
	}

	if (!anybuffs) output += "None; ";

	// The string will always end in a "; " so let's truncate that off.
	output.resize(output.size() - 2);
	output += '\n';

	return output;
}

actionResult craft::commonSynth(int cpChange, int efficiency, int successChance, bool doubleDurability)
{
	if (!changeCP(cpChange)) return actionResult::failNoCP;
	actionResult output;

	if (rollPercent(successChance))
	{
		increaseProgress(efficiency);
		output = actionResult::success;
	}
	else output = actionResult::failRNG;

	hitDurability(doubleDurability ? 20 : 10);

	return output;
}

actionResult craft::commonTouch(int cpChange, int efficiency, int successChance, bool doubleDurability)
{
	if (!changeCP(cpChange)) return actionResult::failNoCP;
	actionResult output;

	if (rollPercent(successChance))
	{
		increaseQuality(efficiency);
		output = actionResult::success;
	}
	else output = actionResult::failRNG;

	hitDurability(doubleDurability ? 20 : 10);

	return output;
}

/***
SYNTHESIS ACTIONS
***/

actionResult craft::flawlessSynthesis()
{
	if(makersMarkTime <= 0)
	{
		if (!changeCP(-15)) return actionResult::failNoCP;
		hitDurability();
	}
	actionResult output;
	if (rollPercent(90))
	{
		progress += 40;
		output = actionResult::success;
	}
	else output = actionResult::failRNG;

	return output;
}

actionResult craft::pieceByPiece()
{
	if (!changeCP(-15)) return actionResult::failNoCP;

	actionResult output;
	if (rollPercent(90))
	{
		int remaining = recipe.difficulty - progress;
		progress += min((remaining * 33) / 100, 1000);
		output = actionResult::success;
	}
	else output = actionResult::failRNG;

	hitDurability();
	return output;
}

actionResult craft::delicateSynthesis()
{
	if (!changeCP(-32)) return actionResult::failNoCP;
	
	increaseProgress(100);
	increaseQuality(100);

	hitDurability();
	return actionResult::success;
}

actionResult craft::intensiveSynthesis()
{
	if (cond != condition::good && cond != condition::excellent) return actionResult::failSoftUnavailable;
	return commonSynth(-12, 300, 80);
}

actionResult craft::muscleMemory()
{
	if (step != 1) return actionResult::failHardUnavailable;
	if (!changeCP(-6)) return actionResult::failNoCP;

	progress = min((recipe.difficulty * 33) / 100, 1000);

	hitDurability();
	return actionResult::success;
}

/***
TOUCH ACTIONS
***/

actionResult craft::byregotsBlessing()
{
	if (innerQuietStacks <= 1) return actionResult::failHardUnavailable;
	actionResult output = commonTouch(-24, 100 + 20 * (innerQuietStacks - 1), 90);
	if (output != actionResult::failNoCP) innerQuietStacks = 0;
	return output;
}

actionResult craft::preciseTouch()
{
	if (cond != condition::good && cond != condition::excellent) return actionResult::failSoftUnavailable;
	actionResult output = commonTouch(-18, 100, 70);
	if (output == actionResult::success && innerQuietStacks >= 1 && innerQuietStacks < 11) innerQuietStacks++;
	return output;
}

actionResult craft::patientTouch()
{
	actionResult output = commonTouch(-6, 100, 50);
	if(output == actionResult::success && innerQuietStacks > 0 && innerQuietStacks < 11) innerQuietStacks++;
	else if(output == actionResult::failRNG) innerQuietStacks -= innerQuietStacks / 2;

	return output;
}

actionResult craft::prudentTouch()
{
	if (wasteNotTime > 0) return actionResult::failHardUnavailable;
	if (!changeCP(-21)) return actionResult::failNoCP;
	actionResult output;
	if (rollPercent(70))
	{
		increaseQuality(100);
		output = actionResult::success;
	}
	else output = actionResult::failRNG;

	changeDurability(-5);
	return output;
}

actionResult craft::preparatoryTouch()
{
	actionResult output = commonTouch(-36, 200, 70, true);
	if (output == actionResult::success && innerQuietStacks > 0 && innerQuietStacks < 11) innerQuietStacks++;
	return output;
}

actionResult craft::trainedEye()
{
	if (step != 1 || crafter.level < rlvlToMain(recipe.rLevel) + 10) return actionResult::failHardUnavailable;
	if (!changeCP(-250)) return actionResult::failNoCP;
	quality = recipe.nominalQuality / 2;
	return actionResult::success;
}

actionResult craft::trainedInstinct()
{
	if (step != 1 || crafter.level < rlvlToMain(recipe.rLevel) + 10) return actionResult::failHardUnavailable;
	if (!changeCP(-250)) return actionResult::failNoCP;
	quality = (rng.generateInt(30, 100) * recipe.nominalQuality) / 100;
	return actionResult::success;
}

/***
CP
***/

actionResult craft::comfortZone()
{
	if (!changeCP(-66)) return actionResult::failNoCP;

	// An existing CZ buff won't give the 8 CP on a step casting the new one
	comfortZoneTime = 0;

	return actionResult::success;
}

void craft::comfortZonePost()
{
	comfortZoneTime = 10;
}

actionResult craft::rumination()
{
	if (innerQuietStacks <= 1) return actionResult::failHardUnavailable;

//	const int& iQBuffs = innerQuietStacks - 1;
//	changeCP((21 * iQBuffs - iQBuffs * iQBuffs + 10) / 2);
	changeCP((23 * innerQuietStacks - innerQuietStacks * innerQuietStacks - 12) / 2);

	innerQuietStacks = 0;

	return actionResult::success;
}

actionResult craft::tricksOfTheTrade()
{
	if (cond != condition::good && cond != condition::excellent) return actionResult::failSoftUnavailable;

	changeCP(20);

	return actionResult::success;
}

/***
DURABILITY
***/

actionResult craft::mastersMend()
{
	if (!changeCP(-92)) return actionResult::failNoCP;

	changeDurability(30);

	return actionResult::success;
}

actionResult craft::mastersMend2()
{
	if (!changeCP(-160)) return actionResult::failNoCP;

	changeDurability(60);

	return actionResult::success;
}

actionResult craft::wasteNot()
{
	if (!changeCP(-56)) return actionResult::failNoCP;

	return actionResult::success;
}

void craft::wasteNotPost()
{
	wasteNotTime = 4;
}

actionResult craft::wasteNot2()
{
	if (!changeCP(-98)) return actionResult::failNoCP;

	return actionResult::success;
}

void craft::wasteNot2Post()
{
	wasteNotTime = 8;
}

actionResult craft::manipulation()
{
	if (!changeCP(-88)) return actionResult::failNoCP;

	manipulationTime = manipulation2Time = 0;

	return actionResult::success;
}

void craft::manipulationPost()
{
	manipulationTime = 3;
}

actionResult craft::manipulation2()
{
	if (!changeCP(-96)) return actionResult::failNoCP;

	manipulationTime = manipulation2Time = 0;

	return actionResult::success;
}

void craft::manipulation2Post()
{
	manipulation2Time = 8;
}

/***
BUFFS
***/

actionResult craft::innerQuiet()
{
	if (innerQuietStacks > 0) return actionResult::failHardUnavailable;
	if (!changeCP(-18)) return actionResult::failNoCP;

	innerQuietStacks = 1;
	return actionResult::success;
}

actionResult craft::steadyHand()
{
	if (!changeCP(-22)) return actionResult::failNoCP;

	steadyHandTime = steadyHand2Time = 0;
	return actionResult::success;
}

void craft::steadyHandPost()
{
	steadyHandTime = 5;
}

actionResult craft::steadyHand2()
{
	if (!changeCP(-25)) return actionResult::failNoCP;

	steadyHandTime = steadyHand2Time = 0;
	return actionResult::success;
}

void craft::steadyHand2Post()
{
	steadyHand2Time = 5;
}

actionResult craft::ingenuity()
{
	if (!changeCP(-24)) return actionResult::failNoCP;
	
	ingenuityTime = ingenuity2Time = 0;
	return actionResult::success;
}

void craft::ingenuityPost()
{
	ingenuityTime = 5;
	calculateFactors();
}

actionResult craft::ingenuity2()
{
	if (!changeCP(-32)) return actionResult::failNoCP;

	ingenuityTime = ingenuity2Time = 0;
	return actionResult::success;
}

void craft::ingenuity2Post()
{
	ingenuity2Time = 5;
	calculateFactors();
}

actionResult craft::greatStrides()
{
	if (!changeCP(-32)) return actionResult::failNoCP;

	return actionResult::success;
}

void craft::greatStridesPost()
{
	greatStridesTime = 3;
}

actionResult craft::innovation()
{
	if (!changeCP(-18)) return actionResult::failNoCP;

	return actionResult::success;
}

void craft::innovationPost()
{
	innovationTime = 3;
}

actionResult craft::makersMark()
{
	if (step != 1) return actionResult::failHardUnavailable;
	if (!changeCP(-20)) return actionResult::failNoCP;

	return actionResult::success;
}

void craft::makersMarkPost()
{
	makersMarkTime = min(recipe.difficulty / 100 + 1, 25);
}

actionResult craft::initialPreparations()
{
	if (step != 1) return actionResult::failHardUnavailable;
	if (!changeCP(-50)) return actionResult::failNoCP;

	initialPreparationsActive = true;
	return actionResult::success;
}

/***
SPECIALITY ACTIONS
***/

actionResult craft::whistle()
{
	if (!crafter.specialist || whistleStacks > 0) return actionResult::failHardUnavailable;
	if (!changeCP(-36)) return actionResult::failNoCP;

	whistleStacks = 11;	// before the step ends this time because it can trigger on its own step
	return actionResult::success;
}

actionResult craft::satisfaction()
{
	if (!crafter.specialist) return actionResult::failHardUnavailable;
	if (whistleStacks <= 0 || whistleStacks % 3 != 0) return actionResult::failSoftUnavailable;

	whistleStacks--;
	changeCP(15);
	return actionResult::success;
}

actionResult craft::innovativeTouch()
{
	if (!crafter.specialist) return actionResult::failHardUnavailable;

	return commonTouch(-8, 100, 40);
}

//innovateTouch() uses innovation()'s post-action

actionResult craft::nymeiasWheel()
{
	if (!crafter.specialist || whistleStacks <= 0) return actionResult::failHardUnavailable;
	if (!changeCP(-18)) return actionResult::failNoCP;

	if (whistleStacks < 4) changeDurability(30);
	else if (whistleStacks < 9) changeDurability(20);
	else changeDurability(10);

	whistleStacks = 0;	// Does not trigger Finishing Touches
	return actionResult::success;
}

actionResult craft::byregotsMiracle()
{
	if (!crafter.specialist || innerQuietStacks <= 1) return actionResult::failHardUnavailable;
	actionResult output = commonTouch(-10, 100 + 15 * (innerQuietStacks - 1), 70);
	
	if (output != actionResult::failNoCP) innerQuietStacks -= innerQuietStacks / 2;
	return output;
}

actionResult craft::trainedHand()
{
	if (!crafter.specialist || innerQuietStacks <= 0 || whistleStacks <= 0) return actionResult::failHardUnavailable;
	if (innerQuietStacks != whistleStacks) return actionResult::failSoftUnavailable;
	if (!changeCP(-16)) return actionResult::failNoCP;

	increaseProgress(150);
	increaseQuality(150);

	hitDurability();
	return actionResult::success;
}

actionResult craft::heartOfTheSpecialist()
{
	if (!crafter.specialist || heartUses > 5) return actionResult::failHardUnavailable;
	if (!changeCP(-45)) return actionResult::failNoCP;

	heartUses++;
	return actionResult::success;
}

void craft::heartOfTheSpecialistPost()
{
	heartTime = 7;
}

actionResult craft::specialtyReinforce()
{
	if (!crafter.specialist || !initialPreparationsActive) return actionResult::failHardUnavailable;

	changeDurability(25);
	initialPreparationsActive = false;

	return actionResult::success;
}

actionResult craft::specialtyRefurbish()
{
	if (!crafter.specialist || !initialPreparationsActive) return actionResult::failHardUnavailable;

	changeCP(65);
	initialPreparationsActive = false;

	return actionResult::success;
}

actionResult craft::specialtyReflect()
{
	if (!crafter.specialist || !initialPreparationsActive || innerQuietStacks < 1) return actionResult::failHardUnavailable;

	innerQuietStacks = min(innerQuietStacks + 3, 11);
	initialPreparationsActive = false;

	return actionResult::success;
}

actionResult craft::observe()
{
	if (!changeCP(-7)) return actionResult::failNoCP;
	else return actionResult::success;
}

void craft::observePost()
{
	observeCombo = true;
}

actionResult craft::reclaim()
{
	if (reclaimActive) return actionResult::failHardUnavailable;
	if (!changeCP(-50)) return actionResult::failNoCP;

	reclaimActive = true;

	return actionResult::success;
}

actionResult craft::reuse()
{
	if (reuseActive || quality < recipe.nominalQuality) return actionResult::failHardUnavailable;
	if (rollPercent(75, false)) return actionResult::failSoftUnavailable;
	if (!changeCP(-60)) return actionResult::failNoCP;

	reuseActive = true;

	return actionResult::success;
}


actionResult craft::performOne(actions action)
{
	switch (action)
	{
	case actions::basicSynth: return basicSynth();
	case actions::standardSynthesis: return standardSynthesis();
	case actions::flawlessSynthesis: return flawlessSynthesis();
	case actions::carefulSynthesis: return carefulSynthesis();
	case actions::carefulSynthesis2: return carefulSynthesis2();
	case actions::carefulSynthesis3: return carefulSynthesis3();
	case actions::pieceByPiece: return pieceByPiece();
	case actions::rapidSynthesis: return rapidSynthesis();
	case actions::rapidSynthesis2: return rapidSynthesis2();
	case actions::rapidSynthesis3: return rapidSynthesis3();
	case actions::focusedSynthesis: return focusedSynthesis();
	case actions::delicateSynthesis: return delicateSynthesis();
	case actions::intensiveSynthesis: return intensiveSynthesis();
	case actions::muscleMemory: return muscleMemory();
	case actions::brandOfTheElements: return brandOfTheElements();
	
	case actions::basicTouch: return basicTouch();
	case actions::standardTouch: return standardTouch();
	case actions::advancedTouch: return advancedTouch();
	case actions::hastyTouch: return hastyTouch();
	case actions::hastyTouch2: return hastyTouch2();
	case actions::byregotsBlessing: return byregotsBlessing();
	case actions::preciseTouch: return preciseTouch();
	case actions::focusedTouch: return focusedTouch();
	case actions::patientTouch: return patientTouch();
	case actions::prudentTouch: return prudentTouch();
	case actions::preparatoryTouch: return preparatoryTouch();
	case actions::trainedEye: return trainedEye();
	case actions::trainedInstinct: return trainedInstinct();

	case actions::comfortZone: return comfortZone();
	case actions::rumination: return rumination();
	case actions::tricksOfTheTrade: return tricksOfTheTrade();

	case actions::mastersMend: return mastersMend();
	case actions::mastersMend2: return mastersMend2();
	case actions::wasteNot: return wasteNot();
	case actions::wasteNot2: return wasteNot2();
	case actions::manipulation: return manipulation();
	case actions::manipulation2: return manipulation2();

	case actions::innerQuiet: return innerQuiet();
	case actions::steadyHand: return steadyHand();
	case actions::steadyHand2: return steadyHand2();
	case actions::ingenuity: return ingenuity();
	case actions::ingenuity2: return ingenuity2();
	case actions::greatStrides: return greatStrides();
	case actions::innovation: return innovation();
	case actions::makersMark: return makersMark();
	case actions::initialPreparations: return initialPreparations();
	case actions::nameOfTheElements: return nameOfTheElements();

	case actions::whistle: return whistle();
	case actions::satisfaction: return satisfaction();
	case actions::innovativeTouch: return innovativeTouch();
	case actions::nymeiasWheel: return nymeiasWheel();
	case actions::byregotsMiracle: return byregotsMiracle();
	case actions::trainedHand: return trainedHand();
	case actions::heartOfTheSpecialist: return heartOfTheSpecialist();
	case actions::specialtyReinforce: return specialtyReinforce();
	case actions::specialtyRefurbish: return specialtyRefurbish();
	case actions::specialtyReflect: return specialtyReflect();

	case actions::observe: return observe();
	case actions::reclaim: return reclaim();
	case actions::reuse: return reuse();
	default:
		assert(false);
		return actionResult::failHardUnavailable;
	}
}

void craft::performOnePost(actions action)
{
	switch (action)
	{
	case actions::comfortZone: return comfortZonePost();
	case actions::wasteNot: return wasteNotPost();
	case actions::wasteNot2: return wasteNot2Post();
	case actions::manipulation: return manipulationPost();
	case actions::manipulation2: return manipulation2Post();
	case actions::steadyHand: return steadyHandPost();
	case actions::steadyHand2: return steadyHand2Post();
	case actions::ingenuity: return ingenuityPost();
	case actions::ingenuity2: return ingenuity2Post();
	case actions::greatStrides: return greatStridesPost();
	case actions::innovation:
	case actions::innovativeTouch:
		return innovationPost();
	case actions::makersMark: return makersMarkPost();
	case actions::nameOfTheElements:
		return nameOfTheElementsPost();
	case actions::observe: return observePost();
	case actions::heartOfTheSpecialist: return heartOfTheSpecialistPost();
	default: return;
	}
}

craft::endResult craft::performAll(const craft::sequenceType& sequence, goalType goal, int progressWiggle, bool echoEach)
{
	endResult craftResult;
	craftResult.invalidActions = 0;

	craft::sequenceType::const_iterator it;
	for (it = sequence.cbegin(); it != sequence.cend(); ++it)
	{
		if (echoEach)
		{
			cout << getState() << '\n';
			cout << "Step " << step << ", ";
		}
		actionResult result = performOne(*it);
		if (echoEach)
		{
			cout << "Performing " << simpleText.at(*it) << ": ";
			switch (result)
			{
			case actionResult::success:
				cout << "success\n";
				break;
			case actionResult::failRNG:
				cout << "failed roll\n";
				break;
			case actionResult::failNoCP:
				cout << "not enough CP\n";
				break;
			case actionResult::failHardUnavailable:
			case actionResult::failSoftUnavailable:
				cout << "not available\n";
				break;
			}
			cout << '\n';
		}
		//		assert(checkBuffsValid()); This profiles somewhat high, so disable it unless something comes up
		if (result == actionResult::failNoCP || result == actionResult::failHardUnavailable)
			craftResult.invalidActions++;
		if (result != actionResult::success && result != actionResult::failRNG)
			continue;

		if (durability <= 0)
			break;
		endStep();
		if (result == actionResult::success) performOnePost(*it);

		if (progress + progressWiggle >= recipe.difficulty)
		{
			step--;	// to undo the step increase in endStep()
			break;
		}
	}

	if (progress - progressWiggle < recipe.difficulty)	// The sim says we hit the goal, but it might actually be under
		progress -= progressWiggle;	// so make it fail

	if (echoEach)
	{
		cout << getState() << '\n';
	}

	craftResult.progress = progress;
	craftResult.quality = quality;
	switch (goal)
	{
	case goalType::quality:
		craftResult.hqPercent = hqPercentFromQuality((quality * 100) / recipe.quality);
		break;
	case goalType::maxQuality:
		break;
	case goalType::collectability:
		craftResult.collectableHit = quality >= recipe.quality;
		break;
	}
	craftResult.durability = durability;
	craftResult.steps = step;
	if (it != sequence.cend()) ++it;	// the iterator needs to sit on the one after the last craft in order for the next calculation to work
	craftResult.invalidActions += static_cast<int>(distance(it, sequence.cend()));	// Count everything that didn't happen post-macro
	craftResult.reuseUsed = reuseActive;

	return craftResult;
}
