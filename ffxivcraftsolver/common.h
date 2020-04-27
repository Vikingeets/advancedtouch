#pragma once
#include <algorithm>
#include <vector>
#include <string>

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

	std::vector<std::pair<int, int>> points;	// must be in order low to high
	
	bool expert;
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
	collectability,	// Hitting the goal is all that matters
	points			// Get the best average point outcome
};

inline std::vector<std::string> tokenSplit(const std::string& str, char token)
{
	std::vector<std::string> out;

	size_t position = str.find_first_not_of(token);

	while (position != str.npos)
	{
		size_t nextComma = str.find_first_of(token, position);
		out.push_back(std::string(str, position, nextComma - position));
		position = str.find_first_not_of(token, nextComma);
	}

	return out;
}

inline std::string lowercase(std::string makeLower)
{
	std::transform(makeLower.begin(), makeLower.end(), makeLower.begin(), tolower);
	return makeLower;
}