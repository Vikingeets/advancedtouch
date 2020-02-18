#pragma once

enum class classes
{
	CRP,
	BSM,
	ARM,
	GSM,
	LTW,
	WVR,
	ALC,
	CUL
};

struct crafterStats
{
	int level;		// The level shown ingame. Used for ability availability
	int rLevel;		// The equivalent rLevel, e.g. level 70 is rLevel 290
	int craftsmanship;
	int control;
	int CP;
	classes classKind;	// The crafter's class
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
	quality,		// Maximize average HQ%
	maxQuality,		// Push quality as far as possible
	collectability	// Hitting the goal is all that matters
};