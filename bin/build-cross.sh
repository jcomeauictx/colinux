#!/bin/sh

source ./build-common.sh

# Updated by Sam Lantinga <slouken@libsdl.org>
# These are the files from the current MingW release

GCC=gcc-3.3.1-20030804-1
GCC_ARCHIVE1=gcc-core-3.3.1-20030804-1-src.tar.gz
GCC_ARCHIVE2=gcc-g++-3.3.1-20030804-1-src.tar.gz
GCC_PATCH=""
BINUTILS=binutils-2.15.90-20040222-1
BINUTILS_ARCHIVE=$BINUTILS-src.tar.gz
MINGW=mingw-runtime-3.3
MINGW_ARCHIVE=$MINGW.tar.gz
W32API_ARCHIVE=$W32API.tar.gz

CHECKSUM_FILE=$SRCDIR/.build-cross.md5

download_files()
{
	mkdir -p "$SRCDIR"
	
	download_file "$GCC_ARCHIVE1" "$MINGW_URL"
	download_file "$GCC_ARCHIVE2" "$MINGW_URL"
	download_file "$BINUTILS_ARCHIVE" "$MINGW_URL"
	download_file "$MINGW_ARCHIVE" "$MINGW_URL"
	download_file "$W32API_ARCHIVE" "$MINGW_URL"
}

check_md5sums()
{
	echo "Check md5sum"
	if md5sum --check $CHECKSUM_FILE >>$COLINUX_BUILD_LOG 2>&1 ; then
		echo "Skip $TARGET-windres,$TARGET-ar,$TARGET-gcc"
		echo " - already installed on $PREFIX/bin"
		exit 0
	fi
}

create_md5sums()
{
	echo "Create md5sum"
	md5sum --binary \
	    $GCC_PATCH \
	    $PREFIX/bin/$TARGET-windres \
	    $PREFIX/bin/$TARGET-ar \
	    $PREFIX/bin/$TARGET-gcc \
	    $PREFIX/$TARGET/bin/gcc \
	    $PREFIX/$TARGET/bin/strip \
	    > $CHECKSUM_FILE
	test $? -ne 0 && error_exit 1 "can not create md5sum"
	if [ "$GCC_PATCH" != "" ]; then
		md5sum --binary $SRCDIR/$GCC_PATCH >> $CHECKSUM_FILE
	fi
}


install_libs()
{
	echo "Installing cross libs and includes"
	mkdir -p "$PREFIX/$TARGET"
	cd "$PREFIX/$TARGET"
	gzip -dc "$SRCDIR/$MINGW_ARCHIVE" | tar x
	gzip -dc "$SRCDIR/$W32API_ARCHIVE" | tar x
	cd "$TOPDIR"
}

extract_binutils()
{
	cd "$SRCDIR"
	rm -rf "$BINUTILS"
	echo "Extracting binutils"
	gzip -dc "$SRCDIR/$BINUTILS_ARCHIVE" | tar x
	cd "$TOPDIR"
}

configure_binutils()
{
	cd "$TOPDIR"
	rm -rf "binutils-$TARGET"
	mkdir "binutils-$TARGET"
	cd "binutils-$TARGET"
	echo "Configuring binutils"
	"$SRCDIR/$BINUTILS/configure" --prefix="$PREFIX" --target=$TARGET &> configure.log
	cd "$TOPDIR"
}

build_binutils()
{
	cd "$TOPDIR/binutils-$TARGET"
	echo "Building binutils"
	make >>$COLINUX_BUILD_LOG 2>&1
	test $? -ne 0 && error_exit 1 "make binutils failed"
	cd "$TOPDIR"
}

install_binutils()
{
	cd "$TOPDIR/binutils-$TARGET"
	echo "Installing binutils"
	make install >>$COLINUX_BUILD_LOG 2>&1
	test $? -ne 0 && error_exit 1 "install binutils failed"
	cd "$TOPDIR"
}

extract_gcc()
{
	cd "$SRCDIR"
	rm -rf "$GCC"
	echo "Extracting gcc"
	gzip -dc "$SRCDIR/$GCC_ARCHIVE1" | tar x
	gzip -dc "$SRCDIR/$GCC_ARCHIVE2" | tar x
	cd "$TOPDIR"
}

patch_gcc()
{
	if [ "$GCC_PATCH" != "" ]; then
		echo "Patching gcc"
		cd "$SRCDIR/$GCC"
		patch -p1 < "$SRCDIR/$GCC_PATCH"
		cd "$TOPDIR"
	fi
}

configure_gcc()
{
	cd "$TOPDIR"
	rm -rf "gcc-$TARGET"
	mkdir "gcc-$TARGET"
	cd "gcc-$TARGET"
	echo "Configuring gcc"
	"$SRCDIR/$GCC/configure" -v \
		--prefix="$PREFIX" --target=$TARGET \
		--with-headers="$PREFIX/$TARGET/include" \
		--with-gnu-as --with-gnu-ld \
		--without-newlib --disable-multilib >>$COLINUX_BUILD_LOG 2>&1
	test $? -ne 0 && error_exit 1 "configure gcc failed"
	cd "$TOPDIR"
}

build_gcc()
{
	cd "$TOPDIR/gcc-$TARGET"
	echo "Building gcc"
	make LANGUAGES="c c++" >>$COLINUX_BUILD_LOG 2>&1
	test $? -ne 0 && error_exit 1 "make gcc failed"
	cd "$TOPDIR"
}

install_gcc()
{
	cd "$TOPDIR/gcc-$TARGET"
	echo "Installing gcc"
	make LANGUAGES="c c++" install >>$COLINUX_BUILD_LOG 2>&1
	test $? -ne 0 && error_exit 1 "install gcc failed"
	cd "$TOPDIR"
}

final_tweaks()
{
	echo "Finalizing installation"

	# remove gcc build headers
	rm -rf "$PREFIX/$TARGET/sys-include"

        # Add extra binary links
	if [ ! -f "$PREFIX/$TARGET/bin/objdump" ]; then
		ln "$PREFIX/bin/$TARGET-objdump" "$PREFIX/$TARGET/bin/objdump"
	fi

	# make cc and c++ symlinks to gcc and g++
	if [ ! -f "$PREFIX/$TARGET/bin/g++" ]; then
		ln "$PREFIX/bin/$TARGET-g++" "$PREFIX/$TARGET/bin/g++"
	fi
	if [ ! -f "$PREFIX/$TARGET/bin/cc" ]; then
		ln -s "gcc" "$PREFIX/$TARGET/bin/cc"
	fi
	if [ ! -f "$PREFIX/$TARGET/bin/c++" ]; then
		ln -s "g++" "$PREFIX/$TARGET/bin/c++"
	fi

	# strip all the binaries
	ls "$PREFIX"/bin/* "$PREFIX/$TARGET"/bin/* | egrep -v '.dll$' |
	while read file; do
		strip "$file"
	done
	
	# Installation should have been successful, so clean-up
	#  after ourselves an little bit.
	rm -rf *i686-pc-mingw32

	echo "Installation complete!"
}

build_cross()
{
        download_files
	# Only Download? Than ready.
	test "$1" = "--download-only" && exit 0

	# do not check files, if rebuild forced
	test "$1" = "--rebuild-all" || check_md5sums

        install_libs

        extract_binutils
        configure_binutils
        build_binutils
        install_binutils

        extract_gcc
        patch_gcc
        configure_gcc
        build_gcc
        install_gcc

        final_tweaks
	create_md5sums
}

build_cross $1
