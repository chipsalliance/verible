// Copyright 2017-2023 The Verible Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "verible/common/util/sha256.h"

#include <array>
#include <string>

#include "absl/strings/escaping.h"
#include "absl/strings/string_view.h"
#include "gtest/gtest.h"

namespace verible {
namespace {

// The tests are comparing this SHA256 implementation and the SHA256 digest
// produced by the OpenSSL SHA256 implementation. The following code was used to
// create the expected SHA256 digests:
//
// #include "openssl/sha.h"
//
// std::string SslSHA256Digest(absl::string_view content) {
//   std::array<unsigned char, SHA256_DIGEST_LENGTH> buf;
//   ::SHA256(reinterpret_cast<const unsigned char *>(content.data()),
//            content.size(), buf.data());
//   return absl::BytesToHexString(absl::string_view(
//       reinterpret_cast<const char *>(buf.data()), buf.size()));
// }
//
// (OpenSSL is not linked to avoid a dependency bloat)
// Alternative: `printf "banana" | sha256sum`

TEST(Sha256, DigestsAreEqual) {
  static constexpr absl::string_view kOpenSslSha256BananaDigest =
      "b493d48364afe44d11c0165cf470a4164d1e2609911ef998be868d46ade3de4e";
  EXPECT_EQ(Sha256Hex("banana"), kOpenSslSha256BananaDigest);
}

TEST(Sha256, NonAsciiDigestsAreEqual) {
  static constexpr absl::string_view kOpenSslSha256JaBananaDigest =
      "787bcc7042939ad9607bc8ca87332e4178716be0f0b890cbf673884d39d8ff79";
  EXPECT_EQ(Sha256Hex("バナナ"), kOpenSslSha256JaBananaDigest);
}

TEST(Sha256, LargeInputDigestsAreEqual) {
  static constexpr absl::string_view kLargeText = R"(
Internet Engineering Task Force (IETF)                   D. Eastlake 3rd
Request for Comments: 6234                                        Huawei
Obsoletes: 4634                                                T. Hansen
Updates: 3174                                                  AT&T Labs
Category: Informational                                         May 2011
ISSN: 2070-1721


                       US Secure Hash Algorithms
                   (SHA and SHA-based HMAC and HKDF)

Abstract

   The United States of America has adopted a suite of Secure Hash
   Algorithms (SHAs), including four beyond SHA-1, as part of a Federal
   Information Processing Standard (FIPS), namely SHA-224, SHA-256,
   SHA-384, and SHA-512.  This document makes open source code
   performing these SHA hash functions conveniently available to the
   Internet community.  The sample code supports input strings of
   arbitrary bit length.  Much of the text herein was adapted by the
   authors from FIPS 180-2.

   This document replaces RFC 4634, fixing errata and adding code for an
   HMAC-based extract-and-expand Key Derivation Function, HKDF (RFC
   5869).  As with RFC 4634, code to perform SHA-based Hashed Message
   Authentication Codes (HMACs) is also included.)";
  static constexpr absl::string_view kOpenSslSha256LargeTextDigest =
      "11fc4b5feb7b63ddcc15cfb05d1f969da2e0d537ec8eded8370e12811f7ab1a8";
  EXPECT_EQ(Sha256Hex(kLargeText), kOpenSslSha256LargeTextDigest);
}

TEST(Sha256, EmptyInputDigestsAreEqual) {
  static constexpr absl::string_view kOpenSslSha256EmptyStringDigest =
      "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";
  EXPECT_EQ(Sha256Hex(""), kOpenSslSha256EmptyStringDigest);
}

TEST(Sha256, IncrementallyAddedDigestsAreEqual) {
  static constexpr absl::string_view kOpenSslSha256BananaDigest =
      "b493d48364afe44d11c0165cf470a4164d1e2609911ef998be868d46ade3de4e";
  Sha256Context context;
  context.AddInput("b");
  context.AddInput("");
  context.AddInput("anan");
  context.AddInput("a");
  auto digest = context.BuildAndReset();

  std::string actual = absl::BytesToHexString(absl::string_view(
      reinterpret_cast<const char *>(digest.data()), digest.size()));
  EXPECT_EQ(actual, kOpenSslSha256BananaDigest);
}

}  // namespace
}  // namespace verible
