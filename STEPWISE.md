# What?

Stepwise mode allows you to manually try crafting actions against a simulated crafting session. Alternatively, it can use Advanced Touch's solver to suggest the next action in the craft.

# Using stepwise mode

Start Advanced Touch in 'stepwise' mode, along with the usual options to set your class, stats, recipe, goal, etc.

The current state of the craft, along with all active buffs, will be listed. Afterwards, you may enter commands at the prompt.

# Setting up the status

You may set the craft to any valid state, using the following commands:

s, step s

	Sets the current step. Useful to prevent the solver from suggesting MuMe or Reflect halfway into a craft. e.g. `s 15`

d, durability d

	Sets the current durability.

p, progress p

	Sets the current progress.

q, quality q

	Sets the current quality.

c, condition e/g/n/p (expert: g/n/c/s/pl/pr)

	Sets the current condition to excellent/good/normal/poor. e.g. `c g`

CP c

	Sets the crafter's remaining CP.

buff action time

	Enables a given buff, with its remaining duration as that time, or 0 to clear. The buff name must be in the shortened format that Advanced Touch inputs and outputs.

	For Inner Quiet, set to the number of stacks.

	For Name of the Elements, set to 0 to indicate that it has already been used this craft, or -1 to indicate that it has not.

	Example: `buff wasteNot2 4`

# Simulating

ac, action a s/f/?

	Perform the given crafting action. The action must be in the shortened format that Advanced Touch inputs and outputs. Afterwards, you may set s, f, or ? to indicate whether the action succeeded, failed, or to let AT's RNG decide (the default).

	With s or f, the stepwise will always proc a normal (or poor) condition in the following step. Actions that are guaranteed to succeed ingame are guaranteed in stepwise, even if f is specified.

	Example: `ac patientTouch s`

so, solve g

	Executes the solver against the current crafting status. You may optionally specify a number of generations. The solver will run until it settles on an action to take next, or until 5 times the generation count has been tried. You may cancel the solve early by hitting Control-C

su, suggested s/f/?

	Equivalent to using `ac` on the action that the solver suggests.

n, next c

	Equivalent to using `su s`, then optionally `c` for a condition, then `so`

nf, nextfailed c
	Equivalent to using `su f`, then optionally `c` for a condition, then `so`

u, undo u

	Undoes the last action. You may set a number of times to undo.

reset

	Returns the craft to its initial state, equivalent to undoing as many times as possible. Cannot be undone.

exit, quit

	Ends stepwise mode