#include "common.h"
#include "craft.h"
#include "solver.h"
#include "stepwise.h"

#include <cassert>
#include <csignal>
#include <string>
#include <vector>
#include <stack>
#include <iostream>

using namespace std;

constexpr int generationMultiplier = 5;
constexpr int streakTolerance = 90;	// percent

constexpr int invalidInt = numeric_limits<int>::min();

extern volatile sig_atomic_t termFlag;

extern "C" void handler(int sig);

int stringToInt(const string& str, int minimum)
{
	try
	{
		int i = static_cast<int>(stoul(str));
		if (i < minimum) return invalidInt;
		else return i;
	}
	catch (logic_error)
	{
		return invalidInt;
	}
}

bool isBuff(actions action)
{
	switch (action)
	{
	case actions::muscleMemory:
	case actions::wasteNot:
	case actions::wasteNot2:
	case actions::manipulation:
	case actions::innerQuiet:
	case actions::reflect:
	case actions::greatStrides:
	case actions::veneration:
	case actions::innovation:
	case actions::nameOfTheElements:
	case actions::finalAppraisal:
	case actions::observe:
		return true;
	default:
		return false;
	}
}

bool applyBuff(const vector<string>& command, craft* crafting)
{
	if (command.size() == 1)
	{
		cout << "Possible buffs:\n";
		for (auto it = simpleText.begin(); it != simpleText.end(); ++it)
			if (isBuff(it->first)) cout << '\t' << it->second << '\n';
		return false;
	}
	const string& buffName = lowercase(command[1]);
	auto it = find_if(simpleText.begin(), simpleText.end(),
		[&buffName](const pair<actions, string>& p){return buffName == lowercase(p.second); });
	if (it == simpleText.end())
	{
		cout << "Unknown action:" << command[1] << "\n";
		return false;
	}
	if (!isBuff(it->first))
	{
		cout << command[1] << " is not a buff";
		return false;
	}
	if (command.size() == 2)
	{
		if (it->first == actions::innerQuiet)
			cout << command[1] << " requires a stack amount\n";
		else if (it->first == actions::observe)
			cout << command[1] << " requires 0 to disable or 1 to enable\n";
		else
			cout << command[1] << " requires a duration\n";
		return false;
	}
	int duration = stringToInt(command[2], it->first == actions::nameOfTheElements ? -1 : 0);
	if (duration == invalidInt)
	{
		cout << command[2] << " must be set to at least " << (it->first == actions::nameOfTheElements? "-1" : "0");
		return false;
	}
	crafting->setBuff(it->first, duration);
	return true;
}

bool doAction(actions action, craft::rngOverride over, craft* crafting, craft::sequenceType& seed)
{
	cout << "Performing " << simpleText.at(action) << ":";
	craft::actionResult result = crafting->performOneComplete(action, over);
	switch (result)
	{
	case craft::actionResult::success:
		cout << " success\n\n";
		break;
	case craft::actionResult::failRNG:
		cout << " failed roll\n\n";
		break;
	case craft::actionResult::failNoCP:
		cout << " insufficient CP\n\n";
		break;
	case craft::actionResult::failHardUnavailable:
	case craft::actionResult::failSoftUnavailable:
		cout << " not available\n\n";
		break;
	}
	bool output = result == craft::actionResult::success || result == craft::actionResult::failRNG;
	if (output && action != actions::finalAppraisal)
	{
		if (seed.size() >= crafting->getStep() - 1) seed[crafting->getStep() - 2] = action;
		else seed.push_back(action);
	}

	return output;
}

bool doAction(const vector<string>& command, craft* crafting, craft::sequenceType& seed)
{
	if (command.size() == 1)
	{
		cout << "Possible actions:\n";
		for (auto it = simpleText.begin(); it != simpleText.end(); ++it)
			cout << '\t' << it->second << '\n';
		return false;
	}

	if (crafting->maxedProgress())
	{
		cout << "The craft is complete. Either undo, reset, quit, or lower progress.\n";
		return false;
	}
	else if (crafting->outOfDurability())
	{
		cout << "The craft has failed. Either undo, reset, quit, or raise durability.\n";
		return false;
	}

	const string& actionName = lowercase(command[1]);
	auto it = find_if(simpleText.begin(), simpleText.end(),
		[&actionName](const pair<actions, string>& p) {return actionName == lowercase(p.second); });
	if (it == simpleText.end())
	{
		cout << "Unknown action:" << command[1] << "\n";
		return false;
	}
	craft::rngOverride over;
	if (command.size() == 2 || command[2] == "?") over = craft::rngOverride::random;
	else if (command[2] == "s") over = craft::rngOverride::success;
	else if (command[2] == "f") over = craft::rngOverride::failure;
	else
	{
		cout << "Unknown success/failure. Set s, f, or ?" << '\n';
		return false;
	}
	return doAction(it->first, over, crafting, seed);
}

bool stepwiseUpdate(int generations, int currentGeneration, int simsPerTrial, goalType goal, strategy strat, solver::trial status, int uniquePopulation, int cacheHits)
{
	(void)uniquePopulation;
	(void)cacheHits;
	if (termFlag) return false;

	const chrono::milliseconds updateDelay(500ms);
	static chrono::time_point<chrono::steady_clock> stepwiseNextUpdate(chrono::steady_clock::now() + updateDelay);
	if (chrono::steady_clock::now() < stepwiseNextUpdate) return true;

	cout << currentGeneration << "/" << generations;
	if (strat != strategy::nqOnly)
	{
		switch (goal)
		{
		case goalType::hq:
			cout << ", " << status.outcome.hqPercent / simsPerTrial << "% HQ";
			break;
		case goalType::maxQuality:
			cout << ", " << status.outcome.quality / simsPerTrial << " quality";
			break;
		case goalType::collectability:
			cout << ", " << status.outcome.collectableGoalsHit * 100 / simsPerTrial << "% hit goal";
			break;
		}
	}

#ifdef _DEBUG
	
	cout << " " << simpleText.at(status.sequence.front());

#endif

	cout << endl;

	stepwiseNextUpdate = chrono::steady_clock::now() + updateDelay;

	return true;
}

actions doSolve(
	const vector<string>& command,
	const crafterStats& c,
	const recipeStats& r,
	craft::sequenceType* seed,
	goalType g,
	const craft& iS,
	int tCnt,
	strategy s,
	int population,
	int simsPerTrial,
	int generations,
	int maxCacheSize
)
{
	if (iS.maxedProgress())
	{
		cout << "The craft is complete. Either undo, reset, quit, or lower progress.\n";
		return actions::invalid;
	}
	else if (iS.outOfDurability())
	{
		cout << "The craft has failed. Either undo, reset, quit, or raise durability.\n";
		return actions::invalid;
	}
	if (command.size() > 1)
	{
		generations = stringToInt(command[1], 1);
		if (generations == invalidInt)
		{
			cout << "You must set at least 1 generation, or omit to use the value in the options file.\n";
			return actions::invalid;
		}
	}

	int partialIncrement = iS.getStep() - 1;
	if (partialIncrement > seed->size()) partialIncrement = seed->size();
	craft::sequenceType partialSeed(next(seed->begin(), partialIncrement), seed->end());

	signal(SIGINT, handler);

	solver solve(c, r, partialSeed, g, iS, tCnt, s, population);

	craft::sequenceType result = solve.executeSolver(simsPerTrial, generations * generationMultiplier, generations, generations * streakTolerance / 100, maxCacheSize, stepwiseUpdate).sequence;

	termFlag = 0;
	signal(SIGINT, SIG_DFL);

	if (result.empty())
	{
		// Shouldn't Happen
		cout << "The solver was unable to find an action.";
		return actions::invalid;
	}
	seed->resize(min(static_cast<int>(seed->size()), partialIncrement));
	seed->insert(seed->end(), result.cbegin(), result.cend());

	actions suggestion = result.front();
	cout << "Suggested action: " << simpleText.at(suggestion) << "\n\n";

	return suggestion;
}

int performStepwise(
	const crafterStats& crafter,
	const recipeStats& recipe,
	const craft::sequenceType& seed,
	goalType goal,
	int initialQuality,
	int threads,
	int simsPerSequence,
	int stepwiseGenerations,
	int population,
	int maxCacheSize,
	strategy strat
	)
{
	craft currentStatus = craft(initialQuality, crafter, recipe, false);
	random rand;
	currentStatus.setRNG(&rand);

	stack<craft> craftHistory;
	craftHistory.push(currentStatus);

	bool printStatus = true;
	actions lastSuggested = actions::invalid;

	// Holds an entire sequence, start to end. Modified as actions happen.
	craft::sequenceType currentSeed = seed;

	while (true)
	{
		if (printStatus)
		{
			cout << "Step " << currentStatus.getStep() << '\n';
			cout << currentStatus.getState();
		}
		cout << endl;
		printStatus = true;

		string commandString;
		cout << "> ";
		getline(cin, commandString);

		vector<string> commandOrig = tokenSplit(commandString, ' ');
		if (commandOrig.empty())
		{
			printStatus = false;
			continue;
		}
		vector<string> command = commandOrig;
		for (auto& cmd : command)
			cmd = lowercase(cmd);

		if (command[0] == "step" || command[0] == "s")
		{
			if (command.size() == 1)
			{
				cout << "Step number required\n";
				printStatus = false;
				continue;
			}
			int step = stringToInt(command[1], 1);
			if (step == invalidInt)
			{
				cout << "Step must be at least 1";
				printStatus = false;
				continue;
			}
			else currentStatus.setStep(step);
		}
		else if (command[0] == "progress" || command[0] == "p")
		{
			if (command.size() == 1)
			{
				cout << "Progress amount required\n";
				printStatus = false;
				continue;
			}
			int progress = stringToInt(command[1], 0);
			if (progress == invalidInt)
			{
				cout << "Progress must be at least 0";
				printStatus = false;
				continue;
			}
			else currentStatus.setProgress(progress);
		}
		else if (command[0] == "quality" || command[0] == "q")
		{
			if (command.size() == 1)
			{
				cout << "Quality amount required\n";
				printStatus = false;
				continue;
			}
			int quality = stringToInt(command[1], 0);
			if (quality == invalidInt)
			{
				cout << "Quality must be at least 0";
				printStatus = false;
				continue;
			}
			else currentStatus.setQuality(quality);
		}
		else if (command[0] == "durability" || command[0] == "d")
		{
			if (command.size() == 1)
			{
				cout << "Durabilty required\n";
				printStatus = false;
				continue;
			}
			int durability = stringToInt(command[1], -15);
			if (durability == invalidInt)
			{
				cout << "Durability must be at least -15";
				printStatus = false;
				continue;
			}
			else currentStatus.setDurability(durability);
		}
		else if (command[0] == "condition" || command[0] == "c")
		{
			if (command.size() == 1)
			{
				cout << "Condition required: e, g, n, or p\n";
				printStatus = false;
				continue;
			}
			if (command[1] == "e" && !recipe.expert)
				currentStatus.setCondition(craft::condition::excellent);
			else if (command[1] == "g")
				currentStatus.setCondition(craft::condition::good);
			else if (command[1] == "n")
				currentStatus.setCondition(craft::condition::normal);
			else if (command[1] == "p")
				currentStatus.setCondition(recipe.expert ? craft::condition::pliant : craft::condition::poor);
			else if (command[1] == "c" && recipe.expert)
				currentStatus.setCondition(craft::condition::centered);
			else if (command[1] == "s" && recipe.expert)
				currentStatus.setCondition(craft::condition::sturdy);
			else
			{
				if (recipe.expert)
					cout << "Condition required: g, n, c, s, or p\n";
				else
					cout << "Condition required: e, g, n, or p\n";
				printStatus = false;
				continue;
			}
		}
		else if (command[0] == "cp" || command[0] == "c")
		{
			if (command.size() == 1)
			{
				cout << "CP amount required\n";
				printStatus = false;
				continue;
			}
			int CP = stringToInt(command[1], 0);
			if (CP == invalidInt)
			{
				cout << "CP must be at least 0";
				printStatus = false;
				continue;
			}
			else currentStatus.setCP(CP);
		}
		else if (command[0] == "buff")
		{
			printStatus = applyBuff(commandOrig, &currentStatus);
			if (!printStatus) continue;
		}
		else if (command[0] == "action" || command[0] == "ac")
		{
			printStatus = doAction(commandOrig, &currentStatus, currentSeed);
			if (!printStatus) continue;
			lastSuggested = actions::invalid;
		}
		else if (command[0] == "solve" || command[0] == "so")
		{

			actions suggestion = doSolve(commandOrig, crafter, recipe, &currentSeed, goal, currentStatus, threads, strat, population, simsPerSequence, stepwiseGenerations, maxCacheSize);
			if (suggestion != actions::invalid) lastSuggested = suggestion;
			printStatus = suggestion != actions::invalid;
			if (!printStatus) continue;
		}
		else if (command[0] == "suggested" || command[0] == "su")
		{
			if (lastSuggested == actions::invalid)
			{
				cout << "No action has been suggested for this step. Please execute the solver.\n";
				printStatus = false;
				continue;
			}
			craft::rngOverride over;
			if (command.size() == 1 || command[1] == "?") over = craft::rngOverride::random;
			else if (command[1] == "s") over = craft::rngOverride::success;
			else if (command[1] == "f") over = craft::rngOverride::failure;
			else
			{
				cout << "Unknown success/failure. Set s, f, or ?" << '\n';
				printStatus = false;
				continue;
			}
			printStatus = doAction(lastSuggested, over, &currentStatus, currentSeed);
			if (!printStatus) continue;
			lastSuggested = actions::invalid;
		}
		else if (command[0] == "undo" || command[0] == "u")
		{
			if (craftHistory.empty()) continue;	// Shouldn't Happen
			int amount;
			if (command.size() == 1) amount = 1;
			else
			{
				amount = stringToInt(command[1], 0);
				if (amount == invalidInt)
				{
					cout << "Undo amount must be at least 0.\n";
					printStatus = false;
					continue;
				}
			}
			for (int i = 0; i < amount; ++i)
			{
				if (craftHistory.size() == 1) break;
				craftHistory.pop();
			}
			currentStatus = craftHistory.top();
			continue;
		}
		else if (command[0] == "reset")
		{
			currentStatus = craft(initialQuality, crafter, recipe, false);
			currentStatus.setRNG(&rand);
			craftHistory = stack<craft>();
			craftHistory.push(currentStatus);
			lastSuggested = actions::invalid;
			// Don't reset the seed; it can help prime future solutions
			continue;
		}
		else if (command[0] == "exit" || command[0] == "quit")
		{
			return 0;
		}
		else
		{
			cout << "Unknown command " << command[0] << '\n';
			printStatus = false;
			continue;
		}

		craftHistory.push(currentStatus);
	}
}
