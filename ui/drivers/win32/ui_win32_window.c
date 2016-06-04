/* RetroArch - A frontend for libretro.
 *  Copyright (C) 2015-2016 - Ali Bouhlel
 *  Copyright (C) 2011-2016 - Daniel De Matteis
 *
 * RetroArch is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Found-
 * ation, either version 3 of the License, or (at your option) any later version.
 *
 * RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with RetroArch.
 * If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdint.h>
#include <boolean.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#ifdef _MSC_VER
#pragma comment( lib, "comctl32" )
#endif

#define IDI_ICON 1

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0500 //_WIN32_WINNT_WIN2K
#endif

#ifndef _WIN32_IE
#define _WIN32_IE 0x0300
#endif

#include <windows.h>
#include <commdlg.h>
#include <commctrl.h>

#include <retro_inline.h>
#include <file/file_path.h>

#include "ui_win32_window.h"

#include "../../ui_companion_driver.h"
#include "../../../driver.h"
#include "../../../runloop.h"
#include "../../../tasks/tasks_internal.h"

void ui_window_win32_destroy(void *data)
{
   ui_window_win32_t *window = (ui_window_win32_t*)data;
   DestroyWindow(window->hwnd);
}

void ui_window_win32_set_focused(void *data)
{
   ui_window_win32_t *window = (ui_window_win32_t*)data;
   SetFocus(window->hwnd);
}

void ui_window_win32_set_visible(void *data, 
        bool set_visible)
{
   ui_window_win32_t *window = (ui_window_win32_t*)data;
   ShowWindow(window->hwnd, visible ? SW_SHOWNORMAL : SW_HIDE);
}

void ui_window_win32_set_title(void *data, char *buf)
{
   ui_window_win32_t *window = (ui_window_win32_t*)data;
   SetWindowText(window->hwnd, buf);
}

const ui_window_t ui_window_win32 = {
   ui_window_win32_destroy,
   ui_window_win32_set_focused,
   ui_window_win32_set_visible,
   ui_window_win32_set_title,
   "win32"
};
