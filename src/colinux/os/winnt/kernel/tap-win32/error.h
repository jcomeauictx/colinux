/*
 *  TAP-Win32 -- A kernel driver to provide virtual tap device functionality
 *               on Windows.  Originally derived from the CIPE-Win32
 *               project by Damion K. Wilson, with extensive modifications by
 *               James Yonan.
 *
 *  All source code which derives from the CIPE-Win32 project is
 *  Copyright (C) Damion K. Wilson, 2003, and is released under the
 *  GPL version 2 (see below).
 *
 *  All other source code is Copyright (C) James Yonan, 2003,
 *  and is released under the GPL version 2 (see below).
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program (see the file COPYING included with this
 *  distribution); if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

//-----------------
// DEBUGGING OUTPUT
//-----------------

#define NOTE_ERROR(a) \
{ \
  a->m_LastErrorFilename = __FILE__; \
  a->m_LastErrorLineNumber = __LINE__; \
}

#if DBG

VOID MyAssert (const unsigned char *file, int line);

VOID DebugPacket (const char *prefix,
		  const unsigned char *data,
		  int len);

#define DEBUGP(fmt) { DbgPrint fmt; }

#define MYASSERT(exp) \
{ \
  if (!(exp)) \
    { \
      MyAssert(__FILE__, __LINE__); \
    } \
}

#define DEBUG_PACKET(prefix, data, len) \
  DebugPacket (prefix, data, len)

#else 
#define DEBUGP(fmt)
#define MYASSERT(exp)
#define DEBUG_PACKET(prefix, data, len)
#endif
