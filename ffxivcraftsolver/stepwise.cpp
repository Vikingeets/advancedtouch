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
		int i = stoi(str);
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
		cout << "Unknown action: " << command[1] << "\n";
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
		cout << command[1] << " must be set to at least " << (it->first == actions::nameOfTheElements? "-1" : "0");
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
		if (static_cast<int>(seed.size()) >= crafting->getStep() - 1) seed[crafting->getStep() - 2] = action;
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
		cout << "Unknown action: " << command[1] << "\n";
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

bool setCondition(const vector<string>& command, const recipeStats recipe, craft* crafting)
{
	if (command.size() == 1)
	{
		{
			if (recipe.expert)
				cout << "Condition required: g, n, c, s, or p\n";
			else
				cout << "Condition required: e, g, n, or p\n";
			return false;
		}
	}
	if (command[1] == "e" && !recipe.expert)
		crafting->setCondition(craft::condition::excellent);
	else if (command[1] == "g")
		crafting->setCondition(craft::condition::good);
	else if (command[1] == "n")
		crafting->setCondition(craft::condition::normal);
	else if (command[1] == "p")
		crafting->setCondition(recipe.expert ? craft::condition::pliant : craft::condition::poor);
	else if (command[1] == "c" && recipe.expert)
		crafting->setCondition(craft::condition::centered);
	else if (command[1] == "s" && recipe.expert)
		crafting->setCondition(craft::condition::sturdy);
	else
	{
		if (recipe.expert)
			cout << "Condition required: g, n, c, s, or p\n";
		else
			cout << "Condition required: e, g, n, or p\n";
		return false;
	}
	return true;
}

bool stepwiseUpdate(int generations, int currentGeneration, int simsPerTrial, goalType goal, strategy strat, solver::trial status, int uniquePopulation, int cacheHits)
{
	(void)uniquePopulation;
	(void)cacheHits;
	if (termFlag) return false;

	const chrono::milliseconds updateDelay(500ms);
	static chrono::time_point<chrono::steady_clock> stepwiseNextUpdate(chrono::steady_clock::now() + updateDelay);
	if (chrono::steady_clock::now() < stepwiseNextUpdate) return true;

	cout << currentGeneration + 1 << "/" << generations;
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
		case goalType::points:
			cout << ", " << status.outcome.points / simsPerTrial << " average points";
			break;
		}
	}

#ifdef _DEBUG
	
	if(!status.sequence.empty())
		cout << " " << simpleText.at(status.sequence.front());

#endif

	cout << endl;

	stepwiseNextUpdate = chrono::steady_clock::now() + updateDelay;

	return true;
}

actions doSolve(
	const vector<string>& command,
	solver* solve,
	int* lastSolvedStep,
	craft::sequenceType* seed,
	const craft& iS,
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

	solve->setInitialState(iS);
	solve->incrementSeeds(iS.getStep() - *lastSolvedStep);

	signal(SIGINT, handler);

	craft::sequenceType result = solve->executeSolver(simsPerTrial, generations * generationMultiplier, generations, generations * streakTolerance / 100, maxCacheSize, stepwiseUpdate).sequence;

	termFlag = 0;
	signal(SIGINT, SIG_DFL);

	if (result.empty())
	{
		cout << "The solver was unable to find an action.";
		return actions::invalid;
	}
	seed->resize(min(static_cast<int>(seed->size()), iS.getStep() - 1));
	seed->insert(seed->end(), result.cbegin(), result.cend());

	actions suggestion = result.front();
	cout << "Suggested action: " << simpleText.at(suggestion) << "\n\n";

	*lastSolvedStep = iS.getStep();

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
	randomGenerator rand;
	stack<craft> craftHistory;
	craftHistory.emplace(initialQuality, crafter, recipe, false);
	
	craftHistory.top().setRNG(&rand);

	solver solve(crafter, recipe, seed, goal, craftHistory.top(), threads, strat, population);
	int lastSolvedStep = 0;

	bool printStatus = true;		// Generally doubles as a success/fail flag
	actions lastSuggested = actions::invalid;

	// Holds an entire sequence, start to end. Modified as actions happen.
	craft::sequenceType currentSeed = seed;

	while (true)
	{
		assert(!craftHistory.empty());
		if (printStatus)
		{
			cout << "Step " << craftHistory.top().getStep() << '\n';
			cout << craftHistory.top().getState();
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
			}
			else
			{
				craftHistory.push(craftHistory.top());
				craftHistory.top().setStep(step);
			}
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
			}
			else
			{
				craftHistory.push(craftHistory.top());
				craftHistory.top().setProgress(progress);
			}
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
			}
			else
			{
				craftHistory.push(craftHistory.top());
				craftHistory.top().setQuality(quality);
			}
		}
		else if (command[0] == "durability" || command[0] == "d")
		{
			if (command.size() == 1)
			{
				cout << "Durabilty required\n";
				printStatus = false;
				continue;
			}
			int durability = stringToInt(command[1], 1);
			if (durability == invalidInt)
			{
				cout << "Durability must be at least 1";
				printStatus = false;
			}
			else
			{
				craftHistory.push(craftHistory.top());
				craftHistory.top().setDurability(durability);
			}
		}
		else if (command[0] == "condition" || command[0] == "c")
		{
			craftHistory.push(craftHistory.top());
			printStatus = setCondition(command, recipe, &craftHistory.top());
			if (!printStatus) craftHistory.pop();
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
			}
			else
			{
				craftHistory.push(craftHistory.top());
				craftHistory.top().setCP(CP);
			}
		}
		else if (command[0] == "buff")
		{
			craftHistory.push(craftHistory.top());
			printStatus = applyBuff(commandOrig, &craftHistory.top());
			if (!printStatus) craftHistory.pop();
		}
		else if (command[0] == "action" || command[0] == "ac")
		{
			craftHistory.push(craftHistory.top());
			printStatus = doAction(commandOrig, &craftHistory.top(), currentSeed);
			if (!printStatus) craftHistory.pop();
			else lastSuggested = actions::invalid;
		}
		else if (command[0] == "solve" || command[0] == "so")
		{
			actions suggestion = doSolve(commandOrig, &solve, &lastSolvedStep, &currentSeed, craftHistory.top(), simsPerSequence, stepwiseGenerations, maxCacheSize);
			if (suggestion != actions::invalid) lastSuggested = suggestion;
			printStatus = suggestion != actions::invalid;
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
			craftHistory.push(craftHistory.top());
			printStatus = doAction(lastSuggested, over, &craftHistory.top(), currentSeed);
			if (!printStatus) craftHistory.pop();
			else lastSuggested = actions::invalid;
		}
		else if (command[0] == "next" || command[0] == "n")
		{
			if (lastSuggested == actions::invalid)
			{
				cout << "No action has been suggested for this step. Please execute the solver.\n";
				printStatus = false;
				continue;
			}
			craftHistory.push(craftHistory.top());
			printStatus = doAction(lastSuggested, craft::rngOverride::success, &craftHistory.top(), currentSeed);
			if (!printStatus)
			{
				craftHistory.pop();
				continue;
			}

			if (command.size() == 1 && craftHistory.top().getCondition() != craft::condition::poor) command.push_back("n");
			setCondition(command, recipe, &craftHistory.top());
			
			cout << "Step " << craftHistory.top().getStep() << '\n';
			cout << craftHistory.top().getState() << endl;

			lastSuggested = doSolve(vector<string>(), &solve, &lastSolvedStep, &currentSeed, craftHistory.top(), simsPerSequence, stepwiseGenerations, maxCacheSize);
		}
		else if (command[0] == "undo" || command[0] == "u")
		{
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
		}
		else if (command[0] == "reset")
		{
			while (craftHistory.size() > 1)
				craftHistory.pop();
			lastSuggested = actions::invalid;
		}
		else if (command[0] == "exit" || command[0] == "quit")
		{
			return 0;
		}
		else
		{
			cout << "Unknown command " << command[0] << '\n';
			printStatus = false;
		}
	}
}
