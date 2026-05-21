#include "ui_local.h"

#define GAMEOVER_BACK_X     256     /* 320 - 128/2, centered */
#define GAMEOVER_BACK_Y     300
#define GAMEOVER_BACK_W     128
#define GAMEOVER_BACK_H     64

static const float s_red[4]     = { 1.0f, 0.0f, 0.0f, 1.0f };
static const float s_white[4]   = { 1.0f, 1.0f, 1.0f, 1.0f };
static const float s_overlay[4] = { 0.0f, 0.0f, 0.0f, 0.85f };

typedef struct {
    menuframework_s  menu;
    menubitmap_s     back;
} permadeathGameOverInfo_t;

static permadeathGameOverInfo_t s_permadeathGameOver;

static void PermadeathGameOver_Back( void *ptr, int notification ) {
    if ( notification != QM_ACTIVATED ) {
        return;
    }
    UI_PopMenu();
    /* Disconnect returns to the main menu; progress was already wiped server-side. */
    trap_Cmd_ExecuteText( EXEC_APPEND, "disconnect\n" );
}

static void PermadeathGameOver_Draw( void ) {
    /* Dark semi-transparent overlay */
    UI_FillRect( 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, s_overlay );

    /* "GAME OVER" headline */
    UI_DrawProportionalString( SCREEN_WIDTH / 2, 150,
                               "GAME OVER",
                               UI_CENTER | UI_GIANTFONT,
                               (float *)s_red );

    /* Subtitle */
    UI_DrawProportionalString( SCREEN_WIDTH / 2, 230,
                               "Your campaign progress has been reset.",
                               UI_CENTER | UI_SMALLFONT,
                               (float *)s_white );

    /* Render the Back button */
    Menu_Draw( &s_permadeathGameOver.menu );
}

void UI_PermadeathGameOver_f( void ) {
    memset( &s_permadeathGameOver, 0, sizeof( s_permadeathGameOver ) );

    s_permadeathGameOver.menu.draw       = PermadeathGameOver_Draw;
    s_permadeathGameOver.menu.fullscreen = qtrue;
    s_permadeathGameOver.menu.wrapAround = qtrue;

    s_permadeathGameOver.back.generic.type     = MTYPE_BITMAP;
    s_permadeathGameOver.back.generic.name     = "menu/art/back_0";
    s_permadeathGameOver.back.generic.flags    = QMF_LEFT_JUSTIFY | QMF_PULSEIFFOCUS;
    s_permadeathGameOver.back.generic.callback = PermadeathGameOver_Back;
    s_permadeathGameOver.back.generic.x        = GAMEOVER_BACK_X;
    s_permadeathGameOver.back.generic.y        = GAMEOVER_BACK_Y;
    s_permadeathGameOver.back.width            = GAMEOVER_BACK_W;
    s_permadeathGameOver.back.height           = GAMEOVER_BACK_H;
    s_permadeathGameOver.back.focuspic         = "menu/art/back_1";

    Menu_AddItem( &s_permadeathGameOver.menu, &s_permadeathGameOver.back );

    UI_PushMenu( &s_permadeathGameOver.menu );
}
