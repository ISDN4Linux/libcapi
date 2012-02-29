#
# Copyright (c) 2012 Hans Petter Selasky. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#
#
# Makefile for shared CAPI access library
#

VERSION=2.0.1

OSNAME!= uname

PREFIX?=	/usr/local
LOCALBASE?=	/usr/local
BINDIR?=	${PREFIX}/sbin
MANDIR?=	${PREFIX}/man/man
LIBDIR?=	${PREFIX}/lib
INCLUDEDIR?=	${PREFIX}/include

LIB=		capi20
SHLIB_MAJOR=	1
SHLIB_MINOR=	0

.if (${OSNAME} != NetBSD)
DPADD+=         ${LIBMD}
LDADD+=         -lmd
.endif

CFLAGS+=	-D_GNU_SOURCE

SRCS=		capilib.c
.if defined(HAVE_BINTEC)
SRCS+=		bintec.c
CFLAGS+=	-DHAVE_BINTEC
.endif
INCS=		capi20.h

MKLINT=		no
WARNS=		3

NO_WERROR=
NOGCCERROR=
NO_PROFILE=

.if defined(HAVE_DEBUG)
CFLAGS+=	-DHAVE_DEBUG
CFLAGS+=	-g
.endif

.if defined(HAVE_MAN)
MAN=		capi20.3
MLINKS+=	capi20.3 capi.3
MLINKS+=	capi20.3 capi20_be_alloc_bintec.3
MLINKS+=	capi20.3 capi20_be_alloc_i4b.3
MLINKS+=	capi20.3 capi20_be_free.3
MLINKS+=	capi20.3 capi20_register.3
MLINKS+=	capi20.3 capi20_release.3
MLINKS+=	capi20.3 capi20_put_message.3
MLINKS+=	capi20.3 capi20_get_message.3
MLINKS+=	capi20.3 capi20_wait_for_message.3
MLINKS+=	capi20.3 capi20_get_manufacturer.3
MLINKS+=	capi20.3 capi20_get_version.3
MLINKS+=	capi20.3 capi20_get_serial_number.3
MLINKS+=	capi20.3 capi20_get_profile.3
MLINKS+=	capi20.3 capi20_is_installed.3
MLINKS+=	capi20.3 capi20_fileno.3
MLINKS+=	capi20.3 capi20_encode.3
MLINKS+=	capi20.3 capi20_decode.3
MLINKS+=	capi20.3 capi20_get_errstr.3
MLINKS+=	capi20.3 capi20_command_pack.3
MLINKS+=	capi20.3 capi20_command_unpack.3
.else
MAN=
.endif

package:

	make clean cleandepend HAVE_MAN=YES HAVE_BINTEC=YES

	tar -cvf temp.tar --exclude="*~" --exclude="*#" \
		--exclude=".svn" --exclude="*.orig" --exclude="*.rej" \
		Makefile bintec.c capilib.c capi20.3

	rm -rf libcapi-${VERSION}

	mkdir libcapi-${VERSION}

	tar -xvf temp.tar -C libcapi-${VERSION}

	rm -rf temp.tar

	tar -jcvf libcapi-${VERSION}.tar.bz2 libcapi-${VERSION}

.include <bsd.lib.mk>
