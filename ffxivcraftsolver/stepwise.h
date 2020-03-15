#pragma once
#include "common.h"
#include "craft.h"
#include "solver.h"

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
);