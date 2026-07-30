// time_config.h — časová zóna a NTP servery
#ifndef TIME_CONFIG_H
#define TIME_CONFIG_H

#ifndef NTP_TIMEZONE
#define NTP_TIMEZONE "CET-1CEST,M3.5.0,M10.5.0/3"
#endif

#ifndef NTP_SERVER1
#define NTP_SERVER1 "pool.ntp.org"
#endif

#ifndef NTP_SERVER2
#define NTP_SERVER2 "time.cloudflare.com"
#endif

#ifndef NTP_SERVER3
#define NTP_SERVER3 "europe.pool.ntp.org"
#endif

#ifndef NTP_RESYNC_HOURS
#define NTP_RESYNC_HOURS 24
#endif

#endif
