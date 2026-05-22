# Quake III: Permadeath

A Q3A mod meant for original Single Player experience.

Adds the possibility to lose all progress in the campaign, for those bragging about the AI being too easy even on Nightmare difficulty, well, there's always the permadeath mod!

* **Game over screen** (and 'special' ones, according to how, where and when does the player die! Try to break the permadeath rules... if you can.)
* **Statistics**! Because you need some sense of 'progress' in between wins and losses.
* **Achievements**, because why not?
* Minimal QoL HUD changes: 
    * **Health** count with better red indicator (loops a fade to red from 60 hp and faster the lower you have, instead of starting to blinking from 25hp like in vanilla)
    * **Ammo** and **armor** are white if their values are higher than normal.
* There's Permadeath, and ***True* Permadeath** in case you believe that having extra lives is unfaithful to the concept of permadeath (accessible through `Setup -> Game Options -> True Permadeath`).
* **Auto-record**: to record demos automatically through single player (ON by default: `Setup -> Game Options -> Auto-record`)
* New medal: Haste. Beat the map faster than normal. Can be risky, unless you know what you're doing ;)


---
## Prerequisites

* Quake 3 Arena v1.32 installed 
* quake3e or any ioquake3-based port (highly recommend quake3e these days).

## Installation

1. Create a new subdirectory called "permadeath" in your game directory.
2. You need only the `permadeath.pk3`, `cgame.dll`, `cgamex86_64.dll`, `qagame.dll`, `qagamex86_64.dll`, `ui.dll`, `uix86_64.dll` and `description.txt` inside the "permadeath" subfolder to run
3. Add these parameters to make it work properly:
    ```
    +set fs_game permadeath +set vm_cgame 0 +set vm_game 0 +set vm_ui 0 +set sv_pure 0
    ```
4. Open the game and go to Single Player to start your permadeath run!

