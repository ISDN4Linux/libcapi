/*-
 * Copyright (c) 2014 Hans Petter Selasky. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *---------------------------------------------------------------------------
 *
 *	capiclient.c - Implementation of Remote CAPI over TCP
 *	-----------------------------------------------------
 *
 *---------------------------------------------------------------------------*/

/* system includes */
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <sys/endian.h>
#include <sys/ioctl.h>
#include <sys/filio.h>
#include <sys/socket.h>
#include <sys/ioccom.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <errno.h>
#include <err.h>
#include <netdb.h>


#define CAPI_MAKE_IOCTL
#include "capi20.h"

#include "capilib.h"

#define	CAPISERVER_HDR_SIZE 4		/* bytes */

/* define supported commands */
enum {
	CAPISERVER_CMD_CAPI_MSG,
	CAPISERVER_CMD_CAPI_REGISTER,
	CAPISERVER_CMD_CAPI_MANUFACTURER,
	CAPISERVER_CMD_CAPI_VERSION,
	CAPISERVER_CMD_CAPI_SERIAL,
	CAPISERVER_CMD_CAPI_PROFILE,
	CAPISERVER_CMD_CAPI_START,
};

/*---------------------------------------------------------------------------*
 *	capi20_be_alloc_client - Allocate a CAPI remote client backend
 *
 * @param cbe_pp	 Pointer to pointer that should be pointed to backend.
 *
 * @retval 0             Backend allocation was successful.
 *
 * @retval Else          An error happened.
 *---------------------------------------------------------------------------*/
uint16_t
capi20_be_alloc_client(struct capi20_backend **cbe_pp)
{
	struct capi20_backend *cbe;

	if (cbe_pp == NULL)
		return (CAPI_ERROR_INVALID_PARAM);

	cbe = malloc(sizeof(*cbe));
	if(cbe == NULL)
		return (CAPI_ERROR_OS_RESOURCE_ERROR);

	*cbe_pp = cbe;

	memset(cbe, 0, sizeof(*cbe));

	cbe->bBackendType = CAPI_BACKEND_TYPE_CLIENT;

	return (0);
}

/*---------------------------------------------------------------------------*
 *	capilib_alloc_app_client - Allocate a remote CAPI application
 *
 * @param cbe                      Pointer to CAPI backend.
 *
 * @retval 0                       Application allocation failed.
 *
 * @retval Else                    Pointer to CAPI Application.
 *---------------------------------------------------------------------------*/
struct app_softc *
capilib_alloc_app_client(struct capi20_backend *cbe)
{
	struct addrinfo hints;
	struct addrinfo *res;
	struct addrinfo *res0;
	struct app_softc *sc;
	int s;
	int error;

	sc = capilib_alloc_app_sub(cbe);
	if (sc == NULL)
		return NULL;

	bzero(&hints, sizeof(hints));

	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	error = getaddrinfo(cbe->sHostName, cbe->sServName, &hints, &res0);
	if (error) {
	    capilib_free_app(sc);
	    return NULL;
	}

	s = -1;
	for (res = res0; res; res = res->ai_next) {
	    s = socket(res->ai_family, res->ai_socktype,
		       res->ai_protocol);
	    if (s < 0) {
		continue;
	    }
	    if (1) {
		int temp;

		temp = 1;
		setsockopt(s, IPPROTO_TCP, TCP_NODELAY, &temp, (int)sizeof(temp));

		/* use small buffer sizes to force regular ACK-ing */
		temp = 1500;
		setsockopt(s, SOL_SOCKET, SO_SNDBUF, &temp, (int)sizeof(temp));

		/* use small buffer sizes to force regular ACK-ing */
		temp = 1500;
		setsockopt(s, SOL_SOCKET, SO_RCVBUF, &temp, (int)sizeof(temp));
	    }
	    if (connect(s, res->ai_addr, res->ai_addrlen) < 0) {
		close(s);
		s = -1;
		continue;
	    }
	    break;
	}

	freeaddrinfo(res0);

	if (s < 0) {
	    capilib_free_app(sc);
	    return NULL;
	}

	sc->sc_fd = s;

	return (sc);
}

/*---------------------------------------------------------------------------*
 *      capilib_client_do_sync_cmd - remote CAPI client IOCTL wrapper
 *
 * @param sc             Pointer to Application Softc.
 *
 * @param cmd            command
 * @param data           data for command
 * @param len            length of data for command
 *
 * @retval 0             Success.
 *
 * @retval Else          An error happened.
 *---------------------------------------------------------------------------*/
static int
capilib_client_do_sync_cmd(struct app_softc *sc, uint8_t cmd, void *data, int len)
{
	uint8_t header[CAPISERVER_HDR_SIZE] __aligned(4);
	int temp;

	header[0] = len & 0xFF;
	header[1] = (len >> 8) & 0xFF;
	header[2] = cmd;
	header[3] = 0;

	if (write(sc->sc_fd, header, sizeof(header)) != sizeof(header))
		return (-1);
	if (write(sc->sc_fd, data, len) != len)
		return (-1);
	if (capilib_read(sc->sc_fd, header, sizeof(header), 0) != sizeof(header))
		return (-1);
	if (header[2] != cmd)
		return (-1);
	if (header[3] != 0)
		return (-1);
	temp = (header[1] << 8) | header[0];
	if (temp != len)
		return (-1);
	if (capilib_read(sc->sc_fd, data, len, 0) != len)
		return (-1);

	return (0);
}

#define	CAPI_FWD(x) (x) = htole32(x)
#define	CAPI_REV(x) (x) = le32toh(x)

/*---------------------------------------------------------------------------*
 *	capilib_client_do_ioctl - remote CAPI IOCTL wrapper
 *
 * @param sc             Pointer to Application Softc.
 *
 * @param cmd            Command to perform.
 *
 * @param data           Associated command data.
 *
 * @retval 0             Success.
 *
 * @retval Else          An error happened.
 *---------------------------------------------------------------------------*/
int
capilib_client_do_ioctl(struct app_softc *sc, uint32_t cmd, void *data)
{
	int error = 0;

	switch (cmd) {
	case FIONBIO:
		return (ioctl(sc->sc_fd, FIONBIO, data));

	case CAPI_REGISTER_REQ:
		if (1) {
			struct capi_register_req *req = data;
			CAPI_FWD(req->max_logical_connections);
			CAPI_FWD(req->max_b_data_blocks);
			CAPI_FWD(req->max_b_data_len);
			CAPI_FWD(req->max_msg_data_size);
			CAPI_FWD(req->app_id);
		}
		error = capilib_client_do_sync_cmd(sc, CAPISERVER_CMD_CAPI_REGISTER,
		    data, IOCPARM_LEN(cmd));
		if (1) {
			struct capi_register_req *req = data;
			CAPI_REV(req->max_logical_connections);
			CAPI_REV(req->max_b_data_blocks);
			CAPI_REV(req->max_b_data_len);
			CAPI_REV(req->max_msg_data_size);
			CAPI_REV(req->app_id);
		}
		break;

	case CAPI_GET_MANUFACTURER_REQ:
		if (1) {
			uint32_t *pcontroller = data;
			CAPI_FWD(*pcontroller);
		}
		error = capilib_client_do_sync_cmd(sc, CAPISERVER_CMD_CAPI_MANUFACTURER,
		    data, IOCPARM_LEN(cmd));
		if (1) {
			uint32_t *pcontroller = data;
			CAPI_REV(*pcontroller);
		}
		break;

	case CAPI_GET_VERSION_REQ:
		if (1) {
			uint32_t *pcontroller = data;
			CAPI_FWD(*pcontroller);
		}
		error = capilib_client_do_sync_cmd(sc, CAPISERVER_CMD_CAPI_VERSION,
		    data, IOCPARM_LEN(cmd));
		if (1) {
			uint32_t *pcontroller = data;
			CAPI_REV(*pcontroller);
		}
		break;

	case CAPI_GET_SERIAL_REQ:
		if (1) {
			uint32_t *pcontroller = data;
			CAPI_FWD(*pcontroller);
		}
		error = capilib_client_do_sync_cmd(sc, CAPISERVER_CMD_CAPI_SERIAL,
		    data, IOCPARM_LEN(cmd));
		if (1) {
			uint32_t *pcontroller = data;
			CAPI_REV(*pcontroller);
		}
		break;

	case CAPI_GET_PROFILE_REQ:
		if (1) {
			uint32_t *pcontroller = data;
			CAPI_FWD(*pcontroller);
		}
		error = capilib_client_do_sync_cmd(sc, CAPISERVER_CMD_CAPI_PROFILE,
		    data, IOCPARM_LEN(cmd));
		if (1) {
			uint32_t *pcontroller = data;
			CAPI_REV(*pcontroller);
		}
		break;

	case CAPI_START_D_CHANNEL_REQ:
		error = capilib_client_do_sync_cmd(sc, CAPISERVER_CMD_CAPI_START,
		    data, IOCPARM_LEN(cmd));
		break;

	case CAPI_SET_STACK_VERSION_REQ:
	case CAPI_IOCTL_TEST_REQ:
		errno = 0;
		return (0);
	default:
		errno = ENOTTY;
		return (-1);
	}

	if (error) {
		errno = ENOTTY;
		return (-1);
	}
	return (0);
}

