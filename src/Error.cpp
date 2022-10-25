/**
 * Error.cpp
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

#include "include/LittleFS/Error.h"
#include "../littlefs/lfs.h"
#include <IFS/Error.h>
#include <FlashString/Map.hpp>

namespace IFS
{
namespace LittleFS
{
#define LFS_ERROR_TRANSLATION_MAP(XX)                                                                                  \
	XX(IO, Error::ReadFailure)                                                                                         \
	XX(CORRUPT, Error::BadFileSystem)                                                                                  \
	XX(NOENT, Error::NotFound)                                                                                         \
	XX(EXIST, Error::Exists)                                                                                           \
	XX(FBIG, Error::TooBig)                                                                                            \
	XX(BADF, Error::InvalidHandle)                                                                                     \
	XX(INVAL, Error::BadParam)                                                                                         \
	XX(NOSPC, Error::NoSpace)                                                                                          \
	XX(NAMETOOLONG, Error::NameTooLong)

#define LFS_ERROR_MAP(XX)                                                                                              \
	XX(NOTDIR, "Entry is not a dir")                                                                                   \
	XX(ISDIR, "Entry is a dir")                                                                                        \
	XX(NOTEMPTY, "Dir is not empty")                                                                                   \
	XX(NOMEM, "No more memory available")                                                                              \
	XX(NOATTR, "No data/attr available")

#define XX(tag, desc) DEFINE_FSTR_LOCAL(str_##tag, #tag)
LFS_ERROR_MAP(XX)
#undef XX

#define XX(tag, desc) {LFS_ERR_##tag, &str_##tag},
DEFINE_FSTR_MAP_LOCAL(errorMap, int, FlashString, LFS_ERROR_MAP(XX))
#undef XX

int translateLfsError(int lfs_error)
{
	switch(lfs_error) {
#define XX(err_lfs, err_sys)                                                                                           \
	case LFS_ERR_##err_lfs:                                                                                            \
		return err_sys;
		LFS_ERROR_TRANSLATION_MAP(XX)
#undef XX
	default:
		return Error::fromSystem(lfs_error);
	}
}

String lfsErrorToStr(int err)
{
	return errorMap[std::min(err, 0)].content();
}

} // namespace LittleFS
} // namespace IFS
