#pragma once
#include <vector>
#include <map>
#include <thread>
#include <atomic>
#include <functional>
#include "common.h"
#include "craft.h"

class solver
{
public:
	enum class threadCommand
	{
		simulate,
		mutate,
		terminate
	};

	struct netResult
	{
		// each value is the sum of the runs, i.e. divide by the number of simulations to get the average
		int successes;	// each run would add 0 or 1 to this
		int progress;	// the tiebreak on 0 successes
		int quality;	// used for maxQuality or to tiebreak on 0 collectableGoalsHit
		union
		{
			int hqPercent;
			int collectableGoalsHit;	// an individual run would only add 0 or 1 to this
		};
		short steps;		// actions until the synth ends, not neccesarily the size of the sequence
		short invalidActions;		// i.e. those that don't occur due to preconditions not met, insufficient CP, or occuring after the end of the synth
		bool firstInvalid;		// doesn't need to be a sum. if it's true for any of them it's true for all of them
	};

	struct trial
	{
		craft::sequenceType sequence;
		/*
		outcome is not atomic. each thread will hold a vector<netResult> for the results of each trial
		(with full 0 for any not performed), and combine them under lock
		*/
		netResult outcome{};

		struct statistics
		{
			short additions;
			short replacements;
			short removals;
			short shifts;
			short swaps;

			statistics() :
				additions(0),
				replacements(0),
				removals(0),
				shifts(0),
				swaps(0)
			{}
		};
		statistics stats;
	};

	struct threadOrder
	{
		threadCommand command;
		std::vector<trial> const * trials;
		std::vector<std::atomic<int>>* counters;
		std::vector<bool> const * cached;
		// used in sim mode
		crafterStats const * crafter;
		recipeStats const * recipe;
		craft const * initialState;
		int numberOfSimulations;
		goalType goal;
	};

private:
	threadOrder order;	// not atomic: protected with orderSetLock

	const crafterStats crafter;
	const recipeStats recipe;
	goalType goal;
	craft initialState;
	int numberOfThreads;
	
	strategy strat;

	bool gatherStatistics;
	
	std::vector<trial> trials;	// protected by threadCompleteLock, newest to oldest
	std::vector<netResult> simResults;
	std::vector<bool> cached;
	// mutated is cleared at the start of each run, then appended to by the thread reporters under lock
	std::vector<trial> mutated;
	std::vector<std::atomic<int>> sequenceCounters;	// kept separate: trials must be movable for sort to work

#if ATOMIC_INT_LOCK_FREE != 2
#warning "atomic<int> is not always lock free on this platform. consider changing the type of sequenceCounters"
#endif // ATOMIC_INT_LOCK_FREE != 2

	const std::vector<actions> availableActions, availableWithoutFirst;	// used in solve mode. cached here
																		// accessed by multiple threads read-only

	std::condition_variable orderSet, threadComplete;
	std::mutex orderSetLock, threadCompleteLock;
	
	int threadsDone;	// not atomic: protected with threadCompleteLock. reset in setOrder

	bool compareResult(const solver::trial& a, const solver::trial& b, int simulationsPerTrial, bool alwaysRejectInvalids) const;

	std::vector<double> populationSelections;

	void setSelections(int population);

public:
	static std::vector<actions> getAvailable(const crafterStats& crafter, const recipeStats& recipe, bool useConditionals, bool includeFirst);

	static int actionLevel(actions action);

	solver() = delete;

	// Constructor for multisynth mode
	solver(const crafterStats& c, const recipeStats & r, const craft::sequenceType& seed,
		goalType g, int iQ, int tCnt, bool nLock);

	// Constructor for solve mode
	solver(const crafterStats& c,	// the crafter
		const recipeStats& r,		// the recipe
		const craft::sequenceType& seed,	// the initial seed (may be empty)
		goalType g,		// the goal
		int iQ,			// the initial quality
		int tCnt,		// the number of threads to use
		bool nLock,		// whether all conditions should be normal
		strategy s,		// the strategy
		int population,	// the population. must be here to properly allocate counter vector
		bool uC,		// use conditionals?
		bool gS			// gather statistics?
	);

	// Constructor for solve mode with initial state
	solver(const crafterStats& c,
		const recipeStats& r,
		const craft::sequenceType& seed,
		goalType g,
		const craft& iS,
		int tCnt,
		strategy s,
		int population
	);

	void setInitialState(craft iS) { initialState = iS; }

	void resetSeeds(const craft::sequenceType& seed);

	void incrementSeeds(int amount);

	trial executeMultisim(int simulationsPerTrial);

	using solverCallback = std::function<
		bool(					// return true to continue solving, false to abort
			int generations,
			int currentGen,
			int simsPerTrial,
			goalType goal,
			strategy strat,
			trial outcome,
			int uniquePopulation,	// 0 if gatherStatistics == false
			int cacheHits			// 0 if gatherStatistics == false
			)>;

	trial executeSolver(
		int simulationsPerTrial,
		int generations,
		int generationWindow,		// for stepwise mode
		int generationEarly,
		int maxCacheSize,
		solverCallback callback
	);

	goalType getGoal() const
	{
		return goal;
	}																									// due to the whole vector<atomic> thing
	
	/* interthread communication */
	// called by main thread
	void setOrder(threadOrder odr);
	void waitOnSimsDone();
	void waitOnMutationsDone();

	// called by the worker threads
	threadOrder waitOnCommandChange(threadCommand previous);
	void reportThreadSimResults(const std::vector<netResult>& threadResults);	// after calling this, thread returns to waiting on command
	void reportThreadMutationResults(const std::vector<trial>& children);
	trial mutateSequence(trial input, random& rng);
};