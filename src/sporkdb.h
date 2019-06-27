// Copyright (c) 2017 The PIVX developers
// Copyright (c) 2019 FXTC developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef PIVX_SPORKDB_H
#define PIVX_SPORKDB_H

#include <boost/filesystem/path.hpp>
// FXTC BEGIN
//#include <leveldbwrapper.h>
#include <dbwrapper.h>
// FXTC END
#include <spork.h>

class CSporkDB : public CDBWrapper
{
public:
    CSporkDB(size_t nCacheSize, bool fMemory = false, bool fWipe = false);

private:
    CSporkDB(const CSporkDB&);
    void operator=(const CSporkDB&);

public:
    bool WriteSpork(const int nSporkId, const CSporkMessage& spork);
    bool ReadSpork(const int nSporkId, CSporkMessage& spork);
    bool SporkExists(const int nSporkId);
};


#endif //PIVX_SPORKDB_H
