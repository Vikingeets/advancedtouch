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
	uint8_t progressFactor;
	uint8_t controlFactor;
};

// Accessed by multiple threads, but only written before they are started
unordered_map<int32_t, recipeData> recipeDataTable;

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
		if (split.size() < 9) continue;

		int32_t rLvl = atoi(split[0].c_str());
		recipeData recipe = {
			static_cast<int8_t>(atoi(split[1].c_str())),	// classLevel
			static_cast<uint8_t>(atoi(split[7].c_str())),	// progressFactor
			static_cast<uint8_t>(atoi(split[8].c_str())),	// controlFactor
		};
		if (rLvl == 0 ||
			recipe.classLevel == 0 ||
			recipe.progressFactor == 0 ||
			recipe.controlFactor == 0)
			continue;

		recipeDataTable.insert(recipeDataTable.end(), pair<int32_t, recipeData>(rLvl, recipe));
	}
	return true;
}

const array<int, 40> cLevelToRLevel = {
	120, 125, 130, 133, 136, 139, 142, 145, 148, 150,	// 51-60
	260, 265, 270, 273, 276, 279, 282, 285, 288, 290,	// 61-70
	385, 395, 400, 403, 406, 409, 412, 415, 418, 420,	// 71-80
	518, 520, 525, 530, 535, 540, 545, 550, 555, 560	// 81-90
};

int mainToRlvl(int level)
{
	if (level <= 50) return level;
	if (level > 90) level = 90;
	return cLevelToRLevel[static_cast<size_t>(level) - 51];
}

int rlvlToMain(int rLvl)
{
	assert(!recipeDataTable.empty());
	
	if (rLvl <= 50) return rLvl;
	auto it = recipeDataTable.find(rLvl);
	if (it == recipeDataTable.end()) return 90;

	return it->second.classLevel;
}

int getProgressFactor(int rLvl)
{
	assert(!recipeDataTable.empty());

	if (rLvl < 1) rLvl = 1;
	auto it = recipeDataTable.find(rLvl);
	if (it == recipeDataTable.end()) return 0;

	return it->second.progressFactor;
}

int getQualityFactor(int rLvl)
{
	assert(!recipeDataTable.empty());

	if (rLvl < 1) rLvl = 1;
	auto it = recipeDataTable.find(rLvl);
	if (it == recipeDataTable.end()) return 0;

	return it->second.controlFactor;
}