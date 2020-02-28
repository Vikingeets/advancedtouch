#pragma once

struct crafterStats
{
	int level;		// The level shown ingame. Used for ability availability
	int cLevel;		// The equivalent cLevel, e.g. level 70 is cLevel 290
	int craftsmanship;
	int control;
	int CP;
};

struct recipeStats
{
	int rLevel;
	int difficulty;
	int quality;		// 0 if unspecified aka maxQuality. For collectable goal, the collectability times 10
	int nominalQuality;	// always what is listed in the file
	int durability;

	int suggestedCraftsmanship;
	int suggestedControl;
};

enum class strategy
{
	standard,	// Maximize success rate, then HQ rate
	hqOrBust,	// Maximize only HQ rate
	nqOnly		// Maximize only success rate
};

enum class goalType
{
	hq,				// Maximize average HQ%
	maxQuality,		// Push quality as far as possible
	collectability	// Hitting the goal is all that matters
};