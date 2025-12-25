// Copyright (c) 2014-2024, The Monero Project
// 
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without modification, are
// permitted provided that the following conditions are met:
// 
// 1. Redistributions of source code must retain the above copyright notice, this list of
//    conditions and the following disclaimer.
// 
// 2. Redistributions in binary form must reproduce the above copyright notice, this list
//    of conditions and the following disclaimer in the documentation and/or other
//    materials provided with the distribution.
// 
// 3. Neither the name of the copyright holder nor the names of its contributors may be
//    used to endorse or promote products derived from this software without specific
//    prior written permission.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
// THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
// THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
// 
// Parts of this file are originally copyright (c) 2012-2013 The Cryptonote developers

#include <algorithm>
#include <numeric>
#include <optional>
#include <string>
#include <tuple>
#include <queue>
#include <boost/format.hpp>
#include <boost/optional/optional.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/numeric/conversion/cast.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/range/adaptor/transformed.hpp>
#include <boost/preprocessor/stringize.hpp>
#include <openssl/evp.h>
#include "include_base_utils.h"
using namespace epee;

#include "cryptonote_config.h"
#include "hardforks/hardforks.h"
#include "cryptonote_core/tx_sanity_check.h"
#include "wallet2.h"
#include "wallet_args.h"
#include "cryptonote_basic/cryptonote_format_utils.h"
#include "net/parse.h"
#include "rpc/core_rpc_server_commands_defs.h"
#include "rpc/core_rpc_server_error_codes.h"
#include "misc_language.h"
#include "cryptonote_basic/cryptonote_basic_impl.h"
#include "multisig/multisig.h"
#include "multisig/multisig_account.h"
#include "multisig/multisig_kex_msg.h"
#include "multisig/multisig_tx_builder_ringct.h"
#include "common/command_line.h"
#include "common/threadpool.h"
#include "int-util.h"
#include "profile_tools.h"
#include "crypto/crypto.h"
#include "serialization/binary_utils.h"
#include "serialization/string.h"
#include "cryptonote_basic/blobdatatype.h"
#include "mnemonics/electrum-words.h"
#include "common/i18n.h"
#include "common/util.h"
#include "common/apply_permutation.h"
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include "common/json_util.h"
#include "memwipe.h"
#include "common/base58.h"
#include "common/combinator.h"
#include "common/dns_utils.h"
#include "common/notify.h"
#include "common/perf_timer.h"
#include "ringct/rctSigs.h"
#include "ringdb.h"
#include "device/device_cold.hpp"
#include "device_trezor/device_trezor.hpp"
#include "net/socks_connect.h"

extern "C"
{
#include "crypto/keccak.h"
#include "crypto/crypto-ops.h"
}
using namespace std;
using namespace crypto;
using namespace cryptonote;

#undef MONERO_DEFAULT_LOG_CATEGORY
#define MONERO_DEFAULT_LOG_CATEGORY "wallet.wallet2"

// used to choose when to stop adding outputs to a tx
#define APPROXIMATE_INPUT_BYTES 80

// used to target a given block weight (additional outputs may be added on top to build fee)
#define TX_WEIGHT_TARGET(bytes) (bytes*2/3)

#define UNSIGNED_TX_PREFIX "Monero unsigned tx set\005"
#define SIGNED_TX_PREFIX "Monero signed tx set\005"
#define MULTISIG_UNSIGNED_TX_PREFIX "Monero multisig unsigned tx set\001"

#define RECENT_OUTPUT_RATIO (0.5) // 50% of outputs are from the recent zone
#define RECENT_OUTPUT_DAYS (1.8) // last 1.8 day makes up the recent zone (taken from monerolink.pdf, Miller et al)
#define RECENT_OUTPUT_ZONE ((time_t)(RECENT_OUTPUT_DAYS * 86400))
#define RECENT_OUTPUT_BLOCKS (RECENT_OUTPUT_DAYS * 720)

#define FEE_ESTIMATE_GRACE_BLOCKS 10 // estimate fee valid for that many blocks

#define SECOND_OUTPUT_RELATEDNESS_THRESHOLD 0.0f

#define SUBADDRESS_LOOKAHEAD_MAJOR 50
#define SUBADDRESS_LOOKAHEAD_MINOR 200

#define KEY_IMAGE_EXPORT_FILE_MAGIC "Monero key image export\003"

#define MULTISIG_EXPORT_FILE_MAGIC "Monero multisig export\001"

#define OUTPUT_EXPORT_FILE_MAGIC "Monero output export\004"

#define SEGREGATION_FORK_HEIGHT 99999999
#define TESTNET_SEGREGATION_FORK_HEIGHT 99999999
#define STAGENET_SEGREGATION_FORK_HEIGHT 99999999
#define SEGREGATION_FORK_VICINITY 1500 /* blocks */

#define FIRST_REFRESH_GRANULARITY     1024

#define GAMMA_SHAPE 19.28
#define GAMMA_SCALE (1/1.61)

#define DEFAULT_MIN_OUTPUT_COUNT 5
#define DEFAULT_MIN_OUTPUT_VALUE (2*COIN)

#define DEFAULT_INACTIVITY_LOCK_TIMEOUT 90 // a minute and a half

#define IGNORE_LONG_PAYMENT_ID_FROM_BLOCK_VERSION 12

#define DEFAULT_UNLOCK_TIME (CRYPTONOTE_DEFAULT_TX_SPENDABLE_AGE * DIFFICULTY_TARGET_V2)
#define RECENT_SPEND_WINDOW (15 * DIFFICULTY_TARGET_V2)

static const std::string MULTISIG_SIGNATURE_MAGIC = "SigMultisigPkV1";

static const std::string ASCII_OUTPUT_MAGIC = "MoneroAsciiDataV1";

static const std::string BACKGROUND_WALLET_SUFFIX = ".background";

boost::mutex tools::wallet2::default_daemon_address_lock;
std::string tools::wallet2::default_daemon_address = "";

namespace
{
  std::string get_default_ringdb_path()
  {
    boost::filesystem::path dir = tools::get_default_data_dir();
    // remove .bitmonero, replace with .shared-ringdb
    dir = dir.remove_filename();
    dir /= ".shared-ringdb";
    return dir.string();
  }

  bool keys_intersect(const std::unordered_set<crypto::public_key>& s1, const std::unordered_set<crypto::public_key>& s2)
  {
    if (s1.empty() || s2.empty())
      return false;

    for (const auto& e: s1)
    {
      if (s2.find(e) != s2.end())
        return true;
    }

    return false;
  }

  void add_reason(std::string &reasons, const char *reason)
  {
    if (!reasons.empty())
      reasons += ", ";
    reasons += reason;
  }

  std::string get_text_reason(const cryptonote::COMMAND_RPC_SEND_RAW_TX::response &res)
  {
      std::string reason;
      if (res.low_mixin)
        add_reason(reason, "bad ring size");
      if (res.double_spend)
        add_reason(reason, "double spend");
      if (res.invalid_input)
        add_reason(reason, "invalid input");
      if (res.invalid_output)
        add_reason(reason, "invalid output");
      if (res.too_few_outputs)
        add_reason(reason, "too few outputs");
      if (res.too_big)
        add_reason(reason, "too big");
      if (res.overspend)
        add_reason(reason, "overspend");
      if (res.fee_too_low)
        add_reason(reason, "fee too low");
      if (res.sanity_check_failed)
        add_reason(reason, "tx sanity check failed");
      if (res.not_relayed)
        add_reason(reason, "tx was not relayed");
      return reason;
  }

  size_t get_num_outputs(const std::vector<cryptonote::tx_destination_entry> &dsts, const std::vector<tools::wallet2::transfer_details> &transfers, const std::vector<size_t> &selected_transfers)
  {
    size_t outputs = dsts.size();
    uint64_t needed_money = 0;
    for (const auto& dt: dsts)
      needed_money += dt.amount;
    uint64_t found_money = 0;
    for(size_t idx: selected_transfers)
      found_money += transfers[idx].amount();
    if (found_money != needed_money)
      ++outputs; // change
    if (outputs < 2)
      ++outputs; // extra 0 dummy output
    return outputs;
  }
}

namespace
{
// Create on-demand to prevent static initialization order fiasco issues.
struct options {
  const command_line::arg_descriptor<std::string> daemon_address = {"daemon-address", tools::wallet2::tr("Use daemon instance at <host>:<port>"), ""};
  const command_line::arg_descriptor<std::string> daemon_host = {"daemon-host", tools::wallet2::tr("Use daemon instance at host <arg> instead of localhost"), ""};
  const command_line::arg_descriptor<std::string> proxy = {"proxy", tools::wallet2::tr("[<ip>:]<port> socks proxy to use for daemon connections"), {}, true};
  const command_line::arg_descriptor<bool> trusted_daemon = {"trusted-daemon", tools::wallet2::tr("Enable commands which rely on a trusted daemon"), false};
  const command_line::arg_descriptor<bool> untrusted_daemon = {"untrusted-daemon", tools::wallet2::tr("Disable commands which rely on a trusted daemon"), false};
  const command_line::arg_descriptor<std::string> password = {"password", tools::wallet2::tr("Wallet password (escape/quote as needed)"), "", true};
  const command_line::arg_descriptor<std::string> password_file = wallet_args::arg_password_file();
  const command_line::arg_descriptor<int> daemon_port = {"daemon-port", tools::wallet2::tr("Use daemon instance at port <arg> instead of 18081"), 0};
  const command_line::arg_descriptor<std::string> daemon_login = {"daemon-login", tools::wallet2::tr("Specify username[:password] for daemon RPC client"), "", true};
  const command_line::arg_descriptor<std::string> daemon_ssl = {"daemon-ssl", tools::wallet2::tr("Enable SSL on daemon RPC connections: enabled|disabled|autodetect"), "autodetect"};
  const command_line::arg_descriptor<std::string> daemon_ssl_private_key = {"daemon-ssl-private-key", tools::wallet2::tr("Path to a PEM format private key"), ""};
  const command_line::arg_descriptor<std::string> daemon_ssl_certificate = {"daemon-ssl-certificate", tools::wallet2::tr("Path to a PEM format certificate"), ""};
  const command_line::arg_descriptor<std::string> daemon_ssl_ca_certificates = {"daemon-ssl-ca-certificates", tools::wallet2::tr("Path to file containing concatenated PEM format certificate(s) to replace system CA(s).")};
  const command_line::arg_descriptor<std::vector<std::string>> daemon_ssl_allowed_fingerprints = {"daemon-ssl-allowed-fingerprints", tools::wallet2::tr("List of valid fingerprints of allowed RPC servers")};
  const command_line::arg_descriptor<bool> daemon_ssl_allow_any_cert = {"daemon-ssl-allow-any-cert", tools::wallet2::tr("Allow any SSL certificate from the daemon"), false};
  const command_line::arg_descriptor<bool> daemon_ssl_allow_chained = {"daemon-ssl-allow-chained", tools::wallet2::tr("Allow user (via --daemon-ssl-ca-certificates) chain certificates"), false};
  const command_line::arg_descriptor<bool> testnet = {"testnet", tools::wallet2::tr("For testnet. Daemon must also be launched with --testnet flag"), false};
  const command_line::arg_descriptor<bool> stagenet = {"stagenet", tools::wallet2::tr("For stagenet. Daemon must also be launched with --stagenet flag"), false};
  const command_line::arg_descriptor<std::string, false, true, 2> shared_ringdb_dir = {
    "shared-ringdb-dir", tools::wallet2::tr("Set shared ring database path"),
    get_default_ringdb_path(),
    {{ &testnet, &stagenet }},
    [](std::array<bool, 2> testnet_stagenet, bool defaulted, std::string val)->std::string {
      if (testnet_stagenet[0])
        return (boost::filesystem::path(val) / "testnet").string();
      else if (testnet_stagenet[1])
        return (boost::filesystem::path(val) / "stagenet").string();
      return val;
    }
  };
  const command_line::arg_descriptor<uint64_t> kdf_rounds = {"kdf-rounds", tools::wallet2::tr("Number of rounds for the key derivation function"), 1};
  const command_line::arg_descriptor<std::string> hw_device = {"hw-device", tools::wallet2::tr("HW device to use"), ""};
  const command_line::arg_descriptor<std::string> hw_device_derivation_path = {"hw-device-deriv-path", tools::wallet2::tr("HW device wallet derivation path (e.g., SLIP-10)"), ""};
  const command_line::arg_descriptor<std::string> tx_notify = { "tx-notify" , "Run a program for each new incoming transaction, '%s' will be replaced by the transaction hash" , "" };
  const command_line::arg_descriptor<bool> no_dns = {"no-dns", tools::wallet2::tr("Do not use DNS"), false};
  const command_line::arg_descriptor<bool> offline = {"offline", tools::wallet2::tr("Do not connect to a daemon, nor use DNS"), false};
  const command_line::arg_descriptor<std::string> extra_entropy = {"extra-entropy", tools::wallet2::tr("File containing extra entropy to initialize the PRNG (any data, aim for 256 bits of entropy to be useful, which typically means more than 256 bits of data)")};
  const command_line::arg_descriptor<bool> allow_mismatched_daemon_version = {"allow-mismatched-daemon-version", tools::wallet2::tr("Allow communicating with a daemon that uses a different version"), false};
};

void do_prepare_file_names(const std::string& file_path, std::string& keys_file, std::string& wallet_file, std::string &mms_file)
{
  keys_file = file_path;
  wallet_file = file_path;
  if(string_tools::get_extension(keys_file) == "keys")
  {//provided keys file name
    wallet_file = string_tools::cut_off_extension(wallet_file);
  }else
  {//provided wallet file name
    keys_file += ".keys";
  }
  mms_file = file_path + ".mms";
}

uint64_t calculate_fee(uint64_t fee_per_kb, size_t bytes)
{
  uint64_t kB = (bytes + 1023) / 1024;
  return kB * fee_per_kb;
}

uint64_t calculate_fee_from_weight(uint64_t base_fee, uint64_t weight, uint64_t fee_quantization_mask)
{
  uint64_t fee = weight * base_fee;
  fee = (fee + fee_quantization_mask - 1) / fee_quantization_mask * fee_quantization_mask;
  return fee;
}

std::string get_weight_string(size_t weight)
{
  return std::to_string(weight) + " weight";
}

std::string get_weight_string(const cryptonote::transaction &tx, size_t blob_size)
{
  return get_weight_string(get_transaction_weight(tx, blob_size));
}

std::unique_ptr<tools::wallet2> make_basic(const boost::program_options::variables_map& vm, bool unattended, const options& opts, const std::function<boost::optional<tools::password_container>(const char *, bool)> &password_prompter)
{
  const bool testnet = command_line::get_arg(vm, opts.testnet);
  const bool stagenet = command_line::get_arg(vm, opts.stagenet);
  const network_type nettype = testnet ? TESTNET : stagenet ? STAGENET : MAINNET;
  const uint64_t kdf_rounds = command_line::get_arg(vm, opts.kdf_rounds);
  THROW_WALLET_EXCEPTION_IF(kdf_rounds == 0, tools::error::wallet_internal_error, "KDF rounds must not be 0");

  const bool use_proxy = command_line::has_arg(vm, opts.proxy);
  auto daemon_address = command_line::get_arg(vm, opts.daemon_address);
  auto daemon_host = command_line::get_arg(vm, opts.daemon_host);
  auto daemon_port = command_line::get_arg(vm, opts.daemon_port);
  auto device_name = command_line::get_arg(vm, opts.hw_device);
  auto device_derivation_path = command_line::get_arg(vm, opts.hw_device_derivation_path);
  auto daemon_ssl_private_key = command_line::get_arg(vm, opts.daemon_ssl_private_key);
  auto daemon_ssl_certificate = command_line::get_arg(vm, opts.daemon_ssl_certificate);
  auto daemon_ssl_ca_file = command_line::get_arg(vm, opts.daemon_ssl_ca_certificates);
  auto daemon_ssl_allowed_fingerprints = command_line::get_arg(vm, opts.daemon_ssl_allowed_fingerprints);
  auto daemon_ssl_allow_any_cert = command_line::get_arg(vm, opts.daemon_ssl_allow_any_cert);
  auto daemon_ssl = command_line::get_arg(vm, opts.daemon_ssl);

  // user specified CA file or fingeprints implies enabled SSL by default
  epee::net_utils::ssl_options_t ssl_options = epee::net_utils::ssl_support_t::e_ssl_support_enabled;
  if (daemon_ssl_allow_any_cert)
    ssl_options.verification = epee::net_utils::ssl_verification_t::none;
  else if (!daemon_ssl_ca_file.empty() || !daemon_ssl_allowed_fingerprints.empty())
  {
    std::vector<std::vector<uint8_t>> ssl_allowed_fingerprints{ daemon_ssl_allowed_fingerprints.size() };
    std::transform(daemon_ssl_allowed_fingerprints.begin(), daemon_ssl_allowed_fingerprints.end(), ssl_allowed_fingerprints.begin(), epee::from_hex_locale::to_vector);
    for (const auto &fpr: ssl_allowed_fingerprints)
    {
      THROW_WALLET_EXCEPTION_IF(fpr.size() != SSL_FINGERPRINT_SIZE, tools::error::wallet_internal_error,
          "SHA-256 fingerprint should be " BOOST_PP_STRINGIZE(SSL_FINGERPRINT_SIZE) " bytes long.");
    }

    ssl_options = epee::net_utils::ssl_options_t{
      std::move(ssl_allowed_fingerprints), std::move(daemon_ssl_ca_file)
    };

    if (command_line::get_arg(vm, opts.daemon_ssl_allow_chained))
      ssl_options.verification = epee::net_utils::ssl_verification_t::user_ca;
  }

  if (ssl_options.verification != epee::net_utils::ssl_verification_t::user_certificates || !command_line::is_arg_defaulted(vm, opts.daemon_ssl))
  {
    THROW_WALLET_EXCEPTION_IF(!epee::net_utils::ssl_support_from_string(ssl_options.support, daemon_ssl), tools::error::wallet_internal_error,
       tools::wallet2::tr("Invalid argument for ") + std::string(opts.daemon_ssl.name));
  }

  ssl_options.auth = epee::net_utils::ssl_authentication_t{
    std::move(daemon_ssl_private_key), std::move(daemon_ssl_certificate)
  };

  THROW_WALLET_EXCEPTION_IF(!daemon_address.empty() && !daemon_host.empty() && 0 != daemon_port,
      tools::error::wallet_internal_error, tools::wallet2::tr("can't specify daemon host or port more than once"));

  boost::optional<epee::net_utils::http::login> login{};
  if (command_line::has_arg(vm, opts.daemon_login))
  {
    auto parsed = tools::login::parse(
      command_line::get_arg(vm, opts.daemon_login), false, [password_prompter](bool verify) {
        if (!password_prompter)
        {
          MERROR("Password needed without prompt function");
          return boost::optional<tools::password_container>();
        }
        return password_prompter("Daemon client password", verify);
      }
    );
    if (!parsed)
      return nullptr;

    login.emplace(std::move(parsed->username), std::move(parsed->password).password());
  }

  if (daemon_host.empty())
    daemon_host = "localhost";

  if (!daemon_port)
  {
    daemon_port = get_config(nettype).RPC_DEFAULT_PORT;
  }

  // if no daemon settings are given and we have a previous one, reuse that one
  if (command_line::is_arg_defaulted(vm, opts.daemon_host) && command_line::is_arg_defaulted(vm, opts.daemon_port) && command_line::is_arg_defaulted(vm, opts.daemon_address))
  {
    // not a bug: taking a const ref to a temporary in this way is actually ok in a recent C++ standard
    const std::string &def = tools::wallet2::get_default_daemon_address();
    if (!def.empty())
      daemon_address = def;
  }

  if (daemon_address.empty())
    daemon_address = std::string("http://") + daemon_host + ":" + std::to_string(daemon_port);

  {
    const boost::string_ref real_daemon = boost::string_ref{daemon_address}.substr(0, daemon_address.rfind(':'));

    /* If SSL or proxy is enabled, then a specific cert, CA or fingerprint must
       be specified. This is specific to the wallet. */
    const bool verification_required =
      ssl_options.verification != epee::net_utils::ssl_verification_t::none &&
      (ssl_options.support == epee::net_utils::ssl_support_t::e_ssl_support_enabled || use_proxy);

    THROW_WALLET_EXCEPTION_IF(
      verification_required && !ssl_options.has_strong_verification(real_daemon),
      tools::error::wallet_internal_error,
      tools::wallet2::tr("Enabling --") + std::string{use_proxy ? opts.proxy.name : opts.daemon_ssl.name} + tools::wallet2::tr(" requires --") +
        opts.daemon_ssl_allow_any_cert.name + tools::wallet2::tr(" or --") +
        opts.daemon_ssl_ca_certificates.name + tools::wallet2::tr(" or --") + opts.daemon_ssl_allowed_fingerprints.name + tools::wallet2::tr(" or use of a .onion/.i2p domain")
    );
  }

  std::string proxy;
  if (use_proxy)
  {
    proxy = command_line::get_arg(vm, opts.proxy);
    THROW_WALLET_EXCEPTION_IF(
      !net::get_tcp_endpoint(proxy),
      tools::error::wallet_internal_error,
      std::string{"Invalid address specified for --"} + opts.proxy.name);
  }

  boost::optional<bool> trusted_daemon;
  if (!command_line::is_arg_defaulted(vm, opts.trusted_daemon) || !command_line::is_arg_defaulted(vm, opts.untrusted_daemon))
    trusted_daemon = command_line::get_arg(vm, opts.trusted_daemon) && !command_line::get_arg(vm, opts.untrusted_daemon);
  THROW_WALLET_EXCEPTION_IF(!command_line::is_arg_defaulted(vm, opts.trusted_daemon) && !command_line::is_arg_defaulted(vm, opts.untrusted_daemon),
    tools::error::wallet_internal_error, tools::wallet2::tr("--trusted-daemon and --untrusted-daemon are both seen, assuming untrusted"));

  // set --trusted-daemon if local and not overridden
  if (!trusted_daemon)
  {
    try
    {
      trusted_daemon = false;
      if (tools::is_local_address(daemon_address))
      {
        MINFO(tools::wallet2::tr("Daemon is local, assuming trusted"));
        trusted_daemon = true;
      }
    }
    catch (const std::exception &e) { }
  }

  std::unique_ptr<tools::wallet2> wallet(new tools::wallet2(nettype, kdf_rounds, unattended));
  if (!wallet->init(std::move(daemon_address), std::move(login), std::move(proxy), 0, *trusted_daemon, std::move(ssl_options)))
  {
    THROW_WALLET_EXCEPTION(tools::error::wallet_internal_error, tools::wallet2::tr("failed to initialize the wallet"));
  }
  boost::filesystem::path ringdb_path = command_line::get_arg(vm, opts.shared_ringdb_dir);
  wallet->set_ring_database(ringdb_path.string());
  wallet->get_message_store().set_options(vm);
  wallet->device_name(device_name);
  wallet->device_derivation_path(device_derivation_path);

  if (command_line::get_arg(vm, opts.no_dns))
    wallet->enable_dns(false);

  if (command_line::get_arg(vm, opts.offline))
    wallet->set_offline();

  const std::string extra_entropy = command_line::get_arg(vm, opts.extra_entropy);
  if (!extra_entropy.empty())
  {
    std::string data;
    THROW_WALLET_EXCEPTION_IF(!epee::file_io_utils::load_file_to_string(extra_entropy, data),
        tools::error::wallet_internal_error, "Failed to load extra entropy from " + extra_entropy);
    add_extra_entropy_thread_safe(data.data(), data.size());
  }

  if (command_line::has_arg(vm, opts.allow_mismatched_daemon_version))
    wallet->allow_mismatched_daemon_version(true);

  try
  {
    if (!command_line::is_arg_defaulted(vm, opts.tx_notify))
      wallet->set_tx_notify(std::shared_ptr<tools::Notify>(new tools::Notify(command_line::get_arg(vm, opts.tx_notify).c_str())));
  }
  catch (const std::exception &e)
  {
    MERROR("Failed to parse tx notify spec: " << e.what());
  }

  return wallet;
}

boost::optional<tools::password_container> get_password(const boost::program_options::variables_map& vm, const options& opts, const std::function<boost::optional<tools::password_container>(const char*, bool)> &password_prompter, const bool verify)
{
  if (command_line::has_arg(vm, opts.password) && !command_line::is_arg_defaulted(vm, opts.password_file))
  {
    THROW_WALLET_EXCEPTION(tools::error::wallet_internal_error, tools::wallet2::tr("can't specify more than one of --password and --password-file"));
  }

  if (command_line::has_arg(vm, opts.password))
  {
    return tools::password_container{command_line::get_arg(vm, opts.password)};
  }

  if (!command_line::is_arg_defaulted(vm, opts.password_file))
  {
    std::string password;
    const auto password_file = command_line::get_arg(vm, opts.password_file);
    bool r = epee::file_io_utils::load_file_to_string(password_file,
                                                      password);
    THROW_WALLET_EXCEPTION_IF(!r, tools::error::wallet_internal_error, tools::wallet2::tr("the password file specified could not be read"));

    // Remove line breaks the user might have inserted
    boost::trim_right_if(password, boost::is_any_of("\r\n"));
    return {tools::password_container{std::move(password)}};
  }

  THROW_WALLET_EXCEPTION_IF(!password_prompter, tools::error::wallet_internal_error, tools::wallet2::tr("no password specified; use --prompt-for-password to prompt for a password"));

  return password_prompter(verify ? tools::wallet2::tr("Enter a new password for the wallet") : tools::wallet2::tr("Wallet password"), verify);
}

std::pair<std::unique_ptr<tools::wallet2>, tools::password_container> generate_from_json(const std::string& json_file, const boost::program_options::variables_map& vm, bool unattended, const options& opts, const std::function<boost::optional<tools::password_container>(const char *, bool)> &password_prompter)
{
  const bool testnet = command_line::get_arg(vm, opts.testnet);
  const bool stagenet = command_line::get_arg(vm, opts.stagenet);
  const network_type nettype = testnet ? TESTNET : stagenet ? STAGENET : MAINNET;

  /* GET_FIELD_FROM_JSON_RETURN_ON_ERROR Is a generic macro that can return
  false. Gcc will coerce this into unique_ptr(nullptr), but clang correctly
  fails. This large wrapper is for the use of that macro */
  std::unique_ptr<tools::wallet2> wallet;
  epee::wipeable_string password;
  const auto do_generate = [&]() -> bool {
    std::string buf;
    if (!epee::file_io_utils::load_file_to_string(json_file, buf)) {
      THROW_WALLET_EXCEPTION(tools::error::wallet_internal_error, std::string(tools::wallet2::tr("Failed to load file ")) + json_file);
      return false;
    }

    rapidjson::Document json;
    if (json.Parse(buf.c_str()).HasParseError()) {
      THROW_WALLET_EXCEPTION(tools::error::wallet_internal_error, tools::wallet2::tr("Failed to parse JSON"));
      return false;
    }

    GET_FIELD_FROM_JSON_RETURN_ON_ERROR(json, version, unsigned, Uint, true, 0);
    const int current_version = 1;
    THROW_WALLET_EXCEPTION_IF(field_version > current_version, tools::error::wallet_internal_error,
      ((boost::format(tools::wallet2::tr("Version %u too new, we can only grok up to %u")) % field_version % current_version)).str());

    GET_FIELD_FROM_JSON_RETURN_ON_ERROR(json, filename, std::string, String, true, std::string());

    GET_FIELD_FROM_JSON_RETURN_ON_ERROR(json, scan_from_height, uint64_t, Uint64, false, 0);
    const bool recover = true;

    GET_FIELD_FROM_JSON_RETURN_ON_ERROR(json, password, std::string, String, false, std::string());

    GET_FIELD_FROM_JSON_RETURN_ON_ERROR(json, viewkey, std::string, String, false, std::string());
    crypto::secret_key viewkey;
    if (field_viewkey_found)
    {
      cryptonote::blobdata viewkey_data;
      if(!epee::string_tools::parse_hexstr_to_binbuff(field_viewkey, viewkey_data) || viewkey_data.size() != sizeof(crypto::secret_key))
      {
        THROW_WALLET_EXCEPTION(tools::error::wallet_internal_error, tools::wallet2::tr("failed to parse view key secret key"));
      }
      viewkey = *reinterpret_cast<const crypto::secret_key*>(viewkey_data.data());
      crypto::public_key pkey;
      if (viewkey == crypto::null_skey)
        THROW_WALLET_EXCEPTION(tools::error::wallet_internal_error, tools::wallet2::tr("view secret key may not be all zeroes"));
      if (!crypto::secret_key_to_public_key(viewkey, pkey)) {
        THROW_WALLET_EXCEPTION(tools::error::wallet_internal_error, tools::wallet2::tr("failed to verify view key secret key"));
      }
    }

    GET_FIELD_FROM_JSON_RETURN_ON_ERROR(json, spendkey, std::string, String, false, std::string());
    crypto::secret_key spendkey;
    if (field_spendkey_found)
    {
      cryptonote::blobdata spendkey_data;
      if(!epee::string_tools::parse_hexstr_to_binbuff(field_spendkey, spendkey_data) || spendkey_data.size() != sizeof(crypto::secret_key))
      {
        THROW_WALLET_EXCEPTION(tools::error::wallet_internal_error, tools::wallet2::tr("failed to parse spend key secret key"));
      }
      spendkey = *reinterpret_cast<const crypto::secret_key*>(spendkey_data.data());
      crypto::public_key pkey;
      if (spendkey == crypto::null_skey)
        THROW_WALLET_EXCEPTION(tools::error::wallet_internal_error, tools::wallet2::tr("spend secret key may not be all zeroes"));
      if (!crypto::secret_key_to_public_key(spendkey, pkey)) {
        THROW_WALLET_EXCEPTION(tools::error::wallet_internal_error, tools::wallet2::tr("failed to verify spend key secret key"));
      }
    }

    GET_FIELD_FROM_JSON_RETURN_ON_ERROR(json, seed, std::string, String, false, std::string());
    std::string old_language;
    crypto::secret_key recovery_key;
    bool restore_deterministic_wallet = false;
    if (field_seed_found)
    {
      if (!crypto::ElectrumWords::words_to_bytes(field_seed, recovery_key, old_language))
      {
        THROW_WALLET_EXCEPTION(tools::error::wallet_internal_error, tools::wallet2::tr("Electrum-style word list failed verification"));
      }
      restore_deterministic_wallet = true;

      GET_FIELD_FROM_JSON_RETURN_ON_ERROR(json, seed_passphrase, std::string, String, false, std::string());
      if (field_seed_passphrase_found)
      {
        if (!field_seed_passphrase.empty())
          recovery_key = cryptonote::decrypt_key(recovery_key, field_seed_passphrase);
      }
    }

    GET_FIELD_FROM_JSON_RETURN_ON_ERROR(json, address, std::string, String, false, std::string());

    GET_FIELD_FROM_JSON_RETURN_ON_ERROR(json, create_address_file, int, Int, false, false);
    bool create_address_file = field_create_address_file;

    // compatibility checks
    if (!field_seed_found && !field_viewkey_found && !field_spendkey_found)
    {
      THROW_WALLET_EXCEPTION(tools::error::wallet_internal_error, tools::wallet2::tr("At least one of either an Electrum-style word list, private view key, or private spend key must be specified"));
    }
    if (field_seed_found && (field_viewkey_found || field_spendkey_found))
    {
      THROW_WALLET_EXCEPTION(tools::error::wallet_internal_error, tools::wallet2::tr("Both Electrum-style word list and private key(s) specified"));
    }

    // if an address was given, we check keys against it, and deduce the spend
    // public key if it was not given
    if (field_address_found)
    {
      cryptonote::address_parse_info info;
      if(!get_account_address_from_str(info, nettype, field_address))
      {
        THROW_WALLET_EXCEPTION(tools::error::wallet_internal_error, tools::wallet2::tr("invalid address"));
      }
      if (field_viewkey_found)
      {
        crypto::public_key pkey;
        if (!crypto::secret_key_to_public_key(viewkey, pkey)) {
          THROW_WALLET_EXCEPTION(tools::error::wallet_internal_error, tools::wallet2::tr("failed to verify view key secret key"));
        }
        if (info.address.m_view_public_key != pkey) {
          THROW_WALLET_EXCEPTION(tools::error::wallet_internal_error, tools::wallet2::tr("view key does not match standard address"));
        }
      }
      if (field_spendkey_found)
      {
        crypto::public_key pkey;
        if (!crypto::secret_key_to_public_key(spendkey, pkey)) {
          THROW_WALLET_EXCEPTION(tools::error::wallet_internal_error, tools::wallet2::tr("failed to verify spend key secret key"));
        }
        if (info.address.m_spend_public_key != pkey) {
          THROW_WALLET_EXCEPTION(tools::error::wallet_internal_error, tools::wallet2::tr("spend key does not match standard address"));
        }
      }
    }

    const bool deprecated_wallet = restore_deterministic_wallet && ((old_language == crypto::ElectrumWords::old_language_name) ||
      crypto::ElectrumWords::get_is_old_style_seed(field_seed));
    THROW_WALLET_EXCEPTION_IF(deprecated_wallet, tools::error::wallet_internal_error,
      tools::wallet2::tr("Cannot generate deprecated wallets from JSON"));

    wallet.reset(make_basic(vm, unattended, opts, password_prompter).release());
    wallet->set_refresh_from_block_height(field_scan_from_height);
    wallet->explicit_refresh_from_block_height(field_scan_from_height_found);
    if (!old_language.empty())
      wallet->set_seed_language(old_language);

    try
    {
      if (!field_seed.empty())
      {
        wallet->generate(field_filename, field_password, recovery_key, recover, false, create_address_file);
        password = field_password;
      }
      else if (field_viewkey.empty() && !field_spendkey.empty())
      {
        wallet->generate(field_filename, field_password, spendkey, recover, false, create_address_file);
        password = field_password;
      }
      else
      {
        cryptonote::account_public_address address;
        if (!crypto::secret_key_to_public_key(viewkey, address.m_view_public_key)) {
          THROW_WALLET_EXCEPTION(tools::error::wallet_internal_error, tools::wallet2::tr("failed to verify view key secret key"));
        }

        if (field_spendkey.empty())
        {
          // if we have an address but no spend key, we can deduce the spend public key
          // from the address
          if (field_address_found)
          {
            cryptonote::address_parse_info info;
            if(!get_account_address_from_str(info, nettype, field_address))
            {
              THROW_WALLET_EXCEPTION(tools::error::wallet_internal_error, std::string(tools::wallet2::tr("failed to parse address: ")) + field_address);
            }
            address.m_spend_public_key = info.address.m_spend_public_key;
          }
          else
          {
            THROW_WALLET_EXCEPTION(tools::error::wallet_internal_error, tools::wallet2::tr("Address must be specified in order to create watch-only wallet"));
          }
          wallet->generate(field_filename, field_password, address, viewkey, create_address_file);
          password = field_password;
        }
        else
        {
          if (!crypto::secret_key_to_public_key(spendkey, address.m_spend_public_key)) {
            THROW_WALLET_EXCEPTION(tools::error::wallet_internal_error, tools::wallet2::tr("failed to verify spend key secret key"));
          }
          wallet->generate(field_filename, field_password, address, spendkey, viewkey, create_address_file);
          password = field_password;
        }
      }
    }
    catch (const std::exception& e)
    {
      THROW_WALLET_EXCEPTION(tools::error::wallet_internal_error, std::string(tools::wallet2::tr("failed to generate new wallet: ")) + e.what());
    }
    return true;
  };

  if (do_generate())
  {
    return {std::move(wallet), tools::password_container(password)};
  }
  return {nullptr, tools::password_container{}};
}

std::string strjoin(const std::vector<size_t> &V, const char *sep)
{
  std::stringstream ss;
  bool first = true;
  for (const auto &v: V)
  {
    if (!first)
      ss << sep;
    ss << std::to_string(v);
    first = false;
  }
  return ss.str();
}

static bool emplace_or_replace(std::unordered_multimap<crypto::hash, tools::wallet2::pool_payment_details> &container,
  const crypto::hash &key, const tools::wallet2::pool_payment_details &pd)
{
  auto range = container.equal_range(key);
  for (auto i = range.first; i != range.second; ++i)
  {
    if (i->second.m_pd.m_tx_hash == pd.m_pd.m_tx_hash && i->second.m_pd.m_subaddr_index == pd.m_pd.m_subaddr_index)
    {
      i->second = pd;
      return false;
    }
  }
  container.emplace(key, pd);
  return true;
}

void drop_from_short_history(std::list<crypto::hash> &short_chain_history, size_t N)
{
  std::list<crypto::hash>::iterator right;
  // drop early N off, skipping the genesis block
  if (short_chain_history.size() > N) {
    right = short_chain_history.end();
    std::advance(right,-1);
    std::list<crypto::hash>::iterator left = right;
    std::advance(left, -N);
    short_chain_history.erase(left, right);
  }
}

size_t estimate_rct_tx_size(int n_inputs, int mixin, int n_outputs, size_t extra_size, bool bulletproof, bool clsag, bool bulletproof_plus, bool use_view_tags)
{
  size_t size = 0;

  // tx prefix

  // first few bytes
  size += 1 + 6;

  // vin
  size += n_inputs * (1+6+(mixin+1)*2+32);

  // vout
  size += n_outputs * (6+32);

  // extra
  size += extra_size;

  // rct signatures

  // type
  size += 1;

  // rangeSigs
  if (bulletproof || bulletproof_plus)
  {
    size_t log_padded_outputs = 0;
    while ((1<<log_padded_outputs) < n_outputs)
      ++log_padded_outputs;
    size += (2 * (6 + log_padded_outputs) + (bulletproof_plus ? 6 : (4 + 5))) * 32 + 3;
  }
  else
    size += (2*64*32+32+64*32) * n_outputs;

  // MGs/CLSAGs
  if (clsag)
    size += n_inputs * (32 * (mixin+1) + 64);
  else
    size += n_inputs * (64 * (mixin+1) + 32);

  if (use_view_tags)
    size += n_outputs * sizeof(crypto::view_tag);

  // mixRing - not serialized, can be reconstructed
  /* size += 2 * 32 * (mixin+1) * n_inputs; */

  // pseudoOuts
  size += 32 * n_inputs;
  // ecdhInfo
  size += 8 * n_outputs;
  // outPk - only commitment is saved
  size += 32 * n_outputs;
  // txnFee
  size += 4;

  LOG_PRINT_L2("estimated " << (bulletproof_plus ? "bulletproof plus" : bulletproof ? "bulletproof" : "borromean") << " rct tx size for " << n_inputs << " inputs with ring size " << (mixin+1) << " and " << n_outputs << " outputs: " << size << " (" << ((32 * n_inputs/*+1*/) + 2 * 32 * (mixin+1) * n_inputs + 32 * n_outputs) << " saved)");
  return size;
}

size_t estimate_tx_size(bool use_rct, int n_inputs, int mixin, int n_outputs, size_t extra_size, bool bulletproof, bool clsag, bool bulletproof_plus, bool use_view_tags)
{
  if (use_rct)
    return estimate_rct_tx_size(n_inputs, mixin, n_outputs, extra_size, bulletproof, clsag, bulletproof_plus, use_view_tags);
  else
    return n_inputs * (mixin+1) * APPROXIMATE_INPUT_BYTES + extra_size + (use_view_tags ? (n_outputs * sizeof(crypto::view_tag)) : 0);
}

uint64_t estimate_tx_weight(bool use_rct, int n_inputs, int mixin, int n_outputs, size_t extra_size, bool bulletproof, bool clsag, bool bulletproof_plus, bool use_view_tags)
{
  size_t size = estimate_tx_size(use_rct, n_inputs, mixin, n_outputs, extra_size, bulletproof, clsag, bulletproof_plus, use_view_tags);
  if (use_rct && (bulletproof || bulletproof_plus) && n_outputs > 2)
  {
    const uint64_t bp_base = (32 * ((bulletproof_plus ? 6 : 9) + 7 * 2)) / 2; // notional size of a 2 output proof, normalized to 1 proof (ie, divided by 2)
    size_t log_padded_outputs = 2;
    while ((1<<log_padded_outputs) < n_outputs)
      ++log_padded_outputs;
    uint64_t nlr = 2 * (6 + log_padded_outputs);
    const uint64_t bp_size = 32 * ((bulletproof_plus ? 6 : 9) + nlr);
    const uint64_t bp_clawback = (bp_base * (1<<log_padded_outputs) - bp_size) * 4 / 5;
    MDEBUG("clawback on size " << size << ": " << bp_clawback);
    size += bp_clawback;
  }
  return size;
}

uint8_t get_bulletproof_fork()
{
  return 8;
}

uint8_t get_bulletproof_plus_fork()
{
  return HF_VERSION_BULLETPROOF_PLUS;
}

uint8_t get_clsag_fork()
{
  return HF_VERSION_CLSAG;
}

uint8_t get_view_tag_fork()
{
  return HF_VERSION_VIEW_TAGS;
}

uint64_t calculate_fee(bool use_per_byte_fee, const cryptonote::transaction &tx, size_t blob_size, uint64_t base_fee, uint64_t fee_quantization_mask)
{
  if (use_per_byte_fee)
    return calculate_fee_from_weight(base_fee, cryptonote::get_transaction_weight(tx, blob_size), fee_quantization_mask);
  else
    return calculate_fee(base_fee, blob_size);
}

bool get_short_payment_id(crypto::hash8 &payment_id8, const tools::wallet2::pending_tx &ptx, hw::device &hwdev)
{
  std::vector<tx_extra_field> tx_extra_fields;
  parse_tx_extra(ptx.tx.extra, tx_extra_fields); // ok if partially parsed
  cryptonote::tx_extra_nonce extra_nonce;
  if (find_tx_extra_field_by_type(tx_extra_fields, extra_nonce))
  {
    if(get_encrypted_payment_id_from_tx_extra_nonce(extra_nonce.nonce, payment_id8))
    {
      if (ptx.dests.empty())
      {
        MWARNING("Encrypted payment id found, but no destinations public key, cannot decrypt");
        return false;
      }
      return hwdev.decrypt_payment_id(payment_id8, ptx.dests[0].addr.m_view_public_key, ptx.tx_key);
    }
  }
  return false;
}

uint64_t get_outgoing_amount(const cryptonote::transaction &tx, const uint64_t amount_spent)
{
  return tx.version == 1 ? get_outs_money_amount(tx) : (amount_spent - tx.rct_signatures.txnFee);
}

tools::wallet2::tx_construction_data get_construction_data_with_decrypted_short_payment_id(const tools::wallet2::pending_tx &ptx, hw::device &hwdev)
{
  tools::wallet2::tx_construction_data construction_data = ptx.construction_data;
  crypto::hash8 payment_id = null_hash8;
  if (get_short_payment_id(payment_id, ptx, hwdev))
  {
    // Remove encrypted
    remove_field_from_tx_extra(construction_data.extra, typeid(cryptonote::tx_extra_nonce));
    // Add decrypted
    std::string extra_nonce;
    set_encrypted_payment_id_to_tx_extra_nonce(extra_nonce, payment_id);
    THROW_WALLET_EXCEPTION_IF(!add_extra_nonce_to_tx_extra(construction_data.extra, extra_nonce),
        tools::error::wallet_internal_error, "Failed to add decrypted payment id to tx extra");
    LOG_PRINT_L1("Decrypted payment ID: " << payment_id);
  }
  return construction_data;
}

uint32_t get_subaddress_clamped_sum(uint32_t idx, uint32_t extra)
{
  static constexpr uint32_t uint32_max = std::numeric_limits<uint32_t>::max();
  if (idx > uint32_max - extra)
    return uint32_max;
  return idx + extra;
}

static void setup_shim(hw::wallet_shim * shim, tools::wallet2 * wallet)
{
  shim->get_tx_pub_key_from_received_outs = std::bind(&tools::wallet2::get_tx_pub_key_from_received_outs, wallet, std::placeholders::_1);
}

bool get_pruned_tx(const cryptonote::COMMAND_RPC_GET_TRANSACTIONS::entry &entry, cryptonote::transaction &tx, crypto::hash &tx_hash)
{
  cryptonote::blobdata bd;

  // easy case if we have the whole tx
  if (!entry.as_hex.empty() || (!entry.prunable_as_hex.empty() && !entry.pruned_as_hex.empty()))
  {
    CHECK_AND_ASSERT_MES(epee::string_tools::parse_hexstr_to_binbuff(entry.as_hex.empty() ? entry.pruned_as_hex + entry.prunable_as_hex : entry.as_hex, bd), false, "Failed to parse tx data");
    CHECK_AND_ASSERT_MES(cryptonote::parse_and_validate_tx_from_blob(bd, tx), false, "Invalid tx data");
    tx_hash = cryptonote::get_transaction_hash(tx);
    // if the hash was given, check it matches
    CHECK_AND_ASSERT_MES(entry.tx_hash.empty() || epee::string_tools::pod_to_hex(tx_hash) == entry.tx_hash, false,
        "Response claims a different hash than the data yields");
    return true;
  }
  // case of a pruned tx with its prunable data hash
  if (!entry.pruned_as_hex.empty() && !entry.prunable_hash.empty())
  {
    crypto::hash ph;
    CHECK_AND_ASSERT_MES(epee::string_tools::hex_to_pod(entry.prunable_hash, ph), false, "Failed to parse prunable hash");
    CHECK_AND_ASSERT_MES(epee::string_tools::parse_hexstr_to_binbuff(entry.pruned_as_hex, bd), false, "Failed to parse pruned data");
    CHECK_AND_ASSERT_MES(parse_and_validate_tx_base_from_blob(bd, tx), false, "Invalid base tx data");
    // only v2 txes can calculate their txid after pruned
    if (bd[0] > 1)
    {
      tx_hash = cryptonote::get_pruned_transaction_hash(tx, ph);
    }
    else
    {
      // for v1, we trust the dameon
      CHECK_AND_ASSERT_MES(epee::string_tools::hex_to_pod(entry.tx_hash, tx_hash), false, "Failed to parse tx hash");
    }
    return true;
  }
  return false;
}

// Given M (threshold) and N (total), calculate the number of private multisig keys each
// signer should have. This value is equal to (N - 1) choose (N - M)
// Prereq: M >= 1 && N >= M && N <= 16
uint64_t num_priv_multisig_keys_post_setup(uint64_t threshold, uint64_t total)
{
  THROW_WALLET_EXCEPTION_IF(threshold < 1 || total < threshold || threshold > 16,
    tools::error::wallet_internal_error, "Invalid arguments to num_priv_multisig_keys_post_setup");

  uint64_t n_multisig_keys = 1;
  for (uint64_t i = 2; i <= total - 1; ++i) n_multisig_keys *= i; // multiply by (N - 1)!
  for (uint64_t i = 2; i <= total - threshold; ++i) n_multisig_keys /= i; // divide by (N - M)!
  for (uint64_t i = 2; i <= threshold - 1; ++i) n_multisig_keys /= i; // divide by ((N - 1) - (N - M))!
  return n_multisig_keys;
}

/**
 * @brief Derives the chacha key to encrypt wallet cache files given the chacha key to encrypt the wallet keys files
 *
 * @param keys_data_key the chacha key that encrypts wallet keys files
 * @return crypto::chacha_key the chacha key that encrypts the wallet cache files
 */
crypto::chacha_key derive_cache_key(const crypto::chacha_key& keys_data_key, const unsigned char domain_separator)
{
  static_assert(HASH_SIZE == sizeof(crypto::chacha_key), "Mismatched sizes of hash and chacha key");

  crypto::chacha_key cache_key;
  epee::mlocked<tools::scrubbed_arr<char, HASH_SIZE+1>> cache_key_data;
  memcpy(cache_key_data.data(), &keys_data_key, HASH_SIZE);
  cache_key_data[HASH_SIZE] = domain_separator;
  cn_fast_hash(cache_key_data.data(), HASH_SIZE+1, (crypto::hash&) cache_key);

  return cache_key;
}
  //-----------------------------------------------------------------
} //namespace

namespace tools
{
constexpr const std::chrono::seconds wallet2::rpc_timeout;
const char* wallet2::tr(const char* str) { return i18n_translate(str, "tools::wallet2"); }

gamma_picker::gamma_picker(const std::vector<uint64_t> &rct_offsets, double shape, double scale):
    rct_offsets(rct_offsets)
{
  gamma = std::gamma_distribution<double>(shape, scale);
  THROW_WALLET_EXCEPTION_IF(rct_offsets.size() < std::max<size_t>(1, CRYPTONOTE_DEFAULT_TX_SPENDABLE_AGE), error::wallet_internal_error, "Bad offset calculation");
  const size_t blocks_in_a_year = 86400 * 365 / DIFFICULTY_TARGET_V2;
  const size_t blocks_to_consider = std::min<size_t>(rct_offsets.size(), blocks_in_a_year);
  const size_t outputs_to_consider = rct_offsets.back() - (blocks_to_consider < rct_offsets.size() ? rct_offsets[rct_offsets.size() - blocks_to_consider - 1] : 0);
  begin = rct_offsets.data();
  end = rct_offsets.data() + rct_offsets.size() - (std::max(1, CRYPTONOTE_DEFAULT_TX_SPENDABLE_AGE) - 1);
  num_rct_outputs = *(end - 1);
  THROW_WALLET_EXCEPTION_IF(num_rct_outputs == 0, error::wallet_internal_error, "No rct outputs");
  average_output_time = DIFFICULTY_TARGET_V2 * blocks_to_consider / static_cast<double>(outputs_to_consider); // this assumes constant target over the whole rct range
};

gamma_picker::gamma_picker(const std::vector<uint64_t> &rct_offsets): gamma_picker(rct_offsets, GAMMA_SHAPE, GAMMA_SCALE) {}

uint64_t gamma_picker::pick()
{
  double x = gamma(engine);
  x = exp(x);

  if (x > DEFAULT_UNLOCK_TIME) 
  {
    // We are trying to select an output from the chain that appeared 'x' seconds before the
    // current chain tip, where 'x' is selected from the gamma distribution recommended in Miller et al.
    // (https://arxiv.org/pdf/1704.04299/).
    // Our method is to get the average time delta between outputs in the recent past, estimate the number of
    // outputs 'n' that would have appeared between 'chain_tip - x' and 'chain_tip', select the real output at
    // 'current_num_outputs - n', then randomly select an output from the block where that output appears.
    // Source code to paper: https://github.com/maltemoeser/moneropaper
    //
    // Due to the 'default spendable age' mechanic in Monero, 'current_num_outputs' only contains
    // currently *unlocked* outputs, which means the earliest output that can be selected is not at the chain tip!
    // Therefore, we must offset 'x' so it matches up with the timing of the outputs being considered. We do
    // this by saying if 'x` equals the expected age of the first unlocked output (compared to the current
    // chain tip - i.e. DEFAULT_UNLOCK_TIME), then select the first unlocked output.
    x -= DEFAULT_UNLOCK_TIME;
  }
  else 
  {
    // If the spent time suggested by the gamma is less than the unlock time, that means the gamma is suggesting an output
    // that is no longer feasible to be spent (possible since the gamma was constructed when consensus rules did not enforce the
    // lock time). The assumption made in this code is that an output expected spent quicker than the unlock time would likely
    // be spent within RECENT_SPEND_WINDOW after allowed. So it returns an output that falls between 0 and the RECENT_SPEND_WINDOW.
    // The RECENT_SPEND_WINDOW was determined with empirical analysis of observed data.
    x = crypto::rand_idx(static_cast<uint64_t>(RECENT_SPEND_WINDOW));
  }

  uint64_t output_index = x / average_output_time;
  if (output_index >= num_rct_outputs)
    return std::numeric_limits<uint64_t>::max(); // bad pick
  output_index = num_rct_outputs - 1 - output_index;

  const uint64_t *it = std::lower_bound(begin, end, output_index);
  THROW_WALLET_EXCEPTION_IF(it == end, error::wallet_internal_error, "output_index not found");
  uint64_t index = std::distance(begin, it);

  const uint64_t first_rct = index == 0 ? 0 : rct_offsets[index - 1];
  const uint64_t n_rct = rct_offsets[index] - first_rct;
  if (n_rct == 0)
    return std::numeric_limits<uint64_t>::max(); // bad pick
  MTRACE("Picking 1/" << n_rct << " in block " << index);
  return first_rct + crypto::rand_idx(n_rct);
};

boost::mutex wallet_keys_unlocker::lockers_lock;
std::map<wallet2*, std::size_t> wallet_keys_unlocker::lockers_per_wallet = {};
wallet_keys_unlocker::wallet_keys_unlocker(wallet2 &w, const epee::wipeable_string *password):
  w(w),
  should_relock(true)
{
  static_assert(!std::is_move_assignable_v<wallet2> && !std::is_move_constructible_v<wallet2>,
    "Keeping track of wallet instances by pointer doesn't work if wallet2 can be moved");

  if (password == nullptr || !w.is_key_encryption_enabled())
  {
    should_relock = false;
    return;
  }

  w.generate_chacha_key_from_password(*password, key);

  boost::lock_guard<boost::mutex> lock(lockers_lock);
  if (lockers_per_wallet[std::addressof(w)]++ > 0)
    return;

  w.decrypt_keys(key);
}

wallet_keys_unlocker::~wallet_keys_unlocker()
{
  if (!should_relock)
    return;

  try
  {
    boost::lock_guard<boost::mutex> lock(lockers_lock);
    wallet2* w_ptr = std::addressof(w);
    if (lockers_per_wallet[w_ptr] == 0)
    {
      MERROR("There are no lockers in wallet_keys_unlocker dtor");
      return;
    }
    if (--lockers_per_wallet[w_ptr] > 0)
      return; // there are other unlock-ers for this wallet, do nothing for now
    lockers_per_wallet.erase(w_ptr);
    w.encrypt_keys(key);
  }
  catch (...)
  {
    MERROR("Failed to re-encrypt wallet keys");
    // do not propagate through dtor, we'd crash
  }
}

void wallet_device_callback::on_button_request(uint64_t code)
{
  if (wallet)
    wallet->on_device_button_request(code);
}

void wallet_device_callback::on_button_pressed()
{
  if (wallet)
    wallet->on_device_button_pressed();
}

boost::optional<epee::wipeable_string> wallet_device_callback::on_pin_request()
{
  if (wallet)
    return wallet->on_device_pin_request();
  return boost::none;
}

boost::optional<epee::wipeable_string> wallet_device_callback::on_passphrase_request(bool & on_device)
{
  if (wallet)
    return wallet->on_device_passphrase_request(on_device);
  else
    on_device = true;
  return boost::none;
}

void wallet_device_callback::on_progress(const hw::device_progress& event)
{
  if (wallet)
    wallet->on_device_progress(event);
}

wallet2::wallet2(network_type nettype, uint64_t kdf_rounds, bool unattended, std::unique_ptr<epee::net_utils::http::http_client_factory> http_client_factory):
  m_http_client(http_client_factory->create()),
  m_upper_transaction_weight_limit(0),
  m_run(true),
  m_callback(0),
  m_trusted_daemon(false),
  m_nettype(nettype),
  m_multisig_rounds_passed(0),
  m_always_confirm_transfers(true),
  m_print_ring_members(false),
  m_store_tx_info(true),
  m_default_mixin(0),
  m_default_priority(fee_priority::Default),
  m_refresh_type(RefreshOptimizeCoinbase),
  m_auto_refresh(true),
  m_first_refresh_done(false),
  m_refresh_from_block_height(0),
  m_explicit_refresh_from_block_height(true),
  m_skip_to_height(0),
  m_ask_password(AskPasswordToDecrypt),
  m_max_reorg_depth(ORPHANED_BLOCKS_MAX_COUNT),
  m_min_output_count(0),
  m_min_output_value(0),
  m_merge_destinations(false),
  m_confirm_backlog(true),
  m_confirm_backlog_threshold(0),
  m_confirm_export_overwrite(true),
  m_auto_low_priority(true),
  m_segregate_pre_fork_outputs(true),
  m_key_reuse_mitigation2(true),
  m_segregation_height(0),
  m_ignore_fractional_outputs(true),
  m_ignore_outputs_above(MONEY_SUPPLY),
  m_ignore_outputs_below(0),
  m_track_uses(false),
  m_is_background_wallet(false),
  m_background_sync_type(BackgroundSyncOff),
  m_background_syncing(false),
  m_processing_background_cache(false),
  m_custom_background_key(boost::none),
  m_show_wallet_name_when_locked(false),
  m_inactivity_lock_timeout(DEFAULT_INACTIVITY_LOCK_TIMEOUT),
  m_setup_background_mining(BackgroundMiningMaybe),
  m_is_initialized(false),
  m_kdf_rounds(kdf_rounds),
  is_old_file_format(false),
  m_watch_only(false),
  m_multisig(false),
  m_multisig_threshold(0),
  m_node_rpc_proxy(*m_http_client, m_daemon_rpc_mutex),
  m_account_public_address{crypto::null_pkey, crypto::null_pkey},
  m_subaddress_lookahead_major(SUBADDRESS_LOOKAHEAD_MAJOR),
  m_subaddress_lookahead_minor(SUBADDRESS_LOOKAHEAD_MINOR),
  m_original_keys_available(false),
  m_message_store(http_client_factory->create()),
  m_key_device_type(hw::device::device_type::SOFTWARE),
  m_ring_history_saved(true),
  m_ringdb(),
  m_last_block_reward(0),
  m_unattended(unattended),
  m_devices_registered(false),
  m_device_last_key_image_sync(0),
  m_use_dns(true),
  m_offline(false),
  m_rpc_version(0),
  m_export_format(ExportFormat::Binary),
  m_enable_multisig(false),
  m_pool_info_query_time(0),
  m_has_ever_refreshed_from_node(false),
  m_allow_mismatched_daemon_version(false)
{
}

wallet2::~wallet2()
{
  deinit();
}

bool wallet2::has_testnet_option(const boost::program_options::variables_map& vm)
{
  return command_line::get_arg(vm, options().testnet);
}

bool wallet2::has_stagenet_option(const boost::program_options::variables_map& vm)
{
  return command_line::get_arg(vm, options().stagenet);
}

bool wallet2::has_proxy_option() const
{
  return !m_proxy.empty();
}

std::string wallet2::device_name_option(const boost::program_options::variables_map& vm)
{
  return command_line::get_arg(vm, options().hw_device);
}

std::string wallet2::device_derivation_path_option(const boost::program_options::variables_map &vm)
{
  return command_line::get_arg(vm, options().hw_device_derivation_path);
}

void wallet2::init_options(boost::program_options::options_description& desc_params)
{
  const options opts{};
  command_line::add_arg(desc_params, opts.daemon_address);
  command_line::add_arg(desc_params, opts.daemon_host);
  command_line::add_arg(desc_params, opts.proxy);
  command_line::add_arg(desc_params, opts.trusted_daemon);
  command_line::add_arg(desc_params, opts.untrusted_daemon);
  command_line::add_arg(desc_params, opts.password);
  command_line::add_arg(desc_params, opts.password_file);
  command_line::add_arg(desc_params, opts.daemon_port);
  command_line::add_arg(desc_params, opts.daemon_login);
  command_line::add_arg(desc_params, opts.daemon_ssl);
  command_line::add_arg(desc_params, opts.daemon_ssl_private_key);
  command_line::add_arg(desc_params, opts.daemon_ssl_certificate);
  command_line::add_arg(desc_params, opts.daemon_ssl_ca_certificates);
  command_line::add_arg(desc_params, opts.daemon_ssl_allowed_fingerprints);
  command_line::add_arg(desc_params, opts.daemon_ssl_allow_any_cert);
  command_line::add_arg(desc_params, opts.daemon_ssl_allow_chained);
  command_line::add_arg(desc_params, opts.testnet);
  command_line::add_arg(desc_params, opts.stagenet);
  command_line::add_arg(desc_params, opts.shared_ringdb_dir);
  command_line::add_arg(desc_params, opts.kdf_rounds);
  mms::message_store::init_options(desc_params);
  command_line::add_arg(desc_params, opts.hw_device);
  command_line::add_arg(desc_params, opts.hw_device_derivation_path);
  command_line::add_arg(desc_params, opts.tx_notify);
  command_line::add_arg(desc_params, opts.no_dns);
  command_line::add_arg(desc_params, opts.offline);
  command_line::add_arg(desc_params, opts.extra_entropy);
  command_line::add_arg(desc_params, opts.allow_mismatched_daemon_version);
}

std::pair<std::unique_ptr<wallet2>, tools::password_container> wallet2::make_from_json(const boost::program_options::variables_map& vm, bool unattended, const std::string& json_file, const std::function<boost::optional<tools::password_container>(const char *, bool)> &password_prompter)
{
  const options opts{};
  return generate_from_json(json_file, vm, unattended, opts, password_prompter);
}

std::pair<std::unique_ptr<wallet2>, password_container> wallet2::make_from_file(
  const boost::program_options::variables_map& vm, bool unattended, const std::string& wallet_file, const std::function<boost::optional<tools::password_container>(const char *, bool)> &password_prompter)
{
  const options opts{};
  auto pwd = get_password(vm, opts, password_prompter, false);
  if (!pwd)
  {
    return {nullptr, password_container{}};
  }
  auto wallet = make_basic(vm, unattended, opts, password_prompter);
  if (wallet && !wallet_file.empty())
  {
    wallet->load(wallet_file, pwd->password());
  }
  return {std::move(wallet), std::move(*pwd)};
}

std::pair<std::unique_ptr<wallet2>, password_container> wallet2::make_new(const boost::program_options::variables_map& vm, bool unattended, const std::function<boost::optional<password_container>(const char *, bool)> &password_prompter)
{
  const options opts{};
  auto pwd = get_password(vm, opts, password_prompter, true);
  if (!pwd)
  {
    return {nullptr, password_container{}};
  }
  return {make_basic(vm, unattended, opts, password_prompter), std::move(*pwd)};
}

std::unique_ptr<wallet2> wallet2::make_dummy(const boost::program_options::variables_map& vm, bool unattended, const std::function<boost::optional<tools::password_container>(const char *, bool)> &password_prompter)
{
  const options opts{};
  return make_basic(vm, unattended, opts, password_prompter);
}

//----------------------------------------------------------------------------------------------------
bool wallet2::set_daemon(std::string daemon_address, boost::optional<epee::net_utils::http::login> daemon_login, bool trusted_daemon, epee::net_utils::ssl_options_t ssl_options, const std::string& proxy)
{
  boost::lock_guard<boost::recursive_mutex> lock(m_daemon_rpc_mutex);

  if(daemon_address.empty()) {
    daemon_address.append("http://localhost:" + std::to_string(get_config(m_nettype).RPC_DEFAULT_PORT));
  }

  if(m_http_client->is_connected())
    m_http_client->disconnect();
  CHECK_AND_ASSERT_MES2(m_proxy.empty() || proxy.empty() , "It is not possible to set global proxy (--proxy) and daemon specific proxy together.");
  if(m_proxy.empty())
    CHECK_AND_ASSERT_MES(set_proxy(proxy), false, "failed to set proxy address");
  const bool changed = m_daemon_address != daemon_address;
  m_daemon_address = std::move(daemon_address);
  m_daemon_login = std::move(daemon_login);
  m_trusted_daemon = trusted_daemon;
  if (changed)
  {
    m_rpc_version = 0;
    m_node_rpc_proxy.invalidate();
    m_pool_info_query_time = 0;
  }

  const std::string address = get_daemon_address();
  MINFO("setting daemon to " << address);
  bool ret =  m_http_client->set_server(address, get_daemon_login(), std::move(ssl_options));
  if (ret)
  {
    CRITICAL_REGION_LOCAL(default_daemon_address_lock);
    default_daemon_address = address;
  }
  return ret;
}
//----------------------------------------------------------------------------------------------------
bool wallet2::set_proxy(const std::string &address)
{
  return m_http_client->set_proxy(address);
}
//----------------------------------------------------------------------------------------------------
bool wallet2::init(std::string daemon_address, boost::optional<epee::net_utils::http::login> daemon_login, const std::string &proxy_address, uint64_t upper_transaction_weight_limit, bool trusted_daemon, epee::net_utils::ssl_options_t ssl_options)
{
  m_proxy = proxy_address;
  CHECK_AND_ASSERT_MES(set_proxy(m_proxy), false, "failed to set proxy address");
  m_checkpoints.init_default_checkpoints(m_nettype);
  m_is_initialized = true;
  m_upper_transaction_weight_limit = upper_transaction_weight_limit;
  return set_daemon(daemon_address, daemon_login, trusted_daemon, std::move(ssl_options));
}
//----------------------------------------------------------------------------------------------------
bool wallet2::is_deterministic() const
{
  crypto::secret_key second;
  keccak((uint8_t *)&get_account().get_keys().m_spend_secret_key, sizeof(crypto::secret_key), (uint8_t *)&second, sizeof(crypto::secret_key));
  sc_reduce32((uint8_t *)&second);
  return memcmp(second.data,get_account().get_keys().m_view_secret_key.data, sizeof(crypto::secret_key)) == 0;
}
//----------------------------------------------------------------------------------------------------
bool wallet2::get_seed(epee::wipeable_string& electrum_words, const epee::wipeable_string &passphrase) const
{
  bool keys_deterministic = is_deterministic();
  if (!keys_deterministic)
  {
    std::cout << "This is not a deterministic wallet" << std::endl;
    return false;
  }
  if (seed_language.empty())
  {
    std::cout << "seed_language not set" << std::endl;
    return false;
  }

  crypto::secret_key key = get_account().get_keys().m_spend_secret_key;
  if (!passphrase.empty())
    key = cryptonote::encrypt_key(key, passphrase);
  if (!crypto::ElectrumWords::bytes_to_words(key, electrum_words, seed_language))
  {
    std::cout << "Failed to create seed from key for language: " << seed_language << std::endl;
    return false;
  }

  return true;
}
//----------------------------------------------------------------------------------------------------
bool wallet2::get_multisig_seed(epee::wipeable_string& seed, const epee::wipeable_string &passphrase) const
{
  const multisig::multisig_account_status ms_status{get_multisig_status()};

  if (!ms_status.multisig_is_active)
  {
    std::cout << "This is not a multisig wallet" << std::endl;
    return false;
  }
  if (!ms_status.is_ready)
  {
    std::cout << "This multisig wallet is not yet finalized" << std::endl;
    return false;
  }

  const uint64_t num_expected_ms_keys = num_priv_multisig_keys_post_setup(ms_status.threshold, ms_status.total);

  crypto::secret_key skey;
  crypto::public_key pkey;
  const account_keys &keys = get_account().get_keys();
  THROW_WALLET_EXCEPTION_IF(num_expected_ms_keys != keys.m_multisig_keys.size(),
    error::wallet_internal_error, "Unexpected number of private multisig keys")
  epee::wipeable_string data;
  data.append((const char*)&ms_status.threshold, sizeof(uint32_t));
  data.append((const char*)&ms_status.total, sizeof(uint32_t));
  skey = keys.m_spend_secret_key;
  data.append((const char*)&skey, sizeof(skey));
  pkey = keys.m_account_address.m_spend_public_key;
  data.append((const char*)&pkey, sizeof(pkey));
  skey = keys.m_view_secret_key;
  data.append((const char*)&skey, sizeof(skey));
  pkey = keys.m_account_address.m_view_public_key;
  data.append((const char*)&pkey, sizeof(pkey));
  for (const auto &skey: keys.m_multisig_keys)
    data.append((const char*)&skey, sizeof(skey));
  for (const auto &signer: m_multisig_signers)
    data.append((const char*)&signer, sizeof(signer));

  if (!passphrase.empty())
  {
    crypto::secret_key key;
    crypto::cn_slow_hash(passphrase.data(), passphrase.size(), (crypto::hash&)key);
    sc_reduce32((unsigned char*)key.data);
    data = encrypt(data, key, true);
  }

  seed = epee::to_hex::wipeable_string({(const unsigned char*)data.data(), data.size()});

  return true;
}
//----------------------------------------------------------------------------------------------------
bool wallet2::reconnect_device()
{
  bool r = true;
  hw::device &hwdev = lookup_device(m_device_name);
  hwdev.set_name(m_device_name);
  hwdev.set_network_type(m_nettype);
  hwdev.set_derivation_path(m_device_derivation_path);
  hwdev.set_callback(get_device_callback());
  r = hwdev.init();
  if (!r){
    MERROR("Could not init device");
    return false;
  }

  r = hwdev.connect();
  if (!r){
    MERROR("Could not connect to the device");
    return false;
  }

  m_account.set_device(hwdev);
  return true;
}
//----------------------------------------------------------------------------------------------------
/*!
 * \brief Gets the seed language
 */
const std::string &wallet2::get_seed_language() const
{
  return seed_language;
}
/*!
 * \brief Sets the seed language
 * \param language  Seed language to set to
 */
void wallet2::set_seed_language(const std::string &language)
{
  seed_language = language;
}
//----------------------------------------------------------------------------------------------------
cryptonote::account_public_address wallet2::get_subaddress(const cryptonote::subaddress_index& index) const
{
  hw::device &hwdev = m_account.get_device();
  return hwdev.get_subaddress(m_account.get_keys(), index);
}
//----------------------------------------------------------------------------------------------------
boost::optional<cryptonote::subaddress_index> wallet2::get_subaddress_index(const cryptonote::account_public_address& address) const
{
  auto index = m_subaddresses.find(address.m_spend_public_key);
  if (index == m_subaddresses.end())
    return boost::none;
  return index->second;
}
//----------------------------------------------------------------------------------------------------
crypto::public_key wallet2::get_subaddress_spend_public_key(const cryptonote::subaddress_index& index) const
{
  hw::device &hwdev = m_account.get_device();
  return hwdev.get_subaddress_spend_public_key(m_account.get_keys(), index);
}
//----------------------------------------------------------------------------------------------------
std::string wallet2::get_subaddress_as_str(const cryptonote::subaddress_index& index) const
{
  cryptonote::account_public_address address = get_subaddress(index);
  return cryptonote::get_account_address_as_str(m_nettype, !index.is_zero(), address);
}
//----------------------------------------------------------------------------------------------------
std::string wallet2::get_integrated_address_as_str(const crypto::hash8& payment_id) const
{
  return cryptonote::get_account_integrated_address_as_str(m_nettype, get_address(), payment_id);
}
//----------------------------------------------------------------------------------------------------
void wallet2::add_subaddress_account(const std::string& label)
{
  uint32_t index_major = (uint32_t)get_num_subaddress_accounts();
  expand_subaddresses({index_major, 0});
  m_subaddress_labels[index_major][0] = label;
}
//----------------------------------------------------------------------------------------------------
void wallet2::add_subaddress(uint32_t index_major, const std::string& label)
{
  THROW_WALLET_EXCEPTION_IF(index_major >= m_subaddress_labels.size(), error::account_index_outofbound);
  uint32_t index_minor = (uint32_t)get_num_subaddresses(index_major);
  expand_subaddresses({index_major, index_minor});
  m_subaddress_labels[index_major][index_minor] = label;
}
//----------------------------------------------------------------------------------------------------
bool wallet2::should_expand(const cryptonote::subaddress_index &index) const
{
  const uint32_t last_major = m_subaddress_labels.size() - 1 > (std::numeric_limits<uint32_t>::max() - m_subaddress_lookahead_major) ? std::numeric_limits<uint32_t>::max()  : (m_subaddress_labels.size() + m_subaddress_lookahead_major - 1);
  if (index.major > last_major)
    return false;
  const size_t nsub = index.major < m_subaddress_labels.size() ? m_subaddress_labels[index.major].size() : 0;
  const uint32_t last_minor = nsub - 1 > (std::numeric_limits<uint32_t>::max() - m_subaddress_lookahead_minor) ? std::numeric_limits<uint32_t>::max()  : (nsub + m_subaddress_lookahead_minor - 1);
  if (index.minor > last_minor)
    return false;
  return true;
}
//----------------------------------------------------------------------------------------------------
void wallet2::expand_subaddresses(const cryptonote::subaddress_index& index)
{
  // check if index will overflow container (usually only applicable on 32-bit systems)
  if constexpr (sizeof(std::size_t) <= sizeof(std::uint32_t))
  {
    static constexpr std::uint32_t max_idx = static_cast<std::uint32_t>(std::numeric_limits<std::size_t>::max());
    const bool cannot_label_index = index.major == max_idx || index.minor == max_idx;
    THROW_WALLET_EXCEPTION_IF(cannot_label_index, error::wallet_internal_error, "subaddress index out of range");
  }

  // resize subaddress labels just big enough that `m_subaddress_labels[index.major][index.minor]` is present
  if (m_subaddress_labels.size() <= index.major)
    m_subaddress_labels.resize(index.major + 1, {"Untitled account"});
  auto &subaddr_labels_in_account = m_subaddress_labels[index.major];
  if (subaddr_labels_in_account.size() <= index.minor)
    subaddr_labels_in_account.resize(index.minor + 1);
  get_account_tags(); //trigger m_account_tags integrity checks

  // compile all indices present in subaddress scanning map, as well as every major index
  std::unordered_set<cryptonote::subaddress_index> all_indices;
  std::unordered_map<std::uint32_t, std::uint32_t> lowest_missing_minor;
  for (const auto &p : m_subaddresses)
  {
    all_indices.insert(p.second);
    lowest_missing_minor[p.second.major];
  }

  // find lowest "missing" minor index in map, for all major indices
  // this is an optimization which allows us to skip re-generating pubkeys that we already have
  for (auto &p : lowest_missing_minor)
  {
    const std::uint32_t major = p.first;
    std::uint32_t &minor = p.second;
    while (all_indices.count({major, minor}) && minor < std::numeric_limits<std::uint32_t>::max())
      ++minor;
  }

  // resize subaddress scanning map to highest historical received subaddress index plus lookahead
  hw::device &hwdev = m_account.get_device();
  const std::uint32_t major_base = std::max<std::uint32_t>(m_subaddress_labels.size(), 1) - 1;
  const std::uint32_t major_end = get_subaddress_clamped_sum(major_base, m_subaddress_lookahead_major);
  for (std::uint32_t major = 0; major < major_end; ++major)
  {
    const std::size_t n_minor_labels = (major < m_subaddress_labels.size()) ? m_subaddress_labels.at(major).size() : 0;
    const std::uint32_t minor_base = std::max<std::uint32_t>(n_minor_labels, 1) - 1;
    const std::uint32_t minor_end = get_subaddress_clamped_sum(minor_base, m_subaddress_lookahead_minor);
    const std::uint32_t minor_begin = lowest_missing_minor.count(major) ? lowest_missing_minor.at(major) : 0;
    if (minor_begin >= minor_end)
      continue;
    const std::vector<crypto::public_key> pkeys
      = hwdev.get_subaddress_spend_public_keys(m_account.get_keys(), major, minor_begin, minor_end);
    for (std::uint32_t minor = minor_begin; minor < minor_end; ++minor)
    {
      const crypto::public_key &D = pkeys.at(minor - minor_begin);
      m_subaddresses[D] = {major, minor};
    }
  }
}
//----------------------------------------------------------------------------------------------------
void wallet2::create_one_off_subaddress(const cryptonote::subaddress_index& index)
{
  const crypto::public_key pkey = get_subaddress_spend_public_key(index);
  m_subaddresses[pkey] = index;
}
//----------------------------------------------------------------------------------------------------
std::string wallet2::get_subaddress_label(const cryptonote::subaddress_index& index) const
{
  if (index.major >= m_subaddress_labels.size() || index.minor >= m_subaddress_labels[index.major].size())
  {
    MERROR("Subaddress label doesn't exist");
    return "";
  }
  return m_subaddress_labels[index.major][index.minor];
}
//----------------------------------------------------------------------------------------------------
wallet2::tx_entry_data wallet2::get_tx_entries(const std::unordered_set<crypto::hash> &txids)
{
  tx_entry_data tx_entries;
  tx_entries.tx_entries.reserve(txids.size());

  const size_t SLICE_SIZE =  100; // RESTRICTED_TRANSACTIONS_COUNT as defined in rpc/core_rpc_server.cpp, hardcoded in daemon code
  std::unordered_set<crypto::hash>::const_iterator it = txids.begin();
  for(size_t slice = 0; slice < txids.size(); slice += SLICE_SIZE) {
    cryptonote::COMMAND_RPC_GET_TRANSACTIONS::request req = AUTO_VAL_INIT(req);
    cryptonote::COMMAND_RPC_GET_TRANSACTIONS::response res = AUTO_VAL_INIT(res);
    req.decode_as_json = false;
    req.prune = true;

    size_t ntxes = slice + SLICE_SIZE > txids.size() ? txids.size() - slice : SLICE_SIZE;
    for (size_t i = slice; i < slice + ntxes; ++i)
    {
      req.txs_hashes.push_back(epee::string_tools::pod_to_hex(*it));
      ++it;
    }

    {
      const boost::lock_guard<boost::recursive_mutex> lock{m_daemon_rpc_mutex};
      bool r = epee::net_utils::invoke_http_json("/gettransactions", req, res, *m_http_client, rpc_timeout);
      THROW_WALLET_EXCEPTION_IF(!r, error::wallet_internal_error, "Failed to get transaction from daemon");
      THROW_WALLET_EXCEPTION_IF(res.txs.size() != req.txs_hashes.size(), error::wallet_internal_error, "Failed to get transaction from daemon");
    }

    for (auto& tx_info : res.txs)
    {
      if (!tx_info.in_pool)
      {
        tx_entries.lowest_height = std::min(tx_info.block_height, tx_entries.lowest_height);
        tx_entries.highest_height = std::max(tx_info.block_height, tx_entries.highest_height);
      }

      cryptonote::transaction tx;
      crypto::hash tx_hash;
      THROW_WALLET_EXCEPTION_IF(!get_pruned_tx(tx_info, tx, tx_hash), error::wallet_internal_error, "Failed to get transaction from daemon");
      tx_entries.tx_entries.emplace_back(process_tx_entry_t{ std::move(tx_info), std::move(tx), std::move(tx_hash) });
    }
  }

  return tx_entries;
}
//----------------------------------------------------------------------------------------------------
void wallet2::sort_scan_tx_entries(std::vector<process_tx_entry_t> &unsorted_tx_entries)
{
  // If any txs we're scanning have the same height, then we need to request the
  // blocks those txs are in to see what order they appear in the chain. We
  // need to scan txs in the same order they appear in the chain so that the
  // `m_transfers` container holds entries in a consistently sorted order.
  // This ensures that hot wallets <> cold wallets both maintain the same order
  // of m_transfers, which they rely on when importing/exporting. Same goes
  // for multisig wallets when they synchronize.
  std::set<uint64_t> entry_heights;
  std::set<uint64_t> entry_heights_requested;
  COMMAND_RPC_GET_BLOCKS_BY_HEIGHT::request req;
  COMMAND_RPC_GET_BLOCKS_BY_HEIGHT::response res;
  for (const auto & tx_info : unsorted_tx_entries)
  {
    if (!tx_info.tx_entry.in_pool && !cryptonote::is_coinbase(tx_info.tx))
    {
      const uint64_t height = tx_info.tx_entry.block_height;
      if (entry_heights.find(height) == entry_heights.end())
      {
        entry_heights.insert(height);
      }
      else if (entry_heights_requested.find(height) == entry_heights_requested.end())
      {
        req.heights.push_back(height);
        entry_heights_requested.insert(height);
      }
    }
  }

  {
    const boost::lock_guard<boost::recursive_mutex> lock{m_daemon_rpc_mutex};
    bool r = net_utils::invoke_http_bin("/getblocks_by_height.bin", req, res, *m_http_client, rpc_timeout);
    THROW_WALLET_EXCEPTION_IF(!r, error::wallet_internal_error, "Failed to get blocks by height from daemon");
    THROW_WALLET_EXCEPTION_IF(res.blocks.size() != req.heights.size(), error::wallet_internal_error, "Failed to get blocks by height from daemon");
  }

  std::unordered_map<uint64_t, cryptonote::block> parsed_blocks;
  for (size_t i = 0; i < res.blocks.size(); ++i)
  {
    const auto &blk = res.blocks[i];
    cryptonote::block parsed_block;
    THROW_WALLET_EXCEPTION_IF(!cryptonote::parse_and_validate_block_from_blob(blk.block, parsed_block),
        error::wallet_internal_error, "Failed to parse block");
    parsed_blocks[req.heights[i]] = std::move(parsed_block);
  }

  // sort tx_entries in chronologically ascending order; pool txs to the back
  auto cmp_tx_entry = [&](const process_tx_entry_t& l, const process_tx_entry_t& r)
  {
    if (l.tx_entry.in_pool)
      return false;
    else if (r.tx_entry.in_pool)
      return true;
    else if (l.tx_entry.block_height > r.tx_entry.block_height)
      return false;
    else if (l.tx_entry.block_height < r.tx_entry.block_height)
      return true;
    else // l.tx_entry.block_height == r.tx_entry.block_height
    {
      // coinbase tx is the first tx in a block
      if (cryptonote::is_coinbase(r.tx))
        return false;
      if (cryptonote::is_coinbase(l.tx))
        return true;

      // in case std::sort is comparing elem to itself
      if (l.tx_hash == r.tx_hash)
        return false;

      // see which tx hash comes first in the block
      THROW_WALLET_EXCEPTION_IF(parsed_blocks.find(l.tx_entry.block_height) == parsed_blocks.end(),
          error::wallet_internal_error, std::string("Expected block not returned by daemon, ") +
          "left tx: " + string_tools::pod_to_hex(l.tx_hash) + ", right tx: " + string_tools::pod_to_hex(r.tx_hash));
      const auto &blk = parsed_blocks[l.tx_entry.block_height];
      for (const auto &tx_hash : blk.tx_hashes)
      {
        if (tx_hash == r.tx_hash)
          return false;
        if (tx_hash == l.tx_hash)
          return true;
      }
      THROW_WALLET_EXCEPTION(error::wallet_internal_error, "Tx hashes not found in block");
      return false;
    }
  };
  std::sort(unsorted_tx_entries.begin(), unsorted_tx_entries.end(), cmp_tx_entry);
}
//----------------------------------------------------------------------------------------------------
void wallet2::process_scan_txs(const tx_entry_data &txs_to_scan, const tx_entry_data &txs_to_reprocess, const std::unordered_set<crypto::hash> &tx_hashes_to_reprocess, detached_blockchain_data &dbd)
{
  LOG_PRINT_L0("Processing " << txs_to_scan.tx_entries.size() << " txs, re-processing "
      << txs_to_reprocess.tx_entries.size() << " txs");

  // Sort the txs in chronologically ascending order they appear in the chain
  std::vector<process_tx_entry_t> process_txs;
  process_txs.reserve(txs_to_scan.tx_entries.size() + txs_to_reprocess.tx_entries.size());
  process_txs.insert(process_txs.end(), txs_to_scan.tx_entries.begin(), txs_to_scan.tx_entries.end());
  process_txs.insert(process_txs.end(), txs_to_reprocess.tx_entries.begin(), txs_to_reprocess.tx_entries.end());
  sort_scan_tx_entries(process_txs);

  for (const auto &tx_info : process_txs)
  {
    const auto &tx_entry = tx_info.tx_entry;

    // Ignore callbacks when re-processing a tx to avoid confusing feedback to user
    bool ignore_callbacks = tx_hashes_to_reprocess.find(tx_info.tx_hash) != tx_hashes_to_reprocess.end();
    process_new_transaction(
      tx_info.tx_hash,
      tx_info.tx,
      tx_entry.output_indices,
      tx_entry.block_height,
      0,
      tx_entry.block_timestamp,
      cryptonote::is_coinbase(tx_info.tx),
      tx_entry.in_pool,
      tx_entry.double_spend_seen,
      {}, {}, // unused caches
      ignore_callbacks);

    // Re-set destination addresses if they were previously set
    if (m_confirmed_txs.find(tx_info.tx_hash) != m_confirmed_txs.end() &&
        dbd.detached_confirmed_txs_dests.find(tx_info.tx_hash) != dbd.detached_confirmed_txs_dests.end())
    {
      m_confirmed_txs[tx_info.tx_hash].m_dests = std::move(dbd.detached_confirmed_txs_dests[tx_info.tx_hash]);
    }
  }

  LOG_PRINT_L0("Done processing " << txs_to_scan.tx_entries.size() << " txs and re-processing "
      << txs_to_reprocess.tx_entries.size() << " txs");
}
//----------------------------------------------------------------------------------------------------
void reattach_blockchain(hashchain &blockchain, wallet2::detached_blockchain_data &dbd)
{
  if (!dbd.detached_blockchain.empty())
  {
    LOG_PRINT_L0("Re-attaching " << dbd.detached_blockchain.size() << " blocks");
    for (size_t i = 0; i < dbd.detached_blockchain.size(); ++i)
      blockchain.push_back(dbd.detached_blockchain[i]);
  }

  THROW_WALLET_EXCEPTION_IF(blockchain.size() != dbd.original_chain_size,
    error::wallet_internal_error, "Unexpected blockchain size after re-attaching");
}
//----------------------------------------------------------------------------------------------------
bool has_nonrequested_tx_at_height_or_above_requested(uint64_t height, const std::unordered_set<crypto::hash> &requested_txids, const wallet2::transfer_container &transfers,
    const wallet2::payment_container &payments, const std::unordered_map<crypto::hash, wallet2::confirmed_transfer_details> &confirmed_txs)
{
  for (const auto &td : transfers)
    if (td.m_block_height >= height && requested_txids.find(td.m_txid) == requested_txids.end())
      return true;

  for (const auto &pmt : payments)
    if (pmt.second.m_block_height >= height && requested_txids.find(pmt.second.m_tx_hash) == requested_txids.end())
      return true;

  for (const auto &ct : confirmed_txs)
    if (ct.second.m_block_height >= height && requested_txids.find(ct.first) == requested_txids.end())
      return true;

  return false;
}
//----------------------------------------------------------------------------------------------------
void wallet2::scan_tx(const std::unordered_set<crypto::hash> &txids)
{
  THROW_WALLET_EXCEPTION_IF(m_background_syncing || m_is_background_wallet, error::wallet_internal_error,
    "cannot scan tx from background wallet");

  // Get the transactions from daemon in batches sorted lowest height to highest
  tx_entry_data txs_to_scan = get_tx_entries(txids);
  if (txs_to_scan.tx_entries.empty())
    return;

  // Re-process wallet's txs >= lowest scan_tx height. Re-processing ensures
  // process_new_transaction is called with txs in chronological order. Say that
  // tx2 spends an output from tx1, and the user calls scan_tx(tx1) *after* tx2
  // has already been scanned. In this case, we will "re-process" tx2 *after*
  // processing tx1 to ensure the wallet picks up that tx2 spends the output
  // from tx1, and to ensure transfers are placed in the sorted transfers
  // container in chronological order. Note: in the above example, if tx2 is
  // a sweep to a different wallet's address, the wallet will not be able to
  // detect tx2. The wallet would need to scan tx1 first in that case.
  // TODO: handle this sweep case
  detached_blockchain_data dbd;
  dbd.original_chain_size = m_blockchain.size();
  if (txs_to_scan.highest_height > 0)
  {
    // When connected to an untrusted daemon, if we will need to re-process 1+
    // tx that the user did not request to scan, then we fail out because
    // re-requesting those unexpected txs from the daemon poses a more severe
    // and unintuitive privacy risk to the user
    THROW_WALLET_EXCEPTION_IF(!is_trusted_daemon() &&
      has_nonrequested_tx_at_height_or_above_requested(txs_to_scan.lowest_height, txids, m_transfers, m_payments, m_confirmed_txs),
      error::wont_reprocess_recent_txs_via_untrusted_daemon
    );

    LOG_PRINT_L0("Re-processing wallet's existing txs (if any) starting from height " << txs_to_scan.lowest_height);
    dbd = detach_blockchain(txs_to_scan.lowest_height);
  }
  std::unordered_set<crypto::hash> tx_hashes_to_reprocess;
  tx_hashes_to_reprocess.reserve(dbd.detached_tx_hashes.size());
  for (const auto &tx_hash : dbd.detached_tx_hashes)
  {
    if (txids.find(tx_hash) == txids.end())
      tx_hashes_to_reprocess.insert(tx_hash);
  }
  // re-request txs from daemon to re-process with all tx data needed
  tx_entry_data txs_to_reprocess = get_tx_entries(tx_hashes_to_reprocess);

  process_scan_txs(txs_to_scan, txs_to_reprocess, tx_hashes_to_reprocess, dbd);
  reattach_blockchain(m_blockchain, dbd);

  // If the highest scan_tx height exceeds the wallet's known scan height, then
  // the wallet should skip ahead to the scan_tx's height in order to service
  // the request in a timely manner. Skipping unrequested transactions avoids
  // generating sequences of calls to process_new_transaction which process
  // transactions out-of-order, relative to their order in the blockchain, as
  // the process_new_transaction implementation requires transactions to be
  // processed in blockchain order. If a user misses a tx, they should either
  // use rescan_bc, or manually scan missed txs with scan_tx.
  uint64_t skip_to_height = txs_to_scan.highest_height + 1;
  if (skip_to_height > m_blockchain.size())
  {
    m_skip_to_height = skip_to_height;
    LOG_PRINT_L0("Next refresh will skip to height " << skip_to_height);

    // update last block reward here because the refresh loop won't necessarily set it
    try
    {
      cryptonote::block_header_response block_header;
      if (m_node_rpc_proxy.get_block_header_by_height(txs_to_scan.highest_height, block_header))
        throw std::runtime_error("Failed to request block header by height");
      m_last_block_reward = block_header.reward;
    }
    catch (...) { MERROR("Failed getting block header at height " << txs_to_scan.highest_height); }

    // The wallet's blockchain state will now sync from the expected height correctly on next refresh loop
  }
}
//----------------------------------------------------------------------------------------------------
void wallet2::set_subaddress_label(const cryptonote::subaddress_index& index, const std::string &label)
{
  THROW_WALLET_EXCEPTION_IF(index.major >= m_subaddress_labels.size(), error::account_index_outofbound);
  THROW_WALLET_EXCEPTION_IF(index.minor >= m_subaddress_labels[index.major].size(), error::address_index_outofbound);
  m_subaddress_labels[index.major][index.minor] = label;
}
//----------------------------------------------------------------------------------------------------
void wallet2::set_subaddress_lookahead(size_t major, size_t minor)
{
  THROW_WALLET_EXCEPTION_IF(major == 0, error::wallet_internal_error, "Subaddress major lookahead may not be zero");
  THROW_WALLET_EXCEPTION_IF(major > 0xffffffff, error::wallet_internal_error, "Subaddress major lookahead is too large");
  THROW_WALLET_EXCEPTION_IF(minor == 0, error::wallet_internal_error, "Subaddress minor lookahead may not be zero");
  THROW_WALLET_EXCEPTION_IF(minor > 0xffffffff, error::wallet_internal_error, "Subaddress minor lookahead is too large");

  const uint32_t old_major_lookahead = m_subaddress_lookahead_major;
  const uint32_t old_minor_lookahead = m_subaddress_lookahead_minor;

  m_subaddress_lookahead_major = major;
  m_subaddress_lookahead_minor = minor;

  if (old_major_lookahead >= major && old_minor_lookahead >= minor)
    return;

  expand_subaddresses({0, 0});
}
//----------------------------------------------------------------------------------------------------
/*!
 * \brief Tells if the wallet file is deprecated.
 */
bool wallet2::is_deprecated() const
{
  return is_old_file_format;
}
//----------------------------------------------------------------------------------------------------
void wallet2::set_spent(size_t idx, uint64_t height)
{
  CHECK_AND_ASSERT_THROW_MES(idx < m_transfers.size(), "Invalid index");
  transfer_details &td = m_transfers[idx];
  LOG_PRINT_L2("Setting SPENT at " << height << ": ki " << td.m_key_image << ", amount " << print_money(td.m_amount));
  td.m_spent = true;
  td.m_spent_height = height;
}
//----------------------------------------------------------------------------------------------------
void wallet2::set_unspent(size_t idx)
{
  CHECK_AND_ASSERT_THROW_MES(idx < m_transfers.size(), "Invalid index");
  transfer_details &td = m_transfers[idx];
  LOG_PRINT_L2("Setting UNSPENT: ki " << td.m_key_image << ", amount " << print_money(td.m_amount));
  td.m_spent = false;
  td.m_spent_height = 0;
}
//----------------------------------------------------------------------------------------------------
bool wallet2::is_spent(const transfer_details &td, bool strict) const
{
  if (strict)
  {
    return td.m_spent && td.m_spent_height > 0;
  }
  else
  {
    return td.m_spent;
  }
}
//----------------------------------------------------------------------------------------------------
bool wallet2::is_spent(size_t idx, bool strict) const
{
  CHECK_AND_ASSERT_THROW_MES(idx < m_transfers.size(), "Invalid index");
  const transfer_details &td = m_transfers[idx];
  return is_spent(td, strict);
}
//----------------------------------------------------------------------------------------------------
void wallet2::freeze(size_t idx)
{
  CHECK_AND_ASSERT_THROW_MES(idx < m_transfers.size(), "Invalid transfer_details index");
  transfer_details &td = m_transfers[idx];
  td.m_frozen = true;
}
//----------------------------------------------------------------------------------------------------
void wallet2::thaw(size_t idx)
{
  CHECK_AND_ASSERT_THROW_MES(idx < m_transfers.size(), "Invalid transfer_details index");
  transfer_details &td = m_transfers[idx];
  td.m_frozen = false;
}
//----------------------------------------------------------------------------------------------------
bool wallet2::frozen(size_t idx) const
{
  CHECK_AND_ASSERT_THROW_MES(idx < m_transfers.size(), "Invalid transfer_details index");
  const transfer_details &td = m_transfers[idx];
  return td.m_frozen;
}
//----------------------------------------------------------------------------------------------------
bool wallet2::frozen(const multisig_tx_set& txs) const
{
  // Each call to frozen(const key_image&) is O(N), so if we didn't use batching like we did here,
  // this op would be O(M * N) instead of O(M + N). N = # wallet transfers, M = # key images in set.
  // Step 1. Collect all key images from all pending txs into set
  std::unordered_set<crypto::key_image> kis_to_sign;
  for (const auto& ptx : txs.m_ptx)
  {
    const tools::wallet2::tx_construction_data& cd = ptx.construction_data;
    CHECK_AND_ASSERT_THROW_MES(cd.sources.size() == ptx.tx.vin.size(), "mismatched multisg tx set source sizes");
    for (size_t src_idx = 0; src_idx < cd.sources.size(); ++src_idx)
    {
      // Extract keys images from tx vin and construction data
      const crypto::key_image multisig_ki = rct::rct2ki(cd.sources[src_idx].multisig_kLRki.ki);
      CHECK_AND_ASSERT_THROW_MES(ptx.tx.vin[src_idx].type() == typeid(cryptonote::txin_to_key), "multisig tx cannot be miner");
      const crypto::key_image& vin_ki = boost::get<cryptonote::txin_to_key>(ptx.tx.vin[src_idx]).k_image;

      // Add key images to set (there will be some overlap)
      kis_to_sign.insert(multisig_ki);
      kis_to_sign.insert(vin_ki);
    }
  }
  // Step 2. Scan all transfers for frozen key images
  for (const auto& td : m_transfers)
    if (td.m_frozen && kis_to_sign.count(td.m_key_image))
      return true;

  return false;
}
//----------------------------------------------------------------------------------------------------
void wallet2::freeze(const crypto::key_image &ki)
{
  freeze(get_transfer_details(ki));
}
//----------------------------------------------------------------------------------------------------
void wallet2::thaw(const crypto::key_image &ki)
{
  thaw(get_transfer_details(ki));
}
//----------------------------------------------------------------------------------------------------
bool wallet2::frozen(const crypto::key_image &ki) const
{
  return frozen(get_transfer_details(ki));
}
//----------------------------------------------------------------------------------------------------
size_t wallet2::get_transfer_details(const crypto::key_image &ki) const
{
  for (size_t idx = 0; idx < m_transfers.size(); ++idx)
  {
    const transfer_details &td = m_transfers[idx];
    if (td.m_key_image == ki)
    {
      if (td.m_key_image_known)
        return idx;
      else if (td.m_key_image_partial)
        CHECK_AND_ASSERT_THROW_MES(false, "Transfer detail lookups are not allowed for multisig partial key images");
    }
  }
  CHECK_AND_ASSERT_THROW_MES(false, "Key image not found");
}
//----------------------------------------------------------------------------------------------------
bool wallet2::frozen(const transfer_details &td) const
{
  return td.m_frozen;
}
//----------------------------------------------------------------------------------------------------
void wallet2::check_acc_out_precomp(const tx_out &o, const crypto::key_derivation &derivation, const std::vector<crypto::key_derivation> &additional_derivations, size_t i, tx_scan_info_t &tx_scan_info) const
{
  hw::device &hwdev = m_account.get_device();
  boost::unique_lock<hw::device> hwdev_lock (hwdev);
  hwdev.set_mode(hw::device::TRANSACTION_PARSE);
  crypto::public_key output_public_key;
  if (!get_output_public_key(o, output_public_key))
  {
     tx_scan_info.error = true;
     LOG_ERROR("wrong type id in transaction out");
     return;
  }
  tx_scan_info.received = is_out_to_acc_precomp(m_subaddresses, output_public_key, derivation, additional_derivations, i, hwdev, get_output_view_tag(o));
  if(tx_scan_info.received)
  {
    tx_scan_info.money_transfered = o.amount; // may be 0 for ringct outputs
  }
  else
  {
    tx_scan_info.money_transfered = 0;
  }
  tx_scan_info.error = false;
}
//----------------------------------------------------------------------------------------------------
void wallet2::check_acc_out_precomp(const tx_out &o, const crypto::key_derivation &derivation, const std::vector<crypto::key_derivation> &additional_derivations, size_t i, const is_out_data *is_out_data, tx_scan_info_t &tx_scan_info) const
{
  if (!is_out_data || i >= is_out_data->received.size())
    return check_acc_out_precomp(o, derivation, additional_derivations, i, tx_scan_info);

  tx_scan_info.received = is_out_data->received[i];
  if(tx_scan_info.received)
  {
    tx_scan_info.money_transfered = o.amount; // may be 0 for ringct outputs
  }
  else
  {
    tx_scan_info.money_transfered = 0;
  }
  tx_scan_info.error = false;
}
//----------------------------------------------------------------------------------------------------
void wallet2::check_acc_out_precomp_once(const tx_out &o, const crypto::key_derivation &derivation, const std::vector<crypto::key_derivation> &additional_derivations, size_t i, const is_out_data *is_out_data, tx_scan_info_t &tx_scan_info, bool &already_seen) const
{
  tx_scan_info.received = boost::none;
  if (already_seen)
    return;
  check_acc_out_precomp(o, derivation, additional_derivations, i, is_out_data, tx_scan_info);
  if (tx_scan_info.received)
    already_seen = true;
}
//----------------------------------------------------------------------------------------------------
static uint64_t decodeRct(const rct::rctSig & rv, const crypto::key_derivation &derivation, unsigned int i, rct::key & mask, hw::device &hwdev)
{
  crypto::secret_key scalar1;
  hwdev.derivation_to_scalar(derivation, i, scalar1);
  try
  {
    switch (rv.type)
    {
    case rct::RCTTypeSimple:
    case rct::RCTTypeBulletproof:
    case rct::RCTTypeBulletproof2:
    case rct::RCTTypeCLSAG:
    case rct::RCTTypeBulletproofPlus:
      return rct::decodeRctSimple(rv, rct::sk2rct(scalar1), i, mask, hwdev);
    case rct::RCTTypeFull:
      return rct::decodeRct(rv, rct::sk2rct(scalar1), i, mask, hwdev);
    default:
      LOG_ERROR("Unsupported rct type: " << rv.type);
      return 0;
    }
  }
  catch (const std::exception &e)
  {
    LOG_ERROR("Failed to decode input " << i);
    return 0;
  }
}
//----------------------------------------------------------------------------------------------------
void wallet2::scan_output(const cryptonote::transaction &tx, bool miner_tx, const crypto::public_key &tx_pub_key, size_t i, tx_scan_info_t &tx_scan_info, int &num_vouts_received, std::unordered_map<cryptonote::subaddress_index, uint64_t> &tx_money_got_in_outs, std::vector<size_t> &outs, bool pool)
{
  THROW_WALLET_EXCEPTION_IF(i >= tx.vout.size(), error::wallet_internal_error, "Invalid vout index");

  // if keys are encrypted, ask for password
  if (m_ask_password == AskPasswordToDecrypt && !m_unattended && !m_watch_only && m_multisig_rescan_k.empty() && !m_background_syncing)
  {
    static critical_section password_lock;
    CRITICAL_REGION_LOCAL(password_lock);
    if (!m_encrypt_keys_after_refresh && !m_processing_background_cache)
    {
      boost::optional<epee::wipeable_string> pwd = m_callback->on_get_password(pool ? "output found in pool" : "output received");
      THROW_WALLET_EXCEPTION_IF(!pwd, error::password_needed, tr("Password is needed to compute key image for incoming monero"));
      THROW_WALLET_EXCEPTION_IF(!verify_password(*pwd), error::password_needed, tr("Invalid password: password is needed to compute key image for incoming monero"));
      m_encrypt_keys_after_refresh.reset(new wallet_keys_unlocker(*this, &*pwd));
    }
  }

  crypto::public_key output_public_key;
  THROW_WALLET_EXCEPTION_IF(!get_output_public_key(tx.vout[i], output_public_key), error::wallet_internal_error, "Failed to get output public key");

  if (m_multisig || m_background_syncing/*no spend key*/)
  {
    tx_scan_info.in_ephemeral.pub = output_public_key;
    tx_scan_info.in_ephemeral.sec = crypto::null_skey;
    tx_scan_info.ki = rct::rct2ki(rct::zero());
  }
  else
  {
    bool r = cryptonote::generate_key_image_helper_precomp(m_account.get_keys(), output_public_key, tx_scan_info.received->derivation, i, tx_scan_info.received->index, tx_scan_info.in_ephemeral, tx_scan_info.ki, m_account.get_device());
    THROW_WALLET_EXCEPTION_IF(!r, error::wallet_internal_error, "Failed to generate key image");
    THROW_WALLET_EXCEPTION_IF(tx_scan_info.in_ephemeral.pub != output_public_key,
        error::wallet_internal_error, "key_image generated ephemeral public key not matched with output_key");
  }

  THROW_WALLET_EXCEPTION_IF(std::find(outs.begin(), outs.end(), i) != outs.end(), error::wallet_internal_error, "Same output cannot be added twice");
  if (tx_scan_info.money_transfered == 0 && !miner_tx)
  {
    tx_scan_info.money_transfered = tools::decodeRct(tx.rct_signatures, tx_scan_info.received->derivation, i, tx_scan_info.mask, m_account.get_device());
  }
  if (tx_scan_info.money_transfered == 0)
  {
    MERROR("Invalid output amount, skipping");
    tx_scan_info.error = true;
    return;
  }
  outs.push_back(i);
  THROW_WALLET_EXCEPTION_IF(tx_money_got_in_outs[tx_scan_info.received->index] >= std::numeric_limits<uint64_t>::max() - tx_scan_info.money_transfered,
      error::wallet_internal_error, "Overflow in received amounts");
  tx_money_got_in_outs[tx_scan_info.received->index] += tx_scan_info.money_transfered;
  tx_scan_info.amount = tx_scan_info.money_transfered;
  ++num_vouts_received;
}
//----------------------------------------------------------------------------------------------------
void wallet2::cache_tx_data(const cryptonote::transaction& tx, const crypto::hash &txid, tx_cache_data &tx_cache_data) const
{
  if(!parse_tx_extra(tx.extra, tx_cache_data.tx_extra_fields))
  {
    // Extra may only be partially parsed, it's OK if tx_extra_fields contains public key
    LOG_PRINT_L0("Transaction extra has unsupported format: " << txid);
    if (tx_cache_data.tx_extra_fields.empty())
      return;
  }

  // Don't try to extract tx public key if tx has no ouputs
  const bool is_miner = tx.vin.size() == 1 && tx.vin[0].type() == typeid(cryptonote::txin_gen);
  if (!is_miner || m_refresh_type != RefreshType::RefreshNoCoinbase)
  {
    const size_t rec_size = (is_miner && m_refresh_type == RefreshType::RefreshOptimizeCoinbase && tx.version < 2) ? 1 : tx.vout.size();
    if (!tx.vout.empty())
    {
      // if tx.vout is not empty, we loop through all tx pubkeys
      const std::vector<boost::optional<cryptonote::subaddress_receive_info>> rec(rec_size, boost::none);

      tx_extra_pub_key pub_key_field;
      size_t pk_index = 0;
      while (find_tx_extra_field_by_type(tx_cache_data.tx_extra_fields, pub_key_field, pk_index++))
        tx_cache_data.primary.push_back({pub_key_field.pub_key, {}, rec});

      // additional tx pubkeys and derivations for multi-destination transfers involving one or more subaddresses
      tx_extra_additional_pub_keys additional_tx_pub_keys;
      if (find_tx_extra_field_by_type(tx_cache_data.tx_extra_fields, additional_tx_pub_keys))
      {
        for (size_t i = 0; i < additional_tx_pub_keys.data.size(); ++i)
          tx_cache_data.additional.push_back({additional_tx_pub_keys.data[i], {}, {}});
      }
    }
  }
}
//----------------------------------------------------------------------------------------------------
bool wallet2::spends_one_of_ours(const cryptonote::transaction &tx) const
{
  for (const auto &in: tx.vin)
  {
    if (in.type() != typeid(cryptonote::txin_to_key))
      continue;
    const cryptonote::txin_to_key &in_to_key = boost::get<cryptonote::txin_to_key>(in);
    auto it = m_key_images.find(in_to_key.k_image);
    if (it != m_key_images.end())
      return true;
  }
  return false;
}
//----------------------------------------------------------------------------------------------------
void wallet2::process_new_transaction(const crypto::hash &txid, const cryptonote::transaction& tx, const std::vector<uint64_t> &o_indices, uint64_t height, uint8_t block_version, uint64_t ts, bool miner_tx, bool pool, bool double_spend_seen, const tx_cache_data &tx_cache_data, std::map<std::pair<uint64_t, uint64_t>, size_t> *output_tracker_cache, bool ignore_callbacks)
{
  PERF_TIMER(process_new_transaction);
  // In this function, tx (probably) only contains the base information
  // (that is, the prunable stuff may or may not be included)
  if (!miner_tx && !pool)
    process_unconfirmed(txid, tx, height);

  // per receiving subaddress index
  std::unordered_map<cryptonote::subaddress_index, uint64_t> tx_money_got_in_outs;
  std::unordered_map<cryptonote::subaddress_index, amounts_container> tx_amounts_individual_outs;

  crypto::public_key tx_pub_key = null_pkey;
  bool notify = false;

  std::vector<tx_extra_field> local_tx_extra_fields;
  if (tx_cache_data.tx_extra_fields.empty())
  {
    if(!parse_tx_extra(tx.extra, local_tx_extra_fields))
    {
      // Extra may only be partially parsed, it's OK if tx_extra_fields contains public key
      LOG_PRINT_L0("Transaction extra has unsupported format: " << txid);
    }
  }
  const std::vector<tx_extra_field> &tx_extra_fields = tx_cache_data.tx_extra_fields.empty() ? local_tx_extra_fields : tx_cache_data.tx_extra_fields;

  // Don't try to extract tx public key if tx has no ouputs
  size_t pk_index = 0;
  std::vector<tx_scan_info_t> tx_scan_info(tx.vout.size());
  std::deque<bool> output_found(tx.vout.size(), false);
  uint64_t total_received_1 = 0;
  while (!tx.vout.empty())
  {
    std::vector<size_t> outs;
    // if tx.vout is not empty, we loop through all tx pubkeys

    tx_extra_pub_key pub_key_field;
    if(!find_tx_extra_field_by_type(tx_extra_fields, pub_key_field, pk_index++))
    {
      if (pk_index > 1)
        break;
      LOG_PRINT_L0("Public key wasn't found in the transaction extra. Skipping transaction " << txid);
      if(!ignore_callbacks && 0 != m_callback)
	m_callback->on_skip_transaction(height, txid, tx);
      break;
    }
    if (!tx_cache_data.primary.empty())
    {
      THROW_WALLET_EXCEPTION_IF(tx_cache_data.primary.size() < pk_index || pub_key_field.pub_key != tx_cache_data.primary[pk_index - 1].pkey,
          error::wallet_internal_error, "tx_cache_data is out of sync");
    }

    int num_vouts_received = 0;
    tx_pub_key = pub_key_field.pub_key;
    const cryptonote::account_keys& keys = m_account.get_keys();
    crypto::key_derivation derivation;

    std::vector<crypto::key_derivation> additional_derivations;
    tx_extra_additional_pub_keys additional_tx_pub_keys;
    const wallet2::is_out_data *is_out_data_ptr = NULL;
    if (tx_cache_data.primary.empty())
    {
      hw::device &hwdev = m_account.get_device();
      boost::unique_lock<hw::device> hwdev_lock (hwdev);
      hw::reset_mode rst(hwdev);

      hwdev.set_mode(hw::device::TRANSACTION_PARSE);
      if (!hwdev.generate_key_derivation(tx_pub_key, keys.m_view_secret_key, derivation))
      {
        MWARNING("Failed to generate key derivation from tx pubkey in " << txid << ", skipping");
        static_assert(sizeof(derivation) == sizeof(rct::key), "Mismatched sizes of key_derivation and rct::key");
        memcpy(&derivation, rct::identity().bytes, sizeof(derivation));
      }

      if (pk_index == 1)
      {
        // additional tx pubkeys and derivations for multi-destination transfers involving one or more subaddresses
        if (find_tx_extra_field_by_type(tx_extra_fields, additional_tx_pub_keys))
        {
          for (size_t i = 0; i < additional_tx_pub_keys.data.size(); ++i)
          {
            additional_derivations.push_back({});
            if (!hwdev.generate_key_derivation(additional_tx_pub_keys.data[i], keys.m_view_secret_key, additional_derivations.back()))
            {
              MWARNING("Failed to generate key derivation from additional tx pubkey in " << txid << ", skipping");
              memcpy(&additional_derivations.back(), rct::identity().bytes, sizeof(crypto::key_derivation));
            }
          }
        }
      }
    }
    else
    {
      THROW_WALLET_EXCEPTION_IF(pk_index - 1 >= tx_cache_data.primary.size(),
          error::wallet_internal_error, "pk_index out of range of tx_cache_data");
      is_out_data_ptr = &tx_cache_data.primary[pk_index - 1];
      derivation = tx_cache_data.primary[pk_index - 1].derivation;
      if (pk_index == 1)
      {
        for (size_t n = 0; n < tx_cache_data.additional.size(); ++n)
        {
          additional_tx_pub_keys.data.push_back(tx_cache_data.additional[n].pkey);
          additional_derivations.push_back(tx_cache_data.additional[n].derivation);
        }
      }
    }

    if (miner_tx && m_refresh_type == RefreshNoCoinbase)
    {
      // assume coinbase isn't for us
    }
    else if (miner_tx && m_refresh_type == RefreshOptimizeCoinbase && tx.version < 2)
    {
      check_acc_out_precomp_once(tx.vout[0], derivation, additional_derivations, 0, is_out_data_ptr, tx_scan_info[0], output_found[0]);
      THROW_WALLET_EXCEPTION_IF(tx_scan_info[0].error, error::acc_outs_lookup_error, tx, tx_pub_key, m_account.get_keys());

      // this assumes that the miner tx pays a single address
      if (tx_scan_info[0].received)
      {
        // process the other outs from that tx
        // the first one was already checked
        for (size_t i = 1; i < tx.vout.size(); ++i)
        {
          check_acc_out_precomp_once(tx.vout[i], derivation, additional_derivations, i, is_out_data_ptr, tx_scan_info[i], output_found[i]);
        }
        // then scan all outputs from 0
        hw::device &hwdev = m_account.get_device();
        boost::unique_lock<hw::device> hwdev_lock (hwdev);
        hwdev.set_mode(hw::device::NONE);
        for (size_t i = 0; i < tx.vout.size(); ++i)
        {
          THROW_WALLET_EXCEPTION_IF(tx_scan_info[i].error, error::acc_outs_lookup_error, tx, tx_pub_key, m_account.get_keys());
          if (tx_scan_info[i].received)
          {
            hwdev.conceal_derivation(tx_scan_info[i].received->derivation, tx_pub_key, additional_tx_pub_keys.data, derivation, additional_derivations);
            scan_output(tx, miner_tx, tx_pub_key, i, tx_scan_info[i], num_vouts_received, tx_money_got_in_outs, outs, pool);
            if (!tx_scan_info[i].error)
            {
              tx_amounts_individual_outs[tx_scan_info[i].received->index].push_back(tx_scan_info[i].money_transfered);
            }
          }
        }
      }
    }
    else
    {
      for (size_t i = 0; i < tx.vout.size(); ++i)
      {
        check_acc_out_precomp_once(tx.vout[i], derivation, additional_derivations, i, is_out_data_ptr, tx_scan_info[i], output_found[i]);
        THROW_WALLET_EXCEPTION_IF(tx_scan_info[i].error, error::acc_outs_lookup_error, tx, tx_pub_key, m_account.get_keys());
        if (tx_scan_info[i].received)
        {
          hw::device &hwdev = m_account.get_device();
          boost::unique_lock<hw::device> hwdev_lock (hwdev);
          hwdev.set_mode(hw::device::NONE);
          hwdev.conceal_derivation(tx_scan_info[i].received->derivation, tx_pub_key, additional_tx_pub_keys.data, derivation, additional_derivations);
          scan_output(tx, miner_tx, tx_pub_key, i, tx_scan_info[i], num_vouts_received, tx_money_got_in_outs, outs, pool);
          if (!tx_scan_info[i].error)
          {
            tx_amounts_individual_outs[tx_scan_info[i].received->index].push_back(tx_scan_info[i].money_transfered);
          }
        }
      }
    }

    if(!outs.empty() && num_vouts_received > 0)
    {
      //good news - got money! take care about it
      //usually we have only one transfer for user in transaction
      if (!pool)
      {
        THROW_WALLET_EXCEPTION_IF(tx.vout.size() != o_indices.size(), error::wallet_internal_error,
            "transactions outputs size=" + std::to_string(tx.vout.size()) +
            " not match with daemon response size=" + std::to_string(o_indices.size()));

        // we're going to re-process this receive when background sync is disabled
        if (m_background_syncing && m_background_sync_data.txs.find(txid) == m_background_sync_data.txs.end())
        {
          size_t bgs_idx = m_background_sync_data.txs.size();
          background_synced_tx_t bgs_tx = {
            .index_in_background_sync_data = bgs_idx,
            .tx                            = tx,
            .output_indices                = o_indices,
            .height                        = height,
            .block_timestamp               = ts,
            .double_spend_seen             = double_spend_seen
          };
          LOG_PRINT_L2("Adding received tx " << txid << " to background sync data (idx=" << bgs_idx << ")");
          m_background_sync_data.txs.insert({txid, std::move(bgs_tx)});
        }
      }

      for(size_t o: outs)
      {
	THROW_WALLET_EXCEPTION_IF(tx.vout.size() <= o, error::wallet_internal_error, "wrong out in transaction: internal index=" +
				  std::to_string(o) + ", total_outs=" + std::to_string(tx.vout.size()));

        auto kit = m_pub_keys.find(tx_scan_info[o].in_ephemeral.pub);
	THROW_WALLET_EXCEPTION_IF(kit != m_pub_keys.end() && kit->second >= m_transfers.size(),
            error::wallet_internal_error, std::string("Unexpected transfer index from public key: ")
            + "got " + (kit == m_pub_keys.end() ? "<none>" : boost::lexical_cast<std::string>(kit->second))
            + ", m_transfers.size() is " + boost::lexical_cast<std::string>(m_transfers.size()));
        if (kit == m_pub_keys.end())
        {
          uint64_t amount = tx.vout[o].amount ? tx.vout[o].amount : tx_scan_info[o].amount;
          if (!pool)
          {
	    m_transfers.push_back(transfer_details{});
	    transfer_details& td = m_transfers.back();
	    td.m_block_height = height;
	    td.m_internal_output_index = o;
	    td.m_global_output_index = o_indices[o];
	    td.m_tx = (const cryptonote::transaction_prefix&)tx;
	    td.m_txid = txid;
            td.m_key_image = tx_scan_info[o].ki;
            td.m_key_image_known = !m_watch_only && !m_multisig && !m_background_syncing;
            if (!td.m_key_image_known)
            {
              // we might have cold signed, and have a mapping to key images
              std::unordered_map<crypto::public_key, crypto::key_image>::const_iterator i = m_cold_key_images.find(tx_scan_info[o].in_ephemeral.pub);
              if (i != m_cold_key_images.end())
              {
                td.m_key_image = i->second;
                td.m_key_image_known = true;
              }
            }
            if (m_watch_only)
            {
              // for view wallets, that flag means "we want to request it"
              td.m_key_image_request = true;
            }
            else
            {
              td.m_key_image_request = false;
            }
            td.m_key_image_partial = m_multisig;
            td.m_amount = amount;
            td.m_pk_index = pk_index - 1;
            td.m_subaddr_index = tx_scan_info[o].received->index;
            if (should_expand(tx_scan_info[o].received->index))
              expand_subaddresses(tx_scan_info[o].received->index);
            if (tx.vout[o].amount == 0)
            {
              td.m_mask = tx_scan_info[o].mask;
              td.m_rct = true;
            }
            else if (miner_tx && tx.version == 2)
            {
              td.m_mask = rct::identity();
              td.m_rct = true;
            }
            else
            {
              td.m_mask = rct::identity();
              td.m_rct = false;
            }
            td.m_frozen = false;
	    set_unspent(m_transfers.size()-1);
            if (td.m_key_image_known)
	      m_key_images[td.m_key_image] = m_transfers.size()-1;
	    m_pub_keys[tx_scan_info[o].in_ephemeral.pub] = m_transfers.size()-1;
            if (output_tracker_cache)
              (*output_tracker_cache)[std::make_pair(tx.vout[o].amount, td.m_global_output_index)] = m_transfers.size() - 1;
            if (m_multisig)
            {
              THROW_WALLET_EXCEPTION_IF(m_multisig_rescan_k.empty() && !m_multisig_rescan_info.empty(),
                  error::wallet_internal_error, "NULL m_multisig_rescan_k");
              if (!m_multisig_rescan_info.empty() && m_multisig_rescan_info.front().size() >= m_transfers.size())
                update_multisig_rescan_info(m_multisig_rescan_k, m_multisig_rescan_info, m_transfers.size() - 1);
            }
	    LOG_PRINT_L0("Received money: " << print_money(td.amount()) << ", with tx: " << txid);
	    if (!ignore_callbacks && 0 != m_callback)
	      m_callback->on_money_received(height, txid, tx, td.m_amount, 0, td.m_subaddr_index, spends_one_of_ours(tx), td.m_tx.unlock_time);
          }
          total_received_1 += amount;
          notify = true;
        }
	else if (m_transfers[kit->second].m_spent || m_transfers[kit->second].amount() >= tx_scan_info[o].amount)
        {
	  LOG_ERROR("Public key " << epee::string_tools::pod_to_hex(kit->first)
              << " from received " << print_money(tx_scan_info[o].amount) << " output already exists with "
              << (m_transfers[kit->second].m_spent ? "spent" : "unspent") << " "
              << print_money(m_transfers[kit->second].amount()) << " in tx " << m_transfers[kit->second].m_txid << ", received output ignored");
          THROW_WALLET_EXCEPTION_IF(tx_money_got_in_outs[tx_scan_info[o].received->index] < tx_scan_info[o].amount,
              error::wallet_internal_error, "Unexpected values of new and old outputs");
          tx_money_got_in_outs[tx_scan_info[o].received->index] -= tx_scan_info[o].amount;

          amounts_container& tx_amounts_this_out = tx_amounts_individual_outs[tx_scan_info[o].received->index]; // Only for readability on the following lines
          auto amount_iterator = std::find(tx_amounts_this_out.begin(), tx_amounts_this_out.end(), tx_scan_info[o].amount);
          THROW_WALLET_EXCEPTION_IF(amount_iterator == tx_amounts_this_out.end(),
              error::wallet_internal_error, "Unexpected values of new and old outputs");
          tx_amounts_this_out.erase(amount_iterator);
        }
        else
        {
	  LOG_ERROR("Public key " << epee::string_tools::pod_to_hex(kit->first)
              << " from received " << print_money(tx_scan_info[o].amount) << " output already exists with "
              << print_money(m_transfers[kit->second].amount()) << ", replacing with new output");
          // The new larger output replaced a previous smaller one
          THROW_WALLET_EXCEPTION_IF(tx_money_got_in_outs[tx_scan_info[o].received->index] < tx_scan_info[o].amount,
              error::wallet_internal_error, "Unexpected values of new and old outputs");
          THROW_WALLET_EXCEPTION_IF(m_transfers[kit->second].amount() > tx_scan_info[o].amount,
              error::wallet_internal_error, "Unexpected values of new and old outputs");
          tx_money_got_in_outs[tx_scan_info[o].received->index] -= m_transfers[kit->second].amount();

          uint64_t amount = tx.vout[o].amount ? tx.vout[o].amount : tx_scan_info[o].amount;
          uint64_t burnt = m_transfers[kit->second].amount();
          uint64_t extra_amount = amount - burnt;
          if (!pool)
          {
            transfer_details &td = m_transfers[kit->second];
	    td.m_block_height = height;
	    td.m_internal_output_index = o;
	    td.m_global_output_index = o_indices[o];
	    td.m_tx = (const cryptonote::transaction_prefix&)tx;
	    td.m_txid = txid;
            td.m_amount = amount;
            td.m_pk_index = pk_index - 1;
            td.m_subaddr_index = tx_scan_info[o].received->index;
            if (should_expand(tx_scan_info[o].received->index))
              expand_subaddresses(tx_scan_info[o].received->index);
            if (tx.vout[o].amount == 0)
            {
              td.m_mask = tx_scan_info[o].mask;
              td.m_rct = true;
            }
            else if (miner_tx && tx.version == 2)
            {
              td.m_mask = rct::identity();
              td.m_rct = true;
            }
            else
            {
              td.m_mask = rct::identity();
              td.m_rct = false;
            }
            if (output_tracker_cache)
              (*output_tracker_cache)[std::make_pair(tx.vout[o].amount, td.m_global_output_index)] = kit->second;
            if (m_multisig)
            {
              THROW_WALLET_EXCEPTION_IF(m_multisig_rescan_k.empty() && !m_multisig_rescan_info.empty(),
                  error::wallet_internal_error, "NULL m_multisig_rescan_k");
              if (!m_multisig_rescan_info.empty() && m_multisig_rescan_info.front().size() >= m_transfers.size())
                update_multisig_rescan_info(m_multisig_rescan_k, m_multisig_rescan_info, m_transfers.size() - 1);
            }
            THROW_WALLET_EXCEPTION_IF(td.get_public_key() != tx_scan_info[o].in_ephemeral.pub, error::wallet_internal_error, "Inconsistent public keys");
	    THROW_WALLET_EXCEPTION_IF(td.m_spent, error::wallet_internal_error, "Inconsistent spent status");

	    LOG_PRINT_L0("Received money: " << print_money(td.amount()) << ", with tx: " << txid);
	    if (!ignore_callbacks && 0 != m_callback)
	      m_callback->on_money_received(height, txid, tx, td.m_amount, burnt, td.m_subaddr_index, spends_one_of_ours(tx), td.m_tx.unlock_time);
          }
          total_received_1 += extra_amount;
          notify = true;
        }
      }
    }
  }

  THROW_WALLET_EXCEPTION_IF(tx_money_got_in_outs.size() != tx_amounts_individual_outs.size(), error::wallet_internal_error, "Inconsistent size of output arrays");

  uint64_t tx_money_spent_in_ins = 0;
  // The line below is equivalent to "boost::optional<uint32_t> subaddr_account;", but avoids the GCC warning: ‘*((void*)& subaddr_account +4)’ may be used uninitialized in this function
  // It's a GCC bug with boost::optional, see https://gcc.gnu.org/bugzilla/show_bug.cgi?id=47679
  auto subaddr_account ([]()->boost::optional<uint32_t> {return boost::none;}());
  std::set<uint32_t> subaddr_indices;
  // check all outputs for spending (compare key images)
  for(auto& in: tx.vin)
  {
    if(in.type() != typeid(cryptonote::txin_to_key))
      continue;
    const cryptonote::txin_to_key &in_to_key = boost::get<cryptonote::txin_to_key>(in);
    auto it = m_key_images.find(in_to_key.k_image);
    if(it != m_key_images.end())
    {
      transfer_details& td = m_transfers[it->second];
      uint64_t amount = in_to_key.amount;
      if (amount > 0)
      {
        if(amount != td.amount())
        {
          MERROR("Inconsistent amount in tx input: got " << print_money(amount) <<
            ", expected " << print_money(td.amount()));
          // this means:
          //   1) the same output pub key was used as destination multiple times,
          //   2) the wallet set the highest amount among them to transfer_details::m_amount, and
          //   3) the wallet somehow spent that output with an amount smaller than the above amount, causing inconsistency
          td.m_amount = amount;
        }
      }
      else
      {
        amount = td.amount();
      }
      tx_money_spent_in_ins += amount;
      if (subaddr_account && *subaddr_account != td.m_subaddr_index.major)
        LOG_ERROR("spent funds are from different subaddress accounts; count of incoming/outgoing payments will be incorrect");
      subaddr_account = td.m_subaddr_index.major;
      subaddr_indices.insert(td.m_subaddr_index.minor);
      LOG_PRINT_L0("Spent money: " << print_money(amount) << ", with tx: " << txid);
      set_spent(it->second, height);
      if (!pool)
      {
        if (!ignore_callbacks && 0 != m_callback)
          m_callback->on_money_spent(height, txid, tx, amount, tx, td.m_subaddr_index);

        if (m_background_syncing && m_background_sync_data.txs.find(txid) == m_background_sync_data.txs.end())
        {
          size_t bgs_idx = m_background_sync_data.txs.size();
          background_synced_tx_t bgs_tx = {
            .index_in_background_sync_data = bgs_idx,
            .tx                            = tx,
            .output_indices                = o_indices,
            .height                        = height,
            .block_timestamp               = ts,
            .double_spend_seen             = double_spend_seen
          };
          LOG_PRINT_L2("Adding spent tx " << txid << " to background sync data (idx=" << bgs_idx << ")");
          m_background_sync_data.txs.insert({txid, std::move(bgs_tx)});
        }
      }
    }

    if (!pool && (m_track_uses || (m_background_syncing && it == m_key_images.end())))
    {
      PERF_TIMER(track_uses);
      const uint64_t amount = in_to_key.amount;
      std::vector<uint64_t> offsets = cryptonote::relative_output_offsets_to_absolute(in_to_key.key_offsets);
      if (output_tracker_cache)
      {
        for (uint64_t offset: offsets)
        {
          const std::map<std::pair<uint64_t, uint64_t>, size_t>::const_iterator i = output_tracker_cache->find(std::make_pair(amount, offset));
          if (i != output_tracker_cache->end())
          {
            size_t idx = i->second;
            THROW_WALLET_EXCEPTION_IF(idx >= m_transfers.size(), error::wallet_internal_error, "Output tracker cache index out of range");

            if (m_track_uses)
              m_transfers[idx].m_uses.push_back(std::make_pair(height, txid));

            // We'll re-process all txs which *might* be spends when we disable
            // background sync and retrieve the spend key. We don't know if an
            // output is a spend in this tx if we don't know its key image.
            if (m_background_syncing && !m_transfers[idx].m_key_image_known && m_background_sync_data.txs.find(txid) == m_background_sync_data.txs.end())
            {
              size_t bgs_idx = m_background_sync_data.txs.size();
              background_synced_tx_t bgs_tx = {
                .index_in_background_sync_data = bgs_idx,
                .tx                            = tx,
                .output_indices                = o_indices,
                .height                        = height,
                .block_timestamp               = ts,
                .double_spend_seen             = double_spend_seen
              };
              LOG_PRINT_L2("Adding plausible spent tx " << txid << " to background sync data (idx=" << bgs_idx << ")");
              m_background_sync_data.txs.insert({txid, std::move(bgs_tx)});
            }
          }
        }
      }
      else for (transfer_details &td: m_transfers)
      {
        if (amount != in_to_key.amount)
          continue;
        for (uint64_t offset: offsets)
          if (offset == td.m_global_output_index)
          {
            if (m_track_uses)
              td.m_uses.push_back(std::make_pair(height, txid));
            if (m_background_syncing && !td.m_key_image_known && m_background_sync_data.txs.find(txid) == m_background_sync_data.txs.end())
            {
              size_t bgs_idx = m_background_sync_data.txs.size();
              background_synced_tx_t bgs_tx = {
                .index_in_background_sync_data = bgs_idx,
                .tx                            = tx,
                .output_indices                = o_indices,
                .height                        = height,
                .block_timestamp               = ts,
                .double_spend_seen             = double_spend_seen
              };
              LOG_PRINT_L2("Adding plausible spent tx " << txid << " to background sync data (idx=" << bgs_idx << ")");
              m_background_sync_data.txs.insert({txid, std::move(bgs_tx)});
            }
          }
      }
    }
  }

  uint64_t fee = miner_tx ? 0 : tx.version == 1 ? tx_money_spent_in_ins - get_outs_money_amount(tx) : tx.rct_signatures.txnFee;

  if (tx_money_spent_in_ins > 0)
  {
    uint64_t self_received = std::accumulate<decltype(tx_money_got_in_outs.begin()), uint64_t>(tx_money_got_in_outs.begin(), tx_money_got_in_outs.end(), 0,
      [&subaddr_account] (uint64_t acc, const std::pair<cryptonote::subaddress_index, uint64_t>& p)
      {
        return acc + (p.first.major == *subaddr_account ? p.second : 0);
      });
    if (!pool)
    {
      process_outgoing(txid, tx, height, ts, tx_money_spent_in_ins, self_received, *subaddr_account, subaddr_indices);
      // if sending to yourself at the same subaddress account, set the outgoing payment amount to 0 so that it's less confusing
      if (tx_money_spent_in_ins == self_received + fee)
      {
        auto i = m_confirmed_txs.find(txid);
        THROW_WALLET_EXCEPTION_IF(i == m_confirmed_txs.end(), error::wallet_internal_error,
          "confirmed tx wasn't found: " + string_tools::pod_to_hex(txid));
        i->second.m_change = self_received;
      }
    }
    else if (!m_unconfirmed_txs.count(txid))
    {
      // Add to unconfirmed txs if not already there (e.g. restoring wallet, or running the wallet in parallel to the sending wallet w/same seed)
      add_unconfirmed_tx(txid, tx, tx_money_spent_in_ins, {}/*don't know dests*/, crypto::null_hash/*don't know payment_id*/, self_received, *subaddr_account, subaddr_indices);
      auto i = m_unconfirmed_txs.find(txid);
      THROW_WALLET_EXCEPTION_IF(i == m_unconfirmed_txs.end(), error::wallet_internal_error,
        "unconfirmed tx wasn't found: " + string_tools::pod_to_hex(txid));
      i->second.m_amount_out = get_outgoing_amount(tx, tx_money_spent_in_ins);
    }
  }

  // remove change sent to the spending subaddress account from the list of received funds
  uint64_t sub_change = 0;
  for (auto i = tx_money_got_in_outs.begin(); i != tx_money_got_in_outs.end();)
  {
    if (subaddr_account && i->first.major == *subaddr_account)
    {
      sub_change += i->second;
      tx_amounts_individual_outs.erase(i->first);
      i = tx_money_got_in_outs.erase(i);
    }
    else
      ++i;
  }

  // create payment_details for each incoming transfer to a subaddress index
  if (tx_money_got_in_outs.size() > 0)
  {
    tx_extra_nonce extra_nonce;
    crypto::hash payment_id = null_hash;
    if (find_tx_extra_field_by_type(tx_extra_fields, extra_nonce))
    {
      crypto::hash8 payment_id8 = null_hash8;
      if(get_encrypted_payment_id_from_tx_extra_nonce(extra_nonce.nonce, payment_id8))
      {
        // We got a payment ID to go with this tx
        LOG_PRINT_L2("Found encrypted payment ID: " << payment_id8);
        MINFO("Consider using subaddresses instead of encrypted payment IDs");
        if (tx_pub_key != null_pkey)
        {
          if (!m_account.get_device().decrypt_payment_id(payment_id8, tx_pub_key, m_account.get_keys().m_view_secret_key))
          {
            LOG_PRINT_L0("Failed to decrypt payment ID: " << payment_id8);
          }
          else
          {
            LOG_PRINT_L2("Decrypted payment ID: " << payment_id8);
            // put the 64 bit decrypted payment id in the first 8 bytes
            memcpy(payment_id.data, payment_id8.data, 8);
            // rest is already 0, but guard against code changes above
            memset(payment_id.data + 8, 0, 24);
          }
        }
        else
        {
          LOG_PRINT_L1("No public key found in tx, unable to decrypt payment id");
        }
      }
      else if (get_payment_id_from_tx_extra_nonce(extra_nonce.nonce, payment_id))
      {
        bool ignore = block_version >= IGNORE_LONG_PAYMENT_ID_FROM_BLOCK_VERSION;
        if (ignore)
        {
          LOG_PRINT_L2("Found unencrypted payment ID in tx " << txid << " (ignored)");
          MWARNING("Found OBSOLETE AND IGNORED unencrypted payment ID: these are bad for privacy, use subaddresses instead");
          payment_id = crypto::null_hash;
        }
        else
        {
          LOG_PRINT_L2("Found unencrypted payment ID: " << payment_id);
          MWARNING("Found unencrypted payment ID: these are bad for privacy, consider using subaddresses instead");
        }
      }
    }

    uint64_t total_received_2 = sub_change;
    for (const auto& i : tx_money_got_in_outs)
      total_received_2 += i.second;
    if (total_received_1 != total_received_2)
    {
      const el::Level level = el::Level::Warning;
      MCLOG_RED(level, "global", "**********************************************************************");
      MCLOG_RED(level, "global", "Consistency failure in amounts received");
      MCLOG_RED(level, "global", "Check transaction " << txid);
      MCLOG_RED(level, "global", "**********************************************************************");
      exit(1);
      return;
    }

    bool all_same = true;
    for (const auto& i : tx_money_got_in_outs)
    {
      payment_details payment;
      payment.m_tx_hash      = txid;
      payment.m_fee          = fee;
      payment.m_amount       = i.second;
      payment.m_amounts      = tx_amounts_individual_outs[i.first];
      payment.m_block_height = height;
      payment.m_unlock_time  = tx.unlock_time;
      payment.m_timestamp    = ts;
      payment.m_coinbase     = miner_tx;
      payment.m_subaddr_index = i.first;
      if (pool) {
        if (emplace_or_replace(m_unconfirmed_payments, payment_id, pool_payment_details{payment, double_spend_seen}))
          all_same = false;
        if (0 != m_callback)
          m_callback->on_unconfirmed_money_received(height, txid, tx, payment.m_amount, payment.m_subaddr_index);
      }
      else
        m_payments.emplace(payment_id, payment);
      LOG_PRINT_L2("Payment found in " << (pool ? "pool" : "block") << ": " << payment_id << " / " << payment.m_tx_hash << " / " << payment.m_amount);
    }

    // if it's a pool tx and we already had it, don't notify again
    if (pool && all_same)
      notify = false;
  }

  if (notify)
  {
    std::shared_ptr<tools::Notify> tx_notify = m_tx_notify;
    if (tx_notify)
      tx_notify->notify("%s", epee::string_tools::pod_to_hex(txid).c_str(), NULL);
  }
}
//----------------------------------------------------------------------------------------------------
void wallet2::process_unconfirmed(const crypto::hash &txid, const cryptonote::transaction& tx, uint64_t height)
{
  if (m_unconfirmed_txs.empty())
    return;

  auto unconf_it = m_unconfirmed_txs.find(txid);
  if(unconf_it != m_unconfirmed_txs.end()) {
    if (store_tx_info()) {
      try {
        m_confirmed_txs.insert(std::make_pair(txid, confirmed_transfer_details(unconf_it->second, height)));
      }
      catch (...) {
        // can fail if the tx has unexpected input types
        LOG_PRINT_L0("Failed to add outgoing transaction to confirmed transaction map");
      }
    }
    m_unconfirmed_txs.erase(unconf_it);
  }
}
//----------------------------------------------------------------------------------------------------
void wallet2::process_outgoing(const crypto::hash &txid, const cryptonote::transaction &tx, uint64_t height, uint64_t ts, uint64_t spent, uint64_t received, uint32_t subaddr_account, const std::set<uint32_t>& subaddr_indices)
{
  std::pair<std::unordered_map<crypto::hash, confirmed_transfer_details>::iterator, bool> entry = m_confirmed_txs.insert(std::make_pair(txid, confirmed_transfer_details()));
  // fill with the info we know, some info might already be there
  if (entry.second)
  {
    // this case will happen if the tx is from our outputs, but was sent by another
    // wallet (eg, we're a cold wallet and the hot wallet sent it). For RCT transactions,
    // we only see 0 input amounts, so have to deduce amount out from other parameters.
    entry.first->second.m_amount_in = spent;
    entry.first->second.m_amount_out = get_outgoing_amount(tx, spent);
    entry.first->second.m_change = received;

    std::vector<tx_extra_field> tx_extra_fields;
    parse_tx_extra(tx.extra, tx_extra_fields); // ok if partially parsed
    tx_extra_nonce extra_nonce;
    if (find_tx_extra_field_by_type(tx_extra_fields, extra_nonce))
    {
      // we do not care about failure here
      get_payment_id_from_tx_extra_nonce(extra_nonce.nonce, entry.first->second.m_payment_id);
    }
    entry.first->second.m_subaddr_account = subaddr_account;
    entry.first->second.m_subaddr_indices = subaddr_indices;
  }

  entry.first->second.m_rings.clear();
  for (const auto &in: tx.vin)
  {
    if (in.type() != typeid(cryptonote::txin_to_key))
      continue;
    const auto &txin = boost::get<cryptonote::txin_to_key>(in);
    entry.first->second.m_rings.push_back(std::make_pair(txin.k_image, txin.key_offsets));
  }
  entry.first->second.m_block_height = height;
  entry.first->second.m_timestamp = ts;
  entry.first->second.m_unlock_time = tx.unlock_time;

  add_rings(tx);
}
//----------------------------------------------------------------------------------------------------
bool wallet2::should_skip_block(const cryptonote::block &b, uint64_t height) const
{
  // seeking only for blocks that are not older then the wallet creation time plus 1 day. 1 day is for possible user incorrect time setup
  return !(b.timestamp + 60*60*24 > m_account.get_createtime() && height >= m_refresh_from_block_height && height >= m_skip_to_height);
}
//----------------------------------------------------------------------------------------------------
void wallet2::process_new_blockchain_entry(const cryptonote::block& b, const cryptonote::block_complete_entry& bche, const parsed_block &parsed_block, const crypto::hash& bl_id, uint64_t height, const std::vector<tx_cache_data> &tx_cache_data, size_t tx_cache_data_offset, std::map<std::pair<uint64_t, uint64_t>, size_t> *output_tracker_cache)
{
  THROW_WALLET_EXCEPTION_IF(bche.txs.size() + 1 != parsed_block.o_indices.indices.size(), error::wallet_internal_error,
      "block transactions=" + std::to_string(bche.txs.size()) +
      " not match with daemon response size=" + std::to_string(parsed_block.o_indices.indices.size()));

  THROW_WALLET_EXCEPTION_IF(height != m_blockchain.size(), error::wallet_internal_error,
      "New blockchain entry mismatch: block height " + std::to_string(height) +
      " is not the expected next height " + std::to_string(m_blockchain.size()));

  //handle transactions from new block
    
  //optimization: seeking only for blocks that are not older then the wallet creation time plus 1 day. 1 day is for possible user incorrect time setup
  if (!should_skip_block(b, height))
  {
    TIME_MEASURE_START(miner_tx_handle_time);
    if (m_refresh_type != RefreshNoCoinbase)
      process_new_transaction(get_transaction_hash(b.miner_tx), b.miner_tx, parsed_block.o_indices.indices[0].indices, height, b.major_version, b.timestamp, true, false, false, tx_cache_data[tx_cache_data_offset], output_tracker_cache);
    ++tx_cache_data_offset;
    TIME_MEASURE_FINISH(miner_tx_handle_time);

    TIME_MEASURE_START(txs_handle_time);
    THROW_WALLET_EXCEPTION_IF(bche.txs.size() != b.tx_hashes.size(), error::wallet_internal_error, "Wrong amount of transactions for block");
    THROW_WALLET_EXCEPTION_IF(bche.txs.size() != parsed_block.txes.size(), error::wallet_internal_error, "Wrong amount of transactions for block");
    for (size_t idx = 0; idx < b.tx_hashes.size(); ++idx)
    {
      process_new_transaction(b.tx_hashes[idx], parsed_block.txes[idx], parsed_block.o_indices.indices[idx+1].indices, height, b.major_version, b.timestamp, false, false, false, tx_cache_data[tx_cache_data_offset++], output_tracker_cache);
    }
    TIME_MEASURE_FINISH(txs_handle_time);
    m_last_block_reward = cryptonote::get_outs_money_amount(b.miner_tx);
    LOG_PRINT_L2("Processed block: " << bl_id << ", height " << height << ", " <<  miner_tx_handle_time + txs_handle_time << "(" << miner_tx_handle_time << "/" << txs_handle_time <<")ms");
  }else
  {
    if (!(height % 128))
      LOG_PRINT_L2( "Skipped block by timestamp, height: " << height << ", block time " << b.timestamp << ", account time " << m_account.get_createtime());
  }
  m_blockchain.push_back(bl_id);

  if (0 != m_callback)
    m_callback->on_new_block(height, b);
}
//----------------------------------------------------------------------------------------------------
void wallet2::get_short_chain_history(std::list<crypto::hash>& ids, uint64_t granularity) const
{
  size_t i = 0;
  size_t current_multiplier = 1;
  size_t blockchain_size = std::max((size_t)(m_blockchain.size() / granularity * granularity), m_blockchain.offset());
  size_t sz = blockchain_size - m_blockchain.offset();
  if(!sz)
  {
    if(m_blockchain.size() > m_blockchain.offset())
      ids.push_back(m_blockchain[m_blockchain.offset()]);
    ids.push_back(m_blockchain.genesis());
    return;
  }
  size_t current_back_offset = 1;
  bool base_included = false;
  while(current_back_offset < sz)
  {
    ids.push_back(m_blockchain[m_blockchain.offset() + sz-current_back_offset]);
    if(sz-current_back_offset == 0)
      base_included = true;
    if(i < 10)
    {
      ++current_back_offset;
    }else
    {
      current_back_offset += current_multiplier *= 2;
    }
    ++i;
  }
  if(!base_included)
    ids.push_back(m_blockchain[m_blockchain.offset()]);
  if(m_blockchain.offset())
    ids.push_back(m_blockchain.genesis());
}
//----------------------------------------------------------------------------------------------------
void wallet2::parse_block_round(const cryptonote::blobdata &blob, cryptonote::block &bl, crypto::hash &bl_id, bool &error) const
{
  error = !cryptonote::parse_and_validate_block_from_blob(blob, bl, bl_id);
}
//----------------------------------------------------------------------------------------------------
void read_pool_txs(const cryptonote::COMMAND_RPC_GET_TRANSACTIONS::request &req, const cryptonote::COMMAND_RPC_GET_TRANSACTIONS::response &res, bool r, const std::vector<crypto::hash> &txids, std::vector<std::tuple<cryptonote::transaction, crypto::hash, bool>> &txs)
{
  if (r && res.status == CORE_RPC_STATUS_OK)
  {
    MDEBUG("Reading pool txs");
    if (res.txs.size() == req.txs_hashes.size())
    {
      for (const auto &tx_entry: res.txs)
      {
        if (tx_entry.in_pool)
        {
          cryptonote::transaction tx;
          cryptonote::blobdata bd;
          crypto::hash tx_hash;

          if (get_pruned_tx(tx_entry, tx, tx_hash))
          {
            const std::vector<crypto::hash>::const_iterator i = std::find_if(txids.begin(), txids.end(),
                [tx_hash](const crypto::hash &e) { return e == tx_hash; });
            if (i != txids.end())
            {
              txs.emplace_back(std::move(tx), tx_hash, tx_entry.double_spend_seen);
            }
            else
            {
              MERROR("Got txid " << tx_hash << " which we did not ask for");
            }
          }
          else
          {
            LOG_PRINT_L0("Failed to parse transaction from daemon");
          }
        }
        else
        {
          LOG_PRINT_L1("Transaction from daemon was in pool, but is no more");
        }
      }
    }
    else
    {
      LOG_PRINT_L0("Expected " << req.txs_hashes.size() << " out of " << txids.size() << " tx(es), got " << res.txs.size());
    }
  }
}
//----------------------------------------------------------------------------------------------------
void wallet2::process_pool_info_extent(const cryptonote::COMMAND_RPC_GET_BLOCKS_FAST::response &res, std::vector<std::tuple<cryptonote::transaction, crypto::hash, bool>> &process_txs, bool refreshed)
{
  std::vector<std::tuple<cryptonote::transaction, crypto::hash, bool>> added_pool_txs;
  added_pool_txs.reserve(res.added_pool_txs.size() + res.remaining_added_pool_txids.size());

  for (const auto &pool_tx: res.added_pool_txs)
  {
    cryptonote::transaction tx;
    THROW_WALLET_EXCEPTION_IF(!cryptonote::parse_and_validate_tx_base_from_blob(pool_tx.tx_blob, tx),
        error::wallet_internal_error, "Failed to validate transaction base from daemon");
    added_pool_txs.emplace_back(std::move(tx), pool_tx.tx_hash, pool_tx.double_spend_seen);
  }

  // getblocks.bin may return more added pool transactions than we're allowed to request in restricted mode
  if (!res.remaining_added_pool_txids.empty())
  {
    // request the remaining txs
    m_node_rpc_proxy.get_transactions(res.remaining_added_pool_txids,
      [this, &res, &added_pool_txs](const cryptonote::COMMAND_RPC_GET_TRANSACTIONS::request &req_t, const cryptonote::COMMAND_RPC_GET_TRANSACTIONS::response &resp_t, bool r)
      {
        read_pool_txs(req_t, resp_t, r, res.remaining_added_pool_txids, added_pool_txs);
        if (!r || resp_t.status != CORE_RPC_STATUS_OK)
          LOG_PRINT_L0("Error calling gettransactions daemon RPC: r " << r << ", status " << get_rpc_status(m_trusted_daemon, resp_t.status));
      }
    );
  }

  update_pool_state_from_pool_data(res.pool_info_extent == COMMAND_RPC_GET_BLOCKS_FAST::INCREMENTAL, res.removed_pool_txids, added_pool_txs, process_txs, refreshed);
}
//----------------------------------------------------------------------------------------------------
void wallet2::pull_blocks(bool first, bool try_incremental, uint64_t start_height, uint64_t &blocks_start_height, const std::list<crypto::hash> &short_chain_history, std::vector<cryptonote::block_complete_entry> &blocks, std::vector<cryptonote::COMMAND_RPC_GET_BLOCKS_FAST::block_output_indices> &o_indices, uint64_t &current_height, std::vector<std::tuple<cryptonote::transaction, crypto::hash, bool>>& process_pool_txs)
{
  cryptonote::COMMAND_RPC_GET_BLOCKS_FAST::request req = AUTO_VAL_INIT(req);
  cryptonote::COMMAND_RPC_GET_BLOCKS_FAST::response res = AUTO_VAL_INIT(res);
  req.block_ids = short_chain_history;

  MDEBUG("Pulling blocks: start_height " << start_height);

  req.prune = true;
  req.start_height = start_height;
  req.no_miner_tx = m_refresh_type == RefreshNoCoinbase;

  req.requested_info = (first && !m_background_syncing) ? COMMAND_RPC_GET_BLOCKS_FAST::BLOCKS_AND_POOL : COMMAND_RPC_GET_BLOCKS_FAST::BLOCKS_ONLY;
  if (try_incremental && !m_background_syncing)
    req.pool_info_since = m_pool_info_query_time;

  {
    const boost::lock_guard<boost::recursive_mutex> lock{m_daemon_rpc_mutex};
    bool r = net_utils::invoke_http_bin("/getblocks.bin", req, res, *m_http_client, rpc_timeout);
    THROW_ON_RPC_RESPONSE_ERROR(r, {}, res, "getblocks.bin", error::get_blocks_error, get_rpc_status(m_trusted_daemon, res.status));
    THROW_WALLET_EXCEPTION_IF(res.blocks.size() != res.output_indices.size(), error::wallet_internal_error,
        "mismatched blocks (" + boost::lexical_cast<std::string>(res.blocks.size()) + ") and output_indices (" +
        boost::lexical_cast<std::string>(res.output_indices.size()) + ") sizes from daemon");
  }

  blocks_start_height = res.start_height;
  blocks = std::move(res.blocks);
  o_indices = std::move(res.output_indices);
  current_height = res.current_height;
  if (res.pool_info_extent != COMMAND_RPC_GET_BLOCKS_FAST::NONE)
    m_pool_info_query_time = res.daemon_time;

  MDEBUG("Pulled blocks: blocks_start_height " << blocks_start_height << ", count " << blocks.size()
      << ", height " << blocks_start_height + blocks.size() << ", node height " << res.current_height
      << ", pool info " << static_cast<unsigned int>(res.pool_info_extent));

  if (first && !m_background_syncing)
  {
    if (res.pool_info_extent != COMMAND_RPC_GET_BLOCKS_FAST::NONE)
    {
      process_pool_info_extent(res, process_pool_txs, true);
    }
    else
    {
      // If we did not get any pool info, neither incremental nor the whole pool, we probably talk
      // to a daemon that does not yet support giving back pool info with the 'getblocks' call,
      // and we have to update in the "old way"
      update_pool_state_by_pool_query(process_pool_txs, true);
    }
  }

}
//----------------------------------------------------------------------------------------------------
void wallet2::pull_hashes(uint64_t start_height, uint64_t &blocks_start_height, const std::list<crypto::hash> &short_chain_history, std::vector<crypto::hash> &hashes)
{
  cryptonote::COMMAND_RPC_GET_HASHES_FAST::request req = AUTO_VAL_INIT(req);
  cryptonote::COMMAND_RPC_GET_HASHES_FAST::response res = AUTO_VAL_INIT(res);
  req.block_ids = short_chain_history;

  req.start_height = start_height;

  {
    const boost::lock_guard<boost::recursive_mutex> lock{m_daemon_rpc_mutex};
    bool r = net_utils::invoke_http_bin("/gethashes.bin", req, res, *m_http_client, rpc_timeout);
    THROW_ON_RPC_RESPONSE_ERROR(r, {}, res, "gethashes.bin", error::get_hashes_error, get_rpc_status(m_trusted_daemon, res.status));
  }

  blocks_start_height = res.start_height;
  hashes = std::move(res.m_block_ids);
}
//----------------------------------------------------------------------------------------------------
void wallet2::process_parsed_blocks(const uint64_t start_height, const std::vector<cryptonote::block_complete_entry> &blocks, const std::vector<parsed_block> &parsed_blocks, uint64_t& blocks_added, std::map<std::pair<uint64_t, uint64_t>, size_t> *output_tracker_cache)
{
  blocks_added = 0;

  THROW_WALLET_EXCEPTION_IF(blocks.size() != parsed_blocks.size(), error::wallet_internal_error, "size mismatch");
  THROW_WALLET_EXCEPTION_IF(!m_blockchain.is_in_bounds(start_height), error::out_of_hashchain_bounds_error);

  tools::threadpool& tpool = tools::threadpool::getInstanceForCompute();
  tools::threadpool::waiter waiter(tpool);

  size_t num_txes = 0;
  std::vector<tx_cache_data> tx_cache_data;
  for (size_t i = 0; i < blocks.size(); ++i)
    num_txes += 1 + parsed_blocks[i].txes.size();
  tx_cache_data.resize(num_txes);
  size_t txidx = 0;
  crypto::hash prev_block_id;
  bool has_prev_block = m_blockchain.is_in_bounds(start_height - 1);
  if (has_prev_block) {
    prev_block_id = m_blockchain[start_height - 1];
  }
  for (size_t i = 0; i < blocks.size(); ++i)
  {
    if (has_prev_block) {
      THROW_WALLET_EXCEPTION_IF(prev_block_id != parsed_blocks[i].block.prev_id, error::wallet_internal_error,
          "Parent block hash mismatch at height " + std::to_string(start_height + i) +
          ": expected " + string_tools::pod_to_hex(prev_block_id) +
          ", but received a new block with prev_id " + string_tools::pod_to_hex(parsed_blocks[i].block.prev_id));
    }
    prev_block_id = parsed_blocks[i].hash;
    has_prev_block = true;

    THROW_WALLET_EXCEPTION_IF(parsed_blocks[i].txes.size() != parsed_blocks[i].block.tx_hashes.size(),
        error::wallet_internal_error, "Mismatched parsed_blocks[i].txes.size() and parsed_blocks[i].block.tx_hashes.size()");
    if (should_skip_block(parsed_blocks[i].block, start_height + i))
    {
      txidx += 1 + parsed_blocks[i].block.tx_hashes.size();
      continue;
    }
    if (m_refresh_type != RefreshNoCoinbase)
      tpool.submit(&waiter, [&, i, txidx](){ cache_tx_data(parsed_blocks[i].block.miner_tx, get_transaction_hash(parsed_blocks[i].block.miner_tx), tx_cache_data[txidx]); });
    ++txidx;
    for (size_t idx = 0; idx < parsed_blocks[i].txes.size(); ++idx)
    {
      tpool.submit(&waiter, [&, i, idx, txidx](){ cache_tx_data(parsed_blocks[i].txes[idx], parsed_blocks[i].block.tx_hashes[idx], tx_cache_data[txidx]); });
      ++txidx;
    }
  }
  THROW_WALLET_EXCEPTION_IF(txidx != num_txes, error::wallet_internal_error, "txidx does not match tx_cache_data size");
  THROW_WALLET_EXCEPTION_IF(!waiter.wait(), error::wallet_internal_error, "Exception in thread pool");

  hw::device &hwdev =  m_account.get_device();
  hw::reset_mode rst(hwdev);
  hwdev.set_mode(hw::device::TRANSACTION_PARSE);
  const cryptonote::account_keys &keys = m_account.get_keys();

  auto gender = [&](wallet2::is_out_data &iod) {
    if (!hwdev.generate_key_derivation(iod.pkey, keys.m_view_secret_key, iod.derivation))
    {
      MWARNING("Failed to generate key derivation from tx pubkey, skipping");
      static_assert(sizeof(iod.derivation) == sizeof(rct::key), "Mismatched sizes of key_derivation and rct::key");
      memcpy(&iod.derivation, rct::identity().bytes, sizeof(iod.derivation));
    }
  };

  for (size_t i = 0; i < tx_cache_data.size(); ++i)
  {
    if (tx_cache_data[i].empty())
      continue;
    tpool.submit(&waiter, [&gender, &tx_cache_data, i]() {
      auto &slot = tx_cache_data[i];
      for (auto &iod: slot.primary)
        gender(iod);
      for (auto &iod: slot.additional)
        gender(iod);
    }, true);
  }
  THROW_WALLET_EXCEPTION_IF(!waiter.wait(), error::wallet_internal_error, "Exception in thread pool");

  auto geniod = [&](const cryptonote::transaction &tx, size_t n_vouts, size_t txidx) {
    for (size_t k = 0; k < n_vouts; ++k)
    {
      const auto &o = tx.vout[k];
      crypto::public_key output_public_key;
      if (get_output_public_key(o, output_public_key))
      {
        std::vector<crypto::key_derivation> additional_derivations;
        additional_derivations.reserve(tx_cache_data[txidx].additional.size());
        for (const auto &iod: tx_cache_data[txidx].additional)
          additional_derivations.push_back(iod.derivation);
        for (size_t l = 0; l < tx_cache_data[txidx].primary.size(); ++l)
        {
          THROW_WALLET_EXCEPTION_IF(tx_cache_data[txidx].primary[l].received.size() != n_vouts,
              error::wallet_internal_error, "Unexpected received array size");
          tx_cache_data[txidx].primary[l].received[k] = is_out_to_acc_precomp(m_subaddresses, output_public_key, tx_cache_data[txidx].primary[l].derivation, additional_derivations, k, hwdev, get_output_view_tag(o));
          additional_derivations.clear();
        }
      }
    }
  };
  struct geniod_params
  {
    const cryptonote::transaction &tx;
    size_t n_outs;
    size_t txidx;
  };
  std::vector<geniod_params> geniods;
  geniods.reserve(num_txes);

  txidx = 0;
  uint8_t hf_version_view_tags = get_view_tag_fork();
  for (size_t i = 0; i < blocks.size(); ++i)
  {
    if (should_skip_block(parsed_blocks[i].block, start_height + i))
    {
      txidx += 1 + parsed_blocks[i].block.tx_hashes.size();
      continue;
    }

    if (m_refresh_type != RefreshType::RefreshNoCoinbase)
    {
      THROW_WALLET_EXCEPTION_IF(txidx >= tx_cache_data.size(), error::wallet_internal_error, "txidx out of range");
      const cryptonote::transaction& tx = parsed_blocks[i].block.miner_tx;
      const size_t n_vouts = (m_refresh_type == RefreshType::RefreshOptimizeCoinbase && tx.version < 2) ? 1 : tx.vout.size();
      if (parsed_blocks[i].block.major_version >= hf_version_view_tags)
        geniods.push_back(geniod_params{ tx, n_vouts, txidx });
      else
        tpool.submit(&waiter, [&, n_vouts, txidx](){ geniod(tx, n_vouts, txidx); }, true);
    }
    ++txidx;
    for (size_t j = 0; j < parsed_blocks[i].txes.size(); ++j)
    {
      THROW_WALLET_EXCEPTION_IF(txidx >= tx_cache_data.size(), error::wallet_internal_error, "txidx out of range");
      if (parsed_blocks[i].block.major_version >= hf_version_view_tags)
        geniods.push_back(geniod_params{ parsed_blocks[i].txes[j], parsed_blocks[i].txes[j].vout.size(), txidx });
      else
        tpool.submit(&waiter, [&, i, j, txidx](){ geniod(parsed_blocks[i].txes[j], parsed_blocks[i].txes[j].vout.size(), txidx); }, true);
      ++txidx;
    }
  }
  THROW_WALLET_EXCEPTION_IF(txidx != tx_cache_data.size(), error::wallet_internal_error, "txidx did not reach expected value");

  // View tags significantly speed up the geniod function that determines if an output belongs to the account.
  // Because the speedup is so large, the overhead from submitting individual geniods to the thread pool eats into
  // the benefit of executing in parallel. So to maximize the benefit from threads when view tags are enabled,
  // the wallet starts submitting geniod function calls to the thread pool in batches of size GENIOD_BATCH_SIZE.
  if (geniods.size())
  {
    size_t GENIOD_BATCH_SIZE = 100;
    size_t num_batch_txes = 0;
    size_t batch_start = 0;
    while (batch_start < geniods.size())
    {
      size_t batch_end = std::min(batch_start + GENIOD_BATCH_SIZE, geniods.size());
      THROW_WALLET_EXCEPTION_IF(batch_end < batch_start, error::wallet_internal_error, "Thread batch end overflow");
      tpool.submit(&waiter, [&geniods, &geniod, batch_start, batch_end]() {
        for (size_t i = batch_start; i < batch_end; ++i)
        {
          const geniod_params &gp = geniods[i];
          geniod(gp.tx, gp.n_outs, gp.txidx);
        }
      }, true);
      num_batch_txes += batch_end - batch_start;
      batch_start = batch_end;
    }
    THROW_WALLET_EXCEPTION_IF(num_batch_txes != geniods.size(), error::wallet_internal_error, "txes batched for thread pool did not reach expected value");
  }
  THROW_WALLET_EXCEPTION_IF(!waiter.wait(), error::wallet_internal_error, "Exception in thread pool");

  hwdev.set_mode(hw::device::NONE);

  size_t current_index = start_height;
  size_t tx_cache_data_offset = 0;
  for (size_t i = 0; i < blocks.size(); ++i)
  {
    const crypto::hash &bl_id = parsed_blocks[i].hash;
    const cryptonote::block &bl = parsed_blocks[i].block;

    if(current_index >= m_blockchain.size())
    {
      process_new_blockchain_entry(bl, blocks[i], parsed_blocks[i], bl_id, current_index, tx_cache_data, tx_cache_data_offset, output_tracker_cache);
      ++blocks_added;
    }
    else if(bl_id != m_blockchain[current_index])
    {
      //split detected here !!!
      THROW_WALLET_EXCEPTION_IF(current_index == start_height, error::wallet_internal_error,
        "wrong daemon response: split starts from the first block in response " + string_tools::pod_to_hex(bl_id) +
        " (height " + std::to_string(start_height) + "), local block id at this height: " +
        string_tools::pod_to_hex(m_blockchain[current_index]));

      const uint64_t reorg_depth = m_blockchain.size() - current_index;
      THROW_WALLET_EXCEPTION_IF(reorg_depth > m_max_reorg_depth, error::reorg_depth_error,
        tr("reorg exceeds maximum allowed depth, use 'set max-reorg-depth N' to allow it, reorg depth: ") +
        std::to_string(reorg_depth));

      handle_reorg(current_index, output_tracker_cache);
      process_new_blockchain_entry(bl, blocks[i], parsed_blocks[i], bl_id, current_index, tx_cache_data, tx_cache_data_offset, output_tracker_cache);
    }
    else
    {
      LOG_PRINT_L2("Block is already in blockchain: " << string_tools::pod_to_hex(bl_id));
    }
    ++current_index;
    tx_cache_data_offset += 1 + parsed_blocks[i].txes.size();
  }
}
//----------------------------------------------------------------------------------------------------
void wallet2::refresh(bool trusted_daemon)
{
  uint64_t blocks_fetched = 0;
  refresh(trusted_daemon, 0, blocks_fetched);
}
//----------------------------------------------------------------------------------------------------
void wallet2::refresh(bool trusted_daemon, uint64_t start_height, uint64_t & blocks_fetched)
{
  bool received_money = false;
  refresh(trusted_daemon, start_height, blocks_fetched, received_money);
}
//----------------------------------------------------------------------------------------------------
void check_block_hard_fork_version(cryptonote::network_type nettype, uint8_t hf_version, uint64_t height, bool &wallet_is_outdated, bool &daemon_is_outdated)
{
  const size_t wallet_num_hard_forks = nettype == TESTNET ? num_testnet_hard_forks
    : nettype == STAGENET ? num_stagenet_hard_forks : num_mainnet_hard_forks;
  const hardfork_t *wallet_hard_forks = nettype == TESTNET ? testnet_hard_forks
    : nettype == STAGENET ? stagenet_hard_forks : mainnet_hard_forks;

  wallet_is_outdated = hf_version > wallet_hard_forks[wallet_num_hard_forks-1].version;
  if (wallet_is_outdated)
    return;

  // check block's height falls within wallet's expected range for block's given version
  size_t fork_index;
  for (fork_index = 0; fork_index < wallet_num_hard_forks; ++fork_index)
    if (wallet_hard_forks[fork_index].version == hf_version)
      break;
  THROW_WALLET_EXCEPTION_IF(fork_index == wallet_num_hard_forks, error::wallet_internal_error, "Fork not found in table");
  uint64_t start_height = hf_version == 1 ? 0 : wallet_hard_forks[fork_index].height;
  uint64_t end_height = fork_index == wallet_num_hard_forks - 1
    ? std::numeric_limits<uint64_t>::max()
    : wallet_hard_forks[fork_index + 1].height;

  daemon_is_outdated = height < start_height || height >= end_height;
}
//----------------------------------------------------------------------------------------------------
void wallet2::pull_and_parse_next_blocks(bool first, bool try_incremental, uint64_t start_height, uint64_t &blocks_start_height, std::list<crypto::hash> &short_chain_history, const std::vector<cryptonote::block_complete_entry> &prev_blocks, const std::vector<parsed_block> &prev_parsed_blocks, std::vector<cryptonote::block_complete_entry> &blocks, std::vector<parsed_block> &parsed_blocks, std::vector<std::tuple<cryptonote::transaction, crypto::hash, bool>>& process_pool_txs, bool &last, bool &error, std::exception_ptr &exception)
{
  error = false;
  last = false;
  exception = NULL;

  try
  {
    drop_from_short_history(short_chain_history, 3);

    THROW_WALLET_EXCEPTION_IF(prev_blocks.size() != prev_parsed_blocks.size(), error::wallet_internal_error, "size mismatch");

    // prepend the last 3 blocks, should be enough to guard against a block or two's reorg
    auto s = std::next(prev_parsed_blocks.rbegin(), std::min((size_t)3, prev_parsed_blocks.size())).base();
    for (; s != prev_parsed_blocks.end(); ++s)
    {
      short_chain_history.push_front(s->hash);
    }

    // pull the new blocks
    std::vector<cryptonote::COMMAND_RPC_GET_BLOCKS_FAST::block_output_indices> o_indices;
    uint64_t current_height;
    pull_blocks(first, try_incremental, start_height, blocks_start_height, short_chain_history, blocks, o_indices, current_height, process_pool_txs);
    THROW_WALLET_EXCEPTION_IF(blocks.size() != o_indices.size(), error::wallet_internal_error, "Mismatched sizes of blocks and o_indices");

    tools::threadpool& tpool = tools::threadpool::getInstanceForCompute();
    tools::threadpool::waiter waiter(tpool);
    parsed_blocks.resize(blocks.size());
    for (size_t i = 0; i < blocks.size(); ++i)
    {
      tpool.submit(&waiter, boost::bind(&wallet2::parse_block_round, this, std::cref(blocks[i].block),
        std::ref(parsed_blocks[i].block), std::ref(parsed_blocks[i].hash), std::ref(parsed_blocks[i].error)), true);
    }
    THROW_WALLET_EXCEPTION_IF(!waiter.wait(), error::wallet_internal_error, "Exception in thread pool");
    for (size_t i = 0; i < blocks.size(); ++i)
    {
      if (parsed_blocks[i].error)
      {
        error = true;
        break;
      }

      if (!m_allow_mismatched_daemon_version)
      {
        // make sure block's hard fork version is expected at the block's height
        uint8_t hf_version = parsed_blocks[i].block.major_version;
        uint64_t height = blocks_start_height + i;
        bool wallet_is_outdated = false;
        bool daemon_is_outdated = false;
        check_block_hard_fork_version(m_nettype, hf_version, height, wallet_is_outdated, daemon_is_outdated);
        THROW_WALLET_EXCEPTION_IF(wallet_is_outdated || daemon_is_outdated, error::incorrect_fork_version,
          "Unexpected hard fork version v" + std::to_string(hf_version) + " at height " + std::to_string(height) + ". " +
          (wallet_is_outdated
            ? "Make sure your wallet is up to date"
            : "Make sure the node you are connected to is running the latest version")
        );
      }

      parsed_blocks[i].o_indices = std::move(o_indices[i]);
    }

    boost::mutex error_lock;
    for (size_t i = 0; i < blocks.size(); ++i)
    {
      parsed_blocks[i].txes.resize(blocks[i].txs.size());
      for (size_t j = 0; j < blocks[i].txs.size(); ++j)
      {
        tpool.submit(&waiter, [&, i, j](){
          if (!parse_and_validate_tx_base_from_blob(blocks[i].txs[j].blob, parsed_blocks[i].txes[j]))
          {
            boost::unique_lock<boost::mutex> lock(error_lock);
            error = true;
          }
        }, true);
      }
    }
    THROW_WALLET_EXCEPTION_IF(!waiter.wait(), error::wallet_internal_error, "Exception in thread pool");
    last = !blocks.empty() && cryptonote::get_block_height(parsed_blocks.back().block) + 1 == current_height;
  }
  catch(...)
  {
    error = true;
    exception = std::current_exception();
  }
}

void wallet2::remove_obsolete_pool_txs(const std::vector<crypto::hash> &tx_hashes, bool remove_if_found)
{
  // remove pool txes to us that aren't in the pool anymore (remove_if_found = false),
  // or remove pool txes to us that were reported as removed (remove_if_found = true)
  std::unordered_multimap<crypto::hash, wallet2::pool_payment_details>::iterator uit = m_unconfirmed_payments.begin();
  while (uit != m_unconfirmed_payments.end())
  {
    const crypto::hash &txid = uit->second.m_pd.m_tx_hash;
    bool found = false;
    for (const auto &it2: tx_hashes)
    {
      if (it2 == txid)
      {
        found = true;
        break;
      }
    }
    auto pit = uit++;
    if ((!remove_if_found && !found) || (remove_if_found && found))
    {
      MDEBUG("Removing " << txid << " from unconfirmed payments");
      m_unconfirmed_payments.erase(pit);
      if (0 != m_callback)
        m_callback->on_pool_tx_removed(txid);
    }
  }
}

//----------------------------------------------------------------------------------------------------
// Code that is common to 'update_pool_state_by_pool_query' and 'update_pool_state_from_pool_data':
// Check wether a tx in the pool is worthy of processing because we did not see it
// yet or because it is "interesting" out of special circumstances
bool wallet2::accept_pool_tx_for_processing(const crypto::hash &txid)
{
  bool txid_found_in_up = false;
  for (const auto &up: m_unconfirmed_payments)
  {
    if (up.second.m_pd.m_tx_hash == txid)
    {
      txid_found_in_up = true;
      break;
    }
  }
  if (m_scanned_pool_txs[0].find(txid) != m_scanned_pool_txs[0].end() || m_scanned_pool_txs[1].find(txid) != m_scanned_pool_txs[1].end())
  {
    // if it's for us, we want to keep track of whether we saw a double spend, so don't bail out
    if (!txid_found_in_up)
    {
      LOG_PRINT_L2("Already seen " << txid << ", and not for us, skipped");
      return false;
    }
  }
  if (!txid_found_in_up)
  {
    LOG_PRINT_L1("Found new pool tx: " << txid);
    bool found = false;
    for (const auto &i: m_unconfirmed_txs)
    {
      if (i.first == txid)
      {
        found = true;
        // if this is a payment to yourself at a different subaddress account, don't skip it
        // so that you can see the incoming pool tx with 'show_transfers' on that receiving subaddress account
        const unconfirmed_transfer_details& utd = i.second;
        for (const auto& dst : utd.m_dests)
        {
          auto subaddr_index = m_subaddresses.find(dst.addr.m_spend_public_key);
          if (subaddr_index != m_subaddresses.end() && subaddr_index->second.major != utd.m_subaddr_account)
          {
            found = false;
            break;
          }
        }
        break;
      }
    }
    if (!found)
    {
      // not one of those we sent ourselves
      return true;
    }
    else
    {
      LOG_PRINT_L1("We sent that one");
      return false;
    }
  }
  else {
    return false;
  }
}
//----------------------------------------------------------------------------------------------------
// Code that is common to 'update_pool_state_by_pool_query' and 'update_pool_state_from_pool_data':
// Process an unconfirmed transfer after we know whether it's in the pool or not
void wallet2::process_unconfirmed_transfer(bool incremental, const crypto::hash &txid, wallet2::unconfirmed_transfer_details &tx_details, bool seen_in_pool, std::chrono::system_clock::time_point now, bool refreshed)
{
  // TODO: set tx_propagation_timeout to CRYPTONOTE_DANDELIONPP_EMBARGO_AVERAGE * 3 / 2 after v15 hardfork
  constexpr const std::chrono::seconds tx_propagation_timeout{500};
  if (seen_in_pool)
  {
    if (tx_details.m_state != wallet2::unconfirmed_transfer_details::pending_in_pool)
    {
      tx_details.m_state = wallet2::unconfirmed_transfer_details::pending_in_pool;
      MINFO("Pending txid " << txid << " seen in pool, marking as pending in pool");
    }
  }
  else
  {
    if (!incremental)
    {
      if (tx_details.m_state == wallet2::unconfirmed_transfer_details::pending_in_pool)
      {
        // For the probably unlikely case that a tx once seen in the pool vanishes
        // again set back to 'pending'
        tx_details.m_state = wallet2::unconfirmed_transfer_details::pending;
        MINFO("Already seen txid " << txid << " vanished from pool, marking as pending");
      }
    }
    // If a tx is pending for a "long time" without appearing in the pool, and if
    // we have refreshed and thus had a chance to really see it if it was there,
    // judge it as failed; the waiting for timeout and refresh happened avoids
    // false alarms with txs going to 'failed' too early
    if (tx_details.m_state == wallet2::unconfirmed_transfer_details::pending && refreshed &&
      now > std::chrono::system_clock::from_time_t(tx_details.m_sent_time) + tx_propagation_timeout)
    {
      LOG_PRINT_L1("Pending txid " << txid << " not in pool after " << tx_propagation_timeout.count() <<
        " seconds, marking as failed");
      tx_details.m_state = wallet2::unconfirmed_transfer_details::failed;

      // the inputs aren't spent anymore, since the tx failed
      for (size_t vini = 0; vini < tx_details.m_tx.vin.size(); ++vini)
      {
        if (tx_details.m_tx.vin[vini].type() == typeid(txin_to_key))
        {
          txin_to_key &tx_in_to_key = boost::get<txin_to_key>(tx_details.m_tx.vin[vini]);
          for (size_t i = 0; i < m_transfers.size(); ++i)
          {
            const transfer_details &td = m_transfers[i];
            if (td.m_key_image == tx_in_to_key.k_image)
            {
                LOG_PRINT_L1("Resetting spent status for output " << vini << ": " << td.m_key_image);
                set_unspent(i);
                break;
            }
          }
        }
      }
    }
  }
}
//----------------------------------------------------------------------------------------------------
// This public method is typically called to make sure that the wallet's pool state is up-to-date by
// clients like simplewallet and the RPC daemon. Before incremental update this was the same method
// that 'refresh' also used, but now it's more complicated because for the time being we support
// the "old" and the "new" way of updating the pool and because only the 'getblocks' call supports
// incremental update but we don't want any blocks here.
//
// simplewallet does NOT update the pool info during automatic refresh to avoid disturbing interactive
// messages and prompts. When it finally calls this method here "to catch up" so to say we can't use
// incremental update anymore, because with that we might miss some txs altogether.
void wallet2::update_pool_state(std::vector<std::tuple<cryptonote::transaction, crypto::hash, bool>> &process_txs, bool refreshed, bool try_incremental)
{
  process_txs.clear();
  if (m_background_syncing)
    return;
  bool updated = false;
  if (m_pool_info_query_time != 0 && try_incremental)
  {
    // We are connected to a daemon that supports giving back pool data with the 'getblocks' call,
    // thus use that, to get the chance to work incrementally and to keep working incrementally;
    // 'POOL_ONLY' was created to support this case
    cryptonote::COMMAND_RPC_GET_BLOCKS_FAST::request req = AUTO_VAL_INIT(req);
    cryptonote::COMMAND_RPC_GET_BLOCKS_FAST::response res = AUTO_VAL_INIT(res);

    req.requested_info = COMMAND_RPC_GET_BLOCKS_FAST::POOL_ONLY;
    req.pool_info_since = m_pool_info_query_time;
    req.prune = true;

    {
      const boost::lock_guard<boost::recursive_mutex> lock{m_daemon_rpc_mutex};
      bool r = net_utils::invoke_http_bin("/getblocks.bin", req, res, *m_http_client, rpc_timeout);
      THROW_ON_RPC_RESPONSE_ERROR(r, {}, res, "getblocks.bin", error::get_blocks_error, get_rpc_status(m_trusted_daemon, res.status));
    }

    m_pool_info_query_time = res.daemon_time;
    if (res.pool_info_extent != COMMAND_RPC_GET_BLOCKS_FAST::NONE)
    {
      process_pool_info_extent(res, process_txs, refreshed);
      updated = true;
    }
    // We SHOULD get pool data here, but if for some crazy reason we don't fall back to the "old" method
  }
  if (!updated)
  {
    update_pool_state_by_pool_query(process_txs, refreshed);
  }
}
//----------------------------------------------------------------------------------------------------
// This is the "old" way of updating the pool with separate queries to get the pool content, used before
// the 'getblocks' command was able to give back pool data in addition to blocks. Before this code was
// the public 'update_pool_state' method. The logic is unchanged. This is a candidate for elimination
// when it's sure that no more "old" daemons can be possibly around.
void wallet2::update_pool_state_by_pool_query(std::vector<std::tuple<cryptonote::transaction, crypto::hash, bool>> &process_txs, bool refreshed)
{
  MTRACE("update_pool_state_by_pool_query start");
  process_txs.clear();

  auto keys_reencryptor = epee::misc_utils::create_scope_leave_handler([&, this]() {
    m_encrypt_keys_after_refresh.reset();
  });

  // get the pool state
  cryptonote::COMMAND_RPC_GET_TRANSACTION_POOL_HASHES_BIN::request req;
  cryptonote::COMMAND_RPC_GET_TRANSACTION_POOL_HASHES_BIN::response res;

  {
    const boost::lock_guard<boost::recursive_mutex> lock{m_daemon_rpc_mutex};
    bool r = epee::net_utils::invoke_http_json("/get_transaction_pool_hashes.bin", req, res, *m_http_client, rpc_timeout);
    THROW_ON_RPC_RESPONSE_ERROR(r, {}, res, "get_transaction_pool_hashes.bin", error::get_tx_pool_error);
  }
  MTRACE("update_pool_state_by_pool_query got pool");

  // remove any pending tx that's not in the pool
  const auto now = std::chrono::system_clock::now();
  std::unordered_map<crypto::hash, wallet2::unconfirmed_transfer_details>::iterator it = m_unconfirmed_txs.begin();
  while (it != m_unconfirmed_txs.end())
  {
    const crypto::hash &txid = it->first;
    MDEBUG("Checking m_unconfirmed_txs entry " << txid);
    bool found = false;
    for (const auto &it2: res.tx_hashes)
    {
      if (it2 == txid)
      {
        found = true;
        break;
      }
    }
    auto pit = it++;
    process_unconfirmed_transfer(false, txid, pit->second, found, now, refreshed);
    MDEBUG("New state of that entry: " << pit->second.m_state);
  }
  MTRACE("update_pool_state_by_pool_query done first loop");

  // remove pool txes to us that aren't in the pool anymore
  // but only if we just refreshed, so that the tx can go in
  // the in transfers list instead (or nowhere if it just
  // disappeared without being mined)
  if (refreshed)
    remove_obsolete_pool_txs(res.tx_hashes, false);

  MTRACE("update_pool_state_by_pool_query done second loop");

  // gather txids of new pool txes to us
  std::vector<crypto::hash> txids;
  for (const auto &txid: res.tx_hashes)
  {
    if (accept_pool_tx_for_processing(txid))
      txids.push_back(txid);
  }

  m_node_rpc_proxy.get_transactions(txids,
    [this, &txids, &process_txs](const cryptonote::COMMAND_RPC_GET_TRANSACTIONS::request &req_t, const cryptonote::COMMAND_RPC_GET_TRANSACTIONS::response &resp_t, bool r)
    {
      read_pool_txs(req_t, resp_t, r, txids, process_txs);
      if (!r || resp_t.status != CORE_RPC_STATUS_OK)
        LOG_PRINT_L0("Error calling gettransactions daemon RPC: r " << r << ", status " << get_rpc_status(m_trusted_daemon, resp_t.status));
    }
  );

  MTRACE("update_pool_state_by_pool_query end");
}
//----------------------------------------------------------------------------------------------------
// Update pool state from pool data we got together with block data, either incremental data with
// txs that are new in the pool since the last time we queried and the ids of txs that were
// removed from the pool since then, or the whole content of the pool if incremental was not
// possible, e.g. because the server was just started or restarted.
void wallet2::update_pool_state_from_pool_data(bool incremental, const std::vector<crypto::hash> &removed_pool_txids, const std::vector<std::tuple<cryptonote::transaction, crypto::hash, bool>> &added_pool_txs, std::vector<std::tuple<cryptonote::transaction, crypto::hash, bool>> &process_txs, bool refreshed)
{
  MTRACE("update_pool_state_from_pool_data start");
  auto keys_reencryptor = epee::misc_utils::create_scope_leave_handler([&, this]() {
    m_encrypt_keys_after_refresh.reset();
  });

  if (refreshed)
  {
    if (incremental)
    {
      // Delete from the list of unconfirmed payments what the daemon reported as tx that was removed from
      // pool; do so only after refresh to not delete too early and too eagerly; maybe we will find the tx
      // later in a block, or not, or find it again in the pool txs because it was first removed but then
      // somehow quickly "resurrected" - that all does not matter here, we retrace the removal
      remove_obsolete_pool_txs(removed_pool_txids, true);
    }
    else
    {
      // Delete from the list of unconfirmed payments what we don't find anymore in the pool; a bit
      // unfortunate that we have to build a new vector with ids first, but better than copying and
      // modifying the code of 'remove_obsolete_pool_txs' here
      std::vector<crypto::hash> txids;
      txids.reserve(added_pool_txs.size());
      for (const auto &pool_tx: added_pool_txs)
      {
        txids.push_back(std::get<1>(pool_tx));
      }
      remove_obsolete_pool_txs(txids, false);
    }
  }

  // Possibly remove any pending tx that's not in the pool
  const auto now = std::chrono::system_clock::now();
  std::unordered_map<crypto::hash, wallet2::unconfirmed_transfer_details>::iterator it = m_unconfirmed_txs.begin();
  while (it != m_unconfirmed_txs.end())
  {
    const crypto::hash &txid = it->first;
    MDEBUG("Checking m_unconfirmed_txs entry " << txid);
    bool found = false;
    for (const auto &pool_tx: added_pool_txs)
    {
      if (std::get<1>(pool_tx) == txid)
      {
        found = true;
        break;
      }
    }
    auto pit = it++;
    process_unconfirmed_transfer(incremental, txid, pit->second, found, now, refreshed);
    MDEBUG("Resulting state of that entry: " << pit->second.m_state);
  }

  // Collect all pool txs that are "interesting" i.e. mostly those that we don't know about yet;
  // if we work incrementally and thus see only new pool txs since last time we asked it should
  // be rare that we know already about one of those, but check nevertheless
  process_txs.clear();
  for (const auto &pool_tx: added_pool_txs)
  {
    if (accept_pool_tx_for_processing(std::get<1>(pool_tx)))
    {
      process_txs.push_back(pool_tx);
    }
  }

  MTRACE("update_pool_state_from_pool_data end");
}
//----------------------------------------------------------------------------------------------------
void wallet2::process_pool_state(const std::vector<std::tuple<cryptonote::transaction, crypto::hash, bool>> &txs)
{
  MTRACE("process_pool_state start");
  const time_t now = time(NULL);
  for (const auto &e: txs)
  {
    const cryptonote::transaction &tx = std::get<0>(e);
    const crypto::hash &tx_hash = std::get<1>(e);
    const bool double_spend_seen = std::get<2>(e);
    process_new_transaction(tx_hash, tx, std::vector<uint64_t>(), 0, 0, now, false, true, double_spend_seen, {});
    m_scanned_pool_txs[0].insert(tx_hash);
    if (m_scanned_pool_txs[0].size() > 5000)
    {
      std::swap(m_scanned_pool_txs[0], m_scanned_pool_txs[1]);
      m_scanned_pool_txs[0].clear();
    }
  }
  MTRACE("process_pool_state end");
}
//----------------------------------------------------------------------------------------------------
void wallet2::fast_refresh(uint64_t stop_height, uint64_t &blocks_start_height, std::list<crypto::hash> &short_chain_history, bool force)
{
  std::vector<crypto::hash> hashes;

  const uint64_t checkpoint_height = (stop_height < 1000) ? 0 : m_checkpoints.get_nearest_checkpoint_height(stop_height);
  if ((stop_height > checkpoint_height && m_blockchain.size()-1 < checkpoint_height) && !force)
  {
    // we will drop all these, so don't bother getting them
    uint64_t missing_blocks = checkpoint_height - m_blockchain.size();
    while (missing_blocks-- > 0)
      m_blockchain.push_back(crypto::null_hash); // maybe a bit suboptimal, but deque won't do huge reallocs like vector
    m_blockchain.push_back(m_checkpoints.get_points().at(checkpoint_height));
    m_blockchain.trim(checkpoint_height);
    short_chain_history.clear();
    get_short_chain_history(short_chain_history);
  }

  size_t current_index = m_blockchain.size();
  while(m_run.load(std::memory_order_relaxed) && current_index < stop_height)
  {
    pull_hashes(0, blocks_start_height, short_chain_history, hashes);
    if (hashes.size() <= 3)
      return;
    if (blocks_start_height < m_blockchain.offset())
    {
      MERROR("Blocks start before blockchain offset: " << blocks_start_height << " " << m_blockchain.offset());
      return;
    }
    current_index = blocks_start_height;
    if (hashes.size() + current_index < stop_height) {
      drop_from_short_history(short_chain_history, 3);
      std::vector<crypto::hash>::iterator right = hashes.end();
      // prepend 3 more
      for (int i = 0; i<3; i++) {
        right--;
        short_chain_history.push_front(*right);
      }
    }
    for(auto& bl_id: hashes)
    {
      if(current_index >= m_blockchain.size())
      {
        if (!(current_index % 1024))
          LOG_PRINT_L2( "Skipped block by height: " << current_index);
        m_blockchain.push_back(bl_id);

        if (0 != m_callback)
        { // FIXME: this isn't right, but simplewallet just logs that we got a block.
          cryptonote::block dummy;
          m_callback->on_new_block(current_index, dummy);
        }
      }
      else if(bl_id != m_blockchain[current_index])
      {
        //split detected here !!!
        return;
      }
      ++current_index;
      if (current_index >= stop_height)
        return;
    }
  }
}


bool wallet2::add_address_book_row(const cryptonote::account_public_address &address, const crypto::hash8 *payment_id, const std::string &description, bool is_subaddress)
{
  wallet2::address_book_row a;
  a.m_address = address;
  a.m_has_payment_id = !!payment_id;
  a.m_payment_id = payment_id ? *payment_id : crypto::null_hash8;
  a.m_description = description;
  a.m_is_subaddress = is_subaddress;
  
  auto old_size = m_address_book.size();
  m_address_book.push_back(a);
  if(m_address_book.size() == old_size+1)
    return true;
  return false;
}

bool wallet2::set_address_book_row(size_t row_id, const cryptonote::account_public_address &address, const crypto::hash8 *payment_id, const std::string &description, bool is_subaddress)
{
  wallet2::address_book_row a;
  a.m_address = address;
  a.m_has_payment_id = !!payment_id;
  a.m_payment_id = payment_id ? *payment_id : crypto::null_hash8;
  a.m_description = description;
  a.m_is_subaddress = is_subaddress;

  const auto size = m_address_book.size();
  if (row_id >= size)
    return false;
  m_address_book[row_id] = a;
  return true;
}

bool wallet2::delete_address_book_row(std::size_t row_id) {
  if(m_address_book.size() <= row_id)
    return false;
  
  m_address_book.erase(m_address_book.begin()+row_id);

  return true;
}

//----------------------------------------------------------------------------------------------------
std::shared_ptr<std::map<std::pair<uint64_t, uint64_t>, size_t>> wallet2::create_output_tracker_cache() const
{
  std::shared_ptr<std::map<std::pair<uint64_t, uint64_t>, size_t>> cache{new std::map<std::pair<uint64_t, uint64_t>, size_t>()};
  for (size_t i = 0; i < m_transfers.size(); ++i)
  {
    const transfer_details &td = m_transfers[i];
    (*cache)[std::make_pair(td.is_rct() ? 0 : td.amount(), td.m_global_output_index)] = i;
  }
  return cache;
}
//----------------------------------------------------------------------------------------------------
void wallet2::refresh(bool trusted_daemon, uint64_t start_height, uint64_t & blocks_fetched, bool& received_money, bool check_pool, bool try_incremental, uint64_t max_blocks)
{
  if (m_offline)
  {
    blocks_fetched = 0;
    received_money = 0;
    return;
  }

  if (!m_first_refresh_done)
  {
    // We want to process the whole pool again, in case we identify received outputs in the chain we might have spent in the pool
    m_pool_info_query_time = 0;
    m_scanned_pool_txs[0].clear();
    m_scanned_pool_txs[1].clear();
    // Clear unconfirmed (received) payments because the data is 100% recovered when scanning
    m_unconfirmed_payments.clear();
    // Don't clear unconfirmed (sent) txs because some data is not recover-able when scanning (dests)
  }

  received_money = false;
  blocks_fetched = 0;
  uint64_t added_blocks = 0;
  size_t try_count = 0;
  crypto::hash last_tx_hash_id = m_transfers.size() ? m_transfers.back().m_txid : null_hash;
  std::list<crypto::hash> short_chain_history;
  tools::threadpool& tpool = tools::threadpool::getInstanceForCompute();
  tools::threadpool::waiter waiter(tpool);
  uint64_t blocks_start_height;
  std::vector<cryptonote::block_complete_entry> blocks;
  std::vector<parsed_block> parsed_blocks;
  // TODO moneromooo-monero says this about the "refreshed" variable:
  // "I had to reorder some code to fix... a timing info leak IIRC. In turn, this undid something I had fixed before, ... a subtle race condition with the txpool.
  // It was pretty subtle IIRC, and so I needed time to think about how to refix it after the move, and I never got to it."
  // https://github.com/monero-project/monero/pull/6097
  bool refreshed = false;
  std::shared_ptr<std::map<std::pair<uint64_t, uint64_t>, size_t>> output_tracker_cache;
  hw::device &hwdev = m_account.get_device();

  // pull the first set of blocks
  get_short_chain_history(short_chain_history, (m_first_refresh_done || trusted_daemon) ? 1 : FIRST_REFRESH_GRANULARITY);
  m_run.store(true, std::memory_order_relaxed);
  if (start_height > m_blockchain.size() || m_refresh_from_block_height > m_blockchain.size() || m_skip_to_height > m_blockchain.size()) {
    if (!start_height)
      start_height = std::max(m_refresh_from_block_height, m_skip_to_height);;
    // we can shortcut by only pulling hashes up to the start_height
    fast_refresh(start_height, blocks_start_height, short_chain_history);
    // regenerate the history now that we've got a full set of hashes
    short_chain_history.clear();
    get_short_chain_history(short_chain_history, (m_first_refresh_done || trusted_daemon) ? 1 : FIRST_REFRESH_GRANULARITY);
    start_height = 0;
    // and then fall through to regular refresh processing
  }

  // If stop() is called during fast refresh we don't need to continue
  if(!m_run.load(std::memory_order_relaxed))
    return;
  // always reset start_height to 0 to force short_chain_ history to be used on
  // subsequent pulls in this refresh.
  start_height = 0;

  auto keys_reencryptor = epee::misc_utils::create_scope_leave_handler([&, this]() {
    m_encrypt_keys_after_refresh.reset();
  });

  auto scope_exit_handler_hwdev = epee::misc_utils::create_scope_leave_handler([&](){hwdev.computing_key_images(false);});

  std::vector<std::tuple<cryptonote::transaction, crypto::hash, bool>> process_pool_txs;
  // Getting and processing the pool state has moved down into method 'pull_blocks' to
  // allow for "conventional" as well as "incremental" update. However the following
  // principle of getting all info first (pool AND blocks) and only process txs afterwards
  // still holds and is still respected:
  // get updated pool state first, but do not process those txes just yet,
  // since that might cause a password prompt, which would introduce a data
  // leak allowing a passive adversary with traffic analysis capability to
  // infer when we get an incoming output

  bool first = true, last = false;
  while(m_run.load(std::memory_order_relaxed) && blocks_fetched < max_blocks)
  {
    uint64_t next_blocks_start_height;
    std::vector<cryptonote::block_complete_entry> next_blocks;
    std::vector<parsed_block> next_parsed_blocks;
    bool error;
    std::exception_ptr exception;
    try
    {
      // pull the next set of blocks while we're processing the current one
      error = false;
      exception = NULL;
      next_blocks.clear();
      next_parsed_blocks.clear();
      added_blocks = 0;
      if (!first && blocks.empty())
      {
        m_node_rpc_proxy.set_height(m_blockchain.size());
        break;
      }
      if (!last)
        tpool.submit(&waiter, [&]{pull_and_parse_next_blocks(first, try_incremental, start_height, next_blocks_start_height, short_chain_history, blocks, parsed_blocks, next_blocks, next_parsed_blocks, process_pool_txs, last, error, exception);});

      if (!first)
      {
        try
        {
          process_parsed_blocks(blocks_start_height, blocks, parsed_blocks, added_blocks, output_tracker_cache.get());
        }
        catch (const tools::error::out_of_hashchain_bounds_error&)
        {
          MINFO("Daemon claims next refresh block is out of hash chain bounds, resetting hash chain");
          uint64_t stop_height = m_blockchain.offset();
          std::vector<crypto::hash> tip(m_blockchain.size() - m_blockchain.offset());
          for (size_t i = m_blockchain.offset(); i < m_blockchain.size(); ++i)
            tip[i - m_blockchain.offset()] = m_blockchain[i];
          cryptonote::block b;
          generate_genesis(b);
          m_blockchain.clear();
          m_blockchain.push_back(get_block_hash(b));
          short_chain_history.clear();
          get_short_chain_history(short_chain_history);
          fast_refresh(stop_height, blocks_start_height, short_chain_history, true);
          THROW_WALLET_EXCEPTION_IF((m_blockchain.size() == stop_height || (m_blockchain.size() == 1 && stop_height == 0) ? false : true), error::wallet_internal_error, "Unexpected hashchain size");
          THROW_WALLET_EXCEPTION_IF(m_blockchain.offset() != 0, error::wallet_internal_error, "Unexpected hashchain offset");
          for (const auto &h: tip)
            m_blockchain.push_back(h);
          short_chain_history.clear();
          get_short_chain_history(short_chain_history);
          start_height = stop_height;
          throw std::runtime_error(""); // loop again
        }
        catch (const std::exception &e)
        {
          MERROR("Error parsing blocks: " << e.what());
          exception = std::current_exception();
          error = true;
        }
        blocks_fetched += added_blocks;
      }
      THROW_WALLET_EXCEPTION_IF(!waiter.wait(), error::wallet_internal_error, "Exception in thread pool");

      // handle error from async fetching thread
      if (error)
      {
        if (exception)
          std::rethrow_exception(exception);
        else
          throw std::runtime_error("proxy exception in refresh thread");
      }

      m_has_ever_refreshed_from_node = true;

      if(!first && blocks_start_height == next_blocks_start_height)
      {
        m_node_rpc_proxy.set_height(m_blockchain.size());
        break;
      }

      first = false;

      // if we've got at least 10 blocks to refresh, assume we're starting
      // a long refresh, and setup a tracking output cache if we need to
      if (m_track_uses && (!output_tracker_cache || output_tracker_cache->empty()) && next_blocks.size() >= 10)
        output_tracker_cache = create_output_tracker_cache();

      // switch to the new blocks from the daemon
      blocks_start_height = next_blocks_start_height;
      blocks = std::move(next_blocks);
      parsed_blocks = std::move(next_parsed_blocks);
    }
    catch (const tools::error::password_needed&)
    {
      blocks_fetched += added_blocks;
      THROW_WALLET_EXCEPTION_IF(!waiter.wait(), error::wallet_internal_error, "Exception in thread pool");
      throw;
    }
    catch (const error::deprecated_rpc_access&)
    {
      THROW_WALLET_EXCEPTION_IF(!waiter.wait(), error::wallet_internal_error, "Exception in thread pool");
      throw;
    }
    catch (const error::reorg_depth_error&)
    {
      THROW_WALLET_EXCEPTION_IF(!waiter.wait(), error::wallet_internal_error, "Exception in thread pool");
      throw;
    }
    catch (const error::incorrect_fork_version&)
    {
      THROW_WALLET_EXCEPTION_IF(!waiter.wait(), error::wallet_internal_error, "Exception in thread pool");
      throw;
    }
    catch (const std::exception&)
    {
      blocks_fetched += added_blocks;
      THROW_WALLET_EXCEPTION_IF(!waiter.wait(), error::wallet_internal_error, "Exception in thread pool");
      if(try_count < 3)
      {
        LOG_PRINT_L1("Another try pull_blocks (try_count=" << try_count << ")...");
        first = true;
        last = false;
        start_height = 0;
        blocks.clear();
        parsed_blocks.clear();
        short_chain_history.clear();
        get_short_chain_history(short_chain_history, 1);
        ++try_count;
      }
      else
      {
        LOG_ERROR("pull_blocks failed, try_count=" << try_count);
        throw;
      }
    }
  }
  if(last_tx_hash_id != (m_transfers.size() ? m_transfers.back().m_txid : null_hash))
    received_money = true;

  try
  {
    // If stop() is called we don't need to check pending transactions
    if (check_pool && m_run.load(std::memory_order_relaxed) && !process_pool_txs.empty())
      process_pool_state(process_pool_txs);
  }
  catch (...)
  {
    LOG_PRINT_L1("Failed to check pending transactions");
  }

  m_first_refresh_done = true;
  if (m_background_syncing || m_is_background_wallet)
    m_background_sync_data.first_refresh_done = true;

  m_multisig_rescan_info = std::vector<std::vector<tools::wallet2::multisig_info>>{};
  for (auto &v: m_multisig_rescan_k)
    memwipe(v.data(), v.size() * sizeof(v[0]));

  m_multisig_rescan_k = std::vector<std::vector<rct::key>>{};

  LOG_PRINT_L1("Refresh done, blocks received: " << blocks_fetched << ", balance (all accounts): " << print_money(balance_all(false)) << ", unlocked: " << print_money(unlocked_balance_all(false)));
}
//----------------------------------------------------------------------------------------------------
bool wallet2::refresh(bool trusted_daemon, uint64_t & blocks_fetched, bool& received_money, bool& ok)
{
  try
  {
    refresh(trusted_daemon, 0, blocks_fetched, received_money);
    ok = true;
  }
  catch (...)
  {
    ok = false;
  }
  return ok;
}
//----------------------------------------------------------------------------------------------------
bool wallet2::get_rct_distribution(uint64_t &start_height, std::vector<uint64_t> &distribution)
{
  MDEBUG("Requesting rct distribution");

  cryptonote::COMMAND_RPC_GET_OUTPUT_DISTRIBUTION::request req = AUTO_VAL_INIT(req);
  cryptonote::COMMAND_RPC_GET_OUTPUT_DISTRIBUTION::response res = AUTO_VAL_INIT(res);
  req.amounts.push_back(0);
  req.from_height = 0;
  req.cumulative = false;
  req.binary = true;
  req.compress = true;

  bool r;
  try
  {
    const boost::lock_guard<boost::recursive_mutex> lock{m_daemon_rpc_mutex};
    r = net_utils::invoke_http_bin("/get_output_distribution.bin", req, res, *m_http_client, rpc_timeout);
    THROW_ON_RPC_RESPONSE_ERROR_GENERIC(r, {}, res, "/get_output_distribution.bin");
  }
  catch(...)
  {
    return false;
  }
  if (res.distributions.size() != 1)
  {
    MWARNING("Failed to request output distribution: not the expected single result");
    return false;
  }
  if (res.distributions[0].amount != 0)
  {
    MWARNING("Failed to request output distribution: results are not for amount 0");
    return false;
  }
  for (size_t i = 1; i < res.distributions[0].data.distribution.size(); ++i)
    res.distributions[0].data.distribution[i] += res.distributions[0].data.distribution[i-1];
  start_height = res.distributions[0].data.start_height;
  distribution = std::move(res.distributions[0].data.distribution);
  return true;
}
//----------------------------------------------------------------------------------------------------
wallet2::detached_blockchain_data wallet2::detach_blockchain(uint64_t height, std::map<std::pair<uint64_t, uint64_t>, size_t> *output_tracker_cache)
{
  LOG_PRINT_L0("Detaching blockchain on height " << height);
  detached_blockchain_data dbd;

  size_t transfers_detached = 0;

  for (size_t i = 0; i < m_transfers.size(); ++i)
  {
    wallet2::transfer_details &td = m_transfers[i];
    if (td.m_spent && td.m_spent_height >= height)
    {
      LOG_PRINT_L1("Resetting spent/frozen status for output " << i << ": " << td.m_key_image);
      set_unspent(i);
      thaw(i);
    }
  }

  for (transfer_details &td: m_transfers)
  {
    while (!td.m_uses.empty() && td.m_uses.back().first >= height)
      td.m_uses.pop_back();
  }

  for (auto it = m_background_sync_data.txs.begin(); it != m_background_sync_data.txs.end(); )
  {
    if(height <= it->second.height)
      it = m_background_sync_data.txs.erase(it);
    else
      ++it;
  }

  if (output_tracker_cache)
    output_tracker_cache->clear();

  auto it = std::find_if(m_transfers.begin(), m_transfers.end(), [&](const transfer_details& td){return td.m_block_height >= height;});
  size_t i_start = it - m_transfers.begin();

  for(size_t i = i_start; i!= m_transfers.size();i++)
  {
    if (!m_transfers[i].m_key_image_known || m_transfers[i].m_key_image_partial)
      continue;
    auto it_ki = m_key_images.find(m_transfers[i].m_key_image);
    THROW_WALLET_EXCEPTION_IF(it_ki == m_key_images.end(), error::wallet_internal_error, "key image not found: index " + std::to_string(i) + ", ki " + epee::string_tools::pod_to_hex(m_transfers[i].m_key_image) + ", " + std::to_string(m_key_images.size()) + " key images known");
    m_key_images.erase(it_ki);
  }

  for(size_t i = i_start; i!= m_transfers.size();i++)
  {
    auto it_pk = m_pub_keys.find(m_transfers[i].get_public_key());
    THROW_WALLET_EXCEPTION_IF(it_pk == m_pub_keys.end(), error::wallet_internal_error, "public key not found");
    m_pub_keys.erase(it_pk);
  }

  transfers_detached = std::distance(it, m_transfers.end());
  dbd.detached_tx_hashes.reserve(transfers_detached);
  for (size_t i = i_start; i!=m_transfers.size();i++)
    dbd.detached_tx_hashes.insert(std::move(m_transfers[i].m_txid));
  MDEBUG(transfers_detached << " transfers detached / expected " << dbd.detached_tx_hashes.size());
  m_transfers.erase(it, m_transfers.end());

  uint64_t blocks_detached = 0;
  dbd.original_chain_size = m_blockchain.size();
  if (height <= m_blockchain.size() && height >= m_blockchain.offset())
  {
    for (uint64_t i = height; i < m_blockchain.size(); ++i)
      dbd.detached_blockchain.push_back(m_blockchain[i]);
    blocks_detached = m_blockchain.size() - height;
    m_blockchain.crop(height);
    MDEBUG(blocks_detached << " blocks detached / expected " << dbd.detached_blockchain.size());
  }

  for (auto it = m_payments.begin(); it != m_payments.end(); )
  {
    if(height <= it->second.m_block_height)
    {
      dbd.detached_tx_hashes.insert(it->second.m_tx_hash);
      it = m_payments.erase(it);
    }
    else
      ++it;
  }

  for (auto it = m_confirmed_txs.begin(); it != m_confirmed_txs.end(); )
  {
    if(height <= it->second.m_block_height)
    {
      dbd.detached_tx_hashes.insert(it->first);
      dbd.detached_confirmed_txs_dests[it->first] = std::move(it->second.m_dests);
      it = m_confirmed_txs.erase(it);
    }
    else
      ++it;
  }

  LOG_PRINT_L0("Detached blockchain on height " << height << ", transfers detached " << transfers_detached << ", blocks detached " << blocks_detached);
  return dbd;
}
//----------------------------------------------------------------------------------------------------
void wallet2::handle_reorg(uint64_t height, std::map<std::pair<uint64_t, uint64_t>, size_t> *output_tracker_cache)
{
  // size  1 2 3 4 5 6 7 8 9
  // block 0 1 2 3 4 5 6 7 8
  //               C
  THROW_WALLET_EXCEPTION_IF(height < m_blockchain.offset() && m_blockchain.size() > m_blockchain.offset(),
      error::wallet_internal_error, "Daemon claims reorg below last checkpoint");

  detached_blockchain_data dbd = detach_blockchain(height, output_tracker_cache);

  if (m_background_syncing && height < m_background_sync_data.start_height)
    m_background_sync_data.start_height = height;

  if (m_callback)
    m_callback->on_reorg(height, dbd.detached_blockchain.size(), dbd.detached_tx_hashes.size());
}
//----------------------------------------------------------------------------------------------------
bool wallet2::deinit()
{
  if(m_is_initialized) {
    m_is_initialized = false;
    unlock_keys_file();
    unlock_background_keys_file();
    m_account.deinit();
  }
  return true;
}
//----------------------------------------------------------------------------------------------------
bool wallet2::clear()
{
  m_blockchain.clear();
  m_transfers.clear();
  m_key_images.clear();
  m_pub_keys.clear();
  m_unconfirmed_txs.clear();
  m_payments.clear();
  m_tx_keys.clear();
  m_additional_tx_keys.clear();
  m_confirmed_txs.clear();
  m_unconfirmed_payments.clear();
  m_scanned_pool_txs[0].clear();
  m_scanned_pool_txs[1].clear();
  m_address_book.clear();
  m_subaddresses.clear();
  m_subaddress_labels.clear();
  m_multisig_rounds_passed = 0;
  m_device_last_key_image_sync = 0;
  m_pool_info_query_time = 0;
  m_skip_to_height = 0;
  m_background_sync_data = background_sync_data_t{};
  return true;
}
//----------------------------------------------------------------------------------------------------
void wallet2::clear_soft(bool keep_key_images)
{
  m_blockchain.clear();
  m_transfers.clear();
  if (!keep_key_images)
    m_key_images.clear();
  m_pub_keys.clear();
  m_unconfirmed_txs.clear();
  m_payments.clear();
  m_confirmed_txs.clear();
  m_unconfirmed_payments.clear();
  m_scanned_pool_txs[0].clear();
  m_scanned_pool_txs[1].clear();
  m_pool_info_query_time = 0;
  m_skip_to_height = 0;
  m_background_sync_data = background_sync_data_t{};

  cryptonote::block b;
  generate_genesis(b);
  m_blockchain.push_back(get_block_hash(b));
  m_last_block_reward = cryptonote::get_outs_money_amount(b.miner_tx);
}
//----------------------------------------------------------------------------------------------------
void wallet2::clear_user_data()
{
  for (auto i = m_confirmed_txs.begin(); i != m_confirmed_txs.end(); ++i)
    i->second.m_dests.clear();
  for (auto i = m_unconfirmed_txs.begin(); i != m_unconfirmed_txs.end(); ++i)
    i->second.m_dests.clear();
  for (auto i = m_transfers.begin(); i != m_transfers.end(); ++i)
    i->m_frozen = false;
  m_tx_keys.clear();
  m_tx_notes.clear();
  m_address_book.clear();
  m_subaddress_labels.clear();
  m_attributes.clear();
  m_account_tags = std::pair<std::map<std::string, std::string>, std::vector<std::string>>();
}
//----------------------------------------------------------------------------------------------------
/*!
 * \brief Stores wallet information to wallet file.
 * \param  keys_file_name Name of wallet file
 * \param  password       Password of wallet file
 * \param  watch_only     true to save only view key, false to save both spend and view keys
 * \return                Whether it was successful.
 */
bool wallet2::store_keys(const std::string& keys_file_name, const epee::wipeable_string& password, bool watch_only)
{
  boost::optional<wallet2::keys_file_data> keys_file_data = get_keys_file_data(password, watch_only);
  CHECK_AND_ASSERT_MES(keys_file_data != boost::none, false, "failed to generate wallet keys data");
  return store_keys_file_data(keys_file_name, keys_file_data.get());
}
//----------------------------------------------------------------------------------------------------
bool wallet2::store_keys(const std::string& keys_file_name, const crypto::chacha_key& key, bool watch_only, bool background_keys_file)
{
  boost::optional<wallet2::keys_file_data> keys_file_data = get_keys_file_data(key, watch_only, background_keys_file);
  CHECK_AND_ASSERT_MES(keys_file_data != boost::none, false, "failed to generate wallet keys data");
  return store_keys_file_data(keys_file_name, keys_file_data.get(), background_keys_file);
}
//----------------------------------------------------------------------------------------------------
bool wallet2::store_keys_file_data(const std::string& keys_file_name, wallet2::keys_file_data &keys_file_data, bool background_keys_file)
{
  std::string tmp_file_name = keys_file_name + ".new";
  std::string buf;
  bool r = ::serialization::dump_binary(keys_file_data, buf);
  r = r && save_to_file(tmp_file_name, buf);
  CHECK_AND_ASSERT_MES(r, false, "failed to generate wallet keys file " << tmp_file_name);

  if (!background_keys_file)
    unlock_keys_file();
  else
    unlock_background_keys_file();

  std::error_code e = tools::replace_file(tmp_file_name, keys_file_name);

  if (!background_keys_file)
    lock_keys_file();
  else
    lock_background_keys_file(keys_file_name);

  if (e) {
    boost::filesystem::remove(tmp_file_name);
    LOG_ERROR("failed to update wallet keys file " << keys_file_name);
    return false;
  }

  return true;
}
//----------------------------------------------------------------------------------------------------
boost::optional<wallet2::keys_file_data> wallet2::get_keys_file_data(const epee::wipeable_string& password, bool watch_only)
{
  crypto::chacha_key key;
  crypto::generate_chacha_key(password.data(), password.size(), key, m_kdf_rounds);
  verify_password_with_cached_key(key);
  return get_keys_file_data(key, watch_only);
}
//----------------------------------------------------------------------------------------------------
boost::optional<wallet2::keys_file_data> wallet2::get_keys_file_data(const crypto::chacha_key& key, bool watch_only, bool background_keys_file)
{
  epee::byte_slice account_data;
  std::string multisig_signers;
  std::string multisig_derivations;
  cryptonote::account_base account = m_account;

  if (m_ask_password == AskPasswordToDecrypt && !m_unattended && !m_watch_only)
  {
    account.encrypt_viewkey(key);
    account.decrypt_keys(key);
  }

  if (watch_only || background_keys_file)
    account.forget_spend_key();

  account.encrypt_keys(key);

  bool r = epee::serialization::store_t_to_binary(account, account_data);
  CHECK_AND_ASSERT_MES(r, boost::none, "failed to serialize wallet keys");
  boost::optional<wallet2::keys_file_data> keys_file_data = (wallet2::keys_file_data) {};

  // Create a JSON object with "key_data" and "seed_language" as keys.
  rapidjson::Document json;
  json.SetObject();
  rapidjson::Value value(rapidjson::kStringType);
  value.SetString(reinterpret_cast<const char*>(account_data.data()), account_data.size());
  json.AddMember("key_data", value, json.GetAllocator());
  if (!seed_language.empty())
  {
    value.SetString(seed_language.c_str(), seed_language.length());
    json.AddMember("seed_language", value, json.GetAllocator());
  }

  rapidjson::Value value2(rapidjson::kNumberType);

  value2.SetInt(m_key_device_type);
  json.AddMember("key_on_device", value2, json.GetAllocator());

  value2.SetInt((watch_only || m_watch_only) ? 1 :0); // WTF ? JSON has different true and false types, and not boolean ??
  json.AddMember("watch_only", value2, json.GetAllocator());

  value2.SetInt(m_multisig ? 1 :0);
  json.AddMember("multisig", value2, json.GetAllocator());

  value2.SetUint(m_multisig_threshold);
  json.AddMember("multisig_threshold", value2, json.GetAllocator());

  if (m_multisig)
  {
    bool r = ::serialization::dump_binary(m_multisig_signers, multisig_signers);
    CHECK_AND_ASSERT_MES(r, boost::none, "failed to serialize wallet multisig signers");
    value.SetString(multisig_signers.c_str(), multisig_signers.length());
    json.AddMember("multisig_signers", value, json.GetAllocator());

    r = ::serialization::dump_binary(m_multisig_derivations, multisig_derivations);
    CHECK_AND_ASSERT_MES(r, boost::none, "failed to serialize wallet multisig derivations");
    value.SetString(multisig_derivations.c_str(), multisig_derivations.length());
    json.AddMember("multisig_derivations", value, json.GetAllocator());

    value2.SetUint(m_multisig_rounds_passed);
    json.AddMember("multisig_rounds_passed", value2, json.GetAllocator());
  }

  value2.SetInt(m_always_confirm_transfers ? 1 :0);
  json.AddMember("always_confirm_transfers", value2, json.GetAllocator());

  value2.SetInt(m_print_ring_members ? 1 :0);
  json.AddMember("print_ring_members", value2, json.GetAllocator());

  value2.SetInt(m_store_tx_info ? 1 :0);
  json.AddMember("store_tx_info", value2, json.GetAllocator());

  value2.SetUint(m_default_mixin);
  json.AddMember("default_mixin", value2, json.GetAllocator());

  value2.SetUint(boost::numeric_cast<unsigned>(fee_priority_utilities::as_integral(m_default_priority)));
  json.AddMember("default_priority", value2, json.GetAllocator());

  value2.SetInt(m_auto_refresh ? 1 :0);
  json.AddMember("auto_refresh", value2, json.GetAllocator());

  value2.SetInt(m_refresh_type);
  json.AddMember("refresh_type", value2, json.GetAllocator());

  value2.SetUint64(m_refresh_from_block_height);
  json.AddMember("refresh_height", value2, json.GetAllocator());

  value2.SetUint64(m_skip_to_height);
  json.AddMember("skip_to_height", value2, json.GetAllocator());

  value2.SetInt(1); // exists for deserialization backwards compatibility
  json.AddMember("confirm_non_default_ring_size", value2, json.GetAllocator());

  value2.SetInt(m_ask_password);
  json.AddMember("ask_password", value2, json.GetAllocator());

  value2.SetUint64(m_max_reorg_depth);
  json.AddMember("max_reorg_depth", value2, json.GetAllocator());

  value2.SetUint(m_min_output_count);
  json.AddMember("min_output_count", value2, json.GetAllocator());

  value2.SetUint64(m_min_output_value);
  json.AddMember("min_output_value", value2, json.GetAllocator());

  value2.SetInt(cryptonote::get_default_decimal_point());
  json.AddMember("default_decimal_point", value2, json.GetAllocator());

  value2.SetInt(m_merge_destinations ? 1 :0);
  json.AddMember("merge_destinations", value2, json.GetAllocator());

  value2.SetInt(m_confirm_backlog ? 1 :0);
  json.AddMember("confirm_backlog", value2, json.GetAllocator());

  value2.SetUint(m_confirm_backlog_threshold);
  json.AddMember("confirm_backlog_threshold", value2, json.GetAllocator());

  value2.SetInt(m_confirm_export_overwrite ? 1 :0);
  json.AddMember("confirm_export_overwrite", value2, json.GetAllocator());

  value2.SetInt(m_auto_low_priority ? 1 : 0);
  json.AddMember("auto_low_priority", value2, json.GetAllocator());

  value2.SetUint(m_nettype);
  json.AddMember("nettype", value2, json.GetAllocator());

  value2.SetInt(m_segregate_pre_fork_outputs ? 1 : 0);
  json.AddMember("segregate_pre_fork_outputs", value2, json.GetAllocator());

  value2.SetInt(m_key_reuse_mitigation2 ? 1 : 0);
  json.AddMember("key_reuse_mitigation2", value2, json.GetAllocator());

  value2.SetUint(m_segregation_height);
  json.AddMember("segregation_height", value2, json.GetAllocator());

  value2.SetInt(m_ignore_fractional_outputs ? 1 : 0);
  json.AddMember("ignore_fractional_outputs", value2, json.GetAllocator());

  value2.SetUint64(m_ignore_outputs_above);
  json.AddMember("ignore_outputs_above", value2, json.GetAllocator());

  value2.SetUint64(m_ignore_outputs_below);
  json.AddMember("ignore_outputs_below", value2, json.GetAllocator());

  value2.SetInt(m_track_uses ? 1 : 0);
  json.AddMember("track_uses", value2, json.GetAllocator());

  value2.SetInt(m_background_sync_type);
  json.AddMember("background_sync_type", value2, json.GetAllocator());

  value2.SetInt(m_show_wallet_name_when_locked ? 1 : 0);
  json.AddMember("show_wallet_name_when_locked", value2, json.GetAllocator());

  value2.SetInt(m_inactivity_lock_timeout);
  json.AddMember("inactivity_lock_timeout", value2, json.GetAllocator());

  value2.SetInt(m_setup_background_mining);
  json.AddMember("setup_background_mining", value2, json.GetAllocator());

  value2.SetUint(m_subaddress_lookahead_major);
  json.AddMember("subaddress_lookahead_major", value2, json.GetAllocator());

  value2.SetUint(m_subaddress_lookahead_minor);
  json.AddMember("subaddress_lookahead_minor", value2, json.GetAllocator());

  value2.SetInt(m_original_keys_available ? 1 : 0);
  json.AddMember("original_keys_available", value2, json.GetAllocator());

  value2.SetInt(m_export_format);
  json.AddMember("export_format", value2, json.GetAllocator());

  value2.SetInt(false);
  json.AddMember("load_deprecated_formats", value2, json.GetAllocator());

  value2.SetUint(1);
  json.AddMember("encrypted_secret_keys", value2, json.GetAllocator());

  value.SetString(m_device_name.c_str(), m_device_name.size());
  json.AddMember("device_name", value, json.GetAllocator());

  value.SetString(m_device_derivation_path.c_str(), m_device_derivation_path.size());
  json.AddMember("device_derivation_path", value, json.GetAllocator());

  std::string original_address;
  std::string original_view_secret_key;
  if (m_original_keys_available)
  {  
    original_address = get_account_address_as_str(m_nettype, false, m_original_address);
    value.SetString(original_address.c_str(), original_address.length());
    json.AddMember("original_address", value, json.GetAllocator());
    original_view_secret_key = epee::string_tools::pod_to_hex(unwrap(unwrap(m_original_view_secret_key)));
    value.SetString(original_view_secret_key.c_str(), original_view_secret_key.length());
    json.AddMember("original_view_secret_key", value, json.GetAllocator());
  }
  
  // This value is serialized for compatibility with wallets which support the pay-to-use RPC system
  value2.SetInt(0);
  json.AddMember("persistent_rpc_client_id", value2, json.GetAllocator());

  // This value is serialized for compatibility with wallets which support the pay-to-use RPC system
  value2.SetFloat(0.0f);
  json.AddMember("auto_mine_for_rpc_payment", value2, json.GetAllocator());

  // This value is serialized for compatibility with wallets which support the pay-to-use RPC system
  value2.SetUint64(0);
  json.AddMember("credits_target", value2, json.GetAllocator());

  value2.SetInt(m_enable_multisig ? 1 : 0);
  json.AddMember("enable_multisig", value2, json.GetAllocator());

  if (m_background_sync_type == BackgroundSyncCustomPassword && !background_keys_file && m_custom_background_key)
  {
    value.SetString(reinterpret_cast<const char*>(m_custom_background_key.get().data()), m_custom_background_key.get().size());
    json.AddMember("custom_background_key", value, json.GetAllocator());
  }

  // Serialize the JSON object
  rapidjson::StringBuffer buffer;
  rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
  json.Accept(writer);

  // Encrypt the entire JSON object.
  std::string cipher;
  cipher.resize(buffer.GetSize());
  keys_file_data.get().iv = crypto::rand<crypto::chacha_iv>();
  crypto::chacha20(buffer.GetString(), buffer.GetSize(), key, keys_file_data.get().iv, &cipher[0]);
  keys_file_data.get().account_data = cipher;
  return keys_file_data;
}
//----------------------------------------------------------------------------------------------------
void wallet2::setup_keys(const epee::wipeable_string &password)
{
  crypto::chacha_key key;
  crypto::generate_chacha_key(password.data(), password.size(), key, m_kdf_rounds);

  // re-encrypt, but keep viewkey unencrypted
  if (m_ask_password == AskPasswordToDecrypt && !m_unattended && !m_watch_only)
  {
    m_account.encrypt_keys(key);
    m_account.decrypt_viewkey(key);
  }

  m_cache_key = derive_cache_key(key, config::HASH_KEY_WALLET_CACHE);

  get_ringdb_key();
}
//----------------------------------------------------------------------------------------------------
void validate_background_cache_password_usage(const tools::wallet2::BackgroundSyncType background_sync_type, const boost::optional<epee::wipeable_string> &background_cache_password, const bool multisig, const bool watch_only, const bool key_on_device)
{
  THROW_WALLET_EXCEPTION_IF(multisig || watch_only || key_on_device, error::wallet_internal_error, multisig
      ? "Background sync not implemented for multisig wallets" : watch_only
      ? "Background sync not implemented for view only wallets"
      : "Background sync not implemented for HW wallets");

  switch (background_sync_type)
  {
    case tools::wallet2::BackgroundSyncOff:
    {
      THROW_WALLET_EXCEPTION(error::wallet_internal_error, "background sync is not enabled");
      break;
    }
    case tools::wallet2::BackgroundSyncReusePassword:
    {
      THROW_WALLET_EXCEPTION_IF(background_cache_password, error::wallet_internal_error,
          "unexpected custom background cache password");
      break;
    }
    case tools::wallet2::BackgroundSyncCustomPassword:
    {
      THROW_WALLET_EXCEPTION_IF(!background_cache_password, error::wallet_internal_error,
          "expected custom background cache password");
      break;
    }
    default: THROW_WALLET_EXCEPTION(error::wallet_internal_error, "unknown background sync type");
  }
}
//----------------------------------------------------------------------------------------------------
void get_custom_background_key(const epee::wipeable_string &password, crypto::chacha_key &custom_background_key, const uint64_t kdf_rounds)
{
  crypto::chacha_key key;
  crypto::generate_chacha_key(password.data(), password.size(), key, kdf_rounds);
  custom_background_key = derive_cache_key(key, config::HASH_KEY_BACKGROUND_KEYS_FILE);
}
//----------------------------------------------------------------------------------------------------
const crypto::chacha_key wallet2::get_cache_key()
{
  if (m_background_sync_type == BackgroundSyncCustomPassword && m_background_syncing)
  {
    THROW_WALLET_EXCEPTION_IF(!m_custom_background_key, error::wallet_internal_error, "Custom background key not set");
    // Domain separate keys used to encrypt background keys file and cache
    return derive_cache_key(m_custom_background_key.get(), config::HASH_KEY_BACKGROUND_CACHE);
  }
  else
  {
    return m_cache_key;
  }
}
//----------------------------------------------------------------------------------------------------
void wallet2::verify_password_with_cached_key(const epee::wipeable_string &password)
{
  crypto::chacha_key key;
  crypto::generate_chacha_key(password.data(), password.size(), key, m_kdf_rounds);
  verify_password_with_cached_key(key);
}
//----------------------------------------------------------------------------------------------------
void wallet2::verify_password_with_cached_key(const crypto::chacha_key &key)
{
  // We use m_cache_key as a deterministic test to see if given key corresponds to original password
  const crypto::chacha_key cache_key = derive_cache_key(key, config::HASH_KEY_WALLET_CACHE);
  THROW_WALLET_EXCEPTION_IF(cache_key != m_cache_key, error::invalid_password);
}
//----------------------------------------------------------------------------------------------------
void wallet2::change_password(const std::string &filename, const epee::wipeable_string &original_password, const epee::wipeable_string &new_password)
{
  THROW_WALLET_EXCEPTION_IF(m_background_syncing || m_is_background_wallet, error::wallet_internal_error,
      "cannot change password from background wallet");

  if (m_ask_password == AskPasswordToDecrypt && !m_unattended && !m_watch_only)
    decrypt_keys(original_password);
  setup_keys(new_password);
  if (!filename.empty())
    store_to(filename, new_password, true); // force rewrite keys file to possible new location
}
//----------------------------------------------------------------------------------------------------
/*!
 * \brief Load wallet information from wallet file.
 * \param keys_file_name Name of wallet file
 * \param password       Password of wallet file
 */
bool wallet2::load_keys(const std::string& keys_file_name, const epee::wipeable_string& password)
{
  std::string keys_file_buf;
  bool r = load_from_file(keys_file_name, keys_file_buf);
  THROW_WALLET_EXCEPTION_IF(!r, error::file_read_error, keys_file_name);

  // Load keys from buffer
  boost::optional<crypto::chacha_key> keys_to_encrypt;
  r = wallet2::load_keys_buf(keys_file_buf, password, keys_to_encrypt);

  // Rewrite with encrypted keys if unencrypted, ignore errors
  if (r && keys_to_encrypt != boost::none)
  {
    if (m_ask_password == AskPasswordToDecrypt && !m_unattended && !m_watch_only)
      encrypt_keys(keys_to_encrypt.get());
    bool saved_ret = store_keys(keys_file_name, password, m_watch_only);
    if (!saved_ret)
    {
      // just moan a bit, but not fatal
      MERROR("Error saving keys file with encrypted keys, not fatal");
    }
    if (m_ask_password == AskPasswordToDecrypt && !m_unattended && !m_watch_only)
      decrypt_keys(keys_to_encrypt.get());
    m_keys_file_locker.reset();
  }
  return r;
}
//----------------------------------------------------------------------------------------------------
bool wallet2::load_keys_buf(const std::string& keys_buf, const epee::wipeable_string& password) {
  boost::optional<crypto::chacha_key> keys_to_encrypt;
  return wallet2::load_keys_buf(keys_buf, password, keys_to_encrypt);
}
//----------------------------------------------------------------------------------------------------
bool wallet2::load_keys_buf(const std::string& keys_buf, const epee::wipeable_string& password, boost::optional<crypto::chacha_key>& keys_to_encrypt) {

  // Decrypt the contents
  rapidjson::Document json;
  wallet2::keys_file_data keys_file_data;
  bool encrypted_secret_keys = false;
  bool r = ::serialization::parse_binary(keys_buf, keys_file_data);
  THROW_WALLET_EXCEPTION_IF(!r, error::wallet_internal_error, "internal error: failed to deserialize keys buffer");
  crypto::chacha_key key;
  crypto::generate_chacha_key(password.data(), password.size(), key, m_kdf_rounds);
  std::string account_data;
  account_data.resize(keys_file_data.account_data.size());
  crypto::chacha20(keys_file_data.account_data.data(), keys_file_data.account_data.size(), key, keys_file_data.iv, &account_data[0]);
  const bool try_v0_format = json.Parse(account_data.c_str()).HasParseError() || !json.IsObject();
  if (try_v0_format)
    crypto::chacha8(keys_file_data.account_data.data(), keys_file_data.account_data.size(), key, keys_file_data.iv, &account_data[0]);

  // Check if it's a background keys file if both of the above formats fail
  {
    m_is_background_wallet = false;
    m_background_syncing = false;
    cryptonote::account_base account_data_check;
    if (try_v0_format && !epee::serialization::load_t_from_binary(account_data_check, account_data))
    {
      get_custom_background_key(password, key, m_kdf_rounds);
      crypto::chacha20(keys_file_data.account_data.data(), keys_file_data.account_data.size(), key, keys_file_data.iv, &account_data[0]);
      m_is_background_wallet = !json.Parse(account_data.c_str()).HasParseError() && json.IsObject();
      m_background_syncing = m_is_background_wallet; // start a background wallet background syncing
    }
  }

  // The contents should be JSON if the wallet follows the new format.
  if (json.Parse(account_data.c_str()).HasParseError())
  {
    is_old_file_format = true;
    m_watch_only = false;
    m_multisig = false;
    m_multisig_threshold = 0;
    m_multisig_signers.clear();
    m_multisig_rounds_passed = 0;
    m_multisig_derivations.clear();
    m_always_confirm_transfers = false;
    m_print_ring_members = false;
    m_store_tx_info = true;
    m_default_mixin = 0;
    m_default_priority = fee_priority::Default;
    m_auto_refresh = true;
    m_refresh_type = RefreshType::RefreshDefault;
    m_refresh_from_block_height = 0;
    m_skip_to_height = 0;
    m_ask_password = AskPasswordToDecrypt;
    cryptonote::set_default_decimal_point(CRYPTONOTE_DISPLAY_DECIMAL_POINT);
    m_max_reorg_depth = ORPHANED_BLOCKS_MAX_COUNT;
    m_min_output_count = 0;
    m_min_output_value = 0;
    m_merge_destinations = false;
    m_confirm_backlog = true;
    m_confirm_backlog_threshold = 0;
    m_confirm_export_overwrite = true;
    m_auto_low_priority = true;
    m_segregate_pre_fork_outputs = true;
    m_key_reuse_mitigation2 = true;
    m_segregation_height = 0;
    m_ignore_fractional_outputs = true;
    m_ignore_outputs_above = MONEY_SUPPLY;
    m_ignore_outputs_below = 0;
    m_track_uses = false;
    m_background_sync_type = BackgroundSyncOff;
    m_show_wallet_name_when_locked = false;
    m_inactivity_lock_timeout = DEFAULT_INACTIVITY_LOCK_TIMEOUT;
    m_setup_background_mining = BackgroundMiningMaybe;
    m_subaddress_lookahead_major = SUBADDRESS_LOOKAHEAD_MAJOR;
    m_subaddress_lookahead_minor = SUBADDRESS_LOOKAHEAD_MINOR;
    m_original_keys_available = false;
    m_export_format = ExportFormat::Binary;
    m_device_name = "";
    m_device_derivation_path = "";
    m_key_device_type = hw::device::device_type::SOFTWARE;
    encrypted_secret_keys = false;
    m_enable_multisig = false;
    m_allow_mismatched_daemon_version = false;
    m_custom_background_key = boost::none;
  }
  else if(json.IsObject())
  {
    if (!json.HasMember("key_data"))
    {
      LOG_ERROR("Field key_data not found in JSON");
      return false;
    }
    if (!json["key_data"].IsString())
    {
      LOG_ERROR("Field key_data found in JSON, but not String");
      return false;
    }
    const char *field_key_data = json["key_data"].GetString();
    account_data = std::string(field_key_data, field_key_data + json["key_data"].GetStringLength());

    if (json.HasMember("key_on_device"))
    {
      GET_FIELD_FROM_JSON_RETURN_ON_ERROR(json, key_on_device, int, Int, false, hw::device::device_type::SOFTWARE);
      m_key_device_type = static_cast<hw::device::device_type>(field_key_on_device);
    }

    GET_FIELD_FROM_JSON_RETURN_ON_ERROR(json, seed_language, std::string, String, false, std::string());
    if (field_seed_language_found)
    {
      set_seed_language(field_seed_language);
    }
    GET_FIELD_FROM_JSON_RETURN_ON_ERROR(json, watch_only, int, Int, false, false);
    m_watch_only = field_watch_only;
    GET_FIELD_FROM_JSON_RETURN_ON_ERROR(json, multisig, int, Int, false, false);
    m_multisig = field_multisig;
    GET_FIELD_FROM_JSON_RETURN_ON_ERROR(json, multisig_threshold, unsigned int, Uint, m_multisig, 0);
    m_multisig_threshold = field_multisig_threshold;
    GET_FIELD_FROM_JSON_RETURN_ON_ERROR(json, multisig_rounds_passed, unsigned int, Uint, false, 0);
    m_multisig_rounds_passed = field_multisig_rounds_passed;
    if (m_multisig)
    {
      if (!json.HasMember("multisig_signers"))
      {
        LOG_ERROR("Field multisig_signers not found in JSON");
        return false;
      }
      if (!json["multisig_signers"].IsString())
      {
        LOG_ERROR("Field multisig_signers found in JSON, but not String");
        return false;
      }
      const char *field_multisig_signers = json["multisig_signers"].GetString();
      std::string multisig_signers = std::string(field_multisig_signers, field_multisig_signers + json["multisig_signers"].GetStringLength());
      r = ::serialization::parse_binary(multisig_signers, m_multisig_signers);
      if (!r)
      {
        LOG_ERROR("Field multisig_signers found in JSON, but failed to parse");
        return false;
      }

      //previous version of multisig does not have this field
      if (json.HasMember("multisig_derivations"))
      {
        if (!json["multisig_derivations"].IsString())
        {
          LOG_ERROR("Field multisig_derivations found in JSON, but not String");
          return false;
        }
        const char *field_multisig_derivations = json["multisig_derivations"].GetString();
        std::string multisig_derivations = std::string(field_multisig_derivations, field_multisig_derivations + json["multisig_derivations"].GetStringLength());
        r = ::serialization::parse_binary(multisig_derivations, m_multisig_derivations);
        if (!r)
        {
          LOG_ERROR("Field multisig_derivations found in JSON, but failed to parse");
          return false;
        }
      }
    }
    GET_FIELD_FROM_JSON_RETURN_ON_ERROR(json, always_confirm_transfers, int, Int, false, true);
    m_always_confirm_transfers = field_always_confirm_transfers;
    GET_FIELD_FROM_JSON_RETURN_ON_ERROR(json, print_ring_members, int, Int, false, true);
    m_print_ring_members = field_print_ring_members;
    if (json.HasMember("store_tx_info"))
    {
      GET_FIELD_FROM_JSON_RETURN_ON_ERROR(json, store_tx_info, int, Int, true, true);
      m_store_tx_info = field_store_tx_info;
    }
    else if (json.HasMember("store_tx_keys")) // backward compatibility
    {
      GET_FIELD_FROM_JSON_RETURN_ON_ERROR(json, store_tx_keys, int, Int, true, true);
      m_store_tx_info = field_store_tx_keys;
    }
    else
      m_store_tx_info = true;
    GET_FIELD_FROM_JSON_RETURN_ON_ERROR(json, default_mixin, unsigned int, Uint, false, 0);
    m_default_mixin = field_default_mixin;
    GET_FIELD_FROM_JSON_RETURN_ON_ERROR(json, default_priority, unsigned int, Uint, false, 0);
    if (field_default_priority_found)
    {
      m_default_priority = fee_priority_utilities::from_integral(boost::numeric_cast<uint32_t>(field_default_priority));
    }
    else
    {
      GET_FIELD_FROM_JSON_RETURN_ON_ERROR(json, default_fee_multiplier, unsigned int, Uint, false, 0);
      if (field_default_fee_multiplier_found)
        m_default_priority = fee_priority_utilities::from_integral(boost::numeric_cast<uint32_t>(field_default_fee_multiplier));
      else
        m_default_priority = fee_priority::Default;
    }
    GET_FIELD_FROM_JSON_RETURN_ON_ERROR(json, auto_refresh, int, Int, false, true);
    m_auto_refresh = field_auto_refresh;
    GET_FIELD_FROM_JSON_RETURN_ON_ERROR(json, refresh_type, int, Int, false, RefreshType::RefreshDefault);
    m_refresh_type = RefreshType::RefreshDefault;
    if (field_refresh_type_found)
    {
      if (field_refresh_type == RefreshFull || field_refresh_type == RefreshOptimizeCoinbase || field_refresh_type == RefreshNoCoinbase)
        m_refresh_type = (RefreshType)field_refresh_type;
      else
        LOG_PRINT_L0("Unknown refresh-type value (" << field_refresh_type << "), using default");
    }
    GET_FIELD_FROM_JSON_RETURN_ON_ERROR(json, refresh_height, uint64_t, Uint64, false, 0);
    m_refresh_from_block_height = field_refresh_height;
    GET_FIELD_FROM_JSON_RETURN_ON_ERROR(json, skip_to_height, uint64_t, Uint64, false, 0);
    m_skip_to_height = field_skip_to_height;
    GET_FIELD_FROM_JSON_RETURN_ON_ERROR(json, ask_password, AskPasswordType, Int, false, AskPasswordToDecrypt);
    m_ask_password = field_ask_password;
    GET_FIELD_FROM_JSON_RETURN_ON_ERROR(json, default_decimal_point, int, Int, false, CRYPTONOTE_DISPLAY_DECIMAL_POINT);
    cryptonote::set_default_decimal_point(field_default_decimal_point);
    GET_FIELD_FROM_JSON_RETURN_ON_ERROR(json, max_reorg_depth, uint64_t, Uint64, false, ORPHANED_BLOCKS_MAX_COUNT);
    m_max_reorg_depth = field_max_reorg_depth;
    GET_FIELD_FROM_JSON_RETURN_ON_ERROR(json, min_output_count, uint32_t, Uint, false, 0);
    m_min_output_count = field_min_output_count;
    GET_FIELD_FROM_JSON_RETURN_ON_ERROR(json, min_output_value, uint64_t, Uint64, false, 0);
    m_min_output_value = field_min_output_value;
    GET_FIELD_FROM_JSON_RETURN_ON_ERROR(json, merge_destinations, int, Int, false, false);
    m_merge_destinations = field_merge_destinations;
    GET_FIELD_FROM_JSON_RETURN_ON_ERROR(json, confirm_backlog, int, Int, false, true);
    m_confirm_backlog = field_confirm_backlog;
    GET_FIELD_FROM_JSON_RETURN_ON_ERROR(json, confirm_backlog_threshold, uint32_t, Uint, false, 0);
    m_confirm_backlog_threshold = field_confirm_backlog_threshold;
    GET_FIELD_FROM_JSON_RETURN_ON_ERROR(json, confirm_export_overwrite, int, Int, false, true);
    m_confirm_export_overwrite = field_confirm_export_overwrite;
    GET_FIELD_FROM_JSON_RETURN_ON_ERROR(json, auto_low_priority, int, Int, false, true);
    m_auto_low_priority = field_auto_low_priority;
    GET_FIELD_FROM_JSON_RETURN_ON_ERROR(json, nettype, uint8_t, Uint, false, static_cast<uint8_t>(m_nettype));
    // The network type given in the program argument is inconsistent with the network type saved in the wallet
    THROW_WALLET_EXCEPTION_IF(static_cast<uint8_t>(m_nettype) != field_nettype, error::wallet_internal_error,
    (boost::format("%s wallet cannot be opened as %s wallet")
    % (field_nettype == 0 ? "Mainnet" : field_nettype == 1 ? "Testnet" : "Stagenet")
    % (m_nettype == MAINNET ? "mainnet" : m_nettype == TESTNET ? "testnet" : "stagenet")).str());
    GET_FIELD_FROM_JSON_RETURN_ON_ERROR(json, segregate_pre_fork_outputs, int, Int, false, true);
    m_segregate_pre_fork_outputs = field_segregate_pre_fork_outputs;
    GET_FIELD_FROM_JSON_RETURN_ON_ERROR(json, key_reuse_mitigation2, int, Int, false, true);
    m_key_reuse_mitigation2 = field_key_reuse_mitigation2;
    GET_FIELD_FROM_JSON_RETURN_ON_ERROR(json, segregation_height, int, Uint, false, 0);
    m_segregation_height = field_segregation_height;
    GET_FIELD_FROM_JSON_RETURN_ON_ERROR(json, ignore_fractional_outputs, int, Int, false, true);
    m_ignore_fractional_outputs = field_ignore_fractional_outputs;
    GET_FIELD_FROM_JSON_RETURN_ON_ERROR(json, ignore_outputs_above, uint64_t, Uint64, false, MONEY_SUPPLY);
    m_ignore_outputs_above = field_ignore_outputs_above;
    GET_FIELD_FROM_JSON_RETURN_ON_ERROR(json, ignore_outputs_below, uint64_t, Uint64, false, 0);
    m_ignore_outputs_below = field_ignore_outputs_below;
    GET_FIELD_FROM_JSON_RETURN_ON_ERROR(json, track_uses, int, Int, false, false);
    m_track_uses = field_track_uses;
    GET_FIELD_FROM_JSON_RETURN_ON_ERROR(json, show_wallet_name_when_locked, int, Int, false, false);
    m_show_wallet_name_when_locked = field_show_wallet_name_when_locked;
    GET_FIELD_FROM_JSON_RETURN_ON_ERROR(json, inactivity_lock_timeout, uint32_t, Uint, false, DEFAULT_INACTIVITY_LOCK_TIMEOUT);
    m_inactivity_lock_timeout = field_inactivity_lock_timeout;
    GET_FIELD_FROM_JSON_RETURN_ON_ERROR(json, setup_background_mining, BackgroundMiningSetupType, Int, false, BackgroundMiningMaybe);
    m_setup_background_mining = field_setup_background_mining;
    GET_FIELD_FROM_JSON_RETURN_ON_ERROR(json, subaddress_lookahead_major, uint32_t, Uint, false, SUBADDRESS_LOOKAHEAD_MAJOR);
    m_subaddress_lookahead_major = field_subaddress_lookahead_major;
    GET_FIELD_FROM_JSON_RETURN_ON_ERROR(json, subaddress_lookahead_minor, uint32_t, Uint, false, SUBADDRESS_LOOKAHEAD_MINOR);
    m_subaddress_lookahead_minor = field_subaddress_lookahead_minor;

    GET_FIELD_FROM_JSON_RETURN_ON_ERROR(json, encrypted_secret_keys, uint32_t, Uint, false, false);
    encrypted_secret_keys = field_encrypted_secret_keys;

    GET_FIELD_FROM_JSON_RETURN_ON_ERROR(json, export_format, ExportFormat, Int, false, Binary);
    m_export_format = field_export_format;

    GET_FIELD_FROM_JSON_RETURN_ON_ERROR(json, device_name, std::string, String, false, std::string());
    if (m_device_name.empty())
    {
      if (field_device_name_found)
      {
        m_device_name = field_device_name;
      }
      else
      {
        m_device_name = m_key_device_type == hw::device::device_type::LEDGER ? "Ledger" : "default";
      }
    }

    GET_FIELD_FROM_JSON_RETURN_ON_ERROR(json, device_derivation_path, std::string, String, false, std::string());
    m_device_derivation_path = field_device_derivation_path;
    
    if (json.HasMember("original_keys_available"))
    {
      GET_FIELD_FROM_JSON_RETURN_ON_ERROR(json, original_keys_available, int, Int, false, false);
      m_original_keys_available = field_original_keys_available;
      if (m_original_keys_available)
      {
        GET_FIELD_FROM_JSON_RETURN_ON_ERROR(json, original_address, std::string, String, true, std::string());
        address_parse_info info;
        bool ok = get_account_address_from_str(info, m_nettype, field_original_address);
        if (!ok)
        {
          LOG_ERROR("Failed to parse original_address from JSON");
          return false;
        }
        m_original_address = info.address;
        GET_FIELD_FROM_JSON_RETURN_ON_ERROR(json, original_view_secret_key, std::string, String, true, std::string());
        ok = epee::string_tools::hex_to_pod(field_original_view_secret_key, m_original_view_secret_key);
        if (!ok)
        {
          LOG_ERROR("Failed to parse original_view_secret_key from JSON");
          return false;
        }
      }
    }
    else
    {
      m_original_keys_available = false;
    }

    GET_FIELD_FROM_JSON_RETURN_ON_ERROR(json, enable_multisig, int, Int, false, false);
    m_enable_multisig = field_enable_multisig;

    GET_FIELD_FROM_JSON_RETURN_ON_ERROR(json, background_sync_type, BackgroundSyncType, Int, false, BackgroundSyncOff);
    m_background_sync_type = field_background_sync_type;

    // Load encryption key used to encrypt background cache
    crypto::chacha_key custom_background_key;
    m_custom_background_key = boost::none;
    if (m_background_sync_type == BackgroundSyncCustomPassword && !m_is_background_wallet)
    {
      if (!json.HasMember("custom_background_key"))
      {
        LOG_ERROR("Field custom_background_key not found in JSON");
        return false;
      }
      else if (!json["custom_background_key"].IsString())
      {
        LOG_ERROR("Field custom_background_key found in JSON, but not String");
        return false;
      }
      else if (json["custom_background_key"].GetStringLength() != sizeof(crypto::chacha_key))
      {
        LOG_ERROR("Field custom_background_key found in JSON, but not correct length");
        return false;
      }
      const char *field_custom_background_key = json["custom_background_key"].GetString();
      memcpy(custom_background_key.data(), field_custom_background_key, sizeof(crypto::chacha_key));
      m_custom_background_key = boost::optional<crypto::chacha_key>(custom_background_key);
      LOG_PRINT_L1("Loaded custom background key derived from custom password");
    }
    else if (json.HasMember("custom_background_key"))
    {
      LOG_ERROR("Unexpected field custom_background_key found in JSON");
    }
  }
  else
  {
    THROW_WALLET_EXCEPTION(error::wallet_internal_error, "invalid password");
    return false;
  }

  r = epee::serialization::load_t_from_binary(m_account, account_data);
  THROW_WALLET_EXCEPTION_IF(!r, error::invalid_password);
  if (m_key_device_type == hw::device::device_type::LEDGER || m_key_device_type == hw::device::device_type::TREZOR) {
    LOG_PRINT_L0("Account on device. Initing device...");
    hw::device &hwdev = lookup_device(m_device_name);
    THROW_WALLET_EXCEPTION_IF(!hwdev.set_name(m_device_name), error::wallet_internal_error, "Could not set device name " + m_device_name);
    hwdev.set_network_type(m_nettype);
    hwdev.set_derivation_path(m_device_derivation_path);
    hwdev.set_callback(get_device_callback());
    THROW_WALLET_EXCEPTION_IF(!hwdev.init(), error::wallet_internal_error, "Could not initialize the device " + m_device_name);
    THROW_WALLET_EXCEPTION_IF(!hwdev.connect(), error::wallet_internal_error, "Could not connect to the device " + m_device_name);
    m_account.set_device(hwdev);

    account_public_address device_account_public_address;
    bool fetch_device_address = true;

    ::hw::device_cold* dev_cold = nullptr;
    if (m_key_device_type == hw::device::device_type::TREZOR && (dev_cold = dynamic_cast<::hw::device_cold*>(&hwdev)) != nullptr) {
      THROW_WALLET_EXCEPTION_IF(!dev_cold->get_public_address_with_no_passphrase(device_account_public_address), error::wallet_internal_error, "Cannot get a device address");
      if (device_account_public_address == m_account.get_keys().m_account_address) {
        LOG_PRINT_L0("Wallet opened with an empty passphrase");
        fetch_device_address = false;
        dev_cold->set_use_empty_passphrase(true);
      } else {
        fetch_device_address = true;
        LOG_PRINT_L0("Wallet opening with an empty passphrase failed. Retry again: " << fetch_device_address);
        dev_cold->reset_session();
      }
    }

    if (fetch_device_address) {
      THROW_WALLET_EXCEPTION_IF(!hwdev.get_public_address(device_account_public_address), error::wallet_internal_error, "Cannot get a device address");
    }

    THROW_WALLET_EXCEPTION_IF(device_account_public_address != m_account.get_keys().m_account_address, error::wallet_internal_error, "Device wallet does not match wallet address. If the device uses the passphrase feature, please check whether the passphrase was entered correctly (it may have been misspelled - different passphrases generate different wallets, passphrase is case-sensitive). "
                                                                                                                                     "Device address: " + cryptonote::get_account_address_as_str(m_nettype, false, device_account_public_address) +
                                                                                                                                     ", wallet address: " + m_account.get_public_address_str(m_nettype));
    LOG_PRINT_L0("Device inited...");
  } else if (key_on_device()) {
    THROW_WALLET_EXCEPTION(error::wallet_internal_error, "hardware device not supported");
  }

  if (r)
  {
    if (encrypted_secret_keys)
    {
      m_account.decrypt_keys(key);
    }
    else
    {
      keys_to_encrypt = key;
    }
  }
  const cryptonote::account_keys& keys = m_account.get_keys();
  hw::device &hwdev = m_account.get_device();
  r = r && hwdev.verify_keys(keys.m_view_secret_key,  keys.m_account_address.m_view_public_key);
  if (!m_watch_only && !m_multisig && hwdev.device_protocol() != hw::device::PROTOCOL_COLD && !m_is_background_wallet)
    r = r && hwdev.verify_keys(keys.m_spend_secret_key, keys.m_account_address.m_spend_public_key);
  THROW_WALLET_EXCEPTION_IF(!r, error::wallet_files_doesnt_correspond, m_keys_file, m_wallet_file);

  if (r)
  {
    if (!m_is_background_wallet)
      setup_keys(password);
    else
      m_custom_background_key = boost::optional<crypto::chacha_key>(key);
  }

  return true;
}

/*!
 * \brief verify password for default wallet keys file.
 * \param password       Password to verify
 * \return               true if password is correct
 *
 * for verification only
 * should not mutate state, unlike load_keys()
 * can be used prior to rewriting wallet keys file, to ensure user has entered the correct password
 *
 */
bool wallet2::verify_password(const epee::wipeable_string& password, crypto::secret_key &spend_key_out)
{
  // this temporary unlocking is necessary for Windows (otherwise the file couldn't be loaded).
  unlock_keys_file();
  const bool no_spend_key = m_account.get_device().device_protocol() == hw::device::PROTOCOL_COLD || m_watch_only || m_multisig || m_is_background_wallet;
  bool r = verify_password(m_keys_file, password, no_spend_key, m_account.get_device(), m_kdf_rounds, spend_key_out);
  lock_keys_file();
  return r;
}

/*!
 * \brief verify password for specified wallet keys file.
 * \param keys_file_name  Keys file to verify password for
 * \param password        Password to verify
 * \param no_spend_key    If set = only verify view keys, otherwise also spend keys
 * \param hwdev           The hardware device to use
 * \return                true if password is correct
 *
 * for verification only
 * should not mutate state, unlike load_keys()
 * can be used prior to rewriting wallet keys file, to ensure user has entered the correct password
 *
 */
bool wallet2::verify_password(const std::string& keys_file_name, const epee::wipeable_string& password, bool no_spend_key, hw::device &hwdev, uint64_t kdf_rounds, crypto::secret_key &spend_key_out)
{
  rapidjson::Document json;
  wallet2::keys_file_data keys_file_data;
  std::string buf;
  bool encrypted_secret_keys = false;
  bool r = load_from_file(keys_file_name, buf);
  THROW_WALLET_EXCEPTION_IF(!r, error::file_read_error, keys_file_name);

  // Decrypt the contents
  r = ::serialization::parse_binary(buf, keys_file_data);
  THROW_WALLET_EXCEPTION_IF(!r, error::wallet_internal_error, "internal error: failed to deserialize \"" + keys_file_name + '\"');
  crypto::chacha_key key;
  crypto::generate_chacha_key(password.data(), password.size(), key, kdf_rounds);
  std::string account_data;
  account_data.resize(keys_file_data.account_data.size());
  crypto::chacha20(keys_file_data.account_data.data(), keys_file_data.account_data.size(), key, keys_file_data.iv, &account_data[0]);
  const bool try_v0_format = json.Parse(account_data.c_str()).HasParseError() || !json.IsObject();
  if (try_v0_format)
    crypto::chacha8(keys_file_data.account_data.data(), keys_file_data.account_data.size(), key, keys_file_data.iv, &account_data[0]);

  // Check if it's a background keys file if both of the above formats fail
  {
    cryptonote::account_base account_data_check;
    if (try_v0_format && !epee::serialization::load_t_from_binary(account_data_check, account_data))
    {
      get_custom_background_key(password, key, kdf_rounds);
      crypto::chacha20(keys_file_data.account_data.data(), keys_file_data.account_data.size(), key, keys_file_data.iv, &account_data[0]);
      const bool is_background_wallet = json.Parse(account_data.c_str()).HasParseError() && json.IsObject();
      no_spend_key = no_spend_key || is_background_wallet;
    }
  }

  // The contents should be JSON if the wallet follows the new format.
  if (json.Parse(account_data.c_str()).HasParseError())
  {
    // old format before JSON wallet key file format
  }
  else
  {
    account_data = std::string(json["key_data"].GetString(), json["key_data"].GetString() +
      json["key_data"].GetStringLength());
    GET_FIELD_FROM_JSON_RETURN_ON_ERROR(json, encrypted_secret_keys, uint32_t, Uint, false, false);
    encrypted_secret_keys = field_encrypted_secret_keys;
  }

  cryptonote::account_base account_data_check;

  r = epee::serialization::load_t_from_binary(account_data_check, account_data);

  if (encrypted_secret_keys)
    account_data_check.decrypt_keys(key);

  const cryptonote::account_keys& keys = account_data_check.get_keys();
  r = r && hwdev.verify_keys(keys.m_view_secret_key,  keys.m_account_address.m_view_public_key);
  if(!no_spend_key)
    r = r && hwdev.verify_keys(keys.m_spend_secret_key, keys.m_account_address.m_spend_public_key);
  spend_key_out = (!no_spend_key && r) ? keys.m_spend_secret_key : crypto::null_skey;
  return r;
}

bool wallet2::is_key_encryption_enabled() const
{
  return !is_unattended()
    && ask_password() == tools::wallet2::AskPasswordToDecrypt
    && !watch_only()
    && !is_background_syncing();
}

void wallet2::encrypt_keys(const crypto::chacha_key &key)
{
  m_account.encrypt_keys(key);
  m_account.decrypt_viewkey(key);
}

void wallet2::decrypt_keys(const crypto::chacha_key &key)
{
  verify_password_with_cached_key(key);

  m_account.encrypt_viewkey(key);
  m_account.decrypt_keys(key);
}

void wallet2::encrypt_keys(const epee::wipeable_string &password)
{
  crypto::chacha_key key;
  crypto::generate_chacha_key(password.data(), password.size(), key, m_kdf_rounds);
  encrypt_keys(key);
}

void wallet2::decrypt_keys(const epee::wipeable_string &password)
{
  crypto::chacha_key key;
  crypto::generate_chacha_key(password.data(), password.size(), key, m_kdf_rounds);
  decrypt_keys(key);
}

void wallet2::setup_new_blockchain()
{
  cryptonote::block b;
  generate_genesis(b);
  m_blockchain.push_back(get_block_hash(b));
  m_last_block_reward = cryptonote::get_outs_money_amount(b.miner_tx);
  add_subaddress_account(tr("Primary account"));
}

void wallet2::create_keys_file(const std::string &wallet_, bool watch_only, const epee::wipeable_string &password, bool create_address_file)
{
  if (!wallet_.empty())
  {
    bool r = store_keys(m_keys_file, password, watch_only);
    THROW_WALLET_EXCEPTION_IF(!r, error::file_save_error, m_keys_file);

    if (create_address_file)
    {
      r = save_to_file(m_wallet_file + ".address.txt", m_account.get_public_address_str(m_nettype), true);
      if(!r) MERROR("String with address text not saved");
    }
  }
}


/*!
 * \brief determine the key storage for the specified wallet file
 * \param device_type     (OUT) wallet backend as enumerated in hw::device::device_type
 * \param keys_file_name  Keys file to verify password for
 * \param password        Password to verify
 * \return                true if password correct, else false
 *
 * for verification only - determines key storage hardware
 *
 */
bool wallet2::query_device(hw::device::device_type& device_type, const std::string& keys_file_name, const epee::wipeable_string& password, uint64_t kdf_rounds)
{
  rapidjson::Document json;
  wallet2::keys_file_data keys_file_data;
  std::string buf;
  bool r = load_from_file(keys_file_name, buf);
  THROW_WALLET_EXCEPTION_IF(!r, error::file_read_error, keys_file_name);

  // Decrypt the contents
  r = ::serialization::parse_binary(buf, keys_file_data);
  THROW_WALLET_EXCEPTION_IF(!r, error::wallet_internal_error, "internal error: failed to deserialize \"" + keys_file_name + '\"');
  crypto::chacha_key key;
  crypto::generate_chacha_key(password.data(), password.size(), key, kdf_rounds);
  std::string account_data;
  account_data.resize(keys_file_data.account_data.size());
  crypto::chacha20(keys_file_data.account_data.data(), keys_file_data.account_data.size(), key, keys_file_data.iv, &account_data[0]);
  if (json.Parse(account_data.c_str()).HasParseError() || !json.IsObject())
    crypto::chacha8(keys_file_data.account_data.data(), keys_file_data.account_data.size(), key, keys_file_data.iv, &account_data[0]);

  device_type = hw::device::device_type::SOFTWARE;
  // The contents should be JSON if the wallet follows the new format.
  if (json.Parse(account_data.c_str()).HasParseError())
  {
    // old format before JSON wallet key file format
  }
  else
  {
    account_data = std::string(json["key_data"].GetString(), json["key_data"].GetString() +
      json["key_data"].GetStringLength());

    if (json.HasMember("key_on_device"))
    {
      GET_FIELD_FROM_JSON_RETURN_ON_ERROR(json, key_on_device, int, Int, false, hw::device::device_type::SOFTWARE);
      device_type = static_cast<hw::device::device_type>(field_key_on_device);
    }
  }

  cryptonote::account_base account_data_check;

  r = epee::serialization::load_t_from_binary(account_data_check, account_data);
  if (!r) return false;
  return true;
}

void wallet2::init_type(hw::device::device_type device_type)
{
  m_account_public_address = m_account.get_keys().m_account_address;
  m_watch_only = false;
  m_multisig = false;
  m_multisig_threshold = 0;
  m_multisig_signers.clear();
  m_original_keys_available = false;
  m_key_device_type = device_type;
}

/*!
 * \brief  Generates a wallet or restores one. Assumes the multisig setup
 *         has already completed for the provided multisig info.
 * \param  wallet_              Name of wallet file
 * \param  password             Password of wallet file
 * \param  multisig_data        The multisig restore info and keys
 * \param  create_address_file  Whether to create an address file
 */
void wallet2::generate(const std::string& wallet_, const epee::wipeable_string& password,
  const epee::wipeable_string& multisig_data, bool create_address_file)
{
  clear();
  prepare_file_names(wallet_);

  if (!wallet_.empty())
  {
    boost::system::error_code ignored_ec;
    THROW_WALLET_EXCEPTION_IF(boost::filesystem::exists(m_wallet_file, ignored_ec), error::file_exists, m_wallet_file);
    THROW_WALLET_EXCEPTION_IF(boost::filesystem::exists(m_keys_file,   ignored_ec), error::file_exists, m_keys_file);
  }

  m_account.generate(rct::rct2sk(rct::zero()), true, false);

  THROW_WALLET_EXCEPTION_IF(multisig_data.size() < 32, error::invalid_multisig_seed);
  size_t offset = 0;
  uint32_t threshold = *(uint32_t*)(multisig_data.data() + offset);
  offset += sizeof(uint32_t);
  uint32_t total = *(uint32_t*)(multisig_data.data() + offset);
  offset += sizeof(uint32_t);

  THROW_WALLET_EXCEPTION_IF(threshold < 1, error::invalid_multisig_seed);
  THROW_WALLET_EXCEPTION_IF(total < threshold, error::invalid_multisig_seed);
  THROW_WALLET_EXCEPTION_IF(threshold > 16, error::invalid_multisig_seed); // doing N choose (N - M + 1) might overflow
  const uint64_t n_multisig_keys = num_priv_multisig_keys_post_setup(threshold, total);
  THROW_WALLET_EXCEPTION_IF(multisig_data.size() != 8 + 32 * (4 + n_multisig_keys + total), error::invalid_multisig_seed);

  std::vector<crypto::secret_key> multisig_keys;
  std::vector<crypto::public_key> multisig_signers;
  crypto::secret_key spend_secret_key = *(crypto::secret_key*)(multisig_data.data() + offset);
  offset += sizeof(crypto::secret_key);
  crypto::public_key spend_public_key = *(crypto::public_key*)(multisig_data.data() + offset);
  offset += sizeof(crypto::public_key);
  crypto::secret_key view_secret_key = *(crypto::secret_key*)(multisig_data.data() + offset);
  offset += sizeof(crypto::secret_key);
  crypto::public_key view_public_key = *(crypto::public_key*)(multisig_data.data() + offset);
  offset += sizeof(crypto::public_key);
  for (size_t n = 0; n < n_multisig_keys; ++n)
  {
    multisig_keys.push_back(*(crypto::secret_key*)(multisig_data.data() + offset));
    offset += sizeof(crypto::secret_key);
  }
  for (size_t n = 0; n < total; ++n)
  {
    multisig_signers.push_back(*(crypto::public_key*)(multisig_data.data() + offset));
    offset += sizeof(crypto::public_key);
  }

  crypto::public_key calculated_view_public_key;
  THROW_WALLET_EXCEPTION_IF(!crypto::secret_key_to_public_key(view_secret_key, calculated_view_public_key), error::invalid_multisig_seed);
  THROW_WALLET_EXCEPTION_IF(view_public_key != calculated_view_public_key, error::invalid_multisig_seed);
  crypto::public_key local_signer;
  THROW_WALLET_EXCEPTION_IF(!crypto::secret_key_to_public_key(spend_secret_key, local_signer), error::invalid_multisig_seed);
  THROW_WALLET_EXCEPTION_IF(std::find(multisig_signers.begin(), multisig_signers.end(), local_signer) == multisig_signers.end(), error::invalid_multisig_seed);

  m_account.make_multisig(view_secret_key, spend_secret_key, spend_public_key, multisig_keys);

  // Not possible to restore a multisig wallet that is able to activate the MMS
  // (because the original keys are not (yet) part of the restore info), so
  // keep m_original_keys_available to false
  init_type(hw::device::device_type::SOFTWARE);
  m_multisig = true;
  m_multisig_threshold = threshold;
  m_multisig_signers = multisig_signers;
  // wallet is assumed already finalized
  m_multisig_rounds_passed = multisig::multisig_setup_rounds_required(m_multisig_signers.size(), m_multisig_threshold);
  setup_keys(password);

  create_keys_file(wallet_, false, password, m_nettype != MAINNET || create_address_file);
  setup_new_blockchain();

  if (!wallet_.empty())
    store();
}

/*!
 * \brief  Generates a wallet or restores one.
 * \param  wallet_                 Name of wallet file
 * \param  password                Password of wallet file
 * \param  recovery_param          If it is a restore, the recovery key
 * \param  recover                 Whether it is a restore
 * \param  two_random              Whether it is a non-deterministic wallet
 * \param  create_address_file     Whether to create an address file
 * \return                         The secret key of the generated wallet
 */
crypto::secret_key wallet2::generate(const std::string& wallet_, const epee::wipeable_string& password,
  const crypto::secret_key& recovery_param, bool recover, bool two_random, bool create_address_file)
{
  clear();
  prepare_file_names(wallet_);

  if (!wallet_.empty())
  {
    boost::system::error_code ignored_ec;
    THROW_WALLET_EXCEPTION_IF(boost::filesystem::exists(m_wallet_file, ignored_ec), error::file_exists, m_wallet_file);
    THROW_WALLET_EXCEPTION_IF(boost::filesystem::exists(m_keys_file,   ignored_ec), error::file_exists, m_keys_file);
  }

  crypto::secret_key retval = m_account.generate(recovery_param, recover, two_random);

  init_type(hw::device::device_type::SOFTWARE);
  setup_keys(password);

  // calculate a starting refresh height
  if(m_refresh_from_block_height == 0 && !recover){
    m_refresh_from_block_height = estimate_blockchain_height();
  }

  create_keys_file(wallet_, false, password, m_nettype != MAINNET || create_address_file);

  setup_new_blockchain();

  if (!wallet_.empty())
    store();

  return retval;
}

 uint64_t wallet2::estimate_blockchain_height()
 {
   // -1 month for fluctuations in block time and machine date/time setup.
   // avg seconds per block
   const int seconds_per_block = DIFFICULTY_TARGET_V2;
   // ~num blocks per month
   const uint64_t blocks_per_month = 60*60*24*30/seconds_per_block;

   // try asking the daemon first
   std::string err;
   uint64_t height = 0;

   // we get the max of approximated height and local height.
   // approximated height is the least of daemon target height
   // (the max of what the other daemons are claiming is their
   // height) and the theoretical height based on the local
   // clock. This will be wrong only if both the local clock
   // is bad *and* a peer daemon claims a highest height than
   // the real chain.
   // local height is the height the local daemon is currently
   // synced to, it will be lower than the real chain height if
   // the daemon is currently syncing.
   // If we use the approximate height we subtract one month as
   // a safety margin.
   height = get_approximate_blockchain_height();
   uint64_t target_height = get_daemon_blockchain_target_height(err);
   if (err.empty()) {
     if (target_height < height)
       height = target_height;
   } else {
     // if we couldn't talk to the daemon, check safety margin.
     if (height > blocks_per_month)
       height -= blocks_per_month;
     else
       height = 0;
   }
   uint64_t local_height = get_daemon_blockchain_height(err);
   if (err.empty() && local_height > height)
     height = local_height;
   return height;
 }

/*!
* \brief Creates a watch only wallet from a public address and a view secret key.
* \param  wallet_                 Name of wallet file
* \param  password                Password of wallet file
* \param  account_public_address  The account's public address
* \param  viewkey                 view secret key
* \param  create_address_file     Whether to create an address file
*/
void wallet2::generate(const std::string& wallet_, const epee::wipeable_string& password,
  const cryptonote::account_public_address &account_public_address,
  const crypto::secret_key& viewkey, bool create_address_file)
{
  clear();
  prepare_file_names(wallet_);

  if (!wallet_.empty())
  {
    boost::system::error_code ignored_ec;
    THROW_WALLET_EXCEPTION_IF(boost::filesystem::exists(m_wallet_file, ignored_ec), error::file_exists, m_wallet_file);
    THROW_WALLET_EXCEPTION_IF(boost::filesystem::exists(m_keys_file,   ignored_ec), error::file_exists, m_keys_file);
  }

  m_account.create_from_viewkey(account_public_address, viewkey);
  init_type(hw::device::device_type::SOFTWARE);
  m_watch_only = true;
  m_account_public_address = account_public_address;
  setup_keys(password);

  create_keys_file(wallet_, true, password, m_nettype != MAINNET || create_address_file);

  setup_new_blockchain();

  if (!wallet_.empty())
    store();
}

/*!
* \brief Creates a wallet from a public address and a spend/view secret key pair.
* \param  wallet_                 Name of wallet file
* \param  password                Password of wallet file
* \param  account_public_address  The account's public address
* \param  spendkey                spend secret key
* \param  viewkey                 view secret key
* \param  create_address_file     Whether to create an address file
*/
void wallet2::generate(const std::string& wallet_, const epee::wipeable_string& password,
  const cryptonote::account_public_address &account_public_address,
  const crypto::secret_key& spendkey, const crypto::secret_key& viewkey, bool create_address_file)
{
  clear();
  prepare_file_names(wallet_);

  if (!wallet_.empty())
  {
    boost::system::error_code ignored_ec;
    THROW_WALLET_EXCEPTION_IF(boost::filesystem::exists(m_wallet_file, ignored_ec), error::file_exists, m_wallet_file);
    THROW_WALLET_EXCEPTION_IF(boost::filesystem::exists(m_keys_file,   ignored_ec), error::file_exists, m_keys_file);
  }

  m_account.create_from_keys(account_public_address, spendkey, viewkey);
  init_type(hw::device::device_type::SOFTWARE);
  m_account_public_address = account_public_address;
  setup_keys(password);

  create_keys_file(wallet_, false, password, create_address_file);

  setup_new_blockchain();

  if (!wallet_.empty())
    store();
}

/*!
* \brief Creates a wallet from a device
* \param  wallet_        Name of wallet file
* \param  password       Password of wallet file
* \param  device_name    device string address
*/
void wallet2::restore(const std::string& wallet_, const epee::wipeable_string& password, const std::string &device_name, bool create_address_file)
{
  clear();
  prepare_file_names(wallet_);

  boost::system::error_code ignored_ec;
  if (!wallet_.empty()) {
    THROW_WALLET_EXCEPTION_IF(boost::filesystem::exists(m_wallet_file, ignored_ec), error::file_exists, m_wallet_file);
    THROW_WALLET_EXCEPTION_IF(boost::filesystem::exists(m_keys_file,   ignored_ec), error::file_exists, m_keys_file);
  }

  auto &hwdev = lookup_device(device_name);
  hwdev.set_name(device_name);
  hwdev.set_network_type(m_nettype);
  hwdev.set_derivation_path(m_device_derivation_path);
  hwdev.set_callback(get_device_callback());

  m_account.create_from_device(hwdev);
  init_type(m_account.get_device().get_type());
  setup_keys(password);
  m_device_name = device_name;

  create_keys_file(wallet_, false, password, m_nettype != MAINNET || create_address_file);
  if (m_subaddress_lookahead_major == SUBADDRESS_LOOKAHEAD_MAJOR && m_subaddress_lookahead_minor == SUBADDRESS_LOOKAHEAD_MINOR)
  {
    // the default lookahead setting (50:200) is clearly too much for hardware wallet
    m_subaddress_lookahead_major = 5;
    m_subaddress_lookahead_minor = 20;
  }
  setup_new_blockchain();
  if (!wallet_.empty()) {
    store();
  }
}
//----------------------------------------------------------------------------------------------------
void wallet2::get_uninitialized_multisig_account(multisig::multisig_account &account_out) const
{
  // create uninitialized multisig account
  account_out = multisig::multisig_account{
      // k_base = H(normal private spend key)
      multisig::get_multisig_blinded_secret_key(this->get_account().get_keys().m_spend_secret_key),
      // k_view = H(normal private view key)
      multisig::get_multisig_blinded_secret_key(this->get_account().get_keys().m_view_secret_key)
    };
}
//----------------------------------------------------------------------------------------------------
void wallet2::get_reconstructed_multisig_account(multisig::multisig_account &account_out) const
{
  const multisig::multisig_account_status ms_status{this->get_multisig_status()};
  CHECK_AND_ASSERT_THROW_MES(ms_status.multisig_is_active,
    "The wallet is not multisig, so the multisig account couldn't be reconstructed");

  // reconstruct multisig account
  crypto::public_key common_pubkey;
  crypto::secret_key_to_public_key(this->get_account().get_keys().m_view_secret_key, common_pubkey);

  multisig::multisig_keyset_map_memsafe_t kex_origins_map;
  for (const auto &derivation : m_multisig_derivations)
    kex_origins_map[derivation];

  account_out = multisig::multisig_account{
      m_multisig_threshold,
      m_multisig_signers,
      this->get_account().get_keys().m_spend_secret_key,
      this->get_account().get_keys().m_view_secret_key,
      this->get_account().get_keys().m_multisig_keys,
      this->get_account().get_keys().m_view_secret_key,
      m_account_public_address.m_spend_public_key,
      common_pubkey,
      m_multisig_rounds_passed,
      std::move(kex_origins_map),
      ""
    };
}
//----------------------------------------------------------------------------------------------------
std::string wallet2::make_multisig(const epee::wipeable_string &password,
  const std::vector<std::string> &initial_kex_msgs,
  const std::uint32_t threshold)
{
  // decrypt account keys
  std::optional<wallet_keys_unlocker> unlocker(std::in_place, *this, &password);

  // create multisig account
  multisig::multisig_account multisig_account;
  this->get_uninitialized_multisig_account(multisig_account);

  // open initial kex messages, validate them, extract signers
  std::vector<multisig::multisig_kex_msg> expanded_msgs;
  std::vector<crypto::public_key> signers;
  expanded_msgs.reserve(initial_kex_msgs.size());
  signers.reserve(initial_kex_msgs.size() + 1);

  for (const std::string &msg : initial_kex_msgs)
  {
    expanded_msgs.emplace_back(msg);

    // validate each message
    // 1. must be 'round 1'
    CHECK_AND_ASSERT_THROW_MES(expanded_msgs.back().get_round() == 1,
      "Trying to make multisig with message that has invalid multisig kex round (should be '1').");

    // 2. duplicate signers not allowed (the number of signers is implied by the number of initial kex messages passed
    //    in, so we can't just ignore duplicates here)
    CHECK_AND_ASSERT_THROW_MES(std::find(signers.begin(), signers.end(), expanded_msgs.back().get_signing_pubkey()) == signers.end(),
      "Duplicate signers not allowed when converting a wallet to multisig.");

    // add signer
    signers.push_back(expanded_msgs.back().get_signing_pubkey());
  }

  // expect that self is in the input list (this guarantees that the input list size always equals the number of intended
  //   signers for the account [when combined with duplicate checking])
  CHECK_AND_ASSERT_THROW_MES(std::find(signers.begin(), signers.end(), multisig_account.get_base_pubkey()) != signers.end(),
    "The local account's signer key was not found in initial multisig kex messages when converting a wallet to multisig.");

  // initialize key exchange
  multisig_account.initialize_kex(threshold, signers, expanded_msgs);
  CHECK_AND_ASSERT_THROW_MES(multisig_account.account_is_active(), "Failed to activate multisig account.");

  // update wallet state
  if (!m_original_keys_available)
  {
    // Save the original i.e. non-multisig keys so the MMS can continue to use them to encrypt and decrypt messages
    // (making a wallet multisig overwrites those keys, see account_base::make_multisig)
    m_original_address = this->get_account().get_keys().m_account_address;
    m_original_view_secret_key = this->get_account().get_keys().m_view_secret_key;
    m_original_keys_available = true;
  }

  // clear wallet caches
  this->clear();

  // account base
  MINFO("Creating multisig address...");
  CHECK_AND_ASSERT_THROW_MES(m_account.make_multisig(multisig_account.get_common_privkey(),
    multisig_account.get_base_privkey(),
    multisig_account.get_multisig_pubkey(),
    multisig_account.get_multisig_privkeys()),
      "Failed to create multisig wallet account due to bad keys");

  this->init_type(hw::device::device_type::SOFTWARE);
  m_original_keys_available = true;
  m_multisig = true;
  m_multisig_threshold = threshold;
  m_multisig_signers = signers;
  m_multisig_rounds_passed = 1;

  // derivations stored (note: should be empty in last kex round)
  m_multisig_derivations.clear();
  m_multisig_derivations.reserve(multisig_account.get_kex_keys_to_origins_map().size());

  for (const auto &key_to_origins : multisig_account.get_kex_keys_to_origins_map())
    m_multisig_derivations.push_back(key_to_origins.first);

  // address
  m_account_public_address.m_spend_public_key = multisig_account.get_multisig_pubkey();

  // re-encrypt keys
  unlocker.reset();

  if (!m_wallet_file.empty())
    this->create_keys_file(m_wallet_file, false, password, boost::filesystem::exists(m_wallet_file + ".address.txt"));

  this->setup_new_blockchain();

  if (!m_wallet_file.empty())
    this->store();

  return multisig_account.get_next_kex_round_msg();
}
//----------------------------------------------------------------------------------------------------
std::string wallet2::exchange_multisig_keys(const epee::wipeable_string &password,
  const std::vector<std::string> &kex_messages,
  const bool force_update_use_with_caution /*= false*/)
{
  const multisig::multisig_account_status ms_status{this->get_multisig_status()};
  CHECK_AND_ASSERT_THROW_MES(ms_status.multisig_is_active, "The wallet is not multisig");

  // decrypt account keys
  std::optional<wallet_keys_unlocker> unlocker(std::in_place, *this, &password);

  // reconstruct multisig account
  multisig::multisig_account multisig_account;
  this->get_reconstructed_multisig_account(multisig_account);

  // KLUDGE: early return if there are no kex messages and main kex is complete (will return the post-kex verification round
  //         message) (it's a kludge because this behavior would be more appropriate for a standalone wallet method)
  if (kex_messages.size() == 0)
  {
    CHECK_AND_ASSERT_THROW_MES(multisig_account.main_kex_rounds_done(),
      "Exchange multisig keys: there are no kex messages but the main kex rounds are not done.");

    return multisig_account.get_next_kex_round_msg();
  }

  // open kex messages
  std::vector<multisig::multisig_kex_msg> expanded_msgs;
  expanded_msgs.reserve(kex_messages.size());

  for (const std::string &msg : kex_messages)
    expanded_msgs.emplace_back(msg);

  // update multisig kex
  multisig_account.kex_update(expanded_msgs, force_update_use_with_caution);

  // update wallet state

  // address
  m_account_public_address.m_spend_public_key = multisig_account.get_multisig_pubkey();

  // account base
  CHECK_AND_ASSERT_THROW_MES(m_account.make_multisig(multisig_account.get_common_privkey(),
    multisig_account.get_base_privkey(),
    multisig_account.get_multisig_pubkey(),
    multisig_account.get_multisig_privkeys()),
      "Failed to update multisig wallet account due to bad keys");

  // derivations stored (should be empty in last round)
  m_multisig_derivations.clear();
  m_multisig_derivations.reserve(multisig_account.get_kex_keys_to_origins_map().size());

  for (const auto &key_to_origins : multisig_account.get_kex_keys_to_origins_map())
    m_multisig_derivations.push_back(key_to_origins.first);

  // rounds passed
  m_multisig_rounds_passed = multisig_account.get_kex_rounds_complete();

  // why is this necessary? who knows...
  if (multisig_account.multisig_is_ready())
  {
    // keys are encrypted again
    unlocker.reset();

    if (!m_wallet_file.empty())
    {
      bool r = this->store_keys(m_keys_file, password, false);
      THROW_WALLET_EXCEPTION_IF(!r, error::file_save_error, m_keys_file);

      if (boost::filesystem::exists(m_wallet_file + ".address.txt"))
      {
        r = this->save_to_file(m_wallet_file + ".address.txt", m_account.get_public_address_str(m_nettype), true);
        if(!r) MERROR("String with address text not saved");
      }
    }

    m_subaddresses.clear();
    m_subaddress_labels.clear();
    this->add_subaddress_account(tr("Primary account"));

    if (!m_wallet_file.empty())
      this->store();
  }

  // wallet/file relationship
  if (!m_wallet_file.empty())
    this->create_keys_file(m_wallet_file, false, password, boost::filesystem::exists(m_wallet_file + ".address.txt"));

  return multisig_account.get_next_kex_round_msg();
}
//----------------------------------------------------------------------------------------------------
std::string wallet2::get_multisig_first_kex_msg() const
{
  // create multisig account
  multisig::multisig_account multisig_account;
  this->get_uninitialized_multisig_account(multisig_account);

  return multisig_account.get_next_kex_round_msg();
}
//----------------------------------------------------------------------------------------------------
std::string wallet2::get_multisig_key_exchange_booster(const epee::wipeable_string &password,
  const std::vector<std::string> &kex_messages,
  const std::uint32_t threshold,
  const std::uint32_t num_signers)
{
  CHECK_AND_ASSERT_THROW_MES(kex_messages.size() > 0, "No key exchange messages passed in.");

  // decrypt account keys
  wallet_keys_unlocker unlocker(*this, &password);

  // prepare multisig account
  multisig::multisig_account multisig_account;

  const multisig::multisig_account_status ms_status{this->get_multisig_status()};
  CHECK_AND_ASSERT_THROW_MES(!ms_status.is_ready, "Multisig wallet creation process has already been finished.");

  if (ms_status.multisig_is_active)
  {
    // case: this wallet is in the middle of multisig key exchange
    // - boost the round that comes after the in-progress round

    CHECK_AND_ASSERT_THROW_MES(threshold == m_multisig_threshold,
      "Expected threshold does not match multisig wallet setting.");
    CHECK_AND_ASSERT_THROW_MES(num_signers == m_multisig_signers.size(),
      "Expected number of signers does not match multisig wallet setting.");

    // reconstruct multisig account
    this->get_reconstructed_multisig_account(multisig_account);
  }
  else
  {
    // case: make_multisig() has not been called
    // DANGER: If 'num_signers - threshold > 1', but this wallet's future multisig settings
    //         will be 'num_signers - threshold == 1', then the booster message WILL leak the
    //         future multisig wallet's private keys in this case where the wallet2 multisig wallet is uninitialized.

    this->get_uninitialized_multisig_account(multisig_account);
  }

  // open kex messages
  std::vector<multisig::multisig_kex_msg> expanded_msgs;
  expanded_msgs.reserve(kex_messages.size());

  for (const std::string &msg : kex_messages)
    expanded_msgs.emplace_back(msg);

  // get kex booster message
  // note: booster does not change wallet state other than decrypting/reencrypting account keys
  return multisig_account.get_multisig_kex_round_booster(threshold, num_signers, expanded_msgs).get_msg();
}
//----------------------------------------------------------------------------------------------------
multisig::multisig_account_status wallet2::get_multisig_status() const
{
  multisig::multisig_account_status ret;

  if (m_multisig)
  {
    ret.multisig_is_active = true;
    ret.threshold = m_multisig_threshold;
    ret.total = m_multisig_signers.size();
    ret.kex_is_done = !(get_account().get_keys().m_account_address.m_spend_public_key == rct::rct2pk(rct::identity())) &&
      (m_multisig_rounds_passed >= multisig::multisig_kex_rounds_required(m_multisig_signers.size(), m_multisig_threshold));
    ret.is_ready = ret.kex_is_done &&
      (m_multisig_rounds_passed == multisig::multisig_setup_rounds_required(m_multisig_signers.size(), m_multisig_threshold));
  }
  else
  {
    ret.multisig_is_active = false;
    ret.threshold = 0;
    ret.total = 0;
    ret.kex_is_done = false;
    ret.is_ready = false;
  }

  return ret;
}
//----------------------------------------------------------------------------------------------------
bool wallet2::has_multisig_partial_key_images() const
{
  if (!m_multisig)
    return false;
  for (const auto &td: m_transfers)
    if (td.m_key_image_partial)
      return true;
  return false;
}
//----------------------------------------------------------------------------------------------------
bool wallet2::has_unknown_key_images() const
{
  for (const auto &td: m_transfers)
    if (!td.m_key_image_known)
      return true;
  return false;
}

/*!
 * \brief Rewrites to the wallet file for wallet upgrade (doesn't generate key, assumes it's already there)
 * \param wallet_name Name of wallet file (should exist)
 * \param password    Password for wallet file
 */
void wallet2::rewrite(const std::string& wallet_name, const epee::wipeable_string& password)
{
  if (wallet_name.empty())
    return;
  THROW_WALLET_EXCEPTION_IF(m_background_syncing || m_is_background_wallet, error::wallet_internal_error,
    "cannot change wallet settings from background wallet");
  prepare_file_names(wallet_name);
  boost::system::error_code ignored_ec;
  THROW_WALLET_EXCEPTION_IF(!boost::filesystem::exists(m_keys_file, ignored_ec), error::file_not_found, m_keys_file);
  bool r = store_keys(m_keys_file, password, m_watch_only);
  THROW_WALLET_EXCEPTION_IF(!r, error::file_save_error, m_keys_file);

  // Update the background keys file when we rewrite the main wallet keys file
  if (m_background_sync_type == BackgroundSyncCustomPassword && m_custom_background_key)
  {
    const std::string background_keys_filename = make_background_keys_file_name(wallet_name);
    if (!lock_background_keys_file(background_keys_filename))
    {
      LOG_ERROR("Background keys file " << background_keys_filename << " is opened by another wallet program and cannot be rewritten");
      return; // not fatal, background keys file will just have different wallet settings
    }
    store_background_keys(m_custom_background_key.get());
    store_background_cache(m_custom_background_key.get(), true/*do_reset_background_sync_data*/);
  }
  else if (m_background_sync_type == BackgroundSyncReusePassword)
  {
    reset_background_sync_data(m_background_sync_data);
  }
}
/*!
 * \brief Writes to a file named based on the normal wallet (doesn't generate key, assumes it's already there)
 * \param wallet_name       Base name of wallet file
 * \param password          Password for wallet file
 * \param new_keys_filename [OUT] Name of new keys file
 */
void wallet2::write_watch_only_wallet(const std::string& wallet_name, const epee::wipeable_string& password, std::string &new_keys_filename)
{
  prepare_file_names(wallet_name);
  boost::system::error_code ignored_ec;
  new_keys_filename = m_wallet_file + "-watchonly.keys";
  bool watch_only_keys_file_exists = boost::filesystem::exists(new_keys_filename, ignored_ec);
  THROW_WALLET_EXCEPTION_IF(watch_only_keys_file_exists, error::file_save_error, new_keys_filename);
  bool r = store_keys(new_keys_filename, password, true);
  THROW_WALLET_EXCEPTION_IF(!r, error::file_save_error, new_keys_filename);
}
//----------------------------------------------------------------------------------------------------
void wallet2::wallet_exists(const std::string& file_path, bool& keys_file_exists, bool& wallet_file_exists)
{
  std::string keys_file, wallet_file, mms_file;
  do_prepare_file_names(file_path, keys_file, wallet_file, mms_file);

  boost::system::error_code ignore;
  keys_file_exists = boost::filesystem::exists(keys_file, ignore);
  wallet_file_exists = boost::filesystem::exists(wallet_file, ignore);
}
//----------------------------------------------------------------------------------------------------
bool wallet2::wallet_valid_path_format(const std::string& file_path)
{
  return !file_path.empty();
}
//----------------------------------------------------------------------------------------------------
std::string wallet2::make_background_wallet_file_name(const std::string &wallet_file)
{
  return wallet_file + BACKGROUND_WALLET_SUFFIX;
}
//----------------------------------------------------------------------------------------------------
std::string wallet2::make_background_keys_file_name(const std::string &wallet_file)
{
  return make_background_wallet_file_name(wallet_file) + ".keys";
}
//----------------------------------------------------------------------------------------------------
bool wallet2::parse_long_payment_id(const std::string& payment_id_str, crypto::hash& payment_id)
{
  cryptonote::blobdata payment_id_data;
  if(!epee::string_tools::parse_hexstr_to_binbuff(payment_id_str, payment_id_data))
    return false;

  if(sizeof(crypto::hash) != payment_id_data.size())
    return false;

  payment_id = *reinterpret_cast<const crypto::hash*>(payment_id_data.data());
  return true;
}
//----------------------------------------------------------------------------------------------------
bool wallet2::parse_short_payment_id(const std::string& payment_id_str, crypto::hash8& payment_id)
{
  cryptonote::blobdata payment_id_data;
  if(!epee::string_tools::parse_hexstr_to_binbuff(payment_id_str, payment_id_data))
    return false;

  if(sizeof(crypto::hash8) != payment_id_data.size())
    return false;

  payment_id = *reinterpret_cast<const crypto::hash8*>(payment_id_data.data());
  return true;
}
//----------------------------------------------------------------------------------------------------
bool wallet2::parse_payment_id(const std::string& payment_id_str, crypto::hash& payment_id)
{
  if (parse_long_payment_id(payment_id_str, payment_id))
    return true;
  crypto::hash8 payment_id8;
  if (parse_short_payment_id(payment_id_str, payment_id8))
  {
    memcpy(payment_id.data, payment_id8.data, 8);
    memset(payment_id.data + 8, 0, 24);
    return true;
  }
  return false;
}
//----------------------------------------------------------------------------------------------------
bool wallet2::prepare_file_names(const std::string& file_path)
{
  do_prepare_file_names(file_path, m_keys_file, m_wallet_file, m_mms_file);
  return true;
}
//----------------------------------------------------------------------------------------------------
bool wallet2::check_connection(uint32_t *version, bool *ssl, uint32_t timeout, bool *wallet_is_outdated, bool *daemon_is_outdated)
{
  THROW_WALLET_EXCEPTION_IF(!m_is_initialized, error::wallet_not_initialized);

  if (m_offline)
  {
    m_rpc_version = 0;
    if (version)
      *version = 0;
    if (ssl)
      *ssl = false;
    return false;
  }

  {
    boost::lock_guard<boost::recursive_mutex> lock(m_daemon_rpc_mutex);
    if(!m_http_client->is_connected(ssl))
    {
      m_rpc_version = 0;
      m_node_rpc_proxy.invalidate();
      if (!m_http_client->connect(std::chrono::milliseconds(timeout)))
        return false;
      if(!m_http_client->is_connected(ssl))
        return false;
    }
  }

  if (!m_rpc_version && !check_version(version, wallet_is_outdated, daemon_is_outdated))
    return false;
  if (version)
    *version = m_rpc_version;

  return true;
}
//----------------------------------------------------------------------------------------------------
bool wallet2::check_version(uint32_t *version, bool *wallet_is_outdated, bool *daemon_is_outdated)
{
  uint32_t rpc_version;
  std::vector<std::pair<uint8_t, uint64_t>> daemon_hard_forks;
  uint64_t height;
  uint64_t target_height;
  if (m_node_rpc_proxy.get_rpc_version(rpc_version, daemon_hard_forks, height, target_height))
  {
    if(version)
      *version = 0;
    return false;
  }

  // check wallet compatibility with daemon's hard fork version
  if (!m_allow_mismatched_daemon_version)
    if (!check_hard_fork_version(m_nettype, daemon_hard_forks, height, target_height, wallet_is_outdated, daemon_is_outdated))
      return false;

  m_rpc_version = rpc_version;
  return true;
}
//----------------------------------------------------------------------------------------------------
bool wallet2::check_hard_fork_version(cryptonote::network_type nettype, const std::vector<std::pair<uint8_t, uint64_t>> &daemon_hard_forks, const uint64_t height, const uint64_t target_height, bool *wallet_is_outdated, bool *daemon_is_outdated)
{
  const size_t wallet_num_hard_forks = nettype == TESTNET ? num_testnet_hard_forks
    : nettype == STAGENET ? num_stagenet_hard_forks : num_mainnet_hard_forks;
  const hardfork_t *wallet_hard_forks = nettype == TESTNET ? testnet_hard_forks
    : nettype == STAGENET ? stagenet_hard_forks : mainnet_hard_forks;

  // First check if wallet or daemon is outdated (whether either are unaware of
  // a hard fork). Then check if fork has passed rendering versions incompatible
  if (daemon_hard_forks.size() > 0)
  {
    bool daemon_outdated = daemon_hard_forks.size() < wallet_num_hard_forks;
    bool wallet_outdated = daemon_hard_forks.size() > wallet_num_hard_forks;

    if (daemon_is_outdated)
      *daemon_is_outdated = daemon_outdated;
    if (wallet_is_outdated)
      *wallet_is_outdated = wallet_outdated;

    if (daemon_outdated)
    {
      uint64_t daemon_missed_fork_height = wallet_hard_forks[daemon_hard_forks.size()].height;

      // If the daemon missed the fork, then technically it is no longer part of
      // the Monero network. Don't connect.
      bool daemon_missed_fork = height >= daemon_missed_fork_height || target_height >= daemon_missed_fork_height;
      if (daemon_missed_fork)
        return false;
    }
    else if (wallet_outdated)
    {
      uint64_t wallet_missed_fork_height = daemon_hard_forks[wallet_num_hard_forks].second;

      // If the wallet missed the fork, then technically it is no longer able
      // to communicate with the Monero network. Don't connect.
      bool wallet_missed_fork = height >= wallet_missed_fork_height || target_height >= wallet_missed_fork_height;
      if (wallet_missed_fork)
        return false;
    }
  }
  else
  {
    // Non-updated daemons won't return daemon_hard_forks in response to
    // get_version. Fall back to extra call to get_hard_fork_info by version.
    uint64_t daemon_fork_height;
    get_hard_fork_info(wallet_num_hard_forks-1/* wallet expects "double fork" pattern */, daemon_fork_height);
    bool daemon_outdated = daemon_fork_height == std::numeric_limits<uint64_t>::max();

    if (daemon_is_outdated)
      *daemon_is_outdated = daemon_outdated;

    if (daemon_outdated)
    {
      uint64_t daemon_missed_fork_height = wallet_hard_forks[wallet_num_hard_forks-2].height;
      bool daemon_missed_fork = height >= daemon_missed_fork_height || target_height >= daemon_missed_fork_height;
      if (daemon_missed_fork)
        return false;
    }

    // Don't need to check if wallet is outdated here because the daemons updated
    // for a future hard fork will serve daemon_hard_forks above. The check for
    // an outdated wallet is done above using daemon_hard_forks.
  }

  return true;
}
//----------------------------------------------------------------------------------------------------
void wallet2::set_offline(bool offline)
{
  m_offline = offline;
  m_node_rpc_proxy.set_offline(offline);
  m_http_client->set_auto_connect(!offline);
  if (offline)
  {
    boost::lock_guard<boost::recursive_mutex> lock(m_daemon_rpc_mutex);
    if(m_http_client->is_connected())
      m_http_client->disconnect();
  }
}
//----------------------------------------------------------------------------------------------------
bool wallet2::generate_chacha_key_from_secret_keys(crypto::chacha_key &key) const
{
  hw::device &hwdev =  m_account.get_device();
  return hwdev.generate_chacha_key(m_account.get_keys(), key, m_kdf_rounds);
}
//----------------------------------------------------------------------------------------------------
void wallet2::generate_chacha_key_from_password(const epee::wipeable_string &pass, crypto::chacha_key &key) const
{
  crypto::generate_chacha_key(pass.data(), pass.size(), key, m_kdf_rounds);
}
//----------------------------------------------------------------------------------------------------
void wallet2::load(const std::string& wallet_, const epee::wipeable_string& password, const std::string& keys_buf, const std::string& cache_buf)
{
  clear();
  prepare_file_names(wallet_);

  // determine if loading from file system or string buffer
  bool use_fs = !wallet_.empty();
  THROW_WALLET_EXCEPTION_IF((use_fs && !keys_buf.empty()) || (!use_fs && keys_buf.empty()), error::file_read_error, "must load keys either from file system or from buffer");\

  boost::system::error_code e;
  if (use_fs)
  {
    bool exists = boost::filesystem::exists(m_keys_file, e);
    THROW_WALLET_EXCEPTION_IF(e || !exists, error::file_not_found, m_keys_file);
    lock_keys_file();
    THROW_WALLET_EXCEPTION_IF(!is_keys_file_locked(), error::wallet_internal_error, "internal error: \"" + m_keys_file + "\" is opened by another wallet program");

    // this temporary unlocking is necessary for Windows (otherwise the file couldn't be loaded).
    unlock_keys_file();
    if (!load_keys(m_keys_file, password))
    {
      THROW_WALLET_EXCEPTION_IF(true, error::file_read_error, m_keys_file);
    }
    LOG_PRINT_L0("Loaded wallet keys file, with public address: " << m_account.get_public_address_str(m_nettype));
    lock_keys_file();
  }
  else if (!load_keys_buf(keys_buf, password))
  {
    THROW_WALLET_EXCEPTION_IF(true, error::file_read_error, "failed to load keys from buffer");
  }

  wallet_keys_unlocker unlocker(*this, &password);

  //keys loaded ok!
  //try to load wallet cache. but even if we failed, it is not big problem
  load_wallet_cache(use_fs, cache_buf);

  // Wallets used to wipe, but not erase, old unused multisig key info, which lead to huge memory leaks.
  // Here we erase these multisig keys if they're zero'd out to free up space.
  for (auto &td : m_transfers)
  {
    auto mk_it = td.m_multisig_k.begin();
    while (mk_it != td.m_multisig_k.end())
    {
      if (*mk_it == rct::zero())
        mk_it = td.m_multisig_k.erase(mk_it);
      else
        ++mk_it;
    }
  }

  cryptonote::block genesis;
  generate_genesis(genesis);
  crypto::hash genesis_hash = get_block_hash(genesis);

  if (m_blockchain.empty())
  {
    m_blockchain.push_back(genesis_hash);
    m_last_block_reward = cryptonote::get_outs_money_amount(genesis.miner_tx);
  }
  else
  {
    check_genesis(genesis_hash);
  }

  trim_hashchain();

  if (get_num_subaddress_accounts() == 0)
    add_subaddress_account(tr("Primary account"));

  try
  {
    if (use_fs)
      m_message_store.read_from_file(get_multisig_wallet_state(), m_mms_file);
  }
  catch (const std::exception &e)
  {
    MERROR("Failed to initialize MMS, it will be unusable");
  }

  try
  {
    if (use_fs)
      process_background_cache_on_open();
  }
  catch (const std::exception &e)
  {
    MERROR("Failed to process background cache on open: " << e.what());
  }
}
//----------------------------------------------------------------------------------------------------
void wallet2::load_wallet_cache(const bool use_fs, const std::string& cache_buf)
{
  boost::system::error_code e;
  bool cache_missing = use_fs ? (!boost::filesystem::exists(m_wallet_file, e) || e) : cache_buf.empty();
  if (cache_missing)
  {
    LOG_PRINT_L0("wallet cache missing: " << m_wallet_file << ", starting with empty blockchain");
    m_account_public_address = m_account.get_keys().m_account_address;
  }
  else
  {
    wallet2::cache_file_data cache_file_data;
    std::string cache_file_buf;
    bool r = true;
    if (use_fs)
    {
      r = load_from_file(m_wallet_file, cache_file_buf, std::numeric_limits<size_t>::max());
      THROW_WALLET_EXCEPTION_IF(!r, error::file_read_error, m_wallet_file);
    }

    // try to read it as an encrypted cache
    try
    {
      LOG_PRINT_L1("Trying to decrypt cache data");

      r = ::serialization::parse_binary(use_fs ? cache_file_buf : cache_buf, cache_file_data);
      THROW_WALLET_EXCEPTION_IF(!r, error::wallet_internal_error, "internal error: failed to deserialize \"" + m_wallet_file + '\"');
      std::string cache_data;
      cache_data.resize(cache_file_data.cache_data.size());
      crypto::chacha20(cache_file_data.cache_data.data(), cache_file_data.cache_data.size(), get_cache_key(), cache_file_data.iv, &cache_data[0]);

      try {
        bool loaded = false;

        try
        {
          binary_archive<false> ar{epee::strspan<std::uint8_t>(cache_data)};
          if (::serialization::serialize(ar, *this))
            if (::serialization::check_stream_state(ar))
              loaded = true;
          if (!loaded)
          {
            binary_archive<false> ar{epee::strspan<std::uint8_t>(cache_data)};
            ar.enable_varint_bug_backward_compatibility();
            if (::serialization::serialize(ar, *this))
              if (::serialization::check_stream_state(ar))
                loaded = true;
          }
        }
        catch(...) { }

        if (!loaded)
        {
          std::stringstream iss;
          iss << cache_data;
          boost::archive::portable_binary_iarchive ar(iss);
          ar >> *this;
        }
      }
      catch(...)
      {
        // try with previous scheme: direct from keys
        crypto::chacha_key key;
        generate_chacha_key_from_secret_keys(key);
        crypto::chacha20(cache_file_data.cache_data.data(), cache_file_data.cache_data.size(), key, cache_file_data.iv, &cache_data[0]);
        try {
          std::stringstream iss;
          iss << cache_data;
          boost::archive::portable_binary_iarchive ar(iss);
          ar >> *this;
        }
        catch (...)
        {
          crypto::chacha8(cache_file_data.cache_data.data(), cache_file_data.cache_data.size(), key, cache_file_data.iv, &cache_data[0]);
          try
          {
            std::stringstream iss;
            iss << cache_data;
            boost::archive::portable_binary_iarchive ar(iss);
            ar >> *this;
          }
          catch (...)
          {
            LOG_PRINT_L0("Failed to open portable binary, trying unportable");
            if (use_fs) tools::copy_file(m_wallet_file, m_wallet_file + ".unportable");
            std::stringstream iss;
            iss.str("");
            iss << cache_data;
            boost::archive::binary_iarchive ar(iss);
            ar >> *this;
          }
        }
      }
    }
    catch (...)
    {
      LOG_PRINT_L1("Failed to load encrypted cache, trying unencrypted");
      try {
        std::stringstream iss;
        iss << cache_file_buf;
        boost::archive::portable_binary_iarchive ar(iss);
        ar >> *this;
      }
      catch (...)
      {
        LOG_PRINT_L0("Failed to open portable binary, trying unportable");
        if (use_fs) tools::copy_file(m_wallet_file, m_wallet_file + ".unportable");
        std::stringstream iss;
        iss.str("");
        iss << cache_file_buf;
        boost::archive::binary_iarchive ar(iss);
        ar >> *this;
      }
    }
    THROW_WALLET_EXCEPTION_IF(
      m_account_public_address.m_spend_public_key != m_account.get_keys().m_account_address.m_spend_public_key ||
      m_account_public_address.m_view_public_key  != m_account.get_keys().m_account_address.m_view_public_key,
      error::wallet_files_doesnt_correspond, m_keys_file, m_wallet_file);
  }
}
//----------------------------------------------------------------------------------------------------
void wallet2::process_background_cache_on_open()
{
  if (m_wallet_file.empty())
    return;
  if (m_background_syncing || m_is_background_wallet)
    return;
  if (m_background_sync_type == BackgroundSyncOff)
    return;

  if (m_background_sync_type == BackgroundSyncReusePassword)
  {
    const background_sync_data_t background_sync_data = m_background_sync_data;
    const hashchain blockchain = m_blockchain;
    process_background_cache(background_sync_data, blockchain, m_last_block_reward);

    // Reset the background cache after processing
    reset_background_sync_data(m_background_sync_data);
  }
  else if (m_background_sync_type == BackgroundSyncCustomPassword)
  {
    // If the background wallet files don't exist, recreate them
    const std::string background_keys_file = make_background_keys_file_name(m_wallet_file);
    const std::string background_wallet_file = make_background_wallet_file_name(m_wallet_file);
    const bool background_keys_file_exists = boost::filesystem::exists(background_keys_file);
    const bool background_wallet_exists = boost::filesystem::exists(background_wallet_file);

    THROW_WALLET_EXCEPTION_IF(!lock_background_keys_file(background_keys_file), error::background_wallet_already_open, background_wallet_file);
    THROW_WALLET_EXCEPTION_IF(!m_custom_background_key, error::wallet_internal_error, "Custom background key not set");

    if (!background_keys_file_exists)
    {
      MDEBUG("Background keys file not found, restoring");
      store_background_keys(m_custom_background_key.get());
    }

    if (!background_wallet_exists)
    {
      MDEBUG("Background cache not found, restoring");
      store_background_cache(m_custom_background_key.get(), true/*do_reset_background_sync_data*/);
      return;
    }

    MDEBUG("Loading background cache");

    // Set up a minimal background wallet2 instance
    std::unique_ptr<wallet2> background_w2(new wallet2(m_nettype));
    background_w2->m_is_background_wallet = true;
    background_w2->m_background_syncing = true;
    background_w2->m_background_sync_type = m_background_sync_type;
    background_w2->m_custom_background_key = m_custom_background_key;

    cryptonote::account_base account = m_account;
    account.forget_spend_key();
    background_w2->m_account = account;

    // Load background cache from file
    background_w2->clear();
    background_w2->prepare_file_names(background_wallet_file);
    background_w2->load_wallet_cache(true/*use_fs*/);

    process_background_cache(background_w2->m_background_sync_data, background_w2->m_blockchain, background_w2->m_last_block_reward);

    // Reset the background cache after processing
    store_background_cache(m_custom_background_key.get(), true/*do_reset_background_sync_data*/);
  }
  else
  {
    THROW_WALLET_EXCEPTION(error::wallet_internal_error, "unknown background sync type");
  }
}
//----------------------------------------------------------------------------------------------------
void wallet2::trim_hashchain()
{
  uint64_t height = m_checkpoints.get_max_height();

  for (const transfer_details &td: m_transfers)
    if (td.m_block_height < height)
      height = td.m_block_height;

  if (!m_blockchain.empty() && m_blockchain.size() == m_blockchain.offset())
  {
    MINFO("Fixing empty hashchain");
    try
    {
      cryptonote::block_header_response block_header;
      if (m_node_rpc_proxy.get_block_header_by_height(m_blockchain.size() - 1, block_header))
        throw std::runtime_error("Failed to request block header by height");
      crypto::hash hash;
      epee::string_tools::hex_to_pod(block_header.hash, hash);
      m_blockchain.refill(hash);
    }
    catch(...)
    {
      MERROR("Failed to request block header from daemon, hash chain may be unable to sync till the wallet is loaded with a usable daemon");
    }
  }
  if (height > 0 && m_blockchain.size() > height)
  {
    --height;
    MDEBUG("trimming to " << height << ", offset " << m_blockchain.offset());
    m_blockchain.trim(height);
  }
}
//----------------------------------------------------------------------------------------------------
void wallet2::check_genesis(const crypto::hash& genesis_hash) const {
  std::string what("Genesis block mismatch. You probably use wallet without testnet (or stagenet) flag with blockchain from test (or stage) network or vice versa");

  THROW_WALLET_EXCEPTION_IF(genesis_hash != m_blockchain.genesis(), error::wallet_internal_error, what);
}
//----------------------------------------------------------------------------------------------------
std::string wallet2::path() const
{
  return m_wallet_file;
}
//----------------------------------------------------------------------------------------------------
void wallet2::store()
{
  if (!m_wallet_file.empty())
    store_to("", epee::wipeable_string());
}
//----------------------------------------------------------------------------------------------------
void wallet2::store_to(const std::string &path, const epee::wipeable_string &password, bool force_rewrite_keys)
{
  trim_hashchain();

  const bool had_old_wallet_files = !m_wallet_file.empty();
  THROW_WALLET_EXCEPTION_IF(!had_old_wallet_files && path.empty(), error::wallet_internal_error,
    "Cannot resave wallet to current file since wallet was not loaded from file to begin with");

  // if file is the same, we do:
  // 1. overwrite the keys file iff force_rewrite_keys is specified
  // 2. save cache to the *.new file
  // 3. rename *.new to wallet_name, replacing old cache file
  // else we do:
  // 1. prepare new file names with "path" variable
  // 2. store new keys files
  // 3. remove old keys file
  // 4. store new cache file
  // 5. remove old cache file

  // handle if we want just store wallet state to current files (ex store() replacement);
  bool same_file = had_old_wallet_files && path.empty();
  if (had_old_wallet_files && !path.empty())
  {
    const std::string canonical_old_path = boost::filesystem::canonical(m_wallet_file).string();
    const std::string canonical_new_path = boost::filesystem::weakly_canonical(path).string();
    same_file = canonical_old_path == canonical_new_path;
  }

  THROW_WALLET_EXCEPTION_IF(m_is_background_wallet && !same_file, error::wallet_internal_error,
    "Cannot save background wallet files to a different location");

  if (!same_file)
  {
    // check if we want to store to directory which doesn't exists yet
    boost::filesystem::path parent_path = boost::filesystem::path(path).parent_path();

    // if path is not exists, try to create it
    if (!parent_path.empty() &&  !boost::filesystem::exists(parent_path))
    {
      boost::system::error_code ec;
      if (!boost::filesystem::create_directories(parent_path, ec))
      {
        throw std::logic_error(ec.message());
      }
    }
  }
  else if (m_background_sync_type == BackgroundSyncCustomPassword && m_background_syncing && !m_is_background_wallet)
  {
    // We're background syncing, so store the wallet cache as a background cache
    // keeping the background sync data
    try
    {
      THROW_WALLET_EXCEPTION_IF(!m_custom_background_key, error::wallet_internal_error, "Custom background key not set");
      store_background_cache(m_custom_background_key.get(), false/*do_reset_background_sync_data*/);
    }
    catch (const std::exception &e)
    {
      MERROR("Failed to store background cache while background syncing: " << e.what());
    }
    return;
  }

  // get wallet cache data
  boost::optional<wallet2::cache_file_data> cache_file_data = get_cache_file_data();
  THROW_WALLET_EXCEPTION_IF(cache_file_data == boost::none, error::wallet_internal_error, "failed to generate wallet cache data");

  const std::string new_file = same_file ? m_wallet_file + ".new" : path;
  const std::string old_file = m_wallet_file;
  const std::string old_keys_file = m_keys_file;
  const std::string old_address_file = m_wallet_file + ".address.txt";
  const std::string old_mms_file = m_mms_file;

  if (!same_file)
  {
    prepare_file_names(path);
  }

  if (!same_file || force_rewrite_keys)
  {
    bool r = store_keys(m_keys_file, password, m_watch_only);
    THROW_WALLET_EXCEPTION_IF(!r, error::file_save_error, m_keys_file);
  }

  if (!same_file && had_old_wallet_files)
  {
    bool r = false;
    if (boost::filesystem::exists(old_address_file))
    {
      // save address to the new file
      const std::string address_file = m_wallet_file + ".address.txt";
      r = save_to_file(address_file, m_account.get_public_address_str(m_nettype), true);
      THROW_WALLET_EXCEPTION_IF(!r, error::file_save_error, m_wallet_file);
      // remove old address file
      r = boost::filesystem::remove(old_address_file);
      if (!r) {
        LOG_ERROR("error removing file: " << old_address_file);
      }
    }
    // remove old keys file
    r = boost::filesystem::remove(old_keys_file);
    if (!r) {
      LOG_ERROR("error removing file: " << old_keys_file);
    }
    // remove old message store file
    if (boost::filesystem::exists(old_mms_file))
    {
      r = boost::filesystem::remove(old_mms_file);
      if (!r) {
        LOG_ERROR("error removing file: " << old_mms_file);
      }
    }
  }

  // Save cache to new file. If storing to the same file, the temp path has the ".new" extension
#ifdef WIN32
    // On Windows avoid using std::ofstream which does not work with UTF-8 filenames
    // The price to pay is temporary higher memory consumption for string stream + binary archive
    std::ostringstream oss;
    binary_archive<true> oar(oss);
    bool success = ::serialization::serialize(oar, cache_file_data.get());
    if (success) {
        success = save_to_file(new_file, oss.str());
    }
    THROW_WALLET_EXCEPTION_IF(!success, error::file_save_error, new_file);
#else
    std::ofstream ostr;
    ostr.open(new_file, std::ios_base::binary | std::ios_base::out | std::ios_base::trunc);
    binary_archive<true> oar(ostr);
    bool success = ::serialization::serialize(oar, cache_file_data.get());
    ostr.close();
    THROW_WALLET_EXCEPTION_IF(!success || !ostr.good(), error::file_save_error, new_file);
#endif

  if (same_file)
  {
    // here we have "*.new" file, we need to rename it to be without ".new"
    std::error_code e = tools::replace_file(new_file, m_wallet_file);
    THROW_WALLET_EXCEPTION_IF(e, error::file_save_error, m_wallet_file, e);
  }
  else if (!same_file && had_old_wallet_files)
  {
    // remove old wallet file
    bool r = boost::filesystem::remove(old_file);
    if (!r) {
      LOG_ERROR("error removing file: " << old_file);
    }
  }
  
  if (m_message_store.get_active())
  {
    // While the "m_message_store" object of course always exist, a file for the message
    // store should only exist if the MMS is really active
    m_message_store.write_to_file(get_multisig_wallet_state(), m_mms_file);
  }

  if (m_background_sync_type == BackgroundSyncCustomPassword && !m_background_syncing && !m_is_background_wallet)
  {
    // Update the background wallet cache when we store the main wallet cache
    // Note: if background syncing when this is called, it means the background
    // wallet is open and was already stored above
    try
    {
      THROW_WALLET_EXCEPTION_IF(!m_custom_background_key, error::wallet_internal_error, "Custom background key not set");
      store_background_cache(m_custom_background_key.get(), true/*do_reset_background_sync_data*/);
    }
    catch (const std::exception &e)
    {
      MERROR("Failed to update background cache: " << e.what());
    }
  }
}
//----------------------------------------------------------------------------------------------------
boost::optional<wallet2::cache_file_data> wallet2::get_cache_file_data()
{
  trim_hashchain();
  try
  {
    std::stringstream oss;
    binary_archive<true> ar(oss);
    if (!::serialization::serialize(ar, *this))
      return boost::none;

    boost::optional<wallet2::cache_file_data> cache_file_data = (wallet2::cache_file_data) {};
    cache_file_data.get().cache_data = oss.str();
    std::string cipher;
    cipher.resize(cache_file_data.get().cache_data.size());
    cache_file_data.get().iv = crypto::rand<crypto::chacha_iv>();
    crypto::chacha20(cache_file_data.get().cache_data.data(), cache_file_data.get().cache_data.size(), get_cache_key(), cache_file_data.get().iv, &cipher[0]);
    cache_file_data.get().cache_data = cipher;
    return cache_file_data;
  }
  catch(...)
  {
    return boost::none;
  }
}
//----------------------------------------------------------------------------------------------------
uint64_t wallet2::balance(uint32_t index_major, bool strict) const
{
  uint64_t amount = 0;
  for (const auto& i : balance_per_subaddress(index_major, strict))
    amount += i.second;
  return amount;
}
//----------------------------------------------------------------------------------------------------
uint64_t wallet2::unlocked_balance(uint32_t index_major, bool strict, uint64_t *blocks_to_unlock, uint64_t *time_to_unlock)
{
  uint64_t amount = 0;
  if (blocks_to_unlock)
    *blocks_to_unlock = 0;
  if (time_to_unlock)
    *time_to_unlock = 0;
  for (const auto& i : unlocked_balance_per_subaddress(index_major, strict))
  {
    amount += i.second.first;
    if (blocks_to_unlock && i.second.second.first > *blocks_to_unlock)
      *blocks_to_unlock = i.second.second.first;
    if (time_to_unlock && i.second.second.second > *time_to_unlock)
      *time_to_unlock = i.second.second.second;
  }
  return amount;
}
//----------------------------------------------------------------------------------------------------
std::map<uint32_t, uint64_t> wallet2::balance_per_subaddress(uint32_t index_major, bool strict) const
{
  std::map<uint32_t, uint64_t> amount_per_subaddr;
  for (const auto& td: m_transfers)
  {
    if (td.amount() > m_ignore_outputs_above || td.amount() < m_ignore_outputs_below)
      continue;
    if (td.m_subaddr_index.major == index_major && !is_spent(td, strict) && !td.m_frozen)
    {
      auto found = amount_per_subaddr.find(td.m_subaddr_index.minor);
      if (found == amount_per_subaddr.end())
        amount_per_subaddr[td.m_subaddr_index.minor] = td.amount();
      else
        found->second += td.amount();
    }
  }
  if (!strict)
  {
   for (const auto& utx: m_unconfirmed_txs)
   {
    if (utx.second.m_subaddr_account == index_major && utx.second.m_state != wallet2::unconfirmed_transfer_details::failed)
    {
      // all changes go to 0-th subaddress (in the current subaddress account)
      auto found = amount_per_subaddr.find(0);
      if (found == amount_per_subaddr.end())
        amount_per_subaddr[0] = utx.second.m_change;
      else
        found->second += utx.second.m_change;

      // add transfers to same wallet
      for (const auto &dest: utx.second.m_dests) {
        auto index = get_subaddress_index(dest.addr);
        if (index && (*index).major == index_major)
        {
          auto found = amount_per_subaddr.find((*index).minor);
          if (found == amount_per_subaddr.end())
            amount_per_subaddr[(*index).minor] = dest.amount;
          else
            found->second += dest.amount;
        }
      }
    }
   }

   for (const auto& utx: m_unconfirmed_payments)
   {
    if (utx.second.m_pd.m_subaddr_index.major == index_major)
    {
      amount_per_subaddr[utx.second.m_pd.m_subaddr_index.minor] += utx.second.m_pd.m_amount;
    }
   }
  }
  return amount_per_subaddr;
}
//----------------------------------------------------------------------------------------------------
std::map<uint32_t, std::pair<uint64_t, std::pair<uint64_t, uint64_t>>> wallet2::unlocked_balance_per_subaddress(uint32_t index_major, bool strict)
{
  std::map<uint32_t, std::pair<uint64_t, std::pair<uint64_t, uint64_t>>> amount_per_subaddr;
  const uint64_t blockchain_height = get_blockchain_current_height();
  const uint64_t now = time(NULL);
  for(const transfer_details& td: m_transfers)
  {
    if (td.amount() > m_ignore_outputs_above || td.amount() < m_ignore_outputs_below)
      continue;
    if(td.m_subaddr_index.major == index_major && !is_spent(td, strict) && !td.m_frozen)
    {
      uint64_t amount = 0, blocks_to_unlock = 0, time_to_unlock = 0;
      if (is_transfer_unlocked(td))
      {
        amount = td.amount();
        blocks_to_unlock = 0;
        time_to_unlock = 0;
      }
      else
      {
        uint64_t unlock_height = td.m_block_height + std::max<uint64_t>(CRYPTONOTE_DEFAULT_TX_SPENDABLE_AGE, CRYPTONOTE_LOCKED_TX_ALLOWED_DELTA_BLOCKS);
        if (td.m_tx.unlock_time < CRYPTONOTE_MAX_BLOCK_NUMBER && td.m_tx.unlock_time > unlock_height)
          unlock_height = td.m_tx.unlock_time;
        uint64_t unlock_time = td.m_tx.unlock_time >= CRYPTONOTE_MAX_BLOCK_NUMBER ? td.m_tx.unlock_time : 0;
        blocks_to_unlock = unlock_height > blockchain_height ? unlock_height - blockchain_height : 0;
        time_to_unlock = unlock_time > now ? unlock_time - now : 0;
        amount = 0;
      }
      auto found = amount_per_subaddr.find(td.m_subaddr_index.minor);
      if (found == amount_per_subaddr.end())
        amount_per_subaddr[td.m_subaddr_index.minor] = std::make_pair(amount, std::make_pair(blocks_to_unlock, time_to_unlock));
      else
      {
        found->second.first += amount;
        found->second.second.first = std::max(found->second.second.first, blocks_to_unlock);
        found->second.second.second = std::max(found->second.second.second, time_to_unlock);
      }
    }
  }
  return amount_per_subaddr;
}
//----------------------------------------------------------------------------------------------------
uint64_t wallet2::balance_all(bool strict) const
{
  uint64_t r = 0;
  for (uint32_t index_major = 0; index_major < get_num_subaddress_accounts(); ++index_major)
    r += balance(index_major, strict);
  return r;
}
//----------------------------------------------------------------------------------------------------
uint64_t wallet2::unlocked_balance_all(bool strict, uint64_t *blocks_to_unlock, uint64_t *time_to_unlock)
{
  uint64_t r = 0;
  if (blocks_to_unlock)
    *blocks_to_unlock = 0;
  if (time_to_unlock)
    *time_to_unlock = 0;
  for (uint32_t index_major = 0; index_major < get_num_subaddress_accounts(); ++index_major)
  {
    uint64_t local_blocks_to_unlock, local_time_to_unlock;
    r += unlocked_balance(index_major, strict, blocks_to_unlock ? &local_blocks_to_unlock : NULL, time_to_unlock ? &local_time_to_unlock : NULL);
    if (blocks_to_unlock)
      *blocks_to_unlock = std::max(*blocks_to_unlock, local_blocks_to_unlock);
    if (time_to_unlock)
      *time_to_unlock = std::max(*time_to_unlock, local_time_to_unlock);
  }
  return r;
}
//----------------------------------------------------------------------------------------------------
void wallet2::get_transfers(wallet2::transfer_container& incoming_transfers) const
{
  incoming_transfers = m_transfers;
}
//----------------------------------------------------------------------------------------------------
void wallet2::get_payments(const crypto::hash& payment_id, std::list<wallet2::payment_details>& payments, uint64_t min_height, const boost::optional<uint32_t>& subaddr_account, const std::set<uint32_t>& subaddr_indices) const
{
  auto range = m_payments.equal_range(payment_id);
  std::for_each(range.first, range.second, [&payments, &min_height, &subaddr_account, &subaddr_indices](const payment_container::value_type& x) {
    if (min_height < x.second.m_block_height &&
      (!subaddr_account || *subaddr_account == x.second.m_subaddr_index.major) &&
      (subaddr_indices.empty() || subaddr_indices.count(x.second.m_subaddr_index.minor) == 1))
    {
      payments.push_back(x.second);
    }
  });
}
//----------------------------------------------------------------------------------------------------
void wallet2::get_payments(std::list<std::pair<crypto::hash,wallet2::payment_details>>& payments, uint64_t min_height, uint64_t max_height, const boost::optional<uint32_t>& subaddr_account, const std::set<uint32_t>& subaddr_indices) const
{
  auto range = std::make_pair(m_payments.begin(), m_payments.end());
  std::for_each(range.first, range.second, [&payments, &min_height, &max_height, &subaddr_account, &subaddr_indices](const payment_container::value_type& x) {
    if (min_height < x.second.m_block_height && max_height >= x.second.m_block_height &&
      (!subaddr_account || *subaddr_account == x.second.m_subaddr_index.major) &&
      (subaddr_indices.empty() || subaddr_indices.count(x.second.m_subaddr_index.minor) == 1))
    {
      payments.push_back(x);
    }
  });
}
//----------------------------------------------------------------------------------------------------
void wallet2::get_payments_out(std::list<std::pair<crypto::hash,wallet2::confirmed_transfer_details>>& confirmed_payments,
    uint64_t min_height, uint64_t max_height, const boost::optional<uint32_t>& subaddr_account, const std::set<uint32_t>& subaddr_indices) const
{
  for (auto i = m_confirmed_txs.begin(); i != m_confirmed_txs.end(); ++i) {
    if (i->second.m_block_height <= min_height || i->second.m_block_height > max_height)
      continue;
    if (subaddr_account && *subaddr_account != i->second.m_subaddr_account)
      continue;
    if (!subaddr_indices.empty() && std::count_if(i->second.m_subaddr_indices.begin(), i->second.m_subaddr_indices.end(), [&subaddr_indices](uint32_t index) { return subaddr_indices.count(index) == 1; }) == 0)
      continue;
    confirmed_payments.push_back(*i);
  }
}
//----------------------------------------------------------------------------------------------------
void wallet2::get_unconfirmed_payments_out(std::list<std::pair<crypto::hash,wallet2::unconfirmed_transfer_details>>& unconfirmed_payments, const boost::optional<uint32_t>& subaddr_account, const std::set<uint32_t>& subaddr_indices) const
{
  for (auto i = m_unconfirmed_txs.begin(); i != m_unconfirmed_txs.end(); ++i) {
    if (subaddr_account && *subaddr_account != i->second.m_subaddr_account)
      continue;
    if (!subaddr_indices.empty() && std::count_if(i->second.m_subaddr_indices.begin(), i->second.m_subaddr_indices.end(), [&subaddr_indices](uint32_t index) { return subaddr_indices.count(index) == 1; }) == 0)
      continue;
    unconfirmed_payments.push_back(*i);
  }
}
//----------------------------------------------------------------------------------------------------
void wallet2::get_unconfirmed_payments(std::list<std::pair<crypto::hash,wallet2::pool_payment_details>>& unconfirmed_payments, const boost::optional<uint32_t>& subaddr_account, const std::set<uint32_t>& subaddr_indices) const
{
  for (auto i = m_unconfirmed_payments.begin(); i != m_unconfirmed_payments.end(); ++i) {
    if ((!subaddr_account || *subaddr_account == i->second.m_pd.m_subaddr_index.major) &&
      (subaddr_indices.empty() || subaddr_indices.count(i->second.m_pd.m_subaddr_index.minor) == 1))
    unconfirmed_payments.push_back(*i);
  }
}
//----------------------------------------------------------------------------------------------------
void wallet2::rescan_spent()
{
  // This is RPC call that can take a long time if there are many outputs,
  // so we call it several times, in stripes, so we don't time out spuriously
  std::vector<int> spent_status;
  spent_status.reserve(m_transfers.size());
  const size_t chunk_size = 1000;
  for (size_t start_offset = 0; start_offset < m_transfers.size(); start_offset += chunk_size)
  {
    const size_t n_outputs = std::min<size_t>(chunk_size, m_transfers.size() - start_offset);
    MDEBUG("Calling is_key_image_spent on " << start_offset << " - " << (start_offset + n_outputs - 1) << ", out of " << m_transfers.size());
    COMMAND_RPC_IS_KEY_IMAGE_SPENT::request req = AUTO_VAL_INIT(req);
    COMMAND_RPC_IS_KEY_IMAGE_SPENT::response daemon_resp = AUTO_VAL_INIT(daemon_resp);
    for (size_t n = start_offset; n < start_offset + n_outputs; ++n)
      req.key_images.push_back(string_tools::pod_to_hex(m_transfers[n].m_key_image));

    {
      const boost::lock_guard<boost::recursive_mutex> lock{m_daemon_rpc_mutex};
      bool r = epee::net_utils::invoke_http_json("/is_key_image_spent", req, daemon_resp, *m_http_client, rpc_timeout);
      THROW_ON_RPC_RESPONSE_ERROR(r, {}, daemon_resp, "is_key_image_spent", error::is_key_image_spent_error, get_rpc_status(m_trusted_daemon, daemon_resp.status));
      THROW_WALLET_EXCEPTION_IF(daemon_resp.spent_status.size() != n_outputs, error::wallet_internal_error,
        "daemon returned wrong response for is_key_image_spent, wrong amounts count = " +
        std::to_string(daemon_resp.spent_status.size()) + ", expected " +  std::to_string(n_outputs));
    }

    std::copy(daemon_resp.spent_status.begin(), daemon_resp.spent_status.end(), std::back_inserter(spent_status));
  }

  // update spent status
  for (size_t i = 0; i < m_transfers.size(); ++i)
  {
    transfer_details& td = m_transfers[i];
    // a view wallet may not know about key images
    if (!td.m_key_image_known || td.m_key_image_partial)
      continue;
    if (td.m_spent != (spent_status[i] != COMMAND_RPC_IS_KEY_IMAGE_SPENT::UNSPENT))
    {
      if (td.m_spent)
      {
        LOG_PRINT_L0("Marking output " << i << "(" << td.m_key_image << ") as unspent, it was marked as spent");
        set_unspent(i);
        td.m_spent_height = 0;
      }
      else
      {
        LOG_PRINT_L0("Marking output " << i << "(" << td.m_key_image << ") as spent, it was marked as unspent");
        set_spent(i, td.m_spent_height);
        // unknown height, if this gets reorged, it might still be missed
      }
    }
  }
}
//----------------------------------------------------------------------------------------------------
void wallet2::rescan_blockchain(bool hard, bool refresh, bool keep_key_images)
{
  CHECK_AND_ASSERT_THROW_MES(!hard || !keep_key_images, "Cannot preserve key images on hard rescan");
  const size_t transfers_cnt = m_transfers.size();
  crypto::hash transfers_hash{};

  if(hard)
  {
    clear();
    setup_new_blockchain();
  }
  else
  {
    if (keep_key_images && refresh)
      hash_m_transfers((int64_t) transfers_cnt, transfers_hash);
    clear_soft(keep_key_images);
  }

  if (refresh)
    this->refresh(false);

  if (refresh && keep_key_images)
    finish_rescan_bc_keep_key_images(transfers_cnt, transfers_hash);
}
//----------------------------------------------------------------------------------------------------
bool wallet2::is_transfer_unlocked(const transfer_details& td)
{
  return is_transfer_unlocked(td.m_tx.unlock_time, td.m_block_height);
}
//----------------------------------------------------------------------------------------------------
bool wallet2::is_transfer_unlocked(uint64_t unlock_time, uint64_t block_height)
{
  if(!is_tx_spendtime_unlocked(unlock_time, block_height))
    return false;

  if(block_height + CRYPTONOTE_DEFAULT_TX_SPENDABLE_AGE > get_blockchain_current_height())
    return false;

  return true;
}
//----------------------------------------------------------------------------------------------------
bool wallet2::is_tx_spendtime_unlocked(uint64_t unlock_time, uint64_t block_height)
{
  if(unlock_time < CRYPTONOTE_MAX_BLOCK_NUMBER)
  {
    //interpret as block index
    if(get_blockchain_current_height()-1 + CRYPTONOTE_LOCKED_TX_ALLOWED_DELTA_BLOCKS >= unlock_time)
      return true;
    else
      return false;
  }else
  {
    //interpret as time
    uint64_t adjusted_time;
    try { adjusted_time = get_daemon_adjusted_time(); }
    catch(...) { adjusted_time = time(NULL); } // use local time if no daemon to report blockchain time
    // XXX: this needs to be fast, so we'd need to get the starting heights
    // from the daemon to be correct once voting kicks in
    uint64_t v2height = m_nettype == TESTNET ? 624634 : m_nettype == STAGENET ? 32000  : 1009827;
    uint64_t leeway = block_height < v2height ? CRYPTONOTE_LOCKED_TX_ALLOWED_DELTA_SECONDS_V1 : CRYPTONOTE_LOCKED_TX_ALLOWED_DELTA_SECONDS_V2;
    if(adjusted_time + leeway >= unlock_time)
      return true;
    else
      return false;
  }
  return false;
}
//----------------------------------------------------------------------------------------------------
namespace
{
  template<typename T>
  T pop_index(std::vector<T>& vec, size_t idx)
  {
    CHECK_AND_ASSERT_MES(!vec.empty(), T(), "Vector must be non-empty");
    CHECK_AND_ASSERT_MES(idx < vec.size(), T(), "idx out of bounds");

    T res = vec[idx];
    if (idx + 1 != vec.size())
    {
      vec[idx] = vec.back();
    }
    vec.resize(vec.size() - 1);

    return res;
  }

  template<typename T>
  T pop_random_value(std::vector<T>& vec)
  {
    CHECK_AND_ASSERT_MES(!vec.empty(), T(), "Vector must be non-empty");

    size_t idx = crypto::rand_idx(vec.size());
    return pop_index (vec, idx);
  }

  template<typename T>
  T pop_back(std::vector<T>& vec)
  {
    CHECK_AND_ASSERT_MES(!vec.empty(), T(), "Vector must be non-empty");

    T res = vec.back();
    vec.pop_back();
    return res;
  }

  template<typename T>
  void pop_if_present(std::vector<T>& vec, T e)
  {
    for (size_t i = 0; i < vec.size(); ++i)
    {
      if (e == vec[i])
      {
        pop_index (vec, i);
        return;
      }
    }
  }
}
//----------------------------------------------------------------------------------------------------
// This returns a handwavy estimation of how much two outputs are related
// If they're from the same tx, then they're fully related. From close block
// heights, they're kinda related. The actual values don't matter, just
// their ordering, but it could become more murky if we add scores later.
float wallet2::get_output_relatedness(const transfer_details &td0, const transfer_details &td1) const
{
  int dh;

  // expensive test, and same tx will fall onto the same block height below
  if (td0.m_txid == td1.m_txid)
    return 1.0f;

  // same block height -> possibly tx burst, or same tx (since above is disabled)
  dh = td0.m_block_height > td1.m_block_height ? td0.m_block_height - td1.m_block_height : td1.m_block_height - td0.m_block_height;
  if (dh == 0)
    return 0.9f;

  // adjacent blocks -> possibly tx burst
  if (dh == 1)
    return 0.8f;

  // could extract the payment id, and compare them, but this is a bit expensive too

  // similar block heights
  if (dh < 10)
    return 0.2f;

  // don't think these are particularly related
  return 0.0f;
}
//----------------------------------------------------------------------------------------------------
size_t wallet2::pop_best_value_from(const transfer_container &transfers, std::vector<size_t> &unused_indices, const std::vector<size_t>& selected_transfers, bool smallest) const
{
  std::vector<size_t> candidates;
  float best_relatedness = 1.0f;
  for (size_t n = 0; n < unused_indices.size(); ++n)
  {
    const transfer_details &candidate = transfers[unused_indices[n]];
    float relatedness = 0.0f;
    for (std::vector<size_t>::const_iterator i = selected_transfers.begin(); i != selected_transfers.end(); ++i)
    {
      float r = get_output_relatedness(candidate, transfers[*i]);
      if (r > relatedness)
      {
        relatedness = r;
        if (relatedness == 1.0f)
          break;
      }
    }

    if (relatedness < best_relatedness)
    {
      best_relatedness = relatedness;
      candidates.clear();
    }

    if (relatedness == best_relatedness)
      candidates.push_back(n);
  }

  // we have all the least related outputs in candidates, so we can pick either
  // the smallest, or a random one, depending on request
  size_t idx;
  if (smallest)
  {
    idx = 0;
    for (size_t n = 0; n < candidates.size(); ++n)
    {
      const transfer_details &td = transfers[unused_indices[candidates[n]]];
      if (td.amount() < transfers[unused_indices[candidates[idx]]].amount())
        idx = n;
    }
  }
  else
  {
    idx = crypto::rand_idx(candidates.size());
  }
  return pop_index (unused_indices, candidates[idx]);
}
//----------------------------------------------------------------------------------------------------
size_t wallet2::pop_best_value(std::vector<size_t> &unused_indices, const std::vector<size_t>& selected_transfers, bool smallest) const
{
  return pop_best_value_from(m_transfers, unused_indices, selected_transfers, smallest);
}
//----------------------------------------------------------------------------------------------------
// Select random input sources for transaction.
// returns:
//    direct return: amount of money found
//    modified reference: selected_transfers, a list of iterators/indices of input sources
uint64_t wallet2::select_transfers(uint64_t needed_money, std::vector<size_t> unused_transfers_indices, std::vector<size_t>& selected_transfers) const
{
  uint64_t found_money = 0;
  selected_transfers.reserve(unused_transfers_indices.size());
  while (found_money < needed_money && !unused_transfers_indices.empty())
  {
    size_t idx = pop_best_value(unused_transfers_indices, selected_transfers);

    const transfer_container::const_iterator it = m_transfers.begin() + idx;
    selected_transfers.push_back(idx);
    found_money += it->amount();
  }

  return found_money;
}
//----------------------------------------------------------------------------------------------------
void wallet2::add_unconfirmed_tx(const crypto::hash &txid, const cryptonote::transaction& tx, uint64_t amount_in, const std::vector<cryptonote::tx_destination_entry> &dests, const crypto::hash &payment_id, uint64_t change_amount, uint32_t subaddr_account, const std::set<uint32_t>& subaddr_indices)
{
  unconfirmed_transfer_details& utd = m_unconfirmed_txs[txid];
  utd.m_amount_in = amount_in;
  utd.m_amount_out = 0;
  for (const auto &d: dests)
    utd.m_amount_out += d.amount;
  utd.m_amount_out += change_amount; // dests does not contain change
  utd.m_change = change_amount;
  utd.m_sent_time = time(NULL);
  utd.m_tx = (const cryptonote::transaction_prefix&)tx;
  utd.m_dests = dests;
  utd.m_payment_id = payment_id;
  utd.m_state = wallet2::unconfirmed_transfer_details::pending;
  utd.m_timestamp = time(NULL);
  utd.m_subaddr_account = subaddr_account;
  utd.m_subaddr_indices = subaddr_indices;
  for (const auto &in: tx.vin)
  {
    if (in.type() != typeid(cryptonote::txin_to_key))
      continue;
    const auto &txin = boost::get<cryptonote::txin_to_key>(in);
    utd.m_rings.push_back(std::make_pair(txin.k_image, txin.key_offsets));
  }
}

//----------------------------------------------------------------------------------------------------
crypto::hash wallet2::get_payment_id(const pending_tx &ptx) const
{
  std::vector<tx_extra_field> tx_extra_fields;
  parse_tx_extra(ptx.tx.extra, tx_extra_fields); // ok if partially parsed
  tx_extra_nonce extra_nonce;
  crypto::hash payment_id = null_hash;
  if (find_tx_extra_field_by_type(tx_extra_fields, extra_nonce))
  {
    crypto::hash8 payment_id8 = null_hash8;
    if(get_encrypted_payment_id_from_tx_extra_nonce(extra_nonce.nonce, payment_id8))
    {
      if (ptx.dests.empty())
      {
        MWARNING("Encrypted payment id found, but no destinations public key, cannot decrypt");
        return crypto::null_hash;
      }
      if (ptx.tx_key == crypto::null_skey)
      {
        MWARNING("Encrypted payment id found, but no tx secret key, cannot decrypt");
        return crypto::null_hash;
      }
      if (m_account.get_device().decrypt_payment_id(payment_id8, ptx.dests[0].addr.m_view_public_key, ptx.tx_key))
      {
        memcpy(payment_id.data, payment_id8.data, 8);
      }
    }
    else if (!get_payment_id_from_tx_extra_nonce(extra_nonce.nonce, payment_id))
    {
      payment_id = crypto::null_hash;
    }
  }
  return payment_id;
}

//----------------------------------------------------------------------------------------------------
// take a pending tx and actually send it to the daemon
void wallet2::commit_tx(pending_tx& ptx)
{
  using namespace cryptonote;

  // Normal submit
  COMMAND_RPC_SEND_RAW_TX::request req;
  req.tx_as_hex = epee::string_tools::buff_to_hex_nodelimer(tx_to_blob(ptx.tx));
  req.do_not_relay = false;
  req.do_sanity_checks = true;
  COMMAND_RPC_SEND_RAW_TX::response daemon_send_resp;

  {
    const boost::lock_guard<boost::recursive_mutex> lock{m_daemon_rpc_mutex};
    bool r = epee::net_utils::invoke_http_json("/sendrawtransaction", req, daemon_send_resp, *m_http_client, rpc_timeout);
    THROW_ON_RPC_RESPONSE_ERROR(r, {}, daemon_send_resp, "sendrawtransaction", error::tx_rejected, ptx.tx, get_rpc_status(m_trusted_daemon, daemon_send_resp.status), get_text_reason(daemon_send_resp));
  }

  // sanity checks
  for (size_t idx: ptx.selected_transfers)
  {
    THROW_WALLET_EXCEPTION_IF(idx >= m_transfers.size(), error::wallet_internal_error,
        "Bad output index in selected transfers: " + boost::lexical_cast<std::string>(idx));
  }
  crypto::hash txid;

  txid = get_transaction_hash(ptx.tx);

  // if it's already processed, bail
  if (std::find_if(m_transfers.begin(), m_transfers.end(), [&txid](const transfer_details &td) { return td.m_txid == txid; }) != m_transfers.end())
  {
    MDEBUG("Transaction " << txid << " already processed");
    return;
  }
  if (m_unconfirmed_txs.find(txid) != m_unconfirmed_txs.end())
  {
    MDEBUG("Transaction " << txid << " already processed");
    return;
  }
  if (m_confirmed_txs.find(txid) != m_confirmed_txs.end())
  {
    MDEBUG("Transaction " << txid << " already processed");
    return;
  }

  crypto::hash payment_id = crypto::null_hash;
  std::vector<cryptonote::tx_destination_entry> dests;
  uint64_t amount_in = 0;
  if (store_tx_info())
  {
    payment_id = get_payment_id(ptx);
    dests = ptx.dests;
    for(size_t idx: ptx.selected_transfers)
      amount_in += m_transfers[idx].amount();
  }
  add_unconfirmed_tx(txid, ptx.tx, amount_in, dests, payment_id, ptx.change_dts.amount, ptx.construction_data.subaddr_account, ptx.construction_data.subaddr_indices);
  if (store_tx_info() && ptx.tx_key != crypto::null_skey)
  {
    m_tx_keys[txid] = ptx.tx_key;
    m_additional_tx_keys[txid] = ptx.additional_tx_keys;
  }

  LOG_PRINT_L2("transaction " << txid << " generated ok and sent to daemon, key_images: [" << ptx.key_images << "]");

  for(size_t idx: ptx.selected_transfers)
  {
    set_spent(idx, 0);
  }

  // tx generated, get rid of used k values
  for (size_t idx: ptx.selected_transfers)
  {
    memwipe(m_transfers[idx].m_multisig_k.data(), m_transfers[idx].m_multisig_k.size() * sizeof(m_transfers[idx].m_multisig_k[0]));
    m_transfers[idx].m_multisig_k.clear();
  }

  //fee includes dust if dust policy specified it.
  LOG_PRINT_L1("Transaction successfully sent. <" << txid << ">" << ENDL
            << "Commission: " << print_money(ptx.fee) << " (dust sent to dust addr: " << print_money((ptx.dust_added_to_fee ? 0 : ptx.dust)) << ")" << ENDL
            << "Balance: " << print_money(balance(ptx.construction_data.subaddr_account, false)) << ENDL
            << "Unlocked: " << print_money(unlocked_balance(ptx.construction_data.subaddr_account, false)) << ENDL
            << "Please, wait for confirmation for your balance to be unlocked.");
}

void wallet2::commit_tx(std::vector<pending_tx>& ptx_vector)
{
  for (auto & ptx : ptx_vector)
  {
    commit_tx(ptx);
  }
}
//----------------------------------------------------------------------------------------------------
bool wallet2::save_tx(const std::vector<pending_tx>& ptx_vector, const std::string &filename) const
{
  LOG_PRINT_L0("saving " << ptx_vector.size() << " transactions");
  std::string ciphertext = dump_tx_to_str(ptx_vector);
  if (ciphertext.empty())
    return false;
  return save_to_file(filename, ciphertext);
}
//----------------------------------------------------------------------------------------------------
std::string wallet2::dump_tx_to_str(const std::vector<pending_tx> &ptx_vector) const
{
  LOG_PRINT_L0("saving " << ptx_vector.size() << " transactions");
  unsigned_tx_set txs;
  for (auto &tx: ptx_vector)
  {
    // Short payment id is encrypted with tx_key. 
    // Since sign_tx() generates new tx_keys and encrypts the payment id, we need to save the decrypted payment ID
    // Save tx construction_data to unsigned_tx_set
    txs.txes.push_back(get_construction_data_with_decrypted_short_payment_id(tx, m_account.get_device()));
  }
  
  txs.new_transfers = export_outputs();
  // save as binary
  std::ostringstream oss;
  binary_archive<true> ar(oss);
  try
  {
    if (!::serialization::serialize(ar, txs))
      return std::string();
  }
  catch (...)
  {
    return std::string();
  }
  LOG_PRINT_L2("Saving unsigned tx data: " << oss.str());
  std::string ciphertext = encrypt_with_view_secret_key(oss.str());
  return std::string(UNSIGNED_TX_PREFIX) + ciphertext;
}
//----------------------------------------------------------------------------------------------------
bool wallet2::load_unsigned_tx(const std::string &unsigned_filename, unsigned_tx_set &exported_txs) const
{
  std::string s;
  boost::system::error_code errcode;

  if (!boost::filesystem::exists(unsigned_filename, errcode))
  {
    LOG_PRINT_L0("File " << unsigned_filename << " does not exist: " << errcode);
    return false;
  }
  if (!load_from_file(unsigned_filename.c_str(), s))
  {
    LOG_PRINT_L0("Failed to load from " << unsigned_filename);
    return false;
  }

  return parse_unsigned_tx_from_str(s, exported_txs);
}
//----------------------------------------------------------------------------------------------------
bool wallet2::parse_unsigned_tx_from_str(const std::string &unsigned_tx_st, unsigned_tx_set &exported_txs) const
{
  std::string s = unsigned_tx_st;
  const size_t magiclen = strlen(UNSIGNED_TX_PREFIX) - 1;
  if (strncmp(s.c_str(), UNSIGNED_TX_PREFIX, magiclen))
  {
    LOG_PRINT_L0("Bad magic from unsigned tx");
    return false;
  }
  s = s.substr(magiclen);
  const char version = s[0];
  s = s.substr(1);
  if (version == '\003')
  {
      LOG_PRINT_L0("Not loading deprecated format");
      return false;
  }
  else if (version == '\004')
  {
      LOG_PRINT_L0("Not loading deprecated format");
      return false;
  }
  else if (version == '\005')
  {
    try { s = decrypt_with_view_secret_key(s); }
    catch(const std::exception &e) { LOG_PRINT_L0("Failed to decrypt unsigned tx: " << e.what()); return false; }
    try
    {
      binary_archive<false> ar{epee::strspan<std::uint8_t>(s)};
      if (!::serialization::serialize(ar, exported_txs))
      {
        LOG_PRINT_L0("Failed to parse data from unsigned tx");
        return false;
      }
    }
    catch (...)
    {
      LOG_PRINT_L0("Failed to parse data from unsigned tx");
      return false;
    }
  }
  else
  {
    LOG_PRINT_L0("Unsupported version in unsigned tx");
    return false;
  }
  LOG_PRINT_L1("Loaded tx unsigned data from binary: " << exported_txs.txes.size() << " transactions");

  return true;
}
//----------------------------------------------------------------------------------------------------
bool wallet2::sign_tx(const std::string &unsigned_filename, const std::string &signed_filename, std::vector<wallet2::pending_tx> &txs, std::function<bool(const unsigned_tx_set&)> accept_func, bool export_raw)
{
  unsigned_tx_set exported_txs;
  if(!load_unsigned_tx(unsigned_filename, exported_txs))
    return false;
  
  if (accept_func && !accept_func(exported_txs))
  {
    LOG_PRINT_L1("Transactions rejected by callback");
    return false;
  }
  return sign_tx(exported_txs, signed_filename, txs, export_raw);
}
//----------------------------------------------------------------------------------------------------
bool wallet2::sign_tx(unsigned_tx_set &exported_txs, std::vector<wallet2::pending_tx> &txs, signed_tx_set &signed_txes)
{
  if (!std::get<2>(exported_txs.new_transfers).empty())
    import_outputs(exported_txs.new_transfers);
  else if (!std::get<2>(exported_txs.transfers).empty())
    import_outputs(exported_txs.transfers);

  // sign the transactions
  for (size_t n = 0; n < exported_txs.txes.size(); ++n)
  {
    tools::wallet2::tx_construction_data &sd = exported_txs.txes[n];
    THROW_WALLET_EXCEPTION_IF(sd.sources.empty(), error::wallet_internal_error, "Empty sources");
    THROW_WALLET_EXCEPTION_IF(sd.unlock_time, error::nonzero_unlock_time);
    LOG_PRINT_L1(" " << (n+1) << ": " << sd.sources.size() << " inputs, ring size " << sd.sources[0].outputs.size());
    signed_txes.ptx.push_back(pending_tx());
    tools::wallet2::pending_tx &ptx = signed_txes.ptx.back();
    rct::RCTConfig rct_config = sd.rct_config;
    crypto::secret_key tx_key;
    std::vector<crypto::secret_key> additional_tx_keys;
    bool r = cryptonote::construct_tx_and_get_tx_key(m_account.get_keys(), m_subaddresses, sd.sources, sd.splitted_dsts, sd.change_dts.addr, sd.extra, ptx.tx, tx_key, additional_tx_keys, sd.use_rct, rct_config, sd.use_view_tags);
    THROW_WALLET_EXCEPTION_IF(!r, error::tx_not_constructed, sd.sources, sd.splitted_dsts, m_nettype);
    // we don't test tx size, because we don't know the current limit, due to not having a blockchain,
    // and it's a bit pointless to fail there anyway, since it'd be a (good) guess only. We sign anyway,
    // and if we really go over limit, the daemon will reject when it gets submitted. Chances are it's
    // OK anyway since it was generated in the first place, and rerolling should be within a few bytes.

    // normally, the tx keys are saved in commit_tx, when the tx is actually sent to the daemon.
    // we can't do that here since the tx will be sent from the compromised wallet, which we don't want
    // to see that info, so we save it here
    if (store_tx_info() && tx_key != crypto::null_skey)
    {
      const crypto::hash txid = get_transaction_hash(ptx.tx);
      m_tx_keys[txid] = tx_key;
      m_additional_tx_keys[txid] = additional_tx_keys;
    }

    std::string key_images;
    bool all_are_txin_to_key = std::all_of(ptx.tx.vin.begin(), ptx.tx.vin.end(), [&](const txin_v& s_e) -> bool
    {
      CHECKED_GET_SPECIFIC_VARIANT(s_e, const txin_to_key, in, false);
      key_images += boost::to_string(in.k_image) + " ";
      return true;
    });
    THROW_WALLET_EXCEPTION_IF(!all_are_txin_to_key, error::unexpected_txin_type, ptx.tx);

    ptx.key_images = key_images;
    ptx.fee = 0;
    for (const auto &i: sd.sources) ptx.fee += i.amount;
    for (const auto &i: sd.splitted_dsts) ptx.fee -= i.amount;
    ptx.dust = 0;
    ptx.dust_added_to_fee = false;
    ptx.change_dts = sd.change_dts;
    ptx.selected_transfers = sd.selected_transfers;
    ptx.tx_key = rct::rct2sk(rct::identity()); // don't send it back to the untrusted view wallet
    ptx.dests = sd.dests;
    ptx.construction_data = sd;

    txs.push_back(ptx);

    // add tx keys only to ptx
    txs.back().tx_key = tx_key;
    txs.back().additional_tx_keys = additional_tx_keys;
  }

  // add key image mapping for these txes
  const account_keys &keys = get_account().get_keys();
  hw::device &hwdev = m_account.get_device();
  for (size_t n = 0; n < exported_txs.txes.size(); ++n)
  {
    const cryptonote::transaction &tx = signed_txes.ptx[n].tx;

    crypto::key_derivation derivation;
    std::vector<crypto::key_derivation> additional_derivations;

    crypto::public_key tx_pub_key = get_tx_pub_key_from_extra(tx);
    std::vector<crypto::public_key> additional_tx_pub_keys;
    for (const crypto::secret_key &skey: txs[n].additional_tx_keys)
    {
      additional_tx_pub_keys.resize(additional_tx_pub_keys.size() + 1);
      crypto::secret_key_to_public_key(skey, additional_tx_pub_keys.back());
    }

    // compute derivations
    hwdev.set_mode(hw::device::TRANSACTION_PARSE);
    if (!hwdev.generate_key_derivation(tx_pub_key, keys.m_view_secret_key, derivation))
    {
      MWARNING("Failed to generate key derivation from tx pubkey in " << cryptonote::get_transaction_hash(tx) << ", skipping");
      static_assert(sizeof(derivation) == sizeof(rct::key), "Mismatched sizes of key_derivation and rct::key");
      memcpy(&derivation, rct::identity().bytes, sizeof(derivation));
    }
    for (size_t i = 0; i < additional_tx_pub_keys.size(); ++i)
    {
      additional_derivations.push_back({});
      if (!hwdev.generate_key_derivation(additional_tx_pub_keys[i], keys.m_view_secret_key, additional_derivations.back()))
      {
        MWARNING("Failed to generate key derivation from additional tx pubkey in " << cryptonote::get_transaction_hash(tx) << ", skipping");
        memcpy(&additional_derivations.back(), rct::identity().bytes, sizeof(crypto::key_derivation));
      }
    }

    for (size_t i = 0; i < tx.vout.size(); ++i)
    {
      crypto::public_key output_public_key;
      if (!get_output_public_key(tx.vout[i], output_public_key))
        continue;

      // if this output is back to this wallet, we can calculate its key image already
      if (!is_out_to_acc_precomp(m_subaddresses, output_public_key, derivation, additional_derivations, i, hwdev, get_output_view_tag(tx.vout[i])))
        continue;
      crypto::key_image ki;
      cryptonote::keypair in_ephemeral;
      if (generate_key_image_helper(keys, m_subaddresses, output_public_key, tx_pub_key, additional_tx_pub_keys, i, in_ephemeral, ki, hwdev))
        signed_txes.tx_key_images[output_public_key] = ki;
      else
        MERROR("Failed to calculate key image");
    }
  }

  // add key images
  signed_txes.key_images.resize(m_transfers.size());
  for (size_t i = 0; i < m_transfers.size(); ++i)
  {
    if (!m_transfers[i].m_key_image_known || m_transfers[i].m_key_image_partial)
      LOG_PRINT_L0("WARNING: key image not known in signing wallet at index " << i);
    signed_txes.key_images[i] = m_transfers[i].m_key_image;
  }

  return true;
}
//----------------------------------------------------------------------------------------------------
bool wallet2::sign_tx(unsigned_tx_set &exported_txs, const std::string &signed_filename, std::vector<wallet2::pending_tx> &txs, bool export_raw)
{
  // sign the transactions
  signed_tx_set signed_txes;
  std::string ciphertext = sign_tx_dump_to_str(exported_txs, txs, signed_txes);
  if (ciphertext.empty())
  {
    LOG_PRINT_L0("Failed to sign unsigned_tx_set");
    return false;
  }

  if (!save_to_file(signed_filename, ciphertext))
  {
    LOG_PRINT_L0("Failed to save file to " << signed_filename);
    return false;
  }
  // export signed raw tx without encryption
  if (export_raw)
  {
    for (size_t i = 0; i < signed_txes.ptx.size(); ++i)
    {
      std::string tx_as_hex = epee::string_tools::buff_to_hex_nodelimer(tx_to_blob(signed_txes.ptx[i].tx));
      std::string raw_filename = signed_filename + "_raw" + (signed_txes.ptx.size() == 1 ? "" : ("_" + std::to_string(i)));
      if (!save_to_file(raw_filename, tx_as_hex))
      {
        LOG_PRINT_L0("Failed to save file to " << raw_filename);
        return false;
      }
    }
  }
  return true;
}
//----------------------------------------------------------------------------------------------------
std::string wallet2::sign_tx_dump_to_str(unsigned_tx_set &exported_txs, std::vector<wallet2::pending_tx> &ptx, signed_tx_set &signed_txes)
{
  // sign the transactions
  bool r = sign_tx(exported_txs, ptx, signed_txes);
  if (!r)
  {
    LOG_PRINT_L0("Failed to sign unsigned_tx_set");
    return std::string();
  }

  // save as binary
  std::ostringstream oss;
  binary_archive<true> ar(oss);
  try
  {
    if (!::serialization::serialize(ar, signed_txes))
      return std::string();
  }
  catch(...)
  {
    return std::string();
  }
  LOG_PRINT_L3("Saving signed tx data (with encryption): " << oss.str());
  std::string ciphertext = encrypt_with_view_secret_key(oss.str());
  return std::string(SIGNED_TX_PREFIX) + ciphertext;
}
//----------------------------------------------------------------------------------------------------
bool wallet2::load_tx(const std::string &signed_filename, std::vector<tools::wallet2::pending_tx> &ptx, std::function<bool(const signed_tx_set&)> accept_func)
{
  std::string s;
  boost::system::error_code errcode;
  signed_tx_set signed_txs;

  if (!boost::filesystem::exists(signed_filename, errcode))
  {
    LOG_PRINT_L0("File " << signed_filename << " does not exist: " << errcode);
    return false;
  }

  if (!load_from_file(signed_filename.c_str(), s))
  {
    LOG_PRINT_L0("Failed to load from " << signed_filename);
    return false;
  }

  return parse_tx_from_str(s, ptx, accept_func);
}
//----------------------------------------------------------------------------------------------------
bool wallet2::parse_tx_from_str(const std::string &signed_tx_st, std::vector<tools::wallet2::pending_tx> &ptx, std::function<bool(const signed_tx_set &)> accept_func)
{
  std::string s = signed_tx_st;
  signed_tx_set signed_txs;

  const size_t magiclen = strlen(SIGNED_TX_PREFIX) - 1;
  if (strncmp(s.c_str(), SIGNED_TX_PREFIX, magiclen))
  {
    LOG_PRINT_L0("Bad magic from signed transaction");
    return false;
  }
  s = s.substr(magiclen);
  const char version = s[0];
  s = s.substr(1);
  if (version == '\003')
  {
      LOG_PRINT_L0("Not loading deprecated format");
      return false;
  }
  else if (version == '\004')
  {
      LOG_PRINT_L0("Not loading deprecated format");
      return false;
  }
  else if (version == '\005')
  {
    try { s = decrypt_with_view_secret_key(s); }
    catch (const std::exception &e) { LOG_PRINT_L0("Failed to decrypt signed transaction: " << e.what()); return false; }
    try
    {
      binary_archive<false> ar{epee::strspan<std::uint8_t>(s)};
      if (!::serialization::serialize(ar, signed_txs))
      {
        LOG_PRINT_L0("Failed to deserialize signed transaction");
        return false;
      }
    }
    catch (const std::exception &e)
    {
      LOG_PRINT_L0("Failed to decrypt signed transaction: " << e.what());
      return false;
    }
  }
  else
  {
    LOG_PRINT_L0("Unsupported version in signed transaction");
    return false;
  }
  LOG_PRINT_L0("Loaded signed tx data from binary: " << signed_txs.ptx.size() << " transactions");
  for (auto &c_ptx: signed_txs.ptx) LOG_PRINT_L0(cryptonote::obj_to_json_str(c_ptx.tx));

  if (accept_func && !accept_func(signed_txs))
  {
    LOG_PRINT_L1("Transactions rejected by callback");
    return false;
  }

  // import key images
  bool r = import_key_images(signed_txs.key_images);
  if (!r) return false;

  // remember key images for this tx, for when we get those txes from the blockchain
  for (const auto &e: signed_txs.tx_key_images)
    m_cold_key_images.insert(e);

  ptx = signed_txs.ptx;

  return true;
}
//----------------------------------------------------------------------------------------------------
std::string wallet2::save_multisig_tx(multisig_tx_set txs)
{
  LOG_PRINT_L0("saving " << txs.m_ptx.size() << " multisig transactions");

  // txes generated, get rid of used k values
  for (size_t n = 0; n < txs.m_ptx.size(); ++n)
    for (size_t idx: txs.m_ptx[n].construction_data.selected_transfers)
    {
      memwipe(m_transfers[idx].m_multisig_k.data(), m_transfers[idx].m_multisig_k.size() * sizeof(m_transfers[idx].m_multisig_k[0]));
      m_transfers[idx].m_multisig_k.clear();
    }

  // zero out some data we don't want to share
  for (auto &ptx: txs.m_ptx)
  {
    for (auto &e: ptx.construction_data.sources)
      memwipe(&e.multisig_kLRki.k, sizeof(e.multisig_kLRki.k));
  }

  for (auto &ptx: txs.m_ptx)
  {
    // Get decrypted payment id from pending_tx
    ptx.construction_data = get_construction_data_with_decrypted_short_payment_id(ptx, m_account.get_device());
  }

  // save as binary
  std::ostringstream oss;
  binary_archive<true> ar(oss);
  try
  {
    if (!::serialization::serialize(ar, txs))
      return std::string();
  }
  catch (...)
  {
    return std::string();
  }
  LOG_PRINT_L2("Saving multisig unsigned tx data: " << oss.str());
  std::string ciphertext = encrypt_with_view_secret_key(oss.str());
  return std::string(MULTISIG_UNSIGNED_TX_PREFIX) + ciphertext;
}
//----------------------------------------------------------------------------------------------------
bool wallet2::save_multisig_tx(const multisig_tx_set &txs, const std::string &filename)
{
  std::string ciphertext = save_multisig_tx(txs);
  if (ciphertext.empty())
    return false;
  return save_to_file(filename, ciphertext);
}
//----------------------------------------------------------------------------------------------------
wallet2::multisig_tx_set wallet2::make_multisig_tx_set(const std::vector<pending_tx>& ptx_vector) const
{
  multisig_tx_set txs;
  txs.m_ptx = ptx_vector;

  for (const auto &msk: get_account().get_multisig_keys())
  {
    crypto::public_key pkey = get_multisig_signing_public_key(msk);
    for (auto &ptx: txs.m_ptx) for (auto &sig: ptx.multisig_sigs) sig.signing_keys.insert(pkey);
  }

  txs.m_signers.insert(get_multisig_signer_public_key());
  return txs;
}

std::string wallet2::save_multisig_tx(const std::vector<pending_tx>& ptx_vector)
{
  return save_multisig_tx(make_multisig_tx_set(ptx_vector));
}
//----------------------------------------------------------------------------------------------------
bool wallet2::save_multisig_tx(const std::vector<pending_tx>& ptx_vector, const std::string &filename)
{
  std::string ciphertext = save_multisig_tx(ptx_vector);
  if (ciphertext.empty())
    return false;
  return save_to_file(filename, ciphertext);
}
//----------------------------------------------------------------------------------------------------
bool wallet2::parse_multisig_tx_from_str(std::string multisig_tx_st, multisig_tx_set &exported_txs) const
{
  const size_t magiclen = strlen(MULTISIG_UNSIGNED_TX_PREFIX);
  if (strncmp(multisig_tx_st.c_str(), MULTISIG_UNSIGNED_TX_PREFIX, magiclen))
  {
    LOG_PRINT_L0("Bad magic from multisig tx data");
    return false;
  }
  try
  {
    multisig_tx_st = decrypt_with_view_secret_key(std::string(multisig_tx_st, magiclen));
  }
  catch (const std::exception &e)
  {
    LOG_PRINT_L0("Failed to decrypt multisig tx data: " << e.what());
    return false;
  }
  bool loaded = false;
  try
  {
    binary_archive<false> ar{epee::strspan<std::uint8_t>(multisig_tx_st)};
    if (::serialization::serialize(ar, exported_txs))
      if (::serialization::check_stream_state(ar))
        loaded = true;
  }
  catch (...) {}

  if (!loaded)
  {
    LOG_PRINT_L0("Failed to parse multisig tx data");
    return false;
  }

  // sanity checks
  for (const auto &ptx: exported_txs.m_ptx)
  {
    CHECK_AND_ASSERT_MES(ptx.selected_transfers.size() == ptx.tx.vin.size(), false, "Mismatched selected_transfers/vin sizes");
    for (size_t idx: ptx.selected_transfers)
      CHECK_AND_ASSERT_MES(idx < m_transfers.size(), false, "Transfer index out of range");
    CHECK_AND_ASSERT_MES(ptx.construction_data.selected_transfers.size() == ptx.tx.vin.size(), false, "Mismatched cd selected_transfers/vin sizes");
    for (size_t idx: ptx.construction_data.selected_transfers)
      CHECK_AND_ASSERT_MES(idx < m_transfers.size(), false, "Transfer index out of range");
    CHECK_AND_ASSERT_MES(ptx.construction_data.sources.size() == ptx.tx.vin.size(), false, "Mismatched sources/vin sizes");
  }

  return true;
}
//----------------------------------------------------------------------------------------------------
bool wallet2::load_multisig_tx(cryptonote::blobdata s, multisig_tx_set &exported_txs, std::function<bool(const multisig_tx_set&)> accept_func)
{
  if(!parse_multisig_tx_from_str(s, exported_txs))
  {
    LOG_PRINT_L0("Failed to parse multisig transaction from string");
    return false;
  }

  LOG_PRINT_L1("Loaded multisig tx unsigned data from binary: " << exported_txs.m_ptx.size() << " transactions");
  for (auto &ptx: exported_txs.m_ptx) LOG_PRINT_L0(cryptonote::obj_to_json_str(ptx.tx));

  if (accept_func && !accept_func(exported_txs))
  {
    LOG_PRINT_L1("Transactions rejected by callback");
    return false;
  }

  const bool is_signed = exported_txs.m_signers.size() >= m_multisig_threshold;
  if (is_signed)
  {
    for (const auto &ptx: exported_txs.m_ptx)
    {
      const crypto::hash txid = get_transaction_hash(ptx.tx);
      if (store_tx_info())
      {
        m_tx_keys[txid] = ptx.tx_key;
        m_additional_tx_keys[txid] = ptx.additional_tx_keys;
      }
    }
  }

  return true;
}
//----------------------------------------------------------------------------------------------------
bool wallet2::load_multisig_tx_from_file(const std::string &filename, multisig_tx_set &exported_txs, std::function<bool(const multisig_tx_set&)> accept_func)
{
  std::string s;
  boost::system::error_code errcode;

  if (!boost::filesystem::exists(filename, errcode))
  {
    LOG_PRINT_L0("File " << filename << " does not exist: " << errcode);
    return false;
  }
  if (!load_from_file(filename.c_str(), s))
  {
    LOG_PRINT_L0("Failed to load from " << filename);
    return false;
  }

  if (!load_multisig_tx(s, exported_txs, accept_func))
  {
    LOG_PRINT_L0("Failed to parse multisig tx data from " << filename);
    return false;
  }
  return true;
}
//----------------------------------------------------------------------------------------------------
bool wallet2::sign_multisig_tx(multisig_tx_set &exported_txs, std::vector<crypto::hash> &txids)
{
  THROW_WALLET_EXCEPTION_IF(exported_txs.m_ptx.empty(), error::wallet_internal_error, "No tx found");

  const crypto::public_key local_signer = get_multisig_signer_public_key();

  THROW_WALLET_EXCEPTION_IF(exported_txs.m_signers.find(local_signer) != exported_txs.m_signers.end(),
      error::wallet_internal_error, "Transaction already signed by this private key");
  THROW_WALLET_EXCEPTION_IF(exported_txs.m_signers.size() > m_multisig_threshold,
      error::wallet_internal_error, "Transaction was signed by too many signers");
  THROW_WALLET_EXCEPTION_IF(exported_txs.m_signers.size() == m_multisig_threshold,
      error::wallet_internal_error, "Transaction is already fully signed");
  THROW_WALLET_EXCEPTION_IF(frozen(exported_txs),
    error::wallet_internal_error, "Will not sign multisig tx containing frozen outputs")

  txids.clear();

  // The 'exported_txs' contains a set of different transactions for the multisig group to try to sign. Each of those
  //   transactions has a set of 'signing attempts' corresponding to all the possible signing groups within the multisig.
  // - Here, we will partially sign as many of those signing attempts as possible, for each proposed transaction.
  for (size_t n = 0; n < exported_txs.m_ptx.size(); ++n)
  {
    tools::wallet2::pending_tx &ptx = exported_txs.m_ptx[n];
    THROW_WALLET_EXCEPTION_IF(ptx.multisig_sigs.empty(), error::wallet_internal_error, "No signatures found in multisig tx");
    const tools::wallet2::tx_construction_data &sd = ptx.construction_data;
    LOG_PRINT_L1(" " << (n+1) << ": " << sd.sources.size() << " inputs, ring size " << (sd.sources[0].outputs.size()) <<
        ", signed by " << exported_txs.m_signers.size() << "/" << m_multisig_threshold);

    // reconstruct the partially-signed transaction attempt to verify we are signing something that at least looks like a transaction
    // note: the caller should further verify that the tx details are acceptable (inputs/outputs/memos/tx type)
    multisig::signing::tx_builder_ringct_t multisig_tx_builder;
    THROW_WALLET_EXCEPTION_IF(
      not multisig_tx_builder.init(
        m_account.get_keys(),
        ptx.construction_data.extra,
        ptx.construction_data.subaddr_account,
        ptx.construction_data.subaddr_indices,
        ptx.construction_data.sources,
        ptx.construction_data.splitted_dsts,
        ptx.construction_data.change_dts,
        ptx.construction_data.rct_config,
        ptx.construction_data.use_rct,
        true,  //true = we are reconstructing the tx (it was first constructed by the tx proposer)
        ptx.tx_key,
        ptx.additional_tx_keys,
        ptx.multisig_tx_key_entropy,
        ptx.tx
      ),
      error::wallet_internal_error,
      "error: multisig::signing::tx_builder_ringct_t::init"
    );

    // go through each signing attempt for this transaction (each signing attempt corresponds to some subgroup of signers
    //   of size 'threshold')
    for (auto &sig: ptx.multisig_sigs)
    {
      // skip this partial tx if it's intended for a subgroup of signers that doesn't include the local signer
      // note: this check can only weed out signers who provided multisig_infos to the multisig tx proposer's
      //       (initial author's) last call to import_multisig() before making this tx proposal; all other signers
      //       will encounter a 'need to export multisig' wallet error in get_multisig_k() below
      // note2: the 'need to export multisig' wallet error can also appear if a bad/buggy tx proposer adds duplicate
      //       'used_L' to the set of tx attempts, or if two different tx proposals use the same 'used_L' values and the
      //       local signer calls this function on both of them
      if (sig.ignore.find(local_signer) == sig.ignore.end())
      {
        rct::keyM local_nonces_k(sd.selected_transfers.size(), rct::keyV(multisig::signing::kAlphaComponents));
        rct::key skey = rct::zero();
        auto wiper = epee::misc_utils::create_scope_leave_handler([&]{
          for (auto& e: local_nonces_k)
            memwipe(e.data(), e.size() * sizeof(rct::key));
          memwipe(&skey, sizeof(rct::key));
        });

        // get local signer's nonces for this transaction attempt's inputs
        // note: whoever created 'exported_txs' has full power to match proposed tx inputs (selected_transfers)
        //       with the public nonces of the multisig signers who call this function (via 'used_L' as identifiers), however
        //       the local signer will only use a given nonce exactly once (even if a used_L is repeated)
        for (std::size_t i = 0; i < local_nonces_k.size(); ++i) {
          for (std::size_t j = 0; j < multisig::signing::kAlphaComponents; ++j) {
            get_multisig_k(sd.selected_transfers[i], sig.used_L, local_nonces_k[i][j]);
          }
        }

        // round-robin signing: sign with all local multisig key shares that other signers have not signed with yet
        for (const auto &multisig_skey: get_account().get_multisig_keys())
        {
          crypto::public_key multisig_pkey = get_multisig_signing_public_key(multisig_skey);

          if (sig.signing_keys.find(multisig_pkey) == sig.signing_keys.end())
          {
            sc_add(skey.bytes, skey.bytes, rct::sk2rct(multisig_skey).bytes);
            sig.signing_keys.insert(multisig_pkey);
          }
        }

        THROW_WALLET_EXCEPTION_IF(
          not multisig_tx_builder.next_partial_sign(sig.total_alpha_G, sig.total_alpha_H, local_nonces_k, skey, sig.c_0, sig.s),
          error::wallet_internal_error,
          "error: multisig::signing::tx_builder_ringct_t::next_partial_sign"
        );
      }
    }

    const bool is_last = exported_txs.m_signers.size() + 1 >= m_multisig_threshold;
    if (is_last)
    {
      // if there are signatures from enough signers (assuming the local signer signed 1+ tx attempts), find the tx
      //       attempt with a full set of signatures so this tx can be finalized
      bool found = false;
      for (const auto &sig: ptx.multisig_sigs)
      {
        if (sig.ignore.find(local_signer) == sig.ignore.end() && !keys_intersect(sig.ignore, exported_txs.m_signers))
        {
          THROW_WALLET_EXCEPTION_IF(found, error::wallet_internal_error, "More than one transaction is final");
          THROW_WALLET_EXCEPTION_IF(
            not multisig_tx_builder.finalize_tx(ptx.construction_data.sources, sig.c_0, sig.s, ptx.tx),
            error::wallet_internal_error,
            "error: multisig::signing::tx_builder_ringct_t::finalize_tx"
          );
          found = true;
        }
      }
      THROW_WALLET_EXCEPTION_IF(!found, error::wallet_internal_error,
          "Unable to finalize the transaction: the ignore sets for these tx attempts seem to be malformed.");
      const crypto::hash txid = get_transaction_hash(ptx.tx);
      if (store_tx_info())
      {
        m_tx_keys[txid] = ptx.tx_key;
        m_additional_tx_keys[txid] = ptx.additional_tx_keys;
      }
      txids.push_back(txid);
    }
  }

  // signatures generated, get rid of any unused k values (must do export_multisig() to make more tx attempts with the
  //   inputs in the transactions worked on here)
  for (size_t n = 0; n < exported_txs.m_ptx.size(); ++n)
    for (size_t idx: exported_txs.m_ptx[n].construction_data.selected_transfers)
    {
      memwipe(m_transfers[idx].m_multisig_k.data(), m_transfers[idx].m_multisig_k.size() * sizeof(m_transfers[idx].m_multisig_k[0]));
      m_transfers[idx].m_multisig_k.clear();
    }

  exported_txs.m_signers.insert(get_multisig_signer_public_key());

  return true;
}
//----------------------------------------------------------------------------------------------------
bool wallet2::sign_multisig_tx_to_file(multisig_tx_set &exported_txs, const std::string &filename, std::vector<crypto::hash> &txids)
{
  bool r = sign_multisig_tx(exported_txs, txids);
  if (!r)
    return false;
  return save_multisig_tx(exported_txs, filename);
}
//----------------------------------------------------------------------------------------------------
bool wallet2::sign_multisig_tx_from_file(const std::string &filename, std::vector<crypto::hash> &txids, std::function<bool(const multisig_tx_set&)> accept_func)
{
  multisig_tx_set exported_txs;
  if(!load_multisig_tx_from_file(filename, exported_txs))
    return false;

  if (accept_func && !accept_func(exported_txs))
  {
    LOG_PRINT_L1("Transactions rejected by callback");
    return false;
  }
  return sign_multisig_tx_to_file(exported_txs, filename, txids);
}
//----------------------------------------------------------------------------------------------------
uint64_t wallet2::estimate_fee(bool use_per_byte_fee, bool use_rct, int n_inputs, int mixin, int n_outputs, size_t extra_size, bool bulletproof, bool clsag, bool bulletproof_plus, bool use_view_tags, uint64_t base_fee, uint64_t fee_quantization_mask)
{
  if (use_per_byte_fee)
  {
    const size_t estimated_tx_weight = estimate_tx_weight(use_rct, n_inputs, mixin, n_outputs, extra_size, bulletproof, clsag, bulletproof_plus, use_view_tags);
    return calculate_fee_from_weight(base_fee, estimated_tx_weight, fee_quantization_mask);
  }
  else
  {
    const size_t estimated_tx_size = estimate_tx_size(use_rct, n_inputs, mixin, n_outputs, extra_size, bulletproof, clsag, bulletproof_plus, use_view_tags);
    return calculate_fee(base_fee, estimated_tx_size);
  }
}

uint64_t wallet2::get_fee_multiplier(fee_priority priority, fee_algorithm fee_algorithm)
{
  struct fee_step
  {
    fee_priority maximum_priority; // Determines the maximum priority available for said fee algorithm.
    uint64_t fee_multipliers[4]; // Determines the fee multiplier applied at various priorities for said fee algorithm.
  };

  static constexpr fee_step fee_steps[] =
  {
    { fee_priority::Elevated, {1, 2, 3} },
    { fee_priority::Elevated, {1, 20, 166} },
    { fee_priority::Priority, {1, 4, 20, 166} },
    { fee_priority::Priority, {1, 5, 25, 1000} },
  };

  if (fee_algorithm == fee_algorithm::Unset)
    fee_algorithm = get_fee_algorithm();

  // 0 -> default (here, x1 till fee algorithm 2, x4 from it)
  if (priority == fee_priority::Default)
    priority = m_default_priority;
  if (priority == fee_priority::Default)
  {
    if (fee_algorithm >= fee_algorithm::HardforkV5)
      priority = fee_priority::Normal;
    else
      priority = fee_priority::Unimportant;
  }

  const auto fee_algorithm_index = fee_algorithm_utilities::as_integral(fee_algorithm);
  THROW_WALLET_EXCEPTION_IF(fee_algorithm_index < 0 || fee_algorithm_index > static_cast<int>(std::size(fee_steps)), error::invalid_priority);

  // 1 to 3/4 are allowed as priorities
  const fee_priority max_priority = fee_steps[fee_algorithm_index].maximum_priority;
  if (priority >= fee_priority::Unimportant && priority <= max_priority)
  {
    const fee_priority adjusted_priority = fee_priority_utilities::decrease(priority);
    return fee_steps[fee_algorithm_index].fee_multipliers[fee_priority_utilities::as_integral(adjusted_priority)];
  }

  THROW_WALLET_EXCEPTION_IF (false, error::invalid_priority);
  return 1;
}
//----------------------------------------------------------------------------------------------------
uint64_t wallet2::get_dynamic_base_fee_estimate()
{
  uint64_t fee;
  boost::optional<std::string> result = m_node_rpc_proxy.get_dynamic_base_fee_estimate(FEE_ESTIMATE_GRACE_BLOCKS, fee);
  if (!result)
    return fee;
  const uint64_t base_fee = use_fork_rules(HF_VERSION_PER_BYTE_FEE) ? FEE_PER_BYTE : FEE_PER_KB;
  LOG_PRINT_L1("Failed to query base fee, using " << print_money(base_fee));
  return base_fee;
}
//----------------------------------------------------------------------------------------------------
uint64_t wallet2::get_base_fee()
{
  bool use_dyn_fee = use_fork_rules(HF_VERSION_DYNAMIC_FEE, -30 * 1);
  if (!use_dyn_fee)
    return FEE_PER_KB;

  return get_dynamic_base_fee_estimate();
}
//----------------------------------------------------------------------------------------------------
uint64_t wallet2::get_base_fee(uint32_t priority)
{
  return get_base_fee(fee_priority_utilities::from_integral(priority));
}
//----------------------------------------------------------------------------------------------------
uint64_t wallet2::get_base_fee(fee_priority priority)
{
  const bool use_2021_scaling = use_fork_rules(HF_VERSION_2021_SCALING, -30 * 1);
  if (use_2021_scaling)
  {
    // clamp and map to 0..3 indices, mapping 0 (default, but should not end up here) to 0, and 1..4 to 0..3
    priority = fee_priority_utilities::clamp_modified(priority);
    priority = fee_priority_utilities::decrease(priority);

    std::vector<uint64_t> fees;
    boost::optional<std::string> result = m_node_rpc_proxy.get_dynamic_base_fee_estimate_2021_scaling(FEE_ESTIMATE_GRACE_BLOCKS, fees);
    if (result)
    {
      MERROR("Failed to determine base fee, using default");
      return FEE_PER_BYTE;
    }

    const auto priority_index = fee_priority_utilities::as_integral(priority);
    if (priority_index >= fees.size())
    {
      MERROR("Failed to determine base fee for priority " << priority_index << ", using default");
      return FEE_PER_BYTE;
    }
    return fees[priority_index];
  }
  else
  {
    const uint64_t base_fee = get_base_fee();
    const uint64_t fee_multiplier = get_fee_multiplier(priority);
    return base_fee * fee_multiplier;
  }
}
//----------------------------------------------------------------------------------------------------
uint64_t wallet2::get_fee_quantization_mask()
{
  bool use_per_byte_fee = use_fork_rules(HF_VERSION_PER_BYTE_FEE, 0);
  if (!use_per_byte_fee)
    return 1;

  uint64_t fee_quantization_mask;
  boost::optional<std::string> result = m_node_rpc_proxy.get_fee_quantization_mask(fee_quantization_mask);
  if (result)
    return 1;
  return fee_quantization_mask;
}
//----------------------------------------------------------------------------------------------------
fee_algorithm wallet2::get_fee_algorithm()
{
  // changes at v3, v5, v8
  if (use_fork_rules(HF_VERSION_PER_BYTE_FEE, 0))
    return fee_algorithm::HardforkV8;
  if (use_fork_rules(5, 0))
    return fee_algorithm::HardforkV5;
  if (use_fork_rules(3, -30 * 14))
   return fee_algorithm::HardforkV3;
  return fee_algorithm::PreHardforkV3;
}
//------------------------------------------------------------------------------------------------------------------------------
uint64_t wallet2::get_min_ring_size()
{
  return 32; // HideRing: minimum ring size 32 (vs Monero 16)
}
//------------------------------------------------------------------------------------------------------------------------------
uint64_t wallet2::get_max_ring_size()
{
  return 64; // HideRing: maximum ring size 64 for enhanced privacy
}
