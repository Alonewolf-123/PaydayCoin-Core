// Copyright (c) 2009-2018 The PaydayCoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef PAYDAYCOIN_CLIENTVERSION_H
#define PAYDAYCOIN_CLIENTVERSION_H

#if defined(HAVE_CONFIG_H)
#include <config/paydaycoin-config.h>
#endif //HAVE_CONFIG_H

// Check that required client information is defined
#if !defined(CLIENT_VERSION_MAJOR) || !defined(CLIENT_VERSION_MINOR) || !defined(CLIENT_VERSION_REVISION) || !defined(CLIENT_VERSION_BUILD) || !defined(CLIENT_VERSION_IS_RELEASE) || !defined(COPYRIGHT_YEAR)
#error Client version information missing: version is not defined by paydaycoin-config.h or in any other way
#endif

/**
 * Converts the parameter X to a string after macro replacement on X has been performed.
 * Don't merge these into one macro!
 */
#define STRINGIZE(X) DO_STRINGIZE(X)
#define DO_STRINGIZE(X) #X

//! Copyright string used in Windows .rc files
#define COPYRIGHT_STR "2009-" STRINGIZE(COPYRIGHT_YEAR) " " COPYRIGHT_HOLDERS_FINAL

/**
 * paydaycoind-res.rc includes this file, but it cannot cope with real c++ code.
 * WINDRES_PREPROC is defined to indicate that its pre-processor is running.
 * Anything other than a define should be guarded below.
 */

#if !defined(WINDRES_PREPROC)

#include <string>
#include <vector>

static const int CLIENT_VERSION =
                           1000000 * CLIENT_VERSION_MAJOR
                         +   10000 * CLIENT_VERSION_MINOR
                         +     100 * CLIENT_VERSION_REVISION
                         +       1 * CLIENT_VERSION_BUILD;
static const int CLIENT_VERSION_MASTERNODE_MAJOR = 1;
static const int CLIENT_MASTERNODE_VERSION =
                           1000000 * CLIENT_VERSION_MASTERNODE_MAJOR
                         +   10000 * CLIENT_VERSION_MINOR
                         +     100 * CLIENT_VERSION_REVISION
                         +       1 * CLIENT_VERSION_BUILD;
// these depend on sentinel code used for this client, pull in from sentinel python version (should be same)
static const int CLIENT_VERSION_SENTINEL_MAJOR = 1;  
static const int CLIENT_VERSION_SENTINEL_MINOR = 0;   
static const int CLIENT_VERSION_SENTINEL_REVISION = 0;   
static const int CLIENT_VERSION_SENTINEL_BUILD = 0;                          
static const int CLIENT_SENTINEL_VERSION =
                           1000000 * CLIENT_VERSION_SENTINEL_MAJOR
                         +   10000 * CLIENT_VERSION_SENTINEL_MINOR
                         +     100 * CLIENT_VERSION_SENTINEL_REVISION
                         +       1 * CLIENT_VERSION_SENTINEL_BUILD;  
extern const std::string CLIENT_NAME;
extern const std::string CLIENT_BUILD;
// PAYDAYCOIN
std::string FormatVersion(int nVersion);

std::string FormatFullVersion();
std::string FormatSubVersion(const std::string& name, int nClientVersion, const std::vector<std::string>& comments);

#endif // WINDRES_PREPROC

#endif // PAYDAYCOIN_CLIENTVERSION_H
