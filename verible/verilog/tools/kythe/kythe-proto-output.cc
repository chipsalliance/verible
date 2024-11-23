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

#include "verible/verilog/tools/kythe/kythe-proto-output.h"

#include <string>

#include "google/protobuf/io/coded_stream.h"
#include "google/protobuf/io/zero_copy_stream_impl.h"
#include "third_party/proto/kythe/storage.pb.h"
#include "verible/verilog/tools/kythe/kythe-facts.h"

namespace verilog {
namespace kythe {
namespace {

using ::google::protobuf::io::CodedOutputStream;
using ::google::protobuf::io::FileOutputStream;
using ::kythe::proto::Entry;

// Returns the VName representation in Kythe's storage proto format.
::kythe::proto::VName ConvertVnameToProto(const VName &vname) {
  ::kythe::proto::VName proto_vname;
  *proto_vname.mutable_signature() = vname.signature.ToString();
  *proto_vname.mutable_corpus() = std::string{vname.corpus};
  *proto_vname.mutable_root() = std::string{vname.root};
  *proto_vname.mutable_path() = std::string{vname.path};
  *proto_vname.mutable_language() = std::string{vname.language};
  return proto_vname;
}

// Returns the Fact representation in Kythe's storage proto format.
Entry ConvertEdgeToEntry(const Edge &edge) {
  Entry entry;
  entry.set_fact_name("/");
  *entry.mutable_edge_kind() = std::string{edge.edge_name};
  *entry.mutable_source() = ConvertVnameToProto(edge.source_node);
  *entry.mutable_target() = ConvertVnameToProto(edge.target_node);
  return entry;
}

// Returns the Fact representation in Kythe's storage proto format.
Entry ConvertFactToEntry(const Fact &fact) {
  Entry entry;
  *entry.mutable_fact_name() = std::string{fact.fact_name};
  *entry.mutable_fact_value() = fact.fact_value;
  *entry.mutable_source() = ConvertVnameToProto(fact.node_vname);
  return entry;
}

// Output entry to the stream.
void OutputProto(const Entry &entry, FileOutputStream *stream) {
  CodedOutputStream coded_stream(stream);
  coded_stream.WriteVarint32(entry.ByteSizeLong());
  entry.SerializeToCodedStream(&coded_stream);
}

}  // namespace

KytheProtoOutput::KytheProtoOutput(int fd) : out_(fd) {}
KytheProtoOutput::~KytheProtoOutput() { out_.Close(); }

void KytheProtoOutput::Emit(const Fact &fact) {
  OutputProto(ConvertFactToEntry(fact), &out_);
}
void KytheProtoOutput::Emit(const Edge &edge) {
  OutputProto(ConvertEdgeToEntry(edge), &out_);
}

}  // namespace kythe
}  // namespace verilog
