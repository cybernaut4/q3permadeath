//===========================================================================
//
// Name:			krusade_t.c
// Function:		chat lines for Krusade
// Tab Size:		3 (real tabs)
//===========================================================================

chat "krusade"
{
	#include "teamplay.h"
	//
	type "game_enter"
	{
		"The crusade begins.";
		"Stand and fight, or fall where you stand.";
		"I've crossed harder ground than this, ", 1, ".";
		"No quarter given. None expected.";
		HELLO5;
	} //end type

	type "game_exit"
	{
		"The battle moves on. As do I.";
		"Keep your guard up.";
		GOODBYE;
		GOODBYE5;
	} //end type

	type "level_start"
	{
		LEVEL_START0;
		"This ground will be sanctified.";
		"Forward. Always forward.";
		"New arena. Same result.";
	} //end type

	type "level_end"
	{
		"The crusade continues.";
		"Ground taken. Ground held.";
		"Another field cleared.";
	} //end type

	type "level_end_victory"
	{
		"The righteous prevail.";
		"Your cause was lost before it began, ", 3, ".";
		"Victory belongs to those who push forward.";
		"You fought well, ", 3, ". But not well enough.";
		LEVEL_END_VICTORY2;
	} //end type

	type "level_end_lose"
	{
		"A setback. The crusade is not over.";
		"You earned that one, ", 0, ". For now.";
		"I'll be back. Count on it.";
		"Retreat and regroup.";
	} //end type

	type "hit_talking"
	{
		"Cheap shot, ", 0, ".";
		"You've made an enemy today, ", 0, ".";
		"Strike a man mid-word and you'll answer for it.";
	} //end type

	type "damaged_nokill"
	{
		"You'll need to do better than that.";
		"Is that the best you've got?";
	} //end type

	type "hit_nokill"
	{
		"You're slowing down, ", 0, ".";
		"Hold still.";
	} //end type

	type "enemy_suicide"
	{
		"The arena claims another.";
		"Not the death I would have chosen for you, ", 0, ".";
		DEATH_SUICIDE0;
	} //end type

	type "death_telefrag"
	{
		"Rrrgh.";
		"You'll pay for that, ", 0, ".";
		DEATH_TELEFRAGGED1;
	} //end type

	type "death_lava"
	{
		"The ground betrays me.";
		DEATH_LAVA0;
	} //end type

	type "death_slime"
	{
		"Vile stuff.";
		DEATH_SUICIDE1;
	} //end type

	type "death_drown"
	{
		"The deep takes me.";
		DEATH_SUICIDE0;
	} //end type

	type "death_suicide"
	{
		"A warrior's error.";
		"The arena wins this round.";
		DEATH_SUICIDE0;
		DEATH_SUICIDE1;
	} //end type

	type "death_gauntlet"
	{
		"Too close. Too slow.";
		DEATH_GAUNTLET0;
	} //end type

	type "death_rail"
	{
		"Long shot. Good shot.";
		DEATH_RAIL1;
	} //end type

	type "death_bfg"
	{
		"A coward's weapon, ", 0, ".";
		DEATH_BFG1;
	} //end type

	type "death_insult"
	{
		"Remember this moment, ", 0, ". It won't come again.";
		"A single victory does not win a crusade.";
		"Enjoy it while it lasts.";
		"The crusade is not over, ", 0, ".";
	} //end type

	type "death_praise"
	{
		"Well struck, ", 0, ".";
		"You've earned that one.";
		"I underestimated you, ", 0, ". I won't again.";
	} //end type

	type "kill_rail"
	{
		"Precision. That's what separates warriors from soldiers.";
		KILL_RAIL1;
	} //end type

	type "kill_gauntlet"
	{
		"You let me get too close, ", 0, ".";
		KILL_GAUNTLET2;
	} //end type

	type "kill_telefrag"
	{
		TELEFRAGGED5;
		"Move faster next time.";
	} //end type

	type "kill_insult"
	{
		"Forward. Always forward.";
		"The crusade rolls on.";
		"You were in my way, ", 0, ".";
		"Stand aside or be cut down. Your choice.";
		"One less obstacle.";
		0, ", you fought like you had something to prove.  You don't.";
	} //end type

	type "kill_praise"
	{
		"Well fought, ", 0, ". Better luck on your next crusade.";
		"You push hard, ", 0, ". I respect that.";
		"You made me work for that one.";
	} //end type

	type "random_insult"
	{
		TAUNT1;
		"Come forward and face your end, ", 1, ".";
		"Your fight ends here, ", 0, ".";
		"The righteous do not wait, ", 0, ".";
		TAUNT4;
		TAUNT7;
	} //end type

	type "random_misc"
	{
		"Forward.";
		"The crusade continues.";
		"No rest. No retreat.";
		MISC8;
		MISC2;
		"Hold the line, ", 1, ".";
		"Keep pushing.";
	} //end type

} //end chat krusade
