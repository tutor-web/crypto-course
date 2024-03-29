// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "base58.h"
#include "init.h"
#include "main.h"
#include "net.h"
#include "netbase.h"
#include "rpcserver.h"
#include "util.h"
#include "richlistdb.h"
#ifdef ENABLE_WALLET
#include "wallet.h"
#include "walletdb.h"
#endif

#include <stdint.h>

#include <boost/assign/list_of.hpp>
#include "json/json_spirit_utils.h"
#include "json/json_spirit_value.h"

using namespace std;
using namespace boost;
using namespace boost::assign;
using namespace json_spirit;


Value getinfo(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getinfo\n"
            "Returns an object containing various state info.\n"
            "\nResult:\n"
            "{\n"
            "  \"version\": xxxxx,           (numeric) the wallet build version\n"
            "  \"build_date\": xxxxx,        (string) the wallet build date\n"
            "  \"protocolversion\": xxxxx,   (numeric) the protocol version\n"
            "  \"balance\": xxxxxxx,         (numeric) the total smileycoin balance of the wallet\n"
            "  \"blocks\": xxxxxx,           (numeric) the current number of blocks processed in the server\n"
            "  \"timeoffset\": xxxxx,        (numeric) the time offset\n"
            "  \"connections\": xxxxx,       (numeric) the number of connections\n"
            "  \"proxy\": \"host:port\",     (string, optional) the proxy used by the server\n"
            "  \"difficulty\": xxxxxx,       (numeric) the current difficulty\n"
            "  \"keypoololdest\": xxxxxx,    (numeric) the timestamp (seconds since GMT epoch) of the oldest pre-generated key in the key pool\n"
            "  \"keypoolsize\": xxxx,        (numeric) how many new keys are pre-generated\n"
            "  \"unlocked_until\": ttt,      (numeric) the timestamp in seconds since epoch (midnight Jan 1 1970 GMT) that the wallet is unlocked for transfers, or 0 if the wallet is locked\n"
            "  \"paytxfee\": x.xxxx,         (numeric) the transaction fee set in smly/kb\n"
            "  \"relayfee\": x.xxxx,         (numeric) minimum relay fee for non-free transactions in smly/kb\n"
            "  \"errors\": \"...\"           (string) any error messages\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("getinfo", "")
            + HelpExampleRpc("getinfo", "")
        );

    proxyType proxy;
    GetProxy(NET_IPV4, proxy);
    CScript richpubkey;

    Object obj;
    obj.push_back(Pair("version",         (int)CLIENT_VERSION));
    obj.push_back(Pair("build_date",      CLIENT_DATE));
    obj.push_back(Pair("protocolversion", (int)PROTOCOL_VERSION));
#ifdef ENABLE_WALLET
    if (pwalletMain) {
        obj.push_back(Pair("balance",       ValueFromAmount(pwalletMain->GetBalance())));
    }
#endif
    obj.push_back(Pair("blocks",             (int)chainActive.Height()));
    obj.push_back(Pair("timeoffset",         GetTimeOffset()));
    obj.push_back(Pair("connections",        (int)vNodes.size()));
    obj.push_back(Pair("proxy",              (proxy.first.IsValid() ? proxy.first.ToStringIPPort() : string())));
    obj.push_back(Pair("pow_algo_id",        miningAlgo));
    obj.push_back(Pair("pow_algo",           GetAlgoName(miningAlgo)));
    obj.push_back(Pair("difficulty",         (double)GetDifficulty(NULL, miningAlgo)));
    obj.push_back(Pair("difficulty_sha256d", (double)GetDifficulty(NULL, ALGO_SHA256D)));
    obj.push_back(Pair("difficulty_scrypt",  (double)GetDifficulty(NULL, ALGO_SCRYPT)));
    obj.push_back(Pair("difficulty_groestl", (double)GetDifficulty(NULL, ALGO_GROESTL)));
    obj.push_back(Pair("difficulty_skein",   (double)GetDifficulty(NULL, ALGO_SKEIN)));
    obj.push_back(Pair("difficulty_qubit",   (double)GetDifficulty(NULL, ALGO_QUBIT)));
#ifdef ENABLE_WALLET
    if (pwalletMain) {
        obj.push_back(Pair("keypoololdest", pwalletMain->GetOldestKeyPoolTime()));
        obj.push_back(Pair("keypoolsize",   (int)pwalletMain->GetKeyPoolSize()));
    }
    if (pwalletMain && pwalletMain->IsCrypted())
        obj.push_back(Pair("unlocked_until", nWalletUnlockTime));
    obj.push_back(Pair("paytxfee",      ValueFromAmount(nTransactionFee)));
#endif
    obj.push_back(Pair("relayfee",      ValueFromAmount(CTransaction::nMinRelayTxFee)));
    if(RichList.NextRichScriptPubKey(richpubkey)) {
        CTxDestination des;
        ExtractDestination(richpubkey, des);
        obj.push_back(Pair("oldest_rich_address", CBitcoinAddress(des).ToString()));
    }
    else obj.push_back(Pair("oldest_rich_address", ""));   
    obj.push_back(Pair("errors",        GetWarnings("statusbar")));
    return obj;
}


Value getrichaddresses(const Array& params, bool fHelp)
{

    if (fHelp || params.size() != 0)
        throw runtime_error("getrichaddresses\n"
                            "Returns all rich addresses, ordered by height.\n"
                            );
    Object obj;
    std::multiset< std::pair< CScript, std::pair<int64_t, int> >, RichOrderCompare > retset;
    RichList.GetRichAddresses(retset);
    for(std::set< std::pair< CScript, std::pair<int64_t, int> >, RichOrderCompare >::const_iterator it = retset.begin(); it!=retset.end(); it++ )
    {
        CTxDestination des;
        ExtractDestination(it->first, des);
        obj.push_back(Pair(CBitcoinAddress(des).ToString(), it -> second.second));
    }
    return obj;
}

Value getaddressinfo(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error("getaddressbalance\n"
                            "Returns the balance and height of a given address.\n"
                            );
    CBitcoinAddress address = CBitcoinAddress(params[0].get_str());
    if(!address.IsValid())
        throw runtime_error("Not a valid Smileycoin address");
    Object obj;
    CScript key;    
    key.SetDestination(address.Get());
    std::pair<int64_t, int> value;
    if(!pcoinsTip->GetAddressInfo(key, value))
        throw runtime_error("No information available - address has been emptied or never used.");
    obj.push_back(Pair("Balance", ValueFromAmount(value.first)));
    obj.push_back(Pair("Height", value.second));

    return obj; 
}

Value helloworld(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error("helloworld\n"
                            "Returns the classic Hello World! string.\n"
                            );
    return "Hello, World!";
}

#ifdef ENABLE_WALLET
class DescribeAddressVisitor : public boost::static_visitor<Object>
{
public:
    Object operator()(const CNoDestination &dest) const { return Object(); }

    Object operator()(const CKeyID &keyID) const {
        Object obj;
        CPubKey vchPubKey;
        pwalletMain->GetPubKey(keyID, vchPubKey);
        obj.push_back(Pair("isscript", false));
        obj.push_back(Pair("pubkey", HexStr(vchPubKey)));
        obj.push_back(Pair("iscompressed", vchPubKey.IsCompressed()));
        return obj;
    }

    Object operator()(const CScriptID &scriptID) const {
        Object obj;
        obj.push_back(Pair("isscript", true));
        CScript subscript;
        pwalletMain->GetCScript(scriptID, subscript);
        std::vector<CTxDestination> addresses;
        txnouttype whichType;
        int nRequired;
        ExtractDestinations(subscript, whichType, addresses, nRequired);
        obj.push_back(Pair("script", GetTxnOutputType(whichType)));
        obj.push_back(Pair("hex", HexStr(subscript.begin(), subscript.end())));
        Array a;
        BOOST_FOREACH(const CTxDestination& addr, addresses)
            a.push_back(CBitcoinAddress(addr).ToString());
        obj.push_back(Pair("addresses", a));
        if (whichType == TX_MULTISIG)
            obj.push_back(Pair("sigsrequired", nRequired));
        return obj;
    }
};
#endif

Value validateaddress(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "validateaddress \"smileycoinaddress\"\n"
            "\nReturn information about the given smileycoin address.\n"
            "\nArguments:\n"
            "1. \"smileycoinaddress\"     (string, required) The smileycoin address to validate\n"
            "\nResult:\n"
            "{\n"
            "  \"isvalid\" : true|false,            (boolean) If the address is valid or not. If not, this is the only property returned.\n"
            "  \"address\" : \"smileycoinaddress\", (string) The smileycoin address validated\n"
            "  \"ismine\" : true|false,             (boolean) If the address is yours or not\n"
            "  \"isscript\" : true|false,           (boolean) If the key is a script\n"
            "  \"pubkey\" : \"publickeyhex\",       (string) The hex value of the raw public key\n"
            "  \"iscompressed\" : true|false,       (boolean) If the address is compressed\n"
            "  \"account\" : \"account\"            (string) The account associated with the address, \"\" is the default account\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("validateaddress", "\"1PSSGeFHDnKNxiEyFrD1wcEaHr9hrQDDWc\"")
            + HelpExampleRpc("validateaddress", "\"1PSSGeFHDnKNxiEyFrD1wcEaHr9hrQDDWc\"")
        );

    CBitcoinAddress address(params[0].get_str());
    bool isValid = address.IsValid();

    Object ret;
    ret.push_back(Pair("isvalid", isValid));
    if (isValid)
    {
        CTxDestination dest = address.Get();
        string currentAddress = address.ToString();
        ret.push_back(Pair("address", currentAddress));
#ifdef ENABLE_WALLET
        bool fMine = pwalletMain ? IsMine(*pwalletMain, dest) : false;
        ret.push_back(Pair("ismine", fMine));
        if (fMine) {
            Object detail = boost::apply_visitor(DescribeAddressVisitor(), dest);
            ret.insert(ret.end(), detail.begin(), detail.end());
        }
        if (pwalletMain && pwalletMain->mapAddressBook.count(dest))
            ret.push_back(Pair("account", pwalletMain->mapAddressBook[dest].name));
#endif
    }
    return ret;
}

//
// Used by addmultisigaddress / createmultisig:
//
CScript _createmultisig(const Array& params)
{
    int nRequired = params[0].get_int();
    const Array& keys = params[1].get_array();

    // Gather public keys
    if (nRequired < 1)
        throw runtime_error("a multisignature address must require at least one key to redeem");
    if ((int)keys.size() < nRequired)
        throw runtime_error(
            strprintf("not enough keys supplied "
                      "(got %u keys, but need at least %d to redeem)", keys.size(), nRequired));
    std::vector<CPubKey> pubkeys;
    pubkeys.resize(keys.size());
    for (unsigned int i = 0; i < keys.size(); i++)
    {
        const std::string& ks = keys[i].get_str();
#ifdef ENABLE_WALLET
        // Case 1: Bitcoin address and we have full public key:
        CBitcoinAddress address(ks);
        if (pwalletMain && address.IsValid())
        {
            CKeyID keyID;
            if (!address.GetKeyID(keyID))
                throw runtime_error(
                    strprintf("%s does not refer to a key",ks));
            CPubKey vchPubKey;
            if (!pwalletMain->GetPubKey(keyID, vchPubKey))
                throw runtime_error(
                    strprintf("no full public key for address %s",ks));
            if (!vchPubKey.IsFullyValid())
                throw runtime_error(" Invalid public key: "+ks);
            pubkeys[i] = vchPubKey;
        }

        // Case 2: hex public key
        else
#endif
        if (IsHex(ks))
        {
            CPubKey vchPubKey(ParseHex(ks));
            if (!vchPubKey.IsFullyValid())
                throw runtime_error(" Invalid public key: "+ks);
            pubkeys[i] = vchPubKey;
        }
        else
        {
            throw runtime_error(" Invalid public key: "+ks);
        }
    }
    CScript result;
    result.SetMultisig(nRequired, pubkeys);
    return result;
}

Value createmultisig(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 2 || params.size() > 2)
    {
        string msg = "createmultisig nrequired [\"key\",...]\n"
            "\nCreates a multi-signature address with n signature of m keys required.\n"
            "It returns a json object with the address and redeemScript.\n"

            "\nArguments:\n"
            "1. nrequired      (numeric, required) The number of required signatures out of the n keys or addresses.\n"
            "2. \"keys\"       (string, required) A json array of keys which are smileycoin addresses or hex-encoded public keys\n"
            "     [\n"
            "       \"key\"    (string) smileycoin address or hex-encoded public key\n"
            "       ,...\n"
            "     ]\n"

            "\nResult:\n"
            "{\n"
            "  \"address\":\"multisigaddress\",  (string) The value of the new multisig address.\n"
            "  \"redeemScript\":\"script\"       (string) The string value of the hex-encoded redemption script.\n"
            "}\n"

            "\nExamples:\n"
            "\nCreate a multisig address from 2 addresses\n"
            + HelpExampleCli("createmultisig", "2 \"[\\\"16sSauSf5pF2UkUwvKGq4qjNRzBZYqgEL5\\\",\\\"171sgjn4YtPu27adkKGrdDwzRTxnRkBfKV\\\"]\"") +
            "\nAs a json rpc call\n"
            + HelpExampleRpc("createmultisig", "2, \"[\\\"16sSauSf5pF2UkUwvKGq4qjNRzBZYqgEL5\\\",\\\"171sgjn4YtPu27adkKGrdDwzRTxnRkBfKV\\\"]\"")
        ;
        throw runtime_error(msg);
    }

    // Construct using pay-to-script-hash:
    CScript inner = _createmultisig(params);
    CScriptID innerID = inner.GetID();
    CBitcoinAddress address(innerID);

    Object result;
    result.push_back(Pair("address", address.ToString()));
    result.push_back(Pair("redeemScript", HexStr(inner.begin(), inner.end())));

    return result;
}

Value verifymessage(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 3)
        throw runtime_error(
            "verifymessage \"smileycoinaddress\" \"signature\" \"message\"\n"
            "\nVerify a signed message\n"
            "\nArguments:\n"
            "1. \"smileycoinaddress\"  (string, required) The smileycoin address to use for the signature.\n"
            "2. \"signature\"          (string, required) The signature provided by the signer in base 64 encoding (see signmessage).\n"
            "3. \"message\"            (string, required) The message that was signed.\n"
            "\nResult:\n"
            "true|false   (boolean) If the signature is verified or not.\n"
            "\nExamples:\n"
            "\nUnlock the wallet for 30 seconds\n"
            + HelpExampleCli("walletpassphrase", "\"mypassphrase\" 30") +
            "\nCreate the signature\n"
            + HelpExampleCli("signmessage", "\"1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XZ\" \"my message\"") +
            "\nVerify the signature\n"
            + HelpExampleCli("verifymessage", "\"1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XZ\" \"signature\" \"my message\"") +
            "\nAs json rpc\n"
            + HelpExampleRpc("verifymessage", "\"1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XZ\", \"signature\", \"my message\"")
        );

    string strAddress  = params[0].get_str();
    string strSign     = params[1].get_str();
    string strMessage  = params[2].get_str();

    CBitcoinAddress addr(strAddress);
    if (!addr.IsValid())
        throw JSONRPCError(RPC_TYPE_ERROR, "Invalid address");

    CKeyID keyID;
    if (!addr.GetKeyID(keyID))
        throw JSONRPCError(RPC_TYPE_ERROR, "Address does not refer to key");

    bool fInvalid = false;
    vector<unsigned char> vchSig = DecodeBase64(strSign.c_str(), &fInvalid);

    if (fInvalid)
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Malformed base64 encoding");

    CHashWriter ss(SER_GETHASH, 0);
    ss << strMessageMagic;
    ss << strMessage;

    CPubKey pubkey;
    if (!pubkey.RecoverCompact(ss.GetHash(), vchSig))
        return false;

    return (pubkey.GetID() == keyID);
}
