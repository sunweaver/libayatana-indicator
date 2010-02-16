/*
A small file to parse through the actions that are available
in the desktop file and making those easily usable.

Copyright 2010 Canonical Ltd.

Authors:
    Ted Gould <ted@canonical.com>

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
version 3.0 as published by the Free Software Foundation.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License version 3.0 for more details.

You should have received a copy of the GNU General Public
License along with this library. If not, see
<http://www.gnu.org/licenses/>.
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gio/gdesktopappinfo.h>
#include "indicator-desktop-shortcuts.h"

#define GROUP_SUFFIX          "Shortcut Group"

#define PROP_DESKTOP_FILE_S   "desktop-file"
#define PROP_IDENTITY_S       "identity"

typedef struct _IndicatorDesktopShortcutsPrivate IndicatorDesktopShortcutsPrivate;
struct _IndicatorDesktopShortcutsPrivate {
	GKeyFile * keyfile;
	gchar * identity;
	GArray * nicks;
};

enum {
	PROP_0,
	PROP_DESKTOP_FILE,
	PROP_IDENTITY
};

#define INDICATOR_DESKTOP_SHORTCUTS_GET_PRIVATE(o) \
		(G_TYPE_INSTANCE_GET_PRIVATE ((o), INDICATOR_TYPE_DESKTOP_SHORTCUTS, IndicatorDesktopShortcutsPrivate))

static void indicator_desktop_shortcuts_class_init (IndicatorDesktopShortcutsClass *klass);
static void indicator_desktop_shortcuts_init       (IndicatorDesktopShortcuts *self);
static void indicator_desktop_shortcuts_dispose    (GObject *object);
static void indicator_desktop_shortcuts_finalize   (GObject *object);
static void set_property (GObject * object, guint prop_id, const GValue * value, GParamSpec * pspec);
static void get_property (GObject * object, guint prop_id, GValue * value, GParamSpec * pspec);

G_DEFINE_TYPE (IndicatorDesktopShortcuts, indicator_desktop_shortcuts, G_TYPE_OBJECT);

/* Build up the class */
static void
indicator_desktop_shortcuts_class_init (IndicatorDesktopShortcutsClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (IndicatorDesktopShortcutsPrivate));

	object_class->dispose = indicator_desktop_shortcuts_dispose;
	object_class->finalize = indicator_desktop_shortcuts_finalize;

	/* Property funcs */
	object_class->set_property = set_property;
	object_class->get_property = get_property;

	g_object_class_install_property(object_class, PROP_DESKTOP_FILE,
	                                g_param_spec_string(PROP_DESKTOP_FILE_S,
	                                                    "The path of the desktop file to read",
	                                                    "A path to a desktop file that we'll look for shortcuts in.",
	                                                    NULL,
	                                                    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT_ONLY));
	g_object_class_install_property(object_class, PROP_IDENTITY,
	                                g_param_spec_string(PROP_IDENTITY_S,
	                                                    "The string that represents the identity that we're acting as.",
	                                                    "Used to process ShowIn and NotShownIn fields of the desktop shortcust to get the proper list.",
	                                                    NULL,
	                                                    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT_ONLY));

	return;
}

/* Initialize instance data */
static void
indicator_desktop_shortcuts_init (IndicatorDesktopShortcuts *self)
{
	IndicatorDesktopShortcutsPrivate * priv = INDICATOR_DESKTOP_SHORTCUTS_GET_PRIVATE(self);

	priv->keyfile = NULL;
	priv->identity = NULL;
	priv->nicks = g_array_new(TRUE, TRUE, sizeof(gchar *));

	return;
}

/* Clear object references */
static void
indicator_desktop_shortcuts_dispose (GObject *object)
{
	IndicatorDesktopShortcutsPrivate * priv = INDICATOR_DESKTOP_SHORTCUTS_GET_PRIVATE(object);

	if (priv->keyfile) {
		g_key_file_free(priv->keyfile);
		priv->keyfile = NULL;
	}

	G_OBJECT_CLASS (indicator_desktop_shortcuts_parent_class)->dispose (object);
	return;
}

/* Free all memory */
static void
indicator_desktop_shortcuts_finalize (GObject *object)
{
	IndicatorDesktopShortcutsPrivate * priv = INDICATOR_DESKTOP_SHORTCUTS_GET_PRIVATE(object);

	if (priv->identity != NULL) {
		g_free(priv->identity);
		priv->identity = NULL;
	}

	if (priv->nicks != NULL) {
		gint i;
		for (i = 0; i < priv->nicks->len; i++) {
			gchar * nick = g_array_index(priv->nicks, gchar *, i);
			g_free(nick);
		}
		g_array_free(priv->nicks, TRUE);
		priv->nicks = NULL;
	}

	G_OBJECT_CLASS (indicator_desktop_shortcuts_parent_class)->finalize (object);
	return;
}

/* Sets one of the two properties we have, only at construction though */
static void
set_property (GObject * object, guint prop_id, const GValue * value, GParamSpec * pspec)
{
	g_return_if_fail(INDICATOR_IS_DESKTOP_SHORTCUTS(object));
	IndicatorDesktopShortcutsPrivate * priv = INDICATOR_DESKTOP_SHORTCUTS_GET_PRIVATE(object);

	switch(prop_id) {
	case PROP_DESKTOP_FILE:
		break;
	case PROP_IDENTITY:
		if (priv->identity != NULL) {
			g_warning("Identity already set to '%s' and trying to set it to '%s'.", priv->identity, g_value_get_string(value));
			return;
		}
		priv->identity = g_value_dup_string(value);
		break;
	/* *********************** */
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}

	return;
}

/* Gets either the desktop file our the identity. */
static void
get_property (GObject * object, guint prop_id, GValue * value, GParamSpec * pspec)
{
	g_return_if_fail(INDICATOR_IS_DESKTOP_SHORTCUTS(object));
	IndicatorDesktopShortcutsPrivate * priv = INDICATOR_DESKTOP_SHORTCUTS_GET_PRIVATE(object);

	switch(prop_id) {
	case PROP_DESKTOP_FILE:
		break;
	case PROP_IDENTITY:
		g_value_set_string(value, priv->identity);
		break;
	/* *********************** */
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}

	return;
}

/* Looks through the nicks ot see if this one is in the list,
   and thus valid to use. */
static gboolean
is_valid_nick (gchar ** list, const gchar * nick)
{
	if (*list == NULL)
		return FALSE;
	if (g_strcmp0(*list, nick) == 0)
		return TRUE;
	return is_valid_nick(list++, nick);
}

/* API */

/**
	indicator_desktop_shortcuts_new:
	@file: The desktop file that would be opened to
		find the actions.
	@identity: This is a string that represents the identity
		that should be used in searching those actions.  It 
		relates to the ShowIn and NotShownIn properties.
	
	This function creates the basic object.  It involves opening
	the file and parsing it.  It could potentially block on IO.  At
	the end of the day you'll have a fully functional object.

	Return value: A new #IndicatorDesktopShortcuts object.
*/
IndicatorDesktopShortcuts *
indicator_desktop_shortcuts_new (const gchar * file, const gchar * identity)
{
	GObject * obj = g_object_new(INDICATOR_TYPE_DESKTOP_SHORTCUTS,
	                             PROP_DESKTOP_FILE_S, file,
	                             PROP_IDENTITY_S, identity,
	                             NULL);
	return INDICATOR_DESKTOP_SHORTCUTS(obj);
}

/**
	indicator_desktop_shortcuts_get_nicks:
	@ids: The #IndicatorDesktopShortcuts object to look in

	Give you the list of commands that are available for this desktop
	file given the identity that was passed in at creation.  This will
	filter out the various items in the desktop file.  These nicks can
	then be used as keys for working with the desktop file.

	Return value: A #NULL terminated list of strings.  This memory
		is managed by the @ids object.
*/
const gchar **
indicator_desktop_shortcuts_get_nicks (IndicatorDesktopShortcuts * ids)
{
	g_return_val_if_fail(INDICATOR_IS_DESKTOP_SHORTCUTS(ids), NULL);
	IndicatorDesktopShortcutsPrivate * priv = INDICATOR_DESKTOP_SHORTCUTS_GET_PRIVATE(ids);
	return (const gchar **)priv->nicks->data;
}

/**
	indicator_desktop_shortcuts_nick_get_name:
	@ids: The #IndicatorDesktopShortcuts object to look in
	@nick: Which command that we're referencing.

	This function looks in a desktop file for a nick to find the
	user visible name for that shortcut.  The @nick parameter
	should be gotten from #indicator_desktop_shortcuts_get_nicks
	though it's not required that the exact memory location
	be the same.

	Return value: A user visible string for the shortcut or
		#NULL on error.
*/
gchar *
indicator_desktop_shortcuts_nick_get_name (IndicatorDesktopShortcuts * ids, const gchar * nick)
{
	g_return_val_if_fail(INDICATOR_IS_DESKTOP_SHORTCUTS(ids), NULL);
	IndicatorDesktopShortcutsPrivate * priv = INDICATOR_DESKTOP_SHORTCUTS_GET_PRIVATE(ids);

	g_return_val_if_fail(priv->keyfile != NULL, NULL);
	g_return_val_if_fail(is_valid_nick((gchar **)priv->nicks->data, nick), NULL);

	gchar * groupheader = g_strdup_printf("%s " GROUP_SUFFIX, nick);
	if (!g_key_file_has_group(priv->keyfile, groupheader)) {
		g_warning("The group for nick '%s' doesn't exist anymore.", nick);
		g_free(groupheader);
		return NULL;
	}

	if (!g_key_file_has_key(priv->keyfile, groupheader, G_KEY_FILE_DESKTOP_KEY_NAME, NULL)) {
		g_warning("No name available for nick '%s'", nick);
		g_free(groupheader);
		return NULL;
	}

	gchar * name = g_key_file_get_locale_string(priv->keyfile,
	                                            groupheader,
	                                            G_KEY_FILE_DESKTOP_KEY_NAME,
	                                            NULL,
	                                            NULL);

	g_free(groupheader);

	return name;
}

/**
	indicator_desktop_shortcuts_nick_exec:
	@ids: The #IndicatorDesktopShortcuts object to look in
	@nick: Which command that we're referencing.

	Here we take a @nick and try and execute the action that is
	associated with it.  The @nick parameter should be gotten
	from #indicator_desktop_shortcuts_get_nicks though it's not
	required that the exact memory location be the same.

	Return value: #TRUE on success or #FALSE on error.
*/
gboolean
indicator_desktop_shortcuts_nick_exec (IndicatorDesktopShortcuts * ids, const gchar * nick)
{
	GError * error = NULL;

	g_return_val_if_fail(INDICATOR_IS_DESKTOP_SHORTCUTS(ids), FALSE);
	IndicatorDesktopShortcutsPrivate * priv = INDICATOR_DESKTOP_SHORTCUTS_GET_PRIVATE(ids);

	g_return_val_if_fail(priv->keyfile != NULL, FALSE);
	g_return_val_if_fail(is_valid_nick((gchar **)priv->nicks->data, nick), FALSE);

	gchar * groupheader = g_strdup_printf("%s " GROUP_SUFFIX, nick);
	if (!g_key_file_has_group(priv->keyfile, groupheader)) {
		g_warning("The group for nick '%s' doesn't exist anymore.", nick);
		g_free(groupheader);
		return FALSE;
	}

	if (!g_key_file_has_key(priv->keyfile, groupheader, G_KEY_FILE_DESKTOP_KEY_NAME, NULL)) {
		g_warning("No name available for nick '%s'", nick);
		g_free(groupheader);
		return FALSE;
	}

	if (!g_key_file_has_key(priv->keyfile, groupheader, G_KEY_FILE_DESKTOP_KEY_EXEC, NULL)) {
		g_warning("No exec available for nick '%s'", nick);
		g_free(groupheader);
		return FALSE;
	}

	/* Grab the name and the exec entries out of our current group */
	gchar * name = g_key_file_get_locale_string(priv->keyfile,
	                                            groupheader,
	                                            G_KEY_FILE_DESKTOP_KEY_NAME,
	                                            NULL,
	                                            NULL);

	gchar * exec = g_key_file_get_locale_string(priv->keyfile,
	                                            groupheader,
	                                            G_KEY_FILE_DESKTOP_KEY_EXEC,
	                                            NULL,
	                                            NULL);

	/* Build a new desktop file with the name and exec in the desktop
	   group.  We have to do this with data as apparently there isn't
	   and add_group function in g_key_file.  Go figure. */
	gchar * desktopdata = g_strdup_printf("[" G_KEY_FILE_DESKTOP_GROUP "]\n"
	                                      G_KEY_FILE_DESKTOP_KEY_NAME "=\"%s\"\n"
	                                      G_KEY_FILE_DESKTOP_KEY_EXEC "=\"%s\"\n",
	                                      name, exec);
	

	g_free(name); g_free(exec);

	GKeyFile * launcher = g_key_file_new();
	g_key_file_load_from_data(launcher, desktopdata, -1, G_KEY_FILE_NONE, &error);
	g_free(desktopdata);

	if (error != NULL) {
		g_warning("Unable to build desktop keyfile for executing shortcut '%s': %s", nick, error->message);
		g_error_free(error);
		return FALSE;
	}

	GDesktopAppInfo * appinfo = g_desktop_app_info_new_from_keyfile(launcher);
	gboolean launched = g_app_info_launch(G_APP_INFO(appinfo), NULL, NULL, &error);

	if (error != NULL) {
		g_warning("Unable to launch file from nick '%s': %s", nick, error->message);
		g_error_free(error);
		g_key_file_free(launcher);
		return FALSE;
	}

	g_object_unref(appinfo);
	g_key_file_free(launcher);

	return launched;
}
