// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_CRYPTOHOME_LIBRARY_H_
#define CHROME_BROWSER_CHROMEOS_CROS_CRYPTOHOME_LIBRARY_H_
#pragma once

#include <string>

namespace chromeos {

// This interface defines the interaction with the ChromeOS cryptohome library
// APIs.
class CryptohomeLibrary {
 public:
  CryptohomeLibrary();
  virtual ~CryptohomeLibrary();

  // Asks cryptohomed if a drive is currently mounted.
  virtual bool IsMounted() = 0;

  // Wrappers of the functions for working with Tpm.

  // Returns whether Tpm is ready.
  virtual bool TpmIsReady() = 0;

  // Returns whether Tpm is presented and enabled.
  virtual bool TpmIsEnabled() = 0;

  // Returns whether device has already been owned.
  virtual bool TpmIsOwned() = 0;

  // Returns whether device is being owned (Tpm password is generating).
  virtual bool TpmIsBeingOwned() = 0;

  // Returns Tpm password (if password was cleared empty one is returned).
  // Return value is true if password was successfully acquired.
  virtual bool TpmGetPassword(std::string* password) = 0;

  // Attempts to start owning (if device isn't owned and isn't being owned).
  virtual void TpmCanAttemptOwnership() = 0;

  // Clears Tpm password. Password should be cleared after it was generated and
  // shown to user.
  virtual void TpmClearStoredPassword() = 0;

  virtual bool InstallAttributesGet(const std::string& name,
                                    std::string* value) = 0;
  virtual bool InstallAttributesSet(const std::string& name,
                                    const std::string& value) = 0;
  virtual bool InstallAttributesFinalize() = 0;
  virtual bool InstallAttributesIsReady() = 0;
  virtual bool InstallAttributesIsInvalid() = 0;
  virtual bool InstallAttributesIsFirstInstall() = 0;

  // Get the PKCS#11 token info from the TPM.  This is different from
  // the TpmGetPassword because it's getting the PKCS#11 user PIN and
  // not the TPM password.
  virtual void Pkcs11GetTpmTokenInfo(std::string* label,
                                     std::string* user_pin) = 0;

  // Gets the status of the TPM.  This is different from TpmIsReady
  // because it's getting the staus of the PKCS#11 initialization of
  // the TPM token, not the TPM itself.
  virtual bool Pkcs11IsTpmTokenReady() = 0;

  // Returns hash of |password|, salted with the system salt.
  virtual std::string HashPassword(const std::string& password) = 0;

  // Returns system hash in hex encoded ascii format.
  virtual std::string GetSystemSalt() = 0;

  // Factory function, creates a new instance and returns ownership.
  // For normal usage, access the singleton via CrosLibrary::Get().
  static CryptohomeLibrary* GetImpl(bool stub);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CROS_CRYPTOHOME_LIBRARY_H_
