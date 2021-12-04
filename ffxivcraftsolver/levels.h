#pragma once
#include <string>
#include <utility>

bool populateRecipeTable(const std::string& fileName);

int mainToRlvl(int level);

int rlvlToMain(int level);

int getProgressFactor(int rLvl);

int getQualityFactor(int rLvl);

int getProgressPenalty(int rLvl);

int getQualityPenalty(int rLvl);
