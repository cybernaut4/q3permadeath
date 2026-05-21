#include "g_local.h"

static qboolean pd_gameOverSent[MAX_CLIENTS];
static qboolean pd_pendingRestart;
static int      pd_restartType;
static int      pd_playerMOD;
static qboolean pd_dealtDamage;
static qboolean pd_noAmmoAtDeath;
static int      pd_weaponEmptyTime;    /* level.time last seen holding an empty non-gauntlet weapon */
static int      pd_teleporterUsedTime; /* level.time when Personal Teleporter was consumed */
static int      pd_prevHoldable;       /* STAT_HOLDABLE_ITEM value from previous frame */
static int      pd_secretFoundTime;    /* level.time when the q3dm11 secret was triggered */
static qboolean pd_timeRecorded;       /* qtrue after GameOver records session time this DLL load */
static int      pd_jumppadTime;        /* level.time when human player last touched a jumppad */
static qboolean pd_hitAfterJumppad;    /* received weapon damage while airborne after jumppad */
static int      pd_deniedTime;         /* level.time when EF_AWARD_DENIED flag was first detected */
static int      pd_prevEFlags;         /* ent->client->ps.eFlags from previous ClientThink frame */
static qboolean pd_wasAirborne;        /* player has been airborne since last jumppad use */

/* -----------------------------------------------------------------------
   Fifth iteration: per-track campaign statistics.
   All "pd_s[track]_[stat]" CVARs are registered CVAR_ARCHIVE by the UI.
   The game DLL reads/writes them via trap_Cvar_VariableValue/trap_Cvar_Set.
   ----------------------------------------------------------------------- */

#define PD_CAMPAIGN_SIZE 26
static const char *s_pd_campaign[PD_CAMPAIGN_SIZE] = {
	"q3dm0",
	"q3dm1",  "q3dm2",  "q3dm3",  "q3tourney1",
	"q3dm4",  "q3dm5",  "q3dm6",  "q3tourney2",
	"q3dm7",  "q3dm8",  "q3dm9",  "q3tourney3",
	"q3dm10", "q3dm11", "q3dm12", "q3tourney4",
	"q3dm13", "q3dm14", "q3dm15", "q3tourney5",
	"q3dm16", "q3dm17", "q3dm18", "q3dm19",
	"q3tourney6"
};

static const char *PD_Track( void ) {
	int skill = (int)trap_Cvar_VariableValue( "g_spSkill" );
	if ( skill >= 5 ) return "s5";
	if ( skill >= 4 ) return "s4";
	return "s123";
}

static int PD_CampaignIdx( const char *mapname ) {
	int i;
	for ( i = 0; i < PD_CAMPAIGN_SIZE; i++ ) {
		if ( Q_stricmp( mapname, s_pd_campaign[i] ) == 0 ) return i;
	}
	return -1;
}

static void PD_IncrStat( const char *track, const char *stat ) {
	char cvarname[64];
	int  val;
	Com_sprintf( cvarname, sizeof(cvarname), "pd_%s_%s", track, stat );
	val = (int)trap_Cvar_VariableValue( cvarname );
	trap_Cvar_Set( cvarname, va( "%i", val + 1 ) );
}

/* Record a death for the given game-over type.
   addLevelTime: if qtrue, adds level.time/1000 to curtime before comparing. */
static void PD_RecordDeathStats( int type, qboolean addLevelTime ) {
	const char *tr = PD_Track();
	char curcvar[64], maxcvar[64];
	int  curtime, maxtime;

	Com_sprintf( curcvar, sizeof(curcvar), "pd_%s_curtime", tr );
	Com_sprintf( maxcvar, sizeof(maxcvar), "pd_%s_maxtime", tr );
	curtime = (int)trap_Cvar_VariableValue( curcvar );
	if ( addLevelTime ) curtime += level.time / 1000;
	{
		char totcvar[64];
		int  totaltime;
		Com_sprintf( totcvar, sizeof(totcvar), "pd_%s_totaltime", tr );
		totaltime = (int)trap_Cvar_VariableValue( totcvar );
		trap_Cvar_Set( totcvar, va( "%i", totaltime + curtime ) );
	}
	maxtime = (int)trap_Cvar_VariableValue( maxcvar );
	if ( curtime > maxtime ) trap_Cvar_Set( maxcvar, va( "%i", curtime ) );
	trap_Cvar_Set( curcvar, "0" );

	PD_IncrStat( tr, "deaths" );
	PD_IncrStat( tr, "defeats" ); /* every death is also a defeat */
	switch ( type ) {
		case 4:  PD_IncrStat( tr, "d_kill" );  break;
		case 5:  PD_IncrStat( tr, "d_tfrag" ); break;
		case 6:  PD_IncrStat( tr, "d_frac" );  break;
		case 7:  PD_IncrStat( tr, "d_abyss" ); break;
		case 8:  PD_IncrStat( tr, "d_emb" );   break;
		case 9:  PD_IncrStat( tr, "d_cook" );  break;
		case 10: PD_IncrStat( tr, "d_melt" );  break;
		case 11: PD_IncrStat( tr, "d_drown" ); break;
		case 13: PD_IncrStat( tr, "d_ammo" );  break;
		case 14: PD_IncrStat( tr, "d_tport" ); break;
		case 15: PD_IncrStat( tr, "d_fish" );  break;
		case 17: PD_IncrStat( tr, "d_abyss" ); break; /* redirected — also a void death */
		default: break;
	}
}

/* Called from G_ShutdownGame to accumulate the current level's elapsed time
   into the running session counter, so it persists across map-to-map loads. */
void PermaDeath_Shutdown( void ) {
	const char *tr;
	char  curcvar[64];
	int   curtime;
	if ( g_gametype.integer != GT_SINGLE_PLAYER ) return;
	if ( pd_timeRecorded ) return; /* GameOver already counted this session */
	tr = PD_Track();
	Com_sprintf( curcvar, sizeof(curcvar), "pd_%s_curtime", tr );
	curtime = (int)trap_Cvar_VariableValue( curcvar );
	trap_Cvar_Set( curcvar, va( "%i", curtime + level.time / 1000 ) );
}

/* -----------------------------------------------------------------------
   Match-end tracking — called from LogExit in g_main.c.
   Handles wins, defeats, game completion, medals record, and the
   Imperfect achievement (lost without dying).
   ----------------------------------------------------------------------- */
void PermaDeath_TrackMatchEnd( void ) {
	int        j, playerRank, playerKilled;
	int        impr, excel, gaunt;
	const char *tr;
	char       mapname[MAX_QPATH], cvar[64];
	int        stored;

	if ( g_gametype.integer != GT_SINGLE_PLAYER ) return;
	tr = PD_Track();

	for ( j = 0; j < level.maxclients; j++ ) {
		gclient_t *c = &level.clients[j];
		if ( c->pers.connected != CON_CONNECTED ) continue;
		if ( g_entities[j].r.svFlags & SVF_BOT ) continue;
		if ( pd_gameOverSent[j] ) continue; /* player already died mid-match */

		playerRank   = c->ps.persistant[PERS_RANK];
		playerKilled = c->ps.persistant[PERS_KILLED];

		if ( (int)trap_Cvar_VariableValue( "permadeath_died" ) ) {
			/* Player died this match. If their projectile achieved the winning
			   frag anyway, fire GAME RUINED instead of the normal restart screen. */
			if ( playerRank == 0 ) {
				pd_gameOverSent[j] = qtrue;
				PD_RecordDeathStats( 19, qtrue );
				pd_timeRecorded = qtrue;
				trap_Cvar_Set( "permadeath_died", "0" );
				trap_Cvar_Set( "permadeath_gameOver", "19" );
				trap_Cvar_Set( "pd_achievements", va( "%i",
				    (int)trap_Cvar_VariableValue( "pd_achievements" ) | (1 << 18) ) );
				trap_Cvar_Set( "g_spScores1", "" );
				trap_Cvar_Set( "g_spScores2", "" );
				trap_Cvar_Set( "g_spScores3", "" );
				trap_Cvar_Set( "g_spScores4", "" );
				trap_Cvar_Set( "g_spScores5", "" );
				trap_Cvar_Set( "g_spAwards", "" );
				trap_Cvar_Set( "g_spVideos", "" );
				trap_SendServerCommand( j, "permadeathGameOver" );
			}
			/* else: player died and lost — let InitGame/CheckRestart handle it */
			continue;
		}

		if ( playerRank == 0 ) {
			/* Human player won the match */
			PD_IncrStat( tr, "wins" );
			/* Near death: won with 25 or fewer health */
			if ( g_entities[j].health > 0 && g_entities[j].health <= 25 ) {
				PD_IncrStat( tr, "neardeaths" );
				trap_Cvar_Set( "pd_achievements", va( "%i",
				    (int)trap_Cvar_VariableValue( "pd_achievements" ) | (1 << 20) ) );
			}
			trap_Cvar_VariableStringBuffer( "mapname", mapname, sizeof(mapname) );
			if ( Q_stricmp( mapname, "q3tourney6" ) == 0 ) {
				PD_IncrStat( tr, "gamewon" );
			}
		} else {
			/* Human player lost the match */
			PD_IncrStat( tr, "defeats" );
			if ( playerKilled == 0 ) {
				/* Imperfect: lost without dying (bit 19 of pd_achievements) */
				trap_Cvar_Set( "pd_achievements", va( "%i",
				    (int)trap_Cvar_VariableValue( "pd_achievements" ) | (1 << 19) ) );
			}
		}

		/* Update medals record (best single-match values) */
		impr  = c->ps.persistant[PERS_IMPRESSIVE_COUNT];
		excel = c->ps.persistant[PERS_EXCELLENT_COUNT];
		gaunt = c->ps.persistant[PERS_GAUNTLET_FRAG_COUNT];

#define UPDATE_MAX(sfx, val) \
	do { int _v = (val); \
	     Com_sprintf(cvar, sizeof(cvar), "pd_%s_" sfx, tr); \
	     stored = (int)trap_Cvar_VariableValue(cvar); \
	     if (_v > stored) trap_Cvar_Set(cvar, va("%i", _v)); } while(0)

		UPDATE_MAX("rec_impr",  impr);
		UPDATE_MAX("rec_excel", excel);
		UPDATE_MAX("rec_gaunt", gaunt);
#undef UPDATE_MAX

		break; /* only one human in SP */
	}
}

/* -----------------------------------------------------------------------
   Existing MOD/splash helpers
   ----------------------------------------------------------------------- */

static qboolean PD_IsSplashMOD( int mod ) {
	return ( mod == MOD_GRENADE_SPLASH || mod == MOD_ROCKET_SPLASH ||
	         mod == MOD_PLASMA_SPLASH  || mod == MOD_BFG_SPLASH );
}

void PermaDeath_ClientThink( gentity_t *ent ) {
	int weapon, holdable, eflags;
	if ( g_gametype.integer != GT_SINGLE_PLAYER ) return;
	if ( ent->r.svFlags & SVF_BOT ) return;

	/* Ammo-empty detection */
	weapon = ent->client->ps.weapon;
	if ( weapon != WP_GAUNTLET && ent->client->ps.ammo[weapon] == 0 ) {
		pd_weaponEmptyTime = level.time;
	}

	/* Personal Teleporter consumption detection */
	holdable = ent->client->ps.stats[STAT_HOLDABLE_ITEM];
	if ( pd_prevHoldable > 0 && holdable == 0 ) {
		gitem_t *item = &bg_itemlist[pd_prevHoldable];
		if ( item->giType == IT_HOLDABLE && item->giTag == HI_TELEPORTER ) {
			pd_teleporterUsedTime = level.time;
		}
	}
	pd_prevHoldable = holdable;

	/* EF_AWARD_DENIED rising-edge detection */
	eflags = ent->client->ps.eFlags;
	if ( (eflags & EF_AWARD_DENIED) && !(pd_prevEFlags & EF_AWARD_DENIED) ) {
		pd_deniedTime = level.time;
	}
	pd_prevEFlags = eflags;

	/* Jumppad-airborne tracking: detect landing after jumppad flight */
	if ( pd_jumppadTime > 0 ) {
		if ( ent->client->ps.groundEntityNum == ENTITYNUM_NONE ) {
			pd_wasAirborne = qtrue;
		} else if ( pd_wasAirborne ) {
			/* Player has landed — jumppad flight is over */
			pd_hitAfterJumppad = qfalse;
			pd_wasAirborne     = qfalse;
		}
	}
}

void PermaDeath_TrackJumppad( gentity_t *ent ) {
	if ( g_gametype.integer != GT_SINGLE_PLAYER ) return;
	if ( !ent || !ent->client ) return;
	if ( ent->r.svFlags & SVF_BOT ) return;
	pd_jumppadTime     = level.time;
	pd_hitAfterJumppad = qfalse;
	pd_wasAirborne     = qfalse;
}

void PermaDeath_TrackPlayerHit( gentity_t *targ ) {
	if ( g_gametype.integer != GT_SINGLE_PLAYER ) return;
	if ( !targ || !targ->client ) return;
	if ( targ->r.svFlags & SVF_BOT ) return;
	if ( pd_jumppadTime > 0 &&
	     targ->client->ps.groundEntityNum == ENTITYNUM_NONE ) {
		pd_hitAfterJumppad = qtrue;
	}
}

void PermaDeath_TrackSecret( gentity_t *activator, const char *msg ) {
	char mapname[MAX_QPATH];
	const char *p;
	if ( g_gametype.integer != GT_SINGLE_PLAYER ) return;
	if ( !activator || !activator->client ) return;
	if ( activator->r.svFlags & SVF_BOT ) return;
	trap_Cvar_VariableStringBuffer( "mapname", mapname, sizeof(mapname) );
	if ( Q_stricmp( mapname, "q3dm11" ) != 0 ) return;
	for ( p = msg; *p; p++ ) {
		if ( Q_stricmpn( p, "secret", 6 ) == 0 ) {
			pd_secretFoundTime = level.time;
			return;
		}
	}
}

void PermaDeath_TrackDamage( gentity_t *attacker ) {
	if ( pd_dealtDamage ) return;
	if ( g_gametype.integer != GT_SINGLE_PLAYER ) return;
	if ( !attacker || !attacker->client ) return;
	if ( attacker->r.svFlags & SVF_BOT ) return;
	pd_dealtDamage = qtrue;
}

void PermaDeath_PlayerDied( gentity_t *ent, int meansOfDeath ) {
	int clientNum = ent - g_entities;
	if ( g_gametype.integer != GT_SINGLE_PLAYER ) return;
	if ( ent->r.svFlags & SVF_BOT ) return;
	pd_playerMOD     = meansOfDeath;
	pd_noAmmoAtDeath = ( pd_weaponEmptyTime > 0 &&
	                     level.time - pd_weaponEmptyTime <= 2000 );
	if ( pd_teleporterUsedTime > 0 &&
	     level.time - pd_teleporterUsedTime <= 10000 ) {
		pd_teleporterUsedTime = -1;
	}
	if ( pd_secretFoundTime > 0 &&
	     level.time - pd_secretFoundTime <= 20000 ) {
		pd_secretFoundTime = -1;
	}
	trap_Cvar_Set( "permadeath_died", "1" );
	trap_SendServerCommand( clientNum, "print \"^3[PD] died: permadeath_died=1\n\"" );
}

void PermaDeath_GameOver( gentity_t *ent ) {
	int clientNum = ent - g_entities;
	int type;
	if ( pd_gameOverSent[clientNum] ) return;
	pd_gameOverSent[clientNum] = qtrue;

	if ( pd_teleporterUsedTime == -1 ) {
		type = 14;
	} else if ( !pd_dealtDamage && PD_IsSplashMOD( pd_playerMOD ) ) {
		type = 8;
	} else if ( pd_secretFoundTime == -1 ) {
		type = 15;
	} else if ( pd_deniedTime > 0 &&
	            level.time - pd_deniedTime <= 8000 ) {
		type = 18; /* GAME DENIED */
	} else if ( pd_jumppadTime > 0 && pd_hitAfterJumppad &&
	            pd_playerMOD == MOD_TRIGGER_HURT ) {
		type = 17; /* GAME REDIRECTED */
	} else if ( pd_playerMOD == MOD_TRIGGER_HURT ) {
		type = 7;
	} else if ( pd_playerMOD == MOD_FALLING ) {
		type = 6;
	} else if ( pd_playerMOD == MOD_TELEFRAG ) {
		type = 5;
	} else if ( pd_playerMOD == MOD_SUICIDE ) {
		type = 4;
	} else if ( pd_playerMOD == MOD_LAVA ) {
		type = 9;
	} else if ( pd_playerMOD == MOD_SLIME ) {
		type = 10;
	} else if ( pd_playerMOD == MOD_WATER ) {
		type = 11;
	} else if ( pd_playerMOD == MOD_CRUSH ) {
		type = 12;
	} else if ( pd_noAmmoAtDeath ) {
		type = 13;
	} else {
		type = 1;
	}

	/* record death in stats (includes current level time) */
	PD_RecordDeathStats( type, qtrue );
	pd_timeRecorded = qtrue;

	trap_Cvar_Set( "permadeath_died", "0" );
	trap_Cvar_Set( "permadeath_gameOver", va( "%i", type ) );
	trap_Cvar_Set( "pd_achievements", va( "%i",
	    (int)trap_Cvar_VariableValue( "pd_achievements" ) | (1 << (type - 1)) ) );
	trap_Cvar_Set( "g_spScores1", "" );
	trap_Cvar_Set( "g_spScores2", "" );
	trap_Cvar_Set( "g_spScores3", "" );
	trap_Cvar_Set( "g_spScores4", "" );
	trap_Cvar_Set( "g_spScores5", "" );
	trap_Cvar_Set( "g_spAwards", "" );
	trap_Cvar_Set( "g_spVideos", "" );
	trap_SendServerCommand( clientNum, "permadeathGameOver" );
	G_LogPrintf( "PermaDeath: Game over for %s (type %i, MOD %i) - campaign progress reset\n",
	             ent->client->pers.netname, type, pd_playerMOD );
}

/* Called at the end of G_InitGame after every full DLL reload. */
void PermaDeath_InitGame( void ) {
	int died = (int)trap_Cvar_VariableValue( "permadeath_died" );
	int src  = (int)trap_Cvar_VariableValue( "permadeath_restart_src" );
	trap_Cvar_Set( "permadeath_debug_gt",   va( "%i", g_gametype.integer ) );
	trap_Cvar_Set( "permadeath_debug_died", va( "%i", died ) );
	if ( g_gametype.integer != GT_SINGLE_PLAYER ) return;
	if ( !died ) {
		/* update furthest-map reached for this difficulty track */
		char       mapname[MAX_QPATH];
		int        mapidx, stored;
		const char *tr = PD_Track();
		char       cvarname[64];
		trap_Cvar_VariableStringBuffer( "mapname", mapname, sizeof(mapname) );
		mapidx = PD_CampaignIdx( mapname );
		if ( mapidx >= 0 ) {
			Com_sprintf( cvarname, sizeof(cvarname), "pd_%s_furthest", tr );
			stored = (int)trap_Cvar_VariableValue( cvarname );
			if ( mapidx + 1 > stored )
				trap_Cvar_Set( cvarname, va( "%i", mapidx + 1 ) );
		}
		trap_Cvar_Set( "permadeath_restart_src", "0" );
		return;
	}
	pd_restartType = ( src == 1 ) ? 2 : ( src == 2 ) ? 16 : 3;
	trap_Cvar_Set( "permadeath_restart_src", "0" );
	trap_Cvar_Set( "permadeath_died", "0" );
	pd_pendingRestart = qtrue;
	trap_Cvar_Set( "permadeath_debug_pending", va( "%i", pd_restartType ) );
}

void PermaDeath_CheckRestart( gentity_t *ent ) {
	int clientNum;
	int ach_bit;
	if ( ent->r.svFlags & SVF_BOT ) return;
	if ( g_gametype.integer != GT_SINGLE_PLAYER ) return;
	clientNum = ent - g_entities;
	trap_SendServerCommand( clientNum, va( "print \"^3[PD] CheckRestart c=%i pend=%i type=%i\n\"",
	                                       clientNum, (int)pd_pendingRestart, pd_restartType ) );
	if ( !pd_pendingRestart ) return;
	pd_pendingRestart = qfalse;
	pd_gameOverSent[clientNum] = qtrue;

	/* record death in stats (Shutdown already added level time for the previous session) */
	PD_RecordDeathStats( pd_restartType, qfalse );

	/* Set the correct achievement bit per restart type */
	if ( pd_restartType == 2 )
		ach_bit = 1;   /* ESCAPIST */
	else if ( pd_restartType == 16 )
		ach_bit = 15;  /* OMAE WA MOU */
	else
		ach_bit = 2;   /* RESTARTER */
	trap_Cvar_Set( "pd_achievements", va( "%i",
	    (int)trap_Cvar_VariableValue( "pd_achievements" ) | (1 << ach_bit) ) );
	trap_Cvar_Set( "permadeath_gameOver", va( "%i", pd_restartType ) );
	trap_Cvar_Set( "g_spScores1", "" );
	trap_Cvar_Set( "g_spScores2", "" );
	trap_Cvar_Set( "g_spScores3", "" );
	trap_Cvar_Set( "g_spScores4", "" );
	trap_Cvar_Set( "g_spScores5", "" );
	trap_Cvar_Set( "g_spAwards", "" );
	trap_Cvar_Set( "g_spVideos", "" );
	trap_SendServerCommand( clientNum, "permadeathGameOver" );
	G_LogPrintf( "PermaDeath: No restart for client %i (type %i) - campaign progress reset\n",
	             clientNum, pd_restartType );
}
