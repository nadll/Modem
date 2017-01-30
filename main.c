//############################################################################
// FILE :
//----------------------------------------------------------------------------
// DESC :
//----------------------------------------------------------------------------
// HIST : Version |   Date   | Author | Description
//        --------------------------------------------------------------------
//         01.01  | 30/01/17 |        | 
//############################################################################

/****************************************************************************/
/*                               INCLIB                                     */
/****************************************************************************/
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <syslog.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <pthread.h>
#include <getopt.h>
#include <signal.h>
#include <termios.h>
#include <sys/stat.h>
#include <fcntl.h>
/****************************************************************************/
/*                               INCUSER                                    */
/****************************************************************************/

/****************************************************************************/
/*                               DEFINITION                                 */
/****************************************************************************/
#define Du16WVersion_eVRefSize       10U
#define Du16WVersion_eVNameSize      20U
#define Du16WVersion_eVCopyRightSize 10U
#define Du16WVersion_eDateSize       12U
#define Du16WVersion_eTimeSize       10U
#define Du8Cmd_Timeout               5
/****************************************************************************/
/*                               MACRO                                      */
/****************************************************************************/
#define DBG(fmt, ...) \
do {                  \
     syslog(LOG_LOCAL0|LOG_DEBUG, __FILE__"[%s(l.%d)]: "fmt" ", __func__, __LINE__, ##__VA_ARGS__); \
} while(0)

#define ERROR(fmt, ...) \
do {                    \
     syslog(LOG_ERR, __FILE__"[%s(l.%d)]: "fmt" ", __func__, __LINE__, ##__VA_ARGS__); \
} while(0)

#define PT(fmt, ...) \
do                   \
{                    \
   fprintf(stdout, ""fmt"\n", ##__VA_ARGS__); \
   fflush(stdout);   \
} while (0)
/****************************************************************************/
/*                               TYPEDEF                                    */
/****************************************************************************/
typedef struct
{
   unsigned char u8V0;
   unsigned char u8V1;
   unsigned char u8V2;
   unsigned char u8V3;
   char tu8VRef[Du16WVersion_eVRefSize];
   char tu8VName[Du16WVersion_eVNameSize];
   char tu8VCopyRight[Du16WVersion_eVCopyRightSize];
   char tu8VDate[Du16WVersion_eDateSize];
   char tu8VTime[Du16WVersion_eTimeSize];
} TstVersion_eSWIdent;
/****************************************************************************/
/*                               PROTOTYPE                                  */
/****************************************************************************/
static int _configure_comm(char *port);
static int _write(int fd, char *buf, int length);
static int _send_command(int tty, char *cmd, char **result, int timeout);
static void _close_comm(int fd);
static void _display_help(char *exec_name);
/****************************************************************************/
/*                               CONST                                      */
/****************************************************************************/

/****************************************************************************/
/*                               EXPORT                                     */
/****************************************************************************/

/****************************************************************************/
/*                               INTERN                                     */
/****************************************************************************/

static struct option _long_options[] =
{
   /*    name            has_arg      flag   val */
   {    "port",   required_argument,  NULL,  'p'},
   { "command",   required_argument,  NULL,  'c'},
   {    "help",         no_argument,  NULL,  'h'},
   { "version",         no_argument,  NULL,  'v'},
   /* last element needs to be NULL */
   {      NULL,                   0,  NULL,   0 },
};

int main(int argc, char **argv)
{
   TstVersion_eSWIdent CstApp_eSWIdent =
     {
        1,
        0,
        1,
        0,
        "XXXX",
        "Test",
        "@ACTIA",
        __DATE__,
        __TIME__,
     };

   int fd = -1;
   int option_index = 0;
   int option;

   char *port = NULL;
   char *cmd = NULL;
   char *cmd_output = NULL;
   const char *opt_string = "p:c:vh";

   if(argc < 2)
     _display_help(argv[0]);

   while ((option = getopt_long(argc, argv, opt_string, _long_options, &option_index)) != -1)
     {
        switch (option)
          {
           case 'p' :
              port = strdup(optarg);
              break;
           case 'c' :
              cmd = strdup(optarg);
              break;
           case '?':
           case 'h' :
              _display_help(argv[0]);
              return EXIT_SUCCESS;
              break;
           case 'v':
              PT("%s V%d.%d.%d.%d built on %s-%s\r",
                            CstApp_eSWIdent.tu8VName,
                            CstApp_eSWIdent.u8V0,
                            CstApp_eSWIdent.u8V1,
                            CstApp_eSWIdent.u8V2,
                            CstApp_eSWIdent.u8V3,
                            CstApp_eSWIdent.tu8VDate,
                            CstApp_eSWIdent.tu8VTime);
              return EXIT_SUCCESS;
              break;
           default:
              break;
          }
     }

   /* print any remaining command line arguments */
   if (optind != argc)
     {
        PT("Remaining arguments:");
        while (optind != argc)
          PT("%s", argv[optind ++]);
        putchar ('\n');
     }

   fd = _configure_comm(port);
   _send_command(fd, cmd, &cmd_output, Du8Cmd_Timeout);
   _close_comm(fd);

   return 0;
}

/** --------------------------------------------------------------------------
 * @brief open & configure the communication with the module
 * @return the file descriptor
 */
static int
_configure_comm(char *port)
{
   int fd = -1;
   struct termios tty;

   memset(&tty, 0, sizeof(tty));

   fd = open(port, O_RDWR | O_NOCTTY);

   if (fd < 0)
     return (-1);

   if (tcgetattr(fd, &tty) != 0)
     {
        close(fd);
        perror("tcgetattr Get:");
        return (-1);
     }

   cfsetospeed (&tty, B115200);
   cfsetispeed (&tty, B115200);

   tty.c_cflag &= ~CSIZE;
   tty.c_cflag |=  CS8;              /* select 8 data bits */
   tty.c_cflag |= (CLOCAL | CREAD);  /* enable receiver and set local mode */
   tty.c_cflag &= ~(PARENB | PARODD);/* clear parity enable, Turn off odd parity = even */
   tty.c_cflag &= ~CSTOPB;           /* not 2 stop bits */
   tty.c_cflag &= ~CRTSCTS;          /* disable hw flow control */
   tty.c_cflag |= 0;
   tty.c_iflag &= ~(IXON | IXOFF | IXANY); /* turn off s/w flow control */
   tty.c_lflag = 0;
   tty.c_oflag = 0;

   /* set the minimum number of characters to receive before satisfying the
   * non-canonical read - 0 read doesn't block */
   tty.c_cc[VMIN]  = 0;
   /* set the character timer -> 1 seconds read timeout (t = TIME *0.1 s) */
   tty.c_cc[VTIME] = 10; 

   tcflush(fd, TCIFLUSH);

   /* update the port settings and do it NOW */
   if (tcsetattr(fd, TCSANOW, &tty) != 0)
     {
        close(fd);
        perror("tcgetattr Update :");
        return (-1);
     }
   return (fd);
}

/** ----------------------------------------------------------------------------
 * @brief write data on specified fd
 * @param fd : file descriptor used to send cmd
 * @param buf : command
 * @param length : command's size
 * @return 0 if everything has been written
 */
static int
_write(int fd, char *buf, int length)
{
   int len = write(fd, buf, length);

   tcdrain(fd);
   return (len == length);
}

/** ----------------------------------------------------------------------------
 * @brief send AT command to the modem and parse its output
 * @param tty : File descriptor on which send the command
 * @param cmd : AT command
 * @param result : Buffer in which result will be stored
 * @param it needs to be freed in the caller
 * @param timeout : Command's timeout
 * @return 4 if operation succeed, negative value otherwise
 */
static int
_send_command(int tty, char *cmd, char **result, int timeout)
{
   int i, j, k;
   int state = -10;
   int length, size;
   int cmd_len;
   char vc_value;
   char buffer[128];
   char buffer_cmd[128];
   char *pos, *pos1, *pos2;
   char *chr_null = "";
   struct timespec req;

   memset(buffer_cmd, 0, sizeof(buffer_cmd));
   if (cmd != NULL)
     {
        cmd_len = strlen(cmd);
        memcpy(buffer_cmd, cmd, cmd_len);
        strcat(buffer_cmd, "\r");
        if (_write(tty, buffer_cmd, cmd_len + 1))
          {
             /* wait 500ms for message to be ready */
             req.tv_sec = 0;
             req.tv_nsec = 500000000; 
             nanosleep(&req, NULL);
             i = timeout * 10;
             while (i > 0)
               {
                  pos  = NULL;
                  pos1 = NULL;
                  pos2 = NULL;
                  length = 0;
                  size = 0;
                  memset(buffer, 0, sizeof(buffer));
                  length = read(tty, &buffer[size], sizeof(buffer));
                  if (length > 0)
                    {
                       for (j = size; j < (size + length); j++)
                         if(buffer[j] == 0)
                           buffer[j] = ' ';
                       size += length;
                       buffer[size] = 0;
                       i--;

                       if (result != NULL)
                         *result = malloc(length * sizeof(char*));
                       pos = strstr(buffer, cmd);
                       if (pos != NULL)
                         {
                            state = -9;
                            pos += cmd_len + 1;
                            pos1 = strstr(buffer + cmd_len + 1, "OK");
                            if (pos1 != NULL)
                              {
                                 state = 4;
                                 i = 0;
                                 /* check if command copy */
                                 if (result != NULL)
                                   {
                                      if ((pos2 = strstr(pos, &cmd[2])))
                                        {
                                           /* remove the second echo of part of the command */
                                           /* commandlen - "AT"  + ":" */
                                           pos = pos2 + (cmd_len - 1);
                                        }
                                      k = 0;
                                      for (j = 0; j < (pos1 - pos) ; j++)
                                        {
                                           if (pos[j] > ' ')
                                             (*result)[k++] = pos[j];
                                        }
                                      (*result)[k++] = 0;
                                   }
                                   break;
                              }
                            else
                              {
                                 pos1 = strstr(pos, "+CME ERROR");
                                 if (pos1 != NULL)
                                   {
                                      i = 0;
                                      if (result != NULL)
                                        {
                                           /* operation on buffer[] */
                                           pos2 = strstr(buffer, "+CME ERROR");
                                           if (pos2 != NULL)
                                             {
                                                /* get string message error */
                                                for (j = 0; j < size; j++)
                                                  {
                                                     /*10: strlen("+CME ERROR")*/
                                                     vc_value = pos2[10 + j];
                                                     /* no "Carriage Return", no "Line feed" and no "character null" */
                                                     if ((vc_value != '\r') 
                                                        && (vc_value != '\n')
                                                        && (vc_value != 0x00))
                                                       {
                                                          (*result)[j] = vc_value;
                                                       }
                                                     else
                                                       {
                                                          (*result)[j] = 0; /* end of string */
                                                          state = -2;
                                                          j = size + 1; /* go out */
                                                       }
                                                  }
                                           }
                                           break;
                                        }
                                      else
                                        {
                                           state = -2;
                                        }
                                   }
                                 else
                                   {
                                      pos1 = strstr(pos, "ERROR");
                                      if (pos1 != NULL)
                                        {
                                           state = -1;
                                           i = 0;
                                           if (result != NULL)
                                             {
                                                if ((pos2 = strstr(pos, &cmd[2])))
                                                  {
                                                     /* remove the second echo of part of the command */
                                                     pos = pos2 + (cmd_len - 1);
                                                  }
                                                k = 0;
                                                for (j = 0; j < (pos1 - pos) ; j++)
                                                  {
                                                    if (pos[j] > ' ')
                                                      (*result)[k++] = pos[j];
                                                  }
                                                (*result)[k] = 0;
                                                break;
                                             }
                                        }
                                      else
                                        {
                                           k = 0;
                                           if ((result != NULL)  && (i <= 0))
                                             {
                                                for (j = 0; j < (size) ; j++)
                                                  (*result)[k++] = buffer[j];
                                                (*result)[k] = 0;
                                             }
                                        }
                                      ERROR("Error unexpected result");
                                      break;
                                   }
                              }
                         }
                       else
                         {
                            ERROR("Error missing string");
                            break;
                         }
                    }
                  else
                    {
                       ERROR("Error while reading %d", length);
                       i -= 10;
                       req.tv_sec = 0;
                       req.tv_nsec = 100000000;
                       nanosleep(&req, NULL);
                    }
               }
               DBG("Cmd status=> %d", state);
          }
        else
          {
             ERROR("Error write()");
          }
     }
   else
     {
        cmd = chr_null;
        cmd_len = 0;
     }
   return(state);
}

/** ----------------------------------------------------------------------------
 * @brief close the communication
 * @return
 */
static void 
_close_comm(int fd)
{
   tcflush(fd, TCIOFLUSH);
   close(fd);
   fd = -1;
}

/** --------------------------------------------------------------------------
 * @brief diplay help
 * @param exec_name
 * @return
 */
static void
_display_help(char *exec_name)
{
   PT("Syntax : %s [options]\n", exec_name);
   PT("Options :\n");
   PT("--port    --p Port on which command will be sent");
   PT("--config  --c Command");
   PT("--version --v Report the sw version");
   PT("--help    --h Return this message");
}