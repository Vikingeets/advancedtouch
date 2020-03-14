#include <cassert>
#include <iostream>
#include <vector>
#include <unordered_map>
#include <list>
#include <set>
#include <chrono>
#include "common.h"
#include "solver.h"
#include "craft.h"
#include "random.h"
#include "levels.h"

// for thread priorities
#if defined _WIN32
#define NOMINMAX
#include <windows.h>
#endif

using namespace std;

template <typename T>
class vectorHash
{
public:
	size_t operator()(const vector<T>& in) const
	{
		size_t output = in.size();
		for (T val : in)
		{
			// From Boost's hash_combine
			output ^= hash<T>()(val) + 0x9e3779b9 + (output << 6) + (output >> 2);
		}
		return output;
	}
};

class resultCache
{
private:
	int maxCacheSize;
	list<solver::trial> cacheData;
	unordered_map <craft::sequenceType, list<solver::trial>::iterator, vectorHash<actions>> cacheHash;

	int hits;
	int misses;

	void addToCache(solver::trial entry);
	void moveToFront(list<solver::trial>::iterator it)
	{
		if (it != cacheData.begin())
			cacheData.splice(cacheData.begin(), cacheData, it);
	}
	void removeLastEntry();
public:
	resultCache(int maxSize) :
		maxCacheSize(maxSize),
		hits(0),
		misses(0)
	{
		if(maxCacheSize > 0)
			cacheHash.reserve(maxCacheSize);
	}
	resultCache() :
		resultCache(0)
	{}
	resultCache(const resultCache&) = delete;
	bool operator=(const resultCache&) = delete;

	// returns empty on a miss
	solver::trial getCached(craft::sequenceType sequence, bool gatherStatistics);
	void populateCache(const vector<solver::trial>& input);

	int getHits() const
	{
		return hits;
	}
	int getMisses() const
	{
		return misses;
	}
};

void resultCache::addToCache(solver::trial entry)
{
	if (entry.sequence.empty()) return;
	entry.sequence.shrink_to_fit();
	cacheData.push_front(entry);
	auto it = cacheData.begin();
	cacheHash.insert({ entry.sequence, it });
	assert(cacheData.size() == cacheHash.size());
}

void resultCache::removeLastEntry()
{
	assert(!cacheData.empty() && !cacheHash.empty());
	auto dataIt = std::prev(cacheData.end());
	auto hashIt = cacheHash.find(dataIt->sequence);
	assert(hashIt != cacheHash.end());
	cacheHash.erase(hashIt);
	cacheData.erase(dataIt);
}

solver::trial resultCache::getCached(craft::sequenceType sequence, bool gatherStatistics)
{
	auto it = cacheHash.find(sequence);
	if (it != cacheHash.end())
	{
		if (gatherStatistics) hits++;
		moveToFront(it->second);
		return *(it->second);
	}
	else
	{
		if (gatherStatistics) misses++;
		return solver::trial{};
	}
}

void resultCache::populateCache(const vector<solver::trial>& input)
{
	if (maxCacheSize <= 0) return;
	for (const auto& t : input)
	{
		// Make sure it's not already in the cache
		// Actual empty sequences won't get cached, but that's for the best anyways.
		// Has the side effect of moving these to the top.
		if (!getCached(t.sequence, false).sequence.empty())
			continue;
		while (cacheHash.size() >= maxCacheSize)
			removeLastEntry();
		addToCache(t);
	}
}

void workerMain(solver* solve);

const vector<actions> allActions = {
	actions::basicSynth,
	actions::carefulSynthesis,
	actions::rapidSynthesis,
	actions::focusedSynthesis,
	actions::delicateSynthesis,
	actions::groundwork,
	actions::intensiveSynthesis,
	actions::muscleMemory,
	actions::brandOfTheElements,

	actions::basicTouch,
	actions::standardTouch,
	actions::hastyTouch,
	actions::byregotsBlessing,
	actions::preciseTouch,
	actions::focusedTouch,
	actions::patientTouch,
	actions::prudentTouch,
	actions::preparatoryTouch,
	actions::trainedEye,

	actions::tricksOfTheTrade,

	actions::mastersMend,
	actions::wasteNot,
	actions::wasteNot2,
	actions::manipulation,

	actions::innerQuiet,
	actions::greatStrides,
	actions::veneration,
	actions::innovation,
	actions::nameOfTheElements,
	actions::finalAppraisal,

	actions::reflect,
	actions::observe,
};

int actionTime(actions act)
{
	// I'm only going to explicitly list the buffs since there's less of them
	switch (act)
	{
	case actions::greatStrides:
	case actions::innerQuiet:
	case actions::veneration:
	case actions::innovation:
	case actions::manipulation:
	case actions::nameOfTheElements:
	case actions::wasteNot:
	case actions::finalAppraisal:
		return 2;
	default:
		return 3;
	}
}

int sequenceTime(const craft::sequenceType& sequence)
{
	const int macroLength = 15;
	const int betweenMacroTime = 10;

	int output = 0;

	for (actions a : sequence)
	{
		output += actionTime(a);
	}

	output += static_cast<int>((sequence.size() / (macroLength + 1)) * betweenMacroTime);

	return output;
}

struct mutationGroups
{
	// the one-two ratio and lack of three+ derived from stats gathering
	int preserve;		// top 5%		both children 0 mutations
	int oneMutation;	// next 40%		" 1 mutation
	int twoMutations;	// next 5%		" 2 mutations
	int eliminate;		// bottom 50%
};

mutationGroups divideSequence(int amount)
{
	assert(amount >= 2 && amount % 2 == 0);
	mutationGroups output;

	output.eliminate = amount / 2;	// a lot breaks if this changes

	// preserve gets first dibs, two gets last, the remaining go into one
	output.preserve = (amount + 19) / 20;
	output.twoMutations = amount / 20;
	output.oneMutation = output.eliminate - (output.preserve + output.twoMutations);

	assert(output.preserve + output.oneMutation + output.twoMutations + output.eliminate == amount);
	return output;
}

bool isFirstAction(actions action)
{
	switch (action)
	{
	case actions::muscleMemory:
	case actions::reflect:
	case actions::trainedEye:
		return true;
	default:
		return false;
	}
}

bool isConditional(actions action)
{
	switch (action)
	{
	case actions::intensiveSynthesis:
	case actions::preciseTouch:
	case actions::tricksOfTheTrade:
		return true;
	default:
		return false;
	}
}

// Would return a set, but needs random access iterators for the mutator
vector<actions> solver::getAvailable(const crafterStats& crafter, const recipeStats& recipe, bool useConditionals, bool includeFirst)
{
	vector<actions> output;
	for(actions action : allActions)
	{
		// Reject by level
		if (actionLevel(action) > crafter.level) continue;

		if (!useConditionals && isConditional(action)) continue;

		if (!includeFirst && isFirstAction(action))
			continue;

		if (
			(action == actions::trainedEye) &&
			crafter.level < rlvlToMain(recipe.rLevel) + 10
			)
			continue;

		output.push_back(action);
	}
	return output;
}

int solver::actionLevel(actions action)
{
	switch (action)
	{
	case actions::basicSynth: return 1;
	case actions::basicTouch: return 5;
	case actions::mastersMend: return 7;
	case actions::rapidSynthesis:
	case actions::hastyTouch:
		return 9;
	case actions::innerQuiet: return 11;
	case actions::tricksOfTheTrade:
	case actions::observe:
		return 13;
	case actions::veneration:
	case actions::wasteNot:
		return 15;
	case actions::standardTouch: return 18;
	case actions::greatStrides: return 21;
	case actions::innovation: return 26;
	case actions::nameOfTheElements:
	case actions::brandOfTheElements:
		return 37;
	case actions::finalAppraisal: return 42;
	case actions::wasteNot2: return 47;
	case actions::byregotsBlessing: return 50;
	case actions::preciseTouch: return 53;
	case actions::muscleMemory: return 54;
	case actions::carefulSynthesis: return 62;
	case actions::patientTouch: return 64;
	case actions::manipulation: return 65;
	case actions::prudentTouch: return 66;
	case actions::focusedSynthesis: return 67;
	case actions::focusedTouch: return 68;
	case actions::reflect: return 69;
	case actions::preparatoryTouch: return 71;
	case actions::groundwork: return 72;
	case actions::delicateSynthesis: return 76;
	case actions::intensiveSynthesis: return 78;
	case actions::trainedEye: return 80;
	default:
		assert(false);
		return (numeric_limits<int>::max)();
	}
}

// Constructor for multisynth mode
solver::solver(const crafterStats & c, const recipeStats & r, const craft::sequenceType & seed,
	goalType g, int iQ, int tCnt, bool nLock) :
	crafter(c),
	recipe(r),
	goal(g),
	initialState(iQ, c, r, nLock),
	numberOfThreads(tCnt),
	strat(strategy::standard),	// here and gatherStatistics not used for multisynth,
	gatherStatistics(false),	// but it makes the compiler happy
	trials(1),
	simResults(1),
	sequenceCounters(1),
	threadsDone(0)
{
	order.command = threadCommand::terminate;	// "terminate" doubles as start

	trials[0].sequence = seed;
	trials[0].outcome = netResult();

	sequenceCounters[0].store(0, memory_order_relaxed);
}

// Constructor for solve mode
solver::solver(const crafterStats& c,
	const recipeStats& r,
	const craft::sequenceType& seed,
	goalType g, int iQ,
	int tCnt, bool nLock, strategy s,
	int population,
	bool uC, bool gS) :
	crafter(c),
	recipe(r),
	goal(g),
	initialState(iQ, c, r, nLock),
	numberOfThreads(tCnt),
	strat(s),
	gatherStatistics(gS),
	trials(population),
	simResults(population),
	cached(population, false),
	sequenceCounters(population),
	availableActions(getAvailable(c, r, uC && !nLock, true)),
	availableWithoutFirst(getAvailable(c, r, uC && !nLock, false)),
	threadsDone(0)
{
	assert(numberOfThreads > 0);

	order.command = threadCommand::terminate;

	for_each(trials.begin(), trials.end(),
		[&seed](trial& t) {t.sequence = seed; });
}

// Constructor for solve mode with initial state
solver::solver(const crafterStats& c,
	const recipeStats& r,
	const craft::sequenceType& seed,
	goalType g, const craft& iS,
	int tCnt, strategy s,
	int population) :
	crafter(c),
	recipe(r),
	goal(g),
	initialState(iS),
	numberOfThreads(tCnt),
	strat(s),
	gatherStatistics(false),
	trials(population),
	simResults(population),
	cached(population, false),
	sequenceCounters(population),
	availableActions(getAvailable(c, r, true, initialState.getStep() == 1)),
	availableWithoutFirst(getAvailable(c, r, true, false)),
	threadsDone(0)
{
}

solver::trial solver::executeMultisim(int simulationsPerTrial)
{
	vector<thread> threads;
	
	for (int i = 0; i < numberOfThreads; i++)
		threads.emplace_back(workerMain, this);

	threadOrder orders = {};
	orders.command = threadCommand::simulate;
	orders.trials = &trials;
	orders.counters = &sequenceCounters;
	orders.crafter = &crafter;
	orders.recipe = &recipe;
	orders.numberOfSimulations = simulationsPerTrial;
	order.initialState = &initialState;
	orders.goal = goal;

	setOrder(orders);
	waitOnSimsDone();
	orders.command = threadCommand::terminate;
	setOrder(orders);

	for (auto& t : threads)
		t.join();

	return trials[0];
}

// returns true if a is better than b, so sorting ends up best to worst
bool solver::compareResult(const solver::trial& a, const solver::trial& b, int simulationsPerTrial, bool alwaysRejectInvalids) const
{
	// first reject sequences with invalid actions
	// but if one sequence is considerably better than the others, let the invalids stand
	// its children/siblings with the same result and less invalids will end up getting preferred
	if (a.outcome.invalidActions != b.outcome.invalidActions)
	{
		if ((strat == strategy::nqOnly || alwaysRejectInvalids))
			return a.outcome.invalidActions < b.outcome.invalidActions;
		else
		{
			const int superiorThreshhold = 105;	// percent plus 100
			int aQuality = min(a.outcome.quality, recipe.nominalQuality * simulationsPerTrial);
			int bQuality = min(b.outcome.quality, recipe.nominalQuality * simulationsPerTrial);

			int greaterQuality = max(aQuality, bQuality);
			int lesserQuality = min(aQuality, bQuality);

			bool pruneInvalid = lesserQuality * superiorThreshhold > greaterQuality * 100;

			if (pruneInvalid) return a.outcome.invalidActions < b.outcome.invalidActions;
		}
	}
	switch (strat)
	{
	case strategy::standard:
		// if either didn't make 100% success, rank based on whoever got more
		if (a.outcome.successes != b.outcome.successes)
			return a.outcome.successes > b.outcome.successes;
//		[[fallthrough]]		// both have equal successes (typically 100% or 0%) so rank on HQ
	case strategy::hqOrBust:
		if (a.outcome.successes == 0 && b.outcome.successes == 0)
			return a.outcome.progress > b.outcome.progress;
		switch (goal)
		{
		case goalType::hq:
			if (a.outcome.hqPercent == 100 * simulationsPerTrial && b.outcome.hqPercent == 100 * simulationsPerTrial)
				break;	// i.e. go to nqOnly for length comparison
			else if (a.outcome.hqPercent != b.outcome.hqPercent)
				return a.outcome.hqPercent > b.outcome.hqPercent;
			else return a.outcome.quality > b.outcome.quality;
		case goalType::collectability:
			if (a.outcome.collectableGoalsHit == simulationsPerTrial && b.outcome.collectableGoalsHit == simulationsPerTrial)
				break;
			else if (a.outcome.collectableGoalsHit != b.outcome.collectableGoalsHit)
				return a.outcome.collectableGoalsHit > b.outcome.collectableGoalsHit;
			else return a.outcome.quality > b.outcome.quality;
		case goalType::maxQuality:
			if (a.outcome.quality != b.outcome.quality)
				return a.outcome.quality > b.outcome.quality;
			else break;
		}
//		[[fallthrough]]		// quality is tied
	case strategy::nqOnly:
		// we might have switched to here instead of falling through, so re-compare successes
		if (a.outcome.successes == 0 && b.outcome.successes == 0)
			return a.outcome.progress > b.outcome.progress;
		else if (a.outcome.successes != b.outcome.successes)
			return a.outcome.successes > b.outcome.successes;
		else break;
	}

	// Everything asked for is equal, so compare on macro length
	return sequenceTime(a.sequence) < sequenceTime(b.sequence);
}

solver::trial solver::executeSolver(int simulationsPerTrial, int generations, int generationStreak, int maxCacheSize, solver::solverCallback callback)
{
	vector<thread> threads;
	for (int i = 0; i < numberOfThreads; i++)
		threads.emplace_back(workerMain, this);

	resultCache cache(maxCacheSize);

	threadOrder orders = {};
	orders.command = threadCommand::terminate;
	orders.trials = &trials;
	orders.counters = &sequenceCounters;
	orders.cached = &cached;
	orders.crafter = &crafter;
	orders.recipe = &recipe;
	orders.numberOfSimulations = simulationsPerTrial;
	orders.initialState = &initialState;
	orders.goal = goal;

	actions lastStarter = actions::invalid;
	int starterStreak = 0;
	for (int gen = 0; gen < generations; gen++)
	{
		mutated.clear();
		mutated.reserve(trials.size());
		orders.command = threadCommand::mutate;
		setOrder(orders);
		waitOnMutationsDone();

		// See if any of our prospective sequences already have their results already in the cache
		if (maxCacheSize > 0)
		{
			for (int i = 0; i < trials.size(); ++i)
			{
				trial current = cache.getCached(trials[i].sequence, gatherStatistics);
				bool currentCached = !current.sequence.empty();	// An empty sequence is never cached, so this test is okay
				cached[i] = currentCached;
				if (currentCached)
					trials[i] = current;
			}
		}

		orders.command = threadCommand::simulate;
		setOrder(orders);
		waitOnSimsDone();

		// We don't need to sort the whole population, just enough so each group ends up in the right section
		mutationGroups mG = divideSequence(static_cast<int>(trials.size()));
		auto comp = [this, simulationsPerTrial](const trial& a, const trial& b)
			{return compareResult(a, b, simulationsPerTrial, false);};
		auto compNoInvalids = [this, simulationsPerTrial](const trial& a, const trial& b)
			{return compareResult(a, b, simulationsPerTrial, true); };

		auto beginPos = trials.begin();
		auto preserveStart = beginPos;
		auto oneStart = preserveStart + mG.preserve;
		auto twoStart = oneStart + mG.oneMutation;
		auto eliminateStart = twoStart + mG.twoMutations;
		auto eliminateEnd = eliminateStart + mG.eliminate;
		assert(eliminateEnd == trials.end());

		nth_element(beginPos, eliminateStart, eliminateEnd, comp);
		nth_element(beginPos, twoStart, eliminateStart, comp);
		nth_element(beginPos, oneStart, twoStart, comp);
		nth_element(beginPos, preserveStart, oneStart, compNoInvalids);
		nth_element(beginPos, beginPos, preserveStart, compNoInvalids);

		int uniquePopulation = 0;
		int cacheHits = 0;
		if (gatherStatistics)
		{
			set<craft::sequenceType> uniques;
			for (auto& trial : trials)
			{
				uniques.insert(trial.sequence);
			}
			uniquePopulation = static_cast<int>(uniques.size());
			cacheHits = count(cached.begin(), cached.end(), true);
		}

		if (callback && !callback(generations, gen, simulationsPerTrial, goal, strat, trials.front(), uniquePopulation, cacheHits))
			break;

		if (generationStreak > 0)
		{
			if (trials.front().sequence.empty())
			{
				lastStarter = actions::invalid;
				starterStreak = 0;
			}
			actions currentStarter = trials.front().sequence.front();
			if (currentStarter == lastStarter)
			{
				starterStreak++;
				if (starterStreak >= generationStreak) break;
			}
			else
			{
				lastStarter = currentStarter;
				starterStreak = 0;
			}
		}

		if(maxCacheSize > 0) cache.populateCache(trials);
	}

	orders.command = threadCommand::terminate;
	setOrder(orders);
	for (auto& t : threads)
		t.join();
	
	return trials.front();
}


// Called by main thread
void solver::setOrder(threadOrder odr)
{
	unique_lock<std::mutex> lock(orderSetLock);
	for_each(sequenceCounters.begin(), sequenceCounters.end(),
		[](atomic<int>& a) {a.store(0, memory_order_relaxed);});
	threadsDone = 0;
	order = odr;

	lock.unlock();
	orderSet.notify_all();
	
	return;
}

void solver::waitOnSimsDone()
{
	unique_lock<mutex> lock(threadCompleteLock);

	int* tDone = &threadsDone;
	int* totalThreads = &numberOfThreads;
	threadComplete.wait(lock, [&tDone, &totalThreads]() { return *tDone >= *totalThreads;});
	assert(trials.size() == simResults.size());
	for (int i = 0; i < trials.size(); ++i)
	{
		if (cached[i]) continue;
		trials[i].outcome = simResults[i];
		memset(&(simResults[i]), 0, sizeof(simResults[i]));
	}

	return;
}

void solver::waitOnMutationsDone()
{
	unique_lock<mutex> lock(threadCompleteLock);
	int* tDone = &threadsDone;
	int* totalThreads = &numberOfThreads;
	threadComplete.wait(lock, [&tDone, &totalThreads]() {return *tDone >= *totalThreads;});
	assert(trials.size() == mutated.size());
	for (int i = 0; i < mutated.size(); ++i)
	{
		trials[i] = move(mutated[i]);
		trials[i].outcome = {};
	}
	return;
}

// Called by worker threads
solver::threadOrder solver::waitOnCommandChange(threadCommand previous)
{
	unique_lock<mutex> lock(orderSetLock);

	threadOrder* odr = &order;
	orderSet.wait(lock, [odr, &previous]() { return odr->command != previous; });

	return order;
}

void solver::reportThreadSimResults(const vector<netResult>& threadResults)
{
	unique_lock<mutex> lock(threadCompleteLock);
	assert(simResults.size() == threadResults.size());

	for (int i = 0; i < trials.size(); ++i)
	{
		simResults[i].successes += threadResults[i].successes;
		simResults[i].progress += threadResults[i].progress;
		simResults[i].quality += threadResults[i].quality;
		switch (goal)
		{
		case goalType::hq:
			simResults[i].hqPercent += threadResults[i].hqPercent;
			break;
		case goalType::maxQuality:
			break;
		case goalType::collectability:
			simResults[i].collectableGoalsHit += threadResults[i].collectableGoalsHit;
			break;
		}
		simResults[i].invalidActions += threadResults[i].invalidActions;
		simResults[i].steps += threadResults[i].steps;
	}
	threadsDone++;
	lock.unlock();

	threadComplete.notify_all();	// kick the main thread if it's waiting on threadsDone

	return;
}

void solver::reportThreadMutationResults(const std::vector<trial>& children)
{
	unique_lock<mutex> lock(threadCompleteLock);
	assert(children.size() <= trials.size());

	mutated.insert(mutated.end(), children.begin(), children.end());

	threadsDone++;

	lock.unlock();
	threadComplete.notify_all();

	return;
}

enum class mutationType
{
	add,
	// needs at least 1, but useless without 2
	replace,
	// needs at least 2
	remove,
	shift,
	swap
};

mutationType getRandomMutation(size_t elements, random& rng)
{
	if (elements == 0) return mutationType::add;
	if (elements == 1) return rng.generateInt(0, 2) == 0 ? mutationType::replace : mutationType::add;	// add/replace ratio

	// approximate ratio of mutations appearing in best outcomes
	const int addChance = 2;
	const int replaceChance = addChance + 1;
	const int removeChance = replaceChance + 1;
	const int shiftChance = removeChance + 2;
	const int swapChance = shiftChance + 2;
	const int totalChance = swapChance;
	int selected = rng.generateInt(totalChance - 1);

	if (selected < addChance) return mutationType::add;
	else if (selected < replaceChance) return mutationType::replace;
	else if (selected < removeChance) return mutationType::remove;
	else if (selected < shiftChance) return mutationType::shift;
	else return mutationType::swap;
}

solver::trial solver::mutateSequence(trial input, int numberOfMutations, random& rng)
{
	for (int i = 0; i < numberOfMutations; i++)
	{
		mutationType mutation = getRandomMutation(input.sequence.size(), rng);

		switch (mutation)
		{
		case mutationType::add:
		{
			auto where = input.sequence.begin();
			bool sequenceHasFirst = !input.sequence.empty() && isFirstAction(input.sequence.front());
			advance(where, rng.generateInt(sequenceHasFirst ? 1 : 0, static_cast<int>(input.sequence.size())));	// not "- 1"; it can advance to the end iterator
			actions which = (where == input.sequence.begin()) ?
				availableActions[rng.generateInt(availableActions.size() - 1)] :
				availableWithoutFirst[rng.generateInt(availableWithoutFirst.size() - 1)];
			input.sequence.insert(where, which);
			if (gatherStatistics) input.stats.additions++;
			break;
		}
		case mutationType::replace:
		{
			assert(!input.sequence.empty());
			auto where = input.sequence.begin();
			advance(where, rng.generateInt(input.sequence.size() - 1));
			actions which = (where == input.sequence.begin()) ?
				availableActions[rng.generateInt(availableActions.size() - 1)] :
				availableWithoutFirst[rng.generateInt(availableWithoutFirst.size() - 1)];
			*where = which;
			if (gatherStatistics) input.stats.replacements++;
			break;
		}
		case mutationType::remove:
		{
			assert(!input.sequence.empty());
			auto where = input.sequence.begin();
			advance(where, rng.generateInt(input.sequence.size() - 1));
			input.sequence.erase(where);
			if (gatherStatistics) input.stats.removals++;
			break;
		}
		case mutationType::shift:
		{
			assert(input.sequence.size() > 1);
			auto low = input.sequence.begin();
			auto high = input.sequence.begin();
			advance(low, rng.generateInt(isFirstAction(*low) ? 1 : 0, static_cast<int>(input.sequence.size() - 1)));
			advance(high, rng.generateInt(isFirstAction(*high) ? 1 : 0, static_cast<int>(input.sequence.size() - 1)));
			if (high < low) swap(low, high);

			// To grab the first and put it on the end, move first+1 to first
			// To grab the last and put it at the beginning, move last to first
			decltype(low) grabbed = rng.generateInt(1) == 0 ? low + 1 : high;
			rotate(low, grabbed, high + 1);
			if (gatherStatistics) input.stats.shifts++;
			break;
		}
		case mutationType::swap:
		{
			assert(input.sequence.size() > 1);
			auto first = input.sequence.begin();
			auto second = input.sequence.begin();
			advance(first, rng.generateInt(isFirstAction(*first) ? 1 : 0, static_cast<int>(input.sequence.size() - 1)));
			advance(second, rng.generateInt(isFirstAction(*second) ? 1 : 0, static_cast<int>(input.sequence.size() - 1)));
			iter_swap(first, second);
			if (gatherStatistics) input.stats.swaps++;
			break;
		}
		}
	}

	if (gatherStatistics)
		input.stats.mutations[numberOfMutations]++;

	return input;
}


/*
THREAD FUNCTIONS
*/

void workerPerformSimulations(solver* solve, solver::threadOrder order, random& rng)
{
	int trialNumber = 0;

	vector<solver::netResult> localResults;

	localResults.resize(order.trials->size(), solver::netResult{});

	while (trialNumber < order.trials->size())
	{
		if (
			// Is this result in the cache?
			(*order.cached)[trialNumber]
			||
			// Have we (and the other threads) done all the sims for this one?
			(*order.counters)[trialNumber].fetch_add(1, memory_order_relaxed) >= order.numberOfSimulations
			)
		{
			trialNumber++;
			continue;
		}
		craft synth(*order.initialState);
		synth.setRNG(&rng);

		craft::endResult result = synth.performAll((*order.trials)[trialNumber].sequence, order.goal, false);
		localResults[trialNumber].progress += result.progress;
		if (result.progress >= order.recipe->difficulty)	// a failed synth is always worth 0 quality, even in hqorbust mode
		{
			localResults[trialNumber].successes++;
			localResults[trialNumber].quality += result.quality;
			switch (order.goal)
			{
			case goalType::hq:
				localResults[trialNumber].hqPercent += result.hqPercent;
				break;
			case goalType::maxQuality:
				break;
			case goalType::collectability:
				if (result.collectableHit) localResults[trialNumber].collectableGoalsHit++;
				break;
			default:
				break;
			}
		}
		localResults[trialNumber].steps += result.steps;
		localResults[trialNumber].invalidActions += result.invalidActions;
	}

	// Everything's done (or has been claimed by another thread), so time to report in and wait for the next order
	solve->reportThreadSimResults(localResults);

	return;
}

void workerPerformMutations(solver* solve, solver::threadOrder order, random& rng)
{
	vector<solver::trial> localMutated;
	mutationGroups groups = divideSequence(static_cast<int>(order.trials->size()));
	// a for loop instead of while here since we'll never use the same trial twice
	// ending at size / 2 since the second half are rejects
	for (int trialNumber = 0; trialNumber < groups.eliminate; trialNumber++)
	{
		if ((*order.counters)[trialNumber].fetch_add(1, memory_order_relaxed) > 0)
		{
			// This one's been claimed by another thread
			continue;
		}
		const solver::trial& parentTrial = (*order.trials)[trialNumber];
		if (trialNumber < groups.preserve)
		{
			localMutated.push_back(solve->mutateSequence(parentTrial, 0, rng));
			localMutated.push_back(solve->mutateSequence(parentTrial, 0, rng));
		}
		else if (trialNumber < groups.preserve + groups.oneMutation)
		{
			localMutated.push_back(solve->mutateSequence(parentTrial, 1, rng));
			localMutated.push_back(solve->mutateSequence(parentTrial, 1, rng));
		}
		else
		{
			localMutated.push_back(solve->mutateSequence(parentTrial, 2, rng));
			localMutated.push_back(solve->mutateSequence(parentTrial, 2, rng));
		}
	}

	solve->reportThreadMutationResults(localMutated);

	return;
}

void workerMain(solver* solve)
{
#if defined _WIN32
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_LOWEST);
#endif // defined _WIN32

	random rng;
	solver::threadOrder order;
	order.command = solver::threadCommand::terminate;
	while (true)
	{
		order = solve->waitOnCommandChange(order.command);
		switch (order.command)
		{
		case solver::threadCommand::simulate:
			workerPerformSimulations(solve, order, rng);
			continue;
		case solver::threadCommand::mutate:
			workerPerformMutations(solve, order, rng);
			continue;
		case solver::threadCommand::terminate:
			return;
		}
	}
}