#pragma once
#include <string>
#include <utility>

bool populateRecipeTable(const std::string& fileName);

void populateDifferenceTable();

int mainToRlvl(int level);

int rlvlToMain(int level);

int getStars(int rLvl);

int getSuggestedCraftsmanship(int rLvl);

int getSuggestedControl(int rLvl);

int getProgressFactor(int difference);

int getQualityFactor(int difference);

