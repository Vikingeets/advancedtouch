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
		int steps;		// actions until the synth ends, not neccesarily the size of the sequence
		int invalidActions;		// i.e. those that don't occur due to preconditions not met, insufficient CP, or occuring after the end of the synth
		int reuses;
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
			int additions;
			int replacements;
			int removals;
			int shifts;
			int swaps;

			std::map<int,int> mutations;

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
		// used in sim mode
		crafterStats const * crafter;
		recipeStats const * recipe;
		int numberOfSimulations;
		int wiggle;
		int initialQuality;
		bool normalLock;
		goalType goal;
	};

private:
	threadOrder order;	// not atomic: protected with orderSetLock

	const crafterStats& crafter;
	const recipeStats& recipe;
	goalType goal;
	int wiggleFactor;
	int initialQuality;
	int numberOfThreads;
	bool normalLock; 
	
	strategy strat;

	bool gatherStatistics;
	
	std::vector<trial> trials;	// protected by threadCompleteLock, sorted from best to worst
	std::vector<netResult> simResults;
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

	bool compareResult(const solver::trial& a, const solver::trial& b, int simulationsPerTrial);

public:
	static std::vector<actions> getAvailable(const crafterStats& crafter, const recipeStats& recipe, bool useTricks, bool useReuse, bool includeFirst);

	static int actionLevel(actions action);

	solver() = delete;

	// Constructor for multisynth mode
	solver(const crafterStats& c, const recipeStats & r, const craft::sequenceType& seed,
		goalType g, int pWiggle, int iQ, int tCnt, bool nLock);

	// Constructor for solve mode
	solver(const crafterStats& c,	// the crafter
		const recipeStats& r,		// the recipe
		const craft::sequenceType& seed,	// the initial seed (may be empty)
		goalType g,		// the goal
		int pWiggle,	// the "wiggle factor" to prevent a solution that may not be valid ingame
		int iQ,			// the initial quality
		int tCnt,		// the number of threads to use
		bool nLock,		// whether all conditions should be normal
		strategy s,		// the strategy
		int population,	// the population. must be here to properly allocate counter vector
		bool uT,		// use tricks of the trade?
		bool gS			// gather statistics?
	);

	netResult executeMultisim(int simulationsPerTrial);

	using solverCallback = std::function<
		bool(					// return true to continue solving, false to abort
			int generations,
			int currentGen,
			int simsPerTrial,
			goalType goal,
			strategy strat,
			netResult outcome,
			int uniquePopulation	// 0 if gatherStatistics == false
			)>;

	trial executeSolver(
		int simulationsPerTrial,
		int generations,
		solverCallback callback
	);
																									// due to the whole vector<atomic> thing
	
	/* interthread communication */
	// called by main thread
	void setOrder(threadOrder odr);
	void waitOnSimsDone();
	void waitOnMutationsDone();

	// called by the worker threads
	threadOrder waitOnCommandChange(threadCommand previous);
	void reportThreadSimResults(const std::vector<netResult>& threadResults);	// after calling this, thread returns to waiting on command
	void reportThreadMutationResults(const std::vector<trial>& children);
	trial mutateSequence(trial input, int numberOfMutations, random& rng);
};