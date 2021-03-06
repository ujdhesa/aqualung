/*                                                     -*- linux-c -*-
    Copyright (C) 2004 Tom Szilagyi

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

    $Id$
*/

#ifndef AQUALUNG_STORE_FILE_H
#define AQUALUNG_STORE_FILE_H

#include <glib.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>


enum {
	BATCH_TAG_TITLE   = (1 << 0),
	BATCH_TAG_ARTIST  = (1 << 1),
	BATCH_TAG_ALBUM   = (1 << 2),
	BATCH_TAG_COMMENT = (1 << 3),
	BATCH_TAG_YEAR    = (1 << 4),
	BATCH_TAG_TRACKNO = (1 << 5)
};

int store_file_iter_is_track(GtkTreeIter * iter);
void store_file_iter_addlist_defmode(GtkTreeIter * ms_iter, GtkTreeIter * pl_iter, int new_tab);
void store_file_selection_changed(GtkTreeIter * iter, GtkTextBuffer * buffer, GtkLabel * statusbar);
gboolean store_file_event_cb(GdkEvent * event, GtkTreeIter * iter, GtkTreePath * path);
void store_file_load_icons(void);
void store_file_create_popup_menu(void);
void store_file_insert_progress_bar(GtkWidget * vbox);
void store_file_set_toolbar_sensitivity(GtkTreeIter * iter, GtkWidget * edit,
					GtkWidget * add, GtkWidget * remove);
void store_file_toolbar__edit_cb(gpointer data);
void store_file_toolbar__add_cb(gpointer data);
void store_file_toolbar__remove_cb(gpointer data);

gboolean store_file_remove_track(GtkTreeIter * iter);
gboolean store_file_remove_record(GtkTreeIter * iter);
gboolean store_file_remove_artist(GtkTreeIter * iter);
gboolean store_file_remove_store(GtkTreeIter * iter);

void store_file_progress_bar_hide(void);

void store__add_cb(gpointer data);

void store_file_load(char * file, char * sort);
void store_file_save(GtkTreeIter * iter_store);

void store__addlist_defmode(gpointer data);
void artist__addlist_defmode(gpointer data);
void record__addlist_defmode(gpointer data);
void track__addlist_cb(gpointer data);

typedef struct {
	int type;
	int dirty;
	int readonly;
	int use_relative_paths;
	char * file;
	char * comment;
} store_data_t;

typedef struct {
	char * comment;
} artist_data_t;

typedef struct {
	int year;
	char * comment;
} record_data_t;

typedef struct {
	char * file;
	float duration; /* length in seconds */
	float volume;   /* average volume in dBFS */
	float rva;      /* manual RVA in dB */
	int use_rva;    /* use manual RVA */
	char * comment;
	unsigned size;  /* file size in bytes */
} track_data_t;


#endif /* AQUALUNG_STORE_FILE_H */

// vim: shiftwidth=8:tabstop=8:softtabstop=8 :  
