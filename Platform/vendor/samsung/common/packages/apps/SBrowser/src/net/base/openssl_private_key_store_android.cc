// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/openssl_private_key_store.h"

#include <openssl/evp.h>
#include <openssl/x509.h>

#include "base/logging.h"
#include "base/memory/singleton.h"
#include "crypto/openssl_util.h"
#include "net/android/network_library.h"


#if !defined(SBROWSER_CAC)
namespace net {

bool OpenSSLPrivateKeyStore::StoreKeyPair(const GURL& url,
                                          EVP_PKEY* pkey) {
  // Always clear openssl errors on exit.
  crypto::OpenSSLErrStackTracer err_trace(FROM_HERE);

  // Important: Do not use i2d_PublicKey() here, which returns data in
  // PKCS#1 format, use i2d_PUBKEY() which returns it as DER-encoded
  // SubjectPublicKeyInfo (X.509), as expected by the platform.
  unsigned char* public_key = NULL;
  int public_len = i2d_PUBKEY(pkey, &public_key);

  // Important: Do not use i2d_PrivateKey() here, it returns data
  // in a format that is incompatible with what the platform expects.
  unsigned char* private_key = NULL;
  int private_len = 0;
  crypto::ScopedOpenSSL<
      PKCS8_PRIV_KEY_INFO,
      PKCS8_PRIV_KEY_INFO_free> pkcs8(EVP_PKEY2PKCS8(pkey));
  if (pkcs8.get() != NULL) {
    private_len = i2d_PKCS8_PRIV_KEY_INFO(pkcs8.get(), &private_key);
  }
  bool ret = false;
  if (public_len > 0 && private_len > 0) {
    ret = net::android::StoreKeyPair(
        static_cast<const uint8*>(public_key), public_len,
        static_cast<const uint8*>(private_key), private_len);
  }
  LOG_IF(ERROR, !ret) << "StoreKeyPair failed. pub len = " << public_len
                      << " priv len = " << private_len;
  OPENSSL_free(public_key);
  OPENSSL_free(private_key);
  return ret;
}

}  // namespace net
#else

namespace net {
bool OpenSSLPrivateKeyStore::StoreKeyPair(const GURL& url,
                                          EVP_PKEY* pkey) {
  // Always clear openssl errors on exit.
  crypto::OpenSSLErrStackTracer err_trace(FROM_HERE);

  // Important: Do not use i2d_PublicKey() here, which returns data in
  // PKCS#1 format, use i2d_PUBKEY() which returns it as DER-encoded
  // SubjectPublicKeyInfo (X.509), as expected by the platform.
  unsigned char* public_key = NULL;
  int public_len = i2d_PUBKEY(pkey, &public_key);

  // Important: Do not use i2d_PrivateKey() here, it returns data
  // in a format that is incompatible with what the platform expects.
  unsigned char* private_key = NULL;
  int private_len = 0;
  crypto::ScopedOpenSSL<
      PKCS8_PRIV_KEY_INFO,
      PKCS8_PRIV_KEY_INFO_free> pkcs8(EVP_PKEY2PKCS8(pkey));
  if (pkcs8.get() != NULL) {
    private_len = i2d_PKCS8_PRIV_KEY_INFO(pkcs8.get(), &private_key);
  }
  bool ret = false;
  if (public_len > 0 && private_len > 0) {
    ret = net::android::StoreKeyPair(
        static_cast<const uint8*>(public_key), public_len,
        static_cast<const uint8*>(private_key), private_len);
  }
  LOG_IF(ERROR, !ret) << "StoreKeyPair failed. pub len = " << public_len
                      << " priv len = " << private_len;
  OPENSSL_free(public_key);
  OPENSSL_free(private_key);
  return ret;
}
namespace {

class OpenSSLKeyStoreAndroid : public OpenSSLPrivateKeyStore {
 public:
  ~OpenSSLKeyStoreAndroid() {
    // SAMSUNG_CHANGE : client_certificate >>>
    base::AutoLock lock(lock_);
    for (std::vector<EVP_PKEY*>::iterator it = keys_.begin();
         it != keys_.end(); ++it) {
      EVP_PKEY_free(*it);
    }
    // SAMSUNG_CHANGE : client_certificate <<<
  }

  virtual bool StorePrivateKey(const GURL& url, EVP_PKEY* pkey) {
    // Always clear openssl errors on exit.
    crypto::OpenSSLErrStackTracer err_trace(FROM_HERE);
    // Important: Do not use i2d_PublicKey() here, which returns data in
    // PKKCS#1 format, use i2d_PUBKEY() which returns it as DER-encoded
    // SubjectPublicKeyInfo (X.509), as expected by the platform.
    uint8* public_key = NULL;
    int public_len = i2d_PUBKEY(pkey, &public_key);
    // Important: Do not use i2d_PrivateKey() here, it returns data
    // in a format that is incompatible with what the platform expects
    // (i.e. this crashes the CertInstaller with an assertion error
    // "error:0D0680A8:asn1 encoding routines:ASN1_CHECK_TLEN:wrong tag"
    uint8* private_key = NULL;
    int private_len = 0;
    PKCS8_PRIV_KEY_INFO* pkcs8 = EVP_PKEY2PKCS8(pkey);
    if (pkcs8 != NULL) {
      private_len = i2d_PKCS8_PRIV_KEY_INFO(pkcs8, &private_key);
      PKCS8_PRIV_KEY_INFO_free(pkcs8);
    }
    bool ret = false;
    if (public_len > 0 && private_len > 0) {
      ret = net::android::StoreKeyPair(public_key, public_len, private_key,
                                       private_len);
    }
    LOG_IF(ERROR, !ret) << "StorePrivateKey failed. pub len = " << public_len
                        << " priv len = " << private_len;
    OPENSSL_free(public_key);
    OPENSSL_free(private_key);
    return ret;
  }

  virtual EVP_PKEY* FetchPrivateKey(EVP_PKEY* pkey) {
    // SAMSUNG_CHANGE : client_certificate >>>
    base::AutoLock lock(lock_);
    for (std::vector<EVP_PKEY*>::iterator it = keys_.begin();
         it != keys_.end(); ++it) {
      if (EVP_PKEY_cmp(*it, pkey) == 1)
        return *it;
    }
    // SAMSUNG_CHANGE : client_certificate <<<

    // TODO(joth): Implement when client authentication is required.
    NOTIMPLEMENTED();
    return NULL;
  }

  // SAMSUNG_CHANGE : smartcard_certificate >>>
  virtual bool StorePrivateKey(EVP_PKEY* pkey) {
    LOG(INFO) << "StorePrivateKey pkey : " << pkey;
    if (!pkey)
      return false;

    CRYPTO_add(&pkey->references, 1, CRYPTO_LOCK_EVP_PKEY);
    base::AutoLock lock(lock_);
    keys_.push_back(pkey);
    return true;
  }

  virtual bool StoreCertificateForHiddenPrivateKey(X509* x509) {
    base::AutoLock lock(lock_for_hidden_private_key_);
    certs_for_hidden_private_key_.push_back(x509);
    return true;
  }

  virtual bool HasCertificateHiddenPrivateKey(X509* x509) {
    base::AutoLock lock(lock_for_hidden_private_key_);
    for (std::vector<X509*>::iterator it = certs_for_hidden_private_key_.begin();
      it != certs_for_hidden_private_key_.end(); ++it) {
      // FIXME need to use X509_cmp(*it, x509)? But compare address itself rather than certificates because X509_cmp() returns false.
      if (*it == x509)
        return true;
    }
    return false;
  }
  // SAMSUNG_CHANGE : smartcard_certificate <<<

  static OpenSSLKeyStoreAndroid* GetInstance() {
    // Leak the OpenSSL key store as it is used from a non-joinable worker
    // thread that may still be running at shutdown.
    return Singleton<
        OpenSSLKeyStoreAndroid,
        OpenSSLKeyStoreAndroidLeakyTraits>::get();
  }

 private:
  friend struct DefaultSingletonTraits<OpenSSLKeyStoreAndroid>;
  typedef LeakySingletonTraits<OpenSSLKeyStoreAndroid>
      OpenSSLKeyStoreAndroidLeakyTraits;

  // SAMSUNG_CHANGE : client_certificate >>>
  std::vector<EVP_PKEY*> keys_;
  base::Lock lock_;
  // SAMSUNG_CHANGE : client_certificate <<<

  // SAMSUNG_CHANGE : smartcard_certificate >>>
  std::vector<X509*> certs_for_hidden_private_key_;
  base::Lock lock_for_hidden_private_key_;
  // SAMSUNG_CHANGE : smartcard_certificate <<<

  OpenSSLKeyStoreAndroid() {}

  DISALLOW_COPY_AND_ASSIGN(OpenSSLKeyStoreAndroid);
};

}  // namespace

OpenSSLPrivateKeyStore* OpenSSLPrivateKeyStore::GetInstance() {
  return OpenSSLKeyStoreAndroid::GetInstance();
}

}  // namespace net
#endif
