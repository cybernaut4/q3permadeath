/*
 * ui_pd_stats.c  —  Permadeath statistics menu (fifth iteration)
 *
 * Two-column layout mirroring the System/Display menu (ui_display.c):
 *   Left column  : difficulty tabs (Hurt Me Plenty / Hardcore / Nightmare! / Overall)
 *   Right column : per-difficulty stats drawn directly in the custom draw hook
 *
 * All "pd_s[track]_[stat]" CVARs are registered CVAR_ARCHIVE here so they
 * survive across game sessions even when the game DLL was never loaded.
 */
#include "ui_local.h"

/* -------------------------------------------------------------------------
   Layout constants (match System menu from ui_display.c)
   ------------------------------------------------------------------------- */
#define PDS_LEFTFRAME   "menu/art/frame2_l"
#define PDS_RIGHTFRAME  "menu/art/frame1_r"
#define PDS_BACK0       "menu/art/back_0"
#define PDS_BACK1       "menu/art/back_1"
#define PDS_RESET0      "menu/art/reset_0"
#define PDS_RESET1      "menu/art/reset_1"

/* Tab buttons are right-justified into the left frame.
   Four tabs centred around y=240 with PROP_HEIGHT=27 spacing. */
#define PDS_TAB_X       320
#define PDS_TAB_Y_BASE  (240 - 2*PROP_HEIGHT)   /* = 186 */

/* Right-panel content (UI_DrawString charset font, like System Setup).
   Labels are right-justified at PDS_LX; values left-justified at PDS_VX. */
#define PDS_LX          440   /* label right edge (right-justify pivot) */
#define PDS_VX          448   /* value left edge */
#define PDS_RY          160   /* first stats line y */
#define PDS_LINE        18    /* BIGCHAR_HEIGHT(16) + 2 */

/* Achievements button: right-aligned in the left column, same y as original centred position */
#define PDS_ACH_X       PDS_TAB_X
#define PDS_ACH_Y       355

/* Reset button: bottom-right corner */
#define PDS_RESET_X     512
#define PDS_RESET_Y     (480 - 64)

/* Back button: bottom-left corner */
#define PDS_BACK_X      0
#define PDS_BACK_Y      (480 - 64)

#define PD_VERSION      "0.5"

/* Menu item IDs */
#define ID_PDS_BACK     10
#define ID_PDS_HMP      11
#define ID_PDS_HC       12
#define ID_PDS_NM       13
#define ID_PDS_OVERALL  14
#define ID_PDS_ACHIEVE  15
#define ID_PDS_RESET    16

#define PDS_NUMTABS     4

/* -------------------------------------------------------------------------
   SP campaign map order (index 0-25, stored furthest uses 1-based index,
   0 = never played)
   ------------------------------------------------------------------------- */
#define PDS_CAMPAIGN_SIZE 26
static const char *s_campaign[PDS_CAMPAIGN_SIZE] = {
	"q3dm0",
	"q3dm1",  "q3dm2",  "q3dm3",  "q3tourney1",
	"q3dm4",  "q3dm5",  "q3dm6",  "q3tourney2",
	"q3dm7",  "q3dm8",  "q3dm9",  "q3tourney3",
	"q3dm10", "q3dm11", "q3dm12", "q3tourney4",
	"q3dm13", "q3dm14", "q3dm15", "q3tourney5",
	"q3dm16", "q3dm17", "q3dm18", "q3dm19",
	"q3tourney6"
};

/* -------------------------------------------------------------------------
   Stats data struct
   ------------------------------------------------------------------------- */
typedef struct {
	int deaths;
	int furthest;    /* 1-based campaign index, 0 = never played */
	int maxtime;     /* longest single run, seconds */
	int totaltime;   /* sum of all completed runs, seconds */
	int curtime;     /* current ongoing run (as of last map load), seconds */
	int defeats;
	int wins;        /* matches won */
	int gamewon;     /* full campaign completions */
	int rec_impr;    /* best impressive medals in a single match */
	int rec_excel;   /* best excellent medals in a single match */
	int rec_gaunt;   /* best gauntlet frags in a single match */
	int neardeaths;  /* matches won with 25 or fewer health */
	int d_emb, d_kill, d_frac, d_melt, d_cook;
	int d_abyss, d_tfrag, d_tport, d_ammo, d_drown, d_fish;
} pdStats_t;

/* -------------------------------------------------------------------------
   Menu struct
   ------------------------------------------------------------------------- */
typedef struct {
	menuframework_s menu;
	menubitmap_s    framel;
	menubitmap_s    framer;
	menutext_s      banner;
	menutext_s      tabs[PDS_NUMTABS];
	menutext_s      achieve_btn;
	menubitmap_s    reset_btn;
	menubitmap_s    back;
	int             curTab;    /* 0=Overall 1=HMP 2=HC 3=NM */
} pdstats_t;

static pdstats_t s_pds;

/* mutable colour arrays so we can highlight the active tab */
static float s_tabColors[PDS_NUMTABS][4];

static vec4_t pds_sep_color = { 0.5f, 0.5f, 0.5f, 0.5f };

/* -------------------------------------------------------------------------
   CVAR registration
   All 51 stats CVARs registered CVAR_ARCHIVE once when the menu opens.
   ------------------------------------------------------------------------- */
static void PDS_RegisterCvars( void ) {
	static const char *tracks[] = { "s123", "s4", "s5" };
	static const char *stats[]  = {
		"deaths",   "furthest",  "maxtime",  "curtime", "totaltime", "defeats",
		"wins",     "gamewon",
		"rec_impr", "rec_excel", "rec_gaunt", "neardeaths",
		"d_emb",    "d_kill",    "d_frac",   "d_melt",  "d_cook",
		"d_abyss",  "d_tfrag",   "d_tport",  "d_ammo",  "d_drown", "d_fish"
	};
	vmCvar_t tmp;
	char     name[64];
	int      i, j;
	for ( i = 0; i < 3; i++ ) {
		for ( j = 0; j < 23; j++ ) {
			Com_sprintf( name, sizeof(name), "pd_%s_%s", tracks[i], stats[j] );
			trap_Cvar_Register( &tmp, name, "0", CVAR_ARCHIVE );
		}
	}
}

/* -------------------------------------------------------------------------
   Read stats for one track ("s123", "s4", or "s5")
   ------------------------------------------------------------------------- */
static void PDS_ReadStats( const char *track, pdStats_t *s ) {
	char n[64];
#define RD(field, suffix) \
	Com_sprintf(n, sizeof(n), "pd_%s_" suffix, track); \
	s->field = (int)trap_Cvar_VariableValue(n)

	RD(deaths,    "deaths");
	RD(furthest,  "furthest");
	RD(maxtime,   "maxtime");
	RD(totaltime, "totaltime");
	RD(curtime,   "curtime");
	RD(defeats,   "defeats");
	RD(wins,      "wins");
	RD(gamewon,   "gamewon");
	RD(rec_impr,   "rec_impr");
	RD(rec_excel,  "rec_excel");
	RD(rec_gaunt,  "rec_gaunt");
	RD(neardeaths, "neardeaths");
	RD(d_emb,     "d_emb");
	RD(d_kill,  "d_kill");
	RD(d_frac,  "d_frac");
	RD(d_melt,  "d_melt");
	RD(d_cook,  "d_cook");
	RD(d_abyss, "d_abyss");
	RD(d_tfrag, "d_tfrag");
	RD(d_tport, "d_tport");
	RD(d_ammo,  "d_ammo");
	RD(d_drown, "d_drown");
	RD(d_fish,  "d_fish");
#undef RD
}

/* -------------------------------------------------------------------------
   Tab colour state (active = white, others = red)
   ------------------------------------------------------------------------- */
static void PDS_UpdateTabColors( void ) {
	int i;
	for ( i = 0; i < PDS_NUMTABS; i++ ) {
		if ( i == s_pds.curTab ) {
			/* active tab: white, no pulse */
			s_tabColors[i][0] = 1.0f; s_tabColors[i][1] = 1.0f;
			s_tabColors[i][2] = 1.0f; s_tabColors[i][3] = 1.0f;
			s_pds.tabs[i].generic.flags = QMF_RIGHT_JUSTIFY;
		} else {
			/* inactive tab: red, pulse on focus */
			s_tabColors[i][0] = color_red[0]; s_tabColors[i][1] = color_red[1];
			s_tabColors[i][2] = color_red[2]; s_tabColors[i][3] = color_red[3];
			s_pds.tabs[i].generic.flags = QMF_RIGHT_JUSTIFY | QMF_PULSEIFFOCUS;
		}
	}
}

/* -------------------------------------------------------------------------
   Format seconds as  M:SS  or  H:MM:SS
   ------------------------------------------------------------------------- */
static void PDS_FormatTime( int secs, char *buf, int bufsize ) {
	int h = secs / 3600;
	int m = (secs % 3600) / 60;
	int s = secs % 60;
	if ( h > 0 )
		Com_sprintf( buf, bufsize, "%d:%02d:%02d", h, m, s );
	else
		Com_sprintf( buf, bufsize, "%02d:%02d", m, s );
}

/* -------------------------------------------------------------------------
   Draw the right-panel statistics for a resolved pdStats_t.
   ------------------------------------------------------------------------- */
static void PDS_DrawStatsContent( const pdStats_t *s ) {
	char  buf[64];
	int   y = PDS_RY;
	const char *mapname;

	/* helper: draw a "LABEL     value" line using the charset (console) font */
#define STAT_LINE(label, valstr) \
	UI_DrawString( PDS_LX, y, label, UI_RIGHT|UI_SMALLFONT, text_color_normal ); \
	UI_DrawString( PDS_VX, y, valstr, UI_LEFT|UI_SMALLFONT, text_color_normal ); \
	y += PDS_LINE

	/* --- Base stats (always shown) --- */
	Com_sprintf( buf, sizeof(buf), "%i", s->deaths );
	STAT_LINE( "Total deaths:", buf );

	if ( s->furthest > 0 )
		mapname = s_campaign[s->furthest - 1];
	else
		mapname = "N/A";
	STAT_LINE( "Furthest map:", mapname );

	if ( s->maxtime > 0 ) {
		PDS_FormatTime( s->maxtime, buf, sizeof(buf) );
	} else {
		Q_strncpyz( buf, "N/A", sizeof(buf) );
	}
	STAT_LINE( "Longest alive:", buf );

	PDS_FormatTime( s->totaltime + s->curtime, buf, sizeof(buf) );
	STAT_LINE( "Time played:", buf );

	Com_sprintf( buf, sizeof(buf), "%i", s->defeats );
	STAT_LINE( "Defeats:", buf );

	Com_sprintf( buf, sizeof(buf), "%i", s->wins );
	STAT_LINE( "Matches won:", buf );

	if ( s->gamewon > 0 ) {
		Com_sprintf( buf, sizeof(buf), "%i", s->gamewon );
		STAT_LINE( "Games won:", buf );
	}

	if ( s->neardeaths > 0 ) {
		Com_sprintf( buf, sizeof(buf), "%i", s->neardeaths );
		STAT_LINE( "Near deaths:", buf );
	}

	/* --- Medals record (only shown when at least one value > 0) --- */
	if ( s->rec_impr > 0 || s->rec_excel > 0 || s->rec_gaunt > 0 ) {
		y += PDS_LINE / 2;
		UI_FillRect( 335, y, 230, 1, pds_sep_color );
		y += PDS_LINE / 2 + 2;

		if ( s->rec_impr > 0 ) {
			Com_sprintf( buf, sizeof(buf), "%i", s->rec_impr );
			STAT_LINE( "Best impressive:", buf );
		}
		if ( s->rec_excel > 0 ) {
			Com_sprintf( buf, sizeof(buf), "%i", s->rec_excel );
			STAT_LINE( "Best excellent:", buf );
		}
		if ( s->rec_gaunt > 0 ) {
			Com_sprintf( buf, sizeof(buf), "%i", s->rec_gaunt );
			STAT_LINE( "Best gauntlet:", buf );
		}
	}

	/* --- Separator --- */
	y += PDS_LINE / 2;
	UI_FillRect( 335, y, 230, 1, pds_sep_color );
	y += PDS_LINE / 2 + 2;

	/* --- Conditional death reasons (only shown if count > 0) --- */
#define REASON_LINE(label, count) \
	if ( (count) > 0 ) { \
		Com_sprintf( buf, sizeof(buf), "%i", count ); \
		STAT_LINE( label, buf ); \
	}

	REASON_LINE( "Embarrasing:",  s->d_emb );
	REASON_LINE( "Suicide:",      s->d_kill );
	REASON_LINE( "Fractured:",    s->d_frac );
	REASON_LINE( "Melted:",       s->d_melt );
	REASON_LINE( "Cooked:",       s->d_cook );
	REASON_LINE( "Abyss:",        s->d_abyss );
	REASON_LINE( "Telefragged:",  s->d_tfrag );
	REASON_LINE( "Wrong place:",  s->d_tport );
	REASON_LINE( "Out of ammo:",  s->d_ammo );
	REASON_LINE( "Drowned:",      s->d_drown );
	REASON_LINE( "Deathfish:",    s->d_fish );

#undef REASON_LINE
#undef STAT_LINE
}

/* -------------------------------------------------------------------------
   Custom draw hook — called by Menu_Draw after all menu items are rendered
   ------------------------------------------------------------------------- */
static void PDS_Draw( void ) {
	pdStats_t s, s123, s4, s5;
	static vec4_t verColor = {0.5f, 0.0f, 0.0f, 1.0f};

	Menu_Draw( &s_pds.menu );

	UI_DrawString( 320, 52, "Version " PD_VERSION, UI_CENTER | UI_SMALLFONT, verColor );

	switch ( s_pds.curTab ) {
		default:
		case 0:
			PDS_ReadStats( "s123", &s123 );
			PDS_ReadStats( "s4",   &s4 );
			PDS_ReadStats( "s5",   &s5 );
			s.deaths    = s123.deaths  + s4.deaths  + s5.deaths;
			s.furthest  = s123.furthest > s4.furthest ? s123.furthest : s4.furthest;
			if ( s5.furthest > s.furthest ) s.furthest = s5.furthest;
			s.maxtime   = s123.maxtime  > s4.maxtime  ? s123.maxtime  : s4.maxtime;
			if ( s5.maxtime  > s.maxtime  ) s.maxtime  = s5.maxtime;
			s.totaltime = s123.totaltime + s4.totaltime + s5.totaltime;
			s.curtime   = s123.curtime   + s4.curtime   + s5.curtime;
			s.defeats   = s123.defeats + s4.defeats + s5.defeats;
			s.wins      = s123.wins    + s4.wins    + s5.wins;
			s.gamewon   = s123.gamewon + s4.gamewon + s5.gamewon;
			s.rec_impr  = s123.rec_impr  > s4.rec_impr  ? s123.rec_impr  : s4.rec_impr;
			if ( s5.rec_impr  > s.rec_impr  ) s.rec_impr  = s5.rec_impr;
			s.rec_excel = s123.rec_excel > s4.rec_excel ? s123.rec_excel : s4.rec_excel;
			if ( s5.rec_excel > s.rec_excel ) s.rec_excel = s5.rec_excel;
			s.rec_gaunt  = s123.rec_gaunt > s4.rec_gaunt ? s123.rec_gaunt : s4.rec_gaunt;
			if ( s5.rec_gaunt > s.rec_gaunt ) s.rec_gaunt = s5.rec_gaunt;
			s.neardeaths = s123.neardeaths + s4.neardeaths + s5.neardeaths;
			s.d_emb     = s123.d_emb   + s4.d_emb   + s5.d_emb;
			s.d_kill    = s123.d_kill  + s4.d_kill  + s5.d_kill;
			s.d_frac    = s123.d_frac  + s4.d_frac  + s5.d_frac;
			s.d_melt    = s123.d_melt  + s4.d_melt  + s5.d_melt;
			s.d_cook    = s123.d_cook  + s4.d_cook  + s5.d_cook;
			s.d_abyss   = s123.d_abyss + s4.d_abyss + s5.d_abyss;
			s.d_tfrag   = s123.d_tfrag + s4.d_tfrag + s5.d_tfrag;
			s.d_tport   = s123.d_tport + s4.d_tport + s5.d_tport;
			s.d_ammo    = s123.d_ammo  + s4.d_ammo  + s5.d_ammo;
			s.d_drown   = s123.d_drown + s4.d_drown + s5.d_drown;
			s.d_fish    = s123.d_fish  + s4.d_fish  + s5.d_fish;
			break;
		case 1: PDS_ReadStats( "s123", &s ); break;
		case 2: PDS_ReadStats( "s4",   &s ); break;
		case 3: PDS_ReadStats( "s5",   &s ); break;
	}

	PDS_DrawStatsContent( &s );
}

/* -------------------------------------------------------------------------
   Reset confirmation callback — zeros all 51 stats CVARs
   ------------------------------------------------------------------------- */
static void PDS_ConfirmResetAction( qboolean result ) {
	static const char *tracks[] = { "s123", "s4", "s5" };
	static const char *stats[]  = {
		"deaths",   "furthest",  "maxtime",  "curtime", "totaltime", "defeats",
		"wins",     "gamewon",
		"rec_impr", "rec_excel", "rec_gaunt", "neardeaths",
		"d_emb",    "d_kill",    "d_frac",   "d_melt",  "d_cook",
		"d_abyss",  "d_tfrag",   "d_tport",  "d_ammo",  "d_drown", "d_fish"
	};
	char name[64];
	int  i, j;
	if ( !result ) return;
	for ( i = 0; i < 3; i++ ) {
		for ( j = 0; j < 23; j++ ) {
			Com_sprintf( name, sizeof(name), "pd_%s_%s", tracks[i], stats[j] );
			trap_Cvar_Set( name, "0" );
		}
	}
}

/* -------------------------------------------------------------------------
   Event handler
   ------------------------------------------------------------------------- */
static void PDS_MenuEvent( void *ptr, int event ) {
	if ( event != QM_ACTIVATED ) return;

	switch ( ((menucommon_s *)ptr)->id ) {
		case ID_PDS_BACK:
			UI_PopMenu();
			break;

		case ID_PDS_OVERALL:
			s_pds.curTab = 0;
			PDS_UpdateTabColors();
			break;

		case ID_PDS_HMP:
			s_pds.curTab = 1;
			PDS_UpdateTabColors();
			break;

		case ID_PDS_HC:
			s_pds.curTab = 2;
			PDS_UpdateTabColors();
			break;

		case ID_PDS_NM:
			s_pds.curTab = 3;
			PDS_UpdateTabColors();
			break;

		case ID_PDS_ACHIEVE:
			UI_AchievementsMenu();
			break;

		case ID_PDS_RESET:
			UI_ConfirmMenu( "RESET ALL STATISTICS?", NULL, PDS_ConfirmResetAction );
			break;
	}
}

/* -------------------------------------------------------------------------
   Cache
   ------------------------------------------------------------------------- */
void UI_PdStatsMenu_Cache( void ) {
	trap_R_RegisterShaderNoMip( PDS_LEFTFRAME );
	trap_R_RegisterShaderNoMip( PDS_RIGHTFRAME );
	trap_R_RegisterShaderNoMip( PDS_BACK0 );
	trap_R_RegisterShaderNoMip( PDS_BACK1 );
	trap_R_RegisterShaderNoMip( PDS_RESET0 );
	trap_R_RegisterShaderNoMip( PDS_RESET1 );
}

/* -------------------------------------------------------------------------
   Init + push
   ------------------------------------------------------------------------- */
static void PDS_Init( void ) {
	int i;
	static const char *tabNames[PDS_NUMTABS] = {
		"OVERALL", "HURT ME PLENTY", "HARDCORE", "NIGHTMARE!"
	};
	static const int   tabIDs[PDS_NUMTABS]   = {
		ID_PDS_OVERALL, ID_PDS_HMP, ID_PDS_HC, ID_PDS_NM
	};

	PDS_RegisterCvars();

	memset( &s_pds, 0, sizeof(s_pds) );
	s_pds.curTab = 0;

	UI_PdStatsMenu_Cache();

	s_pds.menu.wrapAround = qtrue;
	s_pds.menu.fullscreen = qtrue;
	s_pds.menu.draw       = PDS_Draw;   /* custom draw hook */

	/* ---------- banner ---------- */
	s_pds.banner.generic.type  = MTYPE_BTEXT;
	s_pds.banner.generic.flags = QMF_CENTER_JUSTIFY;
	s_pds.banner.generic.x     = 320;
	s_pds.banner.generic.y     = 16;
	s_pds.banner.string        = "PERMADEATH";
	s_pds.banner.color         = color_white;
	s_pds.banner.style         = UI_CENTER;

	/* ---------- left frame ---------- */
	s_pds.framel.generic.type  = MTYPE_BITMAP;
	s_pds.framel.generic.name  = PDS_LEFTFRAME;
	s_pds.framel.generic.flags = QMF_INACTIVE;
	s_pds.framel.generic.x     = 0;
	s_pds.framel.generic.y     = 78;
	s_pds.framel.width         = 256;
	s_pds.framel.height        = 329;

	/* ---------- right frame ---------- */
	s_pds.framer.generic.type  = MTYPE_BITMAP;
	s_pds.framer.generic.name  = PDS_RIGHTFRAME;
	s_pds.framer.generic.flags = QMF_INACTIVE;
	s_pds.framer.generic.x     = 376;
	s_pds.framer.generic.y     = 76;
	s_pds.framer.width         = 256;
	s_pds.framer.height        = 334;

	/* ---------- difficulty tabs ---------- */
	for ( i = 0; i < PDS_NUMTABS; i++ ) {
		/* initialise mutable colour slot */
		s_tabColors[i][0] = color_red[0];
		s_tabColors[i][1] = color_red[1];
		s_tabColors[i][2] = color_red[2];
		s_tabColors[i][3] = color_red[3];

		s_pds.tabs[i].generic.type     = MTYPE_PTEXT;
		s_pds.tabs[i].generic.flags    = QMF_RIGHT_JUSTIFY | QMF_PULSEIFFOCUS;
		s_pds.tabs[i].generic.id       = tabIDs[i];
		s_pds.tabs[i].generic.callback = PDS_MenuEvent;
		s_pds.tabs[i].generic.x        = PDS_TAB_X;
		s_pds.tabs[i].generic.y        = PDS_TAB_Y_BASE + i * PROP_HEIGHT;
		s_pds.tabs[i].string           = (char *)tabNames[i];
		s_pds.tabs[i].style            = UI_RIGHT;
		s_pds.tabs[i].color            = s_tabColors[i];
	}
	/* highlight the default tab */
	PDS_UpdateTabColors();

	/* ---------- Achievements button (bottom of right panel) ---------- */
	s_pds.achieve_btn.generic.type     = MTYPE_PTEXT;
	s_pds.achieve_btn.generic.flags    = QMF_RIGHT_JUSTIFY | QMF_PULSEIFFOCUS;
	s_pds.achieve_btn.generic.id       = ID_PDS_ACHIEVE;
	s_pds.achieve_btn.generic.callback = PDS_MenuEvent;
	s_pds.achieve_btn.generic.x        = PDS_ACH_X;
	s_pds.achieve_btn.generic.y        = PDS_ACH_Y;
	s_pds.achieve_btn.string           = "ACHIEVEMENTS";
	s_pds.achieve_btn.color            = color_red;
	s_pds.achieve_btn.style            = UI_RIGHT | UI_SMALLFONT;

	/* ---------- reset button (bottom-right) ---------- */
	s_pds.reset_btn.generic.type     = MTYPE_BITMAP;
	s_pds.reset_btn.generic.name     = PDS_RESET0;
	s_pds.reset_btn.generic.flags    = QMF_LEFT_JUSTIFY | QMF_PULSEIFFOCUS;
	s_pds.reset_btn.generic.callback = PDS_MenuEvent;
	s_pds.reset_btn.generic.id       = ID_PDS_RESET;
	s_pds.reset_btn.generic.x        = PDS_RESET_X;
	s_pds.reset_btn.generic.y        = PDS_RESET_Y;
	s_pds.reset_btn.width            = 128;
	s_pds.reset_btn.height           = 64;
	s_pds.reset_btn.focuspic         = PDS_RESET1;

	/* ---------- back button ---------- */
	s_pds.back.generic.type     = MTYPE_BITMAP;
	s_pds.back.generic.name     = PDS_BACK0;
	s_pds.back.generic.flags    = QMF_LEFT_JUSTIFY | QMF_PULSEIFFOCUS;
	s_pds.back.generic.callback = PDS_MenuEvent;
	s_pds.back.generic.id       = ID_PDS_BACK;
	s_pds.back.generic.x        = PDS_BACK_X;
	s_pds.back.generic.y        = PDS_BACK_Y;
	s_pds.back.width            = 128;
	s_pds.back.height           = 64;
	s_pds.back.focuspic         = PDS_BACK1;

	/* ---------- add items ---------- */
	Menu_AddItem( &s_pds.menu, &s_pds.banner );
	Menu_AddItem( &s_pds.menu, &s_pds.framel );
	Menu_AddItem( &s_pds.menu, &s_pds.framer );
	for ( i = 0; i < PDS_NUMTABS; i++ )
		Menu_AddItem( &s_pds.menu, &s_pds.tabs[i] );
	Menu_AddItem( &s_pds.menu, &s_pds.achieve_btn );
	Menu_AddItem( &s_pds.menu, &s_pds.reset_btn );
	Menu_AddItem( &s_pds.menu, &s_pds.back );
}

void UI_PdStatsMenu( void ) {
	PDS_Init();
	UI_PushMenu( &s_pds.menu );
	Menu_SetCursorToItem( &s_pds.menu, &s_pds.tabs[0] );
}
