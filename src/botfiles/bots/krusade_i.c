//===========================================================================
//
// Name:			krusade_i.c
// Function:		item weights for Krusade
// Tab Size:		4 (real tabs)
//===========================================================================

#include "inv.h"

//initial health/armor states
#define FS_HEALTH				1
#define FS_ARMOR				2

//initial weapon weights
#define W_SHOTGUN				100
#define W_MACHINEGUN			70
#define W_GRENADELAUNCHER		40
#define W_ROCKETLAUNCHER		130
#define W_RAILGUN				90
#define W_BFG10K				30
#define W_LIGHTNING				50
#define W_PLASMAGUN				60

//the bot has the weapons, so the weights change a little bit
#define GWW_SHOTGUN				80
#define GWW_MACHINEGUN			50
#define GWW_GRENADELAUNCHER		30
#define GWW_ROCKETLAUNCHER		100
#define GWW_RAILGUN				30
#define GWW_BFG10K				41
#define GWW_LIGHTNING			40
#define GWW_PLASMAGUN			45

//initial powerup weights
#define W_TELEPORTER			40
#define W_MEDKIT				40
#define W_QUAD					450
#define W_ENVIRO				40
#define W_HASTE					60
#define W_INVISIBILITY			40
#define W_REGEN					400
#define W_FLIGHT				40

//flag weight
#define FLAG_WEIGHT				50

//
#include "fw_items.c"
