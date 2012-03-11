/*  SSNES - A Super Nintendo Entertainment System (SNES) Emulator frontend for libsnes.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 *
 *  Some code herein may be based on code found in BSNES.
 * 
 *  SSNES is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  SSNES is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with SSNES.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <string.h>
#include "../boolean.h"
#include "../strl.h"
#include "../libsnes.hpp"
#include "../input/input_luts.h"
#include "../general.h"
#include <string.h>
#include <ctype.h>

#ifdef HAVE_ZLIB
#include "szlib/zlib.h"
#define WRITEBUFFERSIZE (1024 * 512)
#endif

#ifdef _WIN32
#include "../posix_string.h"
#endif

static char g_rom_ext[1024];

void ssnes_console_set_rom_ext(const char *ext)
{
   strlcpy(g_rom_ext, ext, sizeof(g_rom_ext));
}

const char *ssnes_console_get_rom_ext(void)
{
   const char *id = snes_library_id();

   // SNES9x / bSNES
   if (strstr(id, "SNES"))
      return "smc|fig|sfc|gd3|gd7|dx2|bsx|swc|zip|SMC|FIG|SFC|BSX|GD3|GD7|DX2|SWC|ZIP";
   // FCEU Next
   else if (strstr(id, "FCEU"))
      return "fds|FDS|zip|ZIP|nes|NES|unif|UNIF";
   // VBA Next / Meteor
   else if (strstr(id, "VBA") || strstr(id, "Meteor"))
      return "gb|gbc|gba|GBA|GB|GBC|zip|ZIP";
   // Gambatte
   else if (strstr(id, "gambatte"))
      return "gb|gbc|GB|GBC|zip|ZIP";
   // FBA Next
   else if (strstr(id, "FB Alpha"))
      return "zip|ZIP";
   // Genesis Plus GX/Next
   else if (strstr(id, "Genesis Plus GX"))
      return "md|smd|bin|gen|zip|MD|SMD|bin|GEN|ZIP|sms|SMS|gg|GG|sg|SG";
   else if (*g_rom_ext)
      return g_rom_ext;

   return NULL;
}

void ssnes_console_name_from_id(char *name, size_t size)
{
   if (size == 0)
      return;

   const char *id = snes_library_id();
   if (!id || strlen(id) >= size)
   {
      name[0] = '\0';
      return;
   }

   name[strlen(id)] = '\0';

   for (size_t i = 0; id[i] != '\0'; i++)
   {
      char c = id[i];
      if (isspace(c) || isblank(c))
         name[i] = '_';
      else
         name[i] = tolower(c);
   }
}

void ssnes_console_set_default_keybind_names_for_emulator(void)
{
   const char *id = snes_library_id();

   // Genesis Plus GX/Next
   if (strstr(id, "Genesis Plus GX"))
   {
      strlcpy(ssnes_default_libsnes_keybind_name_lut[SNES_DEVICE_ID_JOYPAD_B],
            "B button", sizeof(ssnes_default_libsnes_keybind_name_lut[SNES_DEVICE_ID_JOYPAD_B]));
      strlcpy(ssnes_default_libsnes_keybind_name_lut[SNES_DEVICE_ID_JOYPAD_A],
            "C button", sizeof(ssnes_default_libsnes_keybind_name_lut[SNES_DEVICE_ID_JOYPAD_A]));
      strlcpy(ssnes_default_libsnes_keybind_name_lut[SNES_DEVICE_ID_JOYPAD_X],
            "Y button", sizeof(ssnes_default_libsnes_keybind_name_lut[SNES_DEVICE_ID_JOYPAD_X]));
      strlcpy(ssnes_default_libsnes_keybind_name_lut[SNES_DEVICE_ID_JOYPAD_Y],
            "A button", sizeof(ssnes_default_libsnes_keybind_name_lut[SNES_DEVICE_ID_JOYPAD_Y]));
      strlcpy(ssnes_default_libsnes_keybind_name_lut[SNES_DEVICE_ID_JOYPAD_L],
            "X button", sizeof(ssnes_default_libsnes_keybind_name_lut[SNES_DEVICE_ID_JOYPAD_L]));
      strlcpy(ssnes_default_libsnes_keybind_name_lut[SNES_DEVICE_ID_JOYPAD_R],
            "Z button", sizeof(ssnes_default_libsnes_keybind_name_lut[SNES_DEVICE_ID_JOYPAD_R]));
      strlcpy(ssnes_default_libsnes_keybind_name_lut[SNES_DEVICE_ID_JOYPAD_SELECT],
            "Mode button", sizeof(ssnes_default_libsnes_keybind_name_lut[SNES_DEVICE_ID_JOYPAD_SELECT]));
   }
}

#ifdef HAVE_ZLIB
static int ssnes_extract_currentfile_in_zip(unzFile uf)
{
   char filename_inzip[PATH_MAX];
   FILE *fout = NULL;

   unz_file_info file_info;
   int err = unzGetCurrentFileInfo(uf,
         &file_info, filename_inzip, sizeof(filename_inzip),
         NULL, 0, NULL, 0);

   if (err != UNZ_OK)
   {
      SSNES_ERR("Error %d with zipfile in unzGetCurrentFileInfo.\n", err);
      return err;
   }

   size_t size_buf = WRITEBUFFERSIZE;
   void *buf = malloc(size_buf);
   if (!buf)
   {
      SSNES_ERR("Error allocating memory\n");
      return UNZ_INTERNALERROR;
   }

   char write_filename[PATH_MAX];

#if defined(__CELLOS_LV2__)
   snprintf(write_filename, sizeof(write_filename), "/dev_hdd1/%s", filename_inzip);
#elif defined(_XBOX)
   snprintf(write_filename, sizeof(write_filename), "cache:\\%s", filename_inzip);
#endif

   err = unzOpenCurrentFile(uf);
   if (err != UNZ_OK)
      SSNES_ERR("Error %d with zipfile in unzOpenCurrentFile.\n", err);
   else
   {
      /* success */
      fout = fopen(write_filename, "wb");

      if (!fout)
         SSNES_ERR("Error opening %s.\n", write_filename);
   }

   if (fout)
   {
      SSNES_LOG("Extracting: %s\n", write_filename);

      do
      {
         err = unzReadCurrentFile(uf, buf, size_buf);
         if (err < 0)
         {
            SSNES_ERR("error %d with zipfile in unzReadCurrentFile.\n", err);
            break;
         }

         if (err > 0)
         {
            if (fwrite(buf, err, 1, fout) != 1)
            {
               SSNES_ERR("Error in writing extracted file.\n");
               err = UNZ_ERRNO;
               break;
            }
         }
      } while (err > 0);

      if (fout)
         fclose(fout);
   }

   if (err == UNZ_OK)
   {
      err = unzCloseCurrentFile (uf);
      if (err != UNZ_OK)
         SSNES_ERR("Error %d with zipfile in unzCloseCurrentFile.\n", err);
   }
   else
      unzCloseCurrentFile(uf); 

   free(buf);
   return err;
}

int ssnes_extract_zipfile(const char *zip_path)
{
   unzFile uf = unzOpen(zip_path); 

   unz_global_info gi;
   int err = unzGetGlobalInfo(uf, &gi);
   if (err != UNZ_OK)
      SSNES_ERR("error %d with zipfile in unzGetGlobalInfo \n",err);

   for (unsigned i = 0; i < gi.number_entry; i++)
   {
      if (ssnes_extract_currentfile_in_zip(uf) != UNZ_OK)
         break;

      if ((i + 1) < gi.number_entry)
      {
         err = unzGoToNextFile(uf);
         if (err != UNZ_OK)
         {
            SSNES_ERR("error %d with zipfile in unzGoToNextFile\n",err);
            break;
         }
      }
   }

   return 0;
}

#endif
