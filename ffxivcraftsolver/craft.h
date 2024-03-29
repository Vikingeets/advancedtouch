#pragma once
#include <string>
#include <map>
#include "common.h"
#include "levels.h"
#include "random.h"

enum class actions : char
{
	// Synthesis
	basicSynth,
	carefulSynthesis,
	rapidSynthesis,
	focusedSynthesis,
	groundwork,
	delicateSynthesis,
	prudentSynthesis,
	intensiveSynthesis,
	muscleMemory,

	// Touch
	basicTouch,
	standardTouch,
	advancedTouch,
	hastyTouch,
	byregotsBlessing,
	preciseTouch,
	focusedTouch,
	prudentTouch,
	preparatoryTouch,
	trainedEye,
	trainedFinesse,

	// CP
	tricksOfTheTrade,

	// Durability
	mastersMend,
	wasteNot,
	wasteNot2,
	manipulation,

	// Buffs
	innerQuiet,	// To allow the stepwise to apply the buff
	reflect,
	greatStrides,
	veneration,
	innovation,
	finalAppraisal,

	observe,

	invalid
};

const std::map<actions, std::string> simpleText = {
	{actions::basicSynth, "basicSynth"},
	{actions::carefulSynthesis, "carefulSynthesis"},
	{actions::rapidSynthesis, "rapidSynthesis"},
	{actions::focusedSynthesis, "focusedSynthesis"},
	{actions::delicateSynthesis, "delicateSynthesis"},
	{actions::groundwork, "groundwork"},
	{actions::prudentSynthesis, "prudentSynthesis"},
	{actions::intensiveSynthesis, "intensiveSynthesis"},
	{actions::muscleMemory, "muscleMemory"},

	{actions::basicTouch, "basicTouch"},
	{actions::standardTouch, "standardTouch"},
	{actions::advancedTouch, "advancedTouch"},
	{actions::hastyTouch, "hastyTouch"},
	{actions::byregotsBlessing, "byregotsBlessing"},
	{actions::preciseTouch, "preciseTouch"},
	{actions::focusedTouch, "focusedTouch"},
	{actions::prudentTouch, "prudentTouch"},
	{actions::preparatoryTouch, "preparatoryTouch"},
	{actions::trainedEye, "trainedEye"},
	{actions::trainedFinesse, "trainedFinesse"},
	
	{actions::tricksOfTheTrade, "tricksOfTheTrade"},

	{actions::mastersMend, "mastersMend"},
	{actions::wasteNot, "wasteNot"},
	{actions::wasteNot2, "wasteNot2"},
	{actions::manipulation, "manipulation"},

	{actions::innerQuiet, "innerQuiet"},
	{actions::reflect, "reflect"},
	{actions::greatStrides, "greatStrides"},
	{actions::veneration, "veneration"},
	{actions::innovation, "innovation"},
	{actions::finalAppraisal, "finalAppraisal"},

	{actions::observe, "observe"},
};


class craft
{
public:
	using sequenceType = std::vector<actions>;

	struct endResult
	{
		int progress;
		int quality;				/// not in union to allow solver tiebreaks
		union
		{
			int hqPercent;			// normal and hqOrBust mode
			bool collectableHit;	// collectable mode
			int points;				// points mode
		};
		int steps;
		int invalidActions;
		bool firstInvalid;			// if the first action in the sequence was invalid
	};

	enum class condition
	{
		poor,
		normal,
		good,
		excellent,

		// expert
		centered,
		sturdy,
		pliant,
		malleable,
		primed,
		goodomen
	};

	enum class actionResult
	{
		success,
		failRNG,
		failNoCP,
		failHardUnavailable,	// for things that should count towards the unavailable counter
		failSoftUnavailable		// for things that might randomly proc
	};

	enum class rngOverride
	{
		success,
		failure,
		random
	};

private:
	crafterStats crafter;
	recipeStats recipe;

	int step;
	int durability; 
	int CP;
	int quality;
	int progress;

	// Each entry as integer percentage. Normal not included
	std::map<condition, int> conditionChances;
	condition cond;

	// Buffs
	int muscleMemoryTime = 0;
	int wasteNotTime = 0;
	int wasteNot2Time = 0;
	int manipulationTime = 0;
	int venerationTime = 0;
	int innerQuiet = 0;
	int greatStridesTime = 0;
	int innovationTime = 0;
	int finalAppraisalTime = 0;
	bool basicTouchCombo = false;	// For BT->ST combo
	bool standardTouchCombo = false;	// for ST->AT combo
	bool observeCombo = false;	// For Focused combo

	bool normalLock;

	randomGenerator* rng;
	rngOverride over;

	// chance == 70 means 70% success and so on
	inline bool rollPercent(int chance) const;

	void increaseProgress(int efficiency);
	void increaseQuality(int efficiency);

	// Negative amount to deduct, positive to add. Returns false if not enough CP (will not deduct if so)
	bool changeCP(int amount);

	int getDurabilityCost(int base)
	{
		if (wasteNotTime > 0 || wasteNot2Time > 0) base -= base/ 2;
		if (cond == condition::sturdy) base -= base / 2;
		return base;
	}

	void raiseDurability(int amount)
	{
		durability = std::min(durability + amount, recipe.durability);
	}

	void lowerDurability(int amount = 10)
	{
		durability -= getDurabilityCost(amount);
	}

	void setProbabilities();

	condition getNextCondition(condition current);
	void endStep(actions action, actionResult result);

	// both percentages from 0-100
	static int hqPercentFromQuality(int qualityPercent);

public:
	craft() = delete;
	craft(const craft&) = default;
	craft(craft&&) = default;
	craft& operator=(const craft&) = default;

	craft(int initialQuality, crafterStats cS, recipeStats rS, bool nLock) :
		crafter(cS),
		recipe(rS),
		step(1),
		durability(recipe.durability),
		CP(crafter.CP),
		quality(initialQuality),
		progress(0),
		cond(condition::normal),
		normalLock(nLock),
		rng(nullptr)
	{
		setProbabilities();
	}

	std::string getState() const;

	void setRNG(randomGenerator* r) { rng = r; }

private:
	actionResult commonSynth(int cpChange, int efficiency, int successChance, int durabilityCost = 10);
	actionResult commonTouch(int cpChange, int efficiency, int successChance, int durabilityCost = 10);

	// Synthesis
	actionResult basicSynth() { return commonSynth(0, crafter.level >= 31 ? 120 : 100, 100); }
	actionResult carefulSynthesis() { return commonSynth(-7, crafter.level >= 82 ? 180 : 150, 100); }
	actionResult rapidSynthesis() { return commonSynth(0, crafter.level >= 63 ? 500 : 250, 50); }
	actionResult focusedSynthesis() { return commonSynth(-5, 200, observeCombo ? 100 : 50); }
	actionResult delicateSynthesis();
	actionResult groundwork();
	actionResult prudentSynthesis();
	actionResult intensiveSynthesis();
	actionResult muscleMemory();
	void muscleMemoryPost();

	// Touches
	actionResult basicTouch() { return commonTouch(-18, 100, 100); }
	void basicTouchPost();
	actionResult standardTouch() { return commonTouch(basicTouchCombo ? -18 : -32, 125, 100); }
	void standardTouchPost();
	actionResult advancedTouch() { return commonTouch(standardTouchCombo ? -18 : -46, 150, 100); }
	actionResult hastyTouch() { return commonTouch(0, 100, 60); }
	actionResult byregotsBlessing();
	actionResult preciseTouch();
	actionResult focusedTouch() { return commonTouch(-18, 150, observeCombo ? 100 : 50); }
	actionResult prudentTouch();
	actionResult preparatoryTouch();
	actionResult trainedEye();
	actionResult trainedFinesse();

	// CP
	actionResult tricksOfTheTrade();

	// Durability
	actionResult mastersMend();
	actionResult wasteNot();
	void wasteNotPost();
	actionResult wasteNot2();
	void wasteNot2Post();
	actionResult manipulation();
	void manipulationPost();

	// Buffs
	actionResult reflect();
	actionResult greatStrides();
	void greatStridesPost();
	actionResult veneration();
	void venerationPost();
	actionResult innovation();
	void innovationPost();
	actionResult finalAppraisal();

	actionResult observe();
	void observePost();

	actionResult performOne(actions action, rngOverride override = rngOverride::random);
	void performOnePost(actions action);

public:
	// Also ends the step and does the post action
	actionResult performOneComplete(actions action, rngOverride override);
	endResult performAll(const sequenceType& sequence, goalType goal, bool echoEach = false);

	void setStep(int s) { step = s; }
	int getStep() const { return step; }
	void setDurability(int d) { durability = std::min(d, recipe.durability); }
	bool outOfDurability() const { return durability <= 0; }
	void setProgress(int p) { progress = p; }
	bool maxedProgress() const { return progress >= recipe.difficulty; }
	void setQuality(int q) { quality = q; }
	void setCondition(condition c) { cond = c; }
	condition getCondition() const { return cond; }
	void setCP(int cp) { CP = std::min(cp, crafter.CP); }
	void setBuff(actions buff, int time);

	// Won't contain invalid stats
	endResult getResult(goalType goal) const;
};
