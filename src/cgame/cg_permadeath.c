#include "cg_local.h"

/* Returns qtrue if this sound should be suppressed: the DEAD achievement is
   already earned and it's an intro voice-over (but not intro_01 or intro_10). */
qboolean CG_PD_ShouldSuppressIntro( int soundParm ) {
    const char *s;
    char buf[16];
    trap_Cvar_VariableStringBuffer( "pd_achievements", buf, sizeof(buf) );
    if ( !( atoi(buf) & 1 ) )
        return qfalse;
    s = CG_ConfigString( CS_SOUNDS + soundParm );
    if ( !s || !*s ) return qfalse;
    if ( !strstr( s, "intro_" ) ) return qfalse;
    if ( strstr( s, "intro_01" ) || strstr( s, "intro_10" ) ) return qfalse;
    return qtrue;
}

void CG_PermadeathGameOver_Activate( void ) {
    trap_Cvar_Set( "g_spScores1", "" );
    trap_Cvar_Set( "g_spScores2", "" );
    trap_Cvar_Set( "g_spScores3", "" );
    trap_Cvar_Set( "g_spScores4", "" );
    trap_Cvar_Set( "g_spScores5", "" );
    trap_Cvar_Set( "g_spAwards", "" );
    trap_Cvar_Set( "g_spVideos", "" );
    trap_SendConsoleCommand( "writeconfig q3config.cfg\ndisconnect\n" );
}

static qboolean pd_musicStopped;
static qboolean pd_hudSuppressed;

/* Returns qtrue from the first frame the player dies in SP permadeath,
   and stays qtrue until the DLL is unloaded (never resets mid-session). */
qboolean CG_PD_IsHudSuppressed( void ) {
    if ( cgs.gametype != GT_SINGLE_PLAYER ) return qfalse;
    if ( !cg.snap ) return qfalse;
    if ( cg.snap->ps.pm_type == PM_DEAD )
        pd_hudSuppressed = qtrue;
    return pd_hudSuppressed;
}

/* The following stubs satisfy hooks wired in cg_main.c and cg_draw.c. */
void CG_PermadeathGameOver_KeyDown( int key ) {}
void CG_PermadeathGameOver_MouseMove( int dx, int dy ) {}

/* Called every render frame from CG_DrawActive.
   Stops background music immediately when the player dies in SP permadeath,
   so the music doesn't keep playing during the "press to confirm death" wait. */
void CG_DrawPermadeathGameOver( void ) {
    if ( cgs.gametype != GT_SINGLE_PLAYER ) return;
    if ( !cg.snap ) return;
    {
        char pdbuf[8];
        trap_Cvar_VariableStringBuffer( "permadeath_died", pdbuf, sizeof(pdbuf) );
        if ( cg.snap->ps.pm_type == PM_DEAD && atoi(pdbuf) && !pd_musicStopped ) {
            trap_S_StopBackgroundTrack();
            pd_musicStopped = qtrue;
        }
    }
    if ( cg.snap->ps.pm_type != PM_DEAD ) {
        pd_musicStopped = qfalse;
    }
}
