/*
 * tools.cpp
 * (C) 2018 by Michael Speck
 */

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifdef WIN32
#define _GNU_SOURCE
#endif

#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef __WIIU__
#include <whb/sdcard.h>
#endif
#include "tools.h"

string trimString(const string& str)
{
	size_t start = 0;
	while (str[start] <= 32 && start < str.length())
		start++;
	size_t len = 0;
	while (str[start + len] > 32 && start + len < str.length())
		len++;
	return str.substr(start,len);
}

/** Parse text file in format:
 * entry=...
 * set {
 * 	entry=...
 * 	...
 * }
 * ...
 */
FileParser::FileParser(const string&  fname)
{
	string line;
	size_t pos;

#ifdef __WIIU__
	if (fname.find("/vol/save") == 0) {
		string content;
        string rel = fname.substr(9);
        while (rel.length() > 0 && rel[0] == '/') rel = rel.substr(1);
        bool success = wiiu_load_file(rel, content);
		if (success) {
			stringstream ifs(content);
			_logdebug(1,"Parsing file %s (via wiiu_load_file)\n",fname.c_str());
            prefix="";
            while (getline(ifs, line)) {
				if (line.length() > 0 && line[line.length()-1] == 13) line = line.substr(0,line.length()-1);
				if ((pos = line.find('=')) != string::npos) {
					string key = prefix + trimString(line.substr(0, pos));
					string val = trimString(line.substr(pos+1));
					ParserEntry pe = {key,val};
					entries.push_back(pe);
					_logdebug(2,"  %s=%s\n",key.c_str(),val.c_str());
				} else if ((pos = line.find('{')) != string::npos) {
					prefix += trimString(line.substr(0,pos)) + ".";
				} else if ((pos = line.find('}')) != string::npos) {
					prefix = prefix.substr(0,prefix.length()-1);
					if ((pos = prefix.rfind('.')) == string::npos) prefix = "";
					else prefix = prefix.substr(0,pos) + ".";
				}
			}
		} else {
			_logerr("Could not load %s via WUT FS\n",fname.c_str());
		}
		return;
	}
#endif

	ifstream ifs(fname);
	if (!ifs.is_open()) {
		_logerr("Could not open %s\n",fname.c_str());
		return;
	}
	_logdebug(1,"Parsing file %s\n",fname.c_str());

	prefix="";
	while (readLine(ifs,line)) {
		if ((pos = line.find('=')) != string::npos) {
			/* plain entry, add to list */
			string key = prefix + trimString(line.substr(0, pos));
			string val = trimString(line.substr(pos+1));
			ParserEntry pe = {key,val};
			entries.push_back(pe);

			_logdebug(2,"  %s=%s\n",key.c_str(),val.c_str());
		} else if ((pos = line.find('{')) != string::npos) {
			/* new subset, adjust prefix */
			prefix += trimString(line.substr(0,pos)) + ".";
		} else if ((pos = line.find('}')) != string::npos) {
			/* subset done, reduce prefix */
			prefix = prefix.substr(0,prefix.length()-1);
			if ((pos = prefix.rfind('.')) == string::npos)
				prefix = "";
			else
				prefix = prefix.substr(0,pos) + ".";
		}
	}
	ifs.close();
}

/** Get value as string, double or int. Return 1 on success, 0 otherwise.
 * Leave value unchanged if not found. */
int FileParser::get(const string& k, string &v)
{
	for (auto& e : entries)
		if (e.key == k) {
			v = e.value;
			return 1;
		}
	_logdebug(2,"Entry %s not found\n",k.c_str());
	return 0;
}
int FileParser::get(const string& k, int &v)
{
	string str;
	int ret = get(k,str);
	if (ret)
		v = stoi(str);
	return ret;
}
int FileParser::get(const string& k, uint &v)
{
	string str;
	int ret = get(k,str);
	if (ret) {
		int i = stoi(str);
		if (i < 0)
			_logerr("%s value %d is not uint\n",k.c_str(),i);
		v = i;
	}
	return ret;
}
int FileParser::get(const string& k, uint8_t &v)
{
	string str;
	int ret = get(k,str);
	if (ret) {
		int i = stoi(str);
		if (i < 0 || i > 255)
			_logerr("%s value %d is not uint8_t!\n",k.c_str(),i);
		v = i;
	}
	return ret;
}
int FileParser::get(const string& k, double &v)
{
	string str;
	int ret = get(k,str);
	if (ret)
		v = stod(str);
	return ret;
}

bool dirExists(const string& name) {
	string path = name;
	if (path.length() > 0 && path[path.length()-1] == '/')
		path = path.substr(0, path.length()-1);
#ifdef __WIIU__
    if (path == "/vol/save") return true;
    if (path.find("/vol/save") == 0) {
        vector<string> temp;
        string rel = path.substr(9);
        while (rel.length() > 0 && rel[0] == '/') rel = rel.substr(1);
        return wiiu_read_dir(rel, RD_FOLDERS, temp) >= 0;
    }
#endif
	struct stat info;
	return (stat(path.c_str(), &info) == 0);
}

bool makeDir(const string& name) {
	string path = name;
	if (path.length() > 0 && path[path.length()-1] == '/')
		path = path.substr(0, path.length()-1);
#ifdef __WIIU__
    if (path == "/vol/save") return true;
    if (path.find("/vol/save") == 0) {
        string rel = path.substr(9);
        while (rel.length() > 0 && rel[0] == '/') rel = rel.substr(1);
        wiiu_make_dir(rel);
        return true;
    }
#endif
#ifdef WIN32
	return mkdir(path.c_str()) == 0;
#else
	return mkdir(path.c_str(), S_IRWXU) == 0;
#endif
}

bool fileExists(const string& name) {
#ifdef __WIIU__
    if (name == "/vol/save" || name == "/vol/save/") return true;
	if (name.find("/vol/save") == 0) {
		string content;
		string rel = name.substr(9);
		while (rel.length() > 0 && rel[0] == '/') rel = rel.substr(1);
        if (rel.empty()) return true;
		return wiiu_load_file(rel, content);
	}
#endif
	if (FILE *file = fopen(name.c_str(), "r")) {
		fclose(file);
		return true;
	}
	return false;
}
bool fileIsWriteable(const string& name) {
#ifdef __WIIU__
	if (name.find("/vol/save") == 0) {
		return true; // assume /vol/save is always writable for our app
	}
#endif
	if (FILE *file = fopen(name.c_str(), "r+")) {
		fclose(file);
		return true;
	}
	return false;
}

/** Not the nicest but hands down most efficient way to do it. */
void strprintf(string& str, const char *fmt, ... )
{
	va_list args;
	char *buf = 0;

	va_start(args,fmt);
	if (vasprintf(&buf,fmt,args) >= 0)
		str = buf;
	else
		str = "";
	va_end(args);
	if (buf)
		free(buf);
}

void _sd_log(const char *fmt, ...)
{
	va_list args;
	va_start(args,fmt);
#ifdef __WIIU__
    char log_buf[1024];
    vsnprintf(log_buf, sizeof(log_buf), fmt, args);
    string content;
    wiiu_load_file("/debug.txt", content);
    content += string(log_buf);
    wiiu_save_file("/debug.txt", content);
#endif
	va_end(args);
}

/** Read all file names from directory exluding .* and Makefile.*
 * Return number of read items, -1 on error.
 */
int readDir(const string &dname, int type, vector<string> &fnames)
{
#ifdef __WIIU__
	if (dname.find("/vol/save") == 0) {
        string rel = dname.substr(9);
        while (rel.length() > 0 && rel[0] == '/') rel = rel.substr(1);
		return wiiu_read_dir(rel, type, fnames);
	}
	/* For sd:/ and other paths on Wii U, fall through to POSIX opendir.
	 * If dir doesn't exist yet, opendir returns NULL and we return -1 safely. */
#endif
	DIR *dir;
	struct dirent *ent;
	struct stat sbuf;

	if ((dir = opendir (dname.c_str())) == NULL) {
		_logerr("Could not open %s\n",dname.c_str());
		return -1;
	}

	fnames.clear();
	while ((ent = readdir(dir)) != NULL) {
		if (strncmp(ent->d_name,"Makefile",8) == 0)
			continue;
		if (ent->d_name[0] == '.')
			continue;

#ifdef __WIIU__
        // WUT romfs devoptab might have buggy stat(). 
        // We use d_type which is well-supported in WUT for romfs!
        bool isDir = (ent->d_type == DT_DIR);
        if (ent->d_type == DT_UNKNOWN) {
            string fname = dname + "/" + string(ent->d_name);
            stat(fname.c_str(),&sbuf);
            isDir = S_ISDIR(sbuf.st_mode);
        }
#else
        string fname = dname + "/" + string(ent->d_name);
        stat(fname.c_str(),&sbuf);
        bool isDir = S_ISDIR(sbuf.st_mode);
#endif

		if (type == RD_FOLDERS) {
			if (!isDir)
				continue;
		} else if (isDir) {
			continue;
        }

		fnames.push_back(ent->d_name);
	}
	closedir (dir);
	sort(fnames.begin(),fnames.end());
	return fnames.size();
}

#ifdef __WIIU__
#include <nn/act.h>
#include <nn/save.h>

static FSClient* s_fsClient = nullptr;
static FSCmdBlock* s_fsCmdBlock = nullptr;

#endif

string getHomeDir() {
#ifdef __WIIU__
    static bool initialized = false;
    if (!initialized) {
        nn::act::Initialize();
        SAVEInit();
        uint8_t slotNo = nn::act::GetSlotNo();
        if (slotNo == 0) slotNo = 1;
        SAVEInitSaveDir(slotNo);
        
        s_fsClient = new FSClient{};
        s_fsCmdBlock = new FSCmdBlock{};
        FSAddClient(s_fsClient, FS_ERROR_FLAG_NONE);
        FSInitCmdBlock(s_fsCmdBlock);
        
        initialized = true;
    }
	return "/vol/save";
#else
	return string(getenv("HOME")?getenv("HOME"):".");
#endif
}

/* not thread safe */
const string &getCustomLevelsetDir() {
	static string path;
#ifdef __WIIU__
	const char* sdRoot = WHBGetSdCardMountPath();
	path = string(sdRoot ? sdRoot : "fs:/vol/external01") + "/lbreakouthd/levels";
#else
	if (string(CONFIGDIR) == ".")
		path = "./levels";
	else
		path = getHomeDir() + "/" + CONFIGDIR + "/levels/";
#endif
	return path;
}

/* not thread safe */
const string &getFullLevelsetPath(const string &n)
{
	static string path;
	if (n[0] != '~') {
		path = string(DATADIR) + "/levels/" + n;
	} else {
#ifdef __WIIU__
		const char* sdRoot = WHBGetSdCardMountPath();
		path = string(sdRoot ? sdRoot : "fs:/vol/external01") + "/lbreakouthd/levels/" + n.substr(1);
#else
		if (string(CONFIGDIR) == ".")
			path = "./levels/";
		else
			path = getHomeDir() + "/" + CONFIGDIR + "/levels/";
		path += n.substr(1);
#endif
	}
	return path;
}

/** Read line from input stream and remove any return carriage char (13)
 * at the end (windows files). */
istream& readLine(istream &ifs, string &str)
{
	istream &ret = getline(ifs, str);
	if (str.length() > 0 && str[str.length()-1] == 13)
		str = str.substr(0,str.length()-1);
	return ret;
}

#ifdef __WIIU__
#include <coreinit/filesystem.h>
#include <coreinit/cache.h>
#include <nn/save.h>
#include <nn/act.h>
#include <cstring>
#include <cstdio>
#include <algorithm>

static uint8_t s_ioBuf[1024 * 128] __attribute__((aligned(0x40)));

bool wiiu_save_file(const string& rel_path, const string& content) {
    if (!s_fsClient || !s_fsCmdBlock) return false;

    uint8_t slotNo = nn::act::GetSlotNo();
    if (slotNo == 0) slotNo = 1;

    string fixed_path = rel_path;
    while (fixed_path.length() > 0 && fixed_path[0] == '/') fixed_path = fixed_path.substr(1);
    if (fixed_path.empty()) return false;
    fixed_path = "/" + fixed_path;

    FSFileHandle handle{};
    FSStatus status = SAVEOpenFile(
        s_fsClient, s_fsCmdBlock,
        slotNo,
        fixed_path.c_str(),
        "w",
        &handle,
        FS_ERROR_FLAG_NONE
    );

    if (status == FS_STATUS_OK) {
        size_t toWrite = content.length();
        if (toWrite > sizeof(s_ioBuf)) toWrite = sizeof(s_ioBuf);
        std::memcpy(s_ioBuf, content.data(), toWrite);

        DCFlushRange(s_ioBuf, toWrite);

        FSWriteFile(s_fsClient, s_fsCmdBlock, s_ioBuf, 1, toWrite, handle, 0, FS_ERROR_FLAG_NONE);
        FSCloseFile(s_fsClient, s_fsCmdBlock, handle, FS_ERROR_FLAG_NONE);
        SAVEFlushQuota(s_fsClient, s_fsCmdBlock, slotNo, FS_ERROR_FLAG_NONE);
    } else {
        _logerr("[SAVE] SAVEOpenFile(w) failed: %d\n", status);
    }

    return status == FS_STATUS_OK;
}

bool wiiu_load_file(const string& rel_path, string& out_content) {
    if (!s_fsClient || !s_fsCmdBlock) return false;

    uint8_t slotNo = nn::act::GetSlotNo();
    if (slotNo == 0) slotNo = 1;

    string fixed_path = rel_path;
    while (fixed_path.length() > 0 && fixed_path[0] == '/') fixed_path = fixed_path.substr(1);
    if (fixed_path.empty()) return false;
    fixed_path = "/" + fixed_path;

    FSFileHandle handle{};
    FSStatus status = SAVEOpenFile(
        s_fsClient, s_fsCmdBlock,
        slotNo,
        fixed_path.c_str(),
        "r",
        &handle,
        FS_ERROR_FLAG_NONE
    );

    bool ok = false;
    if (status == FS_STATUS_OK) {
        FSStat stat{};
        FSGetStatFile(s_fsClient, s_fsCmdBlock, handle, &stat, FS_ERROR_FLAG_NONE);
        size_t toRead = stat.size;
        if (toRead > sizeof(s_ioBuf)) toRead = sizeof(s_ioBuf);

        FSStatus readStatus = FSReadFile(s_fsClient, s_fsCmdBlock, s_ioBuf, 1, toRead, handle, 0, FS_ERROR_FLAG_NONE);
        FSCloseFile(s_fsClient, s_fsCmdBlock, handle, FS_ERROR_FLAG_NONE);
        
        DCInvalidateRange(s_ioBuf, toRead);

        if (readStatus >= 0) {
            out_content = string((char*)s_ioBuf, readStatus);
            ok = true;
        }
    }

    return ok;
}

void wiiu_make_dir(const string& rel_path) {
    if (!s_fsClient || !s_fsCmdBlock) return;

    uint8_t slotNo = nn::act::GetSlotNo();
    if (slotNo == 0) slotNo = 1;

    string fixed_path = rel_path;
    while (fixed_path.length() > 0 && fixed_path[0] == '/') fixed_path = fixed_path.substr(1);
    if (fixed_path.empty()) return;
    fixed_path = "/" + fixed_path;

    SAVEMakeDir(s_fsClient, s_fsCmdBlock, slotNo, fixed_path.c_str(), FS_ERROR_FLAG_NONE);
}

int wiiu_read_dir(const string& rel_path, int type, vector<string>& fnames) {
    if (!s_fsClient || !s_fsCmdBlock) return -1;

    uint8_t slotNo = nn::act::GetSlotNo();
    if (slotNo == 0) slotNo = 1;

    string fixed_path = rel_path;
    while (fixed_path.length() > 0 && fixed_path[0] == '/') fixed_path = fixed_path.substr(1);
    if (fixed_path.empty()) return -1;
    fixed_path = "/" + fixed_path;

    FSDirectoryHandle dirHandle{};
    FSStatus status = SAVEOpenDir(s_fsClient, s_fsCmdBlock, slotNo, fixed_path.c_str(), &dirHandle, FS_ERROR_FLAG_NONE);
    if (status != FS_STATUS_OK) {
        return -1;
    }

    fnames.clear();
    FSDirectoryEntry entry;
    while (FSReadDir(s_fsClient, s_fsCmdBlock, dirHandle, &entry, FS_ERROR_FLAG_NONE) == FS_STATUS_OK) {
        if (type == RD_FOLDERS) {
            if (!(entry.info.flags & FS_STAT_DIRECTORY)) continue;
        } else if (entry.info.flags & FS_STAT_DIRECTORY) {
            continue;
        }
        string fname(entry.name);
        if (fname == "." || fname == "..") continue;
        fnames.push_back(fname);
    }
    FSCloseDir(s_fsClient, s_fsCmdBlock, dirHandle, FS_ERROR_FLAG_NONE);
    sort(fnames.begin(), fnames.end());
    return fnames.size();
}
#endif
