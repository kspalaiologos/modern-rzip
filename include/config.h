
#ifndef _CONFIG_H
#define _CONFIG_H

#define xstr(s) str(s)
#define str(s) #s

#define PACKAGE "modern-rzip"
#define PACKAGE_VERSION xstr(MRZIP_MAJOR) "." xstr(MRZIP_MINOR) "." xstr(MRZIP_PATCH)

#endif
