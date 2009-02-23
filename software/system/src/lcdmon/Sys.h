// screenhack - a 3D animation system for Renderman-compatible renderers
// Copyright (C) 2004  Michael Wouters

// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//

// Sys.h
//
// This header file is included at the top of every source file,
// before any other header file.
// It is intended to add defines that are needed globally and
// to work around Operating System dependend incompatibilities.

// EXAMPLE: If you use autoconf you can add the following here.
// #ifdef HAVE_CONFIG_H
// #include "config.h"
// #endif

// EXAMPLE: You could add stuff like this here too
// (Otherwise add -DCWDEBUG to your CFLAGS).
// #if defined(WANTSDEBUGGING) && defined(HAVE_LIBCWD_BLAHBLAH)
// #define CWDEBUG
// #endif

// The following is the libcwd related mandatory part.
// It must be included before any system header file is included!

#ifdef CWDEBUG
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <libcwd/sys.h>
#endif
