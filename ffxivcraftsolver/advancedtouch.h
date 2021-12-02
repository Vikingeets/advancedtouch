/*

C API for Advanced Touch

*/

#ifndef __advancedtouch_h__
#define __advancedtouch_h__

#ifdef __cplusplus
extern "C"
{
#endif

#define AT_STRATEGY_STANDARD 0		/* Maximize success rate, then quality */
#define AT_STRATEGY_HQORBUST 1		/* Maximize goal rate, even at the cost of success rate */
#define AT_STRATEGY_NQONLY 2		/* Ignore quality; only produce a short NQ rotation */

#define AT_GOAL_HQ 0				/* Maximize HQ% chance */
#define AT_GOAL_MAXQUALITY 1		/* Maximize raw quality, even beyond the recipe's maximum */
#define AT_GOAL_COLLECTABILITY 2	/* Maximize number of crafts that reach the goal */
#define AT_GOAL_POINTS 3			/* Maximize the average number of turnin points */


#define AT_SYNTH_BASICSYNTH 100
#define AT_SYNTH_CAREFULSYNTH 101
#define AT_SYNTH_RAPIDSYNTH 102
#define AT_SYNTH_FOCUSEDSYNTH 103
#define AT_SYNTH_GROUNDWORK 104
#define AT_SYNTH_DELICATESYNTH 105
#define AT_SYNTH_INTENSIVESYNTH 106
#define AT_SYNTH_MUSCLEMEMORY 107
#define AT_SYNTH_PRUDENTSYNTH 108

#define AT_TOUCH_BASICTOUCH 200
#define AT_TOUCH_STANDARDTOUCH 201
#define AT_TOUCH_HASTYTOUCH 202
#define AT_TOUCH_BYREGOTS 203
#define AT_TOUCH_PRECISETOUCH 204
#define AT_TOUCH_FOCUSEDTOUCH 205
#define AT_TOUCH_PRUDENTTOUCH 207
#define AT_TOUCH_PREPARATORYTOUCH 208
#define AT_TOUCH_TRAINEDEYE 209
#define AT_TOUCH_ADVANCEDTOUCH 210
#define AT_TOUCH_TRAINEDFINESSE 211

#define AT_TRICKSOFTHETRADE 300

#define AT_DURA_MASTERSMEND 400
#define AT_DURA_WASTENOT 401
#define AT_DURA_WASTENOT2 402
#define AT_DURA_MANIPULATION 403

#define AT_BUFF_REFLECT 501
#define AT_BUFF_GREATSTRIDES 502
#define AT_BUFF_VENERATION 503
#define AT_BUFF_INNOVATION 504
#define AT_BUFF_FINALAPPRAISAL 506

#define AT_OBSERVE 600

struct atSolver;

struct atCrafter
{
	int level;				/* The crafter's job level */
	int cLevel;				/* And their cLevel */
	int craftsmanship;
	int control;
	int CP;
};

struct atRecipe
{
	int rLevel;
	int difficulty;			/* The progress needed to complete the craft */
	int quality;			/* The quality to solve for. For collectability mode, the collectability times 10. Ignored in maxquality mode */
	int displayedQuality;	/* The quality the ingame recipe displays. Should always be set. */ 
	int durability;

	int suggestedCraftsmanship;
	int suggestedControl;

	int* points;			/* An array of integers for the points structure, as pairs of collectability then value */
	unsigned int pointsSize;	/* The number of integers in the array, e.g. 3 entries will be pointsSize == 6 */
};

struct atSolverResult		/* The collective result */
{
	int sequence[100];		/* Zodiark willing, the solver will never actually produce a rotation this long */
	int sequenceLength;		/* The number of actions actually in the sequence. If this is > 100, the sequence has been truncated. */

	int successes;			/* The number of simulations that completed the craft */
	/*
	 *	Quality results in the union are the sum total. Divide by simulationsPerSequence to get the average
	 */
	union
	{
		int hqPercent;		/* With the hq goal. Goes from 0 to 100 (times simulationsPerSequence) */
		int quality;		/* With the max quality goal. */
		int collectableHit; /* With the collectability goal. +0 on a miss, +1 on a hit */
		int points;			/* With the points goal. */
	};
};

atSolver* atInitSolver(
	atCrafter crafter,
	atRecipe recipe,
	int* initialSequence,		/* The starting sequence to solve from. May be empty. */
	int initialSequenceSize,
	int goal,
	int initialQuality,			/* E.g. HQ ingredients */
	int threads,				/* Number of threads to solve using. Must be > 0 */
	int normalLock,				/* 0 to account for conditions. Non-0 to only consider normal (faster but less accurate) */
	int strategy,
	int population,				/* Number of rotations to consider at a time. Must be > 0 */
	int useConditionals,			/* 0 to disable using actions that need a condition. Non-0 to consider using them. */
	double selectionPressure	/* 1.0 < selectionPressure <= 2.0 */
);
	
/* You must provide both .csv files before you can begin solving! Returns 0 on success, 1 on failure */
int atLoadRecipeTable(const char* filename);		/* e.g. RecipeLevelTable.csv */
int atLoadDifferenceTable(const char* filename);	/* CraftLevelDifference.csv */

/* This function will be called after every generation. Return 0 to continue solving, non-0 to stop early. */
typedef int (*atSolverCallback)(
	int,			/* The current generation */
	atSolverResult	/* The best result so far */
	);

/* Instead of solving, you may instead simulate a single sequence. */
/* It will be initialSequence if no solving has been done, or the best sequence if it has. */
/* strategy and useConditionals will be ignored. Set population above to at least 2. */
atSolverResult atExecuteSimulations(
	atSolver* solver,
	int numberOfSimulations
);

atSolverResult atExecuteSolve(
	atSolver* solver,
	int simulationsPerSequence,		/* The number of times each prospective result is simulated. Must be > 0. */
	int generations,				/* Automatically stop after this many generations. Must be > 0 */
	int maxCacheSize,				/* The number of results to hold in the cache, or 0 to disable */
	atSolverCallback callback		/* May be NULL, in which case the solver will run for all generations specified */
);

void atDeinitSolver(atSolver* solver);			/* Please clean up when you're done. */

#ifdef __cplusplus
}	/* extern "C" */
#endif

#endif /* __advancedtouch_h__ */
