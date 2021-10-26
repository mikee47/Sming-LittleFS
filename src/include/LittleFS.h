/****
 * LittleFS.h
 * Helper functions to assist with filesystem creation
 *
 * Created on: 9 April 2021
 *
 ****/

#pragma once

#include <IFS/FileSystem.h>

namespace IFS
{
/**
 * @brief Create a LittleFS filesystem
 * @param partition
 * @retval FileSystem* constructed filesystem object
 */
FileSystem* createLfsFilesystem(Storage::Partition partition);

} // namespace IFS

/**
 * @brief Mount the first available LittleFS volume
 * @retval bool true on success
 */
bool lfs_mount();

/**
 * @brief Mount LittleFS volume from a specific partition
 * @retval bool true on success
 */
bool lfs_mount(Storage::Partition partition);
