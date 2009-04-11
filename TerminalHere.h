/*
 *  TerminalHere - A contextual menu plugin to open a terminal to the selected folder.
 *  Copyright (C) 2009 Alexandre Beaulieu
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
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef __TERMINAL_HERE_H__
#define __TERMINAL_HERE_H__

#include <Carbon/Carbon.h>
#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFPlugInCOM.h>
#include <ApplicationServices/ApplicationServices.h>
#include <CoreServices/CoreServices.h>

#ifdef __DEBUG__
  #define DEBUG_PRINT printf
  #define DEBUG_CFSTR printCFString
#else
  #define DEBUG_PRINT
  #define DEBUG_CFSTR
#endif

typedef struct TerminalHerePlugin {
  ContextualMenuInterfaceStruct *cmInterface;
  CFUUIDRef factoryId;
  UInt32 refCount;
} TerminalHerePlugin;

#endif // __TERMINAL_HERE_H__