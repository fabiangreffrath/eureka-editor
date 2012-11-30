//------------------------------------------------------------------------
//  Menus
//------------------------------------------------------------------------
//
//  Eureka DOOM Editor
//
//  Copyright (C) 2007-2012 Andrew Apted
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//------------------------------------------------------------------------

#include "main.h"
#include "ui_window.h"
#include "ui_about.h"
#include "editloop.h"
#include "e_basis.h"
#include "e_loadsave.h"
#include "e_nodes.h"
#include "e_cutpaste.h"
#include "e_path.h"
#include "levels.h"
#include "x_mirror.h"


//------------------------------------------------------------------------
//  FILE MENU
//------------------------------------------------------------------------

static void file_do_quit(Fl_Widget *w, void * data)
{
	CMD_Quit();
}

static void file_do_new(Fl_Widget *w, void * data)
{
	CMD_NewMap();
}

static void file_do_open(Fl_Widget *w, void * data)
{
	CMD_OpenMap();
}

static void file_do_save(Fl_Widget *w, void * data)
{
	CMD_SaveMap();
}

static void file_do_export(Fl_Widget *w, void * data)
{
	CMD_ExportMap();
}

static void file_do_build_nodes(Fl_Widget *w, void * data)
{
	CMD_BuildNodes();
}

static void file_do_play_map(Fl_Widget *w, void * data)
{
	CMD_PlayMap();
}


//------------------------------------------------------------------------
//  EDIT MENU
//------------------------------------------------------------------------

static void edit_do_undo(Fl_Widget *w, void * data)
{
	if (BA_Undo())
		edit.RedrawMap = 1;
	else
		fl_beep();
}

static void edit_do_redo(Fl_Widget *w, void * data)
{
	if (BA_Redo())
		edit.RedrawMap = 1;
	else
		fl_beep();
}

static void edit_do_cut(Fl_Widget *w, void * data)
{
	if (! CMD_Copy())
	{
		fl_beep();
		return;
	}

	CMD_Delete(false, false);
}

static void edit_do_copy(Fl_Widget *w, void * data)
{
	if (! CMD_Copy())
	{
		fl_beep();
		return;
	}
}

static void edit_do_paste(Fl_Widget *w, void * data)
{
	if (! CMD_Paste())
	{
		fl_beep();
		return;
	}
}

static void edit_do_delete(Fl_Widget *w, void * data)
{
	// NOTE: actually handled by Editor_Key(), so should not get here

	CMD_Delete(false, false);
}

static void edit_do_select_all(Fl_Widget *w, void * data)
{
	CMD_SelectAll();
}

static void edit_do_unselect_all(Fl_Widget *w, void * data)
{
	CMD_UnselectAll();
}

static void edit_do_invert_sel(Fl_Widget *w, void * data)
{
	CMD_InvertSelection();
}


//------------------------------------------------------------------------
//  VIEW MENU
//------------------------------------------------------------------------

static void view_do_zoom_in(Fl_Widget *w, void * data)
{
	CMD_Zoom(+1, grid.orig_x, grid.orig_y);
}

static void view_do_zoom_out(Fl_Widget *w, void * data)
{
	CMD_Zoom(-1, grid.orig_x, grid.orig_y);
}

static void view_do_whole_map(Fl_Widget *w, void * data)
{
	CMD_ZoomWholeMap();
}

static void view_do_camera_pos(Fl_Widget *w, void * data)
{
	CMD_GoToCamera();
}


//------------------------------------------------------------------------
//  SEARCH MENU
//------------------------------------------------------------------------

static void search_do_find(Fl_Widget *w, void * data)
{
	// TODO: CMD_FindObject()
	fl_beep();
}

static void search_do_jump(Fl_Widget *w, void * data)
{
	CMD_JumpToObject();
}

static void search_do_next(Fl_Widget *w, void * data)
{
	CMD_NextObject();
}

static void search_do_prev(Fl_Widget *w, void * data)
{
	CMD_PrevObject();
}


//------------------------------------------------------------------------
//  MISC MENU
//------------------------------------------------------------------------

static void misc_do_move(Fl_Widget *w, void * data)
{
	// TODO
	fl_beep();
}

static void misc_do_scale(Fl_Widget *w, void * data)
{
	// TODO
	fl_beep();
}

static void misc_do_rotate(Fl_Widget *w, void * data)
{
	// TODO
	fl_beep();
}

static void misc_do_mirror_horiz(Fl_Widget *w, void * data)
{
	CMD_MirrorObjects(false);
}

static void misc_do_mirror_vert(Fl_Widget *w, void * data)
{
	CMD_MirrorObjects(true);
}

#if 0
static void misc_do_other_op(Fl_Widget *w, void * data)
{
	// TODO
	fl_beep();
}
#endif


//------------------------------------------------------------------------
//  BROWSER MENU
//------------------------------------------------------------------------

static void browser_do_textures(Fl_Widget *w, void * data)
{
	CMD_SetBrowser('T');
}

static void browser_do_flats(Fl_Widget *w, void * data)
{
	CMD_SetBrowser('F');
}

static void browser_do_things(Fl_Widget *w, void * data)
{
	CMD_SetBrowser('O');
}

static void browser_do_lines(Fl_Widget *w, void * data)
{
	CMD_SetBrowser('L');
}

static void browser_do_sectors(Fl_Widget *w, void * data)
{
	CMD_SetBrowser('S');
}


#if 0
static void browser_go_wide(Fl_Widget *w, void * data)
{
	// TODO
}

static void browser_go_narrow(Fl_Widget *w, void * data)
{
	// TODO
}
#endif

static void browser_hide(Fl_Widget *w, void * data)
{
	CMD_SetBrowser(0);
}


//------------------------------------------------------------------------
//  HELP MENU
//------------------------------------------------------------------------

void help_do_about(Fl_Widget *w, void * data)
{
	DLG_AboutText();
}


//------------------------------------------------------------------------

#undef FCAL
#define FCAL  (Fl_Callback *)

static Fl_Menu_Item menu_items[] = 
{
	{ "&File", 0, 0, 0, FL_SUBMENU },

		{ "&New Map",   FL_COMMAND + 'n', FCAL file_do_new },
		{ "&Open Map",  FL_COMMAND + 'o', FCAL file_do_open },
		{ "&Save Map",  FL_COMMAND + 's', FCAL file_do_save },
		{ "&Export Map",  FL_COMMAND + 'e', FCAL file_do_export },

		{ "", 0, 0, 0, FL_MENU_DIVIDER|FL_MENU_INACTIVE },

//??   "Preferences...",          FL_F+5,  0,

#if 0
	{ "&DDF", 0, 0, 0, FL_SUBMENU },
		{ "Load DDF &File",    0, FCAL view_do_zoom_out },
		{ "Load DDF &WAD",    0, FCAL view_do_zoom_out },
		{ 0 },

		{ "&Open RTS File",   0, FCAL view_do_zoom_out },
		{ "&Import from WAD", 0, FCAL view_do_zoom_out },
		{ "&Save RTS File",   0, FCAL view_do_zoom_out },
		{ 0 },
#endif

		{ "&Build Nodes  ",   FL_COMMAND + 'b', FCAL file_do_build_nodes },
//TODO	{ "&Play Map",        FL_COMMAND + 'p', FCAL file_do_play_map },

		{ "&Quit",      FL_COMMAND + 'q', FCAL file_do_quit },
		{ 0 },

	{ "&Edit", 0, 0, 0, FL_SUBMENU },

		{ "&Undo",   FL_COMMAND + 'z',  FCAL edit_do_undo },
		{ "&Redo",   FL_COMMAND + 'y',  FCAL edit_do_redo },

		{ "", 0, 0, 0, FL_MENU_DIVIDER|FL_MENU_INACTIVE },

		{ "Cu&t",    FL_COMMAND + 'x',  FCAL edit_do_cut },
		{ "&Copy",   FL_COMMAND + 'c',  FCAL edit_do_copy },
		{ "&Paste",  FL_COMMAND + 'v',  FCAL edit_do_paste },
		{ "Delete",  FL_Delete,           FCAL edit_do_delete },

		{ "", 0, 0, 0, FL_MENU_DIVIDER|FL_MENU_INACTIVE },

		{ "Select &All",       FL_COMMAND + 'a', FCAL edit_do_select_all },
		{ "Unselect All",      FL_COMMAND + 'u', FCAL edit_do_unselect_all },
		{ "&Invert Selection", FL_COMMAND + 'i', FCAL edit_do_invert_sel },

		{ "", 0, 0, 0, FL_MENU_DIVIDER|FL_MENU_INACTIVE },

// TODO
#if 0
		{ "&Move",      0, FCAL misc_do_move },
		{ "Rotate",     0, FCAL misc_do_rotate },
		{ "&Scale",     0, FCAL misc_do_scale },
#endif

		{ "Mirror &Horizontally",  0, FCAL misc_do_mirror_horiz },
		{ "Mirror &Vertically",    0, FCAL misc_do_mirror_vert },

//??	{ "&Other Operation...",   0, FCAL misc_do_other_op },

//??   "~Exchange object numbers", 24,     0,
		{ 0 },

	{ "&View", 0, 0, 0, FL_SUBMENU },

		{ "Zoom &In",      0, FCAL view_do_zoom_in },
		{ "Zoom &Out",     0, FCAL view_do_zoom_out },
		{ "&Whole Map",    0, FCAL view_do_whole_map },
		{ "Go to &Camera", 0, FCAL view_do_camera_pos },

///		{ "&Show Object Nums", 0, FCAL search_do_jump },

		{ "", 0, 0, 0, FL_MENU_DIVIDER|FL_MENU_INACTIVE },

		{ "&Jump to Object",   0, FCAL search_do_jump },
		{ "&Next Object",      0, FCAL search_do_next },
		{ "&Prev Object",      0, FCAL search_do_prev },
		{ "&Find Object by Type", 0, FCAL search_do_find },
///		{ "&Show Object Nums", 0, FCAL search_do_jump },
		{ 0 },

	{ "&Browser", 0, 0, 0, FL_SUBMENU },
	
		{ "Textures",     0, FCAL browser_do_textures },
		{ "Flats",        0, FCAL browser_do_flats },
		{ "Thing Types",  0, FCAL browser_do_things },
		{ "Line Types",   0, FCAL browser_do_lines },
		{ "Sector Types", 0, FCAL browser_do_sectors },

		{ "", 0, 0, 0, FL_MENU_DIVIDER|FL_MENU_INACTIVE },
		{ "Hide",         0, FCAL browser_hide },
		{ 0 },

	{ "&Help", 0, 0, 0, FL_SUBMENU },
		{ "&About Eureka",   0,  FCAL help_do_about },
		{ 0 },

	{ 0 }
};


#ifdef __APPLE__
Fl_Sys_Menu_Bar * Menu_Create(int x, int y, int w, int h)
{
	Fl_Sys_Menu_Bar *bar = new Fl_Sys_Menu_Bar(x, y, w, h);
	bar->menu(menu_items);
	return bar;
}
#else
Fl_Menu_Bar * Menu_Create(int x, int y, int w, int h)
{
	Fl_Menu_Bar *bar = new Fl_Menu_Bar(x, y, w, h);
	bar->textsize(KF_fonth);
	bar->menu(menu_items);
	return bar;
}
#endif


#if 0  // UNUSED

static void Menu_ModeByName(Fl_Menu_Bar *bar, const char *name, int new_mode)
{
	for (int i = bar->size() - 1; i >= 0; i--)
	{
		const char *menu_text = bar->text(i);

		if (! menu_text)
			continue;

		if (menu_text[0] == '&')
			menu_text++;

		if (strcmp(menu_text, name) == 0)
		{
			bar->mode(i, new_mode);
			return;
		}
	}
}

void Menu_SetMode(char mode)
{
	Fl_Menu_Bar *bar = main_win->menu_bar;

	Menu_ModeByName(bar, "Things",   FL_SUBMENU | FL_MENU_INACTIVE);
	Menu_ModeByName(bar, "Linedefs", FL_SUBMENU | FL_MENU_INACTIVE);
	Menu_ModeByName(bar, "Sectors",  FL_SUBMENU | FL_MENU_INACTIVE);
	Menu_ModeByName(bar, "Vertices", FL_SUBMENU | FL_MENU_INACTIVE);
	Menu_ModeByName(bar, "RTS",      FL_SUBMENU | FL_MENU_INACTIVE);

	switch (mode)
	{
		case 't': Menu_ModeByName(bar, "Things",   FL_SUBMENU); break;
		case 'l': Menu_ModeByName(bar, "Linedefs", FL_SUBMENU); break;
		case 's': Menu_ModeByName(bar, "Sectors",  FL_SUBMENU); break;
		case 'v': Menu_ModeByName(bar, "Vertices", FL_SUBMENU); break;
		case 'r': Menu_ModeByName(bar, "RTS",      FL_SUBMENU); break;

		default: break;
	}

	bar->redraw();
}

#endif // UNUSED


//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
