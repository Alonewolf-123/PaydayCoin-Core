// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2018 The PaydayCoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <util/system.h>

#include <chainparamsbase.h>
#include <util/strencodings.h>

#include <stdarg.h>

#if (defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__DragonFly__))
#include <pthread.h>
#include <pthread_np.h>
#endif

#ifndef WIN32
// for posix_fallocate
#ifdef __linux__

#ifdef _POSIX_C_SOURCE
#undef _POSIX_C_SOURCE
#endif

#define _POSIX_C_SOURCE 200112L

#endif // __linux__

#include <algorithm>
#include <fcntl.h>
#include <sched.h>
#include <sys/resource.h>
#include <sys/stat.h>

#else

#ifdef _MSC_VER
#pragma warning(disable:4786)
#pragma warning(disable:4804)
#pragma warning(disable:4805)
#pragma warning(disable:4717)
#endif

#ifdef _WIN32_IE
#undef _WIN32_IE
#endif
#define _WIN32_IE 0x0501

#define WIN32_LEAN_AND_MEAN 1
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <codecvt>

#include <io.h> /* for _commit */
#include <shellapi.h>
#include <shlobj.h>
#endif

#ifdef HAVE_MALLOPT_ARENA_MAX
#include <malloc.h>
#endif

#include <thread>
// PAYDAYCOIN only features
#include <boost/algorithm/string.hpp>
#include <signal.h>
#include <rpc/server.h>
#include <random.h>

bool fMasternodeMode = false;
bool fUnitTest = false;
bool fTPSTest = false;
bool fTPSTestEnabled = false;
bool fConcurrentProcessing = true;
bool fLiteMode = false;
bool fZMQAssetAllocation = false;
bool fZMQAsset = false;
bool fZMQWalletStatus = false;
bool fZMQNetworkStatus = false;
bool fZMQEthStatus = false;
bool fZMQWalletRawTx = false;
bool fAssetIndex = false;
int fAssetIndexPageSize = 25;
std::vector<uint32_t> fAssetIndexGuids; 
uint32_t fGethSyncHeight = 0;
uint32_t fGethCurrentHeight = 0;
pid_t gethPID = 0;
pid_t relayerPID = 0;
std::string fGethSyncStatus = "waiting to sync...";
bool fGethSynced = false;
bool fLoaded = false;
std::vector<JSONRPCRequest> vecTPSRawTransactions;
// Application startup time (used for uptime calculation)
const int64_t nStartupTime = GetTime();

const char * const PAYDAYCOIN_CONF_FILENAME = "paydaycoin.conf";

ArgsManager gArgs;
// PAYDAYCOIN
#ifdef WIN32
    #include <windows.h>
    #include <winnt.h>
    #include <winternl.h>
    #include <stdio.h>
    #include <errno.h>
    #include <assert.h>
    #include <process.h>
    pid_t fork(std::string app, std::string arg)
    {
        std::string appQuoted = "\"" + app + "\"";
        PROCESS_INFORMATION pi;
        STARTUPINFOW si;
        ZeroMemory(&pi, sizeof(pi));
        ZeroMemory(&si, sizeof(si));
        GetStartupInfoW (&si);
        si.cb = sizeof(si); 
        size_t start_pos = 0;
        //Prepare CreateProcess args
        std::wstring appQuoted_w(appQuoted.length(), L' '); // Make room for characters
        std::copy(appQuoted.begin(), appQuoted.end(), appQuoted_w.begin()); // Copy string to wstring.

        std::wstring app_w(app.length(), L' '); // Make room for characters
        std::copy(app.begin(), app.end(), app_w.begin()); // Copy string to wstring.

        std::wstring arg_w(arg.length(), L' '); // Make room for characters
        std::copy(arg.begin(), arg.end(), arg_w.begin()); // Copy string to wstring.

        std::wstring input = appQuoted_w + L" " + arg_w;
        wchar_t* arg_concat = const_cast<wchar_t*>( input.c_str() );
        const wchar_t* app_const = app_w.c_str();
        LogPrintf("CreateProcessW app %s %s\n",app,arg);
        int result = CreateProcessW(app_const, arg_concat, NULL, NULL, FALSE, 
              CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
        if(!result)
        {
            LogPrintf("CreateProcess failed (%d)\n", GetLastError());
            return 0;
        }
        pid_t pid = (pid_t)pi.dwProcessId;
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return pid;
    }
#endif
/** A map that contains all the currently held directory locks. After
 * successful locking, these will be held here until the global destructor
 * cleans them up and thus automatically unlocks them, or ReleaseDirectoryLocks
 * is called.
 */
static std::map<std::string, std::unique_ptr<fsbridge::FileLock>> dir_locks;
/** Mutex to protect dir_locks. */
static std::mutex cs_dir_locks;

bool LockDirectory(const fs::path& directory, const std::string lockfile_name, bool probe_only)
{
    std::lock_guard<std::mutex> ulock(cs_dir_locks);
    fs::path pathLockFile = directory / lockfile_name;

    // If a lock for this directory already exists in the map, don't try to re-lock it
    if (dir_locks.count(pathLockFile.string())) {
        return true;
    }

    // Create empty lock file if it doesn't exist.
    FILE* file = fsbridge::fopen(pathLockFile, "a");
    if (file) fclose(file);
    auto lock = MakeUnique<fsbridge::FileLock>(pathLockFile);
    if (!lock->TryLock()) {
        return error("Error while attempting to lock directory %s: %s", directory.string(), lock->GetReason());
    }
    if (!probe_only) {
        // Lock successful and we're not just probing, put it into the map
        dir_locks.emplace(pathLockFile.string(), std::move(lock));
    }
    return true;
}

void UnlockDirectory(const fs::path& directory, const std::string& lockfile_name)
{
    std::lock_guard<std::mutex> lock(cs_dir_locks);
    dir_locks.erase((directory / lockfile_name).string());
}

void ReleaseDirectoryLocks()
{
    std::lock_guard<std::mutex> ulock(cs_dir_locks);
    dir_locks.clear();
}

bool DirIsWritable(const fs::path& directory)
{
    fs::path tmpFile = directory / fs::unique_path();

    FILE* file = fsbridge::fopen(tmpFile, "a");
    if (!file) return false;

    fclose(file);
    remove(tmpFile);

    return true;
}

bool CheckDiskSpace(const fs::path& dir, uint64_t additional_bytes)
{
    constexpr uint64_t min_disk_space = 52428800; // 50 MiB

    uint64_t free_bytes_available = fs::space(dir).available;
    return free_bytes_available >= min_disk_space + additional_bytes;
}

/**
 * Interpret a string argument as a boolean.
 *
 * The definition of atoi() requires that non-numeric string values like "foo",
 * return 0. This means that if a user unintentionally supplies a non-integer
 * argument here, the return value is always false. This means that -foo=false
 * does what the user probably expects, but -foo=true is well defined but does
 * not do what they probably expected.
 *
 * The return value of atoi() is undefined when given input not representable as
 * an int. On most systems this means string value between "-2147483648" and
 * "2147483647" are well defined (this method will return true). Setting
 * -txindex=2147483648 on most systems, however, is probably undefined.
 *
 * For a more extensive discussion of this topic (and a wide range of opinions
 * on the Right Way to change this code), see PR12713.
 */
static bool InterpretBool(const std::string& strValue)
{
    if (strValue.empty())
        return true;
    return (atoi(strValue) != 0);
}

/** Internal helper functions for ArgsManager */
class ArgsManagerHelper {
public:
    typedef std::map<std::string, std::vector<std::string>> MapArgs;

    /** Determine whether to use config settings in the default section,
     *  See also comments around ArgsManager::ArgsManager() below. */
    static inline bool UseDefaultSection(const ArgsManager& am, const std::string& arg) EXCLUSIVE_LOCKS_REQUIRED(am.cs_args)
    {
        return (am.m_network == CBaseChainParams::MAIN || am.m_network_only_args.count(arg) == 0);
    }

    /** Convert regular argument into the network-specific setting */
    static inline std::string NetworkArg(const ArgsManager& am, const std::string& arg)
    {
        assert(arg.length() > 1 && arg[0] == '-');
        return "-" + am.m_network + "." + arg.substr(1);
    }

    /** Find arguments in a map and add them to a vector */
    static inline void AddArgs(std::vector<std::string>& res, const MapArgs& map_args, const std::string& arg)
    {
        auto it = map_args.find(arg);
        if (it != map_args.end()) {
            res.insert(res.end(), it->second.begin(), it->second.end());
        }
    }

    /** Return true/false if an argument is set in a map, and also
     *  return the first (or last) of the possibly multiple values it has
     */
    static inline std::pair<bool,std::string> GetArgHelper(const MapArgs& map_args, const std::string& arg, bool getLast = false)
    {
        auto it = map_args.find(arg);

        if (it == map_args.end() || it->second.empty()) {
            return std::make_pair(false, std::string());
        }

        if (getLast) {
            return std::make_pair(true, it->second.back());
        } else {
            return std::make_pair(true, it->second.front());
        }
    }

    /* Get the string value of an argument, returning a pair of a boolean
     * indicating the argument was found, and the value for the argument
     * if it was found (or the empty string if not found).
     */
    static inline std::pair<bool,std::string> GetArg(const ArgsManager &am, const std::string& arg)
    {
        LOCK(am.cs_args);
        std::pair<bool,std::string> found_result(false, std::string());

        // We pass "true" to GetArgHelper in order to return the last
        // argument value seen from the command line (so "paydaycoind -foo=bar
        // -foo=baz" gives GetArg(am,"foo")=={true,"baz"}
        found_result = GetArgHelper(am.m_override_args, arg, true);
        if (found_result.first) {
            return found_result;
        }

        // But in contrast we return the first argument seen in a config file,
        // so "foo=bar \n foo=baz" in the config file gives
        // GetArg(am,"foo")={true,"bar"}
        if (!am.m_network.empty()) {
            found_result = GetArgHelper(am.m_config_args, NetworkArg(am, arg));
            if (found_result.first) {
                return found_result;
            }
        }

        if (UseDefaultSection(am, arg)) {
            found_result = GetArgHelper(am.m_config_args, arg);
            if (found_result.first) {
                return found_result;
            }
        }

        return found_result;
    }

    /* Special test for -testnet and -regtest args, because we
     * don't want to be confused by craziness like "[regtest] testnet=1"
     */
    static inline bool GetNetBoolArg(const ArgsManager &am, const std::string& net_arg) EXCLUSIVE_LOCKS_REQUIRED(am.cs_args)
    {
        std::pair<bool,std::string> found_result(false,std::string());
        found_result = GetArgHelper(am.m_override_args, net_arg, true);
        if (!found_result.first) {
            found_result = GetArgHelper(am.m_config_args, net_arg, true);
            if (!found_result.first) {
                return false; // not set
            }
        }
        return InterpretBool(found_result.second); // is set, so evaluate
    }
};

/**
 * Interpret -nofoo as if the user supplied -foo=0.
 *
 * This method also tracks when the -no form was supplied, and if so,
 * checks whether there was a double-negative (-nofoo=0 -> -foo=1).
 *
 * If there was not a double negative, it removes the "no" from the key,
 * and returns true, indicating the caller should clear the args vector
 * to indicate a negated option.
 *
 * If there was a double negative, it removes "no" from the key, sets the
 * value to "1" and returns false.
 *
 * If there was no "no", it leaves key and value untouched and returns
 * false.
 *
 * Where an option was negated can be later checked using the
 * IsArgNegated() method. One use case for this is to have a way to disable
 * options that are not normally boolean (e.g. using -nodebuglogfile to request
 * that debug log output is not sent to any file at all).
 */
static bool InterpretNegatedOption(std::string& key, std::string& val)
{
    assert(key[0] == '-');

    size_t option_index = key.find('.');
    if (option_index == std::string::npos) {
        option_index = 1;
    } else {
        ++option_index;
    }
    if (key.substr(option_index, 2) == "no") {
        bool bool_val = InterpretBool(val);
        key.erase(option_index, 2);
        if (!bool_val ) {
            // Double negatives like -nofoo=0 are supported (but discouraged)
            LogPrintf("Warning: parsed potentially confusing double-negative %s=%s\n", key, val);
            val = "1";
        } else {
            return true;
        }
    }
    return false;
}

ArgsManager::ArgsManager() :
    /* These options would cause cross-contamination if values for
     * mainnet were used while running on regtest/testnet (or vice-versa).
     * Setting them as section_only_args ensures that sharing a config file
     * between mainnet and regtest/testnet won't cause problems due to these
     * parameters by accident. */
    m_network_only_args{
      "-addnode", "-connect",
      "-port", "-bind",
      "-rpcport", "-rpcbind",
      "-wallet",
    }
{
    // nothing to do
}

const std::set<std::string> ArgsManager::GetUnsuitableSectionOnlyArgs() const
{
    std::set<std::string> unsuitables;

    LOCK(cs_args);

    // if there's no section selected, don't worry
    if (m_network.empty()) return std::set<std::string> {};

    // if it's okay to use the default section for this network, don't worry
    if (m_network == CBaseChainParams::MAIN) return std::set<std::string> {};

    for (const auto& arg : m_network_only_args) {
        std::pair<bool, std::string> found_result;

        // if this option is overridden it's fine
        found_result = ArgsManagerHelper::GetArgHelper(m_override_args, arg);
        if (found_result.first) continue;

        // if there's a network-specific value for this option, it's fine
        found_result = ArgsManagerHelper::GetArgHelper(m_config_args, ArgsManagerHelper::NetworkArg(*this, arg));
        if (found_result.first) continue;

        // if there isn't a default value for this option, it's fine
        found_result = ArgsManagerHelper::GetArgHelper(m_config_args, arg);
        if (!found_result.first) continue;

        // otherwise, issue a warning
        unsuitables.insert(arg);
    }
    return unsuitables;
}

const std::list<SectionInfo> ArgsManager::GetUnrecognizedSections() const
{
    // Section names to be recognized in the config file.
    static const std::set<std::string> available_sections{
        CBaseChainParams::REGTEST,
        CBaseChainParams::TESTNET,
        CBaseChainParams::MAIN
    };

    LOCK(cs_args);
    std::list<SectionInfo> unrecognized = m_config_sections;
    unrecognized.remove_if([](const SectionInfo& appeared){ return available_sections.find(appeared.m_name) != available_sections.end(); });
    return unrecognized;
}

void ArgsManager::SelectConfigNetwork(const std::string& network)
{
    LOCK(cs_args);
    m_network = network;
}

bool ArgsManager::ParseParameters(int argc, const char* const argv[], std::string& error)
{
    LOCK(cs_args);
    m_override_args.clear();

    for (int i = 1; i < argc; i++) {
        std::string key(argv[i]);
        std::string val;
        size_t is_index = key.find('=');
        if (is_index != std::string::npos) {
            val = key.substr(is_index + 1);
            key.erase(is_index);
        }
#ifdef WIN32
        std::transform(key.begin(), key.end(), key.begin(), ToLower);
        if (key[0] == '/')
            key[0] = '-';
#endif

        if (key[0] != '-')
            break;

        // Transform --foo to -foo
        if (key.length() > 1 && key[1] == '-')
            key.erase(0, 1);

        // Check for -nofoo
        if (InterpretNegatedOption(key, val)) {
            m_override_args[key].clear();
        } else {
            m_override_args[key].push_back(val);
        }

        // Check that the arg is known
        if (!(IsSwitchChar(key[0]) && key.size() == 1)) {
            if (!IsArgKnown(key)) {
                error = strprintf("Invalid parameter %s", key.c_str());
                return false;
            }
        }
    }

    // we do not allow -includeconf from command line, so we clear it here
    auto it = m_override_args.find("-includeconf");
    if (it != m_override_args.end()) {
        if (it->second.size() > 0) {
            for (const auto& ic : it->second) {
                error += "-includeconf cannot be used from commandline; -includeconf=" + ic + "\n";
            }
            return false;
        }
    }
    return true;
}

bool ArgsManager::IsArgKnown(const std::string& key) const
{
    size_t option_index = key.find('.');
    std::string arg_no_net;
    if (option_index == std::string::npos) {
        arg_no_net = key;
    } else {
        arg_no_net = std::string("-") + key.substr(option_index + 1, std::string::npos);
    }

    LOCK(cs_args);
    for (const auto& arg_map : m_available_args) {
        if (arg_map.second.count(arg_no_net)) return true;
    }
    return false;
}

std::vector<std::string> ArgsManager::GetArgs(const std::string& strArg) const
{
    std::vector<std::string> result = {};
    if (IsArgNegated(strArg)) return result; // special case

    LOCK(cs_args);

    ArgsManagerHelper::AddArgs(result, m_override_args, strArg);
    if (!m_network.empty()) {
        ArgsManagerHelper::AddArgs(result, m_config_args, ArgsManagerHelper::NetworkArg(*this, strArg));
    }

    if (ArgsManagerHelper::UseDefaultSection(*this, strArg)) {
        ArgsManagerHelper::AddArgs(result, m_config_args, strArg);
    }

    return result;
}

bool ArgsManager::IsArgSet(const std::string& strArg) const
{
    if (IsArgNegated(strArg)) return true; // special case
    return ArgsManagerHelper::GetArg(*this, strArg).first;
}

bool ArgsManager::IsArgNegated(const std::string& strArg) const
{
    LOCK(cs_args);

    const auto& ov = m_override_args.find(strArg);
    if (ov != m_override_args.end()) return ov->second.empty();

    if (!m_network.empty()) {
        const auto& cfs = m_config_args.find(ArgsManagerHelper::NetworkArg(*this, strArg));
        if (cfs != m_config_args.end()) return cfs->second.empty();
    }

    const auto& cf = m_config_args.find(strArg);
    if (cf != m_config_args.end()) return cf->second.empty();

    return false;
}

std::string ArgsManager::GetArg(const std::string& strArg, const std::string& strDefault) const
{
    if (IsArgNegated(strArg)) return "0";
    std::pair<bool,std::string> found_res = ArgsManagerHelper::GetArg(*this, strArg);
    if (found_res.first) return found_res.second;
    return strDefault;
}

int64_t ArgsManager::GetArg(const std::string& strArg, int64_t nDefault) const
{
    if (IsArgNegated(strArg)) return 0;
    std::pair<bool,std::string> found_res = ArgsManagerHelper::GetArg(*this, strArg);
    if (found_res.first) return atoi64(found_res.second);
    return nDefault;
}

bool ArgsManager::GetBoolArg(const std::string& strArg, bool fDefault) const
{
    if (IsArgNegated(strArg)) return false;
    std::pair<bool,std::string> found_res = ArgsManagerHelper::GetArg(*this, strArg);
    if (found_res.first) return InterpretBool(found_res.second);
    return fDefault;
}

bool ArgsManager::SoftSetArg(const std::string& strArg, const std::string& strValue)
{
    LOCK(cs_args);
    if (IsArgSet(strArg)) return false;
    ForceSetArg(strArg, strValue);
    return true;
}

bool ArgsManager::SoftSetBoolArg(const std::string& strArg, bool fValue)
{
    if (fValue)
        return SoftSetArg(strArg, std::string("1"));
    else
        return SoftSetArg(strArg, std::string("0"));
}

void ArgsManager::ForceSetArg(const std::string& strArg, const std::string& strValue)
{
    LOCK(cs_args);
    m_override_args[strArg] = {strValue};
}

void ArgsManager::AddArg(const std::string& name, const std::string& help, const bool debug_only, const OptionsCategory& cat)
{
    // Split arg name from its help param
    size_t eq_index = name.find('=');
    if (eq_index == std::string::npos) {
        eq_index = name.size();
    }

    LOCK(cs_args);
    std::map<std::string, Arg>& arg_map = m_available_args[cat];
    auto ret = arg_map.emplace(name.substr(0, eq_index), Arg(name.substr(eq_index, name.size() - eq_index), help, debug_only));
    assert(ret.second); // Make sure an insertion actually happened
}

void ArgsManager::AddHiddenArgs(const std::vector<std::string>& names)
{
    for (const std::string& name : names) {
        AddArg(name, "", false, OptionsCategory::HIDDEN);
    }
}

std::string ArgsManager::GetHelpMessage() const
{
    const bool show_debug = gArgs.GetBoolArg("-help-debug", false);

    std::string usage = "";
    LOCK(cs_args);
    for (const auto& arg_map : m_available_args) {
        switch(arg_map.first) {
            case OptionsCategory::OPTIONS:
                usage += HelpMessageGroup("Options:");
                break;
            case OptionsCategory::CONNECTION:
                usage += HelpMessageGroup("Connection options:");
                break;
            case OptionsCategory::ZMQ:
                usage += HelpMessageGroup("ZeroMQ notification options:");
                break;
            case OptionsCategory::DEBUG_TEST:
                usage += HelpMessageGroup("Debugging/Testing options:");
                break;
            case OptionsCategory::NODE_RELAY:
                usage += HelpMessageGroup("Node relay options:");
                break;
            case OptionsCategory::BLOCK_CREATION:
                usage += HelpMessageGroup("Block creation options:");
                break;
            case OptionsCategory::RPC:
                usage += HelpMessageGroup("RPC server options:");
                break;
            case OptionsCategory::WALLET:
                usage += HelpMessageGroup("Wallet options:");
                break;
            case OptionsCategory::WALLET_DEBUG_TEST:
                if (show_debug) usage += HelpMessageGroup("Wallet debugging/testing options:");
                break;
            case OptionsCategory::CHAINPARAMS:
                usage += HelpMessageGroup("Chain selection options:");
                break;
            case OptionsCategory::GUI:
                usage += HelpMessageGroup("UI Options:");
                break;
            case OptionsCategory::COMMANDS:
                usage += HelpMessageGroup("Commands:");
                break;
            case OptionsCategory::REGISTER_COMMANDS:
                usage += HelpMessageGroup("Register Commands:");
                break;
            default:
                break;
        }

        // When we get to the hidden options, stop
        if (arg_map.first == OptionsCategory::HIDDEN) break;

        for (const auto& arg : arg_map.second) {
            if (show_debug || !arg.second.m_debug_only) {
                std::string name;
                if (arg.second.m_help_param.empty()) {
                    name = arg.first;
                } else {
                    name = arg.first + arg.second.m_help_param;
                }
                usage += HelpMessageOpt(name, arg.second.m_help_text);
            }
        }
    }
    return usage;
}

bool HelpRequested(const ArgsManager& args)
{
    return args.IsArgSet("-?") || args.IsArgSet("-h") || args.IsArgSet("-help") || args.IsArgSet("-help-debug");
}

void SetupHelpOptions(ArgsManager& args)
{
    args.AddArg("-?", "Print this help message and exit", false, OptionsCategory::OPTIONS);
    args.AddHiddenArgs({"-h", "-help"});
}

static const int screenWidth = 79;
static const int optIndent = 2;
static const int msgIndent = 7;

std::string HelpMessageGroup(const std::string &message) {
    return std::string(message) + std::string("\n\n");
}

std::string HelpMessageOpt(const std::string &option, const std::string &message) {
    return std::string(optIndent,' ') + std::string(option) +
           std::string("\n") + std::string(msgIndent,' ') +
           FormatParagraph(message, screenWidth - msgIndent, msgIndent) +
           std::string("\n\n");
}

static std::string FormatException(const std::exception* pex, const char* pszThread)
{
#ifdef WIN32
    char pszModule[MAX_PATH] = "";
    GetModuleFileNameA(nullptr, pszModule, sizeof(pszModule));
#else
    const char* pszModule = "paydaycoin";
#endif
    if (pex)
        return strprintf(
            "EXCEPTION: %s       \n%s       \n%s in %s       \n", typeid(*pex).name(), pex->what(), pszModule, pszThread);
    else
        return strprintf(
            "UNKNOWN EXCEPTION       \n%s in %s       \n", pszModule, pszThread);
}

void PrintExceptionContinue(const std::exception* pex, const char* pszThread)
{
    std::string message = FormatException(pex, pszThread);
    LogPrintf("\n\n************************\n%s\n", message);
    fprintf(stderr, "\n\n************************\n%s\n", message.c_str());
}

fs::path GetDefaultDataDir()
{
    // Windows < Vista: C:\Documents and Settings\Username\Application Data\PaydayCoin
    // Windows >= Vista: C:\Users\Username\AppData\Roaming\PaydayCoin
    // Mac: ~/Library/Application Support/PaydayCoin
    // Unix: ~/.paydaycoin
#ifdef WIN32
    // Windows
    return GetSpecialFolderPath(CSIDL_APPDATA) / "PaydayCoin";
#else
    fs::path pathRet;
    char* pszHome = getenv("HOME");
    if (pszHome == nullptr || strlen(pszHome) == 0)
        pathRet = fs::path("/");
    else
        pathRet = fs::path(pszHome);
#ifdef MAC_OSX
    // Mac
    return pathRet / "Library/Application Support/PaydayCoin";
#else
    // Unix
    return pathRet / ".paydaycoin";
#endif
#endif
}

static fs::path g_blocks_path_cache_net_specific;
static fs::path pathCached;
static fs::path pathCachedNetSpecific;
static CCriticalSection csPathCached;

const fs::path &GetBlocksDir()
{

    LOCK(csPathCached);

    fs::path &path = g_blocks_path_cache_net_specific;

    // This can be called during exceptions by LogPrintf(), so we cache the
    // value so we don't have to do memory allocations after that.
    if (!path.empty())
        return path;

    if (gArgs.IsArgSet("-blocksdir")) {
        path = fs::system_complete(gArgs.GetArg("-blocksdir", ""));
        if (!fs::is_directory(path)) {
            path = "";
            return path;
        }
    } else {
        path = GetDataDir(false);
    }

    path /= BaseParams().DataDir();
    path /= "blocks";
    fs::create_directories(path);
    return path;
}

const fs::path &GetDataDir(bool fNetSpecific)
{

    LOCK(csPathCached);

    fs::path &path = fNetSpecific ? pathCachedNetSpecific : pathCached;

    // This can be called during exceptions by LogPrintf(), so we cache the
    // value so we don't have to do memory allocations after that.
    if (!path.empty())
        return path;

    if (gArgs.IsArgSet("-datadir")) {
        path = fs::system_complete(gArgs.GetArg("-datadir", ""));
        if (!fs::is_directory(path)) {
            path = "";
            return path;
        }
    } else {
        path = GetDefaultDataDir();
    }
    if (fNetSpecific)
        path /= BaseParams().DataDir();

    if (fs::create_directories(path)) {
        // This is the first run, create wallets subdirectory too
        fs::create_directories(path / "wallets");
    }

    return path;
}

void ClearDatadirCache()
{
    LOCK(csPathCached);

    pathCached = fs::path();
    pathCachedNetSpecific = fs::path();
    g_blocks_path_cache_net_specific = fs::path();
}

fs::path GetConfigFile(const std::string& confPath)
{
    return AbsPathForConfigVal(fs::path(confPath), false);
}
// PAYDAYCOIN
fs::path GetMasternodeConfigFile()
{
    fs::path pathConfigFile(gArgs.GetArg("-mnconf", "masternode.conf"));
    if (!pathConfigFile.is_absolute())
        pathConfigFile = GetDataDir() / pathConfigFile;
    return pathConfigFile;
}
static std::string TrimString(const std::string& str, const std::string& pattern)
{
    std::string::size_type front = str.find_first_not_of(pattern);
    if (front == std::string::npos) {
        return std::string();
    }
    std::string::size_type end = str.find_last_not_of(pattern);
    return str.substr(front, end - front + 1);
}

static bool GetConfigOptions(std::istream& stream, const std::string& filepath, std::string& error, std::vector<std::pair<std::string, std::string>>& options, std::list<SectionInfo>& sections)
{
    std::string str, prefix;
    std::string::size_type pos;
    int linenr = 1;
    while (std::getline(stream, str)) {
        bool used_hash = false;
        if ((pos = str.find('#')) != std::string::npos) {
            str = str.substr(0, pos);
            used_hash = true;
        }
        const static std::string pattern = " \t\r\n";
        str = TrimString(str, pattern);
        if (!str.empty()) {
            if (*str.begin() == '[' && *str.rbegin() == ']') {
                const std::string section = str.substr(1, str.size() - 2);
                sections.emplace_back(SectionInfo{section, filepath, linenr});
                prefix = section + '.';
            } else if (*str.begin() == '-') {
                error = strprintf("parse error on line %i: %s, options in configuration file must be specified without leading -", linenr, str);
                return false;
            } else if ((pos = str.find('=')) != std::string::npos) {
                std::string name = prefix + TrimString(str.substr(0, pos), pattern);
                std::string value = TrimString(str.substr(pos + 1), pattern);
                if (used_hash && name.find("rpcpassword") != std::string::npos) {
                    error = strprintf("parse error on line %i, using # in rpcpassword can be ambiguous and should be avoided", linenr);
                    return false;
                }
                options.emplace_back(name, value);
                if ((pos = name.rfind('.')) != std::string::npos && prefix.length() <= pos) {
                    sections.emplace_back(SectionInfo{name.substr(0, pos), filepath, linenr});
                }
            } else {
                error = strprintf("parse error on line %i: %s", linenr, str);
                if (str.size() >= 2 && str.substr(0, 2) == "no") {
                    error += strprintf(", if you intended to specify a negated option, use %s=1 instead", str);
                }
                return false;
            }
        }
        ++linenr;
    }
    return true;
}

bool ArgsManager::ReadConfigStream(std::istream& stream, const std::string& filepath, std::string& error, bool ignore_invalid_keys)
{
    LOCK(cs_args);
    std::vector<std::pair<std::string, std::string>> options;
    if (!GetConfigOptions(stream, filepath, error, options, m_config_sections)) {
        return false;
    }
    for (const std::pair<std::string, std::string>& option : options) {
        std::string strKey = std::string("-") + option.first;
        std::string strValue = option.second;

        if (InterpretNegatedOption(strKey, strValue)) {
            m_config_args[strKey].clear();
        } else {
            m_config_args[strKey].push_back(strValue);
        }

        // Check that the arg is known
        if (!IsArgKnown(strKey)) {
            if (!ignore_invalid_keys) {
                error = strprintf("Invalid configuration value %s", option.first.c_str());
                return false;
            } else {
                LogPrintf("Ignoring unknown configuration value %s\n", option.first);
            }
        }
    }
    return true;
}

bool ArgsManager::ReadConfigFiles(std::string& error, bool ignore_invalid_keys)
{
    {
        LOCK(cs_args);
        m_config_args.clear();
        m_config_sections.clear();
    }

    const std::string confPath = GetArg("-conf", PAYDAYCOIN_CONF_FILENAME);
    fsbridge::ifstream stream(GetConfigFile(confPath));

    if (!stream.good()) {
        // Create empty worldpaycoin.conf if it does not exist
        FILE* configFile = fopen(GetConfigFile(confPath).string().c_str(), "a");
        if (configFile != NULL) {
            unsigned char rand_pwd[32];
            char rpc_passwd[32];
            GetRandBytes(rand_pwd, 32);
            for (int i = 0; i < 32; i++) {
                rpc_passwd[i] = (rand_pwd[i] % 26) + 97;
            }
            rpc_passwd[31] = '\0';
            unsigned char rand_user[16];
            char rpc_user[16];
            GetRandBytes(rand_user, 16);
            for (int i = 0; i < 16; i++) {
                rpc_user[i] = (rand_user[i] % 26) + 97;
            }
            rpc_user[15] = '\0';
            std::string strHeader = "rpcuser=";
            strHeader += rpc_user;
            strHeader += "\nrpcpassword=";
            strHeader += rpc_passwd;
            strHeader += "\naddnode=pday001.usernodes.org\naddnode=pday002.usernodes.org\naddnode=pday003.usernodes.org\naddnode=pday004.usernodes.org\naddnode=pday005.usernodes.org\naddnode=pday006.usernodes.org\naddnode=pday007.usernodes.org\naddnode=pday008.usernodes.org\n";
            strHeader += "txindex=1\n";
            fwrite(strHeader.c_str(), std::strlen(strHeader.c_str()), 1, configFile);
            fclose(configFile);
        }
        // return; // Nothing to read, so just return
        stream.open(GetConfigFile(confPath));
    }

    // ok to not have a config file
    if (stream.good()) {
        if (!ReadConfigStream(stream, confPath, error, ignore_invalid_keys)) {
            return false;
        }
        // if there is an -includeconf in the override args, but it is empty, that means the user
        // passed '-noincludeconf' on the command line, in which case we should not include anything
        bool emptyIncludeConf;
        {
            LOCK(cs_args);
            emptyIncludeConf = m_override_args.count("-includeconf") == 0;
        }
        if (emptyIncludeConf) {
            std::string chain_id = GetChainName();
            std::vector<std::string> includeconf(GetArgs("-includeconf"));
            {
                // We haven't set m_network yet (that happens in SelectParams()), so manually check
                // for network.includeconf args.
                std::vector<std::string> includeconf_net(GetArgs(std::string("-") + chain_id + ".includeconf"));
                includeconf.insert(includeconf.end(), includeconf_net.begin(), includeconf_net.end());
            }

            // Remove -includeconf from configuration, so we can warn about recursion
            // later
            {
                LOCK(cs_args);
                m_config_args.erase("-includeconf");
                m_config_args.erase(std::string("-") + chain_id + ".includeconf");
            }

            for (const std::string& to_include : includeconf) {
                fsbridge::ifstream include_config(GetConfigFile(to_include));
                if (include_config.good()) {
                    if (!ReadConfigStream(include_config, to_include, error, ignore_invalid_keys)) {
                        return false;
                    }
                    LogPrintf("Included configuration file %s\n", to_include.c_str());
                } else {
                    error = "Failed to include configuration file " + to_include;
                    return false;
                }
            }

            // Warn about recursive -includeconf
            includeconf = GetArgs("-includeconf");
            {
                std::vector<std::string> includeconf_net(GetArgs(std::string("-") + chain_id + ".includeconf"));
                includeconf.insert(includeconf.end(), includeconf_net.begin(), includeconf_net.end());
                std::string chain_id_final = GetChainName();
                if (chain_id_final != chain_id) {
                    // Also warn about recursive includeconf for the chain that was specified in one of the includeconfs
                    includeconf_net = GetArgs(std::string("-") + chain_id_final + ".includeconf");
                    includeconf.insert(includeconf.end(), includeconf_net.begin(), includeconf_net.end());
                }
            }
            for (const std::string& to_include : includeconf) {
                fprintf(stderr, "warning: -includeconf cannot be used from included files; ignoring -includeconf=%s\n", to_include.c_str());
            }
        }
    }

    // If datadir is changed in .conf file:
    ClearDatadirCache();
    if (!fs::is_directory(GetDataDir(false))) {
        error = strprintf("specified data directory \"%s\" does not exist.", gArgs.GetArg("-datadir", "").c_str());
        return false;
    }
    return true;
}

std::string ArgsManager::GetChainName() const
{
    LOCK(cs_args);
    bool fRegTest = ArgsManagerHelper::GetNetBoolArg(*this, "-regtest");
    bool fTestNet = ArgsManagerHelper::GetNetBoolArg(*this, "-testnet");

    if (fTestNet && fRegTest)
        throw std::runtime_error("Invalid combination of -regtest and -testnet.");
    if (fRegTest)
        return CBaseChainParams::REGTEST;
    if (fTestNet)
        return CBaseChainParams::TESTNET;
    return CBaseChainParams::MAIN;
}
// PAYDAYCOIN
fs::path GetGethPidFile()
{
    return AbsPathForConfigVal(fs::path("geth.pid"));
}
void KillProcess(const pid_t& pid){
    if(pid <= 0)
        return;
    LogPrintf("%s: Trying to kill pid %d\n", __func__, pid);
    #ifdef WIN32
        HANDLE handy;
        handy =OpenProcess(SYNCHRONIZE|PROCESS_TERMINATE, TRUE,pid);
        TerminateProcess(handy,0);
    #endif  
    #ifndef WIN32
        int result = 0;
        for(int i =0;i<10;i++){
            MilliSleep(500);
            result = kill( pid, SIGINT ) ;
            if(result == 0){
                LogPrintf("%s: Killing with SIGINT %d\n", __func__, pid);
                continue;
            }  
            LogPrintf("%s: Killed with SIGINT\n", __func__);
            return;
        }
        for(int i =0;i<10;i++){
            MilliSleep(500);
            result = kill( pid, SIGTERM ) ;
            if(result == 0){
                LogPrintf("%s: Killing with SIGTERM %d\n", __func__, pid);
                continue;
            }  
            LogPrintf("%s: Killed with SIGTERM\n", __func__);
            return;
        }
        for(int i =0;i<10;i++){
            MilliSleep(500);
            result = kill( pid, SIGKILL ) ;
            if(result == 0){
                LogPrintf("%s: Killing with SIGKILL %d\n", __func__, pid);
                continue;
            }  
            LogPrintf("%s: Killed with SIGKILL\n", __func__);
            return;
        }  
        LogPrintf("%s: Done trying to kill with SIGINT-SIGTERM-SIGKILL\n", __func__);            
    #endif 
}
std::string GetGethFilename(){
    // For Windows:
    #ifdef WIN32
       return "pdaygeth.nod.exe";
    #endif    
    #ifdef MAC_OSX
        // Mac
        return "pdaygeth.nod";
    #else
        // Linux
        return "pdaygeth.nod";
    #endif
}
std::string GetGethAndRelayerFilepath(){
    // For Windows:
    #ifdef WIN32
       return "/bin/win64/";
    #endif    
    #ifdef MAC_OSX
        // Mac
        return "/bin/osx/";
    #else
        // Linux
        return "/bin/linux/";
    #endif
}
bool StopGethNode(pid_t &pid)
{
    if(pid < 0)
        return false;
    if(fUnitTest || fTPSTest)
        return true;
    if(pid){
        try{
            KillProcess(pid);
            LogPrintf("%s: Geth successfully exited from pid %d\n", __func__, pid);
        }
        catch(...){
            LogPrintf("%s: Geth failed to exit from pid %d\n", __func__, pid);
        }
    }
    {
        boost::filesystem::ifstream ifs(GetGethPidFile(), std::ios::in);
        pid_t pidFile = 0;
        while(ifs >> pidFile){
            if(pidFile && pidFile != pid){
                try{
                    KillProcess(pidFile);
                    LogPrintf("%s: Geth successfully exited from pid %d(from geth.pid)\n", __func__, pidFile);
                }
                catch(...){
                    LogPrintf("%s: Geth failed to exit from pid %d(from geth.pid)\n", __func__, pidFile);
                }
            } 
        }  
    }
    boost::filesystem::remove(GetGethPidFile());
    pid = -1;
    return true;
}

bool StartGethNode(const std::string &exePath, pid_t &pid, bool bGethTestnet, int websocketport)
{
    if(fUnitTest || fTPSTest)
        return true;
    LogPrintf("%s: Starting geth on wsport %d (testnet=%d)...\n", __func__, websocketport, bGethTestnet? 1:0);
    std::string gethFilename = GetGethFilename();
    
    // stop any geth nodes before starting
    StopGethNode(pid);
    fs::path fpathDefault = exePath;
    fpathDefault = fpathDefault.parent_path();
    
    fs::path dataDir = GetDataDir(true) / "geth";


    // current executable path
    fs::path attempt1 = fpathDefault.string() + "/" + gethFilename;
    attempt1 = attempt1.make_preferred();
    // current executable path + bin/[os]/pdaygeth.nod
    fs::path attempt2 = fpathDefault.string() + GetGethAndRelayerFilepath() + gethFilename;
    attempt2 = attempt2.make_preferred();
    // $path
    fs::path attempt3 = gethFilename;
    attempt3 = attempt3.make_preferred();
    // $path + bin/[os]/pdaygeth.nod
    fs::path attempt4 = GetGethAndRelayerFilepath() + gethFilename;
    attempt4 = attempt4.make_preferred();
    // /usr/local/bin/pdaygeth.nod
    fs::path attempt5 = fs::system_complete("/usr/local/bin/").string() + gethFilename;
    attempt5 = attempt5.make_preferred();
    // ../Resources
    fs::path attempt6 = fpathDefault.string() + fs::system_complete("/../Resources/").string() + gethFilename;
    attempt6 = attempt6.make_preferred();

    
  
    #ifndef WIN32
    // Prevent killed child-processes remaining as "defunct"
    struct sigaction sa;
    sa.sa_handler = SIG_DFL;
    sa.sa_flags = SA_NOCLDWAIT;
        
    sigaction( SIGCHLD, &sa, NULL ) ;
        
    // Duplicate ("fork") the process. Will return zero in the child
    // process, and the child's PID in the parent (or negative on error).
    pid = fork() ;
    if( pid < 0 ) {
        LogPrintf("Could not start Geth, pid < 0 %d\n", pid);
        return false;
    }

	// TODO: sanitize environment variables as per
	// https://wiki.sei.cmu.edu/confluence/display/c/ENV03-C.+Sanitize+the+environment+when+invoking+external+programs
    if( pid == 0 ) {
        // Order of looking for the geth binary:
        // 1. current executable directory
        // 2. current executable directory/bin/[os]/paydaycoin_geth
        // 3. $path
        // 4. $path/bin/[os]/paydaycoin_geth
        // 5. /usr/local/bin/paydaycoin_geth
        // 6. ../Resources
        std::string portStr = std::to_string(websocketport);
        char * argvAttempt1[13] = {(char*)attempt1.string().c_str(), 
                (char*)"--ws", (char*)"--wsport", (char*)portStr.c_str(), 
                (char*)"--wsorigins", (char*)"*",
                (char*)"--syncmode", (char*)"light", 
                (char*)"--datadir", (char*)dataDir.c_str(),
                bGethTestnet?(char*)"--rinkeby": NULL,
                bGethTestnet?(char*)"--bootnodes=enode://a24ac7c5484ef4ed0c5eb2d36620ba4e4aa13b8c84684e1b4aab0cebea2ae45cb4d375b77eab56516d34bfbd3c1a833fc51296ff084b770b94fb9028c4d25ccf@52.169.42.101:30303": NULL,
                NULL };
        char * argvAttempt2[13] = {(char*)attempt2.string().c_str(), 
                (char*)"--ws", (char*)"--wsport", (char*)portStr.c_str(), 
                (char*)"--wsorigins", (char*)"*",
                (char*)"--syncmode", (char*)"light", 
                (char*)"--datadir", (char*)dataDir.c_str(),
                bGethTestnet?(char*)"--rinkeby": NULL,
                bGethTestnet?(char*)"--bootnodes=enode://a24ac7c5484ef4ed0c5eb2d36620ba4e4aa13b8c84684e1b4aab0cebea2ae45cb4d375b77eab56516d34bfbd3c1a833fc51296ff084b770b94fb9028c4d25ccf@52.169.42.101:30303": NULL,
                NULL };
        char * argvAttempt3[13] = {(char*)attempt3.string().c_str(), 
                (char*)"--ws", (char*)"--wsport", (char*)portStr.c_str(), 
                (char*)"--wsorigins", (char*)"*",
                (char*)"--syncmode", (char*)"light", 
                (char*)"--datadir", (char*)dataDir.c_str(),
                bGethTestnet?(char*)"--rinkeby": NULL,
                bGethTestnet?(char*)"--bootnodes=enode://a24ac7c5484ef4ed0c5eb2d36620ba4e4aa13b8c84684e1b4aab0cebea2ae45cb4d375b77eab56516d34bfbd3c1a833fc51296ff084b770b94fb9028c4d25ccf@52.169.42.101:30303": NULL,
                NULL };
        char * argvAttempt4[13] = {(char*)attempt4.string().c_str(), 
                (char*)"--ws", (char*)"--wsport", (char*)portStr.c_str(), 
                (char*)"--wsorigins", (char*)"*",
                (char*)"--syncmode", (char*)"light", 
                (char*)"--datadir", (char*)dataDir.c_str(),
                bGethTestnet?(char*)"--rinkeby": NULL,
                bGethTestnet?(char*)"--bootnodes=enode://a24ac7c5484ef4ed0c5eb2d36620ba4e4aa13b8c84684e1b4aab0cebea2ae45cb4d375b77eab56516d34bfbd3c1a833fc51296ff084b770b94fb9028c4d25ccf@52.169.42.101:30303": NULL,
                NULL };
        char * argvAttempt5[13] = {(char*)attempt5.string().c_str(), 
                (char*)"--ws", (char*)"--wsport", (char*)portStr.c_str(), 
                (char*)"--wsorigins", (char*)"*",
                (char*)"--syncmode", (char*)"light", 
                (char*)"--datadir", (char*)dataDir.c_str(),
                bGethTestnet?(char*)"--rinkeby": NULL,
                bGethTestnet?(char*)"--bootnodes=enode://a24ac7c5484ef4ed0c5eb2d36620ba4e4aa13b8c84684e1b4aab0cebea2ae45cb4d375b77eab56516d34bfbd3c1a833fc51296ff084b770b94fb9028c4d25ccf@52.169.42.101:30303": NULL,
                NULL };   
        char * argvAttempt6[13] = {(char*)attempt6.string().c_str(), 
                (char*)"--ws", (char*)"--wsport", (char*)portStr.c_str(), 
                (char*)"--wsorigins", (char*)"*",
                (char*)"--syncmode", (char*)"light", 
                (char*)"--datadir", (char*)dataDir.c_str(),
                bGethTestnet?(char*)"--rinkeby": NULL,
                bGethTestnet?(char*)"--bootnodes=enode://a24ac7c5484ef4ed0c5eb2d36620ba4e4aa13b8c84684e1b4aab0cebea2ae45cb4d375b77eab56516d34bfbd3c1a833fc51296ff084b770b94fb9028c4d25ccf@52.169.42.101:30303": NULL,
                NULL };                                                                   
        execv(argvAttempt1[0], &argvAttempt1[0]); // current directory
        if (errno != 0) {
            LogPrintf("Geth not found at %s, trying in current direction bin folder\n", argvAttempt1[0]);
            execv(argvAttempt2[0], &argvAttempt2[0]);
            if (errno != 0) {
                LogPrintf("Geth not found at %s, trying in $PATH\n", argvAttempt2[0]);
                execvp(argvAttempt3[0], &argvAttempt3[0]); // path
                if (errno != 0) {
                    LogPrintf("Geth not found at %s, trying in $PATH bin folder\n", argvAttempt3[0]);
                    execvp(argvAttempt4[0], &argvAttempt4[0]);
                    if (errno != 0) {
                        LogPrintf("Geth not found at %s, trying in /usr/local/bin folder\n", argvAttempt4[0]);
                        execvp(argvAttempt5[0], &argvAttempt5[0]);
                        if (errno != 0) {
                            LogPrintf("Geth not found at %s, trying in ../Resources\n", argvAttempt4[0]);
                            execvp(argvAttempt6[0], &argvAttempt6[0]);
                            if (errno != 0) {
                                LogPrintf("Geth not found in %s, giving up.\n", argvAttempt6[0]);
                            }
                        }
                    }
                }
            }
        }
    } else {
        boost::filesystem::ofstream ofs(GetGethPidFile(), std::ios::out | std::ios::trunc);
        ofs << pid;
    }
    #else
        std::string portStr = std::to_string(websocketport);
        std::string args = std::string("--ws --wsport ") + portStr + std::string(" --wsorigins * --syncmode light --datadir ") +  dataDir.string();
        if(bGethTestnet) {
            args += std::string(" --rinkeby --bootnodes=enode://a24ac7c5484ef4ed0c5eb2d36620ba4e4aa13b8c84684e1b4aab0cebea2ae45cb4d375b77eab56516d34bfbd3c1a833fc51296ff084b770b94fb9028c4d25ccf@52.169.42.101:30303");
        }
        pid = fork(attempt1.string(), args);
        if( pid <= 0 ) {
            LogPrintf("Geth not found at %s, trying in current direction bin folder\n", attempt1.string());
            pid = fork(attempt2.string(), args);
            if( pid <= 0 ) {
                LogPrintf("Geth not found at %s, trying in $PATH\n", attempt2.string());
                pid = fork(attempt3.string(), args);
                if( pid <= 0 ) {
                    LogPrintf("Geth not found at %s, trying in $PATH bin folder\n", attempt3.string());
                    pid = fork(attempt4.string(), args);
                    if( pid <= 0 ) {
                        LogPrintf("Geth not found in %s, giving up.\n", attempt4.string());
                        return false;
                    }
                }
            }
        }  
        boost::filesystem::ofstream ofs(GetGethPidFile(), std::ios::out | std::ios::trunc);
        ofs << pid;
    #endif
    if(pid > 0)
        LogPrintf("%s: Geth Started with pid %d\n", __func__, pid);
    return true;
}

// PAYDAYCOIN - RELAYER
fs::path GetRelayerPidFile()
{
    return AbsPathForConfigVal(fs::path("relayer.pid"));
}
std::string GetRelayerFilename(){
    // For Windows:
    #ifdef WIN32
       return "pdayrelayer.nod.exe";
    #endif    
    #ifdef MAC_OSX
        // Mac
        return "pdayrelayer.nod";
    #else
        // Linux
        return "pdayrelayer.nod";
    #endif
}
bool StopRelayerNode(pid_t &pid)
{
    if(pid < 0)
        return false;
    if(fUnitTest || fTPSTest)
        return true;
    if(pid){
        try{
            KillProcess(pid);
            LogPrintf("%s: Relayer successfully exited from pid %d\n", __func__, pid);
        }
        catch(...){
            LogPrintf("%s: Relayer failed to exit from pid %d\n", __func__, pid);
        }
    }
    {
        boost::filesystem::ifstream ifs(GetRelayerPidFile(), std::ios::in);
        pid_t pidFile = 0;
        while(ifs >> pidFile){
            if(pidFile && pidFile != pid){
                try{
                    KillProcess(pidFile);
                    LogPrintf("%s: Relayer successfully exited from pid %d(from relayer.pid)\n", __func__, pidFile);
                }
                catch(...){
                    LogPrintf("%s: Relayer failed to exit from pid %d(from relayer.pid)\n", __func__, pidFile);
                }
            } 
        }  
    }
    boost::filesystem::remove(GetRelayerPidFile());
    pid = -1;
    return true;
}

bool StartRelayerNode(const std::string &exePath, pid_t &pid, int rpcport, bool bGethTestnet, int websocketport)
{
    if(fUnitTest || fTPSTest)
        return true;
    
    std::string relayerFilename = GetRelayerFilename();
    std::string infuraKey = "b3d07005e22f4127ba935ce09b9a2a8d"; // TODO: parameterize this later through config file
    // stop any relayer process  before starting
    StopRelayerNode(pid);

    // Get RPC credentials
    std::string strRPCUserColonPass;
    if (gArgs.GetArg("-rpcpassword", "") != "" || !GetAuthCookie(&strRPCUserColonPass)) {
        strRPCUserColonPass = gArgs.GetArg("-rpcuser", "") + ":" + gArgs.GetArg("-rpcpassword", "");
    }
    LogPrintf("%s: Starting relayer on port %d, RPC credentials %s, wsport %d...\n", __func__, rpcport, strRPCUserColonPass, websocketport);


    fs::path fpathDefault = exePath;
    fpathDefault = fpathDefault.parent_path();
    
    fs::path dataDir = GetDataDir(true) / "geth";

    // current executable path
    fs::path attempt1 = fpathDefault.string() + "/" + relayerFilename;
    attempt1 = attempt1.make_preferred();
    // current executable path + bin/[os]/pdayrelayer.nod
    fs::path attempt2 = fpathDefault.string() + GetGethAndRelayerFilepath() + relayerFilename;
    attempt2 = attempt2.make_preferred();
    // $path
    fs::path attempt3 = relayerFilename;
    attempt3 = attempt3.make_preferred();
    // $path + bin/[os]/pdayrelayer.nod
    fs::path attempt4 = GetGethAndRelayerFilepath() + relayerFilename;
    attempt4 = attempt4.make_preferred();
    // /usr/local/bin/pdayrelayer.nod
    fs::path attempt5 = fs::system_complete("/usr/local/bin/").string() + relayerFilename;
    attempt5 = attempt5.make_preferred();
    // ../Resources
    fs::path attempt6 = fpathDefault.string() + fs::system_complete("/../Resources/").string() + relayerFilename;
    attempt6 = attempt6.make_preferred();
    #ifndef WIN32
        // Prevent killed child-processes remaining as "defunct"
        struct sigaction sa;
        sa.sa_handler = SIG_DFL;
        sa.sa_flags = SA_NOCLDWAIT;
      
        sigaction( SIGCHLD, &sa, NULL ) ;
		// Duplicate ("fork") the process. Will return zero in the child
        // process, and the child's PID in the parent (or negative on error).
        pid = fork() ;
        if( pid < 0 ) {
            LogPrintf("Could not start Relayer, pid < 0 %d\n", pid);
            return false;
        }

        if( pid == 0 ) {
            // Order of looking for the relayer binary:
            // 1. current executable directory
            // 2. current executable directory/bin/[os]/paydaycoin_relayer
            // 3. $path
            // 4. $path/bin/[os]/paydaycoin_relayer
            // 5. /usr/local/bin/paydaycoin_relayer
            std::string portStr = std::to_string(websocketport);
            std::string rpcPortStr = std::to_string(rpcport);
            char * argvAttempt1[] = {(char*)attempt1.string().c_str(), 
					(char*)"--ethwsport", (char*)portStr.c_str(),
                    (char*)"--datadir", (char*)dataDir.string().c_str(),
                    (char*)"--infurakey", (char*)infuraKey.c_str(),
					(char*)"--sysrpcusercolonpass", (char*)strRPCUserColonPass.c_str(),
					(char*)"--sysrpcport", (char*)rpcPortStr.c_str(), 
                    (char*)"--gethtestnet", (bGethTestnet?(char*)"1": (char*)"0"), NULL };
            char * argvAttempt2[] = {(char*)attempt2.string().c_str(), 
					(char*)"--ethwsport", (char*)portStr.c_str(),
                    (char*)"--datadir", (char*)dataDir.string().c_str(),
                    (char*)"--infurakey", (char*)infuraKey.c_str(),
					(char*)"--sysrpcusercolonpass", (char*)strRPCUserColonPass.c_str(),
					(char*)"--sysrpcport", (char*)rpcPortStr.c_str(),
                    (char*)"--gethtestnet", (bGethTestnet?(char*)"1": (char*)"0"), NULL };
            char * argvAttempt3[] = {(char*)attempt3.string().c_str(), 
					(char*)"--ethwsport", (char*)portStr.c_str(),
                    (char*)"--datadir", (char*)dataDir.string().c_str(),
                    (char*)"--infurakey", (char*)infuraKey.c_str(),
					(char*)"--sysrpcusercolonpass", (char*)strRPCUserColonPass.c_str(),
					(char*)"--sysrpcport", (char*)rpcPortStr.c_str(),
                    (char*)"--gethtestnet", (bGethTestnet?(char*)"1": (char*)"0"), NULL };
            char * argvAttempt4[] = {(char*)attempt4.string().c_str(), 
					(char*)"--ethwsport", (char*)portStr.c_str(),
                    (char*)"--datadir", (char*)dataDir.string().c_str(),
                    (char*)"--infurakey", (char*)infuraKey.c_str(),
					(char*)"--sysrpcusercolonpass", (char*)strRPCUserColonPass.c_str(),
					(char*)"--sysrpcport", (char*)rpcPortStr.c_str(),
                    (char*)"--gethtestnet", (bGethTestnet?(char*)"1": (char*)"0"), NULL };
            char * argvAttempt5[] = {(char*)attempt5.string().c_str(), 
					(char*)"--ethwsport", (char*)portStr.c_str(),
                    (char*)"--datadir", (char*)dataDir.string().c_str(),
                    (char*)"--infurakey", (char*)infuraKey.c_str(),
					(char*)"--sysrpcusercolonpass", (char*)strRPCUserColonPass.c_str(),
					(char*)"--sysrpcport", (char*)rpcPortStr.c_str(),
                    (char*)"--gethtestnet", (bGethTestnet?(char*)"1": (char*)"0"), NULL };
            char * argvAttempt6[] = {(char*)attempt6.string().c_str(), 
					(char*)"--ethwsport", (char*)portStr.c_str(),
                    (char*)"--datadir", (char*)dataDir.string().c_str(),
                    (char*)"--infurakey", (char*)infuraKey.c_str(),
					(char*)"--sysrpcusercolonpass", (char*)strRPCUserColonPass.c_str(),
					(char*)"--sysrpcport", (char*)rpcPortStr.c_str(),
                    (char*)"--gethtestnet", (bGethTestnet?(char*)"1": (char*)"0"), NULL };
            execv(argvAttempt1[0], &argvAttempt1[0]); // current directory
	        if (errno != 0) {
		        LogPrintf("Relayer not found at %s, trying in current direction bin folder\n", argvAttempt1[0]);
		        execv(argvAttempt2[0], &argvAttempt2[0]);
		        if (errno != 0) {
                    LogPrintf("Relayer not found at %s, trying in $PATH\n", argvAttempt2[0]);
                    execvp(argvAttempt3[0], &argvAttempt3[0]); // path
                    if (errno != 0) {
                        LogPrintf("Relayer not found at %s, trying in $PATH bin folder\n", argvAttempt3[0]);
                        execvp(argvAttempt4[0], &argvAttempt4[0]);
                        if (errno != 0) {
                            LogPrintf("Relayer not found at %s, trying in /usr/local/bin folder\n", argvAttempt4[0]);
                            execvp(argvAttempt5[0], &argvAttempt5[0]);
                            if (errno != 0) {
                                LogPrintf("Relayer not found at %s, trying in ../Resources\n", argvAttempt4[0]);
                                execvp(argvAttempt6[0], &argvAttempt6[0]);
                                if (errno != 0) {
                                    LogPrintf("Relayer not found in %s, giving up.\n", argvAttempt6[0]);
                                }
                            }
                        }
                    }
	            }
	        }
        } else {
            boost::filesystem::ofstream ofs(GetRelayerPidFile(), std::ios::out | std::ios::trunc);
            ofs << pid;
        }
    #else
		std::string portStr = std::to_string(websocketport);
        std::string rpcPortStr = std::to_string(rpcport);
        std::string args =
				std::string("--ethwsport ") + portStr +
                std::string(" --datadir ") + dataDir.string() +
                std::string(" --infurakey ") + infuraKey +
				std::string(" --sysrpcusercolonpass ") + strRPCUserColonPass +
				std::string(" --sysrpcport ") + rpcPortStr +
                std::string(" --gethtestnet ") + (bGethTestnet? std::string("1"):std::string("0")); 
        pid = fork(attempt1.string(), args);
        if( pid <= 0 ) {
            LogPrintf("Relayer not found at %s, trying in current direction bin folder\n", attempt1.string());
            pid = fork(attempt2.string(), args);
            if( pid <= 0 ) {
                LogPrintf("Relayer not found at %s, trying in $PATH\n", attempt2.string());
                pid = fork(attempt3.string(), args);
                if( pid <= 0 ) {
                    LogPrintf("Relayer not found at %s, trying in $PATH bin folder\n", attempt3.string());
                    pid = fork(attempt4.string(), args);
                    if( pid <= 0 ) {
                        LogPrintf("Relayer not found in %s, giving up.\n", attempt4.string());
                        return false;
                    }
                }
            }
        }                         
        boost::filesystem::ofstream ofs(GetRelayerPidFile(), std::ios::out | std::ios::trunc);
        ofs << pid;
	#endif
    if(pid > 0)
        LogPrintf("%s: Relayer started with pid %d\n", __func__, pid);
    return true;
}
/* Parse the contents of /proc/meminfo (in buf), return value of "name"
 * (example: MemTotal) */
static long get_entry(const char* name, const char* buf)
{
    const char* hit = strstr(buf, name);
    if (hit == NULL) {
        return -1;
    }

    errno = 0;
    long val = strtol(hit + strlen(name), NULL, 10);
    if (errno != 0) {
        perror("get_entry: strtol() failed");
        return -1;
    }
    return val;
}

/* Like get_entry(), but exit if the value cannot be found */
static long get_entry_fatal(const char* name, const char* buf)
{
    long val = get_entry(name, buf);
    
    return val;
}

/* If the kernel does not provide MemAvailable (introduced in Linux 3.14),
 * approximate it using other data we can get */
static long available_guesstimate(const char* buf)
{
    long Cached = get_entry_fatal("Cached:", buf);
    long MemFree = get_entry_fatal("MemFree:", buf);
    long Buffers = get_entry_fatal("Buffers:", buf);
    long Shmem = get_entry_fatal("Shmem:", buf);

    return MemFree + Cached + Buffers - Shmem;
}

meminfo_t parse_meminfo()
{
    static FILE* fd;
    static char buf[8192];
    meminfo_t m;

    if (fd == NULL)
        fd = fopen("/proc/meminfo", "r");
    if (fd == NULL) {
        return m;
    }
    rewind(fd);

    size_t len = fread(buf, 1, sizeof(buf) - 1, fd);
    if (len == 0) {
       return m;
    }
    buf[len] = 0; // Make sure buf is zero-terminated

    m.MemTotalKiB = get_entry_fatal("MemTotal:", buf);
    m.SwapTotalKiB = get_entry_fatal("SwapTotal:", buf);
    long SwapFree = get_entry_fatal("SwapFree:", buf);

    long MemAvailable = get_entry("MemAvailable:", buf);
    if (MemAvailable <= -1) {
        MemAvailable = available_guesstimate(buf);
        LogPrintf("Warning: Your kernel does not provide MemAvailable data (needs 3.14+)\n"
                        "         Falling back to guesstimate\n");
    }

    // Calculate percentages
    m.MemAvailablePercent = MemAvailable * 100 / m.MemTotalKiB;
    if (m.SwapTotalKiB > 0) {
        m.SwapFreePercent = SwapFree * 100 / m.SwapTotalKiB;
    } else {
        m.SwapFreePercent = 0;
    }

    // Convert kiB to MiB
    m.MemTotalMiB = m.MemTotalKiB / 1024;
    m.MemAvailableMiB = MemAvailable / 1024;
    m.SwapTotalMiB = m.SwapTotalKiB / 1024;
    m.SwapFreeMiB = SwapFree / 1024;

    return m;
}
bool RenameOver(fs::path src, fs::path dest)
{
#ifdef WIN32
    return MoveFileExW(src.wstring().c_str(), dest.wstring().c_str(),
                       MOVEFILE_REPLACE_EXISTING) != 0;
#else
    int rc = std::rename(src.string().c_str(), dest.string().c_str());
    return (rc == 0);
#endif /* WIN32 */
}

/**
 * Ignores exceptions thrown by Boost's create_directories if the requested directory exists.
 * Specifically handles case where path p exists, but it wasn't possible for the user to
 * write to the parent directory.
 */
bool TryCreateDirectories(const fs::path& p)
{
    try
    {
        return fs::create_directories(p);
    } catch (const fs::filesystem_error&) {
        if (!fs::exists(p) || !fs::is_directory(p))
            throw;
    }

    // create_directories didn't create the directory, it had to have existed already
    return false;
}

bool FileCommit(FILE *file)
{
    if (fflush(file) != 0) { // harmless if redundantly called
        LogPrintf("%s: fflush failed: %d\n", __func__, errno);
        return false;
    }
#ifdef WIN32
    HANDLE hFile = (HANDLE)_get_osfhandle(_fileno(file));
    if (FlushFileBuffers(hFile) == 0) {
        LogPrintf("%s: FlushFileBuffers failed: %d\n", __func__, GetLastError());
        return false;
    }
#else
    #if defined(__linux__) || defined(__NetBSD__)
    if (fdatasync(fileno(file)) != 0 && errno != EINVAL) { // Ignore EINVAL for filesystems that don't support sync
        LogPrintf("%s: fdatasync failed: %d\n", __func__, errno);
        return false;
    }
    #elif defined(MAC_OSX) && defined(F_FULLFSYNC)
    if (fcntl(fileno(file), F_FULLFSYNC, 0) == -1) { // Manpage says "value other than -1" is returned on success
        LogPrintf("%s: fcntl F_FULLFSYNC failed: %d\n", __func__, errno);
        return false;
    }
    #else
    if (fsync(fileno(file)) != 0 && errno != EINVAL) {
        LogPrintf("%s: fsync failed: %d\n", __func__, errno);
        return false;
    }
    #endif
#endif
    return true;
}

bool TruncateFile(FILE *file, unsigned int length) {
#if defined(WIN32)
    return _chsize(_fileno(file), length) == 0;
#else
    return ftruncate(fileno(file), length) == 0;
#endif
}

/**
 * this function tries to raise the file descriptor limit to the requested number.
 * It returns the actual file descriptor limit (which may be more or less than nMinFD)
 */
int RaiseFileDescriptorLimit(int nMinFD) {
#if defined(WIN32)
    return 2048;
#else
    struct rlimit limitFD;
    if (getrlimit(RLIMIT_NOFILE, &limitFD) != -1) {
        if (limitFD.rlim_cur < (rlim_t)nMinFD) {
            limitFD.rlim_cur = nMinFD;
            if (limitFD.rlim_cur > limitFD.rlim_max)
                limitFD.rlim_cur = limitFD.rlim_max;
            setrlimit(RLIMIT_NOFILE, &limitFD);
            getrlimit(RLIMIT_NOFILE, &limitFD);
        }
        return limitFD.rlim_cur;
    }
    return nMinFD; // getrlimit failed, assume it's fine
#endif
}

/**
 * this function tries to make a particular range of a file allocated (corresponding to disk space)
 * it is advisory, and the range specified in the arguments will never contain live data
 */
void AllocateFileRange(FILE *file, unsigned int offset, unsigned int length) {
#if defined(WIN32)
    // Windows-specific version
    HANDLE hFile = (HANDLE)_get_osfhandle(_fileno(file));
    LARGE_INTEGER nFileSize;
    int64_t nEndPos = (int64_t)offset + length;
    nFileSize.u.LowPart = nEndPos & 0xFFFFFFFF;
    nFileSize.u.HighPart = nEndPos >> 32;
    SetFilePointerEx(hFile, nFileSize, 0, FILE_BEGIN);
    SetEndOfFile(hFile);
#elif defined(MAC_OSX)
    // OSX specific version
    fstore_t fst;
    fst.fst_flags = F_ALLOCATECONTIG;
    fst.fst_posmode = F_PEOFPOSMODE;
    fst.fst_offset = 0;
    fst.fst_length = (off_t)offset + length;
    fst.fst_bytesalloc = 0;
    if (fcntl(fileno(file), F_PREALLOCATE, &fst) == -1) {
        fst.fst_flags = F_ALLOCATEALL;
        fcntl(fileno(file), F_PREALLOCATE, &fst);
    }
    ftruncate(fileno(file), fst.fst_length);
#else
    #if defined(__linux__)
    // Version using posix_fallocate
    off_t nEndPos = (off_t)offset + length;
    if (0 == posix_fallocate(fileno(file), 0, nEndPos)) return;
    #endif
    // Fallback version
    // TODO: just write one byte per block
    static const char buf[65536] = {};
    if (fseek(file, offset, SEEK_SET)) {
        return;
    }
    while (length > 0) {
        unsigned int now = 65536;
        if (length < now)
            now = length;
        fwrite(buf, 1, now, file); // allowed to fail; this function is advisory anyway
        length -= now;
    }
#endif
}

#ifdef WIN32
fs::path GetSpecialFolderPath(int nFolder, bool fCreate)
{
    WCHAR pszPath[MAX_PATH] = L"";

    if(SHGetSpecialFolderPathW(nullptr, pszPath, nFolder, fCreate))
    {
        return fs::path(pszPath);
    }

    LogPrintf("SHGetSpecialFolderPathW() failed, could not obtain requested path.\n");
    return fs::path("");
}
#endif

void runCommand(const std::string& strCommand)
{
    if (strCommand.empty()) return;
#ifndef WIN32
    int nErr = ::system(strCommand.c_str());
#else
    int nErr = ::_wsystem(std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>,wchar_t>().from_bytes(strCommand).c_str());
#endif
    if (nErr)
        LogPrintf("runCommand error: system(%s) returned %d\n", strCommand, nErr);
}

void SetupEnvironment()
{
#ifdef HAVE_MALLOPT_ARENA_MAX
    // glibc-specific: On 32-bit systems set the number of arenas to 1.
    // By default, since glibc 2.10, the C library will create up to two heap
    // arenas per core. This is known to cause excessive virtual address space
    // usage in our usage. Work around it by setting the maximum number of
    // arenas to 1.
    if (sizeof(void*) == 4) {
        mallopt(M_ARENA_MAX, 1);
    }
#endif
    // On most POSIX systems (e.g. Linux, but not BSD) the environment's locale
    // may be invalid, in which case the "C" locale is used as fallback.
#if !defined(WIN32) && !defined(MAC_OSX) && !defined(__FreeBSD__) && !defined(__OpenBSD__)
    try {
        std::locale(""); // Raises a runtime error if current locale is invalid
    } catch (const std::runtime_error&) {
        setenv("LC_ALL", "C", 1);
    }
#elif defined(WIN32)
    // Set the default input/output charset is utf-8
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);
#endif
    // The path locale is lazy initialized and to avoid deinitialization errors
    // in multithreading environments, it is set explicitly by the main thread.
    // A dummy locale is used to extract the internal default locale, used by
    // fs::path, which is then used to explicitly imbue the path.
    std::locale loc = fs::path::imbue(std::locale::classic());
#ifndef WIN32
    fs::path::imbue(loc);
#else
    fs::path::imbue(std::locale(loc, new std::codecvt_utf8_utf16<wchar_t>()));
#endif
}

bool SetupNetworking()
{
#ifdef WIN32
    // Initialize Windows Sockets
    WSADATA wsadata;
    int ret = WSAStartup(MAKEWORD(2,2), &wsadata);
    if (ret != NO_ERROR || LOBYTE(wsadata.wVersion ) != 2 || HIBYTE(wsadata.wVersion) != 2)
        return false;
#endif
    return true;
}

int GetNumCores()
{
    return std::thread::hardware_concurrency();
}

std::string CopyrightHolders(const std::string& strPrefix)
{
    std::string strCopyrightHolders = strPrefix + strprintf(_(COPYRIGHT_HOLDERS), _(COPYRIGHT_HOLDERS_SUBSTITUTION));

    // Check for untranslated substitution to make sure PaydayCoin Core copyright is not removed by accident
    if (strprintf(COPYRIGHT_HOLDERS, COPYRIGHT_HOLDERS_SUBSTITUTION).find("PaydayCoin Core") == std::string::npos) {
        strCopyrightHolders += "\n" + strPrefix + "The PaydayCoin Core developers";
    }
    return strCopyrightHolders;
}

// Obtain the application startup time (used for uptime calculation)
int64_t GetStartupTime()
{
    return nStartupTime;
}

fs::path AbsPathForConfigVal(const fs::path& path, bool net_specific)
{
    return fs::absolute(path, GetDataDir(net_specific));
}

int ScheduleBatchPriority()
{
#ifdef SCHED_BATCH
    const static sched_param param{};
    if (int ret = pthread_setschedparam(pthread_self(), SCHED_BATCH, &param)) {
        LogPrintf("Failed to pthread_setschedparam: %s\n", strerror(errno));
        return ret;
    }
    return 0;
#else
    return 1;
#endif
}

namespace util {
#ifdef WIN32
WinCmdLineArgs::WinCmdLineArgs()
{
    wchar_t** wargv = CommandLineToArgvW(GetCommandLineW(), &argc);
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>, wchar_t> utf8_cvt;
    argv = new char*[argc];
    args.resize(argc);
    for (int i = 0; i < argc; i++) {
        args[i] = utf8_cvt.to_bytes(wargv[i]);
        argv[i] = &*args[i].begin();
    }
    LocalFree(wargv);
}

WinCmdLineArgs::~WinCmdLineArgs()
{
    delete[] argv;
}

std::pair<int, char**> WinCmdLineArgs::get()
{
    return std::make_pair(argc, argv);
}
#endif
} // namespace util
