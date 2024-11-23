/***************** See RFC 6234 for details. *******************/
/* Copyright (c) 2011 IETF Trust and the persons identified as */
/* authors of the code.  All rights reserved.                  */
/* See sha256.h for terms of use and redistribution.           */

/*
 * Description:
 *   This file implements the Secure Hash Algorithms
 *   SHA-256 as defined in the U.S. National Institute of Standards
 *   and Technology Federal Information Processing Standards
 *   Publication (FIPS PUB) 180-3 published in October 2008
 *   and formerly defined in its predecessors, FIPS PUB 180-1
 *   and FIP PUB 180-2.
 *
 *   A combined document showing all algorithms is available at
 *       http://csrc.nist.gov/publications/fips/
 *              fips180-3/fips180-3_final.pdf
 *
 *   The SHA-256 algorithm produce 256-bit
 *   message digest for a given data stream.  It should take about
 *   2**n steps to find a message with the same digest as a given
 *   message and 2**(n/2) to find any two messages with the same
 *   digest, when n is the digest size in bits.  Therefore, this
 *   algorithm can serve as a means of providing a
 *   "fingerprint" for a message.
 *
 * Caveats:
 *   SHA-256 is designed to work with messages less
 *   than 2^64 bits long.  This implementation uses AddInput
 *   to hash the bits that are a multiple of the size of an 8-bit
 *   octet.
 *
 * The code is derived from: https://www.rfc-editor.org/rfc/rfc6234.txt
 */

#include "verible/common/util/sha256.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

#include "absl/strings/escaping.h"
#include "absl/strings/string_view.h"

namespace verible {
namespace {

// Returns SHA rotate right.
inline uint32_t Sha256Rotr(uint32_t bits, uint32_t word) {
  return (word >> bits) | (word << (32 - bits));
}

// Define the SHA SIGMA and sigma functions.
inline uint32_t Sha256CapitalSigma0(uint32_t word) {
  return Sha256Rotr(2, word) ^ Sha256Rotr(13, word) ^ Sha256Rotr(22, word);
}

inline uint32_t Sha256CapitalSigma1(uint32_t word) {
  return Sha256Rotr(6, word) ^ Sha256Rotr(11, word) ^ Sha256Rotr(25, word);
}

inline uint32_t Sha256Sigma0(uint32_t word) {
  return Sha256Rotr(7, word) ^ Sha256Rotr(18, word) ^ (word >> 3);
}

inline uint32_t Sha256Sigma1(uint32_t word) {
  return Sha256Rotr(17, word) ^ Sha256Rotr(19, word) ^ (word >> 10);
}

// Ch() and Maj() are defined identically in sections 4.1.1,
// 4.1.2, and 4.1.3.
//
// The definitions used in FIPS 180-3 are as follows:
inline uint32_t SHA_Ch(uint32_t x, uint32_t y, uint32_t z) {
  return (x & (y ^ z)) ^ z;
}

inline uint32_t SHA_Maj(uint32_t x, uint32_t y, uint32_t z) {
  return (x & (y | z)) | (y & z);
}

}  // namespace

void Sha256Context::Reset() {
  // Initial Hash Values: FIPS 180-3 section 5.3.3
  constexpr std::array<uint32_t, kSha256HashSize / 4> H0 = {
      0x6A09E667, 0xBB67AE85, 0x3C6EF372, 0xA54FF53A,
      0x510E527F, 0x9B05688C, 0x1F83D9AB, 0x5BE0CD19};

  length_high_ = 0;
  length_low_ = 0;
  message_block_index_ = 0;

  intermediate_hash_[0] = H0[0];
  intermediate_hash_[1] = H0[1];
  intermediate_hash_[2] = H0[2];
  intermediate_hash_[3] = H0[3];
  intermediate_hash_[4] = H0[4];
  intermediate_hash_[5] = H0[5];
  intermediate_hash_[6] = H0[6];
  intermediate_hash_[7] = H0[7];

  overflowed_ = false;
}

bool Sha256Context::AddLength(unsigned int length) {
  const uint32_t old_length_low_ = length_low_;
  length_low_ += length;
  // Test for overflow
  if (length_low_ < old_length_low_) {
    ++length_high_;
    return length_high_ != 0;
  }
  return true;
}

// Adds an array of octets as the next portion of the message.  Returns false if
// the accumulated message is too large (>2 Exabytes).
bool Sha256Context::AddInput(absl::string_view message) {
  if (overflowed_) return false;

  for (const char c : message) {
    message_block_[message_block_index_++] = static_cast<uint8_t>(c);

    if (!AddLength(8)) {
      overflowed_ = true;
      return false;
    }

    if (message_block_index_ == kSha256MessageBlockSize) {
      ProcessMessageBlock();
    }
  }
  return true;
}

void Sha256Context::ProcessMessageBlock() {
  // Constants defined in FIPS 180-3, section 4.2.2
  constexpr std::array<uint32_t, 64> K = {
      0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1,
      0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
      0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786,
      0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
      0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
      0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
      0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
      0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
      0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a,
      0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
      0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

  // Word sequence
  std::array<uint32_t, 64> W;

  // Initialize the first 16 words in the array W
  for (int t = 0, t4 = 0; t < 16; t++, t4 += 4) {
    W[t] = (((uint32_t)message_block_[t4]) << 24) |
           (((uint32_t)message_block_[t4 + 1]) << 16) |
           (((uint32_t)message_block_[t4 + 2]) << 8) |
           ((uint32_t)message_block_[t4 + 3]);
  }

  for (int t = 16; t < 64; t++) {
    W[t] =
        Sha256Sigma1(W[t - 2]) + W[t - 7] + Sha256Sigma0(W[t - 15]) + W[t - 16];
  }

  uint32_t A = intermediate_hash_[0];
  uint32_t B = intermediate_hash_[1];
  uint32_t C = intermediate_hash_[2];
  uint32_t D = intermediate_hash_[3];
  uint32_t E = intermediate_hash_[4];
  uint32_t F = intermediate_hash_[5];
  uint32_t G = intermediate_hash_[6];
  uint32_t H = intermediate_hash_[7];

  for (int t = 0; t < 64; t++) {
    uint32_t temp1 = H + Sha256CapitalSigma1(E) + SHA_Ch(E, F, G) + K[t] + W[t];
    uint32_t temp2 = Sha256CapitalSigma0(A) + SHA_Maj(A, B, C);
    H = G;
    G = F;
    F = E;
    E = D + temp1;
    D = C;
    C = B;
    B = A;
    A = temp1 + temp2;
  }

  intermediate_hash_[0] += A;
  intermediate_hash_[1] += B;
  intermediate_hash_[2] += C;
  intermediate_hash_[3] += D;
  intermediate_hash_[4] += E;
  intermediate_hash_[5] += F;
  intermediate_hash_[6] += G;
  intermediate_hash_[7] += H;

  message_block_index_ = 0;
}

void Sha256Context::PadMessage(uint8_t Pad_Byte) {
  // Check to see if the current message block is too small to hold the initial
  // padding bits and length.  If so, we will pad the block, process it, and
  // then continue padding into a second block.
  if (message_block_index_ >= (kSha256MessageBlockSize - 8)) {
    message_block_[message_block_index_++] = Pad_Byte;
    while (message_block_index_ < kSha256MessageBlockSize) {
      message_block_[message_block_index_++] = 0;
    }
    ProcessMessageBlock();
  } else {
    message_block_[message_block_index_++] = Pad_Byte;
  }

  while (message_block_index_ < (kSha256MessageBlockSize - 8)) {
    message_block_[message_block_index_++] = 0;
  }

  // Store the message length as the last 8 octets
  message_block_[56] = (uint8_t)(length_high_ >> 24);
  message_block_[57] = (uint8_t)(length_high_ >> 16);
  message_block_[58] = (uint8_t)(length_high_ >> 8);
  message_block_[59] = (uint8_t)(length_high_);
  message_block_[60] = (uint8_t)(length_low_ >> 24);
  message_block_[61] = (uint8_t)(length_low_ >> 16);
  message_block_[62] = (uint8_t)(length_low_ >> 8);
  message_block_[63] = (uint8_t)(length_low_);

  ProcessMessageBlock();
}

// Finishes off the digest calculations and returns the 256-bit message digest.
// NOTE: The first octet of hash is stored in the element with index 0, the last
// octet of hash in the element with index 27/31.
std::array<uint8_t, kSha256HashSize> Sha256Context::BuildAndReset() {
  PadMessage(0x80);
  // message may be sensitive, so clear it out
  for (int i = 0; i < kSha256MessageBlockSize; ++i) {
    message_block_[i] = 0;
  }
  length_high_ = 0;
  length_low_ = 0;

  std::array<uint8_t, kSha256HashSize> message_digest;
  for (size_t i = 0; i < message_digest.size(); ++i) {
    message_digest[i] =
        (uint8_t)(intermediate_hash_[i >> 2] >> 8 * (3 - (i & 0x03)));
  }
  return message_digest;
}

std::array<uint8_t, kSha256HashSize> Sha256(absl::string_view content) {
  Sha256Context context;
  context.AddInput(content);
  if (context.IsOverflowed()) {
    return {};
  }
  return context.BuildAndReset();
}

std::string Sha256Hex(absl::string_view content) {
  auto sha256bytes = Sha256(content);
  if (sha256bytes.empty()) {
    return "";
  }
  return absl::BytesToHexString(absl::string_view(
      reinterpret_cast<const char *>(sha256bytes.data()), sha256bytes.size()));
}

}  // namespace verible
