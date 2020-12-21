//------------------------------------------------------------------------
//  LEVEL CUT 'N' PASTE
//------------------------------------------------------------------------
//
//  Eureka DOOM Editor
//
//  Copyright (C) 2009-2019 Andrew Apted
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

#include <map>

#include "e_basis.h"
#include "e_cutpaste.h"
#include "e_hover.h"
#include "e_linedef.h"
#include "e_main.h"
#include "e_sector.h"
#include "e_vertex.h"
#include "Errors.hpp"
#include "m_game.h"
#include "e_objects.h"
#include "r_grid.h"
#include "r_render.h"
#include "w_rawdef.h"

#include "ui_window.h"


#define INVALID_SECTOR  (-999999)


class clipboard_data_c
{
public:
	// the mode used when the objects were Copied
	ObjType mode;

	// NOTE:
	//
	// The references here normally refer to other objects in here
	// (as though this was a little self-contained map).  For example,
	// the LineDef::right field is an index into sides[].
	//
	// The exception is sector references in the SideDefs.  These
	// values refer to the real map when >= 0, and refer to local
	// sectors when < 0 (negate and add 1).
	//
	// That's because we generally want a copy'n'pasted piece of the
	// map (linedefs or sectors) to "fit in" with nearby sections of
	// the map.

	bool uses_real_sectors = false;

	std::vector<Thing *>   things;
	std::vector<Vertex *>  verts;
	std::vector<Sector *>  sectors;
	std::vector<SideDef *> sides;
	std::vector<LineDef *> lines;

public:
	clipboard_data_c(ObjType mode) : mode(mode)
	{
		if(mode == ObjType::linedefs || mode == ObjType::sectors)
			uses_real_sectors = true;
	}

	~clipboard_data_c()
	{
		unsigned int i;

		for (i = 0 ; i < things.size()   ; i++) delete things[i];
		for (i = 0 ; i < verts.size()    ; i++) delete verts[i];
		for (i = 0 ; i < sectors.size()  ; i++) delete sectors[i];
		for (i = 0 ; i < sides.size()    ; i++) delete sides[i];
		for (i = 0 ; i < lines.size()    ; i++) delete lines[i];
	}

	int TotalSize() const
	{
		size_t num = things.size() + verts.size() + sectors.size() + sides.size() + lines.size();

		return (int)num;
	}

	void Paste_BA_Message() const
	{
		size_t t = things.size();
		size_t v = verts.size();
		size_t s = sectors.size();
		size_t l = lines.size();

		if (s > 0)
		{
			const char *name = (s == 1) ? "sector" : "sectors";
			gDocument.basis.setMessage("pasted %zu %s", s, name);
		}
		else if (l > 0)
		{
			const char *name = (l == 1) ? "linedef" : "linedefs";
			gDocument.basis.setMessage("pasted %zu %s", l, name);
		}
		else if (t > 0)
		{
			const char *name = (t == 1) ? "thing" : "things";
			gDocument.basis.setMessage("pasted %zu %s", t, name);
		}
		else if (v > 0)
		{
			const char *name = (v == 1) ? "vertex" : "vertices";
			gDocument.basis.setMessage("pasted %zu %s", v, name);
		}
		else
		{
			gDocument.basis.setMessage("pasted something");
		}
	}

	void CentreOfThings(double *cx, double *cy)
	{
		*cx = *cy = 0;

		if (things.empty())
			return;

		double sum_x = 0;
		double sum_y = 0;

		for (unsigned int i = 0 ; i < things.size() ; i++)
		{
			sum_x += things[i]->x();
			sum_y += things[i]->y();
		}

		sum_x /= (double)things.size();
		sum_y /= (double)things.size();

		*cx = sum_x;
		*cy = sum_y;
	}

	void CentreOfVertices(double *cx, double *cy)
	{
		*cx = *cy = 0;

		if (verts.empty())
			return;

		double sum_x = 0;
		double sum_y = 0;

		for (unsigned int i = 0 ; i < verts.size() ; i++)
		{
			sum_x += verts[i]->x();
			sum_y += verts[i]->y();
		}

		sum_x /= (double)verts.size();
		sum_y /= (double)verts.size();

		*cx = sum_x;
		*cy = sum_y;
	}

	bool HasSectorRefs(int s1, int s2)
	{
		if (! uses_real_sectors)
			return false;

		for (unsigned int i = 0 ; i < sides.size() ; i++)
		{
			if (s1 <= sides[i]->sector && sides[i]->sector <= s2)
				return true;
		}

		return false;
	}

	void InsertRealSector(int snum)
	{
		if (! uses_real_sectors)
			return;

		for (unsigned int i = 0 ; i < sides.size() ; i++)
		{
			if (sides[i]->sector >= snum)
				sides[i]->sector++;
		}
	}

	void DeleteRealSector(int snum)
	{
		if (! uses_real_sectors)
			return;

		for (unsigned int i = 0 ; i < sides.size() ; i++)
		{
			if (sides[i]->sector == snum)
				sides[i]->sector = INVALID_SECTOR;
			else if (sides[i]->sector > snum)
				sides[i]->sector--;
		}
	}

	void RemoveSectorRefs()
	{
		if (! uses_real_sectors)
			return;

		for (unsigned int i = 0 ; i < sides.size() ; i++)
		{
			if (sides[i]->sector >= 0)
				sides[i]->sector = INVALID_SECTOR;
		}
	}
};


static clipboard_data_c * clip_board;

static bool clip_doing_paste;


void Clipboard_Clear()
{
	if (clip_board)
	{
		delete clip_board;
		clip_board = NULL;
	}
}


//
// this remove sidedefs which refer to local sectors, allowing the
// clipboard geometry to persist when changing maps.
//
void Clipboard_ClearLocals()
{
	if (clip_board)
		clip_board->RemoveSectorRefs();
}


static bool Clipboard_HasStuff()
{
	return clip_board ? true : false;
}


void Clipboard_NotifyBegin()
{ }

void Clipboard_NotifyEnd()
{ }


void Clipboard_NotifyInsert(ObjType type, int objnum)
{
	// this function notifies us that a new sector is about to be
	// inserted in the map (causing other sectors to be moved).

	if (type != ObjType::sectors)
		return;

	if (! clip_board)
		return;

	// paste operations should only insert new sectors at the end
	if (objnum < NumSectors)
	{
		SYS_ASSERT(! clip_doing_paste);
	}

#if 0  // OLD WAY
	if (clip_board->HasSectorRefs(objnum, NumSectors-1))
	{
		Clipboard_Clear();
	}
#else
	clip_board->InsertRealSector(objnum);
#endif
}


void Clipboard_NotifyDelete(ObjType type, int objnum)
{
	// this function notifies us that a sector is about to be deleted
	// (causing other sectors to be moved).

	if (type != ObjType::sectors)
		return;

	if (! clip_board)
		return;

	SYS_ASSERT(! clip_doing_paste);

	clip_board->DeleteRealSector(objnum);
}


void Clipboard_NotifyChange(ObjType type, int objnum, int field)
{
	// field changes never affect the clipboard
}


//----------------------------------------------------------------------
//  Texture Clipboard
//----------------------------------------------------------------------

namespace tex_clipboard
{
	SString tex;
	SString flat;

	int thing;
};


void Texboard_Clear()
{
	tex_clipboard::tex.clear();
	tex_clipboard::flat.clear();
	tex_clipboard::thing = 0;
}

int Texboard_GetTexNum()
{
	if (tex_clipboard::tex.empty())
		tex_clipboard::tex = default_wall_tex;

	return BA_InternaliseString(tex_clipboard::tex);
}

int Texboard_GetFlatNum()
{
	if (tex_clipboard::flat.empty())
		tex_clipboard::flat = default_floor_tex;

	return BA_InternaliseString(tex_clipboard::flat);
}

int Texboard_GetThing()
{
	if (tex_clipboard::thing == 0)
		return default_thing;

	return tex_clipboard::thing;
}

void Texboard_SetTex(const SString &new_tex)
{
	tex_clipboard::tex = new_tex;

	if (Features.mix_textures_flats)
		tex_clipboard::flat = new_tex;
}

void Texboard_SetFlat(const SString &new_flat)
{
	tex_clipboard::flat = new_flat;

	if (Features.mix_textures_flats)
		tex_clipboard::tex = new_flat;
}

void Texboard_SetThing(int new_id)
{
	tex_clipboard::thing = new_id;
}


//------------------------------------------------------------------------

static void CopyGroupOfObjects(selection_c *list)
{
	// this is used for LineDefs and Sectors, where we need to copy
	// groups of objects and create internal references between them.

	bool is_sectors = list->what_type() == ObjType::sectors;

	selection_c vert_sel(ObjType::vertices);
	selection_c side_sel(ObjType::sidedefs);
	selection_c line_sel(ObjType::linedefs);

	ConvertSelection(list, &line_sel);
	ConvertSelection(&line_sel, &vert_sel);

	// determine needed sidedefs
	for (sel_iter_c it(line_sel) ; !it.done() ; it.next())
	{
		LineDef *L = gDocument.linedefs[*it];

		if (L->right >= 0) side_sel.set(L->right);
		if (L->left  >= 0) side_sel.set(L->left);
	}


	// these hold the mapping from real index --> clipboard index
	std::map<int, int> vert_map;
	std::map<int, int> sector_map;
	std::map<int, int> side_map;


	for (sel_iter_c it(vert_sel) ; !it.done() ; it.next())
	{
		vert_map[*it] = (int)clip_board->verts.size();

		Vertex * SD = new Vertex;
		*SD = *gDocument.vertices[*it];
		clip_board->verts.push_back(SD);
	}

	if (is_sectors)
	{
		for (sel_iter_c it(list) ; !it.done() ; it.next())
		{
			sector_map[*it] = (int)clip_board->sectors.size();

			Sector * S = new Sector;
			*S = *gDocument.sectors[*it];
			clip_board->sectors.push_back(S);
		}
	}

	for (sel_iter_c it(side_sel) ; !it.done() ; it.next())
	{
		side_map[*it] = (int)clip_board->sides.size();

		SideDef * SD = new SideDef;
		*SD = *gDocument.sidedefs[*it];
		clip_board->sides.push_back(SD);

		// adjust sector references, if needed
		if (is_sectors && list->get(SD->sector))
		{
			SYS_ASSERT(sector_map.find(SD->sector) != sector_map.end());
			SD->sector = -1 - sector_map[SD->sector];
		}
	}

	for (sel_iter_c it(line_sel) ; !it.done() ; it.next())
	{
		LineDef * L = new LineDef;
		*L = *gDocument.linedefs[*it];
		clip_board->lines.push_back(L);

		// adjust vertex references
		SYS_ASSERT(vert_map.find(L->start) != vert_map.end());
		SYS_ASSERT(vert_map.find(L->end)   != vert_map.end());

		L->start = vert_map[L->start];
		L->end   = vert_map[L->end  ];

		// adjust sidedef references
		if (L->right >= 0)
		{
			SYS_ASSERT(side_map.find(L->right) != side_map.end());
			L->right = side_map[L->right];
		}

		if (L->left >= 0)
		{
			SYS_ASSERT(side_map.find(L->left) != side_map.end());
			L->left = side_map[L->left];
		}
	}

	// in sectors mode, copy things too
	if (is_sectors)
	{
		selection_c thing_sel(ObjType::things);

		ConvertSelection(list, &thing_sel);

		for (sel_iter_c it(thing_sel) ; !it.done() ; it.next())
		{
			Thing * T = new Thing;
			*T = *gDocument.things[*it];
			clip_board->things.push_back(T);
		}
	}
}


static bool Clipboard_DoCopy()
{
	SelectHighlight unselect = SelectionOrHighlight();
	if (unselect == SelectHighlight::empty)
		return false;

	// create storage for the copied objects
	if (clip_board)
		delete clip_board;

	bool result = true;

	clip_board = new clipboard_data_c(edit.mode);

	switch (edit.mode)
	{
		case ObjType::things:
			for (sel_iter_c it(edit.Selected) ; !it.done() ; it.next())
			{
				Thing * T = new Thing;
				*T = *gDocument.things[*it];
				clip_board->things.push_back(T);
			}
			break;

		case ObjType::vertices:
			for (sel_iter_c it(edit.Selected) ; !it.done() ; it.next())
			{
				Vertex * V = new Vertex;
				*V = *gDocument.vertices[*it];
				clip_board->verts.push_back(V);
			}
			break;

		case ObjType::linedefs:
		case ObjType::sectors:
			CopyGroupOfObjects(edit.Selected);
			break;

		default:
			result = false;
			break;
	}

	int total = edit.Selected->count_obj();
	if (total == 1)
		Status_Set("copied %s #%d", NameForObjectType(edit.Selected->what_type()), edit.Selected->find_first());
	else
		Status_Set("copied %d %s", total, NameForObjectType(edit.Selected->what_type(), true /* plural */));

	if (unselect == SelectHighlight::unselect)
		Selection_Clear(true /* nosave */);

	return result;
}


//------------------------------------------------------------------------

static void PasteGroupOfObjects(double pos_x, double pos_y)
{
	double cx, cy;
	clip_board->CentreOfVertices(&cx, &cy);

	// these hold the mapping from clipboard index --> real index
	std::map<int, int> vert_map;
	std::map<int, int> sector_map;
	std::map<int, int> side_map;

	unsigned int i;

	for (i = 0 ; i < clip_board->verts.size() ; i++)
	{
		int new_v = gDocument.basis.addNew(ObjType::vertices);
		Vertex * V = gDocument.vertices[new_v];

		vert_map[i] = new_v;

		*V = *clip_board->verts[i];

		V->SetRawX(V->x() + pos_x - cx);
		V->SetRawY(V->y() + pos_y - cy);
	}

	for (i = 0 ; i < clip_board->sectors.size() ; i++)
	{
		int new_s = gDocument.basis.addNew(ObjType::sectors);
		Sector * S = gDocument.sectors[new_s];

		sector_map[i] = new_s;

		*S = *clip_board->sectors[i];
	}

	for (i = 0 ; i < clip_board->sides.size() ; i++)
	{
		// handle invalidated sectors (as if sidedef had been deleted)
		if (clip_board->sides[i]->sector == INVALID_SECTOR)
		{
			side_map[i] = -1;
			continue;
		}

		int new_sd = gDocument.basis.addNew(ObjType::sidedefs);
		SideDef * SD = gDocument.sidedefs[new_sd];

		side_map[i] = new_sd;

		*SD = *clip_board->sides[i];

		if (SD->sector < 0)
		{
			int local = -1 - SD->sector;
			SYS_ASSERT(sector_map.find(local) != sector_map.end());
			SD->sector = sector_map[local];
		}
	}

	for (i = 0 ; i < clip_board->lines.size() ; i++)
	{
		int new_l = gDocument.basis.addNew(ObjType::linedefs);
		LineDef * L = gDocument.linedefs[new_l];

		*L = *clip_board->lines[i];

		// adjust vertex references
		SYS_ASSERT(vert_map.find(L->start) != vert_map.end());
		SYS_ASSERT(vert_map.find(L->end)   != vert_map.end());

		L->start = vert_map[L->start];
		L->end   = vert_map[L->end  ];

		// adjust sidedef references
		if (L->Right(gDocument))
		{
			SYS_ASSERT(side_map.find(L->right) != side_map.end());
			L->right = side_map[L->right];
		}

		if (L->Left(gDocument))
		{
			SYS_ASSERT(side_map.find(L->left) != side_map.end());
			L->left = side_map[L->left];
		}

		// flip linedef if necessary
		if (L->Left(gDocument) && ! L->Right(gDocument))
		{
			FlipLineDef(new_l);
		}

		// if the linedef lost a side, fix texturing
		if (L->OneSided() && is_null_tex(L->Right(gDocument)->MidTex()))
			LD_FixForLostSide(new_l);
	}

	for (i = 0 ; i < clip_board->things.size() ; i++)
	{
		int new_t = gDocument.basis.addNew(ObjType::things);
		Thing * T = gDocument.things[new_t];

		*T = *clip_board->things[i];

		T->SetRawX(T->x() + pos_x - cx);
		T->SetRawY(T->y() + pos_y - cy);
	}
}


static void ReselectGroup()
{
	// this assumes all the new objects are at the end of their
	// array (currently true, but not a guarantee of BA_New).

	if (edit.mode == ObjType::things)
	{
		if (clip_board->mode == ObjType::things ||
		    clip_board->mode == ObjType::sectors)
		{
			int count = (int)clip_board->things.size();

			Selection_Clear();

			edit.Selected->frob_range(NumThings - count, NumThings-1, BitOp::add);
		}
		return;
	}

	bool was_mappy = (clip_board->mode == ObjType::vertices ||
			    	  clip_board->mode == ObjType::linedefs ||
					  clip_board->mode == ObjType::sectors);

	bool is_mappy =  (edit.mode == ObjType::vertices ||
			    	  edit.mode == ObjType::linedefs ||
					  edit.mode == ObjType::sectors);

	if (! (was_mappy && is_mappy))
		return;

	selection_c new_sel(clip_board->mode);

	if (clip_board->mode == ObjType::vertices)
	{
		int count = (int)clip_board->verts.size();
		new_sel.frob_range(NumVertices - count, NumVertices-1, BitOp::add);
	}
	else if (clip_board->mode == ObjType::linedefs)
	{
		// Note: this doesn't behave as expected if the editing mode is
		// SECTORS, because the pasted lines do not completely surround
		// the sectors (non-pasted lines refer to them too).

		int count = (int)clip_board->lines.size();
		new_sel.frob_range(NumLineDefs - count, NumLineDefs-1, BitOp::add);
	}
	else
	{
		SYS_ASSERT(clip_board->mode == ObjType::sectors);

		int count = (int)clip_board->sectors.size();
		new_sel.frob_range(NumSectors - count, NumSectors-1, BitOp::add);
	}

	Selection_Clear();

	ConvertSelection(&new_sel, edit.Selected);
}


static bool Clipboard_DoPaste()
{
	bool reselect = true;  // CONFIG TODO

	unsigned int i;

	if (! Clipboard_HasStuff())
		return false;

	// figure out where to put stuff
	double pos_x = edit.map_x;
	double pos_y = edit.map_y;

	if (! edit.pointer_in_window)
	{
		pos_x = grid.orig_x;
		pos_y = grid.orig_y;
	}

	// honor the grid snapping setting
	pos_x = grid.SnapX(pos_x);
	pos_y = grid.SnapY(pos_y);

	gDocument.basis.begin();
	clip_board->Paste_BA_Message();

	clip_doing_paste = true;

	switch (clip_board->mode)
	{
		case ObjType::things:
		{
			double cx, cy;
			clip_board->CentreOfThings(&cx, &cy);

			for (unsigned int i = 0 ; i < clip_board->things.size() ; i++)
			{
				int new_t = gDocument.basis.addNew(ObjType::things);
				Thing * T = gDocument.things[new_t];

				*T = *clip_board->things[i];

				T->SetRawX(T->x() + pos_x - cx);
				T->SetRawY(T->y() + pos_y - cy);

				recent_things.insert_number(T->type);
			}
			break;
		}

		case ObjType::vertices:
		{
			double cx, cy;
			clip_board->CentreOfVertices(&cx, &cy);

			for (i = 0 ; i < clip_board->verts.size() ; i++)
			{
				int new_v = gDocument.basis.addNew(ObjType::vertices);
				Vertex * V = gDocument.vertices[new_v];

				*V = *clip_board->verts[i];

				V->SetRawX(V->x() + pos_x - cx);
				V->SetRawY(V->y() + pos_y - cy);
			}
			break;
		}

		case ObjType::linedefs:
		case ObjType::sectors:
		{
			PasteGroupOfObjects(pos_x, pos_y);
			break;
		}

		default:
			break;
	}

	clip_doing_paste = false;

	gDocument.basis.end();

	edit.error_mode = false;

	if (reselect)
		ReselectGroup();

	return true;
}


//------------------------------------------------------------------------

void CMD_CopyAndPaste()
{
	if (edit.Selected->empty() && edit.highlight.is_nil())
	{
		Beep("Nothing to copy and paste");
		return;
	}

	if (Clipboard_DoCopy())
	{
		Clipboard_DoPaste();
	}
}


void CMD_Clipboard_Cut()
{
	if (main_win->ClipboardOp(EditCommand::cut))
		return;

	if (edit.render3d && edit.mode != ObjType::things)
	{
		Render3D_CB_Cut();
		return;
	}

	if (! Clipboard_DoCopy())
	{
		Beep("Nothing to cut");
		return;
	}

	ExecuteCommand("Delete");
}


void CMD_Clipboard_Copy()
{
	if (main_win->ClipboardOp(EditCommand::copy))
		return;

	if (edit.render3d && edit.mode != ObjType::things)
	{
		Render3D_CB_Copy();
		return;
	}

	if (! Clipboard_DoCopy())
	{
		Beep("Nothing to copy");
		return;
	}
}


void CMD_Clipboard_Paste()
{
	if (main_win->ClipboardOp(EditCommand::paste))
		return;

	if (edit.render3d && edit.mode != ObjType::things)
	{
		Render3D_CB_Paste();
		return;
	}

	if (! Clipboard_DoPaste())
	{
		Beep("Clipboard is empty");
		return;
	}
}


//------------------------------------------------------------------------

void UnusedVertices(selection_c *lines, selection_c *result)
{
	SYS_ASSERT(lines->what_type() == ObjType::linedefs);

	ConvertSelection(lines, result);

	for (int n = 0 ; n < NumLineDefs ; n++)
	{
		// we are interested in the lines we are NOT deleting
		if (lines->get(n))
			continue;

		const LineDef *L = gDocument.linedefs[n];

		result->clear(L->start);
		result->clear(L->end);
	}
}


void UnusedSideDefs(selection_c *lines, selection_c *secs, selection_c *result)
{
	SYS_ASSERT(lines->what_type() == ObjType::linedefs);

	ConvertSelection(lines, result);

	for (int n = 0 ; n < NumLineDefs ; n++)
	{
		// we are interested in the lines we are NOT deleting
		if (lines->get(n))
			continue;

		const LineDef *L = gDocument.linedefs[n];

		if (L->Right(gDocument)) result->clear(L->right);
		if (L->Left(gDocument))  result->clear(L->left);
	}

	for (int i = 0 ; i < NumSideDefs ; i++)
	{
		const SideDef *SD = gDocument.sidedefs[i];

		if (secs && secs->get(SD->sector))
			result->set(i);
	}
}


void UnusedLineDefs(selection_c *sectors, selection_c *result)
{
	SYS_ASSERT(sectors->what_type() == ObjType::sectors);

	for (int n = 0 ; n < NumLineDefs ; n++)
	{
		const LineDef *L = gDocument.linedefs[n];

		// check if touches a to-be-deleted sector
		//    -1 : no side
		//     0 : deleted side
		//    +1 : kept side
		int right_m = (L->right < 0) ? -1 : sectors->get(L->Right(gDocument)->sector) ? 0 : 1;
		int  left_m = (L->left  < 0) ? -1 : sectors->get(L->Left(gDocument) ->sector) ? 0 : 1;

		if (MAX(right_m, left_m) == 0)
		{
			result->set(n);
		}
	}
}


void DuddedSectors(const selection_c &verts, const selection_c &lines, selection_c *result)
{
	SYS_ASSERT(verts.what_type() == ObjType::vertices);
	SYS_ASSERT(lines.what_type() == ObjType::linedefs);

	// collect all the sectors that touch a linedef being removed.

	bitvec_c del_lines(NumLineDefs);

	for (int n = 0 ; n < NumLineDefs ; n++)
	{
		const LineDef *linedef = gDocument.linedefs[n];

		if (lines.get(n) || verts.get(linedef->start) || verts.get(linedef->end))
		{
			del_lines.set(n);

			if (linedef->WhatSector(Side::left, gDocument) >= 0)
				result->set(linedef->WhatSector(Side::left, gDocument));
			if (linedef->WhatSector(Side::right, gDocument) >= 0)
				result->set(linedef->WhatSector(Side::right, gDocument));
		}
	}

	// visit all linedefs NOT being removed, and see if the sector(s)
	// on it will actually be OK after the delete.

	for (int n = 0 ; n < NumLineDefs ; n++)
	{
		const LineDef *linedef = gDocument.linedefs[n];

		if (lines.get(n) || verts.get(linedef->start) || verts.get(linedef->end))
			continue;

		for (Side what_side : kSides)
		{
			int sec_num = linedef->WhatSector(what_side, gDocument);

			if (sec_num < 0)
				continue;

			// skip sectors that are not potentials for removal,
			// and prevent the expensive tests below...
			if (! result->get(sec_num))
				continue;

			// check if the linedef opposite faces this sector (BUT
			// IGNORING any lines being deleted).  when found, we
			// know that this sector should be kept.

			Side opp_side;
			int opp_ld = OppositeLineDef(n, what_side, &opp_side, &del_lines);

			if (opp_ld < 0)
				continue;

			const LineDef *oppositeLinedef = gDocument.linedefs[opp_ld];

			if (oppositeLinedef->WhatSector(opp_side, gDocument) == sec_num)
				result->clear(sec_num);
		}
	}
}


static void FixupLineDefs(selection_c *lines, selection_c *sectors)
{
	for (sel_iter_c it(lines) ; !it.done() ; it.next())
	{
		const LineDef *L = gDocument.linedefs[*it];

		// the logic is ugly here mainly to handle flipping (in particular,
		// not to flip the line when _both_ sides are unlinked).

		bool do_right = L->Right(gDocument) ? sectors->get(L->Right(gDocument)->sector) : false;
		bool do_left  = L->Left(gDocument)  ? sectors->get(L->Left(gDocument) ->sector) : false;

		// line shouldn't be in list unless it touches the sector
		SYS_ASSERT(do_right || do_left);

		if (do_right && do_left)
		{
			LD_RemoveSideDef(*it, Side::right);
			LD_RemoveSideDef(*it, Side::left);
		}
		else if (do_right)
		{
			LD_RemoveSideDef(*it, Side::right);
			FlipLineDef(*it);
		}
		else // do_left
		{
			LD_RemoveSideDef(*it, Side::left);
		}
	}
}


static bool DeleteVertex_MergeLineDefs(int v_num)
{
	// returns true if OK, false if would create an overlapping linedef
	// [ meaning the vertex should be deleted normally ]

	// find the linedefs
	int ld1 = -1;
	int ld2 = -1;

	for (int n = 0 ; n < NumLineDefs ; n++)
	{
		const LineDef *L = gDocument.linedefs[n];

		if (L->start == v_num || L->end == v_num)
		{
			SYS_ASSERT(ld2 < 0);

			if (ld1 < 0)
				ld1 = n;
			else
				ld2 = n;
		}
	}

	SYS_ASSERT(ld1 >= 0);
	SYS_ASSERT(ld2 >= 0);

	LineDef *L1 = gDocument.linedefs[ld1];
	LineDef *L2 = gDocument.linedefs[ld2];

	// we merge L2 into L1, unless L1 is significantly shorter
	if (L1->CalcLength(gDocument) < L2->CalcLength(gDocument) * 0.7)
	{
		std::swap(ld1, ld2);
		std::swap(L1,  L2);
	}

	// determine the remaining vertices
	int v1 = (L1->start == v_num) ? L1->end : L1->start;
	int v2 = (L2->start == v_num) ? L2->end : L2->start;

	// ensure we don't create two directly overlapping linedefs
	if (LineDefAlreadyExists(v1, v2))
		return false;

	// see what sidedefs would become unused
	selection_c line_sel(ObjType::linedefs);
	selection_c side_sel(ObjType::sidedefs);

	line_sel.set(ld2);

	UnusedSideDefs(&line_sel, NULL /* sec_sel */, &side_sel);


	gDocument.basis.begin();
	gDocument.basis.setMessage("deleted vertex #%d\n", v_num);

	if (L1->start == v_num)
		gDocument.basis.changeLinedef(ld1, LineDef::F_START, v2);
	else
		gDocument.basis.changeLinedef(ld1, LineDef::F_END, v2);

	// NOT-DO: update X offsets on existing linedef

	gDocument.basis.del(ObjType::linedefs, ld2);
	gDocument.basis.del(ObjType::vertices, v_num);

	DeleteObjects(&side_sel);

	gDocument.basis.end();

	return true;
}


void DeleteObjects_WithUnused(selection_c *list, bool keep_things,
							  bool keep_verts, bool keep_lines)
{
	selection_c vert_sel(ObjType::vertices);
	selection_c side_sel(ObjType::sidedefs);
	selection_c line_sel(ObjType::linedefs);
	selection_c  sec_sel(ObjType::sectors);

	switch (list->what_type())
	{
		case ObjType::vertices:
			vert_sel.merge(*list);
			break;

		case ObjType::linedefs:
			line_sel.merge(*list);
			break;

		case ObjType::sectors:
			sec_sel.merge(*list);
			break;

		default: /* OBJ_THINGS or OBJ_SIDEDEFS */
			DeleteObjects(list);
			return;
	}

	if (list->what_type() == ObjType::vertices)
	{
		for (int n = 0 ; n < NumLineDefs ; n++)
		{
			const LineDef *L = gDocument.linedefs[n];

			if (list->get(L->start) || list->get(L->end))
				line_sel.set(n);
		}
	}

	if (!keep_lines && list->what_type() == ObjType::sectors)
	{
		UnusedLineDefs(&sec_sel, &line_sel);

		if (line_sel.notempty())
		{
			UnusedSideDefs(&line_sel, &sec_sel, &side_sel);

			if (!keep_verts)
				UnusedVertices(&line_sel, &vert_sel);
		}
	}

	if (!keep_verts && list->what_type() == ObjType::linedefs)
	{
		UnusedVertices(&line_sel, &vert_sel);
	}

	// try to detect sectors that become "dudded", where all the
	// remaining linedefs of the sector face into the void.
	if (list->what_type() == ObjType::vertices || list->what_type() == ObjType::linedefs)
	{
		DuddedSectors(vert_sel, line_sel, &sec_sel);
		UnusedSideDefs(&line_sel, &sec_sel, &side_sel);
	}

	// delete things from each deleted sector
	if (!keep_things && sec_sel.notempty())
	{
		selection_c thing_sel(ObjType::things);

		ConvertSelection(&sec_sel, &thing_sel);
		DeleteObjects(&thing_sel);
	}

	// perform linedef fixups here (when sectors get removed)
	if (sec_sel.notempty())
	{
		selection_c fixups(ObjType::linedefs);

		ConvertSelection(&sec_sel, &fixups);

		// skip lines which will get deleted
		fixups.unmerge(line_sel);

		FixupLineDefs(&fixups, &sec_sel);
	}

	// actually delete stuff, in the correct order
	DeleteObjects(&line_sel);
	DeleteObjects(&side_sel);
	DeleteObjects(&vert_sel);
	DeleteObjects( &sec_sel);
}


void CMD_Delete()
{
	if (main_win->ClipboardOp(EditCommand::del))
		return;

	SelectHighlight unselect = SelectionOrHighlight();
	if (unselect == SelectHighlight::empty)
	{
		Beep("Nothing to delete");
		return;
	}

	bool keep = Exec_HasFlag("/keep");

	// special case for a single vertex connected to two linedefs,
	// we delete the vertex but merge the two linedefs.
	if (edit.mode == ObjType::vertices && edit.Selected->count_obj() == 1)
	{
		int v_num = edit.Selected->find_first();
		SYS_ASSERT(v_num >= 0);

		if (Vertex_HowManyLineDefs(v_num) == 2)
		{
			if (DeleteVertex_MergeLineDefs(v_num))
				goto success;
		}

		// delete vertex normally
	}

	gDocument.basis.begin();
	gDocument.basis.setMessageForSelection("deleted", *edit.Selected);

	DeleteObjects_WithUnused(edit.Selected, keep, false /* keep_verts */, keep);

	gDocument.basis.end();

success:
	Editor_ClearAction();

	// always clear the selection (deleting objects invalidates it)
	Selection_Clear();

	edit.highlight.clear();
	edit.split_line.clear();

	RedrawMap();
}


//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
