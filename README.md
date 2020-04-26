# What is this thing?
Advanced Touch is a standalone crafting solver for Final Fantasy 14. It is executed from the command line, so you will have to be familiar with your command prompt.

# Usage
The first argument to the program must be the mode: "single", "multi", "solve", or "step". The remaining arguments can be in any order.

The single mode will simulate a single craft of the given recipe and rotation, which can be useful for test runs of your rotation.
The multi mode will simulate multiple crafts of the recipe and rotation, giving an overview of the results.
The solve mode will produce a JSON rotation which can be imported into a Teamcraft or lokyst simulator.
The stepwise mode will start an interactive mode which can simulate a manual crafting run and suggest actions. See STEPWISE.md for details.

# Arguments
-j class (required)
	The crafter's class, as its three letter abbreviation (CRP, BSM, etc)

-f stats (default: levels.json)
	The crafter's statistics, food, and medicine

-n
	ignore the "food" and "medicine" section of the stats file

-e (default)
	In single and multi mode, simulate an empty rotation. In solve and stepwise mode, use an empty rotation as the seed.

-s sequence
	In single and multi mode, the rotation to simulate. In solve and stepwise mode, the initial seed. **Strongly** recommended for stepwise mode. Use solve mode to generate one if you need to.

-r recipe (required)
	The recipe file to simulate or solve

-o options (default: options.json)
	The options file to use

-i initial (default: 0)
	The initial quality of the synth. This should be set even in solve mode, as less quality to solve for can produce more consistent rotations

-z
	Display various statistics while solving and on the winning rotation. Useful for development, probably less so for normal users. Ignored in stepwise mode

There are three quality options:

-q	(the default)
	In single or multi mode, will show the chance of an HQ synth. In solve mode, will try to maximize the HQ percentage chance

-m
	In single or multi mode, will show the absolute quality produced. In solve mode, will try to maximize the produced quality, regardless of the recipe's maximum

-c collectability
	In single or multi mode, will output whether the given collectability was reached. In solve mode, will try to maximize the number of synths that reach it

There are three solver strategies:

-t standard (the default)
	The solver will seek to maximize the odds of a successful synth, then maximize quality (per the -q, -m, or -c setting)

-t hqorbust
	The solver will consider an NQ result to be equivalent to a failed synth. I.e., it will use rapid synth if it would produce more HQ results than standard

-t nqonly
	The solver will ignore quality and seek a efficient NQ synth macro. Useful for synths you don't need to HQ or if you are super lazy about your beast tribes

# Crafter Stats
The stats file (settable with -f) is a JSON document describing an object with several sub-objects. All sub-objects are optional, can be in any order, and are case-sensitive. Unrecognized objects are ignored and may be useful for commenting.

The "food" and "medicine" objects set the craftsmanship, control and cp percentage increases of those items, and their maximum increases. Any missing values will be set to 0. Passing -n on the command line will disable food and medicine, ignoring any given in the stats file.

The other objects describe the crafter's classes, including their level, craftsmanship, control and cp. Only the craftmanship, control, and cp of the crafter selected with -j is used.

The file "examples/9crp50cul.json" describes a level 9 CRP/50 CUL character using a NQ Tempura Platter.

# Rotation file
The rotation file is a JSON array, containing case-sensitive names for each action in order, compatible with Teamcraft's export to optimizer feature.

The example file "ashlumbersequence.json" can be used with the level 5 carpenter.

# Recipes
The recipe file is required and is a JSON document. All values are case-sensitive.

"rlevel" is the recipe level. Note that this is not necessarily the level shown ingame (especially post-50). Either find a level conversion table, or look up the rlevel of your recipe online, e.g. at Teamcraft or Garland Tools.

"difficulty" is the amount of progress required to complete the synth.

"durability" is the recipe's durability rating.

"quality" is the quality required to guarantee a 100% HQ chance. It should still be provided in max quality or collectability mode, as it is used internally.

"expert" is either true or false to indicate whether the recipe is an expert recipe. However, even in stepwise mode, the solver struggles to reach the lowest turnin threshold.

# Options

The options file is a JSON document with several values. All are case-sensitive.

In multi mode, "sims" sets how many simulations to run, with their results averaged for the output. In solve mode, "sims" set how many simulations should be run on each candidate rotation every generation. A higher number will reduce the effect of randomness on solving, but will take more time to simulate

In solve mode, "generations" sets how many generations should be processed.

In stepwise mode, "stepwise generations" sets how many generations to process by default during a solving run.

In solve mode, "population" sets how many rotations should be considered in each generation.

I recommend setting each solve option to at least a hundred, and starting with all three multiplied together to approximately ten million on a modern computer. Stepwise generations can be safely set to less than 50 as long as you use a decent seed.

Advanced Touch can cache the results of its simulations in memory to accelerate solving. Setting "max cache size" greater than zero will use about 25 megabytes of memory per hundred thousand entries. Please set "sims" to a large number to minimize the risk of a lucky (or unlucky) rotation being cached with a bad result.

Setting "normal lock" to true will disable the simulation of conditions. This will run faster and more consistently, but the solution might not be as good as the solver thinks it is, due to not accounting for the whims of the game's RNG.

"threads" sets how many worker threads the program will use to perform a multi or solve. Setting it to the default of 0 will use as many threads as the computer has cores. (On Windows, the solver shouldn't have an effect on the game's performance even using all cores.)

"use conditionals" instructs the solver to try actions that require a condition (i.e. Intensive Synthesis, Precise Touch, and Tricks of the Trade) in generated rotations. It might come up with something clever with conditions, it might not.

# Examples
`advancedtouch single -j crp -f examples/9crp50cul.json -r examples/ashlumberrecipe.json -s examples/ashlumbersequence.json`

`advancedtouch multi -j crp -f examples/9crp50cul.json -r examples/ashlumberrecipe.json -s examples/ashlumbersequence.json -o examples/options.json`

`advancedtouch solve -j crp -f examples/9crp50cul.json -r examples/ashlumberrecipe.json`

`advancedtouch solve -j crp -f examples/9crp50cul.json -r examples/ashlumberrecipe.json -n -c 40 -t hqorbust -i 100`

# Building?
While an MS Visual Studio solution file is in the repo, at the moment Advanced Touch is very much DIY. I find that clang produces a faster executable on Windows than MSVC. Advanced Touch should build on any system that supports C++11, however.
