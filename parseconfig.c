#include "parseconfig.h"
#include <errno.h>

aws_msg create_single_msg(cfg_t *source , char *def_format) {
    aws_msg result;

    // awesome-specific
    result.screen = cfg_getint(source,F_SCREEN);
    result.widget_name = strdup(cfg_getstr(source,F_WIDGET_NAME));
    result.widget_command = strdup(cfg_getstr(source,F_WIDGET_COMMAND));
    result.message = strdup(cfg_getstr(source,F_MESSAGE));
    result.sb_name = strdup(cfg_getstr(source,F_SBNAME));
    result.message_format = strdup(cfg_getstr(source,I_MSG_FORMAT));
    if (!strlen(result.message_format)) {
        free(result.message_format);
        result.message_format = strdup(def_format);
    }


    return result;
}

aws_msg *create_multi_msg(cfg_t *source , int *nr , char *def_format) {
    aws_msg *result = NULL;
    cfg_t *msg;
    int i;
    
    *nr = cfg_size(source,MSG_STR);
    result = calloc(sizeof(aws_msg),*nr);
    if (result==NULL) return result;

    for (i=0 ; i<*nr ; i++) {
        msg = cfg_getnsec(source,MSG_STR,i);
        result[i] = create_single_msg(msg,def_format);
    }
    return result;
}    


int parse_config (char const *config_file , aws_config *config) {
    // New value format
    cfg_opt_t opts_newvalue [] = {
        // The calculation - see
        // http://en.wikipedia.org/wiki/Reverse_Polish_notation#Example
        CFG_STR(F_VALUE,"",CFGF_NONE),
        CFG_END()
    };

    // The message format
    cfg_opt_t opts_message [] = {
        // Which screen
        CFG_INT(F_SCREEN,        0,    CFGF_NONE),
        // Which statusbar
        CFG_STR(F_SBNAME,        "",    CFGF_NONE),
        // What is widget's name
        CFG_STR(F_WIDGET_NAME,        "",    CFGF_NONE),
        // What command
        CFG_STR(F_WIDGET_COMMAND,    "",    CFGF_NONE),
        // Format of message (because of compatible with all awesome versions)
        // If nullstring then fallback the main msg_format
        // If you want to use " char in format, please use \" sequence!
        CFG_STR(I_MSG_FORMAT,   "", CFGF_NONE),
        // The message
        CFG_STR(F_MESSAGE,        "",    CFGF_NONE),
        CFG_END()
    };

    // Single disk options (parts of main options)
    cfg_opt_t opts_single_disk [] = {
        // The mount point
        CFG_STR(I_DISK_MNT_POINT, "/",CFGF_NONE),
        // The size of blocks
        CFG_INT(I_BLOCKSIZE,4,CFGF_NONE),
        // The desired precision
        CFG_INT(F_PRECISION,1,CFGF_NONE),
        // New values
        CFG_SEC(F_NEW_VALUE, opts_newvalue, CFGF_MULTI | CFGF_TITLE),
        // The messages, seqs are:
        // $tk, $fk : total/free kbytes - and the user-definied values
        CFG_SEC(MSG_STR,    opts_message, CFGF_MULTI),
        CFG_END()
    };

    // MPD-style message
    cfg_opt_t opts_mpd_msg [] = {
        // The message when mpd is paused
        CFG_STR(I_MPD_PAUSEMSG, "<< $A - $T >>",CFGF_NONE),
        // The message when mpd is stopped
        CFG_STR(I_MPD_STOPMSG,     "mpd stopped",    CFGF_NONE),
        // This will be printed instead of artist when it's unavaliable
        CFG_STR(I_MPD_UKARTIST,    "unknown artist",CFGF_NONE),
        // This will be printed instead of title when it's unavaliable
        CFG_STR(I_MPD_UKTITLE,    "unknown title",CFGF_NONE),
        // This will be printed when the artist and title are unavaliable
        CFG_STR(I_MPD_FALLBACK,    "$F" ,    CFGF_NONE),
        // This will be printed if mpd isn't running
        CFG_STR(I_MPD_NOTCONNECT, "mpd not running", CFGF_NONE),
        // Messages
        CFG_SEC(MSG_STR,    opts_message, CFGF_MULTI),
        CFG_END()
    };    

    // A network interface
    cfg_opt_t opts_netif [] = {
        // Interface's name
        CFG_STR(I_NETREG,"eth0",CFGF_NONE),
        // Messages
        CFG_SEC(MSG_STR,opts_message,CFGF_MULTI),
        CFG_END()
    };

    // Soundcard mixer state
    cfg_opt_t opts_soundcard_mixer [] = {
        // The mixer's name (see amixer output)
        CFG_STR(I_SND_MIXER_NAME,"Master",CFGF_NONE),
        // Messages
        CFG_SEC(MSG_STR, opts_message, CFGF_MULTI),
        CFG_END()
    };

    // Sensor information
    cfg_opt_t opts_sensor [] = {
        // The file from that read the value
        CFG_STR(I_SENSOR_FILE,"",CFGF_NONE),
        // Create new values
        CFG_SEC(F_NEW_VALUE, opts_newvalue, CFGF_MULTI | CFGF_TITLE),
        // Messages, only seq: $V - the readen value (and the user-definied)
        CFG_SEC(MSG_STR,opts_message, CFGF_MULTI),
        CFG_END()
    };

    // date options
    cfg_opt_t opts_date [] = {
        // Messages - sequences: see 'man date'
        CFG_SEC(MSG_STR,opts_message,CFGF_MULTI),
        // Update interval in 1/10 secs
        CFG_INT(F_UPDATE_INTERVAL, 10, CFGF_NONE),
        CFG_END()
    };

    // disk usage information
    cfg_opt_t opts_disks [] = {
        // A disk
        CFG_SEC(I_SINGLE_DISK, opts_single_disk, CFGF_MULTI),
        // Update interval in 1/10 secs
        CFG_INT(F_UPDATE_INTERVAL, 10, CFGF_NONE),
        CFG_END()
    };    
    
    // mbox information
    cfg_opt_t opts_mbox [] = {
        // the mbox filename
        CFG_STR(I_MBOX_FILE,"~/mbox",CFGF_NONE),
        // Messages, seq: $u is the number of unread messages
        CFG_SEC(MSG_STR,    opts_message, CFGF_MULTI),
        // Update interval in 1/10 secs
        CFG_INT(F_UPDATE_INTERVAL, 10, CFGF_NONE),
        CFG_END()
    };

    // mpd information
    cfg_opt_t opts_mpd [] = {
        // the host where mpd's running
        CFG_STR(I_MPD_HOST,    "localhost",    CFGF_NONE),
        // the port where mpd's listening
        CFG_INT(I_MPD_PORT,    6600,        CFGF_NONE),
        // update interval
        CFG_INT(F_UPDATE_INTERVAL, 10, CFGF_NONE),
        // the messages
        // Sequences are: $A - artist, $T - title, $L - album, $Y - year,
        // $f - filename without path, $F - filename with path,
        // $em - elapsed minutes, 
        // $es - elapsed seconds (mod 60 - 123s = 2m:_3_s)
        // $eS - elapsed total seconds (123s = 123s)
        // $tm - total minutes, $ts - total seconds,
        // $tS - elapsed total seconds, $pp - percent played,
        // $pr - percent remain
        CFG_SEC(I_MPD_MSG_STR,    opts_mpd_msg,    CFGF_MULTI),
        CFG_END()
    };    

    // soundcard information
    cfg_opt_t opts_soundcard [] = {
        // Mixers
        CFG_SEC(I_SND_MIXER, opts_soundcard_mixer, CFGF_MULTI),
        // update interval
        CFG_INT(F_UPDATE_INTERVAL, 10, CFGF_NONE),
        CFG_END()
    };    

    // cpu information
    cfg_opt_t opts_cpu [] = {
        // the /proc/stat file
        CFG_STR(I_CPU_FILESTAT,"/proc/stat",CFGF_NONE),
        // the /proc/cpuinfo file - NOT IMPLEMENTED YET
        CFG_STR(I_CPU_FILEINFO,"/proc/cpuinfo",CFGF_NONE),
        // the precision (in decimals)
        CFG_INT(F_PRECISION,0,CFGF_NONE),
        // update interval in 1/10 secs
        CFG_INT(F_UPDATE_INTERVAL, 10, CFGF_NONE),
        // Messages, seqs are:
        // $sys - system, $user - user, $nice - nice
        CFG_SEC(MSG_STR, opts_message,    CFGF_MULTI),
        CFG_END()
    };    

    // sensor information
    cfg_opt_t opts_sensors [] = {
        // sensors
        CFG_SEC(I_SENSOR,opts_sensor,CFGF_MULTI),
        // update interval
        CFG_INT(F_UPDATE_INTERVAL, 10, CFGF_NONE),
        CFG_END()
    };    

    // memory information
    cfg_opt_t opts_mem [] = {
        // the /proc/meminfo file
        CFG_STR(I_MEMFILE, "/proc/meminfo", CFGF_NONE),
        // precision (in decimals)
        CFG_INT(F_PRECISION,1,CFGF_NONE),
        // update interval
        CFG_INT(F_UPDATE_INTERVAL, 10, CFGF_NONE),
        // new values
        CFG_SEC(F_NEW_VALUE, opts_newvalue, CFGF_MULTI | CFGF_TITLE),
        // Messages, seqs are:
        // $memtotalK - total memory in kb
        // $memfreeK - free memory in kb
        // $memcachedK - cached memory in kb
        // $swaptotalK - total swap in kb
        // $swapfreeK - free swap in kb
        // $swapcachedK - cached swap in kb
        // and the user-definied values ;)
        CFG_SEC(MSG_STR,opts_message,CFGF_MULTI),
        CFG_END()
    };    

    // network information
    cfg_opt_t opts_net [] = {
        // the /prc/net/dev file
        CFG_STR(I_NETFILE, "/proc/net/dev", CFGF_NONE),
        // the registers
        CFG_SEC(I_NETREGS,opts_netif,CFGF_MULTI),
        // update interval
        CFG_INT(F_UPDATE_INTERVAL, 10, CFGF_NONE),
        CFG_END()
    };    

    // uptime information
    cfg_opt_t opts_uptime [] = {
        // the /proc/uptime file
        CFG_STR(I_UPTIMEFILE,"/proc/uptime",CFGF_NONE),
        // Messages, seqs are:
        // $D, $H, $M, $S - uptime date, hour, minute, second
        // $TS, $TM, $TH - total second, minute, hour
        CFG_SEC(MSG_STR,opts_message,CFGF_MULTI),
        // update interval
        CFG_INT(F_UPDATE_INTERVAL, 10, CFGF_NONE),
        CFG_END()
    };    

    // single news
    cfg_opt_t opts_single_news [] = {
        // the type of newsreader client, possible values:
        // newsbeuter, liferea 
        // liferea's database is ~/.liferea_1.4/liferea.db
        CFG_STR(I_NEWSREADER,"newsbeuter",CFGF_NONE),
        // the database file - maybe would be better the full path
        CFG_STR(I_NEWS_DATABASE,".newsbeuter/cache.db",CFGF_NONE),
        // Messages, seqs are:
        // $U - number of unread articles
        // $R - number of readed articles
        // $T - total of articles
        CFG_SEC(MSG_STR,opts_message,CFGF_MULTI),
        CFG_END()
    };

    // the news section
    cfg_opt_t opts_news [] = {
        // update interval
        CFG_INT(F_UPDATE_INTERVAL, 600, CFGF_NONE),
        // the news sources
        CFG_SEC(I_NEWS,opts_single_news,CFGF_MULTI),
        CFG_END()
    };

    // external command
    cfg_opt_t opts_shellcommand [] = {
        // the shell-command what should run and read its output
        CFG_STR(I_CMD_STR,"",CFGF_NONE),
        // the messages, $stdout is the command's standard output
        CFG_SEC(MSG_STR,opts_message,CFGF_MULTI),
        CFG_END()
    };

    // set of external commands
    cfg_opt_t opts_shellcommands [] = {
        // one external command
        CFG_SEC(I_CMD_SEC_STR,opts_shellcommand,CFGF_MULTI),
        // update interval
        CFG_INT(F_UPDATE_INTERVAL,10,CFGF_NONE),
        CFG_END()
    };

    // the main-main section :)
    cfg_opt_t opts [] = {
        // awesome's socket file
        CFG_STR(I_SOCKFILE,".awesome_ctl.0",CFGF_NONE),
        // format of messages
        CFG_STR(I_MSG_FORMAT,"$screen widget_tell $sb_name $widget_name $prop $msg", CFGF_NONE),
        // external commands - reads standard output 
        // please care about the called program's runtime! Don't be long because
        // awesome-status waits for its end!
        CFG_SEC(I_CMD_MAIN_SEC, opts_shellcommands, CFGF_NONE),
        // cpu
        CFG_SEC(I_CPU,    opts_cpu,    CFGF_NONE    ),
        // date
        CFG_SEC(I_DATE,    opts_date,    CFGF_NONE    ),
        // disks
        CFG_SEC(I_DISKS, opts_disks,    CFGF_NONE    ),
        // mbox
        CFG_SEC(I_MBOX, opts_mbox,  CFGF_NONE),
        // memory
        CFG_SEC(I_MEM,    opts_mem,    CFGF_NONE    ),
        // mpd
        CFG_SEC(I_MPD,    opts_mpd,    CFGF_NONE    ),
        // various newsreader's articles
        CFG_SEC(I_NEWS, opts_news, CFGF_NONE),
        // sensors
        CFG_SEC(I_SENSORS, opts_sensors, CFGF_NONE    ),
        // soundcard
        CFG_SEC(I_SND,    opts_soundcard,    CFGF_NONE    ),
        // network
        CFG_SEC(I_NET,    opts_net,    CFGF_NONE    ),
        // uptime
        CFG_SEC(I_UPTIME, opts_uptime,    CFGF_NONE    ),
        CFG_END()
    };

    cfg_t *cfg;

    cfg = cfg_init(opts,CFGF_NONE);
    int ret;
    switch ((ret=cfg_parse(cfg,config_file))) {
            case CFG_FILE_ERROR:
                printf("Parsing configuration file error: %s\n",strerror(errno));
                return 1;
                break;
            case CFG_PARSE_ERROR:
                cfg_error(cfg,"Parsing configuration file failed.\n");
                return 1;
                break;
            case CFG_SUCCESS:    
                break;
    }
                

    // general configs
    config->sockfile = strdup(cfg_getstr(cfg,I_SOCKFILE));
    config->message_format = strdup(cfg_getstr(cfg,I_MSG_FORMAT));

    // sensor's configuration
    cfg_t *cfg_sensors;
    cfg_sensors = cfg_getsec(cfg,I_SENSORS);
    if (cfg_sensors!=NULL) {
        cfg_t *cfg_sensor;
        cfg_t *new_values;
        int i,j;

        config->sensor.sensor_nr = cfg_size(cfg_sensors,I_SENSOR);
        config->sensor.sensors = calloc(sizeof(aws_single_sensor),config->sensor.sensor_nr);
        config->sensor.update_interval = cfg_getint(cfg_sensors,F_UPDATE_INTERVAL);
        for (i=0 ; i<config->sensor.sensor_nr ; i++) {
            cfg_sensor = cfg_getnsec(cfg_sensors,I_SENSOR,i);
            config->sensor.sensors[i].file = strdup(cfg_getstr(cfg_sensor,I_SENSOR_FILE));
            // new values
            config->sensor.sensors[i].values_nr = cfg_size(cfg_sensor,F_NEW_VALUE);  
            config->sensor.sensors[i].values = malloc(config->sensor.sensors[i].values_nr*sizeof(aws_val));
            for (j=0; j<config->sensor.sensors[i].values_nr; j++) {
                new_values = cfg_getnsec(cfg_sensor,F_NEW_VALUE,j);
                config->sensor.sensors[i].values[j].value = strdup(cfg_getstr(new_values,F_VALUE));
                config->sensor.sensors[i].values[j].name = strdup(cfg_title(new_values));
            }
            config->sensor.sensors[i].msg = create_multi_msg(cfg_sensor,&(config->sensor.sensors[i].msg_nr),config->message_format);
        }    
    }
    
    // network's configuration
    cfg_t *cfg_net;
    cfg_net = cfg_getsec(cfg,I_NET);
    if (cfg_net != NULL) {
        cfg_t *cfg_ifs;
        int i;

        config->net.file = strdup(cfg_getstr(cfg_net,I_NETFILE));
        config->net.if_nr = cfg_size(cfg_net,I_NETREGS);
        config->net.ifs=calloc(sizeof(aws_if),config->net.if_nr);
        config->net.update_interval = cfg_getint(cfg_net,F_UPDATE_INTERVAL);
        for (i=0 ; i<config->net.if_nr ; i++) {
            cfg_ifs = cfg_getnsec(cfg_net,I_NETREGS,i);
            config->net.ifs[i].nif = strdup(cfg_getstr(cfg_ifs,I_NETREG));
            config->net.ifs[i].msg = create_multi_msg(cfg_ifs,&(config->net.ifs[i].msg_nr),config->message_format);
        }    
    }    
     
    // command's configuration
    cfg_t *cfg_cmd;
    cfg_cmd = cfg_getsec(cfg,I_CMD_MAIN_SEC);
    if (cfg_cmd != NULL) {
        cfg_t *cfg_cmds;
        int i;

        config->commands.update_interval = cfg_getint(cfg_cmd,F_UPDATE_INTERVAL);
        config->commands.commands_nr = cfg_size(cfg_cmd,I_CMD_SEC_STR);
        config->commands.commands = calloc(sizeof(aws_single_command),config->commands.commands_nr);
        for (i=0; i<config->commands.commands_nr; i++) {
            cfg_cmds = cfg_getnsec(cfg_cmd,I_CMD_SEC_STR,i);
            config->commands.commands[i].command = strdup(cfg_getstr(cfg_cmds,I_CMD_STR));
            config->commands.commands[i].msg = create_multi_msg(cfg_cmds,&(config->commands.commands[i].msg_nr),config->message_format);
        }
    }

    // cpu's configuration
    cfg_t *cfg_cpu;
    cfg_cpu = cfg_getsec(cfg,I_CPU);
    if (cfg_cpu!=NULL) {
        config->cpu.file_stat = strdup(cfg_getstr(cfg_cpu,I_CPU_FILESTAT));
        config->cpu.file_info = strdup(cfg_getstr(cfg_cpu,I_CPU_FILEINFO));
        config->cpu.precision = cfg_getint(cfg_cpu,F_PRECISION);
        config->cpu.update_interval = cfg_getint(cfg_cpu,F_UPDATE_INTERVAL);
        config->cpu.msg  = create_multi_msg(cfg_cpu,&(config->cpu.msg_nr),config->message_format);
    }

    // date/time's configuration
    cfg_t *cfg_date;
    cfg_date = cfg_getsec(cfg,I_DATE);
    if (cfg_date!=NULL) {
        config->date.update_interval = cfg_getint(cfg_date,F_UPDATE_INTERVAL);
        config->date.msg    = create_multi_msg(cfg_date,&(config->date.msg_nr),config->message_format);    
    }

    // sound card's configuration
    cfg_t *cfg_sound;
    cfg_sound = cfg_getsec(cfg,I_SND);
    if (cfg_sound!=NULL) {
        int i; 
        cfg_t *cfg_sound_mixer;

        config->sound.update_interval = cfg_getint(cfg_sound,F_UPDATE_INTERVAL);
        config->sound.mixer_nr = cfg_size(cfg_sound,I_SND_MIXER);
        config->sound.mixers=calloc(sizeof(aws_snd_mixer),config->sound.mixer_nr);
        for ( i=0 ; i<config->sound.mixer_nr ; i++ ) {
            cfg_sound_mixer = cfg_getnsec(cfg_sound,I_SND_MIXER,i);
            config->sound.mixers[i].mixer = strdup(cfg_getstr(cfg_sound_mixer,I_SND_MIXER_NAME));    
            config->sound.mixers[i].msg = create_multi_msg(cfg_sound_mixer,&(config->sound.mixers[i].msg_nr),config->message_format);
        }    
    }    
    
    // memory's configuration
    cfg_t *cfg_mem;
    cfg_mem = cfg_getsec(cfg,I_MEM);
    if (cfg_mem!=NULL) {
        int i;
        cfg_t *new_values;
        config->mem.file = strdup(cfg_getstr(cfg_mem,I_MEMFILE));
        config->mem.precision = cfg_getint(cfg_mem,F_PRECISION);
        config->mem.update_interval = cfg_getint(cfg_mem,F_UPDATE_INTERVAL);
        config->mem.msg = create_multi_msg(cfg_mem,&(config->mem.msg_nr),config->message_format);
        // new values
        config->mem.values_nr = cfg_size(cfg_mem,F_NEW_VALUE);  
        config->mem.values = malloc(config->mem.values_nr*sizeof(aws_val));
        for (i=0; i<config->mem.values_nr; i++) {
            new_values = cfg_getnsec(cfg_mem,F_NEW_VALUE,i);
            config->mem.values[i].value = strdup(cfg_getstr(new_values,F_VALUE));
            config->mem.values[i].name = strdup(cfg_title(new_values));
        }
    }    

    // mbox's configuration
    cfg_t *cfg_mbox;
    cfg_mbox = cfg_getsec(cfg,I_MBOX);
    if (cfg_mbox!=NULL) {
        config->mbox.file = strdup(cfg_getstr(cfg_mbox,I_MBOX_FILE));
        config->mbox.update_interval = cfg_getint(cfg_mbox,F_UPDATE_INTERVAL);
        config->mbox.msg = create_multi_msg(cfg_mbox,&(config->mbox.msg_nr),config->message_format);
    }    

    // news readers configuration
    cfg_t *cfg_news;
    cfg_news = cfg_getsec(cfg,I_NEWS);
    if (cfg_news!=NULL) {
        int i;
        cfg_t *cfg_single_news;
        config->news.update_interval = cfg_getint(cfg_news,F_UPDATE_INTERVAL);
        config->news.news_nr = cfg_size(cfg_news,I_NEWS);
        config->news.news = (aws_singlenews*)calloc(sizeof(aws_singlenews),config->news.news_nr);
        for (i=0; i<config->news.news_nr; i++) {
            cfg_single_news = cfg_getnsec(cfg_news,I_NEWS,i);

            if (!strcmp("newsbeuter",cfg_getstr(cfg_single_news,I_NEWSREADER))) {
                config->news.news[i].client = 0;
            } else if (!strcmp("liferea",cfg_getstr(cfg_single_news,I_NEWSREADER))) {
                config->news.news[i].client = 1;
            } else {
                // undefinied value
                config->news.news[i].client = -1;
            }

            if (config->news.news[i].client != -1) {
                config->news.news[i].file = strdup(cfg_getstr(cfg_single_news,I_NEWS_DATABASE));
                config->news.news[i].msg = create_multi_msg(cfg_single_news,&(config->news.news[i].msg_nr),config->message_format);
            }

        }
    }

    // mpd's configuration    
    cfg_t *cfg_mpd;
    cfg_mpd = cfg_getsec(cfg,I_MPD);
    if (cfg_mpd!=NULL) {
        config->mpd.port    = cfg_getint(cfg_mpd,I_MPD_PORT);
        config->mpd.host    = strdup(cfg_getstr(cfg_mpd,I_MPD_HOST));
        config->mpd.update_interval = cfg_getint(cfg_mpd,F_UPDATE_INTERVAL);
        config->mpd.msg_nr    = cfg_size(cfg_mpd,I_MPD_MSG_STR);
        config->mpd.msg        = calloc(sizeof(aws_mpd_msg),config->mpd.msg_nr);
        int j; cfg_t *cfg_mpd_msg;
        for (j=0 ; j<config->mpd.msg_nr ; j++) {
            cfg_mpd_msg = cfg_getnsec(cfg_mpd,I_MPD_MSG_STR,j);
            config->mpd.msg[j].msg_pause = strdup(cfg_getstr(cfg_mpd_msg,I_MPD_PAUSEMSG));
            config->mpd.msg[j].msg_stop  = strdup(cfg_getstr(cfg_mpd_msg,I_MPD_STOPMSG));
            config->mpd.msg[j].msg_ukartist = strdup(cfg_getstr(cfg_mpd_msg,I_MPD_UKARTIST));
            config->mpd.msg[j].msg_uktitle    = strdup(cfg_getstr(cfg_mpd_msg,I_MPD_UKTITLE));
            config->mpd.msg[j].msg_fallback    = strdup(cfg_getstr(cfg_mpd_msg,I_MPD_FALLBACK));
            config->mpd.msg[j].msg_notconnect = strdup(cfg_getstr(cfg_mpd_msg,I_MPD_NOTCONNECT));
            config->mpd.msg[j].msg    = create_multi_msg(cfg_mpd_msg,&(config->mpd.msg[j].msg_nr),config->message_format);
        }
    }

    // disks configuration
    cfg_t *cfg_disks;
    cfg_disks = cfg_getsec(cfg,I_DISKS);
    if (cfg_disks!=NULL) {
        config->disks.disk_nr    = cfg_size(cfg_disks,I_SINGLE_DISK);
        config->disks.disks    = calloc(sizeof(aws_single_disk),config->disks.disk_nr);
        config->disks.update_interval = cfg_getint(cfg_disks,F_UPDATE_INTERVAL);

        int i,j;
        cfg_t *cfg_single_disk;
        cfg_t *new_values;
        for (j=0 ; j<config->disks.disk_nr ; j++) {
            cfg_single_disk = cfg_getnsec(cfg_disks,I_SINGLE_DISK,j);
            config->disks.disks[j].mnt_point = strdup(cfg_getstr(cfg_single_disk,I_DISK_MNT_POINT));
            config->disks.disks[j].blocksize = cfg_getint(cfg_single_disk,I_BLOCKSIZE);
            config->disks.disks[j].precision = cfg_getint(cfg_single_disk,F_PRECISION);
            // new values
            config->disks.disks[j].values_nr = cfg_size(cfg_single_disk,F_NEW_VALUE);  
            config->disks.disks[j].values = malloc(config->disks.disks[j].values_nr*sizeof(aws_val));
            for (i=0; i<config->disks.disks[j].values_nr; i++) {
                new_values = cfg_getnsec(cfg_single_disk,F_NEW_VALUE,i);
                config->disks.disks[j].values[i].value = strdup(cfg_getstr(new_values,F_VALUE));
                config->disks.disks[j].values[i].name = strdup(cfg_title(new_values));
            }
            config->disks.disks[j].msg = create_multi_msg(cfg_single_disk,&(config->disks.disks[j].msg_nr),config->message_format);
        }
    }    

    // uptime's configuration
    cfg_t *cfg_uptime;
    cfg_uptime = cfg_getsec(cfg,I_UPTIME);
    if (cfg_uptime!=NULL) {
        config->uptime.update_interval = cfg_getint(cfg_uptime,F_UPDATE_INTERVAL);
        config->uptime.file = strdup(cfg_getstr(cfg_uptime,I_UPTIMEFILE));
        config->uptime.msg  = create_multi_msg(cfg_uptime,&(config->uptime.msg_nr),config->message_format);
    }    
    cfg_free(cfg);
    return 0;
}

void printmsg (const char* prefix , aws_msg msg) {

    printf("%sMessage format: %s\n\
            %sScreen: %d\n\
            %sStatusbar name: %s\n\
            %sWidget name: %s\n\
            %sWidget property: %s\n\
            %sMessage: %s\n",
                prefix, msg.message_format,
                prefix, msg.screen,
                prefix, msg.sb_name,
                prefix, msg.widget_name,
                prefix, msg.widget_command,
                prefix, msg.message );

}

void print_new_values(const char *prefix , aws_val *values , int value_nr) {
    int i;

    for (i=0;i<value_nr;i++) {
        printf("%sNew value: %s = '%s'\n",prefix,values[i].name,values[i].value);
    }
}

#define print_interval(indent,iv) printf("%sInterval: %d\n",indent,iv.update_interval)

void printinfo (aws_config myconfig) {
    int i,j;
        
    printf("Commands\n");
    print_interval("\t",myconfig.commands);

    printf("CPU\n");
    print_interval("\t",myconfig.cpu);
    printf("\t%s\n",myconfig.cpu.file_stat);
    printf("\t%s\n",myconfig.cpu.file_info);
    for ( i=0 ; i<myconfig.cpu.msg_nr ; i++) {
        printmsg("\t",myconfig.cpu.msg[i]);
    }    

    printf("Date\n");
    print_interval("\t",myconfig.date);
    for ( i=0 ; i<myconfig.date.msg_nr ; i++) {
        printmsg("\t",myconfig.date.msg[i]);
    }

    printf("Disks\n");
    print_interval("\t",myconfig.disks);
    for ( i=0 ; i<myconfig.disks.disk_nr ; i++) {
        printf("\tMount point: %s Blocksize: %d Precision: %d\n",myconfig.disks.disks[i].mnt_point,myconfig.disks.disks[i].blocksize,myconfig.disks.disks[i].precision);
        print_new_values("\t\t",myconfig.disks.disks[i].values,myconfig.disks.disks[i].values_nr);
        for ( j=0 ; j<myconfig.disks.disks[i].msg_nr ; j++) {
            printmsg("\t\t",myconfig.disks.disks[i].msg[j]);
        }
    }    

    printf("Mbox\n");
    print_interval("\t",myconfig.mbox);
    printf("\tFile: %s\n",myconfig.mbox.file);
    for ( i=0 ; i<myconfig.mbox.msg_nr ; i++) {
        printmsg("\t",myconfig.mbox.msg[i]);
    }

    printf("Mem\n");
    print_interval("\t",myconfig.mem);
    printf("\tFile: %s\n",myconfig.mem.file);
    print_new_values("\t\t",myconfig.mem.values,myconfig.mem.values_nr);
    for ( i=0 ; i<myconfig.mem.msg_nr ; i++ ) {
        printmsg("\t",myconfig.mem.msg[i]);
    }    

    printf("MPD\n");
    print_interval("\t",myconfig.mpd);
    printf("\thost: %s  port: %d\n",myconfig.mpd.host,myconfig.mpd.port);
    for ( i=0 ; i<myconfig.mpd.msg_nr ; i++) {
        printf("\tpause: %s\n\tstop: %s\n\tunknown artist: %s\n\tunknown title: %s\n\tfallback msg: %s\n\tnot connect: %s\n",
            myconfig.mpd.msg[i].msg_pause,
            myconfig.mpd.msg[i].msg_stop,
            myconfig.mpd.msg[i].msg_ukartist,
            myconfig.mpd.msg[i].msg_uktitle,
            myconfig.mpd.msg[i].msg_fallback,
            myconfig.mpd.msg[i].msg_notconnect);
        for (j=0 ; j<myconfig.mpd.msg[i].msg_nr ; j++) {
            printmsg("\t",myconfig.mpd.msg[i].msg[j]);
        }    
    }

    printf("Net\n");
    print_interval("\t",myconfig.net);
    printf("\tFile: %s\n",myconfig.net.file);
    for ( i=0 ; i<myconfig.net.if_nr ; i++ ) {
        printf("\tIf: %s\n",myconfig.net.ifs[i].nif);
        for (j=0 ; j<myconfig.net.ifs[i].msg_nr ; j++) printmsg("\t",myconfig.net.ifs[i].msg[j]);
    }

    printf("News\n");
    print_interval("\t",myconfig.news);
#define GETNEWSREADER(client) client == 0 ? "newsbeuter" : client == 1 ? "liferea" : "UNKNOWN"
    for ( j=0 ; j<myconfig.news.news_nr ; j++) {
        printf("\tClient: %s\n",GETNEWSREADER(myconfig.news.news[j].client));
        if (myconfig.news.news[j].client != -1) {
            printf("\t\tFile: %s\n",myconfig.news.news[j].file);
            for ( i=0 ; i<myconfig.news.news[j].msg_nr ; i++) {
                printmsg("\t\t",myconfig.news.news[j].msg[i]);
            }
        }
    }

    printf("Sensors\n");
    print_interval("\t",myconfig.sensor);
    for ( i=0 ; i<myconfig.sensor.sensor_nr ; i++ ) {
        printf("\tFile: %s\n",myconfig.sensor.sensors[i].file);
        print_new_values("\t\t",myconfig.sensor.sensors[i].values,myconfig.sensor.sensors[i].values_nr);
        for (j=0 ; j<myconfig.sensor.sensors[i].msg_nr ; j++) {
            printmsg("\t\t",myconfig.sensor.sensors[i].msg[j]);
        }
    }

    printf("Soundcard\n");
    print_interval("\t",myconfig.sound);
    for ( i=0 ; i<myconfig.sound.mixer_nr ; i++) {
        printf("\t%s\n",myconfig.sound.mixers[i].mixer);
        for (j=0 ; j<myconfig.sound.mixers[i].msg_nr ; j++) {
            printmsg("\t\t",myconfig.sound.mixers[i].msg[j]);
        }
    }    

    printf("Uptime\n");
    print_interval("\t",myconfig.uptime);
    for ( i=0 ; i<myconfig.uptime.msg_nr ; i++) {
        printmsg("\t",myconfig.uptime.msg[i]);
    }    
}    

// vim: filetype=c:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:encoding=utf-8:textwidth=80
