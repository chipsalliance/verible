// Copyright 2021 The Verible Authors.
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

// A simple code generator taking a yaml file and generating nlohmann/json
// serializable structs.

#include <cstdio>
#include <fstream>
#include <iostream>
#include <memory>
#include <ostream>
#include <regex>
#include <string>
#include <unordered_map>

#include "absl/flags/flag.h"
#include "absl/strings/ascii.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "common/util/init_command_line.h"

ABSL_FLAG(std::string, output, "",
          "Name of the output file. If empty, output is written to stdout");
ABSL_FLAG(std::string, class_namespace, "",
          "Namespace of the generated structs");
ABSL_FLAG(std::string, json_header, "<nlohmann/json.hpp>",
          "Include path to json.hpp including brackets <> or quotes \"\" "
          "around.");

// Interface. Currently private, but could be moved to a header if needed.
struct Location {
  const char *filename;
  int line;
};
inline std::ostream &operator<<(std::ostream &out, const Location &loc) {
  return out << loc.filename << ":" << loc.line << ": ";
}

struct ObjectType;

struct Property {
  Property(const Location &loc, ObjectType *owner, const std::string &name,
           bool is_optional, bool is_array)
      : location(loc),
        owner(owner),
        name(name),
        is_optional(is_optional),
        is_array(is_array) {}

  bool EqualNameValue(const Property &other) const {
    return other.name == name;
  }

  Location location = {"<>", 0};  // Where it is defined
  ObjectType *owner;

  std::string name;
  bool is_optional;
  bool is_array;
  std::string default_value;

  // TODO: have alternative types
  std::string type;
  ObjectType *object_type = nullptr;
};

struct ObjectType {
  ObjectType(const Location &loc, const std::string &name)
      : location(loc), name(name) {}

  Location location;
  std::string name;

  std::vector<std::string> extends;  // name of the superclasses
  std::vector<Property> properties;

  std::vector<const ObjectType *> superclasses;  // fixed up after all read.
};

using ObjectTypeVector = std::vector<ObjectType *>;

static bool contains(const std::string &s, char c) {
  return absl::StrContains(s, c);
}

// Returns if successful.
static bool ParseObjectTypesFromFile(const std::string &filename,
                                     ObjectTypeVector *parsed_out) {
  static const std::regex emptyline_or_comment_re("^[ \t]*(#.*)?");
  static const std::regex toplevel_object_re("^([a-zA-Z0-9_]+):");

  // For now, let's just read up to the first type and leave out alternatives
  static const std::regex property_re(
      "^[ \t]+([a-zA-Z_<]+)([\\?\\+]*):[ ]*([a-zA-Z0-9_]+)[ ]*(=[ \t]*(.+))?");

  Location current_location = {filename.c_str(), 0};
  ObjectType *current_model = nullptr;
  std::ifstream in(filename);
  if (!in.good()) {
    std::cerr << "Can't open " << filename << "\n";
    return false;
  }
  std::string line;
  std::smatch matches;
  while (!in.eof()) {
    std::getline(in, line);
    current_location.line++;

    if (std::regex_match(line, emptyline_or_comment_re)) continue;

    if (std::regex_search(line, matches, toplevel_object_re)) {
      current_model = new ObjectType(current_location, matches[1]);
      parsed_out->push_back(current_model);
      continue;
    }

    if (!current_model) {
      std::cerr << current_location << "No ObjectType definition\n";
      return false;
    }
    if (!std::regex_search(line, matches, property_re)) {
      std::cerr << current_location << "This doesn't look like a property\n";
      return false;
    }

    if (matches[1] == "<") {
      current_model->extends.push_back(matches[3]);
      continue;
    }

    Property property(current_location, current_model, matches[1],
                      contains(matches[2], '?'), contains(matches[2], '+'));
    property.type = matches[3];  // TODO: allow multiple
    property.default_value = matches[5];
    current_model->properties.push_back(property);
  }
  return true;
}

// Validate types and return if successful.
static bool ValidateTypes(ObjectTypeVector *object_types) {
  std::unordered_map<std::string, ObjectType *> typeByName;

  for (auto &obj : *object_types) {
    // We only insert types as they come, so that we can make sure they are
    // used after being defined.
    auto inserted = typeByName.insert({obj->name, obj});
    if (!inserted.second) {
      std::cerr << obj->location << "Duplicate name; previous defined in "
                << inserted.first->second->location << "\n";
      return false;
    }

    // Resolve superclasses
    for (const auto &e : obj->extends) {
      const auto &found = typeByName.find(e);
      if (found == typeByName.end()) {
        std::cerr << obj->location << "Unknown superclass " << e << "\n";
        return false;
      }
      obj->superclasses.push_back(found->second);
    }

    for (auto &p : obj->properties) {
      const std::string &t = p.type;
      if (t == "object" || t == "string" || t == "integer" || t == "boolean") {
        continue;
      }

      const auto &found = typeByName.find(t);
      if (found == typeByName.end()) {
        std::cerr << p.location << "Unknown object type '" << t << "'\n";
        return false;
      }
      p.object_type = found->second;
    }

    // Validate that we don't have properties with the same name twice in
    // one class (including superclasses)
    std::unordered_map<std::string, const Property *> my_property_names;
    for (const auto &p : obj->properties) {
      auto inserted = my_property_names.insert({p.name, &p});
      if (inserted.second) continue;
      std::cerr << p.location << "In class '" << obj->name
                << "' same name property '" << p.name << "' defined here\n"
                << inserted.first->second->location << "  ... and here\n";
      return false;
    }
    for (const auto &s : obj->superclasses) {
      for (const auto &sp : s->properties) {
        auto inserted = my_property_names.insert({sp.name, &sp});
        if (inserted.second) continue;
        const bool is_owner_superclass = (inserted.first->second->owner != obj);
        std::cerr << obj->location << obj->name << " has duplicate property '"
                  << sp.name << "'\n"
                  << inserted.first->second->location << "  ... found in "
                  << (is_owner_superclass ? "super" : "") << "class '"
                  << inserted.first->second->owner->name << "'\n"
                  << sp.location << "  ... and in superclass '" << s->name
                  << "'\n";
        return false;
      }
    }
  }

  return true;
}

std::unique_ptr<ObjectTypeVector> LoadObjectTypes(const std::string &filename) {
  std::unique_ptr<ObjectTypeVector> result(new ObjectTypeVector());

  if (!ParseObjectTypesFromFile(filename, result.get())) return nullptr;
  if (!ValidateTypes(result.get())) return nullptr;
  return result;
}

// Simple code formatter that always indents when it sees "{" at the end
// of a format string and detents on "}" at the beginning of a string.
class CodeFormatter {
 public:
  CodeFormatter(std::ostream *out, int code_indent)
      : out_(out), code_indent_(code_indent) {}

  void push_indent() { indent_ += code_indent_; }
  void pop_indent() {
    if (indent_ > 0) indent_ -= code_indent_;
  }

  // Format string without parameters.
  CodeFormatter &operator()(const char *fmt) {
    IndentIfNeeded(fmt);
    return format(fmt);
  }

  // Format string; every ${} in the format string is replaced with
  // the native ostream printing of that given object.
  template <typename T, typename... Targs>
  CodeFormatter &operator()(const char *fmt, T car, Targs... cdr) {
    IndentIfNeeded(fmt);
    return format(fmt, car, cdr...);
  }

 private:
  void IndentIfNeeded(const char *fmt) {
    if (absl::StartsWith(fmt, "}")) pop_indent();
    if (last_was_newline) {
      *out_ << std::string(indent_, ' ');
    }
    last_was_newline = absl::EndsWith(fmt, "\n");
    if (absl::EndsWith(absl::StripTrailingAsciiWhitespace(fmt), "{"))
      push_indent();
  }

  CodeFormatter &format(const char *fmt) {
    *out_ << fmt;
    return *this;
  }

  template <typename T, typename... Targs>
  CodeFormatter &format(const char *fmt, T car, Targs... cdr) {
    for (/**/; *fmt; ++fmt) {
      if (fmt[0] == '$' && fmt[1] == '{' && fmt[2] == '}') {
        *out_ << car;
        return format(fmt + 3, cdr...);
      }
      *out_ << *fmt;
    }
    return *this;
  }

  std::ostream *const out_;
  const int code_indent_;
  int indent_ = 0;
  bool last_was_newline = false;
};

void GenerateCode(const std::string &filename,
                  const std::string &nlohmann_json_include,
                  const std::string &gen_namespace,
                  const ObjectTypeVector &objects, std::ostream *out) {
  CodeFormatter fmt(out, 2);
  fmt("// Don't modify. Generated from ${}\n", filename);
  fmt("#pragma once\n"
      "#include <string>\n"
      "#include <vector>\n");
  fmt("#include ${}\n\n", nlohmann_json_include);

  if (!gen_namespace.empty()) {
    fmt("namespace ${} {\n", gen_namespace).pop_indent();
  }
  for (const auto &obj : objects) {
    fmt("struct ${}", obj->name);
    bool is_first = true;
    for (const auto &superclass : obj->extends) {
      fmt("${} public ${}", is_first ? " :" : ",", superclass);
      is_first = false;
    }
    fmt(" {\n");
    for (const auto &p : obj->properties) {
      std::string type;
      if (p.object_type)
        type = p.object_type->name;
      else if (p.type == "string")
        type = "std::string";
      else if (p.type == "integer")
        type = "int";
      else if (p.type == "object")
        type = "nlohmann::json";
      else if (p.type == "boolean")
        type = "bool";
      // TODO: optional and array
      if (type.empty()) {
        std::cerr << p.location << "Not supported yet '" << p.type << "'\n";
        continue;
      }
      if (p.is_array) {
        fmt("std::vector<${}> ${}", type, p.name);
      } else {
        fmt("${} ${}", type, p.name);
      }
      if (!p.default_value.empty()) fmt(" = ${}", p.default_value);
      fmt(";\n");
      if (p.is_optional) {
        fmt("bool has_${} = false;  // optional property\n", p.name);
      }
    }

    // nlohmann::json serialization
    fmt("\n");
    fmt("void Deserialize(const nlohmann::json &j) {\n");
    for (const auto &superclass : obj->extends) {
      fmt("${}::Deserialize(j);\n", superclass);
    }
    for (const auto &p : obj->properties) {
      std::string access_call = "j.at(\"" + p.name + "\")";
      std::string access_deref = access_call + ".";
      if (p.is_optional) {
        fmt("if (auto found = j.find(\"${}\"); found != j.end()) {\n", p.name);
        fmt("has_${} = true;\n", p.name);
        access_call = "*found";
        access_deref = "found->";
      }
      if (p.object_type == nullptr || p.is_array) {
        fmt("${}get_to(${});\n", access_deref, p.name);
      } else {
        fmt("${}.Deserialize(${});\n", p.name, access_call);
      }
      if (p.is_optional) {
        fmt("}\n");
      }
    }
    fmt("}\n");  // end of Deserialize

    fmt("void Serialize(nlohmann::json *j) const {\n");
    for (const auto &e : obj->extends) {
      fmt("${}::Serialize(j);\n", e);
    }
    for (const auto &p : obj->properties) {
      if (p.is_optional) fmt("if (has_${}) ", p.name);
      if (p.object_type == nullptr || p.is_array) {
        fmt("(*j)[\"${}\"] = ${};\n", p.name, p.name);
      } else {
        fmt("${}.Serialize(&(*j)[\"${}\"]);\n", p.name, p.name);
      }
    }
    fmt("}\n");  // End of Serialize

    fmt("};\n");  // End of struct

    // functions that are picked up by the nlohmann::json serializer
    // We could generate template code once for all to_json/from_json that take
    // a T obj, but to limit method lookup confusion for other objects that
    // might interact with the json library, let's be explicit for each struct
    fmt("inline void to_json(nlohmann::json &j, const ${} &obj) "
        "{ obj.Serialize(&j); }\n",
        obj->name);
    fmt("inline void from_json(const nlohmann::json &j, ${} &obj) "
        "{ obj.Deserialize(j); }\n\n",
        obj->name);
  }

  if (!gen_namespace.empty()) {
    fmt("}  // ${}\n", gen_namespace);
  }
}

int main(int argc, char *argv[]) {
  const auto usage =
      absl::StrCat("usage: ", argv[0], " [options] <protocol-spec-yaml>");
  const auto file_args = verible::InitCommandLine(usage, &argc, &argv);

  if (file_args.size() < 2) {
    std::cerr << "Need filename" << std::endl;
    return 1;
  }
  const std::string &schema_filename = file_args[1];
  auto objects = LoadObjectTypes(schema_filename);
  if (!objects) {
    fprintf(stderr, "Couldn't parse spec\n");
    return 2;
  }

  std::ostream *out = &std::cout;
  std::unique_ptr<std::ostream> ostream_closer;
  const std::string &output_file = absl::GetFlag(FLAGS_output);
  if (!output_file.empty()) {
    ostream_closer.reset(new std::fstream(output_file, std::ios_base::out));
    out = ostream_closer.get();
  }

  GenerateCode(schema_filename, absl::GetFlag(FLAGS_json_header),
               absl::GetFlag(FLAGS_class_namespace), *objects, out);
}
