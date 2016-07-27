/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 2010-2015 Richard Hughes <richard@hughsie.com>
 * Copyright (C) 2016 Colin Walters <walters@verbum.org>
 *
 * Licensed under the GNU General Public License Version 2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 *(at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"

#include <gio/gio.h>
#include <locale.h>
#include <stdlib.h>
#include <string.h>

#include <libhif/libhif.h>

static gboolean opt_version;
static gboolean opt_yes = TRUE;

static GOptionEntry global_entries[] = {
  { "version", 0, 0, G_OPTION_ARG_NONE, &opt_version, "Print version information and exit", NULL },
  { "yes", 'y', 0, G_OPTION_ARG_NONE, &opt_yes, "Does nothing, we always assume yes", NULL },
  { NULL }
};

static int builtin_install (int argc, char **argv, GCancellable *cancellable, GError **error);

typedef struct {
  const char *name;
  int (*fn) (int argc, char **argv, GCancellable *cancellable, GError **error);
} Command;

static Command supported_commands[] = {
  { "install", builtin_install },
  { NULL }
};

static gboolean
option_context_parse (GOptionContext *context,
                      const GOptionEntry *main_entries,
                      int *argc,
                      char ***argv,
                      GCancellable *cancellable,
                      GError **error)
{
  if (main_entries != NULL)
    g_option_context_add_main_entries (context, main_entries, NULL);
  
  g_option_context_add_main_entries (context, global_entries, NULL);
  
  if (!g_option_context_parse (context, argc, argv, error))
      return FALSE;

  if (opt_version)
    {
      g_print ("%s\n", PACKAGE_STRING);
      exit (EXIT_SUCCESS);
    }
  return TRUE;
}

static gint
pkg_array_compare (HifPackage **p_pkg1,
                   HifPackage **p_pkg2)
{
  return hif_package_cmp (*p_pkg1, *p_pkg2);
}

static void
print_transaction (HifContext   *hifctx)
{
  guint i;
  GPtrArray *install = NULL;

  install = hif_goal_get_packages (hif_context_get_goal (hifctx),
                                   HIF_PACKAGE_INFO_INSTALL,
                                   HIF_PACKAGE_INFO_REINSTALL,
                                   HIF_PACKAGE_INFO_DOWNGRADE,
                                   HIF_PACKAGE_INFO_UPDATE,
                                   -1);

  g_print ("Transaction: %u packages\n", install->len);
  
  if (install->len == 0)
    g_print ("  (empty)\n");
  else
    {
      g_ptr_array_sort (install, (GCompareFunc) pkg_array_compare);

      for (i = 0; i < install->len; i++)
        {
          HifPackage *pkg = install->pdata[i];
          g_print ("  %s (%s)\n", hif_package_get_nevra (pkg), hif_package_get_reponame (pkg));
        }
    }
  g_ptr_array_unref (install);
}

static HifContext *
context_new (void)
{
  HifContext *ctx = hif_context_new ();

  hif_context_set_repo_dir (ctx, "/etc/yum.repos.d/");
#define CACHEDIR "/var/cache/yum"
  hif_context_set_cache_dir (ctx, CACHEDIR "/metadata");
  hif_context_set_solv_dir (ctx, CACHEDIR "/solv");
  hif_context_set_lock_dir (ctx, CACHEDIR "/lock");
#undef CACHEDIR
  hif_context_set_check_disk_space (ctx, FALSE);
  hif_context_set_check_transaction (ctx, TRUE);
  hif_context_set_keep_cache (ctx, FALSE);
  hif_context_set_cache_age (ctx, 0);
  hif_context_set_yumdb_enabled (ctx, FALSE);
  return ctx;
}

static int
builtin_install (int argc, char **argv, GCancellable *cancellable, GError **error)
{
  guint i;
  g_autoptr(GOptionContext) option_context = NULL;
  g_autoptr(HifContext) ctx = context_new ();

  option_context = g_option_context_new ("[PKG...] - Install");
  if (!option_context_parse (option_context, NULL, &argc, &argv, cancellable, error))
    return FALSE;

  if (argc <= 1)
    {
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                           "Not enough arguments, "
                           "expected package or group name");
      return FALSE;
    }

  /* install each package */
  if (!hif_context_setup(ctx, NULL, error))
    return FALSE;
  for (i = 1; i < argc; i++)
    {
      if (!hif_context_install(ctx, argv[i], error))
        return FALSE;
    }
  if (!hif_goal_depsolve (hif_context_get_goal (ctx), HIF_INSTALL, error))
    return FALSE;
  print_transaction (ctx);
  if (!hif_context_run (ctx, NULL, error))
    return FALSE;
  g_print ("Complete.\n");
  return TRUE;
}

static Command *
lookup_command_of_type (Command *commands,
                        const char *name)
{
  Command *command = commands;

  while (command->name) {
      if (g_strcmp0 (name, command->name) == 0)
          return command;
      command++;
  }

  return NULL;
}

static GOptionContext *
option_context_new_with_commands (void)
{
  Command *command = supported_commands;
  GOptionContext *context;
  GString *summary;

  context = g_option_context_new ("COMMAND");

  summary = g_string_new ("Builtin Commands:");

  while (command->name != NULL)
    {
      g_string_append_printf (summary, "\n  %s", command->name);
      command++;
    }

  g_option_context_set_summary (context, summary->str);

  g_string_free (summary, TRUE);

  return context;
}

/**
 * main:
 **/
int
main(int argc, char *argv[])
{
  const char *command_name = NULL;
  guint retval = 1;
  Command *command;
  int in, out;
  g_autoptr(GOptionContext) option_context = NULL;
  g_autoptr(GCancellable) cancellable = NULL;
  g_autoptr(GError) local_error = NULL;
  GError **error = &local_error;

  setlocale(LC_ALL, "");

  /*
   * Parse the global options. We rearrange the options as
   * necessary, in order to pass relevant options through
   * to the commands, but also have them take effect globally.
   */
  for (in = 1, out = 1; in < argc; in++, out++)
    {
      /* The non-option is the command, take it out of the arguments */
      if (argv[in][0] != '-')
        {
          if (command_name == NULL) {
            command_name = argv[in];
            out--;
            continue;
          }
        }
      else if (g_str_equal (argv[in], "--"))
        {
          break;
        }
        
      argv[out] = argv[in];
    }
  
  argc = out;

  command = lookup_command_of_type (supported_commands, command_name);

  if (!command)
    {
      g_autofree char *help = NULL;

      option_context = option_context_new_with_commands ();

      /* This will not return for some options (e.g. --version). */
      (void) option_context_parse (option_context, global_entries, &argc, &argv,
                                   NULL, NULL);
      if (command_name == NULL)
        {
          local_error = g_error_new_literal (G_IO_ERROR, G_IO_ERROR_FAILED,
                                             "No command specified");
        }
      else
        {
          local_error = g_error_new (G_IO_ERROR, G_IO_ERROR_FAILED,
                                     "Unknown command '%s'", command_name);
        }

      help = g_option_context_get_help (option_context, FALSE, NULL);
      g_printerr ("This is micro-yuminst, which implements a subset of `yum`.\n"
                  "For more information, see: https://github.com/cgwalters/micro-yuminst\n"
                  "%s", help);
      retval = 0;
      goto out;
    }

  if (!command->fn (argc, argv, cancellable, error))
    goto out;

  /* success */
  retval = 0;
 out:
  if (local_error != NULL)
    {
      int is_tty = isatty (1);
      const char *prefix = "";
      const char *suffix = "";
      if (is_tty)
        {
          prefix = "\x1b[31m\x1b[1m"; /* red, bold */
          suffix = "\x1b[22m\x1b[0m"; /* bold off, color reset */
        }
      g_printerr ("%serror: %s%s\n", prefix, suffix, local_error->message);
      g_error_free (local_error);
      return 1;
    }
  return retval;
}

