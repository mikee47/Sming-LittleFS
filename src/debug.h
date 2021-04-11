#pragma once

#include <WString.h>
#include "../../littlefs/lfs.h"

String hex_str(uint32_t value)
{
	String s;
	s += "0x";
	s += String(value, HEX);
	return s;
}

String pair_str(const lfs_block_t pair[2])
{
	String s;
	s += '{';
	s += hex_str(pair[0]);
	s += ", ";
	s += hex_str(pair[1]);
	s += '}';
	return s;
}

String mdir_str(const lfs_mdir_t& m)
{
	String s;
	s += "{ pair: ";
	s += pair_str(m.pair);
	s += ", rev: ";
	s += m.rev;
	s += ", off: ";
	s += hex_str(m.off);
	s += ", etag: ";
	s += hex_str(m.etag);
	s += ", count: ";
	s += m.count;
	s += ", erased: ";
	s += m.erased;
	s += ", split: ";
	s += m.split;
	s += ", tail: ";
	s += pair_str(m.tail);
	s += " }";
	return s;
}

String dir_str(const lfs_dir_t& dir)
{
	String s;
	s += "{ id: ";
	s += dir.id;
	s += ", type: ";
	s += dir.type;
	s += ", m: ";
	s += mdir_str(dir.m);
	s += ", pos: ";
	s += hex_str(dir.pos);
	s += ", head: ";
	s += pair_str(dir.head);
	s += " }";
	return s;
}

String file_str(const lfs_file_t& file)
{
	String s;
	s += "{ id: ";
	s += file.id;
	s += ", type: ";
	s += file.type;
	s += ", m: ";
	s += mdir_str(file.m);
	s += ", ctz: {";
	s += hex_str(file.ctz.head);
	s += ", ";
	s += hex_str(file.ctz.size);
	s += "}, flags: ";
	s += hex_str(file.flags);
	s += ", pos: ";
	s += hex_str(file.pos);
	s += ", block: ";
	s += hex_str(file.block);
	s += ", off: ";
	s += hex_str(file.off);
	s += " }";
	return s;
}
