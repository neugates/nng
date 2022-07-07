//
// Copyright 2020 Staysail Systems, Inc. <info@staysail.tech>
//
// This software is supplied under the terms of the MIT License, a
// copy of which should be located in the distribution where this
// file was obtained (LICENSE.txt).  A copy of the license may also be
// found online at https://opensource.org/licenses/MIT.
//

// getrandom is not as nice as arc4random, but on platforms where it
// exists and arc4random does not, we should use it.
//
// getrandom will block only if the urandom device is not seeded yet.
// That can only happen during very early boot (earlier than we should
// normally be running.  This is the only time it can fail with correct
// arguments, and then only if it is interrupted with a signal.

#include <sys/random.h>

#include "core/nng_impl.h"

#ifdef NNG_HAVE_GETRANDOM

#include <fcntl.h>
#include <unistd.h>
static int             urandom_fd   = -1;
static pthread_mutex_t urandom_lock = PTHREAD_MUTEX_INITIALIZER;

#ifndef O_CLOEXEC
#define O_CLOEXEC 0u
#endif

uint32_t
nni_random(void)
{
	int      fd;
	uint32_t val;

	// Documentation claims that as long as we are not using
	// GRND_RANDOM and buflen < 256, this should never fail.
	// The exception here is that we could fail if for some
	// reason we got a signal while blocked at very early boot
	// (i.e. /dev/urandom was not yet seeded).
	if (getrandom(&val, sizeof(val), 0) != sizeof(val)) {
		(void) pthread_mutex_lock(&urandom_lock);
		if ((fd = urandom_fd) == -1) {
			if ((fd = open("/dev/urandom", O_RDONLY | O_CLOEXEC)) <
			    0) {
				(void) pthread_mutex_unlock(&urandom_lock);
				nni_panic("failed to open /dev/urandom");
			}
			urandom_fd = fd;
		}
		(void) pthread_mutex_unlock(&urandom_lock);

		if (read(fd, &val, sizeof(val)) != sizeof(val)) {
			nni_panic("failed reading /dev/urandom");
		}
	}
	return (val);
}

#endif