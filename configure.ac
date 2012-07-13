dnl Process this file with autoconf to produce a configure script.

AC_PREREQ(2.59)
AC_INIT([TDH_Socket_Plugin_for_MySQL], [0.3.3])

#init

AC_CANONICAL_SYSTEM
AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AC_PREFIX_DEFAULT(/usr)

AC_PROG_CC
AC_PROG_CXX
AC_PROG_CPP
AC_PROG_LN_S
AC_PROG_LIBTOOL
AC_PROG_RANLIB

#FLAGS
PRESET_CFLAGS="$CFLAGS"
PRESET_CXXFLAGS="$CXXFLAGS"
PRESET_LDFLAGS="$LDFLAGS"
CFLAGS="$PRESET_CFLAGS -Wall -fPIC -D_GNU_SOURCE -fno-strict-aliasing"
CXXFLAGS="$PRESET_CXXFLAGS -D_GNU_SOURCE -D__STDC_LIMIT_MACROS"
CXXFLAGS="$CXXFLAGS -Wall -fno-rtti -fno-exceptions -fPIC -DPIC" 

AC_CHECK_LIB([pthread], [main])

EASY_INCLUDES="-I`pwd`/libeasy/include/ -I`pwd`/libeasy/memory -I`pwd`/libeasy/util/"
EASY_INCLUDES="$EASY_INCLUDES -I`pwd`/libeasy/thread/ -I`pwd`/libeasy/io/ -I`pwd`/libeasy/packet/ -I`pwd`/libeasy/packet/http -I."
AC_SUBST(EASY_INCLUDES)

#config
if test "$prefix" = "NONE"; then
	prefix="/usr"
fi

ac_tdhs_debug=
AC_ARG_ENABLE(tdhs-debug,
  [AS_HELP_STRING([--enable-tdhs-debug], [specify whether tdh_socket is build with DEBUG])],[ac_tdhs_debug="$enableval"],[ac_tdhs_debug=no])
AC_MSG_CHECKING([if --enable-tdhs-debug is specified])
AC_MSG_RESULT($ac_tdhs_debug)
if test "$ac_tdhs_debug" = "yes"; then
    AC_DEFINE([TDHS_DEBUG], [1],
              [tdh_socket is build with DEBUG])
fi

ac_tdhs_row_cache=
AC_ARG_ENABLE(tdhs-row-cache,
  [AS_HELP_STRING([--enable-tdhs-row-cache], [specify whether tdh_socket is build with row_cache])],[ac_tdhs_row_cache="$enableval"],[ac_tdhs_row_cache=no])
AC_MSG_CHECKING([if --enable-tdhs-row-cache is specified])
AC_MSG_RESULT($ac_tdhs_row_cache)
if test "$ac_tdhs_row_cache" = "yes"; then
    AC_DEFINE([TDHS_ROW_CACHE], [1],
              [tdh_socket is build with row_cache])
fi


ac_mysql_debug=
AC_ARG_ENABLE(mysql-debug,
  [AS_HELP_STRING([--enable-mysql-debug], [specify whether MySQL is build with DBUG_ON])],[ac_mysql_debug="$enableval"],[ac_mysql_debug=no])
AC_MSG_CHECKING([if --enable-mysql-debug is specified])
AC_MSG_RESULT($ac_mysql_debug)



AC_DEFUN([CONFIG_OPTION_MYSQL],[
  AC_MSG_CHECKING([mysql source])

  MYSQL_SOURCE_VERSION=
  MYSQL_INC=
  ac_mysql_source_dir=
  AC_ARG_WITH([mysql-source],
    [AS_HELP_STRING([--with-mysql-source=PATH], [MySQL source directory PATH])],
    [
      ac_mysql_source_dir=`cd $withval && pwd`
      if test -f "$ac_mysql_source_dir/sql/handler.h" ; then
        MYSQL_INC="-I$ac_mysql_source_dir/sql"
        MYSQL_INC="$MYSQL_INC -I$ac_mysql_source_dir/include"
        MYSQL_INC="$MYSQL_INC -I$ac_mysql_source_dir/regex"
        MYSQL_INC="$MYSQL_INC -I$ac_mysql_source_dir"
        AC_SUBST(MYSQL_INC)
        if test -f "$ac_mysql_source_dir/VERSION"; then
          source "$ac_mysql_source_dir/VERSION"
          MYSQL_SOURCE_VERSION="$MYSQL_VERSION_MAJOR.$MYSQL_VERSION_MINOR.$MYSQL_VERSION_PATCH"
        else
          if test -f "$ac_mysql_source_dir/configure.in"; then
            MYSQL_SOURCE_VERSION=`cat $ac_mysql_source_dir/configure.in | grep "\[[MySQL Server\]]" | sed -e "s|.*\([[0-9]]\+\.[[0-9]]\+\.[[0-9]]\+[[0-9a-zA-Z\_\-]]*\).*|\1|"`
          else
            AC_MSG_ERROR([invalid MySQL source directory: $ac_mysql_source_dir])
          fi
        fi
        AC_MSG_RESULT([yes: Using $ac_mysql_source_dir, version $MYSQL_SOURCE_VERSION])
      else
        AC_MSG_ERROR([invalid MySQL source directory: $ac_mysql_source_dir])
      fi
    ],
    [AC_MSG_ERROR([--with-mysql-source=PATH is required for standalone build])]
  )

  MYSQL_BIN_VERSION=
  ac_mysql_config=
  AC_ARG_WITH([mysql-bindir],
    [AS_HELP_STRING([--with-mysql-bindir=PATH], [MySQL binary directory PATH. This should be the directory where mysql_config is located.])],
    [
      mysql_bin_dir=`cd $withval 2> /dev/null && pwd || echo ""`
      ac_mysql_config="$mysql_bin_dir/mysql_config"
    ],
    [
      AC_PATH_PROG([ac_mysql_config], [mysql_config])
    ]
  )

  AC_MSG_CHECKING([mysql binary])
  if test ! -x "$ac_mysql_config" ; then
    AC_MSG_ERROR([mysql_config not found! You have to specify the directory where mysql_config resides to --with-mysql-bindir=PATH.])
  fi

  MYSQL_CFLAGS_ADD=`"$ac_mysql_config" --cflags`
  MYSQL_CFLAGS="$MYSQL_CFLAGS $MYSQL_CFLAGS_ADD"
  if test "$ac_mysql_debug" = "yes"; then
    MYSQL_CFLAGS="$MYSQL_CFLAGS -DDBUG_ON -DSAFE_MUTEX -DENABLED_DEBUG_SYNC"
  else
    MYSQL_CFLAGS="$MYSQL_CFLAGS -DDBUG_OFF"
  fi
  AC_SUBST(MYSQL_CFLAGS)

  MYSQL_BIN_VERSION=`"$ac_mysql_config" --version`
  AC_MSG_RESULT([yes: Using $ac_mysql_config, version $MYSQL_BIN_VERSION])

  MYSQL_LIB=`"$ac_mysql_config" --libs_r`
  LIB_DIR=`echo $MYSQL_LIB | sed -e "s|.*-L/|/|" | sed -e "s| .*||"`
    # FIXME
  if test a`basename "$LIB_DIR"` = amysql ; then
    MYSQL_LIB="-L`dirname $LIB_DIR` $MYSQL_LIB" 
      # FIXME
  fi
  AC_SUBST(MYSQL_LIB)

  if test a$MYSQL_SOURCE_VERSION != a$MYSQL_BIN_VERSION ; then
    AC_MSG_ERROR([MySQL source version does not match MySQL binary version])
  fi

  AC_MSG_CHECKING([mysql plugin dir])
  ac_mysql_plugin_dir=
  AC_ARG_WITH([mysql-plugindir],
    [AS_HELP_STRING([--with-mysql-plugindir=PATH], [MySQL plugin directory where tdhsocket.so to be copied])],
    [
      ac_mysql_plugin_dir=`cd $withval && pwd`
      if test -d "$ac_mysql_plugin_dir/" ; then
        PLUGIN_DIR="$ac_mysql_plugin_dir"
        AC_SUBST(PLUGIN_DIR)
        AC_MSG_RESULT([yes: Using $ac_mysql_plugin_dir])
      else
        AC_MSG_ERROR([invalid MySQL plugin directory : $ac_mysql_plugin_dir])
      fi
    ],
    [
      LIB_DIR_TMP=`"$ac_mysql_config" --plugindir`
      if test ! -d "$LIB_DIR_TMP"; then
        LIB_DIR_TMP=`"$ac_mysql_config" --libs_r | sed -e "s|.*-L/|/|" | sed -e "s| .*||"`/plugin
          # FIXME
      fi
      ac_mysql_plugin_dir=$LIB_DIR_TMP
      PLUGIN_DIR="$ac_mysql_plugin_dir"
      AC_SUBST(PLUGIN_DIR)
      AC_MSG_RESULT([--with-mysql-plugindir was not set. Using $ac_mysql_plugin_dir])
    ]
  )
])



AC_MSG_CHECKING(whether GCC atomic builtins are available)
# either define HAVE_IB_GCC_ATOMIC_BUILTINS or not
AC_TRY_RUN(
  [
    int main()
    {
long	x;
long	y;
long	res;
char	c;

x = 10;
y = 123;
res = __sync_bool_compare_and_swap(&x, x, y);
if (!res || x != y) {
        return(1);
      }

x = 10;
y = 123;
res = __sync_bool_compare_and_swap(&x, x + 1, y);
if (res || x != 10) {
        return(1);
      }

x = 10;
y = 123;
res = __sync_add_and_fetch(&x, y);
if (res != 123 + 10 || x != 123 + 10) {
  return(1);
}

c = 10;
res = __sync_lock_test_and_set(&c, 123);
if (res != 10 || c != 123) {
  return(1);
}

return(0);
    }
  ],
  [
    AC_DEFINE([HAVE_IB_GCC_ATOMIC_BUILTINS], [1],
              [GCC atomic builtins are available])
    AC_MSG_RESULT(yes)
  ],
  [
    AC_MSG_RESULT(no)
  ]
)

AC_MSG_CHECKING(whether pthread_t can be used by GCC atomic builtins)
# either define HAVE_IB_ATOMIC_PTHREAD_T_GCC or not
AC_TRY_RUN(
  [
    #include <pthread.h>
    #include <string.h>

    int main(int argc, char** argv) {
      pthread_t       x1;
      pthread_t       x2;
      pthread_t       x3;

memset(&x1, 0x0, sizeof(x1));
memset(&x2, 0x0, sizeof(x2));
memset(&x3, 0x0, sizeof(x3));

      __sync_bool_compare_and_swap(&x1, x2, x3);

      return(0);
    }
  ],
  [
    AC_DEFINE([HAVE_IB_ATOMIC_PTHREAD_T_GCC], [1],
              [pthread_t can be used by GCC atomic builtins])
    AC_MSG_RESULT(yes)
  ],
  [
    AC_MSG_RESULT(no)
  ]
)

AC_MSG_CHECKING(whether Solaris libc atomic functions are available)
# either define HAVE_IB_SOLARIS_ATOMICS or not
AC_CHECK_FUNCS(atomic_add_long \
	 atomic_cas_32 \
	 atomic_cas_64 \
	 atomic_cas_ulong,

	 AC_DEFINE([HAVE_IB_SOLARIS_ATOMICS], [1],
		   [Define to 1 if Solaris libc atomic functions \
		    are available])
)

AC_MSG_CHECKING(whether pthread_t can be used by Solaris libc atomic functions)
# either define HAVE_IB_ATOMIC_PTHREAD_T_SOLARIS or not
AC_TRY_RUN(
  [
    #include <pthread.h>
    #include <string.h>

    int main(int argc, char** argv) {
      pthread_t       x1;
      pthread_t       x2;
      pthread_t       x3;

      memset(&x1, 0x0, sizeof(x1));
      memset(&x2, 0x0, sizeof(x2));
      memset(&x3, 0x0, sizeof(x3));

      if (sizeof(pthread_t) == 4) {
      
        atomic_cas_32(&x1, x2, x3);
      
      } else if (sizeof(pthread_t) == 8) {
      
        atomic_cas_64(&x1, x2, x3);
      
      } else {
      
        return(1);
      }

return(0);
    }
  ],
  [
    AC_DEFINE([HAVE_IB_ATOMIC_PTHREAD_T_SOLARIS], [1],
              [pthread_t can be used by solaris atomics])
    AC_MSG_RESULT(yes)
  ],
  [
    AC_MSG_RESULT(no)
  ]
)



TDHSOCKET_SUBDIRS="libeasy libtdhs TDHSocket"
CONFIG_OPTION_MYSQL
AC_SUBST(TDHSOCKET_SUBDIRS)

AC_CONFIG_FILES(Makefile 
                 libeasy/Makefile
                 libeasy/include/Makefile
                 libeasy/memory/Makefile
                 libeasy/util/Makefile
                 libeasy/thread/Makefile
                 libeasy/io/Makefile
                 libeasy/packet/Makefile
				 libtdhs/Makefile 
				 TDHSocket/Makefile 
				 )
AC_OUTPUT

