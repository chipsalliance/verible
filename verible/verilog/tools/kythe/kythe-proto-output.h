// Copyright 2017-2020 The Verible Authors.
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

#ifndef VERIBLE_VERILOG_TOOLS_KYTHE_KYTHE_PROTO_OUTPUT_H_
#define VERIBLE_VERILOG_TOOLS_KYTHE_KYTHE_PROTO_OUTPUT_H_

#include "google/protobuf/io/zero_copy_stream_impl.h"
#include "verible/verilog/tools/kythe/kythe-facts-extractor.h"
#include "verible/verilog/tools/kythe/kythe-facts.h"

namespace verilog {
namespace kythe {

class KytheProtoOutput final : public KytheOutput {
 public:
  explicit KytheProtoOutput(int output_fd);
  ~KytheProtoOutput() final;

  // Output Kythe facts from the indexing data in proto format.
  void Emit(const Fact &fact) final;
  void Emit(const Edge &edge) final;

 private:
  ::google::protobuf::io::FileOutputStream out_;
};

}  // namespace kythe
}  // namespace verilog

#endif  // VERIBLE_VERILOG_TOOLS_KYTHE_KYTHE_PROTO_OUTPUT_H_
