#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <confuse.h>

#define MSG_STR "msg"

// flags
#define F_SCREEN "screen"
#define F_WIDGET_COMMAND "widget_property"
#define F_WIDGET_NAME "widget_name"
#define F_MESSAGE "message"
#define F_SBNAME "sb_name"
#define F_UPDATE_INTERVAL "interval"
#define F_PRECISION "precision"
#define F_NEW_VALUE "new_value"
#define F_VALUE "value"

// identifiers
#define I_SOCKFILE "sockfile"
#define I_MSG_FORMAT "message_format"

#define I_CMD_MAIN_SEC "shell_cmd"
#define I_CMD_SEC_STR "command"
#define I_CMD_STR "cmd"

#define I_DATE "date"

#define I_DISKS "disks"
#define I_SINGLE_DISK "disk"
#define I_BLOCKSIZE "blocksize"
#define I_DISK_MNT_POINT "mnt_point"

#define I_SND "snd"
#define I_SND_MIXER "mixer"
#define I_SND_MIXER_NAME "mixer_name"

#define I_MPD "mpd"
#define I_MPD_HOST "host"
#define I_MPD_PORT "port"
#define I_MPD_MSG_STR "mpd_msg"
#define I_MPD_PAUSEMSG "pause_msg"
#define I_MPD_STOPMSG "stop_msg"
#define I_MPD_UKARTIST "uk_artist" //UnKnown artist
#define I_MPD_UKTITLE "uk_title"
#define I_MPD_FALLBACK "fallback"
#define I_MPD_NOTCONNECT "notconnect"

#define I_MBOX "mbox"
#define I_MBOX_FILE "file"

#define I_NEWS "news"
#define I_NEWSREADER "client"
#define I_NEWS_DATABASE "file"

#define I_CPU "cpu"
#define I_CPU_FILESTAT "file_stat"
#define I_CPU_FILEINFO "file_info"

#define I_SENSOR "sensor"
#define I_SENSORS "sensors"
#define I_SENSOR_FILE "file"
#define I_SENSOR_DIV "div"

#define I_MEM "mem"
#define I_MEMFILE "file"

#define I_NET "net"
#define I_NETFILE "file"
#define I_NETREGS "ifs"
#define I_NETREG "nif"

#define I_UPTIME "uptime"
#define I_UPTIMEFILE "file"


typedef struct aws_val {
    char *name , *value;
} aws_val;

// some structure definitions
typedef struct aws_msg {
    int screen;
    char *widget_name;
    char *sb_name;
    char *widget_command; // 'text' or 'data <progressbar_data_name>' or similar
    char *message_format;
    char *message;
} aws_msg;

typedef struct aws_snd_mixer {
    char *mixer;
    int msg_nr;
    aws_msg *msg;
} aws_snd_mixer;

typedef struct aws_mpd_msg {
    char
        *msg_pause, 
        *msg_stop, 
        *msg_ukartist, 
        *msg_uktitle,
        *msg_fallback,
        *msg_notconnect;
    int update_interval;
    int msg_nr;
    aws_msg *msg;
} aws_mpd_msg;

/* individual config structures */

/// cpu information
typedef struct aws_cpu {
    /// The file from read cpu usage, etc.
    char *file_stat , 
         /// The cpu's information (mhz, vendor name, etc.)
         *file_info;
    /// Precision of float numbers
    int precision;
    /// Update interval in 1/10 secs
    int update_interval;
    /// How many messages
    int msg_nr;
    /// The messages
    aws_msg *msg;
} aws_cpu;    

/// Date/time configuration
typedef struct aws_date {
    /// Update interval in 1/10 secs
    int update_interval;
    /// How many messages
    int msg_nr;    
    /// The messages
    aws_msg *msg;
} aws_date;    

/// single disk configuration
typedef struct aws_single_disk {
    /// Number of new values
    int values_nr;
    /// New values (name and value)
    aws_val *values;
    /// Mount point
    char *mnt_point;
    /// Blocksize of device
    int blocksize;
    /// Precision of float numbers
    int precision;
    /// How many messages
    int msg_nr;
    /// The messages
    aws_msg *msg;
} aws_single_disk;

/// Disk usage configuration
typedef struct aws_disks {
    /// Update interval in 1/10 secs
    int update_interval;
    /// How many disks
    int disk_nr;
    /// The disks
    aws_single_disk *disks;
} aws_disks;    

/// mbox configuration
typedef struct aws_mbox {
    /// The file from read the messages
    char *file;
    /// Update interval in 1/10 secs
    int update_interval;
    /// How many messages
    int msg_nr;
    /// The messages
    aws_msg *msg;
} aws_mbox;    

/// Memory configuration
typedef struct aws_mem {
    /// Values
    int values_nr;
    aws_val *values;
    /// The file from read the informations
    char *file;
    /// Precision of float numbers
    int precision;
    /// Update interval in 1/10 secs
    int update_interval;
    /// How many messages
    int msg_nr;
    /// The messages
    aws_msg *msg;
} aws_mem;    

// Not implemented yet!
typedef struct aws_moc {
    char *sockfile;
} aws_moc;

/// MPD configuration
typedef struct aws_mpd {
    /// The hostname where the mpd's running
    char *host;
    /// The port where mpd's listening
    int port;
    /// Update interval in 1/10 secs
    int update_interval;
    /// How many messages
    int msg_nr;
    /// The messages (MSG_STR)
    aws_mpd_msg *msg;
} aws_mpd;    

// Newsreaders configuration
typedef struct aws_singlenews {
    // 0 - newsbeuter
    // 1 - liferea
    int client;
    char *file;
    int msg_nr;
    aws_msg *msg;
} aws_singlenews;

typedef struct aws_news {
    int news_nr;
    aws_singlenews *news;
    int update_interval;
} aws_news;

/// Single sensor configuration
typedef struct aws_single_sensor {
    /// Values
    int values_nr;
    aws_val *values;
    /// The file from read the value
    char *file;
    /// How many messages
    int msg_nr;
    /// The messages
    aws_msg *msg;
} aws_single_sensor;    

/// Sensors information
typedef struct aws_sensor {
    /// Update interval in 1/10 secs
    int update_interval;
    /// How many sensors
    int sensor_nr;
    /// The sensors
    aws_single_sensor *sensors;
} aws_sensor;    

/// Soundcard information
typedef struct aws_sound {
    /// Update interval in 1/10 secs
    int update_interval;
    /// How many mixers
    int mixer_nr;
    /// The mixers
    aws_snd_mixer *mixers;
} aws_sound;    

typedef struct aws_if {
    char *nif; // Net InterFace
    int msg_nr;
    aws_msg *msg;
} aws_if;

typedef struct aws_net {
    char *file;
    int update_interval;
    int if_nr;
    aws_if *ifs;
} aws_net;    

typedef struct aws_single_command {
    char *command;
    int msg_nr;
    aws_msg *msg;
} aws_single_command;

typedef struct aws_cmd {
    int update_interval;
    int commands_nr;
    aws_single_command *commands;
} aws_cmd;

typedef struct aws_uptime {
    char *file;
    int update_interval;
    int msg_nr;
    aws_msg *msg;
} aws_uptime;

// the main config structure
typedef struct aws_config {
    char *sockfile;
    char *message_format;
    aws_cmd     commands;
    aws_cpu     cpu;
    aws_date    date;
    aws_disks   disks;
    aws_mbox    mbox;
    aws_mem     mem;
//    aws_moc        moc;
    aws_mpd     mpd;
    aws_net     net;
    aws_news    news;
    aws_sensor  sensor;
    aws_sound   sound;
    aws_uptime  uptime;
} aws_config;


// interface
int parse_config (const char *config_file , aws_config *config); 
void printinfo(aws_config);

// vim: filetype=c:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:encoding=utf-8:textwidth=8
