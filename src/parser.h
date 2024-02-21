/*
 * Gromit-MPX -- a program for painting on the screen
 *
 * Gromit Copyright (C) 2000 Simon Budig <Simon.Budig@unix-ag.org>
 *
 * Gromit-MPX Copyright (C) 2009,2010 Christian Beier <dontmind@freeshell.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#ifndef PARSER_H
#define PARSER_H

#include "main.h"


typedef struct
{
  GromitPaintType type;
  guint           width;
  gfloat          arrowsize;
  guint           minwidth;
  guint           maxwidth;
  GdkRGBA         *paint_color;
} GromitStyleDef;

extern gpointer HOTKEY_SYMBOL_VALUE;
extern gpointer UNDOKEY_SYMBOL_VALUE;

void scanner_init(GScanner* scanner);
gchar* parse_name (GScanner *scanner);
gboolean parse_style(GScanner *scanner, GromitStyleDef *style);
gboolean parse_tool(GromitData *data, GScanner *scanner, GromitStyleDef *style);


#endif // PARSER_H
