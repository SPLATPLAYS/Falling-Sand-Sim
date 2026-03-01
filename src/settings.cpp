#include "settings.h"
#include "config.h"
#include "input.h"
#include <sdk/os/mcs.h>

#define MCS_FOLDER      "FSandSim"
#define MCS_VAR_BRUSH   "BrushSz"
#define MCS_VAR_OCLOCK  "OCLevel"
#define MCS_VAR_SIMSPD  "SimSpd"

int overclockLevel = OC_LEVEL_DEFAULT;
int simSpeedMode   = SIM_SPEED_MODE_DEFAULT;

// Lookup tables defined here (declared extern in config.h)
const char* const simSpeedModeNames[SIM_SPEED_MODE_MAX + 1] = {
  "NORMAL",  // mode 0: no skip
  "X2",      // mode 1: skip 1  (render every 2nd physics frame)
  "X3",      // mode 2: skip 2  (render every 3rd)
  "X5",      // mode 3: skip 4  (render every 5th)
  "X9",      // mode 4: skip 8  (render every 9th)
};
const int simSkipAmounts[SIM_SPEED_MODE_MAX + 1] = {0, 1, 2, 4, 8};

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

  // Load persisted overclock level
  data  = nullptr;
  size  = 0;
  name2 = nullptr;
  if (MCS_GetVariable(MCS_FOLDER, MCS_VAR_OCLOCK, &vtype, &name2, &data, &size)
      == MCS_OK && data && size >= 1) {
    int val = static_cast<char *>(data)[0] - '0';
    if (val >= OC_LEVEL_MIN && val <= OC_LEVEL_MAX) {
      overclockLevel = val;
    }
  }

  // Load persisted simulation speed mode
  data  = nullptr;
  size  = 0;
  name2 = nullptr;
  if (MCS_GetVariable(MCS_FOLDER, MCS_VAR_SIMSPD, &vtype, &name2, &data, &size)
      == MCS_OK && data && size >= 1) {
    int val = static_cast<char *>(data)[0] - '0';
    if (val >= 0 && val <= SIM_SPEED_MODE_MAX) {
      simSpeedMode = val;
    }
  }
}

// Persist current simulation speed mode as a one-character string ("0".."4")
void saveSimSpeedMode() {
  char buf[2] = { static_cast<char>('0' + simSpeedMode), '\0' };
  enum MCS_Error saveResult = MCS_SetVariable(MCS_FOLDER, MCS_VAR_SIMSPD, VARTYPE_STR, 2, buf);
  if (saveResult != MCS_OK) {
    enum MCS_Error retryFolder = MCS_CreateFolder(MCS_FOLDER, nullptr);
    if (retryFolder == MCS_OK || retryFolder == MCS_FOLDER_EXISTS) {
      if (MCS_SetVariable(MCS_FOLDER, MCS_VAR_SIMSPD, VARTYPE_STR, 2, buf) != MCS_OK) {
        // Give up silently
      }
    }
  }
}

// Persist current overclock level as a one-character string ("0".."4")
void saveOverclockLevel() {
  char buf[2] = { static_cast<char>('0' + overclockLevel), '\0' };
  enum MCS_Error saveResult = MCS_SetVariable(MCS_FOLDER, MCS_VAR_OCLOCK, VARTYPE_STR, 2, buf);
  if (saveResult != MCS_OK) {
    enum MCS_Error retryFolder = MCS_CreateFolder(MCS_FOLDER, nullptr);
    if (retryFolder == MCS_OK || retryFolder == MCS_FOLDER_EXISTS) {
      if (MCS_SetVariable(MCS_FOLDER, MCS_VAR_OCLOCK, VARTYPE_STR, 2, buf) != MCS_OK) {
        // Give up silently
      }
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
