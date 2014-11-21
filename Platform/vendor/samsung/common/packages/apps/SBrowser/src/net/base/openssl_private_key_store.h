// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_OPENSSL_PRIVATE_KEY_STORE_H_
#define NET_BASE_OPENSSL_PRIVATE_KEY_STORE_H_

#include <vector>

// Avoid including <openssl/evp.h>
typedef struct evp_pkey_st EVP_PKEY;


#if defined(SBROWSER_CAC)
// SAMSUNG_CHANGE : smartcard_certificate >>>
typedef struct x509_st X509;
// SAMSUNG_CHANGE : smartcard_certificate <<<
#endif

#include "base/basictypes.h"
#include "net/base/net_export.h"

class GURL;

namespace net {

class X509Certificate;

// OpenSSLPrivateKeyStore provides an interface for storing
// public/private key pairs to system storage on platforms where
// OpenSSL is used.
// This class shall only be used from the network thread.
class NET_EXPORT OpenSSLPrivateKeyStore {
 public:
#if defined(SBROWSER_CAC) 
   // Platforms must define this factory function as appropriate.
  static OpenSSLPrivateKeyStore* GetInstance();

  virtual ~OpenSSLPrivateKeyStore() {}
#endif
  // Called to permanently store a private/public key pair, generated
  // via <keygen> while visiting |url|, to an appropriate system
  // location. Increments |pkey|'s reference count, so the caller is still
  // responsible for calling EVP_PKEY_free on it.
  // |url| is the corresponding server URL.
  // |pkey| is the key pair handle.
  // Returns false if an error occurred whilst attempting to store the key.
  static bool StoreKeyPair(const GURL& url, EVP_PKEY* pkey);

  // Checks that the private key for a given public key is installed.
  // |pub_key| a public key.
  // Returns true if there is a private key that was previously
  // recorded through StoreKeyPair().
  // NOTE: Intentionally not implemented on Android because there is no
  // platform API that can perform this operation silently.
  static bool HasPrivateKey(EVP_PKEY* pub_key);
#if defined(SBROWSER_CAC)
    // SAMSUNG_CHANGE : smartcard_certificate >>>
  virtual bool StorePrivateKey(EVP_PKEY* pkey) { return false; }

  virtual bool StoreCertificateForHiddenPrivateKey(X509* x509) { return false; }
  virtual bool HasCertificateHiddenPrivateKey(X509* x509) { return false; }

protected:
  OpenSSLPrivateKeyStore() {}
  // SAMSUNG_CHANGE : smartcard_certificate <<<
#endif

 private:
#if !defined(SBROWSER_CAC)  
   OpenSSLPrivateKeyStore();  // not implemented.
  ~OpenSSLPrivateKeyStore();  // not implemented.
#endif
  DISALLOW_COPY_AND_ASSIGN(OpenSSLPrivateKeyStore);
};

} // namespace net

#endif  // NET_BASE_OPENSSL_PRIVATE_KEY_STORE_H_
