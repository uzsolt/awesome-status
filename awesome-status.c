/*
 * awesome-status.c
 *
 * Copyright 2008 Zsolt Udvari <udvzsolt@gmail.com>
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#define _GNU_SOURCE
#include <locale.h>
#include <stdio.h>
#include <confuse.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/statfs.h>
#include <sys/types.h>
#include <fcntl.h>
#include <utime.h>

#include "parseconfig.h"
#include "version.h"

#include <time.h>

#include <langinfo.h>
#include <iconv.h>

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <sys/socket.h>
#include <sys/un.h>
#include <netdb.h>
#include <libgen.h>

#ifdef PLUGIN_SOUND
#include <asoundlib.h>
#include <mixer.h>
#endif

#ifdef PLUGIN_NEWS
#include <sqlite3.h>
#endif

#define p_delete(mem_pp)                           \
    do {                                           \
        typeof(**(mem_pp)) **__ptr = (mem_pp);     \
        free(*__ptr);                              \
        *__ptr = NULL;                             \
    } while(0)


/// Search and Replace SubString
char * srss (const char * source , const char * from , const char * to) {
    char * result , *prev, *tmp , *begin;

    int num=0 , ln_from = strlen(from) , ln_to = strlen(to);

    for ( result = strstr(source,from) ; result!=NULL ; result=strstr(result,from) ) {
        result+=ln_from;
        num++;
    }

    // ln_to-ln_from : additional size of "one replace" (maybe negative if "from" is shorter than "to")
    // +1: because of the last \0 char
    result = (char*)malloc(strlen(source)+(ln_to-ln_from)*num+1);
    if (result==NULL) return result;
    result[0]=0;

    begin = prev = strdup(source);
    for ( tmp = strstr(prev,from) ; tmp!=NULL ; tmp = strstr(prev,from) ) {    
        tmp[0]='\0';
        if (prev[0]!='\0') { strcat(result,prev); }
        strcat(result,to);
        tmp+=ln_from;
        prev=tmp;
    }    
    strcat(result,prev);
    free(begin); // malloc by strdup

    return result;
}

/// Search and Replace SubString In Place
void srssip (char **source , const char *from , const char *to) {
    char *result;

    result = srss(*source,from,to);
    free(*source);
    *source = result;

}

aws_config config;

struct sockaddr_un *addr;
int csfd;

int output = 0;


#define OP(x) while ( (x[0]!='+') && (x[0]!='-') && (x[0]!='*') && (x[0]!='/') && (x[0]!='\0') ) { x++; }
#define DELSPACE(x) while (x[0]==' ') {x++;}

double remain(float a , float b) {
    while (a>b) a-=b;
    return a;
}

/// The built-in postfix calculator
double calc (char *line) {

    double result , *stack;
    int deep=0; // stack
    char *end , *prev;

    stack = NULL;
    
    end = prev = line;
    while (strlen(end)!=0) {
        result = strtod(prev,&end);
        if (prev == end) { // doesn't find correct number
            if (deep<2) {
                printf("Not enough number: %s\n",end);
                return -1;
            }
            OP(end);
            switch (end[0]) {
                case '+': result = stack[deep-2]+stack[deep-1];
                          break;
                case '-': result = stack[deep-2]-stack[deep-1];
                          break;
                case '*': result = stack[deep-2]*stack[deep-1];
                          break;
                case '/': if (stack[deep-1]!=0) { // can't divide with zero
                              result = stack[deep-2]/stack[deep-1];
                          } else {
                              result = 0; 
                          }
                          break;
                default:
                          printf("Not enough operand.\n");
                          return -1;
            }
            end++;
            DELSPACE(end);
            deep--;
            stack[deep-1]=result;
            prev = end;
        } else { // find a number so push it to stack
            stack = realloc(stack,(++deep)*sizeof(double));
            stack[deep-1]=result;
            prev = end;
        }
    }

    result = stack[0];
    free(stack);
    return result;

}

/// Frees the dupled values
void values_free(aws_val *values , int values_nr) {
    int i;

    for (i=0;i<values_nr;i++) {
        free(values[i].name);
        free(values[i].value);
    }
    free(values);
}

/// Duples the values (like strdup)
aws_val *values_dup(aws_val *values , int values_nr) {
    aws_val *result;
    int i;

    result = malloc(sizeof(aws_val)*values_nr);
    for (i=0;i<values_nr;i++) {
        result[i].name = strdup(values[i].name);
        result[i].value = strdup(values[i].value);
    }
    return result;
}
        
/// Replace in all values
void replace_in_value(aws_val *values , int values_nr , char *from, char *to) {
    int i;

    for ( i=0; i<values_nr; i++ ) {
        srssip(&(values[i].value),from,to);
    }
}

/// Calculate the value and replace
void calc_and_replace(char **in , aws_val *values , int values_nr , char *formatstr) {
    int i;
    char *tmp;

    tmp = malloc(30);
    for (i=0;i<values_nr;i++) {
        sprintf(tmp,formatstr,calc(values[i].value));
        srssip(in,values[i].name,tmp);
    }
    free(tmp);
}

/// Send message
static int send_msg(char *msg, ssize_t msg_len) { 
    
    int ret_value = EXIT_SUCCESS;

    if(!addr || csfd < 0) {
        return EXIT_FAILURE;
    }    

    if(sendto(csfd, msg, msg_len, MSG_NOSIGNAL,
              (const struct sockaddr *) addr, sizeof(struct sockaddr_un)) == -1)
    {
        switch (errno)
        {
          case ENOENT:
              printf("Can't write to %s\n", addr->sun_path);
              break;
          default:
              perror("error sending datagram");
         }
         ret_value = errno;
    }

    return ret_value;
}


char *multiple_message;


#define LENGTH_MULTIPLE_MESSAGE 1400

/// Widget flush...
static void widget_flush () {
    if (output%2 == 0) {
        send_msg(multiple_message,LENGTH_MULTIPLE_MESSAGE);
    }
    if (output>0) {
        printf("%s",multiple_message);
    }
    multiple_message[0]=0;
}    


/// Widget tell...
void widget_tell(aws_msg data , char* msg) {
    char *message;
    char *screen;
    message = strdup(data.message_format); // malloc
    srssip(&message,"$sb_name",data.sb_name);
    srssip(&message,"$prop",data.widget_command);
    srssip(&message,"$widget_name",data.widget_name);
    srssip(&message,"$msg",msg);
    screen=malloc(10); sprintf(screen,"%d",data.screen);
    srssip(&message,"$screen",screen);
    free(screen);
    strcat(multiple_message,message);
    strcat(multiple_message,"\n");
    if (strlen(multiple_message)>900) widget_flush();
    free(message);
}


#ifdef PLUGIN_MPD


int mpd_socket;

int cnt_mpd_messages;
char *codepage;


/// connect mpd
int connect_mpd () {
    struct hostent* p_host;
    struct in_addr* p_ipaddr;
    int result;
    struct sockaddr_in addr;
    char *tmp;

    p_host = gethostbyname(config.mpd.host);
    if (p_host==NULL) {
        fprintf(stderr,"Couldn't create socket for mpd!\n");
        return -1;
    }    

    p_ipaddr=(struct in_addr*)(p_host->h_addr);

    result=socket(PF_INET,SOCK_STREAM,0);
    if (result<0) {
        fprintf(stderr,"Error in socket (mpd)\n");
        return -1;
    }    

    memset(&addr,0,sizeof(addr));
    addr.sin_family=AF_INET;
    addr.sin_port=htons(config.mpd.port);
    addr.sin_addr=*p_ipaddr;

    if (connect(result,(struct sockaddr*)&addr,sizeof(addr))==0) {
        tmp=malloc(500);
        read(result,tmp,500);
        free(tmp);
        return result;
    } else {
        return -1;
    }    
}    

#define MPD_STOP 0
#define MPD_PLAY 1
#define MPD_PAUSE 2

char *INSET = "UTF-8";

char * convert2iso (char * euc) {
  unsigned start_len;
  static iconv_t euc2utf8t;
  int init = 0;
  size_t iconv_value;
  char * utf8, * utf8start;
  unsigned len, utf8len, utf8lenstart;
  int v;

  if (init == 0)
    {
      init = 1;
      euc2utf8t = iconv_open (codepage, INSET);
      if (euc2utf8t == (iconv_t) -1)
        {
          return NULL;
        }
    }
  start_len = len  = strlen (euc);
  if (!len)
    {
    return NULL;
    }
  utf8lenstart = utf8len = (3*len)/2 + 1;
  utf8start =   utf8 = (char *) calloc (utf8len, 1);
  iconv_value = iconv (euc2utf8t, & euc, & len, & utf8, & utf8len);
  if (iconv_value == (size_t) -1)
    {
      printf ("failed: in string '%s', length %d, "
              "out string '%s', length %d\n",
              euc, len, utf8start, utf8len);
      switch (errno)
        {
        case EILSEQ:
          printf ("Invalid character sequence\n");
          break;
        case EINVAL:
          printf ("EINVAL\n");
          break;
        case E2BIG:
          printf ("E2BIG\n");
          break;
        default:
          printf ("unknown error");
        }
    }
  v = iconv_close (euc2utf8t);
  return utf8start;
}



void print_mpd () {
    char *tmp;
    char *status;
    char *mpd_msg;
    int mpd_status;

    mpd_msg = (char*) malloc (300);

    if (mpd_socket<0) return;

    tmp=malloc(1000);
    write(mpd_socket,"status\n",7);
    read(mpd_socket,tmp,500);
    
    status=strstr(tmp,"state: ");
    status+=7;

    if ( (strncmp(status,"stop",4))==0 ) {
        mpd_status=MPD_STOP;
    } else if ( (strncmp(status,"pause",5)==0) ) {
        mpd_status=MPD_PAUSE;
    } else {
        mpd_status=MPD_PLAY;    
    }    

    long int current, total;
    char *work;

    if (mpd_status != MPD_STOP) {
        // time information
        status=strstr(tmp,"time: ");
        status+=6;
        work=strstr(status,"\n");
        work[0]='\0';
        sscanf(status,"%ld:%ld",&current,&total);
    } else { current = 0; total = 1; }    

    // song information
    write(mpd_socket,"currentsong\n",12);
    read(mpd_socket,tmp,1000);
    char *artist    = (char*)malloc(100);
    char *title    = (char*)malloc(100);
    char *album    = (char*)malloc(100);
    char *filename    = (char*)malloc(300);
    char *year    = (char*)malloc(6);
    int no_info=0;

    if (!sscanf(tmp,"file: %[^\n]",filename)) {filename[0]=0;}
    if (!sscanf(tmp,"%*[^\n]\n%*[^\n]\nArtist: %[^\n]",artist)) {artist[0]=0; no_info+=1;}
    if (!sscanf(tmp,"%*[^\n]\n%*[^\n]\n%*[^\n]\nTitle: %[^\n]",title)) {title[0]=0; no_info+=2;}
    if (!sscanf(tmp,"%*[^\n]\n%*[^\n]\n%*[^\n]\n%*[^\n]\nAlbum: %[^\n]",album)) {album[0]=0; no_info+=4;}
    if (!sscanf(tmp,"%*[^\n]\n%*[^\n]\n%*[^\n]\n%*[^\n]\n%*[^\n]\nDate: %s",year)) {year[0]=0; no_info+=8;}

    int i, j;
    for (i=0 ; i<config.mpd.msg_nr ; i++) {
        if (no_info&1) { strcpy(artist,config.mpd.msg[i].msg_ukartist); }
        if (no_info&2) { strcpy(artist,config.mpd.msg[i].msg_uktitle); }
        for (j=0 ; j<config.mpd.msg[i].msg_nr ; j++) {
            if (mpd_status == MPD_STOP) {
                strcpy(mpd_msg,config.mpd.msg[i].msg_stop);
            } else if ( (no_info&1) && (no_info&2) ) {
                strcpy(mpd_msg,config.mpd.msg[i].msg_fallback);
            } else if (mpd_status == MPD_PAUSE) {
                strcpy(mpd_msg,config.mpd.msg[i].msg_pause);
            } else if (mpd_status == MPD_PLAY) {
                strcpy(mpd_msg,config.mpd.msg[i].msg[j].message);
            }    
            srssip(&mpd_msg,"$A",artist);
            srssip(&mpd_msg,"$T",title);
            srssip(&mpd_msg,"$L",album);
            srssip(&mpd_msg,"$Y",year);
            srssip(&mpd_msg,"$F",filename);
            strcpy(filename,basename(filename));
            srssip(&mpd_msg,"$f",filename);
            if (mpd_status != MPD_STOP) {
                sprintf(artist,"%ld",total); srssip(&mpd_msg,"$tS",artist);
                sprintf(artist,"%ld",current); srssip(&mpd_msg,"$eS",artist);
                sprintf(artist,"%ld",current/total*100); srssip(&mpd_msg,"$pp",artist);
                sprintf(artist,"%ld",100-current/total*100); srssip(&mpd_msg,"$pr",artist);
                sprintf(artist,"%ld",current/60); srssip(&mpd_msg,"$em",artist);
                sprintf(artist,"%02ld",current%60); srssip(&mpd_msg,"$es",artist);
                sprintf(artist,"%ld",total/60);    srssip(&mpd_msg,"$tm",artist);
                sprintf(artist,"%02ld",total%60); srssip(&mpd_msg,"$ts",artist);
            }    
            char *isomsg;
            isomsg = strdup(convert2iso(mpd_msg));
            widget_tell(config.mpd.msg[i].msg[j],isomsg);
            free(isomsg);
        }
    }
    free(artist); free(title); free(album); free(year); free(filename); free(tmp); free(mpd_msg);
}


#endif



#ifdef PLUGIN_TIME
char *time_str;
struct tm *cur_time;
int TIME_LENGTH=10;

void print_time () {
    time_t t;
    int i;
    size_t ret;
    
    t=time(NULL);
    localtime_r(&t,cur_time);
    for (i=0; i<config.date.msg_nr ; i++) {
        ret=strftime(time_str, TIME_LENGTH, config.date.msg[i].message,cur_time);
        while ( (ret==TIME_LENGTH-2) || (ret==0) ) {
            time_str=realloc(time_str,TIME_LENGTH+100);
            TIME_LENGTH += 100;
            ret=strftime(time_str, TIME_LENGTH, config.date.msg[i].message,cur_time);
        }
        widget_tell(config.date.msg[i], time_str);
    }        
}
#endif

#ifdef PLUGIN_SENSORS
char *sensor_msg;
void print_sensors () {
    FILE *file;
    int i,j;
    long value;
    char *msg;
    aws_val *tmp_val;
    
    for (i=0; i<config.sensor.sensor_nr ; i++) {
        file = fopen(config.sensor.sensors[i].file,"r");
        if (file!=NULL) {
            fscanf(file,"%s",sensor_msg);
            value = atol(sensor_msg);
            if (config.sensor.sensors[i].values_nr!=0) {
                tmp_val = values_dup(config.sensor.sensors[i].values,config.sensor.sensors[i].values_nr); // malloc tmp_val
            } else {
                tmp_val = NULL;
            }
            if (tmp_val!=NULL) {
                replace_in_value(tmp_val,config.sensor.sensors[i].values_nr,"$V",sensor_msg);
            }
            for (j=0 ; j<config.sensor.sensors[i].msg_nr ; j++) {
                msg = strdup(config.sensor.sensors[i].msg[j].message); // malloc msg
                srssip(&msg,"$V",sensor_msg);
                calc_and_replace(&msg,tmp_val,config.sensor.sensors[i].values_nr,"%.0f");
                widget_tell(config.sensor.sensors[i].msg[j],msg);
                free(msg); // free msg
            }    
            fclose(file);
        } else {
            fprintf(stderr,"Couldn't open %s sensor file!\n",config.sensor.sensors[i].file);
        }
    }    
}
#endif

#ifdef PLUGIN_UPTIME

void print_uptime () {
    FILE *file;

    file = fopen(config.uptime.file,"r");
    if (file!=NULL) {
        char *line;
        int i;
        long int sec;
        char *msg , *work;

        line=malloc(30);
        fscanf(file,"%s",line);
        i=0;
        while (line[i]!='.') {i++;}
        line[i]='\0';
        sec=atol(line);
        free(line);

        for (i=0 ; i<config.uptime.msg_nr ; i++) {
            msg = strdup(config.uptime.msg[i].message); //malloc
            work = malloc(300); 
            sprintf(work,"%ld",sec/86400);
            srssip(&msg,"$D",work);
            sprintf(work,"%ld",(sec % 86400)/3600);
            srssip(&msg,"$H",work);
            sprintf(work,"%02ld", ((sec % 86400)%3600)/60);
            srssip(&msg,"$M",work);
            sprintf(work,"%02ld", ((sec%86400)%3600)%60);
            srssip(&msg,"$S",work);
            sprintf(work,"%ld",sec);
            srssip(&msg,"$TS",work);
            sprintf(work,"%ld",sec/60);
            srssip(&msg,"$TM",work);
            sprintf(work,"%ld",sec/3600);
            srssip(&msg,"$TH",work);
            widget_tell(config.uptime.msg[i], msg);
            free(work);
            free(msg); // malloced by strdup
        }    
        fclose(file);
    } else {
        fprintf(stderr,"Couldn't open %s file (uptime)!",config.uptime.file);
    }
}
#endif

#ifdef PLUGIN_MBOX
static int update_file (const char *f) {
  FILE *fp;
  char buf[1024];
  int have_new = 0;
  int hdr = 1;
  int locked = 0;

  struct stat sb;
  struct utimbuf ub;
  struct flock fl;

  if (!(fp = fopen (f, "r")))
  {
    fprintf (stderr, "Can't open %s (%s).\n", f, strerror (errno));
    return -1;
  }

  memset (&fl, 0, sizeof (fl));
  fl.l_type = F_RDLCK;
  fl.l_whence = SEEK_SET;
  fl.l_start = fl.l_len = 0;

  if (fcntl (fileno (fp), F_SETLKW, &fl) == -1)
  {
    fprintf (stderr, "Can't lock %s (%s).\n", f, strerror (errno));
    goto bail;
  }

  locked = 1;

  while (fgets (buf, sizeof (buf), fp))
  {
    if (strncmp (buf, "From ", 5) == 0)
    {
      hdr = 1;
      have_new++;
    }
    else if (hdr && strncmp (buf, "Status:", 6) == 0)
    {
      if (strchr (buf + 6, 'O') || strchr (buf + 6, 'R'))
        have_new--;
    }
    else if (hdr && *buf == '\n')
      hdr = 0;
  }

  if (have_new)
  {
    if (stat (f, &sb) == -1)
    {
      fprintf (stderr, "Can't stat %s (%s).\n",f, strerror (errno));
      goto bail;
    }

    ub.modtime = sb.st_mtime;
    ub.actime  = sb.st_mtime - 1;

    if (utime (f, &ub) == -1)
    {
      fprintf (stderr, "Can't utime %s (%s).\n",f, strerror (errno));
      goto bail;
    }
  }

bail:

  if (locked)
  {
    fl.l_type = F_UNLCK;
    if (fcntl (fileno (fp), F_SETLK, &fl) == -1)
    {
      fprintf (stderr, "Can't unlock %s (%s).\n",f, strerror (errno));
    }
  }

  fclose (fp);
  return (have_new);
}

void print_mboxinfo () {
    int u;

    if (config.mbox.msg_nr < 1 ) return;

    u = update_file(config.mbox.file);
    if (u!=-1) {
        char *msg , *tmp;
        int i;
        tmp = malloc(10);
        if (tmp==NULL) return;
        for (i=0 ; i<config.mbox.msg_nr ; i++) {
            msg = strdup(config.mbox.msg[i].message);
            sprintf(tmp,"%d",u);
            srssip(&msg,"$u",tmp);
            widget_tell(config.mbox.msg[i],msg);
            free(msg);
        }
        free(tmp);
    }
}    
#endif

#ifdef PLUGIN_NEWS
// doesn't work in one command - i don't know, why
// 0,1 : newsbeuter
// 2,3 : liferea
const char *sql_commands[4] = {
    "select count(*) from rss_item where unread = 1;",
    "select count(*) from rss_item where unread = 0;",
    "select count(*) from items where read = 0;", 
    "select count(*) from items where read = 1;"
};

long int news_read, news_unread;

static int new_callback(void *NotUsed, int argc, char **argv, char **azColName){

    NotUsed=0;
/*    // we're waiting 2 arguments: readed, unread*/
    if (argc==1) { 
        news_unread = atol(argv[0]);
        return 0;
    }
/*    } else {
        newsb_read = atol(argv[0]);
        printf("%s %s\n",argv[0],argv[1]);
        newsb_unread = atol(argv[1]);
    }*/
    return 0;
}

static int readed_callback(void *NotUsed, int argc, char **argv, char **azColName){

    NotUsed=0;
/*    // we're waiting 2 arguments: readed, unread*/
    if (argc==1) { 
        news_read = atol(argv[0]);
        return 0;
    }
/*    } else {
        newsb_read = atol(argv[0]);
        printf("%s %s\n",argv[0],argv[1]);
        newsb_unread = atol(argv[1]);
    }*/
    return 0;
}

char *errormsg = 0;
void print_news () {
    sqlite3 *db;
    int i,j;
    char *msg=0, *tmp;

    for (j=0; j<config.news.news_nr; j++) {
        if (config.news.news[j].client != -1) {
            if (sqlite3_open(config.news.news[j].file,&db)!=SQLITE_OK) {
                sqlite3_close(db);
                fprintf(stderr,"Error while opening database '%s'\n.",config.news.news[j].file);
                return;
            }

            i = sqlite3_exec(db,sql_commands[2*config.news.news[j].client],new_callback,0,&errormsg);
            if (i!=SQLITE_OK) {
                fprintf(stderr,"Error while executing command1 in database '%s'.\nError message: %s\n",config.news.news[j].file,errormsg);
//                free(errormsg); errormsg = NULL;
                sqlite3_close(db);
                return;
            }

            i = sqlite3_exec(db,sql_commands[2*config.news.news[j].client+1],readed_callback,0,&errormsg);
            if (i!=SQLITE_OK) {
                fprintf(stderr,"Error while executing command2 in database '%s'.\nError message: %s\n",config.news.news[j].file,errormsg);
//                free(errormsg); errormsg = NULL;
                sqlite3_close(db);
                return;
            }

            tmp = (char*)malloc(20); // malloc
            for (i=0; i<config.news.news[j].msg_nr; i++) {
                msg = strdup(config.news.news[j].msg[i].message); // malloc
                sprintf(tmp,"%ld",news_read);
                srssip(&msg,"$R",tmp);
                sprintf(tmp,"%ld",news_unread);
                srssip(&msg,"$U",tmp);
                sprintf(tmp,"%ld",news_read+news_unread);
                srssip(&msg,"$T",tmp);
                widget_tell(config.news.news[j].msg[i],msg);
                free(msg);
            }
            free(tmp); //free(errormsg);
            sqlite3_close(db);
        }
    }
}
#endif

#ifdef PLUGIN_CPU
long prev_cpu_idle = 0 , prev_cpu_sum = 0 , prev_cpu_user = 0 , prev_cpu_nice = 0 , prev_cpu_system = 0;
char *s_cpu_usage, *s_cpu_nice, *s_cpu_user, *s_cpu_system;

void print_cpuinfo () {
    FILE *file;
    float cpu_percent;
    char *msg;

    file = fopen(config.cpu.file_stat,"r");
    if (file!=NULL) {
        int i;
        float nsum=0 , idlediff , frmtotal, cpu_user, cpu_nice, cpu_system;
        char *tmp[8];

        for (i=0 ; i<8 ; i++) tmp[i]=malloc(12);

        fscanf(file,"cpu %s %s %s %s %s %s %s %s",tmp[0],tmp[1],tmp[2],tmp[3],tmp[4],tmp[5],tmp[6],tmp[7]);
        fclose(file); // we need only the first line

        for ( i=0 ; i<8 ; i++) {
            nsum+=atol(tmp[i]);
        }    
        

        idlediff = atol(tmp[3])-prev_cpu_idle;
        cpu_user = atol(tmp[0]);
        cpu_nice = atol(tmp[1]);
        cpu_system = atol(tmp[2]);
        frmtotal = nsum - prev_cpu_sum - (idlediff<0 ? idlediff : 0);
        cpu_percent = 100 - (idlediff>0 ? idlediff : 0)*100/frmtotal;
        // This is a bug: sometimes cpu usage is a negativ number...
        // I don't know why (at me it works good)
/*        if (cpu_percent < 0) {
            FILE *errfile;
            errfile = fopen("/home/zsolt/awesome-status.log","w");
            if (errfile!=NULL) {
                fprintf(errfile,"prev_cpu_idle %f\n\
                    cpu_user %f\n\
                    cpu_nice %f\n\
                    cpu_system %f\n\
                    nsum %f\n\
                    prev_cpu_sum %f\n\
                    idlediff %f\n\
                    cpu_percent %f\n\
                    frmtotal %f\n",prev_cpu_idle,cpu_user,cpu_nice,cpu_system,nsum,prev_cpu_sum,idlediff,cpu_percent,frmtotal);
                fclose(errfile);
            }
        }*/
        char *formatstr;
        formatstr = malloc(10);
        if (formatstr==NULL) return;
        sprintf(formatstr,"%%.%df",config.cpu.precision);
        sprintf(s_cpu_usage,formatstr,cpu_percent);
        sprintf(s_cpu_user,formatstr,cpu_user-prev_cpu_user); prev_cpu_user = cpu_user;
        sprintf(s_cpu_nice,formatstr,cpu_nice-prev_cpu_nice); prev_cpu_nice = cpu_nice;
        sprintf(s_cpu_system,formatstr,cpu_system-prev_cpu_system); prev_cpu_system = cpu_system;
        free(formatstr); // malloc
        prev_cpu_sum = nsum;
        prev_cpu_idle= atol(tmp[3]);
        for (i=0 ; i<8 ; i++) free(tmp[i]);
    } else strcpy(s_cpu_usage,"0");

    int i;
    for ( i=0 ; i<config.cpu.msg_nr ; i++ ) {
        msg=strdup(config.cpu.msg[i].message);
        srssip(&msg,"$cpu",s_cpu_usage);
        srssip(&msg,"$nice",s_cpu_nice);
        srssip(&msg,"$user",s_cpu_user);
        srssip(&msg,"$sys",s_cpu_system);
        widget_tell(config.cpu.msg[i], msg );
        free(msg); // malloced by strdup
    }    
}
#endif

#ifdef PLUGIN_CMD
char *cmd_line;

void print_command () {
    FILE *file;
    char *out_msg;
    int i,j;

    for (i=0; i<config.commands.commands_nr; i++) {
        // read from the command's standard output
        file = popen(config.commands.commands[i].command,"r");
        fscanf(file,"%[^\n]\n",cmd_line);
        for (j=0; j<config.commands.commands[i].msg_nr; j++) {
            out_msg = strdup(config.commands.commands[i].msg[j].message); // malloc
            srssip(&out_msg,"$stdout",cmd_line);
            widget_tell(config.commands.commands[i].msg[j],cmd_line);
            free(out_msg);
        }
        pclose(file);
    }
    
    return;
}
#endif

#ifdef PLUGIN_MEM
long int prev_mem_out = 100;
float prev_swap_out = 100.0;
void print_meminfo () {
    FILE *file;

    file = fopen(config.mem.file,"r");
    if (file!=NULL) {
        char *reg, *s_value;
        char *work , *out_msg;
        int info_num=0;
        long mem_free=0 , mem_total=0 , mem_cached=0;
        long swap_free=0 , swap_total=0 , swap_cached=0;

        reg=malloc(20);
        s_value=malloc(12);
        while ( fscanf(file,"%s %s kB\n",reg,s_value) != EOF ) {
            if (!strcmp(reg,"MemTotal:")) {
                mem_total=atol(s_value);
                info_num++;
            } else if (!strcmp(reg,"MemFree:")) {
                mem_free=atol(s_value);
                info_num++;
            } else if (!strcmp(reg,"Cached:")) {
                mem_cached=atol(s_value);
                info_num++;
            } else if (!strcmp(reg,"SwapCached:")) {
                swap_cached=atol(s_value);
                info_num++;
            } else if (!strcmp(reg,"SwapFree:")) {
                swap_free=atol(s_value);
                info_num++;
            } else if (!strcmp(reg,"SwapTotal:")) {
                swap_total=atol(s_value);
                info_num++;
            }    
            if (info_num == 6) break;
        }
        
        work = malloc(200);
        char *formatstr = malloc(10);
        aws_val *tmp_val;

        if (config.mem.values_nr!=0) {
            tmp_val = values_dup(config.mem.values,config.mem.values_nr); // malloc tmp_val
        } else {
            tmp_val = NULL;
        }

        for (info_num=0 ; info_num<config.mem.msg_nr ; info_num++) {
            out_msg = strdup(config.mem.msg[info_num].message); // malloc out_msg

            /// Memory sequences
            sprintf(work,"%ld",mem_total);
            srssip(&out_msg,"$memtotalK",work);
            if (tmp_val!=NULL) {
                replace_in_value(tmp_val,config.mem.values_nr,"$memtotalK",work);
            }

            sprintf(work,"%ld",mem_free);
            srssip(&out_msg,"$memfreeK",work);
            if (tmp_val!=NULL) {
                replace_in_value(tmp_val,config.mem.values_nr,"$memfreeK",work);
            }

            sprintf(work,"%ld",mem_cached);
            srssip(&out_msg,"$memcachedK",work);
            if (tmp_val!=NULL) {
                replace_in_value(tmp_val,config.mem.values_nr,"$memcachedK",work);
            }
            
            /// Swap sequences
            sprintf(work,"%ld",swap_total);
            srssip(&out_msg,"$swaptotalK",work);
            if (tmp_val!=NULL) {
                replace_in_value(tmp_val,config.mem.values_nr,"$swaptotalK",work);
            }

            sprintf(work,"%ld",swap_free);
            srssip(&out_msg,"$swapfreeK",work);
            if (tmp_val!=NULL) {
                replace_in_value(tmp_val,config.mem.values_nr,"$swapfreeK",work);
            }

            sprintf(work,"%ld",swap_cached);
            srssip(&out_msg,"$swapcachedK",work);
            if (tmp_val!=NULL) {
                replace_in_value(tmp_val,config.mem.values_nr,"$swapcachedK",work);
            }
            
            sprintf(formatstr,"%%.%df",config.mem.precision);
            if (tmp_val!=NULL) {
                calc_and_replace(&out_msg,tmp_val,config.mem.values_nr,formatstr);
            }

            widget_tell(config.mem.msg[info_num], out_msg );
            free(out_msg); // malloced by strdup
        }    
        if (tmp_val!=NULL) {
            values_free(tmp_val,config.mem.values_nr); // free tmp_val
        }

        free(work);free(formatstr);
        free(reg);
        free(s_value);
        fclose(file);
    } else {
        fprintf(stderr,"Couldn't open file %s\n",config.mem.file);
    }
}    
#endif


#ifdef PLUGIN_SOUND
char *sound_msg;
int sound_msg_length = 30;
static char card[30]="default"; 

void print_soundcard () {
    snd_mixer_t *handle;
    snd_mixer_selem_id_t *sid;
    snd_mixer_elem_t *elem;
    long int pmin, pmax, pvol;
    int i;

    snd_mixer_selem_id_alloca(&sid);

    if (snd_mixer_open(&handle,0)<0) {return;}
    if (snd_mixer_attach(handle, card)<0) {snd_mixer_close(handle); return;}
    if (snd_mixer_selem_register(handle,NULL,NULL)<0) {snd_mixer_close(handle); return;}
    if (snd_mixer_load(handle)<0) {snd_mixer_close(handle); return;}

    for (elem=snd_mixer_first_elem(handle) ; elem ; elem = snd_mixer_elem_next(elem) ) {
        snd_mixer_selem_get_id(elem, sid);
        for (i=0;i<config.sound.mixer_nr;i++) {
            if (!strcmp(snd_mixer_selem_id_get_name(sid),config.sound.mixers[i].mixer)) {
                int j;
                char *tmp;
                snd_mixer_selem_get_playback_volume_range(elem, &pmin, &pmax);
                snd_mixer_selem_get_playback_volume(elem,SND_MIXER_SCHN_MONO,&pvol);
                tmp=malloc(20);
                for (j=0 ; j<config.sound.mixers[i].msg_nr ; j++) {
                    if (strlen(config.sound.mixers[i].msg[j].message) >= sound_msg_length) {
                        sound_msg_length = strlen(config.sound.mixers[i].msg[j].message)+1;
                        sound_msg = malloc(sound_msg_length*sizeof(char));
                    }
                    strcpy(sound_msg,config.sound.mixers[i].msg[j].message);
                    sprintf(tmp,"%.0f",(float)pvol/(float)(pmax-pmin)*100); 
                    srssip(&sound_msg,"$v",tmp);
                    sound_msg_length = strlen(sound_msg);
                    widget_tell(config.sound.mixers[i].msg[j],sound_msg);
                }
                free(tmp);
            }
        }
    }
    snd_mixer_close(handle);
}
#endif

#ifdef PLUGIN_DISKS

void print_diskinfo () {
    struct statfs buf;
    int i,j;
    char *diskmsg;

    for (i=0 ; i<config.disks.disk_nr ; i++) {
        if (!statfs(config.disks.disks[i].mnt_point,&buf)) {
            float fkb, tkb; // free_kbytes, total_kbytes
            /// formatstr: because of precision
            char *swap=malloc(40) , *formatstr=malloc(10);
            aws_val *tmp_val;

            if (config.disks.disks[i].values_nr!=0) {
                tmp_val = values_dup(config.disks.disks[i].values,config.disks.disks[i].values_nr); // malloc tmp_val
            } else {
                tmp_val = NULL;
            }

            tkb=buf.f_blocks*config.disks.disks[i].blocksize;
            fkb=buf.f_bfree*config.disks.disks[i].blocksize;
            for (j=0; j<config.disks.disks[i].msg_nr; j++) {
                diskmsg = strdup(config.disks.disks[i].msg[j].message);    // malloc diskmsg
                sprintf(formatstr,"%%.%df",config.disks.disks[i].precision);

                // free kbytes
                sprintf(swap,formatstr,fkb);
                srssip(&diskmsg,"$fk",swap);
                if (tmp_val!=NULL) {
                    replace_in_value(tmp_val,config.disks.disks[i].values_nr,"$fk",swap);
                }

                // total kbytes
                sprintf(swap,formatstr,tkb);
                srssip(&diskmsg,"$tk",swap);
                if (tmp_val!=NULL) {
                    replace_in_value(tmp_val,config.disks.disks[i].values_nr,"$tk",swap);
                }

                if (tmp_val!=NULL) {
                    calc_and_replace(&diskmsg,tmp_val,config.disks.disks[i].values_nr,formatstr);
                }

                widget_tell(config.disks.disks[i].msg[j],diskmsg);

                free(diskmsg);  // free diskmsg

            }

            free(swap); free(formatstr); 
            if (tmp_val!=NULL) {
                values_free(tmp_val,config.disks.disks[i].values_nr); // free tmp_val
            }
        }
    }
}
#endif

#ifdef PLUGIN_NET
float prev_in=0, prev_out=0;
void print_net () {
    FILE *file;
    int i;
    char *fs;

    for (i=0; i<config.net.if_nr ; i++) {
        file=fopen(config.net.file,"r");
        if (file!=NULL) {
            int j;
            float in, out;
            float fin, fout;
            char *net_msg;

            net_msg = malloc(250);
            while (fscanf(file,"%[^\n]\n",net_msg) != EOF) {
                if (strstr(net_msg,config.net.ifs[i].nif)) {
                    break;
                }    
            }

            fs = malloc(200);
            if (fs == NULL) return;
            sprintf(fs,"%s:%%f",config.net.ifs[i].nif);
            sscanf(net_msg,fs,&in);
            sprintf(fs,"%s:%%*s %%*s %%*s %%*s %%*s %%*s %%*s %%*s %%f",config.net.ifs[i].nif);
            sscanf(net_msg,fs,&out); 
            free(net_msg);
            
            fin=(float)(in-prev_in)/1024;
            fout=(float)(out-prev_out)/1024;

            for (j=0 ; j<config.net.ifs[i].msg_nr ; j++) {
                net_msg = strdup(config.net.ifs[i].msg[j].message); // malloc
                sprintf(fs,"%.1f",fin);
                srssip(&net_msg,"$in_kbps",fs);
                sprintf(fs,"%.1f",fout);
                srssip(&net_msg,"$out_kbps",fs);
                sprintf(fs,"%.0f",in/1024);
                srssip(&net_msg,"$in_totalK",fs);
                sprintf(fs,"%.0f",in/1024/1024);
                srssip(&net_msg,"$in_totalM",fs);
                sprintf(fs,"%.0f",out/1024);
                srssip(&net_msg,"$in_totalK",fs);
                sprintf(fs,"%.0f",out/1024/1024);
                srssip(&net_msg,"$in_totalM",fs);
                widget_tell(config.net.ifs[i].msg[j], net_msg);
                free(net_msg); // malloced by strdup
            }    

            prev_in=in; prev_out=out;

            fclose(file);
            free(fs);
        } else {
            fprintf(stderr,"Couldn't open file %s (mem)!",config.net.file);
        }
    }    
}
#endif

#ifdef PLUGIN_MOC
/*struct sockaddr_un *moc_sock;

int connect_moc () {
    int result;

    moc_sock = malloc(sizeof(struct sockaddr_un));
    strcpy(moc_sock->sun_path,config.moc.sockfile);

    moc_sock->sun_family = AF_UNIX;

//    connect(result,)
}

void print_moc () {
}*/
#endif

char *av_plugins;
// help
void help () {
    printf("awesome-status - print system information to awesome window manager\n");
    printf("\nUsage: awesome-status [option]\n");
    printf("--about\t\tabout this program\n");
    printf("--help\t\tthis information\n");
    printf("--only-stdout\tprint system information only to stdout\n");
    printf("--only-awesome\tprint system information only to awesome (default)\n");
    printf("--both\t\tprint system information to awesome and stdout too\n");
    printf("--plugins\tlist built-in plugins\n");
    printf("--print-config\tprint the processed config\n");
    printf("--version\tsame as --about\n");
    printf("\nThis program created by Zsolt Udvari\nEmail: udvzsolt@gmail.com\n");
}    

// version
void version () {
    printf("awesome-status version %s\n",PROGRAM_VERSION);
    printf("This program sends information to the awesome window manager.\n");
    printf("This program is GPL licensed.\n");
}


// gcd(a,b) (greatest common divider)
int euklides (int a , int b) {
    int result=b;

    if (a*b==0) return a<b?b:a;
    while (a%b!=0) {
        result=a%b;
        a=b; b=result;
    }    
    return result;
}

// main
int main (int argc , char **argv) {
    int loop;
    int gcd=0, bcm=1; // "greatest common divider", "biggest common multiplier"
    struct timespec ts;

    char *tmp = (char*)malloc(200);

    strcpy(tmp,getenv("HOME"));
    strcat(tmp,"/.awesome-status.rc");
    if (parse_config(tmp,&config)) {
        printf("Exiting...\n");
        return 1;
    }    

    free(tmp);

    av_plugins=malloc(100);
    av_plugins[0]=0;
#define SET_BCM(x) bcm = bcm==1 ? x.update_interval : bcm*x.update_interval/gcd;
#ifdef PLUGIN_CMD
    strcat(av_plugins,"commands ");
    gcd = euklides(gcd,config.commands.update_interval);
    cmd_line = malloc(400); // i hope this is enough
    SET_BCM(config.commands);
#endif
#ifdef PLUGIN_CPU
    strcat(av_plugins,"cpu ");
    s_cpu_usage = (char*)malloc(10);
    s_cpu_nice = (char*)malloc(10);
    s_cpu_user = (char*)malloc(10);
    s_cpu_system = (char*)malloc(10);
    gcd = euklides(gcd,config.cpu.update_interval);
    SET_BCM(config.cpu);
#endif    
#ifdef PLUGIN_TIME
    strcat(av_plugins,"date/time ");
    time_str = malloc(TIME_LENGTH);
    cur_time = malloc(sizeof(struct tm));
    gcd = euklides(gcd,config.date.update_interval);
    SET_BCM(config.date);
#endif
#ifdef PLUGIN_DISKS
    strcat(av_plugins,"disks ");
    gcd = euklides(gcd,config.disks.update_interval);
    SET_BCM(config.disks);
#endif    
#ifdef PLUGIN_SENSORS
    strcat(av_plugins,"lm_sensors ");
    sensor_msg=malloc(10);
    gcd = euklides(gcd,config.sensor.update_interval);
    SET_BCM(config.sensor);
#endif    
#ifdef PLUGIN_MBOX
    strcat(av_plugins,"mbox ");
    gcd = euklides(gcd,config.mbox.update_interval);
    SET_BCM(config.mbox);
#endif    
#ifdef PLUGIN_MEM
    strcat(av_plugins,"mem/swap ");
    gcd = euklides(gcd,config.mem.update_interval);
    SET_BCM(config.mem);
#endif
/*#ifdef PLUGIN_MOC
    strcat(av_plugins,"moc(under developing) ");
#endif    */
#ifdef PLUGIN_MPD
    strcat(av_plugins,"mpd ");
    codepage = strdup(nl_langinfo(CODESET));
    mpd_socket=connect_mpd ();            
    gcd = euklides(gcd,config.mpd.update_interval);
    SET_BCM(config.mpd);
#endif
#ifdef PLUGIN_NET
    strcat(av_plugins,"net ");
    gcd = euklides(gcd,config.net.update_interval);
    SET_BCM(config.net);
#endif
#ifdef PLUGIN_NEWS
    strcat(av_plugins,"newsbeuter ");
    gcd = euklides(gcd,config.news.update_interval);
    SET_BCM(config.news);
#endif
#ifdef PLUGIN_SOUND
    strcat(av_plugins,"sound ");
    sound_msg=malloc(sound_msg_length);
    SET_BCM(config.sound);
#endif    
#ifdef PLUGIN_UPTIME
    strcat(av_plugins,"uptime ");
    gcd = euklides(gcd,config.uptime.update_interval);
    SET_BCM(config.uptime);
#endif    

    ts.tv_nsec = 1e8*gcd;
    ts.tv_sec = (int)(ts.tv_nsec/1e9);
    ts.tv_nsec=ts.tv_nsec-ts.tv_sec*1e9;

    multiple_message=malloc(LENGTH_MULTIPLE_MESSAGE);multiple_message[0]=0;

    for (loop=2 ; loop<=argc ; loop++) {
        if (!strcmp(argv[loop-1],"--help")) {help(); return 0;}
        else if (!strcmp(argv[loop-1],"--only-stdout")) {output=1;}
        else if (!strcmp(argv[loop-1],"--only-awesome")) {output=0;}
        else if (!strcmp(argv[loop-1],"--both")) {output=2;}
        else if (!strcmp(argv[loop-1],"--plugins")) {printf("Avaliable plugins: %s\n",av_plugins); return 0;}
        else if (!strcmp(argv[loop-1],"--print-config")) {printinfo(config); return 0;}
        else if ( (!strcmp(argv[loop-1],"--version")) || (!strcmp(argv[loop-1],"--about")) ) { version () ; return 0; }
        else { printf("Invalid argument: %s\n",argv[loop-1]); return 1; }
    }    


    setlocale(LC_ALL,"");

    if (output%2 == 0) {
        csfd = socket(AF_UNIX, SOCK_DGRAM, 0);
        if (csfd<0) {
            printf("Error while opening socket...\n");
            return 1;
        }    
        addr = malloc(sizeof(struct sockaddr_un));
        if (addr == NULL) {
            printf("Error while malloc...\n");
            return 1;
        }    
        strcpy(addr->sun_path,config.sockfile);
        addr->sun_family = AF_UNIX;
    }    

#define RUN_IF_LOOP(interval,nr,funct) if ( (nr>0) && (loop%interval.update_interval==0) ) funct;
    loop=0;
    while (1) {    
        loop+=gcd;
#ifdef PLUGIN_CMD
        RUN_IF_LOOP(config.commands,config.commands.commands_nr,print_command());
#endif
#ifdef PLUGIN_CPU
        RUN_IF_LOOP(config.cpu,config.cpu.msg_nr,print_cpuinfo());
#endif            
#ifdef PLUGIN_DISKS
        RUN_IF_LOOP(config.disks,config.disks.disk_nr,print_diskinfo());
#endif            
#ifdef PLUGIN_MBOX
        RUN_IF_LOOP(config.mbox,config.mbox.msg_nr,print_mboxinfo());
#endif            
#ifdef PLUGIN_MEM            
        RUN_IF_LOOP(config.mem,config.mem.msg_nr,print_meminfo());
#endif            
#ifdef PLUGIN_MPD
        RUN_IF_LOOP(config.mpd,config.mpd.msg_nr,print_mpd());
#endif            
#ifdef PLUGIN_NET            
        RUN_IF_LOOP(config.net,config.net.if_nr,print_net());
#endif            
#ifdef PLUGIN_NEWS
        RUN_IF_LOOP(config.news,config.news.news_nr,print_news());
#endif
#ifdef PLUGIN_SENSORS
        RUN_IF_LOOP(config.sensor,config.sensor.sensor_nr,print_sensors());
#endif            
#ifdef PLUGIN_SOUND            
        RUN_IF_LOOP(config.sound,config.sound.mixer_nr,print_soundcard());
#endif
#ifdef PLUGIN_TIME            
        RUN_IF_LOOP(config.date,config.date.msg_nr,print_time());
#endif
#ifdef PLUGIN_UPTIME            
        RUN_IF_LOOP(config.uptime,config.uptime.msg_nr,print_uptime());
#endif
        widget_flush();
        nanosleep(&ts,NULL);
        if (loop>=bcm) loop=0;
    }

    if (output%2 == 0) {
        close(csfd);
        p_delete(&addr);
    }    
    free(multiple_message);
#ifdef PLUGIN_TIME    
    free(cur_time);
#endif    
#ifdef PLUGIN_SOUND    
    free(sound_msg);
#endif    
#ifdef PLUGIN_MPD
    free(codepage);
#endif    

    return 0;
}    

// vim: filetype=c:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:encoding=utf-8:textwidth=80

