#pragma once
#include <string>
#include <utility>

bool populateRecipeTable(const std::string& fileName);

bool populateDifferenceTable(const std::string& fileName);

int mainToRlvl(int level);

int rlvlToMain(int level);

int getSuggestedCraftsmanship(int rLvl);

int getSuggestedControl(int rLvl);

int getProgressFactor(int difference);

int getQualityFactor(int difference);

