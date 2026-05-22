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

static qboolean    pd_hudSuppressed;
static qboolean    pd_deathHandled;  /* qtrue after the first dead frame is processed */
static int         pd_deathTime;
static qboolean    pd_gongScheduled;
static sfxHandle_t pd_gongSfx;

/* Returns qtrue while the HUD should be hidden (died with no extra lives). */
qboolean CG_PD_IsHudSuppressed( void ) {
    if ( cgs.gametype != GT_SINGLE_PLAYER ) return qfalse;
    if ( !cg.snap ) return qfalse;
    return pd_hudSuppressed;
}

/* The following stubs satisfy hooks wired in cg_main.c and cg_draw.c. */
void CG_PermadeathGameOver_KeyDown( int key ) {}
void CG_PermadeathGameOver_MouseMove( int dx, int dy ) {}

/* Fills out[] with the health-warning color: a sawtooth lerp from the normal
   golden color (at opacity 0) to red (at opacity 1).  Speed scales linearly
   with how low health is: 0.5 cycles/s at health=60, 5 cycles/s at health=1. */
void CG_PD_HealthWarningColor( int health, vec4_t out ) {
    float t, speed, alpha;
    int   period_ms, phase_ms;
    t         = (60.0f - (float)health) / 60.0f;           /* 0 at h=60, 1 at h=0 */
    speed     = 0.5f + 4.5f * t;                           /* 0.5 .. 5.0 cycles/s */
    period_ms = (int)(1000.0f / speed);
    if ( period_ms < 1 ) period_ms = 1;
    phase_ms  = cg.time % period_ms;
    alpha     = (float)phase_ms / (float)period_ms;        /* 0..1 sawtooth */
    out[0]    = 1.00f + (0.00f) * alpha;                   /* R: 1.0 -> 1.0  */
    out[1]    = 0.69f + (-0.49f) * alpha;                  /* G: 0.69 -> 0.20 */
    out[2]    = 0.00f + (0.20f) * alpha;                   /* B: 0.0 -> 0.20 */
    out[3]    = 1.0f;
}

/* Called every render frame from CG_DrawActive.
   On the first frame the player is dead:
     - extra lives > 0: play gong 100ms later, leave HUD and music alone
     - extra lives == 0: stop music and hide HUD immediately
   Resets all state when the player is alive again (extra-life respawn). */
void CG_DrawPermadeathGameOver( void ) {
    if ( cgs.gametype != GT_SINGLE_PLAYER ) return;
    if ( !cg.snap ) return;

    if ( cg.snap->ps.pm_type == PM_DEAD ) {
        if ( !pd_deathHandled ) {
            char _lv[4];
            int lives;
            trap_Cvar_VariableStringBuffer( "pd_extra_lives", _lv, sizeof(_lv) );
            lives = atoi( _lv );
            pd_deathHandled = qtrue;
            if ( lives > 0 ) {
                pd_deathTime     = cg.time;
                pd_gongScheduled = qtrue;
            } else {
                trap_S_StopBackgroundTrack();
                pd_hudSuppressed = qtrue;
            }
        }
        if ( pd_gongScheduled && cg.time - pd_deathTime >= 100 ) {
            if ( !pd_gongSfx )
                pd_gongSfx = trap_S_RegisterSound( "sound/world/1shot_gong.wav", qfalse );
            trap_S_StartLocalSound( pd_gongSfx, CHAN_ANNOUNCER );
            pd_gongScheduled = qfalse;
        }
    } else {
        pd_deathHandled  = qfalse;
        pd_gongScheduled = qfalse;
        pd_hudSuppressed = qfalse;
    }
}
