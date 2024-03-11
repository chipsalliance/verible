/***************** See RFC 6234 for details. *******************/
/*
   Copyright (c) 2011 IETF Trust and the persons identified as
   authors of the code.  All rights reserved.

   Redistribution and use in source and binary forms, with or
   without modification, are permitted provided that the following
   conditions are met:

   - Redistributions of source code must retain the above
     copyright notice, this list of conditions and
     the following disclaimer.

   - Redistributions in binary form must reproduce the above
     copyright notice, this list of conditions and the following
     disclaimer in the documentation and/or other materials provided
     with the distribution.

   - Neither the name of Internet Society, IETF or IETF Trust, nor
     the names of specific contributors, may be used to endorse or
     promote products derived from this software without specific
     prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
   CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
   INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
   MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
   DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
   NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
   LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
   HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
   CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
   OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
   EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


   Implementation of the SHA256 Secure Hash Algorithm as defined in the U.S.
   National Institute of Standards and Technology Federal Information Processing
   Standards Publication (FIPS PUB) 180-3 published in October 2008 and formerly
   defined in its predecessors, FIPS PUB 180-1 and FIP PUB 180-2. See:

   The algorithm description is available at
   http://csrc.nist.gov/publications/fips/fips180-3/fips180-3_final.pdf

   The code is derived from: https://www.rfc-editor.org/rfc/rfc6234.txt
*/

#ifndef VERIBLE_COMMON_UTIL_SHA_H_
#define VERIBLE_COMMON_UTIL_SHA_H_

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

namespace verible {

// The size of SHA256 hash in bytes.
inline constexpr int32_t kSha256HashSize = 32;
// The size of a single message block, in bytes.
inline constexpr int32_t kSha256MessageBlockSize = 64;

// The context information for the SHA-256 hashing operation.
class Sha256Context {
 public:
  Sha256Context() { Reset(); }

  // Finishes off the digest calculations and returns the 256-bit message
  // digest. Resets the context in preparation for computing a new SHA-256.
  // NOTE: The first octet of hash is stored in the element with index 0, the
  // last octet of hash in the element with index 27/31.
  std::array<uint8_t, kSha256HashSize> BuildAndReset();

  // Adds an array of octets as the next portion of the message.  Returns false
  // if the accumulated message is too large (>2 Exabytes). Can be called
  // multiple times to incrementally build the digest.
  bool AddInput(std::string_view message);

  // Returns true if the accumulated message is too large (>2 Exabytes).
  bool IsOverflowed() const { return overflowed_; }

 private:
  // Initialize the Sha256Context in preparation for computing a new SHA-256
  // message digest.
  void Reset();

  // Adds "length" to the length. Returns false if the accumulated message is
  // too large (>2 Exabytes).
  bool AddLength(unsigned int length);

  // Process the next 512 bits of the message stored in the message_block_
  // array.
  //
  // Many of the variable names in this code, especially the
  // single character names, were used because those were the
  // names used in the Secure Hash Standard.
  void ProcessMessageBlock();

  // Padds the message to the next even multiple of 512 bits. The first padding
  // bit must be a '1'.  The last 64 bits represent the length of the original
  // message.  All bits in between should be 0.  This helper function will pad
  // the message according to those rules by filling the message_block_ array
  // accordingly.  When it returns, it can be assumed that the message digest
  // has been computed.
  //
  // Pad_Byte is the last byte to add to the message block before the 0-padding
  // and length.  This will contain the last bits of the message followed by
  // another single bit.  If the message was an exact multiple of 8-bits long,
  // Pad_Byte will be 0x80.
  void PadMessage(uint8_t Pad_Byte);

  // Message Digest
  std::array<uint32_t, kSha256HashSize / 4> intermediate_hash_;

  // Message length in bits.
  uint32_t length_high_;
  uint32_t length_low_;

  // The last set index in the message_block_ array.
  int_least16_t message_block_index_;

  std::array<uint8_t, kSha256MessageBlockSize> message_block_;

  // True if the accumulated message is too large (>2 Exabytes).
  bool overflowed_;
};

// Returns the SHA256 hash of the given content.
std::array<uint8_t, kSha256HashSize> Sha256(std::string_view content);

// Returns the HEX string representation of SHA256 hash of the given content.
std::string Sha256Hex(std::string_view content);

}  // namespace verible

#endif  // VERIBLE_COMMON_UTIL_SHA_H_
