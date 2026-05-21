#include "ui_local.h"

#define PD_SOUND        "sound/misc/nightmare.wav"
#define PD_IMAGE        "menu/art/level_complete5"
#define PD_IMAGE_FISH   "textures/sfx/fishy"
#define PD_FONT_WHITE   "menu/art/font3_prop"   /* white version of font2_prop for color tinting */

#define PD_IMG_X    200
#define PD_IMG_Y    70
#define PD_IMG_W    240
#define PD_IMG_H    240
#define PD_TEXT_Y   340
#define PD_TEXT_Y2  (PD_TEXT_Y + 48)  /* second line, one banner-height below */

/* Character widths for the big banner font (propMapB[i][2] from ui_atoms.c).
   A=0 through Z=25. */
static const int pd_propW[26] = {
    33, 31, 31, 30, 21, 21, 32, 30, 13, 29, 31, 21, 40,
    32, 31, 31, 30, 30, 30, 25, 30, 32, 51, 32, 31, 25
};
#define PD_PROPB_GAP_WIDTH   4
#define PD_PROPB_SPACE_WIDTH 12

/* Returns the pixel advance of drawing str (matches the draw loop's advance logic). */
static int PD_BannerAdv( const char *s ) {
    int w = 0;
    for ( ; *s; s++ ) {
        int ch = *s;
        if ( ch == ' ' )
            w += PD_PROPB_SPACE_WIDTH + PD_PROPB_GAP_WIDTH;
        else if ( ch >= 'A' && ch <= 'Z' )
            w += pd_propW[ch - 'A'] + PD_PROPB_GAP_WIDTH;
    }
    return w;
}

typedef struct {
    menuframework_s menu;
    qhandle_t       image;
    qhandle_t       fishImage;
    qhandle_t       whiteFontB;  /* font3_prop: white, so color tints render correctly */
    sfxHandle_t     sound;
    int             type;
    int             openTime;    /* uis.realtime of first draw call; -1 until then */
} pdmenu_t;

static pdmenu_t s_pd;

/* Draw a banner string using the white font so any color filter is reproduced faithfully. */
static void PD_DrawBanner( int x, int y, const char *str, int style, vec4_t color ) {
    qhandle_t saved     = uis.charsetPropB;
    uis.charsetPropB    = s_pd.whiteFontB;
    UI_DrawBannerString( x, y, str, style, color );
    uis.charsetPropB    = saved;
}

/* Draw two banner strings side by side, centered together at (cx, y). */
static void PD_DrawBannerSplit( int cx, int y,
                                const char *p1, vec4_t c1,
                                const char *p2, vec4_t c2 ) {
    int adv1   = PD_BannerAdv( p1 );
    int adv2   = PD_BannerAdv( p2 );
    int dispW  = adv1 + adv2 - PD_PROPB_GAP_WIDTH;
    int startX = cx - dispW / 2;
    PD_DrawBanner( startX,        y, p1, UI_LEFT | UI_DROPSHADOW, c1 );
    PD_DrawBanner( startX + adv1, y, p2, UI_LEFT | UI_DROPSHADOW, c2 );
}

static sfxHandle_t UI_PermadeathMenu_Key( int key ) {
    if ( key & K_CHAR_FLAG ) return 0;
    if ( s_pd.openTime < 0 || uis.realtime - s_pd.openTime < 1000 ) return 1;
    uis.cursorx = SCREEN_WIDTH / 2;
    uis.cursory = SCREEN_HEIGHT / 2;
    UI_PopMenu();
    UI_SetActiveMenu( UIMENU_MAIN );
    return 0;
}

static void PD_GetLines( int type, const char **l1, const char **l2 ) {
    *l2 = NULL;
    switch ( type ) {
        case 19: *l1 = "GAME RUINED";                                     break;
        case 18: *l1 = "GAME DENIED";                                     break;
        case 17: *l1 = "GAME REDIRECTED";                                 break;
        case 16: *l1 = "OMAE WA MOU SHNDEIRU";                           break;
        case 15: *l1 = "YOU GOT A DEATHFISH"; *l2 = "OR WHAT";          break;
        case 14: *l1 = "BANISHED TO";         *l2 = "THE SHADOW REALM";  break;
        case 13: *l1 = "GAME UNLOADED";                                   break;
        case 12: *l1 = "GAME CRUSHED";                                    break;
        case 11: *l1 = "GAME UNDER";                                      break;
        case 10: *l1 = "GAME MELTED";                                     break;
        case 9:  *l1 = "GAME COOKED";                                     break;
        case 8:  *l1 = "EMBARRASING";                                     break;
        case 7:  *l1 = "GAME VOIDED";                                     break;
        case 6:  *l1 = "FRACTURED TO DEATH";                              break;
        case 5:  *l1 = "GAME TELEFRAGGED";                                break;
        case 4:  *l1 = "GAME KILLED";                                     break;
        case 3:  *l1 = "THERE IS NO RESTART";                             break;
        case 2:  *l1 = "THERE IS NO ESCAPE";                              break;
        default: *l1 = "GAME OVER";                                       break;
    }
}

static void UI_PermadeathMenu_Draw( void ) {
    static vec4_t black  = { 0.0f, 0.0f, 0.0f, 1.0f };
    static vec4_t red    = { 0.8f, 0.0f, 0.0f, 1.0f };
    static vec4_t purple = { 0.5f, 0.0f, 0.8f, 1.0f };
    static vec4_t yellow = { 1.0f, 1.0f, 0.0f, 1.0f };
    static vec4_t teal   = { 0.0f, 0.8f, 0.8f, 1.0f };
    static vec4_t green  = { 0.0f, 0.8f, 0.0f, 1.0f };
    static vec4_t orange = { 1.0f, 0.4f, 0.0f, 1.0f };
    const char    *l1, *l2;
    int            type = s_pd.type;

    if ( s_pd.openTime < 0 ) s_pd.openTime = uis.realtime;

    uis.cursorx = -100;
    uis.cursory = -100;

    PD_GetLines( type, &l1, &l2 );

    UI_FillRect( 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, black );
    UI_DrawHandlePic( PD_IMG_X, PD_IMG_Y, PD_IMG_W, PD_IMG_H,
                      ( type == 15 ) ? s_pd.fishImage : s_pd.image );

    switch ( type ) {
        case 18: /* GAME DENIED: "DENIED" in yellow */
            PD_DrawBannerSplit( SCREEN_WIDTH / 2, PD_TEXT_Y, "GAME ", red, "DENIED", yellow );
            break;
        case 17: /* GAME REDIRECTED: default red */
            PD_DrawBanner( SCREEN_WIDTH / 2, PD_TEXT_Y, l1, UI_CENTER | UI_DROPSHADOW, red );
            break;
        case 14: /* BANISHED TO / THE SHADOW REALM: entirely purple */
            PD_DrawBanner( SCREEN_WIDTH / 2, PD_TEXT_Y,  l1, UI_CENTER | UI_DROPSHADOW, purple );
            PD_DrawBanner( SCREEN_WIDTH / 2, PD_TEXT_Y2, l2, UI_CENTER | UI_DROPSHADOW, purple );
            break;
        case 13: /* GAME UNLOADED: "UNLOADED" in yellow */
            PD_DrawBannerSplit( SCREEN_WIDTH / 2, PD_TEXT_Y, "GAME ", red, "UNLOADED", yellow );
            break;
        case 11: /* GAME UNDER: "UNDER" in teal */
            PD_DrawBannerSplit( SCREEN_WIDTH / 2, PD_TEXT_Y, "GAME ", red, "UNDER", teal );
            break;
        case 10: /* GAME MELTED: "MELTED" in green */
            PD_DrawBannerSplit( SCREEN_WIDTH / 2, PD_TEXT_Y, "GAME ", red, "MELTED", green );
            break;
        case 9:  /* GAME COOKED: "COOKED" in orange */
            PD_DrawBannerSplit( SCREEN_WIDTH / 2, PD_TEXT_Y, "GAME ", red, "COOKED", orange );
            break;
        case 7:  /* GAME VOIDED: "VOIDED" in purple */
            PD_DrawBannerSplit( SCREEN_WIDTH / 2, PD_TEXT_Y, "GAME ", red, "VOIDED", purple );
            break;
        default: /* all other types: red */
            PD_DrawBanner( SCREEN_WIDTH / 2, PD_TEXT_Y,  l1, UI_CENTER | UI_DROPSHADOW, red );
            if ( l2 ) {
                PD_DrawBanner( SCREEN_WIDTH / 2, PD_TEXT_Y2, l2, UI_CENTER | UI_DROPSHADOW, red );
            }
            break;
    }
}

void UI_PermadeathGameOver( void ) {
    memset( &s_pd, 0, sizeof(s_pd) );
    s_pd.type           = (int)trap_Cvar_VariableValue( "permadeath_gameOver" );
    s_pd.menu.draw      = UI_PermadeathMenu_Draw;
    s_pd.menu.key       = UI_PermadeathMenu_Key;
    s_pd.menu.fullscreen = qtrue;

    trap_Cvar_Set( "permadeath_gameOver", "0" );

    s_pd.image      = trap_R_RegisterShaderNoMip( PD_IMAGE );
    s_pd.fishImage  = trap_R_RegisterShaderNoMip( PD_IMAGE_FISH );
    s_pd.whiteFontB = trap_R_RegisterShaderNoMip( PD_FONT_WHITE );
    s_pd.sound      = trap_S_RegisterSound( PD_SOUND, qfalse );
    trap_S_StartLocalSound( s_pd.sound, CHAN_ANNOUNCER );
    s_pd.openTime   = -1;

    UI_PushMenu( &s_pd.menu );
}
