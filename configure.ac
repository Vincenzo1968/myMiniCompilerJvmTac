
#AC_INIT([myMiniCompilerJvmTac], [1.0.0], [vincenzolocicero68@gmail.com])

m4_define([my_version], m4_esyscmd_s([sed -n 's/^#define APP_VERSION "\(.*\)"/\1/p' src/version.h]))
AC_INIT([myMiniCompilerJvmTac], [my_version], [vincenzolocicero68@gmail.com])

AC_CONFIG_SRCDIR([src/main.c])

AC_CONFIG_HEADERS([config.h])

AM_INIT_AUTOMAKE([-Wall -Werror foreign])

AC_PROG_CC

# Warning utili (equivalente moderno di -Wall -W -pedantic)
CFLAGS="$CFLAGS -Wall -Wextra -pedantic"

AC_PROG_INSTALL

AC_CONFIG_FILES([
    Makefile
    src/Makefile
])

AC_OUTPUT
