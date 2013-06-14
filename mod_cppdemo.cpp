/**
 * ProFTPD - sample/proof-of-concept C++ module
 * Copyright (c) 2008 Juha Ranta <jmtr@iki.fi>
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307, USA.
 *
 * As a special exemption, Juha Ranta and other respective copyright holders
 * give permission to link this program with OpenSSL, and distribute the
 * resulting executable, without including the source code for OpenSSL in the
 * source distribution.
 */

#include <iostream>
#include <sstream>
#include <map>
#include <string>

#include "mod_cppdemo.h"

#define MOD_CPPDEMO_VERSION		"mod_cppdemo/0.1"

extern "C" module cppdemo_module;
extern pr_response_t *resp_list, *resp_err_list;

#define FTP_CMD_NAME(cmdrec) (cmdrec ? (cmdrec->argv[0] ? cmdrec->argv[0] : "") : "")
#define FTP_CMD_ARG(cmdrec) (cmdrec ? (cmdrec->arg ? cmdrec->arg : "") : "")
#define FOREACH(type, iter, container) for(type iter = (container).begin(); iter != (container).end(); iter++)

typedef std::map<std::string,int> cmd_statistics_t;
static cmd_statistics_t demo_commands;
static time_t demo_time_module_init  = 0;
static time_t demo_time_session_init = 0;

// ----------------------------------------------------------------
// Some sample/dummy utilities
// ----------------------------------------------------------------

int demo_send_stream_data(const std::string& name, std::istream& input) {
  char buffer[1024];

  // open data connection
  if (pr_data_open(NULL, (char*)name.c_str(), PR_NETIO_IO_WR, 0) < 0) {
    return -1;
  }
  session.sf_flags |= SF_ASCII_OVERRIDE;        
    
  int errors = 0;

  // send data
  try {
    while(!input.eof() &&
          input.good()) {
      input.read(buffer, sizeof(buffer));        
      int rv = pr_data_xfer(buffer, input.gcount());
      if (rv < 0 &&
          errno != 0) {
        // data transfer failed..
        errors++;
        break;
      }
    }

  } catch(...) {
    // oops..
    errors++;
    pr_data_abort(0, 0);
  }
    
  // close data connection
  pr_data_close(FALSE);
    
  return errors;
}

// ----------------------------------------------------------------
// ProFTPD module handlers
// ----------------------------------------------------------------

/* PRE_CMD handler
 */
MODRET demo_pre_cmd(cmd_rec *cmd) {
  pr_log_debug(DEBUG5, "cppdemo_pre_cmd(%s, %s)", cmd->argv[0], cmd->arg);

  // collect some statistics
  std::string ftp_cmd = FTP_CMD_NAME(cmd);
  demo_commands[ftp_cmd]++;

  // by default, decline
  return PR_DECLINED(cmd);
}

/* CMD handler
 */
MODRET demo_cmd(cmd_rec *cmd) {
  pr_log_debug(DEBUG5, "cppdemo_cmd(%s, %s)", cmd->argv[0], cmd->arg);

  // trap 'LIST' for 'mod_cppdemo::statistics' 
  std::string ftp_cmd = FTP_CMD_NAME(cmd);
  std::string ftp_arg = FTP_CMD_ARG(cmd);

  // Some dummy/sample functionality ...
  // 
  // samples actions
  // 
  // LIST mod_cppdemo::commands displays information commands issued in
  // this FTP session..
  //
  // LIST mod_cppdemo::info displays information commands 
  // 
  if (ftp_cmd == "LIST" || ftp_cmd == "NLST") {
    std::stringstream tmp;
    int res = 0;
    if (ftp_arg == "mod_cppdemo::commands") {
      FOREACH(cmd_statistics_t::const_iterator, i, demo_commands) {
        std::string cmd = i->first;
        int times = i->second;
        tmp << "FTP-Command '" << cmd << "' executed " << times << " times." << std::endl;
      }       

      res = demo_send_stream_data("mod_cppdemo::commands", tmp);
      return (res ? PR_ERROR(cmd) : PR_HANDLED(cmd));
    } 

    if (ftp_arg == "mod_cppdemo::info") {
      time_t now = time(NULL);
      time_t secs_module  = now - demo_time_module_init;
      time_t secs_session = now - demo_time_session_init;
      tmp << "FTP-Session inited " << secs_session << " seconds ago." << std::endl;
      tmp << "FTP-Module inited " << secs_module << " seconds ago." << std::endl;

      res = demo_send_stream_data("mod_cppdemo::info", tmp);
      return (res ? PR_ERROR(cmd) : PR_HANDLED(cmd));
    }

    if (ftp_arg.find("mod_cppdemo") == 0) {
      tmp << "mod_cppdemo::commands" << std::endl;
      tmp << "mod_cppdemo::info" << std::endl;

      res = demo_send_stream_data("mod_cppdemo", tmp);
      return (res ? PR_ERROR(cmd) : PR_HANDLED(cmd));
    }
  }

  // by default, decline
  return PR_DECLINED(cmd);
}

/* POST_CMD handler
 */
MODRET demo_post_cmd(cmd_rec *cmd) {
  pr_log_debug(DEBUG5, "cppdemo_post(%s, %s)", cmd->argv[0], cmd->arg);

  // by default, decline
  return PR_DECLINED(cmd);
}

/* LOG handler 
 */
MODRET demo_log_handle(cmd_rec *cmd, const std::string& type) {
  pr_log_debug(DEBUG9, "cppdemo_log(cmd, %s)", type.c_str());

  std::string command = (cmd->argv != NULL && cmd->argv[0] != NULL) ? cmd->argv[0] : "";
  std::string argument = cmd->arg != NULL ? cmd->arg : "";

  // collect all errors...
  return PR_DECLINED(cmd);
}

/* Some, but not all, 'ok'-actions should be logged
 */
MODRET demo_log_cmd(cmd_rec *cmd) {
  return demo_log_handle(cmd, "LOG_CMD");
}

/* All errors should be logged
 */
MODRET demo_log_cmd_err(cmd_rec *cmd) {
  return demo_log_handle(cmd, "LOG_CMD_ERR");
}

/* Event handlers
 */
static void demo_event_module_core_exit(const void *event_data,
    void *user_data) {
  pr_log_debug(DEBUG3, "demo_event_module_core_exit()");

  // cleanup here..
  return;
}

static void demo_event_session_core_exit(const void *event_data,
    void *user_data) {     
  pr_log_debug(DEBUG3, "demo_event_session_core_exit()");

  // cleanup here..
  return;
}

/* Initialization routines - Module init
 */
static int demo_init(void) {
  pr_log_debug(DEBUG3, "demo_init()");

  // register cleanup handler - cleanup function for initializations made in
  // this function (for this module)

  pr_event_register(&cppdemo_module, "core.shutdown",
    demo_event_module_core_exit, NULL);
  demo_time_module_init = time(NULL);

  return 0;
}

/* Session init
 */
static int demo_sess_init(void) {

  // register cleanup handler - cleanup function for initializations made in
  // this function (for this session)
  pr_event_register(&cppdemo_module, "core.exit", demo_event_session_core_exit,
    NULL);
  demo_time_session_init = time(NULL);

  return 0;
}

MODRET set_cppdemoconfig(cmd_rec *cmd) {
  int b = 1;
  config_rec *c = NULL;

  pr_log_debug(DEBUG3, "set_cppdemoconfig()");

  // Do something with config directive....
  return HANDLED(cmd);
}

/* Config parameters for this module
 */
static conftable demo_conftab[] = {
  { "CPPDemoConfig",	set_cppdemoconfig,	NULL },
  { NULL }
};

/* The command handler table:
 * first  : command "type" (see the doc/API for more info)
 *
 * second : command "name", or the actual null-terminated ascii text
 *          sent by a client (in uppercase) for this command.  see
 *          include/ftp.h for macros which define all rfced FTP protocol
 *          commands.  Can also be the special macro C_ANY, which receives
 *          ALL commands.
 *
 * third  : command "group" (used for access control via Limit directives),
 *          this can be either G_DIRS (for commands related to directory
 *          listing), G_READ (for commands related to reading files), 
 *          G_WRITE (for commands related to file writing), or the
 *          special G_NONE for those commands against which the
 *          special <Limit READ|WRITE|DIRS> will not be applied.
 *
 * fourth : function pointer to your handler
 *
 * fifth  : TRUE if the command cannot be used before authentication
 *          (via USER/PASS), otherwise FALSE.
 *
 * sixth  : TRUE if the command can be sent during a file transfer
 *          (note: as of 1.1.5, this is obsolete)
 *
 */

static cmdtable demo_cmdtab[] = {
  { PRE_CMD,	  C_ANY,  G_NONE, demo_pre_cmd,     FALSE, FALSE },
  { CMD,	  C_ANY,  G_NONE, demo_cmd,         FALSE, FALSE },
  { POST_CMD,	  C_ANY,  G_NONE, demo_post_cmd,    FALSE, FALSE },
  { LOG_CMD_ERR,  C_ANY,  G_NONE, demo_log_cmd_err, FALSE, FALSE },
  { LOG_CMD,      C_PASS, G_NONE, demo_log_cmd,     FALSE, FALSE },
  { 0, NULL }
};

extern "C" {
  module cppdemo_module = {

    /* Always NULL */
    NULL, NULL,

    /* Module API version (2.0) */
    0x20,

    /* Module name */
    "cppdemo",

    /* Module configuration directive handlers */
    demo_conftab,

    /* Module command handlers */
    demo_cmdtab,

    /* Module authentication handlers (none in this case) */
    NULL,

    /* Module initialization */
    demo_init,

    /* Session initialization */
    demo_sess_init,

    /* Module version */
    MOD_CPPDEMO_VERSION
  };
};

