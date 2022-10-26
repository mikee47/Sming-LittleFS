/****
 * Error.h - LittleFS error codes
 *
 * Created on: 8 April 2021
 *
 * Copyright 2021 mikee47 <mike@sillyhouse.net>
 *
 * This file is part of the Sming-LittleFS Library
 *
 * This library is free software: you can redistribute it and/or modify it under the terms of the
 * GNU General Public License as published by the Free Software Foundation, version 3 or later.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this library.
 * If not, see <https://www.gnu.org/licenses/>.
 *
 ****/

#pragma once

#include <IFS/Types.h>
#include <IFS/Error.h>

namespace IFS
{
namespace LittleFS
{
/*
 * LFS provides only LFS_ERR_IO for low-level errors.
 * We'd like a bit more information about failures, so use these instead.
 */
constexpr int LFS_ERR_IFS{-256}; // Well outside range of standard LFS error codes
constexpr int LFS_ERR_IO_READ{LFS_ERR_IFS + Error::ReadFailure};
constexpr int LFS_ERR_IO_WRITE{LFS_ERR_IFS + Error::WriteFailure};
constexpr int LFS_ERR_IO_ERASE{LFS_ERR_IFS + Error::EraseFailure};

int translateLfsError(int lfs_error);
String lfsErrorToStr(int err);

} // namespace LittleFS
} // namespace IFS
