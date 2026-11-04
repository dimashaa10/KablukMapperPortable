/*
 * Kabluk Mapper Portable - prefs.h
 *
 * The editor settings, kept in a small file next to the EBOOT (kabluk.cfg) so
 * that picking a wad, a folder, a map name and the floor / ceiling textures
 * survives a restart. Until now every start began with "not set" and the same
 * four choices had to be made again.
 *
 * The format is one "key=value" line per setting, '#' starts a comment, and
 * anything the reader does not recognise is ignored - so an older or newer
 * file never stops the editor from starting. Values are taken as they are
 * (a path may hold spaces, ':' or '='), trimmed of trailing whitespace.
 */
#ifndef KMP_PREFS_H
#define KMP_PREFS_H

#include "settings.h"

/* The file the settings live in: "kabluk.cfg", in the working directory -
 * which is the EBOOT's folder on the PSP (main.c chdir()s there). */
#define PREFS_FILE_NAME "kabluk.cfg"

/* Reads the file into 'settings'. Fields the file does not mention are left
 * alone, so call it after settings_defaults(). Returns the number of settings
 * it set, 0 when there is no readable file. */
int prefs_load(Settings *settings);

/* Writes all of them. Returns 1 on success. A failure is not fatal: the
 * editor keeps working with what it has in memory. */
int prefs_save(const Settings *settings);

/* 1 when the two are the same settings - the app uses it to save only when
 * something actually changed. */
int prefs_same(const Settings *a, const Settings *b);

#endif /* KMP_PREFS_H */
