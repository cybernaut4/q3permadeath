"""
patch_permadeath.py  -  applies all source changes for the Q3A permadeath mod.
Usage:  python patch_permadeath.py <path_to_ioq3_source_root> [version]

Architecture:
  - qagame: intercepts SP death, respawn attempt, and map_restart-after-death.
            Sets permadeath_gameOver to 1 (GAME OVER) or 2 (THERE IS NO RESTART),
            resets campaign cvars, notifies cgame.
  - cgame:  receives "permadeathGameOver" server command and disconnects.
  - ui:     intercepts UIMENU_MAIN after disconnect, reads permadeath_gameOver,
            shows the full-screen screen (sound + nightmare icon + message).
"""
import re
import sys
import os

def read(path):
    with open(path, 'r', encoding='utf-8', errors='replace') as f:
        return f.read()

def write(path, content):
    with open(path, 'w', encoding='utf-8') as f:
        f.write(content)

def patch_re(path, pattern, replacement, label):
    content = read(path)
    if not re.search(pattern, content, re.DOTALL):
        print('ERROR: Pattern not found for patch "' + label + '"')
        print('  File   : ' + path)
        print('  Pattern: ' + pattern[:200])
        sys.exit(1)
    new_content = re.sub(pattern, replacement, content, count=1, flags=re.DOTALL)
    write(path, new_content)
    print('  [OK] ' + label)

def patch_literal(path, find, replacement, label):
    content = read(path)
    if find not in content:
        print('ERROR: String not found for patch "' + label + '"')
        print('  File   : ' + path)
        print('  Looking for: ' + repr(find[:120]))
        sys.exit(1)
    write(path, content.replace(find, replacement, 1))
    print('  [OK] ' + label)

def already(path, marker):
    return marker in read(path)

def run(ioq3, PD_VERSION='dev'):
    def p(rel):
        return os.path.join(ioq3, rel.replace('/', os.sep))

    print('Applying permadeath source patches...')

    # =========================================================================
    # SERVER-SIDE GAME MODULE  (qagame)
    # =========================================================================

    # -------------------------------------------------------------------------
    # g_local.h: declare all permadeath server functions
    # -------------------------------------------------------------------------
    f = p('code/game/g_local.h')
    if already(f, 'PermaDeath_GameOver'):
        print('  [SKIP] g_local.h already patched')
    else:
        patch_re(f,
                 r'(void\s+ClientRespawn\s*\(\s*gentity_t\s*\*\s*ent\s*\)\s*;)',
                 ('\\1\n'
                  'void PermaDeath_PlayerDied( gentity_t *ent, gentity_t *attacker, int meansOfDeath );\n'
                  'void PermaDeath_GameOver( gentity_t *ent );\n'
                  'void PermaDeath_InitGame( void );\n'
                  'void PermaDeath_CheckRestart( gentity_t *ent );\n'
                  'void PermaDeath_TrackDamage( gentity_t *attacker );\n'
                  'void PermaDeath_ClientThink( gentity_t *ent );\n'
                  'void PermaDeath_TrackSecret( gentity_t *activator, const char *msg );'),
                 'g_local.h: declare permadeath functions')

    # -------------------------------------------------------------------------
    # g_combat.c: mark player as died so restart-after-death can be detected.
    # -------------------------------------------------------------------------
    f = p('code/game/g_combat.c')
    if already(f, 'PermaDeath_PlayerDied'):
        print('  [SKIP] g_combat.c already patched')
    else:
        patch_literal(f,
                      '\tself->client->ps.pm_type = PM_DEAD;\n\n\tif ( attacker ) {',
                      ('\tself->client->ps.pm_type = PM_DEAD;\n'
                       '\tPermaDeath_PlayerDied( self, attacker, meansOfDeath );\n\n'
                       '\tif ( attacker ) {'),
                      'g_combat.c: call PermaDeath_PlayerDied on death')

    # -------------------------------------------------------------------------
    # g_combat.c: track when the human player deals damage to an enemy so the
    # "EMBARRASING" screen fires only when you never hit anyone before dying.
    # Hooked inside the existing PERS_HITS block which already guards
    # attacker->client && targ != attacker, so the check is safe.
    # -------------------------------------------------------------------------
    if already(f, 'PermaDeath_TrackDamage'):
        print('  [SKIP] g_combat.c (TrackDamage) already patched')
    else:
        patch_literal(f,
                      '\t\tattacker->client->ps.persistant[PERS_ATTACKEE_ARMOR] = (targ->health<<8)|(client->ps.stats[STAT_ARMOR]);\n\t}',
                      ('\t\tattacker->client->ps.persistant[PERS_ATTACKEE_ARMOR] = (targ->health<<8)|(client->ps.stats[STAT_ARMOR]);\n'
                       '\t\tPermaDeath_TrackDamage( attacker );\n'
                       '\t}'),
                      'g_combat.c: track damage dealt by human player')

    # -------------------------------------------------------------------------
    # g_target.c: detect q3dm11 secret trigger so the "YOU GOT A DEATHFISH"
    # screen fires if the player dies within 5 seconds of finding the secret.
    # target_print is the entity that sends the "You have found a secret!" cp.
    # -------------------------------------------------------------------------
    f = p('code/game/g_target.c')
    if already(f, 'PermaDeath_TrackSecret'):
        print('  [SKIP] g_target.c already patched')
    else:
        patch_literal(f,
                      'void Use_Target_Print (gentity_t *ent, gentity_t *other, gentity_t *activator) {\n\tif ( activator->client && ( ent->spawnflags & 4 ) ) {',
                      ('void Use_Target_Print (gentity_t *ent, gentity_t *other, gentity_t *activator) {\n'
                       '\tPermaDeath_TrackSecret( activator, ent->message );\n'
                       '\tif ( activator->client && ( ent->spawnflags & 4 ) ) {'),
                      'g_target.c: detect q3dm11 secret for DEATHFISH screen')

    # -------------------------------------------------------------------------
    # g_active.c: intercept the button-press respawn in SP mode.
    # -------------------------------------------------------------------------
    f = p('code/game/g_active.c')
    if already(f, 'PermaDeath_GameOver'):
        print('  [SKIP] g_active.c (button press) already patched')
    else:
        patch_re(f,
                 r'(// pressing attack or use is the normal respawn method\s*)'
                 r'(if\s*\(\s*ucmd->buttons\s*&\s*\(\s*BUTTON_ATTACK\s*\|\s*BUTTON_USE_HOLDABLE\s*\)\s*\)\s*\{)'
                 r'\s*(ClientRespawn\s*\(\s*ent\s*\)\s*;)\s*(\})',
                 ('\\1'
                  '\\2\n'
                  '\t\t\t\tif ( ( g_gametype.integer == GT_SINGLE_PLAYER ) && !( ent->r.svFlags & SVF_BOT ) ) {\n'
                  '\t\t\t\t\tPermaDeath_GameOver( ent );\n'
                  '\t\t\t\t} else {\n'
                  '\t\t\t\t\t\\3\n'
                  '\t\t\t\t}\n'
                  '\t\t\t\\4'),
                 'g_active.c: intercept SP respawn with permadeath check')

    # -------------------------------------------------------------------------
    # g_active.c: intercept the forcerespawn timer path in SP mode.
    # Without this, the player can wait out the respawn timer and bypass
    # permadeath entirely without pressing any button.
    # -------------------------------------------------------------------------
    if already(f, 'pd_forcerespawn'):
        print('  [SKIP] g_active.c (forcerespawn) already patched')
    else:
        patch_re(f,
                 r'(if\s*\(\s*g_forcerespawn\.integer\s*>\s*0[^{]*\{)\s*'
                 r'(ClientRespawn\s*\(\s*ent\s*\)\s*;)\s*(return\s*;)\s*(\})',
                 ('\\1\n'
                  '\t\t\t\t/* pd_forcerespawn */\n'
                  '\t\t\t\tif ( ( g_gametype.integer == GT_SINGLE_PLAYER ) && !( ent->r.svFlags & SVF_BOT ) ) {\n'
                  '\t\t\t\t\tPermaDeath_GameOver( ent );\n'
                  '\t\t\t\t} else {\n'
                  '\t\t\t\t\t\\2\n'
                  '\t\t\t\t}\n'
                  '\t\t\t\t\\3\n'
                  '\t\t\t\\4'),
                 'g_active.c: intercept SP forcerespawn with permadeath check')

    # -------------------------------------------------------------------------
    # g_active.c: per-frame hook at the end of ClientThink_real (alive path only,
    # after the dead-player early return) to detect when the equipped weapon
    # transitions from having ammo to having none, so the "GAME UNLOADED"
    # 2-second window can be measured accurately.
    # -------------------------------------------------------------------------
    if already(f, 'PermaDeath_ClientThink'):
        print('  [SKIP] g_active.c (ClientThink hook) already patched')
    else:
        patch_literal(f,
                      '\t// perform once-a-second actions\n\tClientTimerActions( ent, msec );\n}',
                      ('\t// perform once-a-second actions\n'
                       '\tClientTimerActions( ent, msec );\n'
                       '\tPermaDeath_ClientThink( ent );\n'
                       '}'),
                      'g_active.c: per-frame ammo-empty detection for GAME UNLOADED')

    # -------------------------------------------------------------------------
    # g_trigger.c: detect jumppad use so GAME REDIRECTED (type 17) can fire when
    # the player is hit mid-air after a jumppad launch and dies in the void.
    # -------------------------------------------------------------------------
    f = p('code/game/g_trigger.c')
    if already(f, 'PermaDeath_TrackJumppad'):
        print('  [SKIP] g_trigger.c (jumppad) already patched')
    else:
        patch_literal(f,
                      '\tBG_TouchJumpPad( &other->client->ps, &self->s );\n}',
                      ('\tBG_TouchJumpPad( &other->client->ps, &self->s );\n'
                       '\tPermaDeath_TrackJumppad( other );\n'
                       '}'),
                      'g_trigger.c: track jumppad use for GAME REDIRECTED')

    # -------------------------------------------------------------------------
    # g_combat.c: track when the human player receives weapon damage while
    # airborne after a jumppad, for the GAME REDIRECTED (type 17) detection.
    # -------------------------------------------------------------------------
    f = p('code/game/g_combat.c')
    if already(f, 'PermaDeath_TrackPlayerHit'):
        print('  [SKIP] g_combat.c (TrackPlayerHit) already patched')
    else:
        patch_literal(f,
                      '\t\tattacker->client->ps.persistant[PERS_ATTACKEE_ARMOR] = (targ->health<<8)|(client->ps.stats[STAT_ARMOR]);\n'
                      '\t\tPermaDeath_TrackDamage( attacker );\n'
                      '\t}',
                      ('\t\tattacker->client->ps.persistant[PERS_ATTACKEE_ARMOR] = (targ->health<<8)|(client->ps.stats[STAT_ARMOR]);\n'
                       '\t\tPermaDeath_TrackDamage( attacker );\n'
                       '\t\tPermaDeath_TrackPlayerHit( targ );\n'
                       '\t}'),
                      'g_combat.c: track player hit while airborne after jumppad')

    # -------------------------------------------------------------------------
    # g_client.c: intercept map_restart when the player previously died.
    # -------------------------------------------------------------------------
    f = p('code/game/g_client.c')
    if already(f, 'PermaDeath_CheckRestart'):
        print('  [SKIP] g_client.c already patched')
    else:
        patch_literal(f,
                      '\t// locate ent at a spawn point\n\tClientSpawn( ent );',
                      ('\t// locate ent at a spawn point\n'
                       '\tPermaDeath_CheckRestart( ent );\n'
                       '\tClientSpawn( ent );'),
                      'g_client.c: intercept restart-after-death')

    # -------------------------------------------------------------------------
    # g_main.c: call PermaDeath_InitGame at the end of G_InitGame so the
    # cvar-persisted death flag is converted to a static after every DLL reload.
    # -------------------------------------------------------------------------
    f = p('code/game/g_main.c')
    if already(f, 'PermaDeath_InitGame'):
        print('  [SKIP] g_main.c already patched')
    else:
        patch_literal(f,
                      '\ttrap_SetConfigstring( CS_INTERMISSION, "" );\n}',
                      ('\ttrap_SetConfigstring( CS_INTERMISSION, "" );\n'
                       '\tPermaDeath_InitGame();\n}'),
                      'g_main.c: call PermaDeath_InitGame at end of G_InitGame')

    # -------------------------------------------------------------------------
    # g_cmds.c: add debug_pd <0..18> console command to preview game over screens.
    # 0=GAME OVER, 1=THERE IS NO ESCAPE, 2=THERE IS NO RESTART, 3=GAME KILLED,
    # 4=GAME TELEFRAGGED, 5=FRACTURED TO DEATH, 6=GAME VOIDED, 7=EMBARRASING,
    # 8=GAME COOKED, 9=GAME MELTED, 10=GAME UNDER, 11=GAME CRUSHED,
    # 12=GAME UNLOADED, 13=BANISHED TO/THE SHADOW REALM, 14=DEATHFISH,
    # 15=OMAE WA MOU, 16=GAME REDIRECTED, 17=GAME DENIED, 18=GAME RUINED
    # -------------------------------------------------------------------------
    f = p('code/game/g_cmds.c')
    if already(f, 'debug_pd'):
        print('  [SKIP] g_cmds.c already patched')
    else:
        patch_literal(f,
                      '\telse\n\t\ttrap_SendServerCommand( clientNum, va("print \\"unknown cmd %s\\n\\"", cmd ) );',
                      ('\telse if (Q_stricmp (cmd, "debug_pd") == 0) {\n'
                       '\t\tchar pd_arg[8]; int pd_n;\n'
                       '\t\ttrap_Argv( 1, pd_arg, sizeof(pd_arg) );\n'
                       '\t\tpd_n = pd_arg[0] ? atoi(pd_arg) : 0;\n'
                       '\t\tif ( pd_n < 0 ) pd_n = 0; else if ( pd_n > 18 ) pd_n = 18;\n'
                       '\t\ttrap_Cvar_Set( "permadeath_gameOver", va( "%i", pd_n + 1 ) );\n'
                       '\t\ttrap_SendServerCommand( clientNum, "permadeathGameOver" );\n'
                       '\t}\n'
                       '\telse\n\t\ttrap_SendServerCommand( clientNum, va("print \\"unknown cmd %s\\n\\"", cmd ) );'),
                      'g_cmds.c: add debug_pd command')

    # =========================================================================
    # CLIENT-SIDE GAME MODULE  (cgame)
    # =========================================================================

    # -------------------------------------------------------------------------
    # cg_local.h  1/2: add permadeath fields to cg_t struct.
    # -------------------------------------------------------------------------
    f = p('code/cgame/cg_local.h')
    if already(f, 'permadeathGameOver'):
        print('  [SKIP] cg_local.h (cg_t field) already patched')
    else:
        patch_re(f,
                 r'(\}\s*cg_t\s*;)',
                 '\t// permadeath\n\tqboolean        permadeathGameOver;\n\tint             pdCursorX, pdCursorY;\n\n\\1',
                 'cg_local.h: add permadeath fields to cg_t')

    # -------------------------------------------------------------------------
    # cg_local.h  2/2: forward-declare the permadeath functions.
    # -------------------------------------------------------------------------
    if already(f, 'CG_DrawPermadeathGameOver'):
        print('  [SKIP] cg_local.h (function decls) already patched')
    else:
        patch_re(f,
                 r'(int\s+CG_DrawStrlen\s*\(\s*const\s+char\s*\*\s*str\s*\)\s*;)',
                 ('// permadeath\n'
                  'void CG_PermadeathGameOver_Activate( void );\n'
                  'void CG_PermadeathGameOver_KeyDown( int key );\n'
                  'void CG_PermadeathGameOver_MouseMove( int dx, int dy );\n'
                  'void CG_DrawPermadeathGameOver( void );\n'
                  'qboolean CG_PD_ShouldSuppressIntro( int soundParm );\n\n'
                  '\\1'),
                 'cg_local.h: declare permadeath functions')

    # -------------------------------------------------------------------------
    # cg_local.h  3/4: declare the intro-suppression helper added for QoL.
    # -------------------------------------------------------------------------
    if already(f, 'CG_PD_ShouldSuppressIntro'):
        print('  [SKIP] cg_local.h (CG_PD_ShouldSuppressIntro) already patched')
    else:
        patch_re(f,
                 r'(void\s+CG_DrawPermadeathGameOver\s*\(\s*void\s*\)\s*;)',
                 '\\1\nqboolean CG_PD_ShouldSuppressIntro( int soundParm );',
                 'cg_local.h: declare CG_PD_ShouldSuppressIntro')

    # -------------------------------------------------------------------------
    # cg_local.h  4/4: declare the HUD suppression helper.
    # -------------------------------------------------------------------------
    if already(f, 'CG_PD_IsHudSuppressed'):
        print('  [SKIP] cg_local.h (CG_PD_IsHudSuppressed) already patched')
    else:
        patch_re(f,
                 r'(qboolean\s+CG_PD_ShouldSuppressIntro\s*\(\s*int\s+soundParm\s*\)\s*;)',
                 '\\1\nqboolean CG_PD_IsHudSuppressed( void );',
                 'cg_local.h: declare CG_PD_IsHudSuppressed')

    # -------------------------------------------------------------------------
    # cg_local.h 5/5: declare CG_PD_HealthWarningColor.
    # -------------------------------------------------------------------------
    if already(f, 'CG_PD_HealthWarningColor'):
        print('  [SKIP] cg_local.h (CG_PD_HealthWarningColor) already patched')
    else:
        patch_re(f,
                 r'(qboolean\s+CG_PD_IsHudSuppressed\s*\(\s*void\s*\)\s*;)',
                 '\\1\nvoid CG_PD_HealthWarningColor( int health, vec4_t out );',
                 'cg_local.h: declare CG_PD_HealthWarningColor')

    # -------------------------------------------------------------------------
    # cg_players.c: redirect sound loading for the krusade skin so it uses
    # sound/player/krusade/ instead of sound/player/sarge/.
    # -------------------------------------------------------------------------
    f = p('code/cgame/cg_players.c')
    if already(f, 'pd_krusade_sounds'):
        print('  [SKIP] cg_players.c (krusade sounds) already patched')
    else:
        patch_literal(f,
                      '\t// sounds\n'
                      '\tdir = ci->modelName;\n'
                      '\tfallback = (cgs.gametype >= GT_TEAM) ? DEFAULT_TEAM_MODEL : DEFAULT_MODEL;',
                      ('\t// sounds\n'
                       '\tdir = ci->modelName;\n'
                       '\t/* pd_krusade_sounds */\n'
                       '\tif ( Q_stricmp( ci->skinName, "krusade" ) == 0 ) dir = "krusade";\n'
                       '\tfallback = (cgs.gametype >= GT_TEAM) ? DEFAULT_TEAM_MODEL : DEFAULT_MODEL;'),
                      'cg_players.c: krusade skin uses sound/player/krusade/')

    # -------------------------------------------------------------------------
    # cg_draw.c: replace the health-number color block.
    # Threshold changes from 25 to 60; blinking replaced with sawtooth fade.
    # -------------------------------------------------------------------------
    f = p('code/cgame/cg_draw.c')
    if already(f, 'CG_PD_HealthWarningColor'):
        print('  [SKIP] cg_draw.c (health warning color) already patched')
    else:
        patch_literal(f,
                      '\tvalue = ps->stats[STAT_HEALTH];\n'
                      '\tif ( value > 100 ) {\n'
                      '\t\ttrap_R_SetColor( colors[3] );\t\t// white\n'
                      '\t} else if (value > 25) {\n'
                      '\t\ttrap_R_SetColor( colors[0] );\t// green\n'
                      '\t} else if (value > 0) {\n'
                      '\t\tcolor = (cg.time >> 8) & 1;\t// flash\n'
                      '\t\ttrap_R_SetColor( colors[color] );\n'
                      '\t} else {\n'
                      '\t\ttrap_R_SetColor( colors[1] );\t// red\n'
                      '\t}',
                      ('\tvalue = ps->stats[STAT_HEALTH];\n'
                       '\tif ( value > 100 ) {\n'
                       '\t\ttrap_R_SetColor( colors[3] );\t\t// white\n'
                       '\t} else if (value > 60) {\n'
                       '\t\ttrap_R_SetColor( colors[0] );\t// normal\n'
                       '\t} else if (value > 0) {\n'
                       '\t\t{\n'
                       '\t\t\tvec4_t wc;\n'
                       '\t\t\tCG_PD_HealthWarningColor( value, wc );\n'
                       '\t\t\ttrap_R_SetColor( wc );\n'
                       '\t\t}\n'
                       '\t} else {\n'
                       '\t\ttrap_R_SetColor( colors[1] );\t// red\n'
                       '\t}'),
                      'cg_draw.c: replace health blink with sawtooth fade (threshold 60)')

    # -------------------------------------------------------------------------
    # cg_draw.c: armor goes white when > 100 (mirrors health > 100 behavior).
    # -------------------------------------------------------------------------
    if already(f, 'pd_armor_white'):
        print('  [SKIP] cg_draw.c (armor white >100) already patched')
    else:
        patch_literal(f,
                      '\tvalue = ps->stats[STAT_ARMOR];\n'
                      '\tif (value > 0 ) {\n'
                      '\t\ttrap_R_SetColor( colors[0] );\n'
                      '\t\tCG_DrawField (370, 432, 3, value);',
                      ('\tvalue = ps->stats[STAT_ARMOR];\n'
                       '\tif (value > 0 ) {\n'
                       '\t\ttrap_R_SetColor( value > 100 ? colors[3] : colors[0] ); /* pd_armor_white */\n'
                       '\t\tCG_DrawField (370, 432, 3, value);'),
                      'cg_draw.c: armor white when >100')

    # -------------------------------------------------------------------------
    # cg_draw.c: ammo goes white when above the weapon's pickup quantity.
    # Table indexed by WP_* (0-10): {none,gauntlet,mg,sg,gl,rl,lg,rail,pg,bfg,hook}
    # -------------------------------------------------------------------------
    if already(f, 'pd_startAmmo'):
        print('  [SKIP] cg_draw.c (ammo white >pickup) already patched')
    else:
        patch_literal(f,
                      '\t\t\t} else {\n'
                      '\t\t\t\tif ( value >= 0 ) {\n'
                      '\t\t\t\t\tcolor = 0;\t// green\n'
                      '\t\t\t\t} else {\n'
                      '\t\t\t\t\tcolor = 1;\t// red\n'
                      '\t\t\t\t}\n'
                      '\t\t\t}',
                      ('\t\t\t} else {\n'
                       '\t\t\t\tif ( value >= 0 ) {\n'
                       '\t\t\t\t\t{\n'
                       '\t\t\t\t\t\tstatic const int pd_startAmmo[11] = {0,0,40,10,10,10,100,10,50,20,0};\n'
                       '\t\t\t\t\t\tint _wp = cent->currentState.weapon;\n'
                       '\t\t\t\t\t\tcolor = (_wp >= 0 && _wp < 11 && value > pd_startAmmo[_wp]) ? 3 : 0;\n'
                       '\t\t\t\t\t}\n'
                       '\t\t\t\t} else {\n'
                       '\t\t\t\t\tcolor = 1;\t// red\n'
                       '\t\t\t\t}\n'
                       '\t\t\t}'),
                      'cg_draw.c: ammo white when above weapon pickup quantity')

    # -------------------------------------------------------------------------
    # cg_event.c: suppress intro_XX sounds (except intro_01/intro_10) once the
    # DEAD achievement has been earned (pd_achievements bit 0 set).
    # -------------------------------------------------------------------------
    f = p('code/cgame/cg_event.c')
    if already(f, 'CG_PD_ShouldSuppressIntro'):
        print('  [SKIP] cg_event.c (suppress intro) already patched')
    else:
        patch_literal(f,
                      '\tcase EV_GENERAL_SOUND:\n'
                      '\t\tDEBUGNAME("EV_GENERAL_SOUND");\n'
                      '\t\tif ( cgs.gameSounds[ es->eventParm ] ) {\n'
                      '\t\t\ttrap_S_StartSound (NULL, es->number, CHAN_VOICE, cgs.gameSounds[ es->eventParm ] );\n'
                      '\t\t} else {\n'
                      '\t\t\ts = CG_ConfigString( CS_SOUNDS + es->eventParm );\n'
                      '\t\t\ttrap_S_StartSound (NULL, es->number, CHAN_VOICE, CG_CustomSound( es->number, s ) );\n'
                      '\t\t}\n'
                      '\t\tbreak;\n'
                      '\n'
                      '\tcase EV_GLOBAL_SOUND:\t// play from the player\'s head so it never diminishes\n'
                      '\t\tDEBUGNAME("EV_GLOBAL_SOUND");\n'
                      '\t\tif ( cgs.gameSounds[ es->eventParm ] ) {\n'
                      '\t\t\ttrap_S_StartSound (NULL, cg.snap->ps.clientNum, CHAN_AUTO, cgs.gameSounds[ es->eventParm ] );\n'
                      '\t\t} else {\n'
                      '\t\t\ts = CG_ConfigString( CS_SOUNDS + es->eventParm );\n'
                      '\t\t\ttrap_S_StartSound (NULL, cg.snap->ps.clientNum, CHAN_AUTO, CG_CustomSound( es->number, s ) );\n'
                      '\t\t}\n'
                      '\t\tbreak;',
                      ('\tcase EV_GENERAL_SOUND:\n'
                       '\t\tDEBUGNAME("EV_GENERAL_SOUND");\n'
                       '\t\tif ( !CG_PD_ShouldSuppressIntro( es->eventParm ) ) {\n'
                       '\t\t\tif ( cgs.gameSounds[ es->eventParm ] ) {\n'
                       '\t\t\t\ttrap_S_StartSound (NULL, es->number, CHAN_VOICE, cgs.gameSounds[ es->eventParm ] );\n'
                       '\t\t\t} else {\n'
                       '\t\t\t\ts = CG_ConfigString( CS_SOUNDS + es->eventParm );\n'
                       '\t\t\t\ttrap_S_StartSound (NULL, es->number, CHAN_VOICE, CG_CustomSound( es->number, s ) );\n'
                       '\t\t\t}\n'
                       '\t\t}\n'
                       '\t\tbreak;\n'
                       '\n'
                       '\tcase EV_GLOBAL_SOUND:\t// play from the player\'s head so it never diminishes\n'
                       '\t\tDEBUGNAME("EV_GLOBAL_SOUND");\n'
                       '\t\tif ( !CG_PD_ShouldSuppressIntro( es->eventParm ) ) {\n'
                       '\t\t\tif ( cgs.gameSounds[ es->eventParm ] ) {\n'
                       '\t\t\t\ttrap_S_StartSound (NULL, cg.snap->ps.clientNum, CHAN_AUTO, cgs.gameSounds[ es->eventParm ] );\n'
                       '\t\t\t} else {\n'
                       '\t\t\t\ts = CG_ConfigString( CS_SOUNDS + es->eventParm );\n'
                       '\t\t\t\ttrap_S_StartSound (NULL, cg.snap->ps.clientNum, CHAN_AUTO, CG_CustomSound( es->number, s ) );\n'
                       '\t\t\t}\n'
                       '\t\t}\n'
                       '\t\tbreak;'),
                      'cg_event.c: suppress intro voice-overs after DEAD achievement')

    # -------------------------------------------------------------------------
    # cg_playerstate.c: suppress "tied for the lead" and "lost the lead" sounds
    # while the player is dead in permadeath, so they don't play during the
    # wait between dying and pressing a button for the game over screen.
    # -------------------------------------------------------------------------
    f = p('code/cgame/cg_playerstate.c')
    if already(f, 'permadeath_died'):
        print('  [SKIP] cg_playerstate.c (suppress lead sounds) already patched')
    else:
        patch_literal(f,
                      '\t\t\t\t\t} else if ( ps->persistant[PERS_RANK] == RANK_TIED_FLAG ) {\n'
                      '\t\t\t\t\t\tCG_AddBufferedSound(cgs.media.tiedLeadSound);\n'
                      '\t\t\t\t\t} else if ( ( ops->persistant[PERS_RANK] & ~RANK_TIED_FLAG ) == 0 ) {\n'
                      '\t\t\t\t\t\tCG_AddBufferedSound(cgs.media.lostLeadSound);\n'
                      '\t\t\t\t\t}',
                      ('\t\t\t\t\t} else if ( ps->persistant[PERS_RANK] == RANK_TIED_FLAG ) {\n'
                       '\t\t\t\t\t\t{ char _pdb[4]; trap_Cvar_VariableStringBuffer("permadeath_died",_pdb,sizeof(_pdb)); if(!atoi(_pdb)) CG_AddBufferedSound(cgs.media.tiedLeadSound); }\n'
                       '\t\t\t\t\t} else if ( ( ops->persistant[PERS_RANK] & ~RANK_TIED_FLAG ) == 0 ) {\n'
                       '\t\t\t\t\t\t{ char _pdb[4]; trap_Cvar_VariableStringBuffer("permadeath_died",_pdb,sizeof(_pdb)); if(!atoi(_pdb)) CG_AddBufferedSound(cgs.media.lostLeadSound); }\n'
                       '\t\t\t\t\t}'),
                      'cg_playerstate.c: suppress tiedlead/lostlead during permadeath death')

    # -------------------------------------------------------------------------
    # cg_servercmds.c: receive "permadeathGameOver" and call Activate.
    # -------------------------------------------------------------------------
    f = p('code/cgame/cg_servercmds.c')
    if already(f, 'permadeathGameOver'):
        print('  [SKIP] cg_servercmds.c already patched')
    else:
        patch_re(f,
                 r'(if\s*\(\s*!cmd\s*\[\s*0\s*\]\s*\)\s*\{[^}]*\})',
                 ('\\1\n\n'
                  '\tif ( !strcmp( cmd, "permadeathGameOver" ) ) {\n'
                  '\t\tCG_PermadeathGameOver_Activate();\n'
                  '\t\treturn;\n'
                  '\t}'),
                 'cg_servercmds.c: handle permadeathGameOver command')

    # -------------------------------------------------------------------------
    # cg_main.c: wire CG_KeyEvent and CG_MouseEvent to stubs.
    # -------------------------------------------------------------------------
    f = p('code/cgame/cg_main.c')
    if already(f, 'CG_PermadeathGameOver_KeyDown'):
        print('  [SKIP] cg_main.c already patched')
    else:
        patch_re(f,
                 r'void\s+CG_KeyEvent\s*\(\s*int\s+key\s*,\s*qboolean\s+down\s*\)\s*\{\s*\}',
                 ('void CG_KeyEvent( int key, qboolean down ) {\n'
                  '\tif ( down ) {\n'
                  '\t\tCG_PermadeathGameOver_KeyDown( key );\n'
                  '\t}\n'
                  '}'),
                 'cg_main.c: CG_KeyEvent handler')

        patch_re(f,
                 r'void\s+CG_MouseEvent\s*\(\s*int\s+x\s*,\s*int\s+y\s*\)\s*\{\s*\}',
                 ('void CG_MouseEvent( int x, int y ) {\n'
                  '\tCG_PermadeathGameOver_MouseMove( x, y );\n'
                  '}'),
                 'cg_main.c: CG_MouseEvent handler')

    # -------------------------------------------------------------------------
    # cg_draw.c: call CG_DrawPermadeathGameOver() stub in CG_DrawActive.
    # -------------------------------------------------------------------------
    f = p('code/cgame/cg_draw.c')
    if already(f, 'CG_DrawPermadeathGameOver'):
        print('  [SKIP] cg_draw.c already patched')
    else:
        patch_re(f,
                 r'(CG_Draw2D\s*\(\s*stereoView\s*\)\s*;)(\s*\})',
                 '\\1\n\tCG_DrawPermadeathGameOver();\\2',
                 'cg_draw.c: call CG_DrawPermadeathGameOver in CG_DrawActive')

    if already(f, 'CG_PD_IsHudSuppressed'):
        print('  [SKIP] cg_draw.c (HUD suppression) already patched')
    else:
        patch_re(f,
                 r'[ \t]+CG_Draw2D\s*\(\s*stereoView\s*\)\s*;',
                 '\tif ( !CG_PD_IsHudSuppressed() ) CG_Draw2D(stereoView);',
                 'cg_draw.c: suppress HUD while dead in permadeath')

    # =========================================================================
    # UI MODULE  (q3_ui)
    # =========================================================================

    # -------------------------------------------------------------------------
    # ui_local.h: declare UI_PermadeathGameOver().
    # -------------------------------------------------------------------------
    f = p('code/q3_ui/ui_local.h')
    if already(f, 'UI_PermadeathGameOver'):
        print('  [SKIP] ui_local.h already patched')
    else:
        patch_re(f,
                 r'(extern void\s+UI_DrawHandlePic\s*\()',
                 'extern void\t\t\tUI_PermadeathGameOver( void );\n\\1',
                 'ui_local.h: declare UI_PermadeathGameOver')

    # -------------------------------------------------------------------------
    # ui_ingame.c: tag Restart Arena (src=1) and Leave Arena (src=2) so the
    # server can show different game-over screens for each.
    # -------------------------------------------------------------------------
    f = p('code/q3_ui/ui_ingame.c')
    if already(f, 'permadeath_restart_src'):
        print('  [SKIP] ui_ingame.c already patched')
    else:
        patch_literal(f,
                      '\tUI_PopMenu();\n\ttrap_Cmd_ExecuteText( EXEC_APPEND, "map_restart 0\\n" );',
                      ('\tUI_PopMenu();\n'
                       '\ttrap_Cvar_Set( "permadeath_restart_src", "1" );\n'
                       '\ttrap_Cmd_ExecuteText( EXEC_APPEND, "map_restart 0\\n" );'),
                      'ui_ingame.c: tag Restart Arena as source 1')

        patch_literal(f,
                      '\tcase ID_LEAVEARENA:\n'
                      '\t\ttrap_Cmd_ExecuteText( EXEC_APPEND, "disconnect\\n" );\n'
                      '\t\tbreak;',
                      ('\tcase ID_LEAVEARENA:\n'
                       '\t\ttrap_Cvar_Set( "permadeath_restart_src", "2" );\n'
                       '\t\ttrap_Cmd_ExecuteText( EXEC_APPEND, "disconnect\\n" );\n'
                       '\t\tbreak;'),
                      'ui_ingame.c: tag Leave Arena as source 2')

    # -------------------------------------------------------------------------
    # ui_atoms.c: intercept UIMENU_MAIN when permadeath_gameOver cvar is set.
    # UI_PermadeathGameOver() reads and clears the cvar itself.
    # -------------------------------------------------------------------------
    f = p('code/q3_ui/ui_atoms.c')
    if already(f, 'permadeath_gameOver'):
        print('  [SKIP] ui_atoms.c already patched')
    else:
        patch_literal(f,
                      '\tcase UIMENU_MAIN:\n\t\tUI_MainMenu();\n\t\treturn;',
                      ('\tcase UIMENU_MAIN:\n'
                       '\t\tif ( trap_Cvar_VariableValue( "permadeath_gameOver" ) ) {\n'
                       '\t\t\tUI_PermadeathGameOver();\n'
                       '\t\t\treturn;\n'
                       '\t\t}\n'
                       '\t\tUI_MainMenu();\n'
                       '\t\treturn;'),
                      'ui_atoms.c: intercept UIMENU_MAIN for permadeath game over')

    # =========================================================================
    # BUILD SYSTEM
    # =========================================================================

    f = p('cmake/basegame.cmake')
    if already(f, 'g_permadeath.c'):
        print('  [SKIP] basegame.cmake: g_permadeath.c already present')
    else:
        patch_literal(f,
                      '${SOURCE_DIR}/game/g_weapon.c',
                      '${SOURCE_DIR}/game/g_weapon.c\n    ${SOURCE_DIR}/game/g_permadeath.c',
                      'basegame.cmake: add g_permadeath.c')

    if already(f, 'cg_permadeath.c'):
        print('  [SKIP] basegame.cmake: cg_permadeath.c already present')
    else:
        patch_literal(f,
                      '${SOURCE_DIR}/cgame/cg_servercmds.c',
                      '${SOURCE_DIR}/cgame/cg_servercmds.c\n    ${SOURCE_DIR}/cgame/cg_permadeath.c',
                      'basegame.cmake: add cg_permadeath.c')

    if already(f, 'ui_permadeath.c'):
        print('  [SKIP] basegame.cmake: ui_permadeath.c already present')
    else:
        patch_literal(f,
                      '${SOURCE_DIR}/q3_ui/ui_video.c',
                      '${SOURCE_DIR}/q3_ui/ui_video.c\n    ${SOURCE_DIR}/q3_ui/ui_permadeath.c',
                      'basegame.cmake: add ui_permadeath.c')

    if already(f, 'ui_achievements.c'):
        print('  [SKIP] basegame.cmake: ui_achievements.c already present')
    else:
        patch_literal(f,
                      '${SOURCE_DIR}/q3_ui/ui_permadeath.c',
                      '${SOURCE_DIR}/q3_ui/ui_permadeath.c\n    ${SOURCE_DIR}/q3_ui/ui_achievements.c',
                      'basegame.cmake: add ui_achievements.c')

    # -------------------------------------------------------------------------
    # ui_local.h: declare UI_AchievementsMenu().
    # -------------------------------------------------------------------------
    f = p('code/q3_ui/ui_local.h')
    if already(f, 'UI_AchievementsMenu'):
        print('  [SKIP] ui_local.h (UI_AchievementsMenu) already patched')
    else:
        patch_re(f,
                 r'(extern void\s+UI_PermadeathGameOver\s*\(\s*void\s*\)\s*;)',
                 '\\1\nextern void\t\t\tUI_AchievementsMenu( void );',
                 'ui_local.h: declare UI_AchievementsMenu')

    # -------------------------------------------------------------------------
    # ui_main.c: register pd_achievements and pd_tier_ach as CVAR_ARCHIVE.
    # -------------------------------------------------------------------------
    f = p('code/q3_ui/ui_main.c')
    if already(f, 'pd_achievements'):
        print('  [SKIP] ui_main.c: pd_achievements already present')
    else:
        patch_literal(f,
                      'vmCvar_t\tui_cdkeychecked;\n'
                      'vmCvar_t\tui_ioq3;\n',
                      ('vmCvar_t\tui_cdkeychecked;\n'
                       'vmCvar_t\tui_ioq3;\n'
                       'vmCvar_t\tui_pdAchievements;\n'
                       'vmCvar_t\tui_pdTierAch;\n'),
                      'ui_main.c: declare achievement vmCvar_t variables')

        patch_literal(f,
                      '\t{ &ui_cdkeychecked, "ui_cdkeychecked", "0", CVAR_ROM },\n'
                      '\t{ &ui_ioq3, "ui_ioq3", "1", CVAR_ROM },',
                      ('\t{ &ui_cdkeychecked, "ui_cdkeychecked", "0", CVAR_ROM },\n'
                       '\t{ &ui_ioq3, "ui_ioq3", "1", CVAR_ROM },\n'
                       '\t{ &ui_pdAchievements, "pd_achievements", "0", CVAR_ARCHIVE },\n'
                       '\t{ &ui_pdTierAch, "pd_tier_ach", "0", CVAR_ARCHIVE },'),
                      'ui_main.c: register achievement cvars in cvarTable')

    if already(f, 'pd_tier_ach'):
        print('  [SKIP] ui_main.c: pd_tier_ach already present')
    else:
        patch_literal(f,
                      'vmCvar_t\tui_pdAchievements;\n',
                      ('vmCvar_t\tui_pdAchievements;\n'
                       'vmCvar_t\tui_pdTierAch;\n'),
                      'ui_main.c: declare ui_pdTierAch vmCvar_t')

        patch_literal(f,
                      '\t{ &ui_pdAchievements, "pd_achievements", "0", CVAR_ARCHIVE },',
                      ('\t{ &ui_pdAchievements, "pd_achievements", "0", CVAR_ARCHIVE },\n'
                       '\t{ &ui_pdTierAch, "pd_tier_ach", "0", CVAR_ARCHIVE },'),
                      'ui_main.c: register pd_tier_ach in cvarTable')

    # -------------------------------------------------------------------------
    # ui_menu.c: add ACHIEVEMENTS button to the main menu, after SINGLE PLAYER.
    # -------------------------------------------------------------------------
    f = p('code/q3_ui/ui_menu.c')
    if already(f, 'ID_ACHIEVEMENTS'):
        print('  [SKIP] ui_menu.c: ACHIEVEMENTS already present')
    else:
        patch_literal(f,
                      '#define ID_SINGLEPLAYER\t\t\t10',
                      ('#define ID_SINGLEPLAYER\t\t\t10\n'
                       '#define ID_ACHIEVEMENTS\t\t\t18'),
                      'ui_menu.c: define ID_ACHIEVEMENTS')

        patch_literal(f,
                      '\tmenutext_s\t\tsingleplayer;\n'
                      '\tmenutext_s\t\tmultiplayer;',
                      ('\tmenutext_s\t\tsingleplayer;\n'
                       '\tmenutext_s\t\tachievements;\n'
                       '\tmenutext_s\t\tmultiplayer;'),
                      'ui_menu.c: add achievements field to mainmenu_t')

        patch_literal(f,
                      '\tcase ID_SINGLEPLAYER:\n'
                      '\t\tUI_SPLevelMenu();\n'
                      '\t\tbreak;\n'
                      '\n'
                      '\tcase ID_MULTIPLAYER:',
                      ('\tcase ID_SINGLEPLAYER:\n'
                       '\t\tUI_SPLevelMenu();\n'
                       '\t\tbreak;\n'
                       '\n'
                       '\tcase ID_ACHIEVEMENTS:\n'
                       '\t\tUI_PdStatsMenu();\n'
                       '\t\tbreak;\n'
                       '\n'
                       '\tcase ID_MULTIPLAYER:'),
                      'ui_menu.c: handle ID_ACHIEVEMENTS in Main_MenuEvent')

        patch_literal(f,
                      '\ty += MAIN_MENU_VERTICAL_SPACING;\n'
                      '\ts_main.multiplayer.generic.type',
                      ('\ty += MAIN_MENU_VERTICAL_SPACING;\n'
                       '\ts_main.achievements.generic.type\t\t= MTYPE_PTEXT;\n'
                       '\ts_main.achievements.generic.flags\t\t= QMF_CENTER_JUSTIFY|QMF_PULSEIFFOCUS;\n'
                       '\ts_main.achievements.generic.x\t\t\t= 320;\n'
                       '\ts_main.achievements.generic.y\t\t\t= y;\n'
                       '\ts_main.achievements.generic.id\t\t\t= ID_ACHIEVEMENTS;\n'
                       '\ts_main.achievements.generic.callback\t= Main_MenuEvent;\n'
                       '\ts_main.achievements.string\t\t\t\t= "PERMADEATH";\n'
                       '\ts_main.achievements.color\t\t\t\t= color_red;\n'
                       '\ts_main.achievements.style\t\t\t\t= style;\n'
                       '\n'
                       '\ty += MAIN_MENU_VERTICAL_SPACING;\n'
                       '\ts_main.multiplayer.generic.type'),
                      'ui_menu.c: initialize achievements menu item')

        patch_literal(f,
                      '\tMenu_AddItem( &s_main.menu,\t&s_main.singleplayer );\n'
                      '\tMenu_AddItem( &s_main.menu,\t&s_main.multiplayer );',
                      ('\tMenu_AddItem( &s_main.menu,\t&s_main.singleplayer );\n'
                       '\tMenu_AddItem( &s_main.menu,\t&s_main.achievements );\n'
                       '\tMenu_AddItem( &s_main.menu,\t&s_main.multiplayer );'),
                      'ui_menu.c: add achievements to Menu_AddItem list')

    # -------------------------------------------------------------------------
    # ui_gameinfo.c: unlock the appropriate pd_tier_ach bit the first time a
    # tier video is shown.  UI_ShowTierVideo(tier) is called with tier = won+1,
    # so the completed tier index is tier-1.  Tiers 1-7 map to tier args 2-8.
    # Skill track: g_spSkill 1-3 → track 0 (skill123), 4 → track 1, 5 → track 2.
    # Bit = (tier - 2) * 3 + track.
    # -------------------------------------------------------------------------
    f = p('code/q3_ui/ui_gameinfo.c')
    if already(f, 'pd_tier_ach'):
        print('  [SKIP] ui_gameinfo.c: pd_tier_ach already present')
    else:
        patch_literal(f,
                      '\tInfo_SetValueForKey( videos, key, va( "%i", 1 ) );\n'
                      '\ttrap_Cvar_Set( "g_spVideos", videos );\n'
                      '\n'
                      '\treturn qtrue;\n'
                      '}',
                      ('\tInfo_SetValueForKey( videos, key, va( "%i", 1 ) );\n'
                       '\ttrap_Cvar_Set( "g_spVideos", videos );\n'
                       '\n'
                       '\tif ( tier >= 2 && tier <= 8 ) {\n'
                       '\t\tint pdSkill = (int)trap_Cvar_VariableValue( "g_spSkill" );\n'
                       '\t\tint track   = (pdSkill >= 5) ? 2 : (pdSkill >= 4) ? 1 : 0;\n'
                       '\t\tint bit     = (tier - 2) * 3 + track;\n'
                       '\t\tint cur     = (int)trap_Cvar_VariableValue( "pd_tier_ach" );\n'
                       '\t\ttrap_Cvar_Set( "pd_tier_ach", va( "%i", cur | (1 << bit) ) );\n'
                       '\t}\n'
                       '\n'
                       '\treturn qtrue;\n'
                       '}'),
                      'ui_gameinfo.c: unlock pd_tier_ach bit on tier completion')

    # =========================================================================
    # FIFTH ITERATION: Permadeath statistics
    # =========================================================================

    # -------------------------------------------------------------------------
    # g_local.h: declare PermaDeath_Shutdown and all fifth-iteration functions.
    # -------------------------------------------------------------------------
    f = p('code/game/g_local.h')
    if already(f, 'PermaDeath_Shutdown'):
        print('  [SKIP] g_local.h (PermaDeath_Shutdown) already patched')
    else:
        patch_literal(f,
                      'void PermaDeath_TrackSecret( gentity_t *activator, const char *msg );',
                      ('void PermaDeath_TrackSecret( gentity_t *activator, const char *msg );\n'
                       'void PermaDeath_Shutdown( void );\n'
                       'void PermaDeath_TrackMatchEnd( void );\n'
                       'void PermaDeath_TrackJumppad( gentity_t *ent );\n'
                       'void PermaDeath_TrackPlayerHit( gentity_t *targ );'),
                      'g_local.h: declare fifth-iteration permadeath functions')

    # -------------------------------------------------------------------------
    # g_main.c: call PermaDeath_Shutdown() at the end of G_ShutdownGame so the
    # current level's elapsed time is accumulated into the session counter before
    # the DLL unloads.
    # -------------------------------------------------------------------------
    f = p('code/game/g_main.c')
    if already(f, 'PermaDeath_Shutdown'):
        print('  [SKIP] g_main.c (PermaDeath_Shutdown) already patched')
    else:
        patch_literal(f,
                      '\t\tBotAIShutdown( restart );\n\t}\n}',
                      '\t\tBotAIShutdown( restart );\n\t}\n\tPermaDeath_Shutdown();\n}',
                      'g_main.c: call PermaDeath_Shutdown in G_ShutdownGame')

    # -------------------------------------------------------------------------
    # g_main.c: call PermaDeath_TrackMatchEnd at the end of LogExit.
    # This handles wins, defeats, campaign completion, medals records, and the
    # Imperfect achievement (replacing the old bot-rank detection loop).
    # -------------------------------------------------------------------------
    if already(f, 'PermaDeath_TrackMatchEnd'):
        print('  [SKIP] g_main.c (PermaDeath_TrackMatchEnd) already patched')
    else:
        patch_literal(f,
                      '\t\ttrap_SendConsoleCommand( EXEC_APPEND, (won) ? "spWin\\n" : "spLose\\n" );\n'
                      '\t}\n'
                      '#endif\n'
                      '\n'
                      '\n'
                      '}',
                      ('\t\ttrap_SendConsoleCommand( EXEC_APPEND, (won) ? "spWin\\n" : "spLose\\n" );\n'
                       '\t}\n'
                       '#endif\n'
                       '\n'
                       '\tPermaDeath_TrackMatchEnd();\n'
                       '\n'
                       '}'),
                      'g_main.c: call PermaDeath_TrackMatchEnd at end of LogExit')

    # -------------------------------------------------------------------------
    # basegame.cmake: add ui_pd_stats.c to the UI source list.
    # -------------------------------------------------------------------------
    f = p('cmake/basegame.cmake')
    if already(f, 'ui_pd_stats.c'):
        print('  [SKIP] basegame.cmake: ui_pd_stats.c already present')
    else:
        patch_literal(f,
                      '${SOURCE_DIR}/q3_ui/ui_achievements.c',
                      ('${SOURCE_DIR}/q3_ui/ui_achievements.c\n'
                       '    ${SOURCE_DIR}/q3_ui/ui_pd_stats.c'),
                      'basegame.cmake: add ui_pd_stats.c')

    # -------------------------------------------------------------------------
    # ui_local.h: declare UI_PdStatsMenu().
    # -------------------------------------------------------------------------
    f = p('code/q3_ui/ui_local.h')
    if already(f, 'UI_PdStatsMenu'):
        print('  [SKIP] ui_local.h (UI_PdStatsMenu) already patched')
    else:
        patch_re(f,
                 r'(extern void\s+UI_AchievementsMenu\s*\(\s*void\s*\)\s*;)',
                 '\\1\nextern void\t\t\tUI_PdStatsMenu( void );',
                 'ui_local.h: declare UI_PdStatsMenu')

    # -------------------------------------------------------------------------
    # ui_atoms.c: print version string at the end of UI_Init, after the main
    # menu has loaded completely.
    # -------------------------------------------------------------------------
    f = p('code/q3_ui/ui_atoms.c')
    if already(f, '^1Permadeath'):
        print('  [SKIP] ui_atoms.c (version print) already patched')
    else:
        patch_literal(f,
                      '\tuis.activemenu = NULL;\n'
                      '\tuis.menusp     = 0;\n'
                      '}',
                      ('\tuis.activemenu = NULL;\n'
                       '\tuis.menusp     = 0;\n'
                       '\ttrap_Print( "^1Permadeath v' + PD_VERSION + '^7\\n" );\n'
                       '}'),
                      'ui_atoms.c: print version on startup')

    # -------------------------------------------------------------------------
    # ui_pd_stats.c: stamp PD_VERSION with the build version (always applied;
    # the source file uses "dev" as a placeholder).
    # -------------------------------------------------------------------------
    f = p('code/q3_ui/ui_pd_stats.c')
    patch_re(f,
             r'#define\s+PD_VERSION\s+"[^"]*"',
             '#define PD_VERSION "' + PD_VERSION + '"',
             'ui_pd_stats.c: set PD_VERSION to ' + PD_VERSION)

    # =========================================================================
    # SIXTH ITERATION: Haste medal in SP intermission
    # =========================================================================

    # -------------------------------------------------------------------------
    # ui_local.h: add AWARD_HASTE to the awardType_t enum (value 6).
    # -------------------------------------------------------------------------
    f = p('code/q3_ui/ui_local.h')
    if already(f, 'AWARD_HASTE'):
        print('  [SKIP] ui_local.h (AWARD_HASTE) already patched')
    else:
        patch_re(f,
                 r'(\tAWARD_PERFECT)\s*\n(\s*\}\s*awardType_t\s*;)',
                 '\\1,\n\tAWARD_HASTE\n\\2',
                 'ui_local.h: add AWARD_HASTE to awardType_t')

    # -------------------------------------------------------------------------
    # ui_sppostgame.c: expand medal infrastructure from 6 to 7 slots and add
    # Haste as the 7th medal checked via the pd_haste cvar set by the game DLL.
    # -------------------------------------------------------------------------
    f = p('code/q3_ui/ui_sppostgame.c')
    if already(f, 'pd_haste'):
        print('  [SKIP] ui_sppostgame.c (Haste medal) already patched')
    else:
        # Expand postgameMenuInfo_t award arrays from [6] to [7]
        patch_re(f,
                 r'(int\s+awardsEarned)\[6\](;\s+int\s+awardsLevels)\[6\](;\s+qboolean\s+playedSound)\[6\]',
                 '\\1[7]\\2[7]\\3[7]',
                 'ui_sppostgame.c: expand award arrays [6] -> [7]')

        # Expand medalLocations array and add 7th position (x=296, screen center)
        patch_literal(f,
                      'static int medalLocations[6] = {144, 448, 88, 504, 32, 560};',
                      'static int medalLocations[7] = {144, 448, 88, 504, 32, 560, 296};',
                      'ui_sppostgame.c: expand medalLocations to [7]')

        # Add "Haste" to medal name array
        patch_literal(f,
                      '"Accuracy", "Impressive", "Excellent", "Gauntlet", "Frags", "Perfect"};',
                      '"Accuracy", "Impressive", "Excellent", "Gauntlet", "Frags", "Perfect", "Haste"};',
                      'ui_sppostgame.c: add Haste to ui_medalNames')

        # Add haste powerup icon to pic names array
        patch_literal(f,
                      '\t"menu/medals/medal_victory"\n};',
                      '\t"menu/medals/medal_victory",\n\t"icons/haste"\n};',
                      'ui_sppostgame.c: add icons/haste to ui_medalPicNames')

        # Add haste sound to sounds array
        patch_literal(f,
                      '\t"sound/feedback/perfect.wav"\n};',
                      '\t"sound/feedback/perfect.wav",\n\t"sound/items/haste.wav"\n};',
                      'ui_sppostgame.c: add haste.wav to ui_medalSounds')

        # Expand local awardValues array from [6] to [7]
        patch_re(f,
                 r'(int\s+awardValues)\[6\];',
                 '\\1[7];',
                 'ui_sppostgame.c: expand awardValues [6] -> [7]')

        # Append Haste check after Perfect block
        patch_literal(f,
                      '\tif( awardValues[AWARD_PERFECT] ) {\n'
                      '\t\tUI_LogAwardData( AWARD_PERFECT, 1 );\n'
                      '\t\tpostgameMenuInfo.awardsEarned[postgameMenuInfo.numAwards] = AWARD_PERFECT;\n'
                      '\t\tpostgameMenuInfo.awardsLevels[postgameMenuInfo.numAwards] = 1;\n'
                      '\t\tpostgameMenuInfo.numAwards++;\n'
                      '\t}',
                      ('\tif( awardValues[AWARD_PERFECT] ) {\n'
                       '\t\tUI_LogAwardData( AWARD_PERFECT, 1 );\n'
                       '\t\tpostgameMenuInfo.awardsEarned[postgameMenuInfo.numAwards] = AWARD_PERFECT;\n'
                       '\t\tpostgameMenuInfo.awardsLevels[postgameMenuInfo.numAwards] = 1;\n'
                       '\t\tpostgameMenuInfo.numAwards++;\n'
                       '\t}\n'
                       '\tif ( trap_Cvar_VariableValue( "pd_haste" ) ) { /* pd_haste */\n'
                       '\t\tpostgameMenuInfo.awardsEarned[postgameMenuInfo.numAwards] = AWARD_HASTE;\n'
                       '\t\tpostgameMenuInfo.awardsLevels[postgameMenuInfo.numAwards] = 1;\n'
                       '\t\tpostgameMenuInfo.numAwards++;\n'
                       '\t}'),
                      'ui_sppostgame.c: add Haste medal check after Perfect')

    # -------------------------------------------------------------------------
    # ui_main.c: register pd_haste_earned as CVAR_ARCHIVE so the count of times
    # the Haste medal was earned persists across sessions.
    # -------------------------------------------------------------------------
    f = p('code/q3_ui/ui_main.c')
    if already(f, 'pd_haste_earned'):
        print('  [SKIP] ui_main.c (pd_haste_earned) already patched')
    else:
        patch_literal(f,
                      '\t{ &ui_pdTierAch, "pd_tier_ach", "0", CVAR_ARCHIVE },',
                      ('\t{ &ui_pdTierAch, "pd_tier_ach", "0", CVAR_ARCHIVE },\n'
                       '\t{ NULL, "pd_haste_earned", "0", CVAR_ARCHIVE },'),
                      'ui_main.c: register pd_haste_earned as CVAR_ARCHIVE')

    # -------------------------------------------------------------------------
    # ui_splevel.c: expand medal arrays from [6] to [7] and add Haste medal
    # display and click-to-play-sound support.
    # -------------------------------------------------------------------------
    f = p('code/q3_ui/ui_splevel.c')
    if already(f, 'pd_haste_gsp'):
        print('  [SKIP] ui_splevel.c (Haste medal) already patched')
    else:
        # Expand struct arrays
        patch_re(f,
                 r'(menubitmap_s\s+item_awards)\[6\]',
                 '\\1[7]',
                 'ui_splevel.c: expand item_awards [6] -> [7]')
        patch_re(f,
                 r'(int\s+awardLevels)\[6\]',
                 '\\1[7]',
                 'ui_splevel.c: expand awardLevels [6] -> [7]')
        patch_re(f,
                 r'(sfxHandle_t\s+awardSounds)\[6\]',
                 '\\1[7]',
                 'ui_splevel.c: expand awardSounds [6] -> [7]')

        # Registration loop: n < 6 -> n < 7
        patch_literal(f,
                      '\tfor( n = 0; n < 6; n++ ) {\n'
                      '\t\ttrap_R_RegisterShaderNoMip( ui_medalPicNames[n] );\n'
                      '\t\tlevelMenuInfo.awardSounds[n] = trap_S_RegisterSound( ui_medalSounds[n], qfalse );\n'
                      '\t}',
                      ('\tfor( n = 0; n < 7; n++ ) {\n'
                       '\t\ttrap_R_RegisterShaderNoMip( ui_medalPicNames[n] );\n'
                       '\t\tlevelMenuInfo.awardSounds[n] = trap_S_RegisterSound( ui_medalSounds[n], qfalse );\n'
                       '\t}'),
                      'ui_splevel.c: registration loop n < 6 -> n < 7')

        # Award-level init: keep loop at 6 (UI_GetAwardLevel only knows 0-5),
        # then set slot 6 from the persistent pd_haste_earned cvar.
        patch_literal(f,
                      '\tfor( n = 0; n < 6; n++ ) {\n'
                      '\t\tlevelMenuInfo.awardLevels[n] = UI_GetAwardLevel( n );\n'
                      '\t}\n'
                      '\tlevelMenuInfo.awardLevels[AWARD_FRAGS] = 100 * (levelMenuInfo.awardLevels[AWARD_FRAGS] / 100);',
                      ('\tfor( n = 0; n < 6; n++ ) {\n'
                       '\t\tlevelMenuInfo.awardLevels[n] = UI_GetAwardLevel( n );\n'
                       '\t}\n'
                       '\tlevelMenuInfo.awardLevels[AWARD_FRAGS] = 100 * (levelMenuInfo.awardLevels[AWARD_FRAGS] / 100);\n'
                       '\tlevelMenuInfo.awardLevels[AWARD_HASTE] = (int)trap_Cvar_VariableValue( "pd_haste_earned" ); /* pd_haste_earned */'),
                      'ui_splevel.c: load AWARD_HASTE level from pd_haste_earned')

        # Build/display loop: n < 6 -> n < 7
        patch_literal(f,
                      '\tfor( n = 0; n < 6; n++ ) {\n'
                      '\t\tif( levelMenuInfo.awardLevels[n] ) {',
                      ('\tfor( n = 0; n < 7; n++ ) {\n'
                       '\t\tif( levelMenuInfo.awardLevels[n] ) {'),
                      'ui_splevel.c: display loop n < 6 -> n < 7')


    # -------------------------------------------------------------------------
    # ui_splevel.c: the draw-counts loop (renders the number under each icon)
    # also hard-codes n < 6, extend to n < 7 for AWARD_HASTE.
    # Separate sentinel so it can be applied independently.
    # -------------------------------------------------------------------------
    f = p('code/q3_ui/ui_splevel.c')
    if already(f, 'pd_haste_draw'):
        print('  [SKIP] ui_splevel.c (draw-counts loop) already patched')
    else:
        patch_literal(f,
                      '\t// draw player award levels\n'
                      '\ty = AWARDS_Y;\n'
                      '\ti = 0;\n'
                      '\tfor( n = 0; n < 6; n++ ) {',
                      ('\t// draw player award levels\n'
                       '\ty = AWARDS_Y;\n'
                       '\ti = 0;\n'
                       '\tfor( n = 0; n < 7; n++ ) { /* pd_haste_draw */'),
                      'ui_splevel.c: draw-counts loop n < 6 -> n < 7')

    # -------------------------------------------------------------------------
    # ui_gameinfo.c: extend UI_LogAwardData guard to allow AWARD_HASTE (index 6)
    # so Haste can be recorded in g_spAwards like every other medal.
    # -------------------------------------------------------------------------
    f = p('code/q3_ui/ui_gameinfo.c')
    if already(f, 'award > AWARD_HASTE'):
        print('  [SKIP] ui_gameinfo.c (AWARD_HASTE guard) already patched')
    else:
        patch_literal(f,
                      '\tif( award > AWARD_PERFECT ) {',
                      '\tif( award > AWARD_HASTE ) {',
                      'ui_gameinfo.c: extend LogAwardData guard to AWARD_HASTE')

    # -------------------------------------------------------------------------
    # ui_sppostgame.c: record the Haste medal in g_spAwards when it is earned
    # so the Choose Level screen can display it (and it clears on death).
    # -------------------------------------------------------------------------
    f = p('code/q3_ui/ui_sppostgame.c')
    if already(f, 'UI_LogAwardData( AWARD_HASTE'):
        print('  [SKIP] ui_sppostgame.c (LogAwardData Haste) already patched')
    else:
        patch_literal(f,
                      '\tif ( trap_Cvar_VariableValue( "pd_haste" ) ) { /* pd_haste */\n'
                      '\t\tpostgameMenuInfo.awardsEarned[postgameMenuInfo.numAwards] = AWARD_HASTE;\n'
                      '\t\tpostgameMenuInfo.awardsLevels[postgameMenuInfo.numAwards] = 1;\n'
                      '\t\tpostgameMenuInfo.numAwards++;\n'
                      '\t}',
                      ('\tif ( trap_Cvar_VariableValue( "pd_haste" ) ) { /* pd_haste */\n'
                       '\t\tUI_LogAwardData( AWARD_HASTE, 1 );\n'
                       '\t\tpostgameMenuInfo.awardsEarned[postgameMenuInfo.numAwards] = AWARD_HASTE;\n'
                       '\t\tpostgameMenuInfo.awardsLevels[postgameMenuInfo.numAwards] = 1;\n'
                       '\t\tpostgameMenuInfo.numAwards++;\n'
                       '\t}'),
                      'ui_sppostgame.c: record AWARD_HASTE in g_spAwards')

    # -------------------------------------------------------------------------
    # ui_splevel.c: use UI_GetAwardLevel(AWARD_HASTE) (reads g_spAwards key a6,
    # cleared on death) instead of the permanent pd_haste_earned counter.
    # -------------------------------------------------------------------------
    f = p('code/q3_ui/ui_splevel.c')
    if already(f, 'pd_haste_gsp'):
        print('  [SKIP] ui_splevel.c (AWARD_HASTE via g_spAwards) already patched')
    else:
        patch_literal(f,
                      'levelMenuInfo.awardLevels[AWARD_HASTE] = (int)trap_Cvar_VariableValue( "pd_haste_earned" ); /* pd_haste_earned */',
                      'levelMenuInfo.awardLevels[AWARD_HASTE] = UI_GetAwardLevel( AWARD_HASTE ); /* pd_haste_gsp */',
                      'ui_splevel.c: AWARD_HASTE reads g_spAwards (clears on death)')

    # =========================================================================
    # SIXTH ITERATION: Auto-record demo toggle in Game Options
    # =========================================================================

    # -------------------------------------------------------------------------
    # ui_main.c: register pd_autorecord as CVAR_ARCHIVE, default off.
    # -------------------------------------------------------------------------
    f = p('code/q3_ui/ui_main.c')
    if already(f, 'pd_autorecord'):
        print('  [SKIP] ui_main.c (pd_autorecord) already patched')
    else:
        patch_literal(f,
                      '\t{ NULL, "pd_haste_earned", "0", CVAR_ARCHIVE },',
                      ('\t{ NULL, "pd_haste_earned", "0", CVAR_ARCHIVE },\n'
                       '\t{ NULL, "pd_autorecord", "0", CVAR_ARCHIVE },'),
                      'ui_main.c: register pd_autorecord as CVAR_ARCHIVE (default off)')

    # -------------------------------------------------------------------------
    # ui_preferences.c: add Auto Record toggle to Game Options.
    # A double-height y gap acts as the visual separator before the new item.
    # -------------------------------------------------------------------------
    f = p('code/q3_ui/ui_preferences.c')
    if already(f, 'pd_autorecord'):
        print('  [SKIP] ui_preferences.c (auto-record) already patched')
    else:
        # New ID constant
        patch_re(f,
                 r'(#define\s+ID_BACK\s+138)',
                 '\\1\n#define ID_AUTORECORD\t\t\t139',
                 'ui_preferences.c: add ID_AUTORECORD')

        # Struct field
        patch_literal(f,
                      '\tmenuradiobutton_s\tallowdownload;\n'
                      '\tmenubitmap_s\t\tback;',
                      '\tmenuradiobutton_s\tallowdownload;\n'
                      '\tmenuradiobutton_s\tautorecord;\n'
                      '\tmenubitmap_s\t\tback;',
                      'ui_preferences.c: add autorecord struct field')

        # SetMenuItems
        patch_literal(f,
                      's_preferences.allowdownload.curvalue\t= trap_Cvar_VariableValue( "cl_allowDownload" ) != 0;',
                      ('s_preferences.allowdownload.curvalue\t= trap_Cvar_VariableValue( "cl_allowDownload" ) != 0;\n'
                       '\ts_preferences.autorecord.curvalue\t\t= trap_Cvar_VariableValue( "pd_autorecord" ) != 0;'),
                      'ui_preferences.c: read pd_autorecord in SetMenuItems')

        # Event handler
        patch_literal(f,
                      '\tcase ID_ALLOWDOWNLOAD:\n'
                      '\t\ttrap_Cvar_SetValue( "cl_allowDownload", s_preferences.allowdownload.curvalue );\n'
                      '\t\ttrap_Cvar_SetValue( "sv_allowDownload", s_preferences.allowdownload.curvalue );\n'
                      '\t\tbreak;\n'
                      '\n'
                      '\tcase ID_BACK:',
                      ('\tcase ID_ALLOWDOWNLOAD:\n'
                       '\t\ttrap_Cvar_SetValue( "cl_allowDownload", s_preferences.allowdownload.curvalue );\n'
                       '\t\ttrap_Cvar_SetValue( "sv_allowDownload", s_preferences.allowdownload.curvalue );\n'
                       '\t\tbreak;\n'
                       '\n'
                       '\tcase ID_AUTORECORD:\n'
                       '\t\ttrap_Cvar_SetValue( "pd_autorecord", s_preferences.autorecord.curvalue );\n'
                       '\t\tbreak;\n'
                       '\n'
                       '\tcase ID_BACK:'),
                      'ui_preferences.c: handle ID_AUTORECORD in event switch')

        # MenuInit: init item (double y gap as separator before the new item)
        patch_re(f,
                 r'(s_preferences\.allowdownload\.generic\.y\s*=\s*y\s*;)'
                 r'(\s*\n\s*s_preferences\.back\.generic\.type)',
                 ('\\1\n'
                  '\n'
                  '\ty += BIGCHAR_HEIGHT * 2;\n'
                  '\ts_preferences.autorecord.generic.type     = MTYPE_RADIOBUTTON;\n'
                  '\ts_preferences.autorecord.generic.name     = "Auto Record:";\n'
                  '\ts_preferences.autorecord.generic.flags    = QMF_PULSEIFFOCUS|QMF_SMALLFONT;\n'
                  '\ts_preferences.autorecord.generic.callback = Preferences_Event;\n'
                  '\ts_preferences.autorecord.generic.id       = ID_AUTORECORD;\n'
                  '\ts_preferences.autorecord.generic.x        = PREFERENCES_X_POS;\n'
                  '\ts_preferences.autorecord.generic.y        = y;'
                  '\\2'),
                 'ui_preferences.c: init autorecord item in MenuInit')

        # Menu_AddItem
        patch_literal(f,
                      '\tMenu_AddItem( &s_preferences.menu, &s_preferences.allowdownload );\n'
                      '\n'
                      '\tMenu_AddItem( &s_preferences.menu, &s_preferences.back );',
                      ('\tMenu_AddItem( &s_preferences.menu, &s_preferences.allowdownload );\n'
                       '\tMenu_AddItem( &s_preferences.menu, &s_preferences.autorecord );\n'
                       '\n'
                       '\tMenu_AddItem( &s_preferences.menu, &s_preferences.back );'),
                      'ui_preferences.c: register autorecord with Menu_AddItem')

    # -------------------------------------------------------------------------
    # cg_main.c: at the end of CG_Init, if pd_autorecord is set and we are in
    # SP mode, stop any existing recording and start a new demo named:
    #   run{DDDD}-skill{N}-{mapname}
    # where DDDD = total deaths + 1 (this run's number), N = g_spSkill.
    # -------------------------------------------------------------------------
    f = p('code/cgame/cg_main.c')
    if already(f, 'pd_autorecord'):
        print('  [SKIP] cg_main.c (auto-record) already patched')
    else:
        patch_literal(f,
                      '\ttrap_S_ClearLoopingSounds( qtrue );\n'
                      '}',
                      ('\ttrap_S_ClearLoopingSounds( qtrue );\n'
                       '\n'
                       '\t/* pd_autorecord: start per-map demo if toggle is on */\n'
                       '\t{\n'
                       '\t\tchar _arec[4];\n'
                       '\t\ttrap_Cvar_VariableStringBuffer( "pd_autorecord", _arec, sizeof(_arec) );\n'
                       '\t\tif ( cgs.gametype == GT_SINGLE_PLAYER && atoi( _arec ) ) {\n'
                       '\t\t\tint  deaths, run, skill;\n'
                       '\t\t\tchar _d1[8], _d2[8], _d3[8], _sk[4];\n'
                       '\t\t\tchar mapname[64], demoname[128];\n'
                       '\t\t\ttrap_Cvar_VariableStringBuffer( "pd_s123_deaths", _d1, sizeof(_d1) );\n'
                       '\t\t\ttrap_Cvar_VariableStringBuffer( "pd_s4_deaths",   _d2, sizeof(_d2) );\n'
                       '\t\t\ttrap_Cvar_VariableStringBuffer( "pd_s5_deaths",   _d3, sizeof(_d3) );\n'
                       '\t\t\ttrap_Cvar_VariableStringBuffer( "g_spSkill",      _sk, sizeof(_sk) );\n'
                       '\t\t\tdeaths = atoi( _d1 ) + atoi( _d2 ) + atoi( _d3 );\n'
                       '\t\t\trun    = deaths + 1;\n'
                       '\t\t\tskill  = atoi( _sk );\n'
                       '\t\t\ttrap_Cvar_VariableStringBuffer( "mapname", mapname, sizeof(mapname) );\n'
                       '\t\t\tCom_sprintf( demoname, sizeof(demoname),\n'
                       '\t\t\t             "run%04i-skill%i-%s", run, skill, mapname );\n'
                       '\t\t\ttrap_Cvar_Set( "pd_demoname_pending", demoname );\n'
                       '\t\t} else {\n'
                       '\t\t\ttrap_Cvar_Set( "pd_demoname_pending", "" );\n'
                       '\t\t}\n'
                       '\t}\n'
                       '}'),
                      'cg_main.c: auto-record demo on map load in CG_Init')

    # -------------------------------------------------------------------------
    # cg_view.c: fire the deferred record on the first frame.
    # demoPlayback is passed directly here, so we can skip recording during
    # demo playback without any engine-side cvar needed.
    # -------------------------------------------------------------------------
    f = p('code/cgame/cg_view.c')
    if already(f, 'pd_demoname_pending'):
        print('  [SKIP] cg_view.c (auto-record first-frame) already patched')
    else:
        patch_literal(f,
                      '\tcg.demoPlayback = demoPlayback;\n',
                      ('\tcg.demoPlayback = demoPlayback;\n'
                       '\n'
                       '\t/* pd_autorecord: fire deferred record on first frame, skip during demo playback */\n'
                       '\t{\n'
                       '\t\tchar _pending[128];\n'
                       '\t\ttrap_Cvar_VariableStringBuffer( "pd_demoname_pending", _pending, sizeof(_pending) );\n'
                       '\t\tif ( _pending[0] ) {\n'
                       '\t\t\ttrap_Cvar_Set( "pd_demoname_pending", "" );\n'
                       '\t\t\tif ( !demoPlayback ) {\n'
                       '\t\t\t\ttrap_SendConsoleCommand( "stoprecord\\n" );\n'
                       '\t\t\t\ttrap_SendConsoleCommand( va( "wait 2; record %s\\n", _pending ) );\n'
                       '\t\t\t}\n'
                       '\t\t}\n'
                       '\t}\n'
                       '\n'
                       '\t/* pd_extraloss: play gong sound when an extra life is consumed */\n'
                       '\t{\n'
                       '\t\tchar _el[4];\n'
                       '\t\tstatic sfxHandle_t pd_gongSfx = 0;\n'
                       '\t\ttrap_Cvar_VariableStringBuffer( "pd_extraloss", _el, sizeof(_el) );\n'
                       '\t\tif ( atoi( _el ) ) {\n'
                       '\t\t\tif ( !pd_gongSfx )\n'
                       '\t\t\t\tpd_gongSfx = trap_S_RegisterSound( "sound/world/1shot_gong.wav", qfalse );\n'
                       '\t\t\ttrap_S_StartLocalSound( pd_gongSfx, CHAN_ANNOUNCER );\n'
                       '\t\t\ttrap_Cvar_Set( "pd_extraloss", "0" );\n'
                       '\t\t}\n'
                       '\t}\n'),
                      'cg_view.c: start auto-record on first frame, skip if demo playback')

    # =========================================================================
    # SIXTH ITERATION: Extra Lives system
    # =========================================================================

    # -------------------------------------------------------------------------
    # ui_main.c: register pd_extra_lives and pd_true_permadeath.
    # -------------------------------------------------------------------------
    f = p('code/q3_ui/ui_main.c')
    if already(f, 'pd_extra_lives'):
        print('  [SKIP] ui_main.c (pd_extra_lives) already patched')
    else:
        patch_literal(f,
                      '\t{ NULL, "pd_autorecord", "0", CVAR_ARCHIVE },',
                      ('\t{ NULL, "pd_autorecord", "0", CVAR_ARCHIVE },\n'
                       '\t{ NULL, "pd_extra_lives", "0", CVAR_ARCHIVE },\n'
                       '\t{ NULL, "pd_true_permadeath", "0", CVAR_ARCHIVE },\n'
                       '\t{ NULL, "pd_completed_maps", "0", CVAR_ARCHIVE },'),
                      'ui_main.c: register pd_extra_lives and pd_true_permadeath')

    # -------------------------------------------------------------------------
    # ui_preferences.c: add True Permadeath toggle below Auto Record.
    # -------------------------------------------------------------------------
    f = p('code/q3_ui/ui_preferences.c')
    if already(f, 'pd_true_permadeath'):
        print('  [SKIP] ui_preferences.c (true-permadeath) already patched')
    else:
        # ID constant
        patch_literal(f,
                      '#define ID_AUTORECORD\t\t\t139',
                      '#define ID_AUTORECORD\t\t\t139\n'
                      '#define ID_TRUEPERMADEATH\t\t140',
                      'ui_preferences.c: add ID_TRUEPERMADEATH constant')

        # Struct field
        patch_literal(f,
                      '\tmenuradiobutton_s\tautorecord;\n'
                      '\tmenubitmap_s\t\tback;',
                      '\tmenuradiobutton_s\tautorecord;\n'
                      '\tmenuradiobutton_s\ttruepermadeath;\n'
                      '\tmenubitmap_s\t\tback;',
                      'ui_preferences.c: add truepermadeath struct field')

        # SetMenuItems
        patch_literal(f,
                      '\ts_preferences.autorecord.curvalue\t\t= trap_Cvar_VariableValue( "pd_autorecord" ) != 0;',
                      ('\ts_preferences.autorecord.curvalue\t\t= trap_Cvar_VariableValue( "pd_autorecord" ) != 0;\n'
                       '\ts_preferences.truepermadeath.curvalue\t= trap_Cvar_VariableValue( "pd_true_permadeath" ) != 0;'),
                      'ui_preferences.c: read pd_true_permadeath in SetMenuItems')

        # Event handler
        patch_literal(f,
                      '\tcase ID_AUTORECORD:\n'
                      '\t\ttrap_Cvar_SetValue( "pd_autorecord", s_preferences.autorecord.curvalue );\n'
                      '\t\tbreak;\n'
                      '\n'
                      '\tcase ID_BACK:',
                      ('\tcase ID_AUTORECORD:\n'
                       '\t\ttrap_Cvar_SetValue( "pd_autorecord", s_preferences.autorecord.curvalue );\n'
                       '\t\tbreak;\n'
                       '\n'
                       '\tcase ID_TRUEPERMADEATH:\n'
                       '\t\ttrap_Cvar_SetValue( "pd_true_permadeath", s_preferences.truepermadeath.curvalue );\n'
                       '\t\tbreak;\n'
                       '\n'
                       '\tcase ID_BACK:'),
                      'ui_preferences.c: handle ID_TRUEPERMADEATH in event switch')

        # MenuInit: item init (immediately below autorecord, no extra separator gap)
        patch_literal(f,
                      '\ts_preferences.autorecord.generic.y        = y;',
                      ('\ts_preferences.autorecord.generic.y        = y;\n'
                       '\n'
                       '\ty += BIGCHAR_HEIGHT;\n'
                       '\ts_preferences.truepermadeath.generic.type     = MTYPE_RADIOBUTTON;\n'
                       '\ts_preferences.truepermadeath.generic.name     = "True Permadeath:";\n'
                       '\ts_preferences.truepermadeath.generic.flags    = QMF_PULSEIFFOCUS|QMF_SMALLFONT;\n'
                       '\ts_preferences.truepermadeath.generic.callback = Preferences_Event;\n'
                       '\ts_preferences.truepermadeath.generic.id       = ID_TRUEPERMADEATH;\n'
                       '\ts_preferences.truepermadeath.generic.x        = PREFERENCES_X_POS;\n'
                       '\ts_preferences.truepermadeath.generic.y        = y;'),
                      'ui_preferences.c: init truepermadeath item in MenuInit')

        # Menu_AddItem
        patch_literal(f,
                      '\tMenu_AddItem( &s_preferences.menu, &s_preferences.autorecord );\n'
                      '\n'
                      '\tMenu_AddItem( &s_preferences.menu, &s_preferences.back );',
                      ('\tMenu_AddItem( &s_preferences.menu, &s_preferences.autorecord );\n'
                       '\tMenu_AddItem( &s_preferences.menu, &s_preferences.truepermadeath );\n'
                       '\n'
                       '\tMenu_AddItem( &s_preferences.menu, &s_preferences.back );'),
                      'ui_preferences.c: register truepermadeath with Menu_AddItem')

    # -------------------------------------------------------------------------
    # cg_draw.c: draw extra lives counter (white) to the right of health,
    # in the gap between the player head icon and the armor field.
    # Layout (virtual 640x480): ammo@0, health@185, head@285, armor@370.
    # Drawn at half digit size (CHAR_WIDTH/2 x CHAR_HEIGHT/2), right-anchored
    # so the right edge aligns with where a full-size digit would end.
    # -------------------------------------------------------------------------
    f = p('code/cgame/cg_draw.c')
    if already(f, 'pd_extra_lives'):
        print('  [SKIP] cg_draw.c (extra-lives HUD) already patched')
    else:
        patch_literal(f,
                      '\tCG_ColorForHealth( hcolor );\n'
                      '\ttrap_R_SetColor( hcolor );\n'
                      '\n'
                      '\n'
                      '\t//\n'
                      '\t// armor\n'
                      '\t//',
                      ('\tCG_ColorForHealth( hcolor );\n'
                       '\ttrap_R_SetColor( hcolor );\n'
                       '\n'
                       '\t/* pd_extra_lives: half-size white counter, right-anchored between head and armor */\n'
                       '\t{\n'
                       '\t\tchar _lv[4];\n'
                       '\t\tint  _lives;\n'
                       '\t\ttrap_Cvar_VariableStringBuffer( "pd_extra_lives", _lv, sizeof(_lv) );\n'
                       '\t\t_lives = atoi( _lv );\n'
                       '\t\tif ( _lives > 0 ) {\n'
                       '\t\t\tint _w = CHAR_WIDTH / 2;\n'
                       '\t\t\tint _h = CHAR_HEIGHT / 2;\n'
                       '\t\t\tint _x = 285 + ICON_SIZE + TEXT_ICON_SPACE + CHAR_WIDTH / 2;\n'
                       '\t\t\tint _y = 432 + CHAR_HEIGHT / 4;\n'
                       '\t\t\tvec4_t _white = { 1.0f, 1.0f, 1.0f, 1.0f };\n'
                       '\t\t\tif ( _lives > 9 ) _lives = 9;\n'
                       '\t\t\ttrap_R_SetColor( _white );\n'
                       '\t\t\tCG_DrawPic( _x, _y, _w, _h, cgs.media.numberShaders[_lives] );\n'
                       '\t\t\ttrap_R_SetColor( NULL );\n'
                       '\t\t}\n'
                       '\t}\n'
                       '\n'
                       '\n'
                       '\t//\n'
                       '\t// armor\n'
                       '\t//'),
                      'cg_draw.c: extra lives counter in CG_DrawStatusBar')

    print('\nAll patches applied successfully.')


if __name__ == '__main__':
    if len(sys.argv) < 2:
        print('Usage: python ' + sys.argv[0] + ' <path_to_ioq3_source_root> [version]')
        sys.exit(1)
    root = sys.argv[1]
    version = sys.argv[2] if len(sys.argv) >= 3 else 'dev'
    if not os.path.isdir(root):
        print('ERROR: Directory not found: ' + root)
        sys.exit(1)
    run(root, version)
