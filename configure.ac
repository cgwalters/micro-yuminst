AC_PREREQ([2.63])
AC_INIT([micro-yuminst], [2016.2], [walters@verbum.org])
AC_CONFIG_HEADER([config.h])
AC_CONFIG_MACRO_DIR([buildutil])
AC_CONFIG_AUX_DIR([build-aux])

AM_INIT_AUTOMAKE([1.11 -Wno-portability foreign no-define tar-ustar no-dist-gzip dist-xz subdir-objects])
AM_MAINTAINER_MODE([enable])
AM_SILENT_RULES([yes])
AC_USE_SYSTEM_EXTENSIONS
AC_SYS_LARGEFILE

AC_PROG_CC
AM_PROG_CC_C_O
changequote(,)dnl
if test "x$GCC" = "xyes"; then
  WARN_CFLAGS="-Wall -Wstrict-prototypes -Werror=missing-prototypes \
		-Werror=implicit-function-declaration \
		-Werror=pointer-arith -Werror=init-self -Werror=format=2 \
		-Werror=format-security \
		-Werror=missing-include-dirs -Werror=aggregate-return \
		-Werror=declaration-after-statement"
fi
changequote([,])dnl
AC_SUBST(WARN_CFLAGS)

# Initialize libtool
LT_PREREQ([2.2.4])
LT_INIT([disable-static])

PKG_PROG_PKG_CONFIG

PKG_CHECK_MODULES(PKGDEP_HIF, [gio-unix-2.0 >= 2.44.0 rpm librepo])
dnl bundled libhif
PKGDEP_HIF_CFLAGS="-I $(pwd)/libhif -I $(pwd)/libhif-build $PKGDEP_HIF_CFLAGS"
PKGDEP_HIF_LIBS="-L$(pwd)/libhif-build/libhif -lhif $PKGDEP_HIF_LIBS"

dnl Try to automatically determine cmake type from CFLAGS
if $(echo $CFLAGS |grep -q -e "-O0"); then
  cmake_args="-DCMAKE_BUILD_TYPE=Debug \
        -DCMAKE_C_FLAGS_DEBUG:STRING='-ggdb -O0' \
        -DCMAKE_CXX_FLAGS_DEBUG:STRING=-ggdb -O0'"
  export cmake_args
else
  cmake_args=-DCMAKE_BUILD_TYPE=RelWithDebugInfo
  export cmake_args
fi  

dnl I picked /usr/libexec/ just because we need an
dnl arbitrary path - we don't actually install there.
(set -euo pipefail; mkdir -p libhif-build && cd libhif-build &&
 set -x && 
 cmake \
     -DCMAKE_INSTALL_PREFIX:PATH=/usr/libexec/$PACKAGE \
     -DINCLUDE_INSTALL_DIR:PATH=/usr/libexec/$PACKAGE/include \
     -DLIB_INSTALL_DIR:PATH=/usr/libexec/$PACKAGE \
     -DSYSCONF_INSTALL_DIR:PATH=/usr/libexec/$PACKAGE/etc \
     -DSHARE_INSTALL_PREFIX:PATH=/usr/libexec/$PACKAGE/share \
     -DBUILD_SHARED_LIBS:BOOL=ON \
     ${cmake_args} ../libhif) || exit 1


AC_CONFIG_FILES([
Makefile
])
AC_OUTPUT

echo "
    $PACKAGE $VERSION
"
