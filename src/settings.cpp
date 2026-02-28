#include "settings.h"
#include "config.h"
#include "input.h"
#include <sdk/os/mcs.h>

#define MCS_FOLDER   "FSandSim"
#define MCS_VAR_BRUSH "BrushSz"

// Initialize MCS folder and load persisted settings
void initSettings() {
  // Create folder (MCS_FOLDER_EXISTS just means it already exists, which is fine)
  enum MCS_Error folderResult = MCS_CreateFolder(MCS_FOLDER, nullptr);
  if (folderResult != MCS_OK && folderResult != MCS_FOLDER_EXISTS) {
    return; // Cannot create folder; skip load
  }

  // Attempt to read persisted brush size
  void *data     = nullptr;
  uint32_t size  = 0;
  enum MCS_VariableType vtype;
  char *name2    = nullptr;

  if (MCS_GetVariable(MCS_FOLDER, MCS_VAR_BRUSH, &vtype, &name2, &data, &size)
      == MCS_OK && data && size >= 1) {
    int val = static_cast<char *>(data)[0] - '0';
    if (val >= BRUSH_SIZE_MIN && val <= BRUSH_SIZE_MAX) {
      brushSize = val;
    }
  }
}

// Persist current brush size as a one-character string ("1".."9")
void saveBrushSize() {
  char buf[2] = { static_cast<char>('0' + brushSize), '\0' };
  // Ignore result: if the folder was deleted mid-session the save silently fails
  enum MCS_Error saveResult = MCS_SetVariable(MCS_FOLDER, MCS_VAR_BRUSH, VARTYPE_STR, 2, buf);
  if (saveResult != MCS_OK) {
    // Re-create the folder and retry once
    enum MCS_Error retryFolder = MCS_CreateFolder(MCS_FOLDER, nullptr);
    if (retryFolder == MCS_OK || retryFolder == MCS_FOLDER_EXISTS) {
      if (MCS_SetVariable(MCS_FOLDER, MCS_VAR_BRUSH, VARTYPE_STR, 2, buf) != MCS_OK) {
        // Give up silently; setting will be lost for this session
      }
    }
  }
}
