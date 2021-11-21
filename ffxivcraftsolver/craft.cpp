#include <cassert>
#include <algorithm>
#include <iostream>
#include <map>
#include <cmath>
#include "craft.h"
#include "levels.h"

using namespace std;
using actionResult = craft::actionResult;

// chance == 70 means 70% success and so on
inline bool craft::rollPercent(int chance) const
{
	if (cond == condition::centered) chance += 25;

	if (chance >= 100) return true;

	switch (over)
	{
	case rngOverride::success:
		return true;
	case rngOverride::failure:
		return false;
	case rngOverride::random:
		assert(rng != nullptr);
		return rng->generateInt(99) < chance;
	}
}

void craft::increaseProgress(int efficiency, bool isBrand)
{
	float bonus = 1.f;

	if (isBrand && nameOfTheElementsTime > 0)
	{
		bonus += (2 * ceil((1.f - static_cast<float>(progress) / static_cast<float>(recipe.difficulty)) * 100.f)) / 100.f;
	}

	if (muscleMemoryTime > 0)
	{
		bonus += 1.f;
		muscleMemoryTime = 0;
	}

	if (venerationTime > 0)
	{
		bonus += 0.5f;
	}

	float baseProgress = (crafter.craftsmanship * 21.f) / 100.f + 2.f;
	float suggestionMod = (crafter.craftsmanship + 10000.f) / (recipe.suggestedCraftsmanship + 10000.f);
	float differenceMod = progressFactor / 100.f;
	float conditionMod;

	switch (cond)
	{
	case condition::malleable:
		conditionMod = 1.5f;
		break;
	case condition::normal:
	default:
		conditionMod = 1.0f;
		break;
	}

	int progressIncrease = static_cast<int>(baseProgress * suggestionMod * differenceMod);
	
	progress += (progressIncrease * conditionMod * efficiency * bonus) / 100;

	if (finalAppraisalTime > 0 && progress >= recipe.difficulty)
	{
		progress = recipe.difficulty - 1;
		finalAppraisalTime = 0;
	}

	return;
}

void craft::increaseQuality(int efficiency)
{
	float bonus = 1.f;

	if (greatStridesTime > 0)
	{
		bonus += 1.f;
		greatStridesTime = 0;
	}

	if (innovationTime > 0)
	{
		bonus += 0.5f;
	}

	int control = crafter.control + (max(0, innerQuietStacks - 1) * crafter.control) / 5;
	
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
	default:
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

	quality += (qualityIncrease * efficiency * bonus) / 100;

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

	if (cond == condition::pliant) amount -= amount / 2;

	if (-amount > CP) return false;

	CP += amount;
	return true;
}

bool inRange(int num, int high, int low)
{
	return low <= num && num <= high;
}

void craft::setProbabilities()
{
	conditionChances.clear();
	if (recipe.expert)
	{
		if (recipe.rLevel == 513)
		{
			conditionChances.insert({ condition::good, 12 });
			conditionChances.insert({ condition::sturdy, 15 });
			conditionChances.insert({ condition::pliant, 12 });
			conditionChances.insert({ condition::malleable, 12 });
			conditionChances.insert({ condition::primed, 12 });
		}
		else
		{
			conditionChances.insert({ condition::good, 12 });
			conditionChances.insert({ condition::centered, 15 });
			conditionChances.insert({ condition::sturdy, 15 });
			conditionChances.insert({ condition::pliant, 12 });
		}
	}
	else
	{
		bool qualityAssurance = crafter.level >= 63;

		if (
			inRange(recipe.rLevel, 160, 254) ||	// 60*+
			inRange(recipe.rLevel, 300, 384) ||	// 70*+
			recipe.rLevel >= 430				// 80*+
			)
		{
			conditionChances.insert({ condition::good, qualityAssurance ? 11 : 10 });
			conditionChances.insert({ condition::excellent, 1 });
		}
		else if (
			inRange(recipe.rLevel, 136, 159) ||	// 55+
			inRange(recipe.rLevel, 276, 299) ||	// 65+
			inRange(recipe.rLevel, 406, 429)	// 75+
			)
		{
			conditionChances.insert({ condition::good, qualityAssurance ? 17 : 15 });
			conditionChances.insert({ condition::excellent, 2 });
		}
		else if (
			inRange(recipe.rLevel, 115, 135) ||	// 51+	
			inRange(recipe.rLevel, 255, 275) ||	// 61+
			inRange(recipe.rLevel, 385, 405)	// 71+
			)
		{
			conditionChances.insert({ condition::good, qualityAssurance ? 22 : 20 });
			conditionChances.insert({ condition::excellent, 2 });
		}
		else									// 1+
		{
			conditionChances.insert({ condition::good, qualityAssurance ? 27 : 25 });
			conditionChances.insert({ condition::excellent, 2 });
		}
	}
}

craft::condition craft::getNextCondition(condition current)
{
	// Unfortunately, std::discrete_distribution is far too slow to be usable.
	if (!recipe.expert)
	{
		switch (current)
		{
		case condition::poor:
		case condition::good:
			return condition::normal;
		default:
		case condition::normal:
			break;
		case condition::excellent:
			return condition::poor;
		}
	}

	if (normalLock || over != rngOverride::random) return condition::normal;

	int roll = rng->generateInt(99);
	
	for (const auto& c : conditionChances)
	{
		if (roll < c.second) return c.first;
		else roll -= c.second;
	}

	return condition::normal;
}

void craft::endStep()
{
	muscleMemoryTime--;
	wasteNotTime--;
	wasteNot2Time--;
	if(manipulationTime > 0)
	{
		raiseDurability(5);
		manipulationTime--;
	}
	greatStridesTime--;
	venerationTime--;
	innovationTime--;
	nameOfTheElementsTime--;
	if (nameOfTheElementsTime == 0) nameless = true;
	finalAppraisalTime--;
	observeCombo = false;
	basicTouchCombo = false;
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

		case condition::centered:
			output += "Centered, ";
			break;
		case condition::sturdy:
			output += "Sturdy, ";
			break;
		case condition::pliant:
			output += "Pliant, ";
			break;
		case condition::malleable:
			output += "Malleable, ";
			break;
		case condition::primed:
			output += "Primed, ";
			break;
		}
	}

	output += "Buffs: ";
	bool anybuffs = false;
	output += stateBuffList("Muscle Memory: ", muscleMemoryTime, &anybuffs);
	output += stateBuffList("Waste Not: ", wasteNotTime, &anybuffs);
	output += stateBuffList("Waste Not 2: ", wasteNot2Time, &anybuffs);
	output += stateBuffList("Manipulation: ", manipulationTime, &anybuffs);
	output += stateBuffList("Inner Quiet: ", innerQuietStacks, &anybuffs);
	output += stateBuffList("Great Strides: ", greatStridesTime, &anybuffs);
	output += stateBuffList("Veneration: ", venerationTime, &anybuffs);
	output += stateBuffList("Innovation: ", innovationTime, &anybuffs);
	output += stateBuffList("Final Appraisal: ", finalAppraisalTime, &anybuffs);
	output += stateBuffList("Name of the Elements: ", nameOfTheElementsTime, &anybuffs);
	if (nameless)
	{
		anybuffs = true;
		output += "Nameless; ";
	}
	if (basicTouchCombo)
	{
		anybuffs = true;
		output += "Basic Touch; ";
	}
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

actionResult craft::commonSynth(int cpChange, int efficiency, int successChance, int durabilityCost)
{
	if (!changeCP(cpChange)) return actionResult::failNoCP;
	actionResult output;

	if (rollPercent(successChance))
	{
		increaseProgress(efficiency);
		output = actionResult::success;
	}
	else output = actionResult::failRNG;

	lowerDurability(durabilityCost);

	return output;
}

actionResult craft::commonTouch(int cpChange, int efficiency, int successChance, int durabilityCost)
{
	if (!changeCP(cpChange)) return actionResult::failNoCP;
	actionResult output;

	if (rollPercent(successChance))
	{
		increaseQuality(efficiency);
		output = actionResult::success;
	}
	else output = actionResult::failRNG;

	lowerDurability(durabilityCost);

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

	lowerDurability();
	return actionResult::success;
}

actionResult craft::intensiveSynthesis()
{
	if (cond != condition::good && cond != condition::excellent) return actionResult::failSoftUnavailable;
	return commonSynth(-6, 400, 100);
}

actionResult craft::muscleMemory()
{
	if (step != 1) return actionResult::failHardUnavailable;

	return commonSynth(-6, 300, 100);
}

void craft::muscleMemoryPost()
{
	muscleMemoryTime = 5;
	if (cond == condition::primed)
		muscleMemoryTime += 2;
}

actionResult craft::brandOfTheElements()
{
	if (!changeCP(-6)) return actionResult::failNoCP;

	increaseProgress(100, true);

	lowerDurability();

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
	return commonTouch(-25, 100, 100, 5);
}

actionResult craft::preparatoryTouch()
{
	actionResult output = commonTouch(-40, 200, 100, 20);
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

	raiseDurability(30);

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
	if (cond == condition::primed)
		wasteNotTime += 2;
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
	if (cond == condition::primed)
		wasteNot2Time += 2;
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
	if (cond == condition::primed)
		manipulationTime += 2;
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

actionResult craft::greatStrides()
{
	if (!changeCP(-32)) return actionResult::failNoCP;

	return actionResult::success;
}

void craft::greatStridesPost()
{
	greatStridesTime = 3;
	if (cond == condition::primed)
		greatStridesTime += 2;
}

actionResult craft::veneration()
{
	if (!changeCP(-18)) return actionResult::failNoCP;

	return actionResult::success;
}

void craft::venerationPost()
{
	venerationTime = 4;
	if (cond == condition::primed)
		venerationTime += 2;
}

actionResult craft::innovation()
{
	if (!changeCP(-18)) return actionResult::failNoCP;

	return actionResult::success;
}

void craft::innovationPost()
{
	innovationTime = 4;
	if (cond == condition::primed)
		innovationTime += 2;
}

actionResult craft::nameOfTheElements()
{
	if (nameOfTheElementsTime > 0) return actionResult::failHardUnavailable;
	// The game lets you use Name while the nameless debuff is active. It succeeds with no effect.
	if (!changeCP(-30)) return actionResult::failNoCP;

	return actionResult::success;
}

void craft::nameOfTheElementsPost()
{
	if (!nameless)
	{
		nameOfTheElementsTime = 3;
		if (cond == condition::primed)
			nameOfTheElementsTime += 2;
	}
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

actionResult craft::performOne(actions action, rngOverride override)
{
	over = override;
	switch (action)
	{
	case actions::basicSynth: return basicSynth();
	case actions::carefulSynthesis: return carefulSynthesis();
	case actions::rapidSynthesis: return rapidSynthesis();
	case actions::focusedSynthesis: return focusedSynthesis();
	case actions::delicateSynthesis: return delicateSynthesis();
	case actions::groundwork: return groundwork();
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
	case actions::greatStrides: return greatStrides();
	case actions::veneration: return veneration();
	case actions::innovation: return innovation();
	case actions::nameOfTheElements: return nameOfTheElements();
	case actions::finalAppraisal: return finalAppraisal();

	case actions::observe: return observe();
	case actions::invalid:
	default:
		assert(false);
		return actionResult::failHardUnavailable;
	}
}

void craft::performOnePost(actions action)
{
	switch (action)
	{
	case actions::basicTouch: return basicTouchPost();
	case actions::muscleMemory: return muscleMemoryPost();
	case actions::wasteNot: return wasteNotPost();
	case actions::wasteNot2: return wasteNot2Post();
	case actions::manipulation: return manipulationPost();
	case actions::greatStrides: return greatStridesPost();
	case actions::veneration: return venerationPost();
	case actions::innovation: return innovationPost();
	case actions::nameOfTheElements: return nameOfTheElementsPost();
	case actions::observe: return observePost();
	default: return;
	}
}

actionResult craft::performOneComplete(actions action, rngOverride override)
{
	actionResult output = performOne(action, override);

	if (output != actionResult::success && output != actionResult::failRNG)
		return output;

	if (action == actions::finalAppraisal) return output;

	endStep();

	if(output == actionResult::success) performOnePost(action);

	return output;
}

craft::endResult craft::performAll(const craft::sequenceType& sequence, goalType goal, bool echoEach)
{
	endResult craftResult;
	craftResult.invalidActions = 0;
	craftResult.firstInvalid = false;
	int softInvalids = 0;

	craft::sequenceType::const_iterator it;
	for (it = sequence.cbegin(); it != sequence.cend(); ++it)
	{
		if (echoEach)
		{
			cout << getState() << '\n';
			cout << "Step " << step << ", ";
		}

		if (it == sequence.cbegin() && *it == actions::finalAppraisal && finalAppraisalTime == 5)
			craftResult.firstInvalid = true;

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
		if (it == sequence.cbegin() && result != actionResult::success && result != actionResult::failRNG)
			craftResult.firstInvalid = true;
		if (result == actionResult::failNoCP || result == actionResult::failHardUnavailable)
			craftResult.invalidActions++;
		if (result == actionResult::failSoftUnavailable) softInvalids++;
		if (result != actionResult::success && result != actionResult::failRNG)
			continue;

		if (*it == actions::finalAppraisal)
		{
			softInvalids++;	// I have never seen this action be useful in a result.
			continue;	// since it doesn't tick
		}

		if (durability <= 0 || progress >= recipe.difficulty)
			break;
		endStep();
		if (result == actionResult::success) performOnePost(*it);
	}

	if (echoEach)
	{
		cout << getState() << '\n';
	}

	craftResult.progress = progress;
	craftResult.quality = quality;
	switch (goal)
	{
	case goalType::hq:
		craftResult.hqPercent = hqPercentFromQuality((quality * 100) / recipe.nominalQuality);
		break;
	case goalType::maxQuality:
		break;
	case goalType::collectability:
		craftResult.collectableHit = quality >= recipe.quality;
		break;
	case goalType::points:
		craftResult.points = 0;
		for (auto p : recipe.points)
		{
			if (quality / 10 >= p.first)
				craftResult.points = p.second;
		}
	}
	craftResult.steps = step;
	if (it != sequence.cend()) ++it;	// the iterator needs to sit on the one after the last craft in order for the next calculation to work
	craftResult.invalidActions += static_cast<int>(distance(it, sequence.cend()));	// Count everything that didn't happen post-macro

	// The solver has a nasty habit of junking up results with soft invalids
	// So aggressively prune them out of anything that they don't help a 100% qual of.
	if (quality < recipe.quality) craftResult.invalidActions += softInvalids;

	return craftResult;
}

void craft::setBuff(actions buff, int timeOrStacks)
{
	if (timeOrStacks < -1) timeOrStacks = -1;
	switch (buff)
	{
	case actions::muscleMemory:
		muscleMemoryTime = timeOrStacks;
		return;
	case actions::wasteNot:
		wasteNotTime = timeOrStacks;
		wasteNot2Time = 0;
		return;
	case actions::wasteNot2:
		wasteNot2Time = timeOrStacks;
		wasteNotTime = 0;
		return;
	case actions::manipulation:
		manipulationTime = timeOrStacks;
		return;
	case actions::innerQuiet:
		if (timeOrStacks < 0) timeOrStacks = 0;
		else if (timeOrStacks > 11) timeOrStacks = 11;
		innerQuietStacks = timeOrStacks;
		return;
	case actions::greatStrides:
		greatStridesTime = timeOrStacks;
		return;
	case actions::veneration:
		venerationTime = timeOrStacks;
		return;
	case actions::innovation:
		innovationTime = timeOrStacks;
		return;
	case actions::nameOfTheElements:
		nameless = timeOrStacks == 0;
		nameOfTheElementsTime = timeOrStacks;
		return;
	case actions::finalAppraisal:
		finalAppraisalTime = timeOrStacks;
		return;
	case actions::basicTouch:
		basicTouchCombo = timeOrStacks > 0;
		return;
	case actions::observe:
		observeCombo = timeOrStacks > 0;
		return;
	default:
		return;
	}
}

craft::endResult craft::getResult(goalType goal) const
{
	endResult craftResult;
	craftResult.invalidActions = 0;
	craftResult.firstInvalid = false;
	craftResult.progress = progress;
	craftResult.quality = quality;
	craftResult.steps = step;

	switch (goal)
	{
	case goalType::hq:
		craftResult.hqPercent = hqPercentFromQuality((quality * 100) / recipe.nominalQuality);
		break;
	case goalType::maxQuality:
		break;
	case goalType::collectability:
		craftResult.collectableHit = quality >= recipe.quality;
		break;
	case goalType::points:
		craftResult.points = 0;
		for (auto p : recipe.points)
		{
			if (quality / 10 >= p.first)
				craftResult.points = p.second;
		}
		break;
	}

	return craftResult;
}
