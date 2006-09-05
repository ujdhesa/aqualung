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

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <sys/stat.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#ifdef HAVE_ID3
#include <id3tag.h>
#endif /* HAVE_ID3 */

#ifdef HAVE_FLAC
#include <FLAC/format.h>
#include <FLAC/metadata.h>
#endif /* HAVE_FLAC */

#ifdef HAVE_MOD_INFO
#include <libmodplug/modplug.h>
#endif /* HAVE_MOD_INFO */

#ifdef HAVE_MOD
#include "decoder/dec_mod.h"
#endif /* HAVE_MOD */

#ifdef HAVE_TAGLIB
#include <id3v1tag.h>
#include <id3v2tag.h>
#include <apetag.h>
#include <xiphcomment.h>
#include <mpegfile.h>
#include <mpcfile.h>
#include <flacfile.h>
#include <vorbisfile.h>
#include <tlist.h>
#include <tbytevector.h>
#endif /* HAVE_TAGLIB */


#include "common.h"
#include "core.h"
#include "cover.h"
#include "decoder/file_decoder.h"
#include "music_browser.h"
#include "gui_main.h"
#include "options.h"
#include "trashlist.h"
#include "i18n.h"
#include "meta_decoder.h"
#include "file_info.h"


/* import destination codes */
#define IMPORT_DEST_ARTIST   1
#define IMPORT_DEST_RECORD   2
#define IMPORT_DEST_TITLE    3
#define IMPORT_DEST_NUMBER   4
#define IMPORT_DEST_COMMENT  5
#define IMPORT_DEST_RVA      6


extern options_t options;

typedef struct _import_data_t {
        GtkTreeModel * model;
        GtkTreeIter track_iter;
        int dest_type; /* one of the above codes */
        char str[MAXLEN];
        float fval;
} import_data_t;


extern GtkWidget * main_window;
extern GtkTreeStore * music_store;
extern GtkTreeSelection * music_select;
extern GtkWidget * music_tree;

GtkWidget * info_window = NULL;
trashlist_t * fileinfo_trash = NULL;

gint page = 0;          /* current notebook page */
gint n_pages = 0;       /* number of notebook pages */
GtkWidget * nb;         /* notebook widget */

#ifdef HAVE_MOD_INFO
file_decoder_t * md_fdec = NULL;
GtkWidget * smp_instr_list;
GtkListStore * smp_instr_list_store = NULL;
void fill_module_info_page(mod_info *mdi, GtkWidget *vbox, char *file);
#endif /* HAVE_MOD_INFO */

import_data_t *
import_data_new(void) {

	import_data_t * data;

	if ((data = (import_data_t *)calloc(1, sizeof(import_data_t))) == NULL) {
		fprintf(stderr, "error: import_data_new(): calloc error\n");
		return NULL;
	}
	return data;
}


static gint
dismiss(GtkWidget * widget, gpointer data) {

#ifdef HAVE_MOD_INFO
        if (md_fdec) {
                file_decoder_close(md_fdec);
                file_decoder_delete(md_fdec);
                md_fdec = NULL;
        }
#endif /* HAVE_MOD_INFO */

        gtk_widget_destroy(info_window);
	info_window = NULL;
	trashlist_free(fileinfo_trash);
	fileinfo_trash = NULL;
	return TRUE;
}


int
info_window_close(GtkWidget * widget, gpointer * data) {

	info_window = NULL;
	trashlist_free(fileinfo_trash);
	fileinfo_trash = NULL;
	return 0;
}


static void
import_button_pressed(GtkWidget * widget, gpointer gptr_data) {

	import_data_t * data = (import_data_t *)gptr_data;
	GtkTreeIter record_iter;
	GtkTreeIter artist_iter;
	GtkTreePath * path;
	char tmp[MAXLEN];
	char * ptmp;
	float ftmp;

	switch (data->dest_type) {
	case IMPORT_DEST_TITLE:
		gtk_tree_store_set(music_store, &(data->track_iter), 0, data->str, -1);
		music_store_mark_changed(&(data->track_iter));
		break;
	case IMPORT_DEST_RECORD:
		gtk_tree_model_iter_parent(data->model, &record_iter, &(data->track_iter));
		gtk_tree_store_set(music_store, &record_iter, 0, data->str, -1);
		music_store_mark_changed(&(data->track_iter));
		break;
	case IMPORT_DEST_ARTIST:
		gtk_tree_model_iter_parent(data->model, &record_iter, &(data->track_iter));
		gtk_tree_model_iter_parent(data->model, &artist_iter, &record_iter);
		gtk_tree_store_set(music_store, &artist_iter, 0, data->str, -1);
		gtk_tree_store_set(music_store, &artist_iter, 1, data->str, -1);
		path = gtk_tree_model_get_path(data->model, &(data->track_iter));
		gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(music_tree), path, NULL, TRUE, 0.5f, 0.0f);
		music_store_mark_changed(&(data->track_iter));
		break;
	case IMPORT_DEST_NUMBER:
		if (data->str[0] != '0') {
			tmp[0] = '0';
			tmp[1] = '\0';
		} else {
			tmp[0] = '\0';
		}
		strncat(tmp, data->str, MAXLEN-1);
		gtk_tree_store_set(music_store, &(data->track_iter), 1, tmp, -1);
		music_store_mark_changed(&(data->track_iter));
		break;
	case IMPORT_DEST_COMMENT:
		gtk_tree_model_get(data->model, &(data->track_iter), 3, &ptmp, -1);
		tmp[0] = '\0';
		if (ptmp != NULL) {
			strncat(tmp, ptmp, MAXLEN-1);
		}
		if ((tmp[strlen(tmp)-1] != '\n') && (tmp[0] != '\0')) {
			strncat(tmp, "\n", MAXLEN-1);
		}
		strncat(tmp, data->str, MAXLEN-1);
		gtk_tree_store_set(music_store, &(data->track_iter), 3, tmp, -1);
		tree_selection_changed_cb(music_select, NULL);
		music_store_mark_changed(&(data->track_iter));
		break;
	case IMPORT_DEST_RVA:
		ftmp = 1.0f;
		gtk_tree_store_set(music_store, &(data->track_iter), 6, data->fval, -1);
		gtk_tree_store_set(music_store, &(data->track_iter), 7, ftmp, -1);
		music_store_mark_changed(&(data->track_iter));
		break;
	}
}


void
append_table(GtkWidget * table, int * cnt, char * field, char * value,
	     char * importbtn_text, import_data_t * data) {

	GtkWidget * hbox;
	GtkWidget * entry;
	GtkWidget * label;

	GtkWidget * button;

	gtk_table_resize(GTK_TABLE(table), *cnt + 1, 2);

	hbox = gtk_hbox_new(FALSE, 0);
	label = gtk_label_new(field);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, *cnt, *cnt+1, GTK_FILL, GTK_FILL, 5, 3);

	entry = gtk_entry_new();
        GTK_WIDGET_UNSET_FLAGS(entry, GTK_CAN_FOCUS);
	gtk_entry_set_text(GTK_ENTRY(entry), value);
	gtk_editable_set_editable(GTK_EDITABLE(entry), FALSE);

	if (importbtn_text == NULL) {
		gtk_table_attach(GTK_TABLE(table), entry, 1, 3, *cnt, *cnt+1,
				 (GtkAttachOptions)(GTK_EXPAND | GTK_FILL), GTK_FILL, 5, 3);
	} else {
		button = gtk_button_new_with_label(importbtn_text);
		g_signal_connect(G_OBJECT(button), "clicked",
				 G_CALLBACK(import_button_pressed), (gpointer)data);
		gtk_table_attach(GTK_TABLE(table), entry, 1, 2, *cnt, *cnt+1,
				 (GtkAttachOptions)(GTK_EXPAND | GTK_FILL), GTK_FILL, 5, 3);
		gtk_table_attach(GTK_TABLE(table), button, 2, 3, *cnt, *cnt+1,
				 GTK_FILL, GTK_FILL, 5, 3);
	}
	
	(*cnt)++;
}


gint
info_window_key_pressed(GtkWidget * widget, GdkEventKey * kevent) {

	switch (kevent->keyval) {

	case GDK_q:
	case GDK_Q:
	case GDK_Escape:
		dismiss(NULL, NULL);
		return TRUE;
		break;
		
	case GDK_Return:
		page = (page+1) % n_pages;
		gtk_notebook_set_current_page(GTK_NOTEBOOK(nb), page);
		break;
		
	}

	return FALSE;
}


#ifdef HAVE_TAGLIB
/* this is a primitive lister for me to experiment with */
void
print_tags(TagLib::ID3v1::Tag * id3v1_tag,
	   TagLib::ID3v2::Tag * id3v2_tag,
	   TagLib::APE::Tag * ape_tag,
	   TagLib::Ogg::XiphComment * oxc) {

	if (id3v1_tag && !id3v1_tag->isEmpty()) {
		std::cout << "-- ID3v1Tag --" << std::endl;
		std::cout << "title   - \"" << id3v1_tag->title()   << "\"" << std::endl;
		std::cout << "artist  - \"" << id3v1_tag->artist()  << "\"" << std::endl;
		std::cout << "album   - \"" << id3v1_tag->album()   << "\"" << std::endl;
		std::cout << "year    - \"" << id3v1_tag->year()    << "\"" << std::endl;
		std::cout << "comment - \"" << id3v1_tag->comment() << "\"" << std::endl;
		std::cout << "track   - \"" << id3v1_tag->track()   << "\"" << std::endl;
		std::cout << "genre   - \"" << id3v1_tag->genre()   << "\"" << std::endl;
	}	
	if (id3v2_tag && !id3v2_tag->isEmpty()) {
		std::cout << "-- ID3v2Tag --" << std::endl;
		std::cout << "title   - \"" << id3v2_tag->title()   << "\"" << std::endl;
		std::cout << "artist  - \"" << id3v2_tag->artist()  << "\"" << std::endl;
		std::cout << "album   - \"" << id3v2_tag->album()   << "\"" << std::endl;
		std::cout << "year    - \"" << id3v2_tag->year()    << "\"" << std::endl;
		std::cout << "comment - \"" << id3v2_tag->comment() << "\"" << std::endl;
		std::cout << "track   - \"" << id3v2_tag->track()   << "\"" << std::endl;
		std::cout << "genre   - \"" << id3v2_tag->genre()   << "\"" << std::endl;

		std::cout << " *** ID3v2 Frame list ***" << std::endl;
		TagLib::ID3v2::FrameList l = id3v2_tag->frameList();
		std::list<TagLib::ID3v2::Frame*>::iterator i;

		for (i = l.begin(); i != l.end(); ++i) {
			TagLib::ID3v2::Frame * frame = *i;
			std::cout << frame->frameID().data() << " | " << frame->toString() << std::endl;
		}
	}
	if (ape_tag && !ape_tag->isEmpty()) {
		std::cout << "-- APE Tag --" << std::endl;
		std::cout << "title   - \"" << ape_tag->title()   << "\"" << std::endl;
		std::cout << "artist  - \"" << ape_tag->artist()  << "\"" << std::endl;
		std::cout << "album   - \"" << ape_tag->album()   << "\"" << std::endl;
		std::cout << "year    - \"" << ape_tag->year()    << "\"" << std::endl;
		std::cout << "comment - \"" << ape_tag->comment() << "\"" << std::endl;
		std::cout << "track   - \"" << ape_tag->track()   << "\"" << std::endl;
		std::cout << "genre   - \"" << ape_tag->genre()   << "\"" << std::endl;
	}
	if (oxc && !oxc->isEmpty()) {
		std::cout << "-- Ogg Xiph Comment --" << std::endl;
		std::cout << "title   - \"" << oxc->title()   << "\"" << std::endl;
		std::cout << "artist  - \"" << oxc->artist()  << "\"" << std::endl;
		std::cout << "album   - \"" << oxc->album()   << "\"" << std::endl;
		std::cout << "year    - \"" << oxc->year()    << "\"" << std::endl;
		std::cout << "comment - \"" << oxc->comment() << "\"" << std::endl;
		std::cout << "track   - \"" << oxc->track()   << "\"" << std::endl;
		std::cout << "genre   - \"" << oxc->genre()   << "\"" << std::endl;

		std::cout << "Number of fields: " << oxc->fieldCount() << std::endl;

		//std::map m = oxc->fieldListMap();
		//std::map<TagLib>
	}
}
#endif /* HAVE_TAGLIB */


#ifdef HAVE_FLAC
void
meta_print_flac(metadata * meta) {

	TagLib::FLAC::File * taglib_flac_file =
		reinterpret_cast<TagLib::FLAC::File *>(meta->taglib_file);
	print_tags(taglib_flac_file->ID3v1Tag(), taglib_flac_file->ID3v2Tag(),
		   NULL, taglib_flac_file->xiphComment());
}
#endif /* HAVE_FLAC */


#ifdef HAVE_OGG_VORBIS
void
meta_print_oggv(metadata * meta) {

	TagLib::Ogg::Vorbis::File * taglib_oggv_file =
		reinterpret_cast<TagLib::Ogg::Vorbis::File *>(meta->taglib_file);
	print_tags(NULL, NULL, NULL, taglib_oggv_file->tag());
}
#endif /* HAVE_OGG_VORBIS */


#ifdef HAVE_MPEG
void
meta_print_mpeg(metadata * meta) {

	TagLib::MPEG::File * taglib_mpeg_file =
		reinterpret_cast<TagLib::MPEG::File *>(meta->taglib_file);
	print_tags(taglib_mpeg_file->ID3v1Tag(), taglib_mpeg_file->ID3v2Tag(),
		   taglib_mpeg_file->APETag(), NULL);
}
#endif /* HAVE_MPEG */


#ifdef HAVE_MPC
void
meta_print_mpc(metadata * meta) {

	TagLib::MPC::File * taglib_mpc_file =
		reinterpret_cast<TagLib::MPC::File *>(meta->taglib_file);
	print_tags(taglib_mpc_file->ID3v1Tag(), NULL,
		   taglib_mpc_file->APETag(), NULL);
}
#endif /* HAVE_MPC */


void
show_file_info(char * name, char * file, int is_called_from_browser,
	       GtkTreeModel * model, GtkTreeIter track_iter) {

        char str[MAXLEN];
	gchar *file_display;

	GtkWidget * vbox;
	GtkWidget * hbox_t;
	GtkWidget * table;
	GtkWidget * hbox_name;
	GtkWidget * label_name;
	GtkWidget * entry_name;
	GtkWidget * hbox_path;
	GtkWidget * label_path;
	GtkWidget * entry_path;
	GtkWidget * hbuttonbox;
	GtkWidget * dismiss_btn;
	GtkWidget * cover_image_area;

	GtkWidget * vbox_file;
	GtkWidget * label_file;
	GtkWidget * table_file;
	GtkWidget * hbox;
	GtkWidget * label;
	GtkWidget * entry;

#ifdef HAVE_ID3
	GtkWidget * vbox_id3v2;
	GtkWidget * label_id3v2;
	GtkWidget * table_id3v2;
#endif /* HAVE_ID3 */

#ifdef HAVE_OGG_VORBIS
	GtkWidget * vbox_vorbis;
	GtkWidget * label_vorbis;
	GtkWidget * table_vorbis;
#endif /* HAVE_OGG_VORBIS */

#ifdef HAVE_FLAC
	GtkWidget * vbox_flac;
	GtkWidget * label_flac;
	GtkWidget * table_flac;
#endif /* HAVE_FLAC */

#ifdef HAVE_MOD_INFO
        mod_info * mdi;
	GtkWidget * vbox_mod;
	GtkWidget * label_mod;
#endif /* HAVE_MOD_INFO */

	metadata * meta = meta_new();
#ifdef HAVE_ID3
	id3_tag_data * id3;
#endif /* HAVE_ID3 */
	oggv_comment * oggv;
	int cnt;


	if (info_window != NULL) {
		gtk_widget_destroy(info_window);
		info_window = NULL;
	}

	if (fileinfo_trash != NULL) {
		trashlist_free(fileinfo_trash);
		fileinfo_trash = NULL;
	}
	fileinfo_trash = trashlist_new();

	if (!meta_read(meta, file)) {
		fprintf(stderr, "show_file_info(): meta_read() returned an error\n");
		return;
	}

#ifdef HAVE_TAGLIB
	switch (meta->format_major) {
#ifdef HAVE_FLAC
	case FORMAT_FLAC:
		meta_print_flac(meta);
		break;
#endif /* HAVE_FLAC */
#ifdef HAVE_OGG_VORBIS
	case FORMAT_VORBIS:
		meta_print_oggv(meta);
		break;
#endif /* HAVE_OGG_VORBIS */
#ifdef HAVE_MPEG
	case FORMAT_MAD:
		meta_print_mpeg(meta);
		break;
#endif /* HAVE_MPEG */
#ifdef HAVE_MPC
	case FORMAT_MPC:
		meta_print_mpc(meta);
		break;
#endif /* HAVE_MPC */
	}
#endif /* HAVE_TAGLIB */


	info_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
        gtk_window_set_title(GTK_WINDOW(info_window), _("File info"));
	gtk_window_set_transient_for(GTK_WINDOW(info_window), GTK_WINDOW(main_window));
	gtk_window_set_position(GTK_WINDOW(info_window), GTK_WIN_POS_CENTER_ON_PARENT);
	gtk_widget_set_size_request(GTK_WIDGET(info_window), 500, -1);
	g_signal_connect(G_OBJECT(info_window), "delete_event",
			 G_CALLBACK(info_window_close), NULL);
        g_signal_connect(G_OBJECT(info_window), "key_press_event",
			 G_CALLBACK(info_window_key_pressed), NULL);
	gtk_container_set_border_width(GTK_CONTAINER(info_window), 5);

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(info_window), vbox);

	hbox_t = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox_t, FALSE, FALSE, 0);

	table = gtk_table_new(2, 2, FALSE);
	gtk_box_pack_start(GTK_BOX(hbox_t), table, TRUE, TRUE, 4);

        hbox_name = gtk_hbox_new(FALSE, 0);
	label_name = gtk_label_new(_("Track:"));
	gtk_box_pack_start(GTK_BOX(hbox_name), label_name, FALSE, FALSE, 0);
	gtk_table_attach(GTK_TABLE(table), hbox_name, 0, 1, 0, 1,
			 GTK_FILL, GTK_FILL, 5, 2);

	entry_name = gtk_entry_new();
        GTK_WIDGET_UNSET_FLAGS(entry_name, GTK_CAN_FOCUS);
	gtk_entry_set_text(GTK_ENTRY(entry_name), name);
	gtk_editable_set_editable(GTK_EDITABLE(entry_name), FALSE);
	gtk_table_attach(GTK_TABLE(table), entry_name, 1, 2, 0, 1,
			 (GtkAttachOptions)(GTK_EXPAND | GTK_FILL), GTK_FILL, 5, 2);

	hbox_path = gtk_hbox_new(FALSE, 0);
	label_path = gtk_label_new(_("File:"));
	gtk_box_pack_start(GTK_BOX(hbox_path), label_path, FALSE, FALSE, 0);
	gtk_table_attach(GTK_TABLE(table), hbox_path, 0, 1, 1, 2,
			 GTK_FILL, GTK_FILL, 5, 2);

	file_display=g_filename_display_name(file);
	entry_path = gtk_entry_new();
        GTK_WIDGET_UNSET_FLAGS(entry_path, GTK_CAN_FOCUS);
	gtk_entry_set_text(GTK_ENTRY(entry_path), file_display);
	g_free(file_display);
	gtk_editable_set_editable(GTK_EDITABLE(entry_path), FALSE);
	gtk_table_attach(GTK_TABLE(table), entry_path, 1, 2, 1, 2,
			 (GtkAttachOptions)(GTK_EXPAND | GTK_FILL), GTK_FILL, 5, 2);

        cover_image_area = gtk_image_new();
	gtk_box_pack_start(GTK_BOX(hbox_t), cover_image_area, FALSE, FALSE, 0);

        display_cover(cover_image_area, 48, 48, file, FALSE);

	nb = gtk_notebook_new();
	gtk_box_pack_start(GTK_BOX(vbox), nb, TRUE, TRUE, 10);

	/* Audio data notebook page */

	vbox_file = gtk_vbox_new(FALSE, 4);
	table_file = gtk_table_new(6, 2, FALSE);
	gtk_box_pack_start(GTK_BOX(vbox_file), table_file, TRUE, TRUE, 10);
	label_file = gtk_label_new(_("Audio data"));
	gtk_notebook_append_page(GTK_NOTEBOOK(nb), vbox_file, label_file);

	hbox = gtk_hbox_new(FALSE, 0);
	label = gtk_label_new(_("Format:"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_table_attach(GTK_TABLE(table_file), hbox, 0, 1, 0, 1, GTK_FILL, GTK_FILL, 5, 3);
	entry = gtk_entry_new();
        GTK_WIDGET_UNSET_FLAGS(entry, GTK_CAN_FOCUS);
	assembly_format_label(str, meta->format_major, meta->format_minor);
	gtk_entry_set_text(GTK_ENTRY(entry), str);
	gtk_editable_set_editable(GTK_EDITABLE(entry), FALSE);
	gtk_table_attach(GTK_TABLE(table_file), entry, 1, 2, 0, 1,
			 (GtkAttachOptions)(GTK_EXPAND | GTK_FILL), GTK_FILL, 5, 3);

	hbox = gtk_hbox_new(FALSE, 0);
	label = gtk_label_new(_("Length:"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_table_attach(GTK_TABLE(table_file), hbox, 0, 1, 1, 2, GTK_FILL, GTK_FILL, 5, 3);
	entry = gtk_entry_new();
        GTK_WIDGET_UNSET_FLAGS(entry, GTK_CAN_FOCUS);
	sample2time(meta->sample_rate, meta->total_samples, str, 0);
	gtk_entry_set_text(GTK_ENTRY(entry), str);
	gtk_editable_set_editable(GTK_EDITABLE(entry), FALSE);
	gtk_table_attach(GTK_TABLE(table_file), entry, 1, 2, 1, 2,
			 (GtkAttachOptions)(GTK_EXPAND | GTK_FILL), GTK_FILL, 5, 3);

	hbox = gtk_hbox_new(FALSE, 0);
	label = gtk_label_new(_("Samplerate:"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_table_attach(GTK_TABLE(table_file), hbox, 0, 1, 2, 3, GTK_FILL, GTK_FILL, 5, 3);
	entry = gtk_entry_new();
        GTK_WIDGET_UNSET_FLAGS(entry, GTK_CAN_FOCUS);
	sprintf(str, _("%ld Hz"), meta->sample_rate);
	gtk_entry_set_text(GTK_ENTRY(entry), str);
	gtk_editable_set_editable(GTK_EDITABLE(entry), FALSE);
	gtk_table_attach(GTK_TABLE(table_file), entry, 1, 2, 2, 3,
			 (GtkAttachOptions)(GTK_EXPAND | GTK_FILL), GTK_FILL, 5, 3);

	hbox = gtk_hbox_new(FALSE, 0);
	label = gtk_label_new(_("Channel count:"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_table_attach(GTK_TABLE(table_file), hbox, 0, 1, 3, 4, GTK_FILL, GTK_FILL, 5, 3);
	entry = gtk_entry_new();
        GTK_WIDGET_UNSET_FLAGS(entry, GTK_CAN_FOCUS);
	if (meta->is_mono)
		strcpy(str, _("MONO"));
	else
		strcpy(str, _("STEREO"));
	gtk_entry_set_text(GTK_ENTRY(entry), str);
	gtk_editable_set_editable(GTK_EDITABLE(entry), FALSE);
	gtk_table_attach(GTK_TABLE(table_file), entry, 1, 2, 3, 4,
			 (GtkAttachOptions)(GTK_EXPAND | GTK_FILL), GTK_FILL, 5, 3);

	hbox = gtk_hbox_new(FALSE, 0);
	label = gtk_label_new(_("Bandwidth:"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_table_attach(GTK_TABLE(table_file), hbox, 0, 1, 4, 5, GTK_FILL, GTK_FILL, 5, 3);
	entry = gtk_entry_new();
        GTK_WIDGET_UNSET_FLAGS(entry, GTK_CAN_FOCUS);
	sprintf(str, _("%.1f kbit/s"), meta->bps / 1000.0f);
	gtk_entry_set_text(GTK_ENTRY(entry), str);
	gtk_editable_set_editable(GTK_EDITABLE(entry), FALSE);
	gtk_table_attach(GTK_TABLE(table_file), entry, 1, 2, 4, 5,
			 (GtkAttachOptions)(GTK_EXPAND | GTK_FILL), GTK_FILL, 5, 3);

	hbox = gtk_hbox_new(FALSE, 0);
	label = gtk_label_new(_("Total samples:"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_table_attach(GTK_TABLE(table_file), hbox, 0, 1, 5, 6, GTK_FILL, GTK_FILL, 5, 3);
	entry = gtk_entry_new();
        GTK_WIDGET_UNSET_FLAGS(entry, GTK_CAN_FOCUS);
	sprintf(str, "%lld", meta->total_samples);
	gtk_entry_set_text(GTK_ENTRY(entry), str);
	gtk_editable_set_editable(GTK_EDITABLE(entry), FALSE);
	gtk_table_attach(GTK_TABLE(table_file), entry, 1, 2, 5, 6,
			 (GtkAttachOptions)(GTK_EXPAND | GTK_FILL), GTK_FILL, 5, 3);


#ifdef HAVE_ID3
	cnt = 0;
	id3 = meta->id3_root;
	if (id3->next != NULL) {
		id3 = id3->next;
		
		vbox_id3v2 = gtk_vbox_new(FALSE, 4);
		table_id3v2 = gtk_table_new(0, 3, FALSE);
		gtk_box_pack_start(GTK_BOX(vbox_id3v2), table_id3v2,
				   TRUE, TRUE, 10);
		label_id3v2 = gtk_label_new(_("ID3v2 tags"));
		gtk_notebook_append_page(GTK_NOTEBOOK(nb), vbox_id3v2,
					 label_id3v2);
		page++;

		while (id3 != NULL) {
			
			if (!is_called_from_browser) {
				append_table(table_id3v2, &cnt, (char *) id3->label, (char *) id3->utf8,
					     NULL, NULL);
			} else {
				import_data_t * data = import_data_new();
				trashlist_add(fileinfo_trash, data);
				
				if (strcmp(id3->id, ID3_FRAME_TITLE) == 0) {
					
					data->model = model;
					data->track_iter = track_iter;
					data->dest_type = IMPORT_DEST_TITLE;
					strncpy(data->str, (char *) id3->utf8, MAXLEN-1);
					append_table(table_id3v2, &cnt,
						     id3->label, (char *) id3->utf8,
						     _("Import as Title"), data);
				} else
				if (strcmp(id3->id, ID3_FRAME_ARTIST) == 0) {
					
					data->model = model;
					data->track_iter = track_iter;
					data->dest_type = IMPORT_DEST_ARTIST;
					strncpy(data->str, (char *) id3->utf8, MAXLEN-1);
					append_table(table_id3v2, &cnt,
						     id3->label, (char *) id3->utf8,
						     _("Import as Artist"), data);
				} else
				if (strcmp(id3->id, ID3_FRAME_ALBUM) == 0) {
					
					data->model = model;
					data->track_iter = track_iter;
					data->dest_type = IMPORT_DEST_RECORD;
					strncpy(data->str, (char *) id3->utf8, MAXLEN-1);
					append_table(table_id3v2, &cnt,
						     id3->label, (char *) id3->utf8,
						     _("Import as Record"), data);
				} else
				if (strcmp(id3->id, ID3_FRAME_TRACK) == 0) {
					
					data->model = model;
					data->track_iter = track_iter;
					data->dest_type = IMPORT_DEST_NUMBER;
					strncpy(data->str, (char *) id3->utf8, MAXLEN-1);
					append_table(table_id3v2, &cnt,
						     id3->label, (char *) id3->utf8,
						     _("Import as Track number"), data);
				} else
				if (strcmp(id3->id, "RVA2") == 0) {
					
					data->model = model;
					data->track_iter = track_iter;
					data->dest_type = IMPORT_DEST_RVA;
					data->fval = id3->fval;
					append_table(table_id3v2, &cnt,
						     _("Relative Volume"), (char *) id3->utf8,
						     _("Import as RVA"), data);
					
				} else {
					char tmp[MAXLEN];
					
					snprintf(tmp, MAXLEN-1, "%s %s",
						 id3->label, id3->utf8);
					data->model = model;
					data->track_iter = track_iter;
					data->dest_type = IMPORT_DEST_COMMENT;
					strncpy(data->str, tmp, MAXLEN-1);
					append_table(table_id3v2, &cnt,
						     id3->label, (char *) id3->utf8,
						     _("Add to Comments"), data);
				}
			}
			id3 = id3->next;
		}
	}
#endif /* HAVE_ID3 */

#ifdef HAVE_OGG_VORBIS
	cnt = 0;
	oggv = meta->oggv_root;
	if (oggv->next != NULL) {
		oggv = oggv->next;
		
		vbox_vorbis = gtk_vbox_new(FALSE, 4);
		table_vorbis = gtk_table_new(0, 3, FALSE);
		gtk_box_pack_start(GTK_BOX(vbox_vorbis), table_vorbis, TRUE, TRUE, 10);
		label_vorbis = gtk_label_new(_("Vorbis comments"));
		gtk_notebook_append_page(GTK_NOTEBOOK(nb), vbox_vorbis, label_vorbis);
		page++;
		
		while (oggv != NULL) {
			
			if (!is_called_from_browser) {
				append_table(table_vorbis, &cnt,
					     oggv->label, oggv->str, NULL, NULL);
			} else {
				import_data_t * data = import_data_new();
				trashlist_add(fileinfo_trash, data);
				
				if (strcmp(oggv->label, "Title:") == 0) {
					
					data->model = model;
					data->track_iter = track_iter;
					data->dest_type = IMPORT_DEST_TITLE;
					strncpy(data->str, oggv->str, MAXLEN-1);
					append_table(table_vorbis, &cnt,
						     oggv->label, oggv->str,
						     _("Import as Title"), data);
				} else
				if (strcmp(oggv->label, "Album:") == 0) {
					
					data->model = model;
					data->track_iter = track_iter;
					data->dest_type = IMPORT_DEST_RECORD;
					strncpy(data->str, oggv->str, MAXLEN-1);
					append_table(table_vorbis, &cnt,
						     oggv->label, oggv->str,
						     _("Import as Record"), data);
				} else
				if (strcmp(oggv->label, "Artist:") == 0) {
					
					data->model = model;
					data->track_iter = track_iter;
					data->dest_type = IMPORT_DEST_ARTIST;
					strncpy(data->str, oggv->str, MAXLEN-1);
					append_table(table_vorbis, &cnt,
						     oggv->label, oggv->str,
						     _("Import as Artist"), data);

				} else if ((strcmp(oggv->label, "Replaygain_track_gain:") == 0) ||
					   (strcmp(oggv->label, "Replaygain_album_gain:") == 0)) {

					char replaygain_label[MAXLEN];

					switch (options.replaygain_tag_to_use) {
					case 0:
						strcpy(replaygain_label, "Replaygain_track_gain:");
						break;
					case 1:
						strcpy(replaygain_label, "Replaygain_album_gain:");
						break;
					default:
						fprintf(stderr, "file_info.c: illegal "
							"replaygain_tag_to_use value -- "
							"please see the programmers\n");
					}
					
					if (strcmp(oggv->label, replaygain_label) == 0) {
						
						data->model = model;
						data->track_iter = track_iter;
						data->dest_type = IMPORT_DEST_RVA;
						data->fval = oggv->fval;
						append_table(table_vorbis, &cnt,
							     oggv->label, oggv->str,
							     _("Import as RVA"), data);
					} else {
						char tmp[MAXLEN];
						
						snprintf(tmp, MAXLEN-1, "%s %s",
							 oggv->label, oggv->str);
						data->model = model;
						data->track_iter = track_iter;
						data->dest_type = IMPORT_DEST_COMMENT;
						strncpy(data->str, tmp, MAXLEN-1);
						append_table(table_vorbis, &cnt,
							     oggv->label, oggv->str,
							     _("Add to Comments"), data);
					}
				} else {
					char tmp[MAXLEN];
					
					snprintf(tmp, MAXLEN-1, "%s %s",
						 oggv->label, oggv->str);
					data->model = model;
					data->track_iter = track_iter;
					data->dest_type = IMPORT_DEST_COMMENT;
					strncpy(data->str, tmp, MAXLEN-1);
					append_table(table_vorbis, &cnt,
						     oggv->label, oggv->str,
						     _("Add to Comments"), data);
				}
			}
			oggv = oggv->next;
		}
	}
#endif /* HAVE_OGG_VORBIS */

#ifdef HAVE_FLAC
	cnt = 0;
	oggv = meta->flac_root;
	if (oggv->next != NULL) {
		oggv = oggv->next;
		
		vbox_flac = gtk_vbox_new(FALSE, 4);
		table_flac = gtk_table_new(0, 3, FALSE);
		gtk_box_pack_start(GTK_BOX(vbox_flac), table_flac, TRUE, TRUE, 10);
		label_flac = gtk_label_new(_("FLAC metadata"));
		gtk_notebook_append_page(GTK_NOTEBOOK(nb), vbox_flac, label_flac);
		page++;
		
		while (oggv != NULL) {
			
			if (!is_called_from_browser) {
				append_table(table_flac, &cnt,
					     oggv->label, oggv->str, NULL, NULL);
			} else {
				import_data_t * data = import_data_new();
				trashlist_add(fileinfo_trash, data);
				
				if (strcmp(oggv->label, "Title:") == 0) {
					
					data->model = model;
					data->track_iter = track_iter;
					data->dest_type = IMPORT_DEST_TITLE;
					strncpy(data->str, oggv->str, MAXLEN-1);
					append_table(table_flac, &cnt,
						     oggv->label, oggv->str,
						     _("Import as Title"), data);
				} else
				if (strcmp(oggv->label, "Album:") == 0) {
					
					data->model = model;
					data->track_iter = track_iter;
					data->dest_type = IMPORT_DEST_RECORD;
					strncpy(data->str, oggv->str, MAXLEN-1);
					append_table(table_flac, &cnt,
						     oggv->label, oggv->str,
						     _("Import as Record"), data);
				} else
				if (strcmp(oggv->label, "Artist:") == 0) {
					
					data->model = model;
					data->track_iter = track_iter;
					data->dest_type = IMPORT_DEST_ARTIST;
					strncpy(data->str, oggv->str, MAXLEN-1);
					append_table(table_flac, &cnt,
						     oggv->label, oggv->str,
						     _("Import as Artist"), data);

				} else if ((strcmp(oggv->label, "Replaygain_track_gain:") == 0) ||
					   (strcmp(oggv->label, "Replaygain_album_gain:") == 0)) {

					char replaygain_label[MAXLEN];

					switch (options.replaygain_tag_to_use) {
					case 0:
						strcpy(replaygain_label, "Replaygain_track_gain:");
						break;
					case 1:
						strcpy(replaygain_label, "Replaygain_album_gain:");
						break;
					default:
						fprintf(stderr, "illegal replaygain_tag_to_use value -- "
							"please see the programmers\n");
					}
					
					if (strcmp(oggv->label, replaygain_label) == 0) {
						
						data->model = model;
						data->track_iter = track_iter;
						data->dest_type = IMPORT_DEST_RVA;
						data->fval = oggv->fval;
						append_table(table_flac, &cnt,
							     oggv->label, oggv->str,
							     _("Import as RVA"), data);
					} else {
						char tmp[MAXLEN];
						
						snprintf(tmp, MAXLEN-1, "%s %s",
							 oggv->label, oggv->str);
						data->model = model;
						data->track_iter = track_iter;
						data->dest_type = IMPORT_DEST_COMMENT;
						strncpy(data->str, tmp, MAXLEN-1);
						append_table(table_flac, &cnt,
							     oggv->label, oggv->str,
							     _("Add to Comments"), data);
					}
				} else {
					char tmp[MAXLEN];
					
					snprintf(tmp, MAXLEN-1, "%s %s",
						 oggv->label, oggv->str);
					data->model = model;
					data->track_iter = track_iter;
					data->dest_type = IMPORT_DEST_COMMENT;
					strncpy(data->str, tmp, MAXLEN-1);
					append_table(table_flac, &cnt,
						     oggv->label, oggv->str,
						     _("Add to Comments"), data);
				}
			}
			oggv = oggv->next;
		}
	}
#endif /* HAVE_FLAC */

#ifdef HAVE_MOD_INFO
	mdi = meta->mod_root;
	if (mdi->active) {
                if ((md_fdec = file_decoder_new()) == NULL) {
                        fprintf(stderr, "show_file_info(): error: file_decoder_new() returned NULL\n");
                }

                if (file_decoder_open(md_fdec, file)) {
                        fprintf(stderr, "file_decoder_open() failed on %s\n", file);
                }

                if (md_fdec->file_lib == MOD_LIB) {

                        vbox_mod = gtk_vbox_new(FALSE, 4);
                        label_mod = gtk_label_new(_("Module info"));
                        gtk_notebook_append_page(GTK_NOTEBOOK(nb), vbox_mod, label_mod);
                        page++;
                        fill_module_info_page(mdi, vbox_mod, file);
                } else {

                        file_decoder_close(md_fdec);
                        file_decoder_delete(md_fdec);
                }
        }
#endif /* HAVE_MOD_INFO */

	/* end of notebook stuff */

	gtk_widget_grab_focus(nb);

	hbuttonbox = gtk_hbutton_box_new();
	gtk_box_pack_end(GTK_BOX(vbox), hbuttonbox, FALSE, FALSE, 0);
	gtk_button_box_set_layout(GTK_BUTTON_BOX(hbuttonbox), GTK_BUTTONBOX_END);

        dismiss_btn = gtk_button_new_from_stock (GTK_STOCK_CLOSE); 
	g_signal_connect(dismiss_btn, "clicked", G_CALLBACK(dismiss), NULL);
  	gtk_container_add(GTK_CONTAINER(hbuttonbox), dismiss_btn);   

	gtk_widget_show_all(info_window);

	n_pages = gtk_notebook_get_n_pages(GTK_NOTEBOOK(nb));

        if (n_pages > 1) {
                page = options.tags_tab_first ? 1 : 0;
                gtk_notebook_set_current_page(GTK_NOTEBOOK(nb), page);
        }

	meta_free(meta);
}

#ifdef HAVE_MOD_INFO

/*
 * type = 0 for sample list
 * type != 0 for instrument list
 */
void
show_list (gint type) {
GtkTreeIter iter;
gint i, len;
gchar temp[MAXLEN], number[MAXLEN];
decoder_t * md_dec;
mod_pdata_t * md_pd;
        
        md_dec = (decoder_t *)(md_fdec->pdec);
        md_pd = (mod_pdata_t *)md_dec->pdata;

        if (type) {
                len = ModPlug_NumInstruments(md_pd->mpf);
        } else {
                len = ModPlug_NumSamples(md_pd->mpf);
        }

        if (len) {
                gtk_list_store_clear(smp_instr_list_store);
                for(i = 0; i < len; i++) {
                        memset(temp, 0, MAXLEN-1);

                        if (type) {
                                ModPlug_InstrumentName(md_pd->mpf, i, temp);
                        } else {
                                ModPlug_SampleName(md_pd->mpf, i, temp);
                        }

                        sprintf(number, "%2d", i);
                        gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(smp_instr_list_store), &iter, NULL, i);
                        gtk_list_store_append(smp_instr_list_store, &iter);
                        gtk_list_store_set(smp_instr_list_store, &iter, 0, number, 1, temp, -1);
                }
        }
}

void
set_first_row (void) {
GtkTreeIter iter;
GtkTreePath * visible_path;

        gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(smp_instr_list_store), &iter, NULL, 0);
        visible_path = gtk_tree_model_get_path (GTK_TREE_MODEL(smp_instr_list_store), &iter);
        gtk_tree_view_set_cursor (GTK_TREE_VIEW (smp_instr_list), visible_path, NULL, TRUE);
        gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (smp_instr_list), visible_path,
                                      NULL, TRUE, 1.0, 0.0);
        gtk_widget_grab_focus(GTK_WIDGET(smp_instr_list));
}

void 
radio_buttons_cb (GtkToggleButton *toggle_button, gboolean state) {

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(toggle_button))) {
                show_list (0);
        } else {
                show_list (1);
        }
        set_first_row();
}

void
fill_module_info_page(mod_info *mdi, GtkWidget *vbox, char *file) {

gchar *a_type[] = {
        "None", "MOD", "S3M", "XM", "MED", "MTM", "IT", "669",
        "ULT", "STM", "FAR", "WAV", "AMF", "AMS", "DSM", "MDL",
        "OKT", "MID", "DMF", "PTM", "DBM", "MT2", "AMF0", "PSM",
        "J2B", "UMX"
};

gint i, n;
gchar temp[MAXLEN];
GtkWidget *table;
GtkWidget *label;
GtkWidget *mod_type_label;
GtkWidget *mod_channels_label;
GtkWidget *mod_patterns_label;
GtkWidget *mod_samples_label;
GtkWidget *mod_instruments_label;
GtkWidget *hseparator;
GtkWidget *hbox;
GtkWidget *samples_radiobutton = NULL;
GtkWidget *instruments_radiobutton = NULL;
GSList *samples_radiobutton_group = NULL;
GtkWidget *scrolledwindow;

GtkCellRenderer *renderer;
GtkTreeViewColumn *column;

        if (mdi->instruments) {

                table = gtk_table_new (2, 6, FALSE);
                gtk_widget_show (table);
                gtk_box_pack_start (GTK_BOX (vbox), table, FALSE, FALSE, 0);
                gtk_container_set_border_width (GTK_CONTAINER (table), 8);
                gtk_table_set_row_spacings (GTK_TABLE (table), 4);
                gtk_table_set_col_spacings (GTK_TABLE (table), 4);

                label = gtk_label_new (_("Type:"));
                gtk_widget_show (label);
                gtk_table_attach (GTK_TABLE (table), label, 0, 1, 0, 1,
                                  (GtkAttachOptions) (GTK_FILL),
                                  (GtkAttachOptions) (0), 0, 0);
                gtk_misc_set_alignment (GTK_MISC (label), 1, 0.5);

                mod_type_label = gtk_label_new ("");
                gtk_widget_show (mod_type_label);
                gtk_table_attach (GTK_TABLE (table), mod_type_label, 1, 2, 0, 1,
                                  (GtkAttachOptions) (GTK_FILL),
                                  (GtkAttachOptions) (0), 0, 0);

                label = gtk_label_new (_("Channels:"));
                gtk_widget_show (label);
                gtk_table_attach (GTK_TABLE (table), label, 2, 3, 0, 1,
                                  (GtkAttachOptions) (GTK_FILL),
                                  (GtkAttachOptions) (0), 0, 0);
                gtk_widget_set_size_request (label, 150, -1);
                gtk_misc_set_alignment (GTK_MISC (label), 1, 0.5);

                mod_channels_label = gtk_label_new ("");
                gtk_widget_show (mod_channels_label);
                gtk_table_attach (GTK_TABLE (table), mod_channels_label, 3, 4, 0, 1,
                                  (GtkAttachOptions) (GTK_FILL),
                                  (GtkAttachOptions) (0), 0, 0);

                label = gtk_label_new (_("Patterns:"));
                gtk_widget_show (label);
                gtk_table_attach (GTK_TABLE (table), label, 4, 5, 0, 1,
                                  (GtkAttachOptions) (GTK_FILL),
                                  (GtkAttachOptions) (0), 0, 0);
                gtk_widget_set_size_request (label, 150, -1);
                gtk_misc_set_alignment (GTK_MISC (label), 1, 0.5);

                mod_patterns_label = gtk_label_new ("");
                gtk_widget_show (mod_patterns_label);
                gtk_table_attach (GTK_TABLE (table), mod_patterns_label, 5, 6, 0, 1,
                                  (GtkAttachOptions) (GTK_FILL),
                                  (GtkAttachOptions) (0), 0, 0);

                label = gtk_label_new (_("Samples:"));
                gtk_widget_show (label);
                gtk_table_attach (GTK_TABLE (table), label, 0, 1, 1, 2,
                                  (GtkAttachOptions) (GTK_FILL),
                                  (GtkAttachOptions) (0), 0, 0);
                gtk_misc_set_alignment (GTK_MISC (label), 1, 0.5);

                mod_samples_label = gtk_label_new ("");
                gtk_widget_show (mod_samples_label);
                gtk_table_attach (GTK_TABLE (table), mod_samples_label, 1, 2, 1, 2,
                                  (GtkAttachOptions) (GTK_FILL),
                                  (GtkAttachOptions) (0), 0, 0);

                label = gtk_label_new (_("Instruments:"));
                gtk_widget_show (label);
                gtk_table_attach (GTK_TABLE (table), label, 2, 3, 1, 2,
                                  (GtkAttachOptions) (GTK_FILL),
                                  (GtkAttachOptions) (0), 0, 0);
                gtk_misc_set_alignment (GTK_MISC (label), 1, 0.5);

                mod_instruments_label = gtk_label_new ("");
                gtk_widget_show (mod_instruments_label);
                gtk_table_attach (GTK_TABLE (table), mod_instruments_label, 3, 4, 1, 2,
                                  (GtkAttachOptions) (GTK_FILL),
                                  (GtkAttachOptions) (0), 0, 0);

                sprintf(temp, "%d", mdi->instruments);
                gtk_label_set_text (GTK_LABEL(mod_instruments_label), temp);

        } else {

                table = gtk_table_new (1, 8, FALSE);
                gtk_widget_show (table);
                gtk_box_pack_start (GTK_BOX (vbox), table, FALSE, FALSE, 0);
                gtk_container_set_border_width (GTK_CONTAINER (table), 8);
                gtk_table_set_row_spacings (GTK_TABLE (table), 4);
                gtk_table_set_col_spacings (GTK_TABLE (table), 4);

                label = gtk_label_new (_("Type:"));
                gtk_widget_show (label);
                gtk_table_attach (GTK_TABLE (table), label, 0, 1, 0, 1,
                                  (GtkAttachOptions) (GTK_FILL),
                                  (GtkAttachOptions) (0), 0, 0);
                gtk_misc_set_alignment (GTK_MISC (label), 1, 0.5);

                mod_type_label = gtk_label_new ("");
                gtk_widget_show (mod_type_label);
                gtk_table_attach (GTK_TABLE (table), mod_type_label, 1, 2, 0, 1,
                                  (GtkAttachOptions) (GTK_FILL),
                                  (GtkAttachOptions) (0), 0, 0);

                label = gtk_label_new (_("Channels:"));
                gtk_widget_show (label);
                gtk_table_attach (GTK_TABLE (table), label, 2, 3, 0, 1,
                                  (GtkAttachOptions) (GTK_FILL),
                                  (GtkAttachOptions) (0), 0, 0);
                gtk_widget_set_size_request (label, 110, -1);
                gtk_misc_set_alignment (GTK_MISC (label), 1, 0.5);

                mod_channels_label = gtk_label_new ("");
                gtk_widget_show (mod_channels_label);
                gtk_table_attach (GTK_TABLE (table), mod_channels_label, 3, 4, 0, 1,
                                  (GtkAttachOptions) (GTK_FILL),
                                  (GtkAttachOptions) (0), 0, 0);

                label = gtk_label_new (_("Patterns:"));
                gtk_widget_show (label);
                gtk_table_attach (GTK_TABLE (table), label, 4, 5, 0, 1,
                                  (GtkAttachOptions) (GTK_FILL),
                                  (GtkAttachOptions) (0), 0, 0);
                gtk_widget_set_size_request (label, 110, -1);
                gtk_misc_set_alignment (GTK_MISC (label), 1, 0.5);

                mod_patterns_label = gtk_label_new ("");
                gtk_widget_show (mod_patterns_label);
                gtk_table_attach (GTK_TABLE (table), mod_patterns_label, 5, 6, 0, 1,
                                  (GtkAttachOptions) (GTK_FILL),
                                  (GtkAttachOptions) (0), 0, 0);

                label = gtk_label_new (_("Samples:"));
                gtk_widget_show (label);
                gtk_table_attach (GTK_TABLE (table), label, 6, 7, 0, 1,
                                  (GtkAttachOptions) (GTK_FILL),
                                  (GtkAttachOptions) (0), 0, 0);
                gtk_widget_set_size_request (label, 110, -1);
                gtk_misc_set_alignment (GTK_MISC (label), 1, 0.5);

                mod_samples_label = gtk_label_new ("");
                gtk_widget_show (mod_samples_label);
                gtk_table_attach (GTK_TABLE (table), mod_samples_label, 7, 8, 0, 1,
                                  (GtkAttachOptions) (GTK_FILL),
                                  (GtkAttachOptions) (0), 0, 0);
        }

        n = mdi->type;
        i = 0;
        
        while (n > 0) {         /* calculate module type index */
                n >>= 1;
                i++;
        }

        gtk_label_set_text (GTK_LABEL(mod_type_label), a_type[i]);
        sprintf(temp, "%d", mdi->channels);

        gtk_label_set_text (GTK_LABEL(mod_channels_label), temp);
        sprintf(temp, "%d", mdi->patterns);
        gtk_label_set_text (GTK_LABEL(mod_patterns_label), temp);
        sprintf(temp, "%d", mdi->samples);
        gtk_label_set_text (GTK_LABEL(mod_samples_label), temp);

        hseparator = gtk_hseparator_new ();
        gtk_widget_show (hseparator);
        gtk_box_pack_start (GTK_BOX (vbox), hseparator, FALSE, FALSE, 2);

        if (mdi->instruments) {

                hbox = gtk_hbox_new (FALSE, 0);
                gtk_widget_show (hbox);
                gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

                samples_radiobutton = gtk_radio_button_new_with_mnemonic (NULL, _("Samples"));
                gtk_widget_show (samples_radiobutton);
                gtk_box_pack_start (GTK_BOX (hbox), samples_radiobutton, FALSE, TRUE, 0);
                gtk_container_set_border_width (GTK_CONTAINER (samples_radiobutton), 4);
                gtk_radio_button_set_group (GTK_RADIO_BUTTON (samples_radiobutton), samples_radiobutton_group);
                samples_radiobutton_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (samples_radiobutton));
		g_signal_connect(G_OBJECT(samples_radiobutton), "toggled",
				 G_CALLBACK(radio_buttons_cb), NULL);

                instruments_radiobutton = gtk_radio_button_new_with_mnemonic (NULL, _("Instruments"));
                gtk_widget_show (instruments_radiobutton);
                gtk_box_pack_start (GTK_BOX (hbox), instruments_radiobutton, FALSE, TRUE, 0);
                gtk_container_set_border_width (GTK_CONTAINER (instruments_radiobutton), 4);
                gtk_radio_button_set_group (GTK_RADIO_BUTTON (instruments_radiobutton), samples_radiobutton_group);
                samples_radiobutton_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (instruments_radiobutton));
        }

        scrolledwindow = gtk_scrolled_window_new (NULL, NULL);
        gtk_container_set_border_width (GTK_CONTAINER (scrolledwindow), 4);
        gtk_widget_show (scrolledwindow);
        gtk_widget_set_size_request (scrolledwindow, -1, 220);
        gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow), 
                                        GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
        gtk_box_pack_start (GTK_BOX (vbox), scrolledwindow, TRUE, TRUE, 0);
        gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolledwindow), GTK_SHADOW_IN);

        smp_instr_list_store = gtk_list_store_new(2, 
                                        G_TYPE_STRING,
                                        G_TYPE_STRING);
        smp_instr_list = gtk_tree_view_new_with_model(GTK_TREE_MODEL(smp_instr_list_store));
        gtk_widget_show (smp_instr_list);
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(_("No."), renderer, "text", 0, NULL);
        gtk_tree_view_column_set_sizing(GTK_TREE_VIEW_COLUMN(column),
                                        GTK_TREE_VIEW_COLUMN_FIXED);
        gtk_tree_view_column_set_spacing(GTK_TREE_VIEW_COLUMN(column), 3);
        gtk_tree_view_column_set_resizable(GTK_TREE_VIEW_COLUMN(column), FALSE);
        gtk_tree_view_column_set_fixed_width(GTK_TREE_VIEW_COLUMN(column), 40);
	gtk_tree_view_append_column(GTK_TREE_VIEW(smp_instr_list), column);
	column = gtk_tree_view_column_new_with_attributes(_("Name"), renderer, "text", 1, NULL);
        gtk_tree_view_column_set_sizing(GTK_TREE_VIEW_COLUMN(column),
                                        GTK_TREE_VIEW_COLUMN_FIXED);
	gtk_tree_view_append_column(GTK_TREE_VIEW(smp_instr_list), column);
        gtk_tree_view_column_set_spacing(GTK_TREE_VIEW_COLUMN(column), 3);
        gtk_tree_view_column_set_resizable(GTK_TREE_VIEW_COLUMN(column), FALSE);
        gtk_container_add (GTK_CONTAINER (scrolledwindow), smp_instr_list);

        if (mdi->instruments && mdi->type == 0x4) { /* if XM module go to instrument page */
                gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (instruments_radiobutton), TRUE);
        } else {
                show_list (0);
                set_first_row();
        }
}
#endif /* HAVE_MOD_INFO */


// vim: shiftwidth=8:tabstop=8:softtabstop=8 :  
