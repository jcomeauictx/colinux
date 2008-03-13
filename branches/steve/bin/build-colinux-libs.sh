#!/bin/sh

# Build libraries for cross platform mingw32

. ./build-common.sh

# Store version of installed libs here
VERSION_CACHE="$PREFIX/$TARGET/include"

# Current developing build system should not same as target
if [ "$BUILD" = "$TARGET" ]
then
	echo "Fatal error: BUILD = TARGET, that's no cross build!"
	exit -1
fi

download_files()
{
	download_file "$FLTK_ARCHIVE" "$FLTK_URL"
	download_file "$W32API_SRC_ARCHIVE" "$MINGW_URL"
	download_file "$WINPCAP_SRC_ARCHIVE" "$WINPCAP_URL"
	download_file "$WX_ARCHIVE" "$WX_URL"
}

check_md5sums()
{
	echo -n "Check libs: "
	cd "$TOPDIR"

	if [ -f $PREFIX/$TARGET/lib/libfltk.a -a \
	     -f $PREFIX/$TARGET/lib/libwin32k.a -a \
	     -f $PREFIX/$TARGET/lib/libwx_$WX_TOOLKIT.a ]
	then

	    if md5sum -c $W32LIBS_CHECKSUM >/dev/null 2>&1
	    then
		# Check versions
		if [ "`cat $VERSION_CACHE/.fltk.version 2>/dev/null`" = "$FLTK_VERSION" -a \
		     "`cat $VERSION_CACHE/.w32api.version 2>/dev/null`" = "$W32API_VERSION" -a \
		     "`cat $VERSION_CACHE/.winpcap.version 2>/dev/null`" = "$WINPCAP_VERSION" -a \
		     "`cat $VERSION_CACHE/.wx.version 2>/dev/null`" = "$WX_VERSION" ]
		then
		    echo "Skip w32api, libfltk, libwin32k, libwx"
		    echo " - already installed on $PREFIX/$TARGET/lib"
		    exit 0
		else
		    echo "Version don't match"
		fi
	    else
		echo "MD5sum don't match, rebuilding"
	    fi
	else
	    echo "missing, rebuilding"
	fi
}

create_md5sums()
{
	echo "Create md5sum"

	# Save version number into files
	echo "$FLTK_VERSION" >$VERSION_CACHE/.fltk.version
	echo "$W32API_VERSION" >$VERSION_CACHE/.w32api.version
	echo "$WINPCAP_VERSION" >$VERSION_CACHE/.winpcap.version
	echo "$WX_VERSION" >$VERSION_CACHE/.wx.version

	mkdir -p $MD5DIR
	cd "$TOPDIR"
	md5sum -b \
	    patch/$FLTK-win32.diff \
	    $W32API_PATCH \
	    $PREFIX/$TARGET/include/w32api.h \
	    > $W32LIBS_CHECKSUM
	test $? -ne 0 && error_exit 10 "can not create md5sum"
	cd "$BINDIR"
}

#
# FLTK
#

extract_fltk()
{
	echo "Extracting FLTK"
	mkdir -p "$BUILD_DIR"
	cd "$BUILD_DIR"
	rm -rf "$FLTK"
	bzip2 -dc "$SOURCE_DIR/$FLTK_ARCHIVE" | tar x
	test $? -ne 0 && error_exit 10 "FLTK extract failed"
}

patch_fltk()
{
	cd "$BUILD_DIR/$FLTK"
	patch -p1 < "$TOPDIR/patch/$FLTK-win32.diff"
	test $? -ne 0 && error_exit 10 "FLTK patch failed"
}

configure_fltk()
{
	echo "Configuring FLTK"
	cd "$BUILD_DIR/$FLTK"

	# Configure for cross compiling without X11.
	./configure \
	 --build=$BUILD \
	 --host=$TARGET \
	 --prefix=$PREFIX/$TARGET \
	 --without-x >>$COLINUX_BUILD_LOG 2>&1
	test $? -ne 0 && error_exit 1 "FLTK configure failed"

	echo "Making FLTK"
	make -C src >>$COLINUX_BUILD_LOG 2>&1
	test $? -ne 0 && error_exit 1 "FLTK make failed"
}

install_fltk()
{
	echo "Installing FLTK"
	cd "$BUILD_DIR/$FLTK"
	make -C src install || exit 1
	make -C FL install || exit 1
}

build_fltk()
{
	extract_fltk
	patch_fltk
	configure_fltk
	install_fltk
}

#
# w32api_src source
#

extract_w32api_src()
{
	echo "Extracting w32api source"
	cd "$BUILD_DIR"
	rm -rf "$W32API_SRC"
	gzip -dc "$SOURCE_DIR/$W32API_SRC_ARCHIVE" | tar x
	test $? -ne 0 && error_exit 10 "w32api extract failed"
}

patch_w32api_src()
{
	if [ "$W32API_PATCH" != "" ]; then
		echo "Patching w32api - $TOPDIR/$W32API_PATCH"
		cd "$BUILD_DIR/$W32API_SRC"
		patch -p1 < "$TOPDIR/$W32API_PATCH"
		test $? -ne 0 && error_exit 10 "w32api source patch failed"
	fi
}

configure_w32api_src()
{
	echo "Configuring w32api source"
	cd "$BUILD_DIR/$W32API_SRC"
	./configure \
	 --build=$BUILD \
	 --host=$TARGET \
	 --prefix=$PREFIX/$TARGET \
	 CC=$TARGET-gcc >>$COLINUX_BUILD_LOG 2>&1
	test $? -ne 0 && error_exit 1 "w32api source configure failed"

	echo "Making w32api source"
	make >>$COLINUX_BUILD_LOG 2>&1
	test $? -ne 0 && error_exit 1 "w32api source make failed"
}

install_w32api_src()
{
	echo "Installing $W32API_SRC"
	cd "$BUILD_DIR/$W32API_SRC"
	make install >>$COLINUX_BUILD_LOG 2>&1
	test $? -ne 0 && error_exit 1 "w32api make install failed"
}

build_w32api_src()
{
	extract_w32api_src
	patch_w32api_src
	configure_w32api_src
	install_w32api_src
}

#
# WinPCAP
#

extract_winpcap_src()
{
	echo "Extracting winpcap source"
	cd "$BUILD_DIR"
	rm -rf "$WINPCAP_SRC"
	unzip "$SOURCE_DIR/$WINPCAP_SRC_ARCHIVE"
	test $? -ne 0 && error_exit 10 "winpcap extracting failed"
}

install_winpcap_src()
{
	echo "Installing $WINPCAP_SRC"
	cd "$BUILD_DIR/$WINPCAP_SRC"
	cp -p Include/pcap.h \
	    Include/pcap-stdinc.h \
	    Include/pcap-bpf.h \
	    Include/bittypes.h \
	    Include/ip6_misc.h \
	    "$PREFIX/$TARGET/include"
	test $? -ne 0 && error_exit 10 "winpcap install headers failed"
	cp -p Lib/libwpcap.a "$PREFIX/$TARGET/lib"
	test $? -ne 0 && error_exit 10 "winpcap install lib failed"
}

build_winpcap_src()
{
	extract_winpcap_src
	install_winpcap_src
}

#
# wxWidgets
#

extract_wx()
{
	echo "Extracting wxWidgets"
	mkdir -p "$BUILD_DIR"
	cd "$BUILD_DIR"
	rm -rf "$WX"
	gzip -dc "$SOURCE_DIR/$WX_ARCHIVE" | tar x
	test $? -ne 0 && error_exit 10 "wxWidgets extract failed"
}

patch_wx()
{
	if [ -f "$TOPDIR/patch/$WX.diff" ]; then
		cd "$BUILD_DIR/$WX"
		patch -p1 < "$TOPDIR/patch/$WX.diff"
		test $? -ne 0 && error_exit 10 "wxWidgets patch failed"
	fi
}

configure_wx()
{
	echo "Configuring wxWidgets"
	cd "$BUILD_DIR/$WX"

	./configure \
	 --build=$BUILD \
	 --host=$TARGET \
	 --prefix=$PREFIX/$TARGET \
	 --with-$WX_TOOLKIT \
	 `cat $TOPDIR/conf/wxWidgets.conf` \
	 BUILD=release
	>>$COLINUX_BUILD_LOG 2>&1
	status=$?
	test $status -ne 0 && error_exit 1 "wxWidgets configure failed"
}

make_wx()
{
	echo "Making wxWidgets"
	cd "$BUILD_DIR/$WX"
	echo "Build log: $COLINUX_BUILD_LOG"
	make BUILD=release >>$COLINUX_BUILD_LOG 2>&1
	test $? -ne 0 && error_exit 1 "wxWidgets make failed"
}

install_wx()
{
	echo "Installing wxWidgets"
	cd "$BUILD_DIR/$WX"
	make install || exit 1
	cd "$PREFIX/bin" && ln -sf "$PREFIX/$TARGET/bin/wx-config" .
	cd "$PREFIX/$TARGET/lib"
	pwd
	for f in libwx*.a
	do
		n=`echo $f | awk '{ print substr($0,1,index($0,"-")-1) }'`.a
		ln -sf $f $n
	done
	cd "$PREFIX/$TARGET/include"
	ln -sf `ls -td wx-*/wx | head -1` .
}

build_wx()
{
	extract_wx
	patch_wx
	configure_wx
	make_wx
	install_wx
}

# ALL

clean_up()
{
	echo "Cleanup building"

	# Installation should have been successful, so clean-up
	#  after ourselves an little bit.
	cd $BUILD_DIR
	rm -rf "$FLTK" "$W32API_SRC" "$WINPCAP_SRC" "$WX"
}

build_colinux_libs()
{
	# do not check files, if rebuild forced
	test "$1" = "--rebuild" || check_md5sums

	download_files
	# Only Download? Than ready.
	test "$1" = "--download-only" && exit 0

	echo "log: $COLINUX_BUILD_LOG"
	mkdir -p `dirname $COLINUX_BUILD_LOG`

	if [ "`cat $VERSION_CACHE/.fltk.version 2>/dev/null`" != "$FLTK_VERSION" ]; then
		build_fltk
	fi
	if [ "`cat $VERSION_CACHE/.w32api.version 2>/dev/null`" != "$W32API_VERSION" ]; then
		build_w32api_src
	fi
	if [ "`cat $VERSION_CACHE/.winpcap.version 2>/dev/null`" != "$WINPCAP_VERSION" ]; then
		build_winpcap_src
	fi
	if [ "`cat $VERSION_CACHE/.wx.version 2>/dev/null`" != "$WX_VERSION" ]; then
		build_wx
	fi

	clean_up
	create_md5sums
}

build_colinux_libs $1
