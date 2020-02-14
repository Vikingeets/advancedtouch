#include "levels.h"
#include <array>
#include <unordered_map>
#include <map>
#include <cassert>
#include <fstream>
#include <iostream>
#include <cstdlib>

using namespace std;

inline vector<string> commaSplit(const string& str)
{
	vector<string> out;

	size_t position = str.find_first_not_of(',');

	while (position != str.npos)
	{
		size_t nextComma = str.find_first_of(',', position);
		out.push_back(string(str, position, nextComma - position));
		position = str.find_first_not_of(',', nextComma);
	}

	return out;
}

const char* recipeTableFilename = "RecipeLevelTable.csv";

struct recipeData
{
	int8_t classLevel;
	int8_t stars;	// unused
	uint16_t suggestedCraftsmanship;
	uint16_t suggestedControl;
};

// Accessed by multiple threads, but only written before they are started
unordered_map<int32_t, recipeData> recipeDataTable;

void populateRecipeTable()
{
	ifstream tableFile(recipeTableFilename, ios::in);
	if (!tableFile)
	{
		cerr << "Failed to open " << recipeTableFilename << endl;
		exit(1);
	}
	
	constexpr size_t buffsize = 8192;
	char out[buffsize];
	while (tableFile.getline(out, buffsize))
	{
		vector<string> split = commaSplit(string(out));
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
}

const array<int, 30> cLevelToRLevel = {
	120,	// 51
	125,
	130,
	133,
	136,
	139,
	142,
	145,
	148,
	150,	// 60
	260,
	265,
	270,
	273,
	276,
	279,
	282,
	285,
	288,
	290,	// 70
	385,
	395,
	400,
	403,
	406,
	409,
	412,
	415,
	418,
	420		// 80
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
	if (it == recipeDataTable.end()) --it;

	return it->second.suggestedCraftsmanship;
}

int getSuggestedControl(int rLvl)
{
	assert(!recipeDataTable.empty());

	if (rLvl < 1) rLvl = 1;
	auto it = recipeDataTable.find(rLvl);
	if (it == recipeDataTable.end()) --it;

	return it->second.suggestedControl;
}