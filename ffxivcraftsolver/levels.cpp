#include "common.h"
#include "levels.h"
#include <array>
#include <unordered_map>
#include <map>
#include <cassert>
#include <fstream>
#include <iostream>
#include <cstdlib>

using namespace std;

struct recipeData
{
	int8_t classLevel;
	int8_t stars;
	uint16_t suggestedCraftsmanship;
	uint16_t suggestedControl;
};

struct differenceData
{
	int16_t progressFactor;
	int16_t qualityFactor;
};

// Accessed by multiple threads, but only written before they are started
unordered_map<int32_t, recipeData> recipeDataTable;
unordered_map<int16_t, differenceData> differenceDataTable;

bool populateRecipeTable(const string& fileName)
{
	ifstream tableFile(fileName, ios::in);
	if (!tableFile) return false;

	recipeDataTable.clear();

	constexpr size_t buffsize = 8192;
	char out[buffsize];
	while (tableFile.getline(out, buffsize))
	{
		vector<string> split = tokenSplit(string(out), ',');
		if (split.size() < 5) continue;

		int32_t rLvl = atoi(split[0].c_str());
		recipeData recipe = {
			static_cast<int8_t>(atoi(split[1].c_str())),
			static_cast<int8_t>(atoi(split[2].c_str())),
			static_cast<uint16_t>(atoi(split[3].c_str())),
			static_cast<uint16_t>(atoi(split[4].c_str()))
		};
		if (rLvl == 0 ||
			recipe.classLevel == 0 ||	// stars can be 0
			recipe.suggestedCraftsmanship == 0 ||
			recipe.suggestedControl == 0)
			continue;

		recipeDataTable.insert(recipeDataTable.end(), pair<int32_t, recipeData>(rLvl, recipe));
	}
	return true;
}

bool populateDifferenceTable(const string& fileName)
{
	ifstream tableFile(fileName, ios::in);
	if (!tableFile) return false;

	differenceDataTable.clear();

	constexpr size_t buffsize = 8192;
	char out[buffsize];
	while (tableFile.getline(out, buffsize))
	{
		vector<string> split = tokenSplit(string(out), ',');
		if (split.size() < 4) continue;

		int16_t difference = atoi(split[1].c_str());
		differenceData diff = {
			static_cast<int16_t>(atoi(split[2].c_str())),
			static_cast<int16_t>(atoi(split[3].c_str())),
		};
		if (diff.progressFactor < 20 ||  diff.qualityFactor < 20)
			continue;

		differenceDataTable.insert(differenceDataTable.end(), pair<int16_t, differenceData>(difference, diff));
	}
	return true;
}

const array<int, 30> cLevelToRLevel = {
	120, 125, 130, 133, 136, 139, 142, 145, 148, 150,	// 51-60
	260, 265, 270, 273, 276, 279, 282, 285, 288, 290,	// 61-70
	385, 395, 400, 403, 406, 409, 412, 415, 418, 420	// 71-80
};

int mainToRlvl(int level)
{
	if (level <= 50) return level;
	if (level > 80) level = 80;
	return cLevelToRLevel[static_cast<size_t>(level) - 51];
}

int rlvlToMain(int rLvl)
{
	assert(!recipeDataTable.empty());
	
	if (rLvl <= 50) return rLvl;
	auto it = recipeDataTable.find(rLvl);
	if (it == recipeDataTable.end()) return 80;

	return it->second.classLevel;
}

int getSuggestedCraftsmanship(int rLvl)
{
	assert(!recipeDataTable.empty());

	if (rLvl < 1) rLvl = 1;
	auto it = recipeDataTable.find(rLvl);
	if (it == recipeDataTable.end()) return 0;

	return it->second.suggestedCraftsmanship;
}

int getSuggestedControl(int rLvl)
{
	assert(!recipeDataTable.empty());

	if (rLvl < 1) rLvl = 1;
	auto it = recipeDataTable.find(rLvl);
	if (it == recipeDataTable.end()) return 0;

	return it->second.suggestedControl;
}

int getProgressFactor(int difference)
{
	assert(!differenceDataTable.empty());

	if (difference < -30) difference = -30;
	else if (difference > 49) difference = 49;

	auto it = differenceDataTable.find(difference);
	if (it == differenceDataTable.end())
	{
		assert(false);
		return 100;
	}

	return it->second.progressFactor;
}

int getQualityFactor(int difference)
{
	assert(!differenceDataTable.empty());

	if (difference < -30) difference = -30;
	else if (difference > 49) difference = 49;

	auto it = differenceDataTable.find(difference);
	if (it == differenceDataTable.end())
	{
		assert(false);
		return 100;
	}

	return it->second.qualityFactor;
}