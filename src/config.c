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

#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>

#include "config.h"
#include "parser.h"
#include "main.h"
#include "build-config.h"

#define KEY_DFLT_SHOW_INTRO_ON_STARTUP TRUE

#define KEYFILE_FLAGS G_KEY_FILE_KEEP_COMMENTS|G_KEY_FILE_KEEP_TRANSLATIONS

/*
 * Functions for parsing the Configuration-file
 */

gboolean parse_config (GromitData *data)
{
  gboolean status = FALSE;
  GromitPaintContext *context=NULL;
  GScanner *scanner;
  GTokenType token;
  gchar *filename;
  int file;
  gchar *name;

  /* try user config location */
  filename = g_strjoin (G_DIR_SEPARATOR_S,
                        g_get_user_config_dir(), "gromit-mpx.cfg", NULL);
  if ((file = open(filename, O_RDONLY)) < 0)
      g_print("Could not open user config %s: %s\n", filename, g_strerror (errno));
  else
      g_print("Using user config %s\n", filename);


  /* try global config file */
  if (file < 0) {
      g_free(filename);
      filename = g_strdup (SYSCONFDIR "/gromit-mpx/gromit-mpx.cfg");
      if ((file = open(filename, O_RDONLY)) < 0)
	  g_print("Could not open system config %s: %s\n", filename, g_strerror (errno));
      else
	  g_print("Using system config %s\n", filename);
  }

  /* was the last possibility, no use to go on */
  if (file < 0) {
      g_free(filename);
      GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(data->win),
						 GTK_DIALOG_DESTROY_WITH_PARENT,
						 GTK_MESSAGE_WARNING,
						 GTK_BUTTONS_CLOSE,
						 _("No usable config file found, falling back to default tools."));
      gtk_dialog_run (GTK_DIALOG (dialog));
      gtk_widget_destroy (dialog);
      return FALSE;
  }

  scanner = g_scanner_new(NULL);
  scanner_init(scanner);
  scanner->input_name = filename;
  g_scanner_input_file (scanner, file);

  token = g_scanner_get_next_token (scanner);

  GromitStyleDef style;

  while (token != G_TOKEN_EOF)
    {
      if (token == G_TOKEN_STRING)
        {
          /*
           * New tool definition
           */

          name = parse_name (scanner);

	  if(!name)
	      goto cleanup;

          if (!parse_tool(data, scanner, &style))
            goto cleanup;

          token = g_scanner_cur_token(scanner);
          //
          /* Are there any tool-options?
           */

          if (token == G_TOKEN_LEFT_PAREN)
            {
              if (! parse_style(scanner, &style))
                goto cleanup;
              token = g_scanner_cur_token (scanner);
            }

          /*
           * Finally we expect a semicolon
           */

          if (token != ';')
            {
              g_printerr ("Expected \";\"\n");
              goto cleanup;
            }

          context =
            paint_context_new (data, style.type, style.paint_color,
                               style.width, style.arrowsize,
                               style.minwidth, style.maxwidth);
          g_hash_table_insert (data->tool_config, name, context);
        }
      else if (token == G_TOKEN_SYMBOL &&
               (scanner->value.v_symbol == HOTKEY_SYMBOL_VALUE ||
                scanner->value.v_symbol == UNDOKEY_SYMBOL_VALUE))
        {
          /*
           * Hot key definition
           */

          gpointer key_type = scanner->value.v_symbol;
          token = g_scanner_get_next_token(scanner);

          if (token != G_TOKEN_EQUAL_SIGN)
            {
              g_scanner_unexp_token (scanner, G_TOKEN_EQUAL_SIGN, NULL,
                                     NULL, NULL, "aborting", TRUE);
              goto cleanup;
            }

          token = g_scanner_get_next_token (scanner);

          if (token != G_TOKEN_STRING)
            {
              g_scanner_unexp_token (scanner, G_TOKEN_STRING, NULL,
                                     NULL, NULL, "aborting", TRUE);
              goto cleanup;
            }

          if (key_type == HOTKEY_SYMBOL_VALUE)
            {
              data->hot_keyval = g_strdup(scanner->value.v_string);
            }
          else if (key_type == UNDOKEY_SYMBOL_VALUE)
            {
              data->undo_keyval = g_strdup(scanner->value.v_string);
            }

          token = g_scanner_get_next_token(scanner);

          if (token != ';')
            {
              g_printerr ("Expected \";\"\n");
              goto cleanup;
            }
        }
      else
        {
          g_printerr ("Expected name of Tool to define or Hot key definition\n");
          goto cleanup;
        }

      token = g_scanner_get_next_token (scanner);
    }

  status = TRUE;

 cleanup:

  if (!status) {
      /* purge incomplete tool config */
      GHashTableIter it;
      gpointer value;
      g_hash_table_iter_init (&it, data->tool_config);
      while (g_hash_table_iter_next (&it, NULL, &value))
	  paint_context_free(value);
      g_hash_table_remove_all(data->tool_config);

      /* alert user */
      GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(data->win),
						 GTK_DIALOG_DESTROY_WITH_PARENT,
						 GTK_MESSAGE_WARNING,
						 GTK_BUTTONS_CLOSE,
						 _("Failed parsing config file %s, falling back to default tools."),
						 filename);
      gtk_dialog_run (GTK_DIALOG (dialog));
      gtk_widget_destroy (dialog);
  }

  g_scanner_destroy (scanner);
  close (file);
  g_free (filename);

  return status;
}


int parse_args (int argc, char **argv, GromitData *data)
{
   gint      i;
   gchar    *arg;
   gboolean  wrong_arg = FALSE;
   gboolean  activate = FALSE;

   for (i=1; i < argc ; i++)
     {
       arg = argv[i];
       if (strcmp (arg, "-a") == 0 ||
           strcmp (arg, "--active") == 0)
         {
           activate = TRUE;
         }
       else if (strcmp (arg, "-d") == 0 ||
                strcmp (arg, "--debug") == 0)
         {
           data->debug = 1;
         }
       else if (strcmp (arg, "-k") == 0 ||
                strcmp (arg, "--key") == 0)
         {
           if (i+1 < argc)
             {
               data->hot_keyval = argv[i+1];
               data->hot_keycode = 0;
               i++;
             }
           else
             {
               g_printerr ("-k requires an Key-Name as argument\n");
               wrong_arg = TRUE;
             }
         }
       else if (strcmp (arg, "-K") == 0 ||
                strcmp (arg, "--keycode") == 0)
         {
           if (i+1 < argc && atoi (argv[i+1]) > 0)
             {
               data->hot_keyval = NULL;
               data->hot_keycode = atoi (argv[i+1]);
               i++;
             }
           else
             {
               g_printerr ("-K requires an keycode > 0 as argument\n");
               wrong_arg = TRUE;
             }
         }
       else if (strcmp (arg, "-o") == 0 ||
                strcmp (arg, "--opacity") == 0)
         {
           if (i+1 < argc && strtod (argv[i+1], NULL) >= 0.0 && strtod (argv[i+1], NULL) <= 1.0)
             {
               data->opacity = strtod (argv[i+1], NULL);
               g_printerr ("Opacity set to: %.2f\n", data->opacity);
               gtk_widget_set_opacity(data->win, data->opacity);
               i++;
             }
           else
             {
               g_printerr ("-o requires an opacity >=0 and <=1 as argument\n");
               wrong_arg = TRUE;
             }
         }
       else if (strcmp (arg, "-u") == 0 ||
                strcmp (arg, "--undo-key") == 0)
         {
           if (i+1 < argc)
             {
               data->undo_keyval = argv[i+1];
               data->undo_keycode = 0;
               i++;
             }
           else
             {
               g_printerr ("-u requires an Key-Name as argument\n");
               wrong_arg = TRUE;
             }
         }
       else if (strcmp (arg, "-U") == 0 ||
                strcmp (arg, "--undo-keycode") == 0)
         {
           if (i+1 < argc && atoi (argv[i+1]) > 0)
             {
               data->undo_keyval = NULL;
               data->undo_keycode = atoi (argv[i+1]);
               i++;
             }
           else
             {
               g_printerr ("-U requires an keycode > 0 as argument\n");
               wrong_arg = TRUE;
             }
         }
       else if (strcmp (arg, "-V") == 0 ||
		strcmp (arg, "--version") == 0)
         {
	     g_print("Gromit-MPX " PACKAGE_VERSION "\n");
	     exit(0);
         }
       else
         {
           g_printerr ("Unknown Option for Gromit-MPX startup: \"%s\"\n", arg);
           wrong_arg = TRUE;
         }

       if (wrong_arg)
         {
           g_printerr ("Please see the Gromit-MPX manpage for the correct usage\n");
           exit (1);
         }
     }

   return activate;
}


void read_keyfile(GromitData *data)
{
    gchar *filename = g_strjoin (G_DIR_SEPARATOR_S,
				 g_get_user_config_dir(), "gromit-mpx.ini", NULL);
    /*
      set defaults
    */
    data->show_intro_on_startup = KEY_DFLT_SHOW_INTRO_ON_STARTUP;

    /*
      read actual settings
    */
    GError *error = NULL;
    GKeyFile *key_file = g_key_file_new ();

    if (!g_key_file_load_from_file (key_file, filename, KEYFILE_FLAGS, &error)) {
	// treat file-not-found not as an error
	if (!g_error_matches (error, G_FILE_ERROR, G_FILE_ERROR_NOENT))
	    g_warning ("Error loading key file: %s", error->message);
	g_error_free(error);
	goto cleanup;
    }

    data->show_intro_on_startup = g_key_file_get_boolean (key_file, "General", "ShowIntroOnStartup", &error);
    data->opacity = g_key_file_get_double (key_file, "Drawing", "Opacity", &error);
    // 0.0 on not-found, but anyway, also don't use 0.0 when user-set
    if(data->opacity == 0)
	data->opacity = DEFAULT_OPACITY;

  cleanup:
    g_free(filename);
    g_key_file_free(key_file);
}


void write_keyfile(GromitData *data)
{
    gchar *filename = g_strjoin (G_DIR_SEPARATOR_S,
				 g_get_user_config_dir(), "gromit-mpx.ini", NULL);

    GError *error = NULL;
    GKeyFile *key_file = g_key_file_new ();

    g_key_file_set_boolean (key_file, "General", "ShowIntroOnStartup", data->show_intro_on_startup);
    g_key_file_set_double (key_file, "Drawing", "Opacity", data->opacity);

    // if file exists but is read-only, bail out
    if (access(filename, F_OK) == 0 && access(filename, W_OK) != 0) {
	g_warning ("Not overwriting read-only key file");
	goto cleanup;
    }

    // Save as a file.
    if (!g_key_file_save_to_file (key_file, filename, &error)) {
	g_warning ("Error saving key file: %s", error->message);
	g_error_free(error);
	goto cleanup;
    }

 cleanup:
    g_free(filename);
    g_key_file_free(key_file);
}
