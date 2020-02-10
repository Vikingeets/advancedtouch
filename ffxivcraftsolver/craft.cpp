#include <cassert>
#include <algorithm>
#include <iostream>
#include <map>
#include "craft.h"
#include "levels.h"

using namespace std;
using actionResult = craft::actionResult;

// chance == 70 means 70% success and so on
inline bool craft::rollPercent(int chance) const
{
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
				levelDifference += recipe.rLevel / 18;
			else
			{
				if (recipe.rLevel == 290)
					levelDifference += 10;
				else if (recipe.rLevel == 300)
					levelDifference += 9;
				else if (recipe.rLevel >= 110)
					levelDifference += 40;
				else
					levelDifference += 8;
			}
			levelDifference = max(levelDifference, -1 * getStars(recipe.rLevel));
		}
	}

	progressFactor = getProgressFactor(levelDifference);
	qualityFactor = getQualityFactor(levelDifference);
}

void craft::increaseProgress(int efficiency, bool isBrand)
{
	int buffedEfficiency = efficiency;

	if (muscleMemoryTime > 0)
	{
		buffedEfficiency += efficiency;
		muscleMemoryTime = 0;
	}

	if (innovationTime > 0)
	{
		buffedEfficiency += efficiency / 5;
	}

	if (isBrand && nameOfTheElementsTime > 0)
	{
		buffedEfficiency += min(2 * (100 - 100 * progress / recipe.difficulty), 200);
	}

	float baseProgress = (crafter.craftsmanship * 21.f) / 100.f + 2.f;
	float suggestionMod = (crafter.craftsmanship + 10000.f) / (recipe.suggestedCraftsmanship + 10000.f);
	float differenceMod = progressFactor / 100.f;

	int progressIncrease = static_cast<int>(baseProgress * suggestionMod * differenceMod);

	progress += (progressIncrease * buffedEfficiency) / 100;

	if (finalAppraisalTime > 0 && progress >= recipe.difficulty)
	{
		progress = recipe.difficulty - 1;
		finalAppraisalTime = 0;
	}

	return;
}

void craft::increaseQuality(int efficiency)
{
	int buffedEfficiency = efficiency;

	if (greatStridesTime > 0)
	{
		buffedEfficiency += efficiency;
		greatStridesTime = 0;
	}

	if (innovationTime > 0)
	{
		buffedEfficiency += efficiency / 5;
	}

	int control = crafter.control + min((max(0, innerQuietStacks - 1) * crafter.control) / 5, 3000);
	
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

	quality += (qualityIncrease * buffedEfficiency) / 100;

	if (innerQuietStacks > 0 && innerQuietStacks < 11) innerQuietStacks++;

	return;
}

// Negative amount to deduct, positive to add. Returns false if not enough CP
bool craft::changeCP(int amount)
{
	if (amount >= 0)
	{
		CP = min(CP + amount, crafter.CP);
		return true;
	}

	if (-amount > CP) return false;

	CP += amount;
	return true;
}

craft::condition craft::getNextCondition(condition current)
{
	if (normalLock) return condition::normal;
	
	switch (current)
	{
	case condition::poor:
	case condition::good:
		return condition::normal;
	case condition::normal:
		break;
	case condition::excellent:
		return condition::poor;
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

	int roll = rng.generateInt(99);

	if (roll >= 100 - excellentChance) return condition::excellent;
	else if (roll >= 100 - excellentChance - goodChance) return condition::good;
	else return condition::normal;
}

void craft::endStep()
{
	muscleMemoryTime--;
	wasteNotTime--;
	wasteNot2Time--;
	if(manipulationTime > 0)
	{
		changeDurability(5);
		manipulationTime--;
	}
	if (ingenuityTime > 0)
	{
		ingenuityTime--;
		if (ingenuityTime == 0) calculateFactors();
	}
	greatStridesTime--;
	innovationTime--;
	nameOfTheElementsTime--;
	finalAppraisalTime--;
	observeCombo = false;
	cond = getNextCondition(cond);

	step++;

	return;
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
	output += ", CP: " + to_string(CP) + '/' + to_string(crafter.CP) + '\n';

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
	output += stateBuffList("Final Appraisal: ", finalAppraisalTime, &anybuffs);
	output += stateBuffList("Waste Not: ", wasteNotTime, &anybuffs);
	output += stateBuffList("Waste Not 2: ", wasteNot2Time, &anybuffs);
	output += stateBuffList("Manipulation: ", manipulationTime, &anybuffs);
	output += stateBuffList("Inner Quiet: ", innerQuietStacks, &anybuffs);
	output += stateBuffList("Ingenuity: ", ingenuityTime, &anybuffs);
	output += stateBuffList("Great Strides: ", greatStridesTime, &anybuffs);
	output += stateBuffList("Innovation: ", innovationTime, &anybuffs);
	if (reuseActive)
	{
		anybuffs = true;
		output += "Reuse; ";
	}
	output += stateBuffList("Name of Elements: ", nameOfTheElementsTime, &anybuffs);
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
	if (step == 1) return actionResult::failHardUnavailable;
	if (cond != condition::good && cond != condition::excellent) return actionResult::failSoftUnavailable;
	return commonSynth(-6, 300, 100);
}

actionResult craft::muscleMemory()
{
	if (step != 1) return actionResult::failHardUnavailable;

	return commonSynth(-6, 300, 100);
}

void craft::muscleMemoryPost()
{
	muscleMemoryTime = 5;
}

actionResult craft::brandOfTheElements()
{
	if (!changeCP(-6)) return actionResult::failNoCP;

	increaseProgress(100, true);

	hitDurability();

	return actionResult::success;
}

/***
TOUCH ACTIONS
***/

actionResult craft::byregotsBlessing()
{
	if (innerQuietStacks <= 1) return actionResult::failHardUnavailable;
	actionResult output = commonTouch(-24, 100 + 20 * (innerQuietStacks - 1), 100);
	if (output != actionResult::failNoCP) innerQuietStacks = 0;
	return output;
}

actionResult craft::preciseTouch()
{
	if (step == 1) return actionResult::failHardUnavailable;
	if (cond != condition::good && cond != condition::excellent) return actionResult::failSoftUnavailable;
	actionResult output = commonTouch(-18, 150, 100);
	if (output == actionResult::success && innerQuietStacks >= 1 && innerQuietStacks < 11) innerQuietStacks++;
	return output;
}

actionResult craft::patientTouch()
{
	actionResult output = commonTouch(-6, 100, 50);
	if(output == actionResult::success && innerQuietStacks > 0 && innerQuietStacks < 11)
		innerQuietStacks = min((innerQuietStacks - 1) * 2, 11);
	else if(output == actionResult::failRNG) innerQuietStacks -= innerQuietStacks / 2;

	return output;
}

actionResult craft::prudentTouch()
{
	if (wasteNotTime > 0 || wasteNot2Time > 0) return actionResult::failHardUnavailable;
	if (!changeCP(-25)) return actionResult::failNoCP;

	increaseQuality(100);
	changeDurability(-5);
	return actionResult::success;
}

actionResult craft::preparatoryTouch()
{
	actionResult output = commonTouch(-40, 200, 100, true);
	if (output == actionResult::success && innerQuietStacks > 0 && innerQuietStacks < 11) innerQuietStacks++;
	return output;
}

actionResult craft::trainedEye()
{
	if (step != 1 || crafter.level < rlvlToMain(recipe.rLevel) + 10) return actionResult::failHardUnavailable;
	if (!changeCP(-250)) return actionResult::failNoCP;
	quality += recipe.nominalQuality;
	return actionResult::success;
}

/***
CP
***/

actionResult craft::tricksOfTheTrade()
{
	if (step == 1) return actionResult::failHardUnavailable;
	if (cond != condition::good && cond != condition::excellent) return actionResult::failSoftUnavailable;

	changeCP(20);

	return actionResult::success;
}

/***
DURABILITY
***/

actionResult craft::mastersMend()
{
	if (!changeCP(-88)) return actionResult::failNoCP;

	changeDurability(30);

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
	wasteNot2Time = 0;
}

actionResult craft::wasteNot2()
{
	if (!changeCP(-98)) return actionResult::failNoCP;

	return actionResult::success;
}

void craft::wasteNot2Post()
{
	wasteNotTime = 0;
	wasteNot2Time = 8;
}

actionResult craft::manipulation()
{
	if (!changeCP(-96)) return actionResult::failNoCP;

	manipulationTime = 0;

	return actionResult::success;
}

void craft::manipulationPost()
{
	manipulationTime = 8;
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

actionResult craft::reflect()
{
	if (step != 1) return actionResult::failHardUnavailable;
	actionResult output = commonTouch(-24, 100, 100);

	if(output == actionResult::success) innerQuietStacks = 3;

	return output;
}

actionResult craft::ingenuity()
{
	if (!changeCP(-22)) return actionResult::failNoCP;
	
	return actionResult::success;
}

void craft::ingenuityPost()
{
	ingenuityTime = 5;
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
	innovationTime = 4;
}

actionResult craft::nameOfTheElements()
{
	if (nameOfTheElementsUsed) return actionResult::failHardUnavailable;
	if (!changeCP(-30)) return actionResult::failNoCP;

	nameOfTheElementsUsed = true;
	return actionResult::success;
}

void craft::nameOfTheElementsPost()
{
	nameOfTheElementsTime = 3;
}

actionResult craft::finalAppraisal()
{
	if (!changeCP(-1)) return actionResult::failNoCP;

	finalAppraisalTime = 5;	// here and not in a post since FA doesn't endStep
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

actionResult craft::reuse()
{
	if (reuseActive || quality < recipe.nominalQuality) return actionResult::failHardUnavailable;
	if (rollPercent(67)) return actionResult::failSoftUnavailable;
	if (!changeCP(-60)) return actionResult::failNoCP;

	reuseActive = true;

	return actionResult::success;
}

actionResult craft::performOne(actions action)
{
	switch (action)
	{
	case actions::basicSynth: return basicSynth();
	case actions::carefulSynthesis: return carefulSynthesis();
	case actions::rapidSynthesis: return rapidSynthesis();
	case actions::focusedSynthesis: return focusedSynthesis();
	case actions::delicateSynthesis: return delicateSynthesis();
	case actions::intensiveSynthesis: return intensiveSynthesis();
	case actions::muscleMemory: return muscleMemory();
	case actions::brandOfTheElements: return brandOfTheElements();
	
	case actions::basicTouch: return basicTouch();
	case actions::standardTouch: return standardTouch();
	case actions::hastyTouch: return hastyTouch();
	case actions::byregotsBlessing: return byregotsBlessing();
	case actions::preciseTouch: return preciseTouch();
	case actions::focusedTouch: return focusedTouch();
	case actions::patientTouch: return patientTouch();
	case actions::prudentTouch: return prudentTouch();
	case actions::preparatoryTouch: return preparatoryTouch();
	case actions::trainedEye: return trainedEye();

	case actions::tricksOfTheTrade: return tricksOfTheTrade();

	case actions::mastersMend: return mastersMend();
	case actions::wasteNot: return wasteNot();
	case actions::wasteNot2: return wasteNot2();
	case actions::manipulation: return manipulation();

	case actions::innerQuiet: return innerQuiet();
	case actions::reflect: return reflect();
	case actions::ingenuity: return ingenuity();
	case actions::greatStrides: return greatStrides();
	case actions::innovation: return innovation();
	case actions::nameOfTheElements: return nameOfTheElements();
	case actions::finalAppraisal: return finalAppraisal();

	case actions::observe: return observe();
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
	case actions::muscleMemory: return muscleMemoryPost();
	case actions::wasteNot: return wasteNotPost();
	case actions::wasteNot2: return wasteNot2Post();
	case actions::manipulation: return manipulationPost();
	case actions::ingenuity: return ingenuityPost();
	case actions::greatStrides: return greatStridesPost();
	case actions::innovation: return innovationPost();
	case actions::nameOfTheElements:
		return nameOfTheElementsPost();
	case actions::observe: return observePost();
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
		if (result == actionResult::failNoCP || result == actionResult::failHardUnavailable)
			craftResult.invalidActions++;
		if (result != actionResult::success && result != actionResult::failRNG)
			continue;

		if (*it == actions::finalAppraisal) continue;	// since it doesn't tick

		if (durability <= 0 || progress + progressWiggle >= recipe.difficulty)
			break;
		endStep();
		if (result == actionResult::success) performOnePost(*it);
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
		craftResult.hqPercent = hqPercentFromQuality((quality * 100) / recipe.nominalQuality);
		break;
	case goalType::maxQuality:
		break;
	case goalType::collectability:
		craftResult.collectableHit = quality >= recipe.quality;
		break;
	}
	craftResult.steps = step;
	if (it != sequence.cend()) ++it;	// the iterator needs to sit on the one after the last craft in order for the next calculation to work
	craftResult.invalidActions += static_cast<int>(distance(it, sequence.cend()));	// Count everything that didn't happen post-macro
	craftResult.reuseUsed = reuseActive;

	return craftResult;
}
