#include <stdio.h>
#include <string.h>

#include <iostream>
#include <algorithm>
#include <array>
#include <string>
#include <bitset>

#include <csignal>
#include <thread>

#include "common.h"
#include "levels.h"
#include "craft.h"
#include "solver.h"

#ifndef RAPIDJSON_HAS_STDSTRING
#define RAPIDJSON_HAS_STDSTRING 1
#endif // !RAPIDJSON_HAS_STDSTRING
#include "rapidjson/document.h"
#include "rapidjson/pointer.h"
#include "rapidjson/filereadstream.h"
#include "rapidjson/error/en.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"

using namespace std;

string programName;

std::mutex random::devicelock;

[[noreturn]] void usage();

volatile sig_atomic_t termFlag = 0;

extern "C" void handler(int sig)
{
	if (sig == SIGINT)
		termFlag = 1;
}

string lowercase(string makeLower)
{
	transform(makeLower.begin(), makeLower.end(), makeLower.begin(), tolower);
	return makeLower;
}

int stringToInt(const string& str, char letter, int minimum = 1)
{
	try
	{
		int i = static_cast<int>(stoul(str));
		if (i < minimum)
		{
			cerr << "argument to -" << letter << " must be at least " << minimum << endl;
			exit(1);
		}
		return i;
	}
	catch (logic_error)
	{
		cerr << "invalid argument to -" << letter << ": " << str << '\n';
		usage();
	}
}

bool openAndParseJSON(const string& filename, rapidjson::Document* d)
{
//	*d = rapidjson::Document();
	FILE* fp;
	errno_t error = fopen_s(&fp, filename.c_str(), "rb");

	if (fp == nullptr)
	{
		constexpr size_t bufsize = 95;
		array<char, bufsize> buffer;

		strerror_s(buffer.data(), bufsize, error);
		cerr << "Failed to open " << filename << ": " << buffer.data();
		exit(1);
	}
	else try {
		constexpr size_t bufsize = 8192;
		array<char, bufsize> buffer;

		rapidjson::FileReadStream is(fp, buffer.data(), bufsize);

		d->ParseStream(is);

		fclose(fp);
		return !d->HasParseError();
	}
	catch (...)
	{
		fclose(fp);
		throw;
	}
}

string sequenceToString(craft::sequenceType& seq)
{
	rapidjson::Document d;
	auto& dAlloc = d.GetAllocator();

	d.SetArray();
	d.Reserve(static_cast<rapidjson::SizeType>(seq.size()), dAlloc);
	for (auto s : seq)
	{
		rapidjson::Value v;
		string str = simpleText.at(s);
		d.PushBack(rapidjson::Value{}.SetString(str.c_str(), static_cast<rapidjson::SizeType>(str.length()), dAlloc), dAlloc);
	}

	rapidjson::StringBuffer buf;
	rapidjson::Writer<rapidjson::StringBuffer> writer(buf);
	d.Accept(writer);

	return string(buf.GetString());
}

string getStringIfExists(const rapidjson::Document&d, const char* key, string def = string())
{
	const rapidjson::Value* ptr = rapidjson::Pointer(key).Get(d);
	if (ptr && ptr->IsString()) return ptr->GetString();
	else return def;
}

int getIntIfExists(const rapidjson::Document& d, const char* key, int def = 0)
{
	const rapidjson::Value* ptr = rapidjson::Pointer(key).Get(d);
	if (ptr && ptr->IsInt()) return ptr->GetInt();
	else return def;
}

bool getBoolIfExists(const rapidjson::Document& d, const char* key, bool def = false)
{
	const rapidjson::Value* ptr = rapidjson::Pointer(key).Get(d);
	if (ptr && ptr->IsBool()) return ptr->GetBool();
	else return def;
}

void parseStats(const rapidjson::Document& d, crafterStats* crafter, bool useFood, bool useMedicine)
{
	if (d.HasParseError())
	{
		cerr << "failed to parse stats" << endl;
		exit(1);
	}
	const rapidjson::Value* foodPtr = useFood ? rapidjson::Pointer("/food").Get(d) : nullptr;
	const rapidjson::Value* medPtr = useMedicine ? rapidjson::Pointer("/medicine").Get(d) : nullptr;

	int foodCPPercent = 0;
	int foodCPMax = 0;
	int foodControlPercent = 0;
	int foodControlMax = 0;
	int foodCraftsmanshipPercent = 0;
	int foodCraftsmanshipMax = 0;

	if(foodPtr && foodPtr->IsObject())
	{
		foodCPPercent = getIntIfExists(d, "/food/cp percent");
		foodCPMax = getIntIfExists(d, "/food/cp max");
		foodControlPercent = getIntIfExists(d, "/food/control percent");
		foodControlMax = getIntIfExists(d, "/food/control max");
		foodCraftsmanshipPercent = getIntIfExists(d, "/food/craftsmanship percent");
		foodCraftsmanshipMax = getIntIfExists(d, "/food/craftsmanship max");
	}

	int medCPPercent = 0;
	int medCPMax = 0;
	int medControlPercent = 0;
	int medControlMax = 0;
	int medCraftsmanshipPercent = 0;
	int medCraftsmanshipMax = 0;

	if (medPtr && medPtr->IsObject())
	{
		medCPPercent = getIntIfExists(d, "/medicine/cp percent");
		medCPMax = getIntIfExists(d, "/medicine/cp max");
		medControlPercent = getIntIfExists(d, "/medicine/control percent");
		medControlMax = getIntIfExists(d, "/medicine/control max");
		medCraftsmanshipPercent = getIntIfExists(d, "/medicine/craftsmanship percent");
		medCraftsmanshipMax = getIntIfExists(d, "/medicine/craftsmanship max");
	}

	string prefix;
	switch (crafter->classKind)
	{
	case classes::CRP:
		prefix = "/crp/";
		break;
	case classes::BSM:
		prefix = "/bsm/";
		break;
	case classes::ARM:
		prefix = "/arm/";
		break;
	case classes::GSM:
		prefix = "/gsm/";
		break;
	case classes::LTW:
		prefix = "/ltw/";
		break;
	case classes::WVR:
		prefix = "/wvr/";
		break;
	case classes::ALC:
		prefix = "/alc/";
		break;
	case classes::CUL:
		prefix = "/cul/";
		break;
	default:
		assert(false);
	}

	crafter->level = getIntIfExists(d, (prefix + "level").c_str());
	crafter->craftsmanship = getIntIfExists(d, (prefix + "craftsmanship").c_str());
	crafter->control = getIntIfExists(d, (prefix + "control").c_str());
	crafter->CP = getIntIfExists(d, (prefix + "cp").c_str(), -1);

	bitset<4> missingStats;
	missingStats[0] = crafter->level <= 0 || crafter->level > 80;
	missingStats[1] = crafter->craftsmanship <= 0;
	missingStats[2] = crafter->control <= 0;
	missingStats[3] = crafter->CP <= -1;		// they want to try zero CP they're welcome to

	if (missingStats.any())
	{
		cerr << "the selected class has missing or invalid stats:";
		if (missingStats[0]) cerr << " level";
		if (missingStats[1]) cerr << " craftsmanship";
		if (missingStats[2]) cerr << " control";
		if (missingStats[3]) cerr << " cp";
		cerr << endl;
		exit(1);
	}

	crafter->rLevel = mainToRlvl(crafter->level);

	crafter->craftsmanship += min((crafter->craftsmanship * foodCraftsmanshipPercent) / 100, foodCraftsmanshipMax);
	crafter->control += min((crafter->control * foodControlPercent) / 100, foodControlMax);
	crafter->CP += min((crafter->CP * foodCPPercent) / 100, foodCPMax);

	crafter->craftsmanship += min((crafter->craftsmanship * medCraftsmanshipPercent) / 100, medCraftsmanshipMax);
	crafter->control += min((crafter->control * medControlPercent) / 100, medControlMax);
	crafter->CP += min((crafter->CP * medCPPercent) / 100, medCPMax);

	return;
}

void parseRecipe(const rapidjson::Document& d, recipeStats* recipe)
{
	if (d.HasParseError())
	{
		cerr << "failed to parse recipe" << endl;
		exit(1);
	}
	recipe->rLevel = getIntIfExists(d, "/rlevel");
	recipe->difficulty = getIntIfExists(d, "/difficulty");
	recipe->quality = getIntIfExists(d, "/quality");
	recipe->nominalQuality = recipe->quality;
	recipe->durability = getIntIfExists(d, "/durability");

	bitset<4> missingStats;
	missingStats[0] = recipe->rLevel <= 0;
	missingStats[1] = recipe->difficulty <= 0;
	missingStats[2] = recipe->quality <= 0;
	missingStats[3] = recipe->durability <= 0;

	if (missingStats.any())
	{
		cerr << "the recipe has missing or invalid stats:";
		if (missingStats[0]) cerr << " rlevel";
		if (missingStats[1]) cerr << " difficulty";
		if (missingStats[2]) cerr << " quality";
		if (missingStats[3]) cerr << " durability";
		cerr << endl;
		exit(1);
	}

	populateRecipeTable();
	populateDifferenceTable();

	recipe->suggestedCraftsmanship = getSuggestedCraftsmanship(recipe->rLevel);
	recipe->suggestedControl = getSuggestedControl(recipe->rLevel);

	return;
}

struct options
{
	int simsPerSequence;
	int generations;
	int population;
	int maxCacheSize;

	bool normalLock;
	int threads;

	bool useTricks;
};

void parseOptions(const rapidjson::Document& d, options* opts, bool solveMode)
{
	if (d.HasParseError())
	{
		cerr << "failed to parse options" << endl;
		exit(1);
	}

	opts->simsPerSequence = getIntIfExists(d, "/sims");
	opts->generations = getIntIfExists(d, "/generations");
	opts->population = getIntIfExists(d, "/population");
	opts->maxCacheSize = getIntIfExists(d, "/max cache size");
	
	opts->normalLock = getBoolIfExists(d, "/normal lock");
	opts->threads = getIntIfExists(d, "/threads");
	opts->useTricks = getBoolIfExists(d, "/use tricks");

	bitset<3> missingStats;
	missingStats[0] = opts->simsPerSequence <= 0;
	missingStats[1] = solveMode && opts->generations <= 0;
	missingStats[2] = solveMode && (opts->population <= 0 || opts->population % 2 == 1);
	if (missingStats.any())
	{
		cerr << "the options has missing or invalid stats:";
		if (missingStats[0]) cerr << " sims";
		if (missingStats[1]) cerr << " generations";
		if (missingStats[2]) cerr << " population";
		cerr << endl;
		exit(1);
	}

	if (opts->threads <= 0) opts->threads = max(1U, thread::hardware_concurrency());

	return;
}

craft::sequenceType parseSequence(const rapidjson::Document& d)
{
	if (d.HasParseError())
	{
		cerr << "failed to parse sequence" << endl;
		exit(1);
	}

	if (!d.IsArray())
	{
		cerr << "sequence file does not contain a JSON array" << endl;
		exit(1);
	}

	craft::sequenceType sequence;

	for (auto& actionString : d.GetArray())
	{
		if (!actionString.IsString())
		{
			cerr << "non-string found in sequence file" << endl;
			exit(1);
		}
		auto it = find_if(simpleText.begin(), simpleText.end(),
			[&actionString](const pair<actions, string>& p) {return actionString == p.second;});

		if (it != simpleText.end())
			sequence.push_back(it->first);
		else
		{
			cerr << "unknown action found in sequence: " << actionString.GetString() << endl;
			exit(1);
		}
	}

	return sequence;
}

int performSingle(const crafterStats& crafter, const recipeStats& recipe,
 const craft::sequenceType& sequence,
	goalType goal,
 int initialQuality,
	bool normalLock)
{
	random rng;
	craft synth(initialQuality, crafter, recipe, normalLock, rng);

	craft::endResult result = synth.performAll(sequence, goal, true);

	cout << "Progress: " << result.progress << "/" << recipe.difficulty << " (" << (result.progress >= recipe.difficulty ? "success" : "failure") << ")\n";
	switch (goal)
	{
	case goalType::quality:
		cout << result.hqPercent << "% HQ chance\n";
		break;
	case goalType::maxQuality:
		cout << "Quality: " << result.quality << '\n';
		break;
	case goalType::collectability:
		cout << "Collectability goal " << (result.collectableHit ? "hit" : "missed") << '\n';
		break;
	}
	cout << result.steps << " steps taken, " << result.invalidActions << " action" << (result.invalidActions == 1 ? "" : "s") << " invalid" << endl;

	return 0;
}

int performMulti(const crafterStats& crafter, const recipeStats& recipe,
 const craft::sequenceType& sequence,
	goalType goal,
 int initialQuality,
	bool normalLock,
 int threads,
	int simsPerSequence)
{
	solver solve(crafter, recipe, sequence, goal, initialQuality, threads, normalLock);

	solver::netResult result = solve.executeMultisim(simsPerSequence);

	cout << result.successes << " completed (" << (result.successes * 100) / simsPerSequence << "%)\n";
	switch (goal)
	{
	case goalType::quality:
		cout << "Average HQ: " << result.hqPercent / simsPerSequence << "%\n";
		break;
	case goalType::maxQuality:
		cout << "Average Quality: " << result.quality / simsPerSequence << '\n';
		break;
	case goalType::collectability:
		cout << result.collectableGoalsHit << " collectables reached goal (" << (result.collectableGoalsHit * 100) / simsPerSequence << "%)\n";
		break;
	}
	cout << result.steps / simsPerSequence << " average step" << (result.steps == 1 ? "" : "s") << ", " << result.invalidActions / simsPerSequence << " average invalid actions\n";

	return 0;
}

bool solveUpdate(int generations, int currentGeneration, int simsPerTrial, goalType goal, strategy strat, solver::trial status, int uniquePopulation, int cacheHits)
{
	if (termFlag) return false;

	const chrono::milliseconds updateDelay(500ms);
	static chrono::time_point<chrono::steady_clock> nextUpdate(chrono::steady_clock::now() + updateDelay);
	if (chrono::steady_clock::now() < nextUpdate) return true;

	cout << "Generation " << currentGeneration << "/" << generations << ", " <<
		status.outcome.successes * 100 / simsPerTrial <<
		"% successes, ";
	if (strat != strategy::nqOnly)
	{
		switch (goal)
		{
		case goalType::quality:
			cout << status.outcome.hqPercent / simsPerTrial << "% HQ, ";
			break;
		case goalType::maxQuality:
			cout << status.outcome.quality / simsPerTrial << " quality, ";
			break;
		case goalType::collectability:
			cout << status.outcome.collectableGoalsHit * 100 / simsPerTrial << "% hit goal, ";
			break;
		default:
			break;
		}
	}

	cout << status.sequence.size() << " step" << (status.sequence.size() != 1 ? "s" : "");
	if (uniquePopulation > 0)
		cout << ", " << uniquePopulation << " unique";
	if (cacheHits > 0)
		cout << ", " << cacheHits << " cache hits";

	cout << endl;

	nextUpdate = chrono::steady_clock::now() + updateDelay;

	return true;
}

int performSolve(const crafterStats& crafter,
 const recipeStats& recipe,

	const craft::sequenceType& sequence,

	goalType goal,

	int initialQuality,

	bool normalLock,

	int threads,

	int simsPerSequence,

	int generations,

	int population,

	int maxCacheSize,

	strategy strat,
	bool useTricks,
	bool gatherStats)
{
	solver solve(crafter, recipe, sequence, goal, initialQuality, threads, normalLock,
		strat, population, useTricks, gatherStats);
	
	solver::trial result = solve.executeSolver(simsPerSequence, generations, maxCacheSize, solveUpdate);
	solver::netResult outcome = result.outcome;

	cout << '\n' << outcome.successes << " completed (" << (outcome.successes * 100) / simsPerSequence << "%)\n";
	switch (goal)
	{
	case goalType::quality:
		cout << "Average HQ: " << outcome.hqPercent / simsPerSequence << "%\n";
		break;
	case goalType::maxQuality:
		cout << "Average Quality: " << outcome.quality / simsPerSequence << '\n';
		break;
	case goalType::collectability:
		cout << outcome.collectableGoalsHit << " collectables reached goal (" << (outcome.collectableGoalsHit * 100) / simsPerSequence << "%)\n";
		break;
	}
	cout << outcome.steps / simsPerSequence << " average step" << (outcome.steps == 1 ? "" : "s") << ", " << outcome.invalidActions / simsPerSequence << " average invalid actions\n\n";

	cout << "Sequence:\n";
	cout << sequenceToString(result.sequence);
	cout << endl;

	if (gatherStats)
	{
		cout << '\n';
		cout << "Additions: " << result.stats.additions << '\n';
		cout << "Replacements: " << result.stats.replacements << '\n';
		cout << "Removals: " << result.stats.removals << '\n';
		cout << "Shifts: " << result.stats.shifts << '\n';
		cout << "Swaps: " << result.stats.swaps << "\n\n";
		
		cout << "Mutations:\n";
		for (auto it = result.stats.mutations.begin(); it != result.stats.mutations.end(); ++it)
			cout << it->first << ": " << it->second << '\n';

		cout << endl;
	}

	return 0;
}

[[noreturn]] void usage()
{
	cerr << "\nusage:\n";
	cerr << programName << " command arguments ...\n";
	cerr << "where 'command' is one of the following:\n";
	cerr << "\tsingle -j class -r recipefile [-f statsfile] [-o optionsfile] [-q | -m | -c collectability]\n"
		"\t\t[-s sequencefile | -e] [-n]\n";
	cerr << "\tmulti -j class -r recipefile [-f statsfile] [-o optionsfile] [-q | -m | -c collectability]\n"
		"\t\t[-s sequencefile | -e] [-n]\n";
	cerr << "\tsolve -j class -r recipefile [-f statsfile] [-o optionsfile] [-t strategy]\n"
		"\t\t[-q | -m | -c collectability] [-s sequencefile | -e] [-n] [-z]\n";
	cerr << endl;
	exit(1);
}

[[noreturn]] void needsArgument(char letter)
{
	cerr << "-" << letter << " requires an argument\n";
	usage();
}

int main(int argc, char* argv[])
{
	programName = argc > 0 ? argv[0] : "advancedtouch";

	if (argc < 2)
	{
		cerr << "command required" << endl;
		usage();
	}

	enum class commands { single, multi, solve };
	commands command;

	crafterStats crafter;
	bool classSet = false;

	recipeStats recipe;
	int initialQuality = 0;

	goalType goal = goalType::quality;
	strategy strat = strategy::standard;

	rapidjson::Document crafterStatsDocument;
	rapidjson::Document recipeStatsDocument;
	int collectableGoal;
	rapidjson::Document optionsStatsDocument;
	rapidjson::Document sequenceDocument;
	bool crafterStatsProvided = false;
	bool recipeStatsProvided = false;
	bool optionsStatsProvided = false;
	bool sequenceProvided = false;
	bool useFood = true;
	bool useMedicine = true;

	bool gatherStatistics = false;

	options opts;

	string currentArgvOrig = argv[1];
	string currentArgv = lowercase(currentArgvOrig);
	if (currentArgv == "single" || currentArgv == "singlesim") command = commands::single;
	else if (currentArgv == "multi" || currentArgv == "multisim") command = commands::multi;
	else if (currentArgv == "solve" || currentArgv == "solver") command = commands::solve;
	else
	{
		cerr << "unknown command '" << currentArgvOrig << "'\n";
		usage();
	}

	for (int currentArgc = 2; currentArgc < argc; currentArgc++)
	{
		currentArgvOrig = argv[currentArgc];
		currentArgv = lowercase(currentArgvOrig);
		if (currentArgv.empty() || currentArgv[0] != '-')
		{
			cerr << "unknown argument '" << currentArgv << "'\n";
			usage();
		}
		else if (currentArgv == "-c")
		{
			currentArgc++;
			if (currentArgc >= argc) needsArgument('c');

			collectableGoal = stringToInt(argv[currentArgc], 'c');
			goal = goalType::collectability;
		}
		else if (currentArgv == "-e")
		{
			sequenceProvided = false;
		}
		else if (currentArgv == "-f")
		{
			currentArgc++;
			if (currentArgc >= argc) needsArgument('f');

			if (!openAndParseJSON(argv[currentArgc], &crafterStatsDocument))
			{
				cerr << "failed to parse " << argv[currentArgc] << ": ";
				cerr << rapidjson::GetParseError_En(crafterStatsDocument.GetParseError());
				cerr << endl;
				exit(1);
			}
			crafterStatsProvided = true;
			// Don't process it yet, we need to know what class the crafter is first
		}
		else if (currentArgv == "-i")
		{
			currentArgc++;
			if (currentArgc >= argc) needsArgument('i');

			initialQuality = stringToInt(argv[currentArgc], 'i', 0);
		}
		else if (currentArgv == "-j")
		{
			currentArgc++;
			if (currentArgc >= argc) needsArgument('j');
			string givenClassOrig = argv[currentArgc];
			string givenClass = lowercase(givenClassOrig);

			if (givenClass == "crp" || givenClass == "carpenter") crafter.classKind = classes::CRP;
			else if (givenClass == "bsm" || givenClass == "blacksmith") crafter.classKind = classes::BSM;
			else if (givenClass == "arm" || givenClass == "armorer") crafter.classKind = classes::ARM;
			else if (givenClass == "gsm" || givenClass == "goldsmith") crafter.classKind = classes::GSM;
			else if (givenClass == "ltw" || givenClass == "leatherworker") crafter.classKind = classes::LTW;
			else if (givenClass == "wvr" || givenClass == "weaver") crafter.classKind = classes::WVR;
			else if (givenClass == "alc" || givenClass == "alchemist") crafter.classKind = classes::ALC;
			else if (givenClass == "cul" || givenClass == "culinarian") crafter.classKind = classes::CUL;
			else
			{
				cerr << "unknown class '" << givenClassOrig << "'\n";
				usage();
			}
			classSet = true;
		}
		else if (currentArgv == "-m")
		{
			goal = goalType::maxQuality;
		}
		else if (currentArgv == "-n")
		{
			useFood = false;
			useMedicine = false;
		}
		else if (currentArgv == "-o")
		{
			currentArgc++;
			if (currentArgc >= argc) needsArgument('o');

			if (!openAndParseJSON(argv[currentArgc], &optionsStatsDocument))
			{
				cerr << "failed to parse " << argv[currentArgc] << ": ";
				cerr << rapidjson::GetParseError_En(optionsStatsDocument.GetParseError());
				cerr << endl;
				exit(1);
			}
			optionsStatsProvided = true;
		}
		else if (currentArgv == "-q")
		{
			goal = goalType::quality;
		}
		else if (currentArgv == "-r")
		{
			currentArgc++;
			if (currentArgc >= argc) needsArgument('r');

			if (!openAndParseJSON(argv[currentArgc], &recipeStatsDocument))
			{
				cerr << "failed to parse " << argv[currentArgc] << ": ";
				cerr << rapidjson::GetParseError_En(recipeStatsDocument.GetParseError());
				cerr << endl;
				exit(1);
			}
			recipeStatsProvided = true;
		}
		else if (currentArgv == "-s")
		{
			currentArgc++;
			if (currentArgc >= argc) needsArgument('s');

			if (!openAndParseJSON(argv[currentArgc], &sequenceDocument))
			{
				cerr << "failed to parse " << argv[currentArgc] << ": ";
				cerr << rapidjson::GetParseError_En(crafterStatsDocument.GetParseError());
				cerr << endl;
				exit(1);
			}
			sequenceProvided = true;
			// Don't process yet, in case -e gets provided later
		}
		else if (currentArgv == "-t")
		{
			if (command != commands::solve)
			{
				cerr << "-t only available in solve mode\n";
				usage();
			}
			currentArgc++;
			if (currentArgc >= argc) needsArgument('t');

			string givenStrategyOrig = argv[currentArgc];
			string givenStrategy = lowercase(givenStrategyOrig);
			if (givenStrategy == "standard") strat = strategy::standard;
			else if (givenStrategy == "hqorbust") strat = strategy::hqOrBust;
			else if (givenStrategy == "nqonly") strat = strategy::nqOnly;
			else
			{
				cerr << "unknown strategy '" << givenStrategyOrig << "'\n";
				usage();
			}
		}
		else if (currentArgv == "-z")
		{
			gatherStatistics = true;
		}
		else
		{
			cerr << "unknown argument " << argv[currentArgc] << endl;
			usage();
		}
	}

	if (!classSet)
	{
		cerr << "-j argument required\n";
		usage();
	}

	if (!crafterStatsProvided)
	{
		if (!openAndParseJSON("levels.json", &crafterStatsDocument))
		{
			cerr << "failed to parse jevels.json: ";
			cerr << rapidjson::GetParseError_En(crafterStatsDocument.GetParseError());
			cerr << endl;
			exit(1);
		}
	}

	parseStats(crafterStatsDocument, &crafter, useFood, useMedicine);
	
	if (!recipeStatsProvided)
	{
		cerr << "no recipe file specified" << endl;
		exit(1);
	}

	parseRecipe(recipeStatsDocument, &recipe);

	if (goal == goalType::collectability)
		recipe.quality = collectableGoal * 10;

	if (!optionsStatsProvided)
	{
		if (!openAndParseJSON("options.json", &optionsStatsDocument))
		{
			cerr << "failed to parse options.json: ";
			cerr << rapidjson::GetParseError_En(optionsStatsDocument.GetParseError());
			cerr << endl;
			exit(1);
		}
	}

	parseOptions(optionsStatsDocument, &opts, command == commands::solve);

	craft::sequenceType seed;
	if (sequenceProvided)
	{
		seed = parseSequence(sequenceDocument);
		if (command == commands::solve)
		{	
			craft::sequenceType seedSorted = seed;
			craft::sequenceType available = solver::getAvailable(crafter, recipe, opts.useTricks, true);
			sort(available.begin(), available.end());
			sort(seedSorted.begin(), seedSorted.end());
			// available is already unique
			seedSorted.erase(unique(seedSorted.begin(), seedSorted.end()), seedSorted.end());
			if (!includes(available.begin(), available.end(), seedSorted.begin(), seedSorted.end()))
			{
				cerr << "warning: the provided seed contains actions not available at the crafter's level or options\n";
				cerr << "the generated solution may not be valid" << endl;
			}
		}
	}
	
	switch (command)
	{
	case commands::single:
		return performSingle(crafter, recipe, seed, goal, initialQuality, opts.normalLock);
	case commands::multi:
		return performMulti(crafter, recipe, seed, goal, initialQuality, opts.normalLock, opts.threads, opts.simsPerSequence);
	case commands::solve:
		signal(SIGINT, handler);
		return performSolve(crafter, recipe, seed, goal, initialQuality, opts.normalLock, opts.threads, opts.simsPerSequence,
			opts.generations, opts.population, opts.maxCacheSize, strat, opts.useTricks, gatherStatistics);
	}
}