#include "components/settings/Settings.h"
#include <cstdlib>
#include <cstring>

using namespace Pinetime::Controllers;

Settings::Settings(Pinetime::Controllers::FS& fs) : fs {fs} {
}

void Settings::Init() {

  // Load default settings from Flash
  LoadSettingsFromFile();
  LoadWatchFaceSelection();
}

void Settings::SaveSettings() {

  // verify if is necessary to save
  if (settingsChanged) {
    SaveSettingsToFile();
  }
  settingsChanged = false;
  if (watchFaceSelectionChanged && SaveWatchFaceSelection()) {
    watchFaceSelectionChanged = false;
  }
}

void Settings::LoadWatchFaceSelection() {
  watchFaceSelection = WatchFaceSelection {};
  watchFaceSelectionChanged = false;
  lfs_info info;
  WatchFaceSelection::Record record;
  if (fs.Stat("/watchface-cycle.dat", &info) != LFS_ERR_OK || info.type != LFS_TYPE_REG || info.size != record.size()) {
    return;
  }
  lfs_file_t file;
  if (fs.FileOpen(&file, "/watchface-cycle.dat", LFS_O_RDONLY) != LFS_ERR_OK) {
    return;
  }
  const auto read = fs.FileRead(&file, record.data(), record.size());
  const auto closed = fs.FileClose(&file);
  if (read == static_cast<int>(record.size()) && closed == LFS_ERR_OK) {
    if (const auto selection = WatchFaceSelection::Decode(record)) {
      watchFaceSelection = *selection;
    }
  }
}

bool Settings::SaveWatchFaceSelection() {
  const auto record = watchFaceSelection.Encode();
  lfs_file_t file;
  if (fs.FileOpen(&file, "/watchface-cycle.tmp", LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC) != LFS_ERR_OK) {
    return false;
  }
  const auto written = fs.FileWrite(&file, record.data(), record.size());
  const auto closed = fs.FileClose(&file);
  if (written == static_cast<int>(record.size()) && closed == LFS_ERR_OK &&
      fs.Rename("/watchface-cycle.tmp", "/watchface-cycle.dat") == LFS_ERR_OK) {
    return true;
  }
  fs.FileDelete("/watchface-cycle.tmp");
  return false;
}

void Settings::LoadSettingsFromFile() {
  SettingsData bufferSettings;
  lfs_file_t settingsFile;

  if (fs.FileOpen(&settingsFile, "/settings.dat", LFS_O_RDONLY) != LFS_ERR_OK) {
    return;
  }
  fs.FileRead(&settingsFile, reinterpret_cast<uint8_t*>(&bufferSettings), sizeof(settings));
  fs.FileClose(&settingsFile);
  if (bufferSettings.version == settingsVersion) {
    settings = bufferSettings;
  }
}

void Settings::SaveSettingsToFile() {
  lfs_file_t settingsFile;

  if (fs.FileOpen(&settingsFile, "/settings.dat", LFS_O_WRONLY | LFS_O_CREAT) != LFS_ERR_OK) {
    return;
  }
  fs.FileWrite(&settingsFile, reinterpret_cast<uint8_t*>(&settings), sizeof(settings));
  fs.FileClose(&settingsFile);
}
