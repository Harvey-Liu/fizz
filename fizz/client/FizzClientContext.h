/*
 *  Copyright (c) 2018-present, Facebook, Inc.
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <fizz/client/CertManager.h>
#include <fizz/client/ECHPolicy.h>
#include <fizz/client/PskCache.h>
#include <fizz/compression/CertDecompressionManager.h>
#include <fizz/protocol/Certificate.h>
#include <fizz/protocol/Factory.h>
#include <fizz/protocol/clock/SystemClock.h>
#include <fizz/record/Types.h>

namespace fizz {
namespace client {

enum class SendKeyShare {
  Always,
  WhenNecessary,
};

class FizzClientContext {
 public:
  FizzClientContext();

  explicit FizzClientContext(std::shared_ptr<Factory> factory)
      : factory_(std::move(factory)), clock_(std::make_shared<SystemClock>()) {}

  virtual ~FizzClientContext() = default;

  /**
   * Set the supported protocol versions, in preference order.
   */
  void setSupportedVersions(std::vector<ProtocolVersion> versions) {
    supportedVersions_ = std::move(versions);
  }

  const auto& getSupportedVersions() const {
    return supportedVersions_;
  }

  /**
   * Set the supported ciphers, in preference order.
   */
  void setSupportedCiphers(std::vector<CipherSuite> ciphers) {
    supportedCiphers_ = std::move(ciphers);
  }

  const auto& getSupportedCiphers() const {
    return supportedCiphers_;
  }

  /**
   * Set the supported signature schemes, in preference order.
   */
  void setSupportedSigSchemes(std::vector<SignatureScheme> schemes) {
    supportedSigSchemes_ = std::move(schemes);
  }

  const auto& getSupportedSigSchemes() const {
    return supportedSigSchemes_;
  }

  /**
   * Set the supported named groups, in preference order.
   */
  void setSupportedGroups(std::vector<NamedGroup> groups) {
    supportedGroups_ = std::move(groups);
  }

  const auto& getSupportedGroups() const {
    return supportedGroups_;
  }

  /**
   * Set the default key shares to send. Must be a subset of supported groups.
   */
  void setDefaultShares(std::vector<NamedGroup> groups) {
    defaultShares_ = std::move(groups);
  }
  const auto& getDefaultShares() const {
    return defaultShares_;
  }

  /**
   * Set the supported psk modes, in preference order.
   */
  void setSupportedPskModes(std::vector<PskKeyExchangeMode> modes) {
    supportedPskModes_ = std::move(modes);
  }

  const auto& getSupportedPskModes() const {
    return supportedPskModes_;
  }

  /**
   * Sets the supported ALPN supported protocols, in preference order.
   */
  void setSupportedAlpns(std::vector<std::string> protocols) {
    supportedAlpns_ = std::move(protocols);
  }

  const auto& getSupportedAlpns() const {
    return supportedAlpns_;
  }

  /**
   * Set the extension types to be placed in ECH OuterExtensions. Ordering does
   * not matter, but vector search is expected to be faster than unordered_set
   * at small sizes.
   */
  void setECHOuterExtensionTypes(std::vector<ExtensionType> types) {
    echOuterExtensionTypes_ = std::move(types);
  }

  const auto& getECHOuterExtensionTypes() const {
    return echOuterExtensionTypes_;
  }

  /**
   * This is a legacy api, prefer setClientCertManager.
   * Sets the certificate to use if the server requests client authentication.
   * This api is meant to be used when you expect
   * to only be presenting one possible cert. This will overwrite any
   * pre-existing configuration.
   */
  [[deprecated("Use FizzClientContext::setClientCertManager")]]
  void setClientCertificate(std::shared_ptr<SelfCert> cert) {
    // Blow away any existing certs on the context.
    if (cert != nullptr) {
      auto certMgr = std::make_shared<CertManager>();
      clientCert_ = cert;
      certMgr->addCertAndOverride(std::move(cert));
      certManager_ = std::move(certMgr);
    } else {
      certManager_ = nullptr;
    }
  }

  /*
   * Sets the certificate manager to select a cert if the server requests client
   * auth
   */
  void setClientCertManager(std::shared_ptr<CertManager> manager) {
    certManager_ = std::move(manager);
  }

  std::shared_ptr<CertManager> getCertManager() const {
    return certManager_;
  }

  /*
   * Retrieves the default client cert used for the ctx. This is a legacy api,
   * do not use.
   */
  [[deprecated(
      "FizzClientContext will no longer allow access to the client certificate. The application is responsible for keeping track of the client certificate installed")]]
  std::shared_ptr<SelfCert> getClientCertificate() const {
    if (clientCert_) {
      return clientCert_;
    }
    if (certManager_) {
      auto result = certManager_->getCert(
          folly::none, supportedSigSchemes_, supportedSigSchemes_, {});
      return result ? result->cert : nullptr;
    }
    return nullptr;
  }

  void setECHPolicy(std::shared_ptr<ECHPolicy> echPolicy) {
    echPolicy_ = std::move(echPolicy);
  }

  ECHPolicy* getECHPolicy() const {
    return echPolicy_.get();
  }

  /**
   * Set the Psk Cache to use.
   */
  void setPskCache(std::shared_ptr<PskCache> pskCache) {
    pskCache_ = std::move(pskCache);
  }

  PskCache* getPskCache() const {
    return pskCache_.get();
  }

  std::shared_ptr<PskCache> getPskCacheShared() const {
    return pskCache_;
  }

  folly::Optional<CachedPsk> getPsk(const std::string& identity) const {
    if (pskCache_) {
      return pskCache_->getPsk(identity);
    } else {
      return folly::none;
    }
  }

  void putPsk(const std::string& identity, CachedPsk psk) const {
    if (pskCache_) {
      pskCache_->putPsk(identity, std::move(psk));
    }
  }

  void removePsk(const std::string& identity) const {
    if (pskCache_) {
      pskCache_->removePsk(identity);
    }
  }

  /**
   * Sets whether we should attempt to send early data.
   */
  void setSendEarlyData(bool sendEarlyData) {
    sendEarlyData_ = sendEarlyData;
  }

  bool getSendEarlyData() const {
    return sendEarlyData_;
  }

  /**
   * Sets whether we want to use compatibility mode (sending a fake session ID
   * and ChangeCipherSpec).
   */
  void setCompatibilityMode(bool enabled) {
    compatMode_ = enabled;
  }

  bool getCompatibilityMode() const {
    return compatMode_;
  }

  /**
   * Set the factory to use. Should generally only be changed for testing.
   */
  void setFactory(std::shared_ptr<Factory> factory) {
    factory_ = std::move(factory);
  }

  const Factory* getFactory() const {
    return factory_.get();
  }

  std::shared_ptr<Factory> getFactoryPtr() const {
    return factory_;
  }

  /**
   * Sets the certificate decompression manager for server certs.
   */
  void setCertDecompressionManager(
      std::shared_ptr<CertDecompressionManager> mgr) {
    certDecompressionManager_ = mgr;
  }

  /**
   * Returns a vector representing the compression algorithms the manager has
   * decompressors for.
   */
  std::vector<CertificateCompressionAlgorithm>
  getSupportedCertDecompressionAlgorithms() const {
    if (certDecompressionManager_) {
      return certDecompressionManager_->getSupportedAlgorithms();
    } else {
      return {};
    }
  }

  /**
   * Given a compression algorithm, returns the decompressor to decompress
   * certs. If the algorithm isn't found, returns nullptr.
   */
  std::shared_ptr<CertificateDecompressor> getCertDecompressorForAlgorithm(
      CertificateCompressionAlgorithm algo) const {
    if (certDecompressionManager_) {
      return certDecompressionManager_->getDecompressor(algo);
    } else {
      return nullptr;
    }
  }

  /**
   * Whether to omit the early record layer when sending early data. This will
   * also omit the EndOfEarlyData message.
   * Default is false, and using this requires a custom record layer.
   */
  void setOmitEarlyRecordLayer(bool enabled) {
    omitEarlyRecordLayer_ = enabled;
  }
  bool getOmitEarlyRecordLayer() const {
    return omitEarlyRecordLayer_;
  }

  /**
   * Whether or not require ALPN.
   * When set to true and ALPN extension is missing, throws
   * no_application_protocol alert.
   */
  void setRequireAlpn(bool enabled) {
    requireAlpn_ = enabled;
  }

  bool getRequireAlpn() const {
    return requireAlpn_;
  }

  /**
   * Controls the maximum age of a ticket's original handshake (i.e. the full
   * handshake that originally authenticated the initial connection) before
   * it's invalidated and removed from the cache.
   */
  void setMaxPskHandshakeLife(std::chrono::seconds life) {
    maxPskHandshakeLife_ = life;
  }
  std::chrono::seconds getMaxPskHandshakeLife() const {
    return maxPskHandshakeLife_;
  }

  void setClock(std::shared_ptr<Clock> clock) {
    clock_ = clock;
  }

  std::shared_ptr<Clock> getClock() const {
    return clock_;
  }

  /**
   * Option to turn off psk_ke optimzation.
   * When PSK is being used to resume and if psk exists but the group is unset,
   * we send an empty key_share. This way we avoid generating a group if the
   * server is likely to prefer psk_ke.
   */
  void setSendKeyShare(SendKeyShare option) {
    sendKeyShare_ = option;
  }

  SendKeyShare getSendKeyShare() const {
    return sendKeyShare_;
  }

  /**
   * Option to place the SNI extension first in the ClientHello. Default is
   * false.
   */
  void setSniExtFirst(bool sniExtFirst) {
    sniExtFirst_ = sniExtFirst;
  }

  bool getSniExtFirst() const {
    return sniExtFirst_;
  }

 private:
  std::shared_ptr<Factory> factory_;

  std::vector<ProtocolVersion> supportedVersions_ = {ProtocolVersion::tls_1_3};
  std::vector<CipherSuite> supportedCiphers_ = {
      CipherSuite::TLS_AES_128_GCM_SHA256,
      CipherSuite::TLS_AES_256_GCM_SHA384,
#if FOLLY_OPENSSL_HAS_CHACHA
      CipherSuite::TLS_CHACHA20_POLY1305_SHA256,
#endif // FOLLY_OPENSSL_HAS_CHACHA
  };
  std::vector<SignatureScheme> supportedSigSchemes_ = {
      SignatureScheme::ecdsa_secp256r1_sha256,
      SignatureScheme::ecdsa_secp384r1_sha384,
      SignatureScheme::ecdsa_secp521r1_sha512,
      SignatureScheme::rsa_pss_sha256};
  std::vector<NamedGroup> supportedGroups_ = {
      NamedGroup::x25519,
      NamedGroup::secp256r1};
  std::vector<NamedGroup> defaultShares_ = {NamedGroup::x25519};
  std::vector<PskKeyExchangeMode> supportedPskModes_ = {
      PskKeyExchangeMode::psk_dhe_ke,
      PskKeyExchangeMode::psk_ke};
  std::vector<std::string> supportedAlpns_;
  std::vector<ExtensionType> echOuterExtensionTypes_ = {
      ExtensionType::key_share};
  bool sendEarlyData_{false};

  bool compatMode_{false};

  bool omitEarlyRecordLayer_{false};

  bool requireAlpn_{false};

  SendKeyShare sendKeyShare_{SendKeyShare::WhenNecessary};

  std::shared_ptr<ECHPolicy> echPolicy_;
  std::shared_ptr<PskCache> pskCache_;
  // Legacy to support non cert mgr api.
  std::shared_ptr<SelfCert> clientCert_{nullptr};
  std::shared_ptr<CertManager> certManager_{nullptr};
  std::shared_ptr<CertDecompressionManager> certDecompressionManager_;
  std::shared_ptr<Clock> clock_;

  std::chrono::seconds maxPskHandshakeLife_{std::chrono::hours(168)};

  bool sniExtFirst_{false};
};
} // namespace client
} // namespace fizz
