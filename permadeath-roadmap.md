# Permadeath Roadmap

## First iteration

- [x] Game over screen appears after dying once
- [x] Bugfix: waiting for auto-respawn countdown eludes the Game Over trigger.
- [x] Special game over screen when Restart Arena is selected after dying: "THERE IS NO ESCAPE" instead of GAME OVER
- [x] Special game over screen after `/map_restart`: "THERE IS NO RESTART" instead of GAME OVER

## Second iteration

- [x] Special game over screen after `/kill` is used: "GAME KILLED"
- [x] Special game over screen if the player dies by falling into the abyss: "GAME VOIDED"
- [x] Special game over screen if the player dies by fall damage: "FRACTURED TO DEATH"
- [x] Special game over screen if the player dies by telefrag: "GAME TELEFRAGGED"
- [x] Create a `/debug_pd <number>` command to preview how the game over different variants look like, where "number" is the special Game Over that I want to see (if there's no value in "number", it takes 0 as default, which is the first and normal "GAME OVER").
- [x] Special game over screen if the player dies by self-damage via weaponry and without inflicting any damage to anybody: "EMBARRASING"
- [x] Special game over screen if the player dies by lava damage: "GAME COOKED"
- [x] Special game over screen if the player dies by slime damage: "GAME MELTED"
- [x] Special game over screen if the player drowns in normal water: "GAME UNDER"
- [x] Special game over screen if the player gets squished: "GAME CRUSHED"
- [x] Special game over screen if the player dies 1 second after reaching zero ammo: "GAME UNLOADED"
- [x] Special game over screen if the player dies 10 seconds after using Personal Teleporter: "BANISHED TO" (line 1)
"THE SHADOW REALM" (line 2)
- [x] Special game over screen if the player dies 5 seconds after finding the secret in map q3dm11: 
"YOU GOT A DEATHFISH" (line 1)
"OR WHAT" (line 2)

## Third iteration

- [x] Color the Special Game Over screens with the following accordingly:
        "GAME OVER": default color.
        "YOU GOT A DEATHFISH OR WHAT: default color.
        "BANISHED TO THE SHADOW REALM": purple color.
        "GAME UNLOADED": with "UNLOADED" in yellow color
        "GAME CRUSHED": default color. 
        "GAME UNDER": with "UNDER" in teal color 
        "GAME MELTED": with "MELTED" in green
        "GAME COOKED": with "COOKED" in bright orange
        "EMBARRASING": default color
        "GAME VOIDED": with "VOIDED" in purple color
        "FRACTURED TO DEATH": default color
        "GAME TELEFRAGGED": default color
        "GAME KILLED": default color
        "THERE IS NO RESTART": default color
        "THERE IS NO ESCAPE": default color

## Fourth iteration
- [x] Achievements
    - [x] Trainee: beat Tier 1 in Hurt Me Plenty -> `skill123-tier1.tga`
    - [x] Hardcore Trainee: beat Tier 1 in Hardcore -> `skill4-tier1.tga`
    - [x] Nightmarish Trainee: beat Tier 1 in Nightmare! -> `skill5-tier1.tga`
    - [x] Skilled: beat Tier 2 in Hurt Me Plenty -> `skill123-tier2.tga`
    - [x] Hardcore Skilled: beat Tier 2 in Hardcore -> `skill4-tier2.tga`
    - [x] Nightmarish Skilled: beat Tier 2 in Nightmare! -> `skill5-tier2.tga`
    - [x] Combatant: beat Tier 3 in Hurt Me Plenty -> `skill123-tier3.tga`
    - [x] Hardcore Combatant: beat Tier 3 in Hardcore -> `skill4-tier3.tga`
    - [x] Nightmarish Combatant: beat Tier 3 in Nightmare! -> `skill5-tier3.tga`
    - [x] Warrior: beat Tier 4 in Hurt Me Plenty -> `skill123-tier4.tga`
    - [x] Hardcore Warrior: beat Tier 4 in Hardcore -> `skill4-tier4.tga`
    - [x] Nightmarish Warrior: beat Tier 4 in Nightmare! -> `skill5-tier4.tga`
    - [x] Veteran: beat Tier 5 in Hurt Me Plenty -> `skill123-tier5.tga`
    - [x] Hardcore Veteran: beat Tier 5 in Hardcore -> `skill4-tier5.tga`
    - [x] Nightmarish Veteran: beat Tier 5 in Nightmare! -> `skill5-tier5.tga`
    - [x] Master: beat Tier 6 in Hurt Me Plenty -> `skill123-tier6.tga`
    - [x] Hardcore Master: beat Tier 6 in Hardcore -> `skill4-tier6.tga`
    - [x] Nightmarish Master: beat Tier 6 in Nightmare! -> `skill5-tier6.tga`
    - [x] Elite: beat the game in Hurt Me Plenty without dying -> `skill123-tier7.tga`
    - [x] Hardcore: beat the game in Hardcore without dying -> `skill4-tier7.tga`
    - [x] Nightmare!: beat the game in Nightmare without dying -> `skill5-tier7.tga`
- [x] Quality of life changes
    - [x] If "dead" achivement is already earned, then henceforth disable all intro messages from the first map, except for `intro_01.wav` and `intro_10.wav`
    
- [x] Make a new Achievements menu under Single Player in the main menu
    - [x] Somehow save the progress of all the game overs that were reached, regardless of losing progress in the single player game.
    - [x] Create an achievement per game over unlocked(including the default one). If the game over was reached once, unlock said achievement.
    - [x] The menu is a grid of icons, the same size as the medals in the Single Player menu (the tga files are located in `pak0.pk3/menu/medals`, `pak4.pk3/menu/medals` and `pak5.pk3/menu/medals`).
    - [x] Pick an icon that has the same size of the medals and use it as the "locked achievement". Cannot be stressed enough: do not display the achievement icon or name if they're not unlocked, if it's locked, either pick any image that you can find and resize it to the likes of the medals, or create a dark-gray square in their place.
    - [x] Create achievements for completing the game on each difficulty (you're welcome to either be creative with the names or use the difficulty name if you can't think of better names befitting of their achievements).
    - [x] Have a debug command that toggles unlocking all achievements (0 to show the ones that were unlocked by progress and 1 to force unlock (this debug command shouldn't overwrite achievement progress)).
    - [x] Create a reset button (use `reset_0.tga` and `reset_1.tga`, from `pak0.pk3/menu/art`), place it on the bottom-right of the menu screen and, if activated, ask for a yes/no confirmation (use as popup background: `pak0.pk3/menu/art/cut_frame.tga`)
    - [x] Create a back button (use `back_0.tga` and `back_1.tga` from `pak0.pk3/menu/art`), and place it at the bottom-left corner of the menu screen.
    - [x] When having a vertical scroll bar, use `arrows_vert_0.tga` as background, then `arrows_vert_bot.tga` and `arrows_vert_top.tga` as their respective hovers for scrolling down and up, respectively. The "scrolling" per se isn't exactly a scrolling, but more like page turning. We could instead change it to horizontal arrows, which is more familiar, since it's how it's layed out for the model selection menu within Player.
    - [x] DO NOT USE `player_models_ports.tga` as it's hard to see transparent icons with that noisy background.
    - [x] Draw `maps_select.tga` on top of an achievement when hovering over it.
    - [x] Sort the 'tier completion' achievements by difficulty (also known as "skill"), then by tier (eg. skill123 of tier1, skill123 of tier2, ..., skill123 of tier7, skill4 of tier1, skill4 of tier2, ...)
    - [x] Bugfix: when hovering on the achievement icons, the red square is smaller than the icon:
    ![hardcore veteran icon, with the cursor nearby](image-2.png)
    ![cursor hovering over the hardcore veteran icon](image-3.png)

    - [x] New special game over: fortunately, the THERE IS NO RESTART game over works despite choosing "Leave Arena" instead of "Restart Arena".. it's going to trigger as soon as the player tries to load any arena, which leaves the impression that he got away until the last second, which is nice. Now, let's make a different one if "Leave Arena" is confirmed, the message will be: "OMAE WA MOU SHNDEIRU" (default colors). 

## Fifth iteration
- [x] From the main menu, replace the "Achievements" menu option for "Permadeath" (do not delete it! we're going to relocate the Achievements menu, read more to find out where and how to proceed). 
- [x] The "Permadeath" option will use the "Options/System" submenu as layout. The "System" layout has a two-column layout where the left side is a tab-like menu: "Graphics", "Display", "Sound" and "Network". The second column changes its content based on what was selected on the left. The System menu is accessed in the "Options" menu. The "Permadeath" will use this layout to display a list of statistics on the right side, based on the selected difficulty or "overall" (which is the sum of all difficulties combined). 
    - [x] The statistics will be (and you can change some of their names if you know better ones): "total deaths", "furthest map" (names the map I was in the furthest in the campaign, all the way from q3dm0 to q3tourney7), "longest time alive" ("keeps counting during matches until I die, even after beating the game"), "defeats" (losing a game without dying).
    - [x] The following statistics will appear if they have a value greater than 0 in a specific difficulty (if they happened in any of the difficulties, in "overall" they'll be displayed in a total sum. eg. if fractured happened "2" times in hurt me plenty and "1" time in hardcore, in overall displays "3" times total.):
        - [x] Death reasons: embarrasing, `/kill`, fractured, melted, cooked, abyss, telefragged, wrong place (the one after using the personal teleporter), out of ammo, drowned, deathfish.
- [x] Move the Achievements option inside Permadeath. To understand where to put it exactly, as reference, take a look at "System" again: there's a "Driver info" option that is revealed when "Graphics" is selected, and is displayed below all options inside it. In "Permadeath" menu's case, we're going to display the "Achievements" menu option regardless of difficulty/"overall" option selected on the left. If you're wondering whether "Achievmenets" should be separated by difficulty, no, it's just the same button that is going to access to the very same list of achievements. Regardless of redundancy, I think it's the easiest way to implement it (unless you want to mess with layout boundaries, I think it's best to simply 'copy-paste' the same option and that's it).
- [x] Add new statistics:
    - [x] Matches won: int value of total matches won.
    - [x] Games won: int value of times that the single player campaign was beaten.
    - [x] Medals record: list of highest amount of medals achieved in a run before dying.
- [x] Change the criteria behind the "Defeats" stat: it should count all kinds of defeat (whether the character dies or the match ended with the player not ending in 1st place but surviving nonetheless)
- [x] Add new achievement:
    - [x] Imperfect: lose a match without dying (grayscaled "perfect" icon, and rotated upside-down).
- [x] Add special game over: starts by touching a jumppad, getting hit midair and dies by falling into the void instead of touching the ground: "GAME REDIRECTED"
- [x] Add special game over: player dies while their projectile is still in flight — the projectile scores the winning frag and ends the match, but the player is already dead: "GAME RUINED"
- [x] Add special game over: if the player gets "denied" (signaled when the game plays the "denied.wav") when trying to pick the powerup item and dies within 8 seconds from the sound being played: "GAME DENIED"
- [x] New achievement: near death (have 25 or less health before winning the match)
    - [x] New stat: Near deaths (times the player had won with 25 health or less)
- [x] Remove "Best frags" stat. It doesn't make sense if the amount of frags are going to be the same as the goal per match.
- [x] Immersive changes:
    - [x] When the player dies, do not allow the game to play "sound/feedback/tiedlead.wav" and "sound/feedback/lostlead.wav" until after the game over screen.
    - [x] When the player dies, stop the music.
    - [x] When the player dies, hide the HUD until the game over screen happens.
- [x] Set the project up for git version control. Including build script, the source files in a subfolder, and everything necessary for this version of the mod to run (eg. pk3, dll...)
- [x] Make version explicit: v0.5
    - [x] Print it before and after building the game (use a variable that can be changed from the very first lines)
    - [x] Print it always on game startup (after the main menu loads completely).
    - [x] Inside Permadeath menu, put a text right below its title, use default monospace font, same red color and size as the bottom text in the main menu. Version must be written like so: "Version 0.5"

## Sixth iteration
- [x] HUD behaviour change:
    - [x] Instead of making the health blink with red when low health, change it so that the red tint goes for a looping fade animation by using a formula for the red's opacity, like `mod($time, 1.0)` (it's a formula I use for Material Maker on the Opacity of a blend node (where 0.0 is the background color, 1.0 is the red color we're talking about), translate it for this scenario), and activate it only when health is 60 or under (instead of the current 25 or lower), and make it go faster the lower it gets (minimum animation speed `mod($time*0.5, 1.0)`, and maximum animation speed `mod($time*5, 1.0)`).
    - [x] Have the armor numbers go white if value is >100 (just like health).
    - [x] Have the ammo numbers go white if value is higher than the starting ammo (aka the amount of ammo gotten from when the weapon is picked up).
    - [x] Verify whether the damage absorption changes depending on the armor that was picked up (red or yellow)
        - N/A: `ARMOR_PROTECTION = 0.66` is a single global constant; the game only tracks armor count (`STAT_ARMOR`), not pickup type. Red and yellow armor absorb damage identically — no swap needed.
- [x] New medal: "Haste" (just like the powerup): this is a medal that is rewarded during intermission (like the "Frags" one). To get this medal, the player has to win before the timestamp designated for said map. The timestamps are the following:
    * q3dm0: 45 seconds
    * q3dm1: 105 seconds
    * q3dm2: 135 seconds
    * q3dm3: 105 seconds
    * q3tourney1: 105 seconds
    * q3dm4: 75 seconds
    * q3dm5: 60 seconds
    * q3dm6: 90 seconds
    * q3tourney2: 60 seconds
    * q3dm7: 80 seconds
    * q3dm8: 90 seconds
    * q3dm9: 75 seconds
    * q3tourney3: 45 seconds
    * q3dm10: 90 seconds
    * q3dm11: 75 seconds
    * q3dm12: 75 seconds
    * q3tourney4: 45 seconds
    * q3dm13: 75 seconds
    * q3dm14: 75 seconds
    * q3dm15: 75 seconds
    * q3tourney5: 40 seconds
    * q3dm16: 90 seconds
    * q3dm17: 40 seconds
    * q3dm18: 40 seconds
    * q3dm19: 75 seconds
    * q3tourney6: 45 seconds
FYI these are based on the level leaderboards from speedrun.com. These are based on the last place of each, but rounded up in multipliers of 10 (the ceiling number of each) * 1.5
The sound to play when awarding such medal must be: `baseq3.pk3/sound/items/haste.wav`
- [x] New toggle: auto-record: enabled by default. Found If enabled, every time a match is loaded, start recording a demo with the following filename pattern: `run*number*-skill*number*-mapname`, where:
    * *number* in "run": is the total death count (seen in the permadeath statistics) + 1, the number has 4 padding zeroes (`0000`)
    * *number* in "skill": is the difficulty chosen for the match
    * "mapname": the codename of the map (eg. q3dm0, q3tourney1...).
For example: `run0013-skill3-q3dm3` (so, if this is my 13th run, it means I died 12 times before this run. The "skill3" means hurt me plenty. The map is Arena of Death (q3dm3)).
    If possible, add a separator inside "Setup/Game Options" and add the toggle below said separator.
- [x] Extra Lives system: if the player has no extra lives and dies, permanent death happens (aka game over screen, reset progress as usual). If the player has any extra lives, the player will have a life subtracted before respawning and a sound will be played (the hud will remain displayed and the music will keep going): `pak0.pk3/sound/world/1shot_gong.wav`. When the player starts the entire run, the player starts with zero extra lives. The player will get an extra life every time the player has beaten a tier. (eg. Tier 0 has one map, if player beats q3dm0, the player will have 1 extra life. If the player beats Tier 1's four maps, the player will get an extra life, so that the player will have 2 lives tops by the time the player reaches Tier 2. If the player beats Tier 2 maps without dying, the player will have 3 lives tops, and so on).
    - [x] Add a counter in the HUD at the right of the health count if there's more than zero extra lives. The number will be white. 
    - [x] New toggle: "True permadeath", if this is enabled, there's no extra lives system and it will work as before, you die once, the game is over.
- [ ] Finish readme.md


## Seventh iteration
- [ ] Investigate whether is there a way to seamlessly execute the mod without the need to add a thousand launch parameters to make it work. Otherwise, verify whether the changes are detected for the run to run properly, or make a warning message to the user telling that the game did not run with the required parameters.
- [ ] Investigate whether the campaign progress isn't affected in the vanilla from the mod, or if there's a need to mirror the progress *for* this mod...
- [ ] Rework Achievements screen with the following layout (similar to the final Skirmish screen) using this mockup I put together (bricolaged from screenshots): `/permadeath_src/Originals/Permadeath-screen-2.png`


## Eighth iteration
- [ ] Special game over screen occurs after changing the fraglimit via console, then winning the last match of the campaign: 
    1. A fake 'victory sequence' (it's probably called the intermission) begins, where the winner is teleported first place and executes the taunt.
    2. When the taunt begins, the player explodes after 1 second (an automatic `/kill` occurs) during the victory/defeat screen and the win.wav music stops playing immediately. Three seconds after, disconnect automatically and show the Game Over Screen with a different message:
    "THE VERY END" (line 1) 
    "OF YOU" (line 2)
    * Note: when entering the intermission on the last map of the tier (it usually plays a ), it usually plays the "End" video, which means the game has been beaten. It shouldn't play at all! I believe that if this victory sequence or intermission was a copy of the original, we wouldn't have to hack/intercept a call to the "End" video, and play our own mischievous game over sequence for cheaters.
- [ ] Special game over screen triggers after doing `/fraglimit 1` and win because you already were in 1st place (while not tied for the lead):
    Exactly the same sequence as the previous one, but the message will say:
    "THE ONLY THING" (line 1)
    "THEY FRAG" (line 2)
    "IS YOU" (line 3)