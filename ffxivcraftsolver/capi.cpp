#include "advancedtouch.h"
#include "levels.h"
#include "craft.h"
#include "solver.h"

using namespace std;

const map<actions, int> atActionsToCActions = {
	{actions::basicSynth, AT_SYNTH_BASICSYNTH},
	{actions::carefulSynthesis, AT_SYNTH_CAREFULSYNTH},
	{actions::rapidSynthesis, AT_SYNTH_RAPIDSYNTH},
	{actions::focusedSynthesis, AT_SYNTH_FOCUSEDSYNTH},
	{actions::delicateSynthesis, AT_SYNTH_DELICATESYNTH},
	{actions::groundwork, AT_SYNTH_GROUNDWORK},
	{actions::prudentSynthesis, AT_SYNTH_PRUDENTSYNTH},
	{actions::intensiveSynthesis, AT_SYNTH_INTENSIVESYNTH},
	{actions::muscleMemory, AT_SYNTH_MUSCLEMEMORY},

	{actions::basicTouch, AT_TOUCH_BASICTOUCH},
	{actions::standardTouch, AT_TOUCH_STANDARDTOUCH},
	{actions::advancedTouch, AT_TOUCH_ADVANCEDTOUCH},
	{actions::hastyTouch, AT_TOUCH_HASTYTOUCH},
	{actions::byregotsBlessing, AT_TOUCH_BYREGOTS},
	{actions::preciseTouch, AT_TOUCH_PRECISETOUCH},
	{actions::focusedTouch, AT_TOUCH_FOCUSEDTOUCH},
	{actions::prudentTouch, AT_TOUCH_PRUDENTTOUCH},
	{actions::preparatoryTouch, AT_TOUCH_PREPARATORYTOUCH},
	{actions::trainedEye, AT_TOUCH_TRAINEDEYE},
	{actions::trainedFinesse, AT_TOUCH_TRAINEDFINESSE},

	{actions::tricksOfTheTrade, AT_TRICKSOFTHETRADE},

	{actions::mastersMend, AT_DURA_MASTERSMEND},
	{actions::wasteNot, AT_DURA_WASTENOT},
	{actions::wasteNot2, AT_DURA_WASTENOT2},
	{actions::manipulation, AT_DURA_MANIPULATION},

	{actions::reflect, AT_BUFF_REFLECT},
	{actions::greatStrides, AT_BUFF_GREATSTRIDES},
	{actions::veneration, AT_BUFF_VENERATION},
	{actions::innovation, AT_BUFF_INNOVATION},
	{actions::finalAppraisal, AT_BUFF_FINALAPPRAISAL},

	{actions::observe, AT_OBSERVE}
};

int atActionToCAction(actions action)
{
	return atActionsToCActions.at(action);
}

void populateResultSequence(atSolverResult* result, craft::sequenceType sequence)
{
	result->sequenceLength = sequence.size();
	if (sequence.size() > 100) sequence.resize(100);
	for (size_t i = 0; i < sequence.size(); ++i)
		result->sequence[i] = atActionToCAction(sequence[i]);
}

actions cActionToATAction(int action)
{
	auto it = find_if(atActionsToCActions.cbegin(), atActionsToCActions.cend(),
		[action](const pair<actions, int>& p) { return action == p.second; });
	if (it == atActionsToCActions.end()) terminate();
	else return it->first;
}

atSolver* atInitSolver(atCrafter crafter, atRecipe recipe, int* initialSequence, int initialSequenceSize, int cGoal, int initialQuality, int threads, int normalLock, int cStrat, int population, int useConditionals, double selectionPressure)
{
	craft::sequenceType seed;
	seed.reserve(initialSequenceSize);

	for (int i = 0; i < initialSequenceSize; ++i)
		seed.push_back(cActionToATAction(initialSequence[i]));

	crafterStats craft;
	craft.level = crafter.level;
	craft.cLevel = crafter.cLevel;
	craft.craftsmanship = crafter.craftsmanship;
	craft.control = crafter.control;
	craft.CP = crafter.CP;

	recipeStats rec;
	rec.rLevel = recipe.rLevel;
	rec.difficulty = recipe.difficulty;
	rec.quality = recipe.quality;
	rec.nominalQuality = recipe.displayedQuality;
	rec.durability = recipe.durability;
	rec.progressFactor = recipe.progressFactor;
	rec.qualityFactor = recipe.qualityFactor;

	for (unsigned int i = 0; i + 1 < recipe.pointsSize; i += 2)
		rec.points.emplace_back(recipe.points[i], recipe.points[i + 1]);

	sort(rec.points.begin(), rec.points.end(), [](const pair<int, int>& a, const pair<int, int>& b)
		{ return a.second < b.second; });

	goalType solveGoal;
	switch (cGoal)
	{
	case AT_GOAL_HQ:
		solveGoal = goalType::hq;
		break;
	case AT_GOAL_MAXQUALITY:
		solveGoal = goalType::maxQuality;
		break;
	case AT_GOAL_COLLECTABILITY:
		solveGoal = goalType::collectability;
		break;
	case AT_GOAL_POINTS:
		solveGoal = goalType::points;
		break;
	default: return nullptr;
	}

	strategy strat;
	switch (cStrat)
	{
	case AT_STRATEGY_STANDARD:
		strat = strategy::standard;
		break;
	case AT_STRATEGY_HQORBUST:
		strat = strategy::hqOrBust;
		break;
	case AT_STRATEGY_NQONLY:
		strat = strategy::nqOnly;
		break;
	default: return nullptr;
	}

	bool nLock = normalLock != 0;
	bool uC = useConditionals != 0;

	return reinterpret_cast<atSolver*>(new solver(craft, rec, seed, solveGoal, initialQuality, threads, nLock, strat, population, uC, false, selectionPressure));
}

int atLoadRecipeTable(const char* filename)
{
	return populateRecipeTable(filename) ? 0 : 1;
}

atSolverCallback cCallback;

bool cSolverCallback(int generations, int currentGeneration, int simsPerTrial, goalType goal, strategy strat, solver::trial status, int uniquePopulation, int cacheHits)
{
	if (!cCallback) return true;

	(void)generations;
	(void)simsPerTrial;
	(void)strat;
	(void)uniquePopulation;
	(void)cacheHits;

	atSolverResult result;

	populateResultSequence(&result, status.sequence);

	result.successes = status.outcome.successes;
	switch (goal)
	{
	case goalType::hq:
		result.hqPercent = status.outcome.hqPercent;
		break;
	case goalType::maxQuality:
		result.quality = status.outcome.quality;
		break;
	case goalType::collectability:
		result.collectableHit = status.outcome.collectableGoalsHit;
		break;
	case goalType::points:
		result.points = status.outcome.points;
		break;
	}

	return cCallback(currentGeneration, result) == 0;
}

atSolverResult atExecuteSimulations(atSolver* cSolver, int numberOfSimulations)
{
	solver* solve = reinterpret_cast<solver*>(cSolver);

	atSolverResult result;
	solver::trial simResult = solve->executeMultisim(numberOfSimulations);

	populateResultSequence(&result, simResult.sequence);

	result.successes = simResult.outcome.successes;
	switch (solve->getGoal())
	{
	case goalType::hq:
		result.hqPercent = simResult.outcome.hqPercent;
		break;
	case goalType::maxQuality:
		result.quality = simResult.outcome.quality;
		break;
	case goalType::collectability:
		result.collectableHit = simResult.outcome.collectableGoalsHit;
		break;
	case goalType::points:
		result.points = simResult.outcome.points;
		break;
	}

	return result;
}

atSolverResult atExecuteSolve(atSolver* cSolver, int simulationsPerSequence, int generations, int maxCacheSize, atSolverCallback callback)
{
	solver* solve = reinterpret_cast<solver*>(cSolver);
	atSolverResult result;

	cCallback = callback;

	solver::trial solveResult = solve->executeSolver(simulationsPerSequence, generations, 0, 0, maxCacheSize, cSolverCallback);

	populateResultSequence(&result, solveResult.sequence);
	
	result.successes = solveResult.outcome.successes;
	switch (solve->getGoal())
	{
	case goalType::hq:
		result.hqPercent = solveResult.outcome.hqPercent;
		break;
	case goalType::maxQuality:
		result.quality = solveResult.outcome.quality;
		break;
	case goalType::collectability:
		result.collectableHit = solveResult.outcome.collectableGoalsHit;
		break;
	case goalType::points:
		result.points = solveResult.outcome.points;
		break;
	}

	return result;
}

void atDeinitSolver(atSolver* solve)
{
	delete reinterpret_cast<solver*>(solve);
}
