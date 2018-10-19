#pragma once
#include <string>
#include <map>
#include "common.h"
#include "random.h"

enum class actions
{
	// Synthesis
	basicSynth,
	standardSynthesis,
	flawlessSynthesis,
	carefulSynthesis,
	carefulSynthesis2,
	carefulSynthesis3,
	pieceByPiece,
	rapidSynthesis,
	rapidSynthesis2,
	rapidSynthesis3,
	focusedSynthesis,
	delicateSynthesis,
	intensiveSynthesis,
	muscleMemory,
	brandOfTheElements,

	// Touch
	basicTouch,
	standardTouch,
	advancedTouch,
	hastyTouch,
	hastyTouch2,
	byregotsBlessing,
	preciseTouch,
	focusedTouch,
	patientTouch,
	prudentTouch,
	preparatoryTouch,
	trainedEye,
	trainedInstinct,

	// CP
	comfortZone,
	rumination,
	tricksOfTheTrade,

	// Durability
	mastersMend,
	mastersMend2,
	wasteNot,
	wasteNot2,
	manipulation,
	manipulation2,

	// Buffs
	innerQuiet,
	steadyHand,
	steadyHand2,
	ingenuity,
	ingenuity2,
	greatStrides,
	innovation,
	makersMark,
	initialPreparations,
	nameOfTheElements,

	// Specialist
	whistle,
	satisfaction,
	innovativeTouch,
	nymeiasWheel,
	byregotsMiracle,
	trainedHand,
	heartOfTheSpecialist,
	specialtyReinforce,
	specialtyRefurbish,
	specialtyReflect,

	observe,
	reclaim,
	reuse
};

const std::map<actions, std::string> simpleText = {
	{actions::basicSynth, "basicSynth"},
	{actions::standardSynthesis, "standardSynthesis"},
	{actions::flawlessSynthesis, "flawlessSynthesis"},
	{actions::carefulSynthesis, "carefulSynthesis"},
	{actions::carefulSynthesis2, "carefulSynthesis2"},
	{actions::carefulSynthesis3, "carefulSynthesis3"},
	{actions::pieceByPiece, "pieceByPiece"},
	{actions::rapidSynthesis, "rapidSynthesis"},
	{actions::rapidSynthesis2, "rapidSynthesis2"},
	{actions::rapidSynthesis3, "rapidSynthesis3"},
	{actions::focusedSynthesis, "focusedSynthesis"},
	{actions::delicateSynthesis, "delicateSynthesis"},
	{actions::intensiveSynthesis, "intensiveSynthesis"},
	{actions::muscleMemory, "muscleMemory"},
	{actions::brandOfTheElements, "brandOfTheElements"},

	{actions::basicTouch, "basicTouch"},
	{actions::standardTouch, "standardTouch"},
	{actions::advancedTouch, "advancedTouch"},
	{actions::hastyTouch, "hastyTouch"},
	{actions::hastyTouch2, "hastyTouch2"},
	{actions::byregotsBlessing, "byregotsBlessing"},
	{actions::preciseTouch, "preciseTouch"},
	{actions::focusedTouch, "focusedTouch"},
	{actions::patientTouch, "patientTouch"},
	{actions::prudentTouch, "prudentTouch"},
	{actions::preparatoryTouch, "preparatoryTouch"},
	{actions::trainedEye, "trainedEye"},
	{actions::trainedInstinct, "trainedInstinct"},

	{actions::comfortZone, "comfortZone"},
	{actions::rumination, "rumination"},
	{actions::tricksOfTheTrade, "tricksOfTheTrade"},

	{actions::mastersMend, "mastersMend"},
	{actions::mastersMend2, "mastersMend2"},
	{actions::wasteNot, "wasteNot"},
	{actions::wasteNot2, "wasteNot2"},
	{actions::manipulation, "manipulation"},
	{actions::manipulation2, "manipulation2"},

	{actions::innerQuiet, "innerQuiet"},
	{actions::steadyHand, "steadyHand"},
	{actions::steadyHand2, "steadyHand2"},
	{actions::ingenuity, "ingenuity"},
	{actions::ingenuity2, "ingenuity2"},
	{actions::greatStrides, "greatStrides"},
	{actions::innovation, "innovation"},
	{actions::makersMark, "makersMark"},
	{actions::initialPreparations, "initialPreparations"},
	{actions::nameOfTheElements, "nameOfTheElements"},

	{actions::whistle, "whistle"},
	{actions::satisfaction, "satisfaction"},
	{actions::innovativeTouch, "innovativeTouch"},
	{actions::nymeiasWheel, "nymeiasWheel"},
	{actions::byregotsMiracle, "byregotsMiracle"},
	{actions::trainedHand, "trainedHand"},
	{actions::heartOfTheSpecialist, "heartOfTheCrafter"},
	{actions::specialtyReinforce, "specialtyReinforce"},
	{actions::specialtyRefurbish, "specialtyRefurbish"},
	{actions::specialtyReflect, "specialtyReflect"},

	{actions::observe, "observe"},
	{actions::reclaim, "reclaim"},
	{actions::reuse, "reuse"}
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
		};
		int durability;
		int steps;
		int invalidActions;
		bool reuseUsed;
	};

	enum class condition
	{
		poor,
		normal,
		good,
		excellent
	};

	enum class actionResult
	{
		success,
		failRNG,
		failNoCP,
		failHardUnavailable,	// for things that should count towards the unavailable counter
		failSoftUnavailable		// for things that might randomly proc
	};

private:
	const crafterStats crafter;
	const recipeStats recipe;

	int step;
	int durability; 
	int CP;
	int quality;
	int progress;

	condition cond;

//	int effectiveLevel;		// Adjusted with Ingenuity
	int progressFactor;
	int qualityFactor;

	// Buffs
	int comfortZoneTime = 0;
	int wasteNotTime = 0;	// Covers wasteNot 1 and 2
	int manipulationTime = 0;
	int manipulation2Time = 0;
	int innerQuietStacks = 0;
	int steadyHandTime = 0;
	int steadyHand2Time = 0;
	int ingenuityTime = 0;
	int ingenuity2Time = 0;
	int greatStridesTime = 0;
	int innovationTime = 0;
	int makersMarkTime = 0;
	int nameOfTheElementsTime = 0;
	bool nameOfTheElementsUsed = false;
	int whistleStacks = 0;
	bool observeCombo = false;	// For Focused combo
	int heartTime = 0;
	int heartUses = 0;

	const bool normalLock;
	bool initialPreparationsActive = false;
	bool reclaimActive = false;
	bool reuseActive = false;
	bool strokeOfGeniusActive = false;

	random& rng;

	// chance == 70 means 70% success and so on
	inline bool rollPercent(int chance, bool canUseSteadyHand = true) const;

	void calculateFactors();

	void increaseProgress(int efficiency);
	void increaseQuality(int efficiency);

	// Negative amount to deduct, positive to add. Returns false if not enough CP (will not deduct if so)
	bool changeCP(int amount);

	// Negative to remove durability, positive to add.
	void changeDurability(int amount)
	{
		durability = std::min(durability + amount, recipe.durability);
	}

	void hitDurability(int amount = 10)
	{
		return changeDurability(wasteNotTime > 0 ? -amount / 2 : -amount);
	}

	condition getNextCondition(condition current);
	void endStep();

	void nameOfTheElementsPost();
	void finishingTouches();

	// both percentages from 0-100
	static int hqPercentFromQuality(int qualityPercent);

public:
	craft() = delete;
	craft(int initialQuality, crafterStats cS, recipeStats rS, bool nLock, random& r) :
		crafter(cS),
		recipe(rS),
		step(1),
		durability(recipe.durability),
		CP(crafter.CP),
		quality(initialQuality),
		progress(0),
		cond(condition::normal),
		normalLock(nLock),
		rng(r)
	{
		calculateFactors();
	}

	bool checkBuffsValid() const;
	std::string getState() const;

private:
	actionResult commonSynth(int cpChange, int efficiency, int successChance, bool doubleDurability = false);
	actionResult commonTouch(int cpChange, int efficiency, int successChance, bool doubeDurability = false);

	// Synthesis
	actionResult basicSynth() { return commonSynth(0, 100, 90); }
	actionResult standardSynthesis() { return commonSynth(-15, 150, 90); }
	actionResult flawlessSynthesis();
	actionResult carefulSynthesis() { return commonSynth(0, 90, 100); }
	actionResult carefulSynthesis2() { return commonSynth(0, 120, 100); }
	actionResult carefulSynthesis3() { return commonSynth(-7, 150, 100); }
	actionResult pieceByPiece();
	actionResult rapidSynthesis() { return commonSynth(0, 250, 50); }
	actionResult rapidSynthesis2() { return commonSynth(-12, 300, 60); }
	actionResult rapidSynthesis3() { return commonSynth(-24, durability >= 20 ? 600 : 200, 60, true); }
	actionResult focusedSynthesis() { return commonSynth(-5, 200, observeCombo ? 100 : 50); }
	actionResult delicateSynthesis();
	actionResult intensiveSynthesis();
	actionResult muscleMemory();
	actionResult brandOfTheElements();

	// Touches
	actionResult basicTouch() { return commonTouch(-18, 100, 70); }
	actionResult standardTouch() { return commonTouch(-32, 125, 80); }
	actionResult advancedTouch() { return commonTouch(-48, 150, 90); }
	actionResult hastyTouch() { return commonTouch(0, 100, 50); }
	actionResult hastyTouch2() { return commonTouch(-5, 100, 60); }
	actionResult byregotsBlessing();
	actionResult preciseTouch();
	actionResult focusedTouch() { return commonTouch(-18, 150, observeCombo ? 100 : 50); }
	actionResult patientTouch();
	actionResult prudentTouch();
	actionResult preparatoryTouch();
	actionResult trainedEye();
	actionResult trainedInstinct();

	// Buffs
	actionResult comfortZone();
	void comfortZonePost();
	actionResult rumination();
	actionResult tricksOfTheTrade();

	// Durability
	actionResult mastersMend();
	actionResult mastersMend2();
	actionResult wasteNot();
	void wasteNotPost();
	actionResult wasteNot2();
	void wasteNot2Post();
	actionResult manipulation();
	void manipulationPost();
	actionResult manipulation2();
	void manipulation2Post();

	// Buffs
	actionResult innerQuiet();
	actionResult steadyHand();
	void steadyHandPost();
	actionResult steadyHand2();
	void steadyHand2Post();
	actionResult ingenuity();
	void ingenuityPost();
	actionResult ingenuity2();
	void ingenuity2Post();
	actionResult greatStrides();
	void greatStridesPost();
	actionResult innovation();
	void innovationPost();		// also for innovativeTouch
	actionResult makersMark();
	void makersMarkPost();
	actionResult initialPreparations();
	actionResult nameOfTheElements();

	// Specialty Actions
	actionResult whistle();
	actionResult satisfaction();
	actionResult innovativeTouch();
	actionResult nymeiasWheel();
	actionResult byregotsMiracle();
	actionResult trainedHand();
	actionResult heartOfTheSpecialist();
	void heartOfTheSpecialistPost();
	actionResult specialtyReinforce();
	actionResult specialtyRefurbish();
	actionResult specialtyReflect();

	actionResult observe();
	void observePost();
	actionResult reclaim();
	actionResult reuse();

public:
	actionResult performOne(actions action);
	void performOnePost(actions action);
	endResult performAll(const sequenceType& sequence, goalType goal, int progessWiggle, bool echoEach = false);
};
