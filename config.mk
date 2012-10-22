CFLAGS=-O3 -march=i686 -fomit-frame-pointer -pipe
P_CMD=1
P_CPU=1
P_DISKS=1
P_MBOX=1
P_MEM=1
# under developing...
# P_MOC=1
P_MPD=1
P_NET=1
P_NEWS=1
P_SENSOR=1
P_SND=1
P_TIME=1
P_UPTIME=1
ifeq ($(P_CMD),1)
	PLUGINS+=-DPLUGIN_CMD
endif
ifeq ($(P_CPU),1)
	PLUGINS+=-DPLUGIN_CPU
endif
ifeq ($(P_DISKS),1)
	PLUGINS+=-DPLUGIN_DISKS
endif	
ifeq ($(P_MBOX),1)
        PLUGINS+=-DPLUGIN_MBOX
endif        
ifeq ($(P_MEM),1)
	PLUGINS+=-DPLUGIN_MEM
endif
#ifeq ($(P_MOC),1)
#	PLUGINS+=-DPLUGIN_MOC
#endif	
ifeq ($(P_MPD),1)
	PLUGINS+=-DPLUGIN_MPD
endif	
ifeq ($(P_NET),1)
	PLUGINS+=-DPLUGIN_NET
endif
ifeq ($(P_NEWS),1)
        PLUGINS+=-DPLUGIN_NEWS
        LD_OPTS+=`pkg-config --libs sqlite3`
        CFLAGS+=`pkg-config --cflags sqlite3`
endif
ifeq ($(P_SENSOR),1)
	PLUGINS+=-DPLUGIN_SENSORS
endif	
ifeq ($(P_SND),1)
	PLUGINS+=-DPLUGIN_SOUND
	LD_OPTS+=`pkg-config --libs alsa`
	CFLAGS+=`pkg-config --cflags alsa`
endif
ifeq ($(P_TIME),1)
	PLUGINS+=-DPLUGIN_TIME
endif
ifeq ($(P_UPTIME),1)
	PLUGINS+=-DPLUGIN_UPTIME
endif	

DESTDIR=/usr/
