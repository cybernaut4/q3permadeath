#include "ui_local.h"

#define ACH_BACK0       "menu/art/back_0"
#define ACH_BACK1       "menu/art/back_1"
#define ACH_RESET0      "menu/art/reset_0"
#define ACH_RESET1      "menu/art/reset_1"
#define ACH_HOVER_PIC   "menu/art/maps_select"
#define ACH_ARROWS_0    "menu/art/gs_arrows_0"
#define ACH_ARROWS_L    "menu/art/gs_arrows_l"
#define ACH_ARROWS_R    "menu/art/gs_arrows_r"

#define ID_ACH_BACK     10
#define ID_ACH_RESET    11
#define ID_ACH_PREVPAGE 12
#define ID_ACH_NEXTPAGE 13

/* 21 game-over achievements (types 1-15 + type 16 OMAE WA MOU + type 17 REDIRECTED
   + type 18 DENIED + type 19 GAME RUINED + Imperfect + Near death) + 21 tier achievements */
#define ACH_GAMEOVER    21
#define ACH_TIER        21
#define ACH_TOTAL       (ACH_GAMEOVER + ACH_TIER)

#define ACH_COLS        6
#define ACH_ROWS        5
#define ACH_ICON_SIZE   40
#define ACH_PER_PAGE    (ACH_COLS * ACH_ROWS)  /* 30 achievements per page */

/* 6 equal-width columns across 640px */
#define ACH_COL_STEP    106
#define ACH_COL_OFF     33      /* (106 - 40) / 2, centered in column */
#define ACH_ROW_STEP    58      /* tighter rows to leave room below the banner */
#define ACH_GRID_Y      72      /* pushed down to clear the ACHIEVEMENTS banner */

/* arrows widget centered horizontally, between grid bottom (row 5: y=344) and label */
#define ACH_ARROWS_X    256     /* (640 - 128) / 2 */
#define ACH_ARROWS_Y    356
#define ACH_LABEL_Y     408
#define ACH_BTN_Y       416

/* maps_select.tga is 256x256, stored bottom-to-top (TGA default).
   Raw scan found content at file rows 81-236; after origin flip that is
   visual rows 19-174 (x: 16-172).  At render size 66: DX=16*66/256=4,
   DY=19*66/256=5, giving a 40x40 visible frame aligned with the icon. */
#define ACH_HOVER_W  66
#define ACH_HOVER_DX  4
#define ACH_HOVER_DY  5

typedef struct {
    menuframework_s menu;
    menubitmap_s    back;
    menubitmap_s    reset;
    menubitmap_s    arrows;
    menubitmap_s    left;
    menubitmap_s    right;
    qhandle_t       icons[ACH_TOTAL];
    qhandle_t       lockedShader;
    qhandle_t       hoverShader;
    int             unlocked;       /* pd_achievements bits 0-14: game-over types */
    int             tier_unlocked;  /* pd_tier_ach bits 0-20: tier completions */
    int             hoverIdx;
    int             page;
    int             numpages;
} achmenu_t;

static achmenu_t s_ach;

/* Tier slots are sorted by skill track first, then by tier:
   slots 21-27 = skill123 tier1-7
   slots 28-34 = skill4   tier1-7
   slots 35-41 = skill5   tier1-7 */
static const char *s_achIcons[ACH_TOTAL] = {
    /* slots 0-14: game-over achievement icons (medal art, types 1-15) */
    "menu/medals/medal_victory",
    "menu/medals/medal_gauntlet",
    "menu/medals/medal_impressive",
    "menu/medals/medal_frags",
    "menu/medals/medal_excellent",
    "menu/medals/medal_accuracy",
    "menu/medals/medal_assist",
    "menu/medals/medal_defend",
    "menu/medals/medal_capture",
    "menu/medals/medal_excellent",
    "menu/medals/medal_gauntlet",
    "menu/medals/medal_frags",
    "menu/medals/medal_impressive",
    "menu/medals/medal_accuracy",
    "menu/medals/medal_victory",
    /* slot 15: type 16 OMAE WA MOU SHNDEIRU */
    "menu/medals/medal_frags",
    /* slot 16: type 17 GAME REDIRECTED */
    "menu/medals/medal_assist",
    /* slot 17: type 18 GAME DENIED */
    "menu/medals/medal_capture",
    /* slot 18: type 19 GAME RUINED */
    "menu/medals/medal_defend",
    /* slot 19: Imperfect (lost without dying) */
    "menu/medals/medal_victory",
    /* slot 20: Near death (won with <= 25 health) */
    "menu/medals/medal_accuracy",
    /* slots 21-27: skill123 tier 1-7 */
    "menu/achievements/skill123-tier1",
    "menu/achievements/skill123-tier2",
    "menu/achievements/skill123-tier3",
    "menu/achievements/skill123-tier4",
    "menu/achievements/skill123-tier5",
    "menu/achievements/skill123-tier6",
    "menu/achievements/skill123-tier7",
    /* slots 28-34: skill4 tier 1-7 */
    "menu/achievements/skill4-tier1",
    "menu/achievements/skill4-tier2",
    "menu/achievements/skill4-tier3",
    "menu/achievements/skill4-tier4",
    "menu/achievements/skill4-tier5",
    "menu/achievements/skill4-tier6",
    "menu/achievements/skill4-tier7",
    /* slots 35-41: skill5 tier 1-7 */
    "menu/achievements/skill5-tier1",
    "menu/achievements/skill5-tier2",
    "menu/achievements/skill5-tier3",
    "menu/achievements/skill5-tier4",
    "menu/achievements/skill5-tier5",
    "menu/achievements/skill5-tier6",
    "menu/achievements/skill5-tier7",
};

static const char *s_achNames[ACH_TOTAL] = {
    /* game-over types 1-15 */
    "DEAD",
    "ESCAPIST",
    "RESTARTER",
    "SELF-INFLICTED",
    "WRONG PLACE",
    "GRAVITY CHECK",
    "INTO THE VOID",
    "ZERO KDR",
    "OVERCOOKED",
    "LIQUIDATED",
    "DEEP DIVE",
    "BETWEEN A ROCK",
    "OUT OF AMMO",
    "EXILED",
    "FISHY BUSINESS",
    /* game-over type 16 */
    "OMAE WA MOU",
    /* game-over type 17 */
    "REDIRECTED",
    /* game-over type 18 */
    "DENIED",
    /* game-over type 19 */
    "RUINED",
    /* Imperfect (lost without dying) */
    "IMPERFECT",
    /* Near death (won with <= 25 health) */
    "NEAR DEATH",
    /* skill123 tier 1-7 */
    "TRAINEE",
    "SKILLED",
    "COMBATANT",
    "WARRIOR",
    "VETERAN",
    "MASTER",
    "ELITE",
    /* skill4 tier 1-7 */
    "HARDCORE TRAINEE",
    "HARDCORE SKILLED",
    "HARDCORE COMBATANT",
    "HARDCORE WARRIOR",
    "HARDCORE VETERAN",
    "HARDCORE MASTER",
    "HARDCORE",
    /* skill5 tier 1-7 */
    "NIGHTMARISH TRAINEE",
    "NIGHTMARISH SKILLED",
    "NIGHTMARISH COMBATANT",
    "NIGHTMARISH WARRIOR",
    "NIGHTMARISH VETERAN",
    "NIGHTMARISH MASTER",
    "NIGHTMARE!",
};

/* idx is the page-local slot (0 to ACH_PER_PAGE-1) */
static void ACH_CellPos( int idx, int *x, int *y ) {
    *x = (idx % ACH_COLS) * ACH_COL_STEP + ACH_COL_OFF;
    *y = ACH_GRID_Y + (idx / ACH_COLS) * ACH_ROW_STEP;
}

static int ACH_GetHoverIdx( void ) {
    int i, x, y;
    int cx = uis.cursorx;
    int cy = uis.cursory;
    int page_start = s_ach.page * ACH_PER_PAGE;
    for ( i = 0; i < ACH_PER_PAGE; i++ ) {
        int ach_idx = page_start + i;
        if ( ach_idx >= ACH_TOTAL ) break;
        ACH_CellPos( i, &x, &y );
        if ( cx >= x && cx < x + ACH_ICON_SIZE &&
             cy >= y && cy < y + ACH_ICON_SIZE ) {
            return ach_idx;
        }
    }
    return -1;
}

/* Map display slot i (>= ACH_GAMEOVER) to its bit index in pd_tier_ach.
   Slots are sorted skill-first: 7 skill123 slots, then 7 skill4, then 7 skill5.
   Bits in pd_tier_ach are grouped tier-first: (tier-1)*3 + track. */
static int ACH_TierBit( int i ) {
    int t        = i - ACH_GAMEOVER; /* 0-20 */
    int track    = t / 7;            /* 0=skill123, 1=skill4, 2=skill5 */
    int tier_idx = t % 7;            /* 0-6 for tiers 1-7 */
    return tier_idx * 3 + track;
}

static void ACH_ConfirmResetAction( qboolean result ) {
    if ( !result ) return;
    trap_Cvar_Set( "pd_achievements", "0" );
    trap_Cvar_Set( "pd_tier_ach", "0" );
    s_ach.unlocked = 0;
    s_ach.tier_unlocked = 0;
}

static void ACH_MenuEvent( void *ptr, int event ) {
    if ( event != QM_ACTIVATED ) return;
    switch ( ((menucommon_s *)ptr)->id ) {
    case ID_ACH_BACK:
        UI_PopMenu();
        break;
    case ID_ACH_RESET:
        UI_ConfirmMenu( "RESET ALL ACHIEVEMENTS?", NULL, ACH_ConfirmResetAction );
        break;
    case ID_ACH_PREVPAGE:
        if ( s_ach.page > 0 ) s_ach.page--;
        break;
    case ID_ACH_NEXTPAGE:
        if ( s_ach.page < s_ach.numpages - 1 ) s_ach.page++;
        break;
    }
}

static sfxHandle_t ACH_Key( int key ) {
    switch ( key ) {
    case K_LEFTARROW:
    case K_KP_LEFTARROW:
        if ( s_ach.page > 0 ) {
            s_ach.page--;
            return menu_move_sound;
        }
        return menu_buzz_sound;
    case K_RIGHTARROW:
    case K_KP_RIGHTARROW:
        if ( s_ach.page < s_ach.numpages - 1 ) {
            s_ach.page++;
            return menu_move_sound;
        }
        return menu_buzz_sound;
    }
    return Menu_DefaultKey( &s_ach.menu, key );
}

static void ACH_Draw( void ) {
    static vec4_t black = { 0.0f, 0.0f, 0.0f, 1.0f };
    static vec4_t white = { 1.0f, 1.0f, 1.0f, 1.0f };
    int i, x, y, ach_idx, is_unlocked;
    int page_start = s_ach.page * ACH_PER_PAGE;
    int go_eff, tier_eff;
    int debug = (int)trap_Cvar_VariableValue( "pd_ach_debug" );

    go_eff   = debug ? 0x1FFFFF  : s_ach.unlocked;
    tier_eff = debug ? 0x1FFFFF  : s_ach.tier_unlocked;

    s_ach.hoverIdx = ACH_GetHoverIdx();

    /* update arrow button active state */
    if ( s_ach.numpages > 1 ) {
        if ( s_ach.page > 0 )
            s_ach.left.generic.flags  &= ~QMF_INACTIVE;
        else
            s_ach.left.generic.flags  |= QMF_INACTIVE;
        if ( s_ach.page < s_ach.numpages - 1 )
            s_ach.right.generic.flags &= ~QMF_INACTIVE;
        else
            s_ach.right.generic.flags |= QMF_INACTIVE;
    } else {
        s_ach.left.generic.flags  |= QMF_INACTIVE;
        s_ach.right.generic.flags |= QMF_INACTIVE;
    }

    UI_FillRect( 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, black );

    UI_DrawBannerString( SCREEN_WIDTH / 2, 10, "ACHIEVEMENTS",
                         UI_CENTER | UI_DROPSHADOW, menu_text_color );

    trap_R_SetColor( NULL );
    for ( i = 0; i < ACH_PER_PAGE; i++ ) {
        ach_idx = page_start + i;
        if ( ach_idx >= ACH_TOTAL ) break;

        ACH_CellPos( i, &x, &y );

        if ( ach_idx < ACH_GAMEOVER )
            is_unlocked = go_eff & (1 << ach_idx);
        else
            is_unlocked = tier_eff & (1 << ACH_TierBit( ach_idx ));

        if ( is_unlocked ) {
            if ( s_ach.icons[ach_idx] )
                UI_DrawHandlePic( x, y, ACH_ICON_SIZE, ACH_ICON_SIZE, s_ach.icons[ach_idx] );
            if ( ach_idx == s_ach.hoverIdx && s_ach.hoverShader )
                UI_DrawHandlePic( x - ACH_HOVER_DX, y - ACH_HOVER_DY,
                                  ACH_HOVER_W, ACH_HOVER_W, s_ach.hoverShader );
        } else {
            UI_DrawHandlePic( x, y, ACH_ICON_SIZE, ACH_ICON_SIZE, s_ach.lockedShader );
        }
    }

    if ( s_ach.hoverIdx >= 0 ) {
        int hover_bit;
        if ( s_ach.hoverIdx < ACH_GAMEOVER )
            hover_bit = go_eff & (1 << s_ach.hoverIdx);
        else
            hover_bit = tier_eff & (1 << ACH_TierBit( s_ach.hoverIdx ));

        if ( hover_bit ) {
            UI_DrawProportionalString( SCREEN_WIDTH / 2, ACH_LABEL_Y,
                                       s_achNames[s_ach.hoverIdx],
                                       UI_CENTER | UI_SMALLFONT | UI_DROPSHADOW, white );
        }
    }

    Menu_Draw( &s_ach.menu );
}

void UI_AchievementsMenu( void ) {
    int i;

    memset( &s_ach, 0, sizeof(s_ach) );
    s_ach.menu.draw       = ACH_Draw;
    s_ach.menu.key        = ACH_Key;
    s_ach.menu.fullscreen = qtrue;
    s_ach.menu.wrapAround = qtrue;
    s_ach.unlocked      = (int)trap_Cvar_VariableValue( "pd_achievements" );
    s_ach.tier_unlocked = (int)trap_Cvar_VariableValue( "pd_tier_ach" );
    s_ach.page          = 0;
    s_ach.numpages      = ( ACH_TOTAL + ACH_PER_PAGE - 1 ) / ACH_PER_PAGE;

    for ( i = 0; i < ACH_TOTAL; i++ )
        s_ach.icons[i] = trap_R_RegisterShaderNoMip( s_achIcons[i] );

    s_ach.lockedShader = trap_R_RegisterShaderNoMip( "menu/achievements/locked" );
    s_ach.hoverShader  = trap_R_RegisterShaderNoMip( ACH_HOVER_PIC );

    s_ach.arrows.generic.type  = MTYPE_BITMAP;
    s_ach.arrows.generic.name  = ACH_ARROWS_0;
    s_ach.arrows.generic.flags = QMF_INACTIVE;
    s_ach.arrows.generic.x     = ACH_ARROWS_X;
    s_ach.arrows.generic.y     = ACH_ARROWS_Y;
    s_ach.arrows.width         = 128;
    s_ach.arrows.height        = 32;

    s_ach.left.generic.type     = MTYPE_BITMAP;
    s_ach.left.generic.flags    = QMF_LEFT_JUSTIFY | QMF_PULSEIFFOCUS;
    s_ach.left.generic.id       = ID_ACH_PREVPAGE;
    s_ach.left.generic.callback = ACH_MenuEvent;
    s_ach.left.generic.x        = ACH_ARROWS_X;
    s_ach.left.generic.y        = ACH_ARROWS_Y;
    s_ach.left.width            = 64;
    s_ach.left.height           = 32;
    s_ach.left.focuspic         = ACH_ARROWS_L;

    s_ach.right.generic.type     = MTYPE_BITMAP;
    s_ach.right.generic.flags    = QMF_LEFT_JUSTIFY | QMF_PULSEIFFOCUS;
    s_ach.right.generic.id       = ID_ACH_NEXTPAGE;
    s_ach.right.generic.callback = ACH_MenuEvent;
    s_ach.right.generic.x        = ACH_ARROWS_X + 61;
    s_ach.right.generic.y        = ACH_ARROWS_Y;
    s_ach.right.width            = 64;
    s_ach.right.height           = 32;
    s_ach.right.focuspic         = ACH_ARROWS_R;

    s_ach.back.generic.type     = MTYPE_BITMAP;
    s_ach.back.generic.name     = ACH_BACK0;
    s_ach.back.generic.flags    = QMF_LEFT_JUSTIFY | QMF_PULSEIFFOCUS;
    s_ach.back.generic.id       = ID_ACH_BACK;
    s_ach.back.generic.callback = ACH_MenuEvent;
    s_ach.back.generic.x        = 0;
    s_ach.back.generic.y        = ACH_BTN_Y;
    s_ach.back.width            = 128;
    s_ach.back.height           = 64;
    s_ach.back.focuspic         = ACH_BACK1;

    s_ach.reset.generic.type     = MTYPE_BITMAP;
    s_ach.reset.generic.name     = ACH_RESET0;
    s_ach.reset.generic.flags    = QMF_LEFT_JUSTIFY | QMF_PULSEIFFOCUS;
    s_ach.reset.generic.id       = ID_ACH_RESET;
    s_ach.reset.generic.callback = ACH_MenuEvent;
    s_ach.reset.generic.x        = 512;
    s_ach.reset.generic.y        = ACH_BTN_Y;
    s_ach.reset.width            = 128;
    s_ach.reset.height           = 64;
    s_ach.reset.focuspic         = ACH_RESET1;

    Menu_AddItem( &s_ach.menu, &s_ach.arrows );
    Menu_AddItem( &s_ach.menu, &s_ach.left );
    Menu_AddItem( &s_ach.menu, &s_ach.right );
    Menu_AddItem( &s_ach.menu, &s_ach.back );
    Menu_AddItem( &s_ach.menu, &s_ach.reset );

    UI_PushMenu( &s_ach.menu );
}
