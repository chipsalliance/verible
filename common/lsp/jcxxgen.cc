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

#include <unistd.h>

#include <fstream>
#include <iostream>
#include <memory>
#include <regex>
#include <string>
#include <unordered_map>

#include "absl/strings/match.h"

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

// Parse models from file. Return vector of models if successful, nullptr
// otherwise.
using ObjectTypeVector = std::vector<ObjectType *>;
std::unique_ptr<ObjectTypeVector> LoadObjectTypes(const char *filename);

static bool contains(const std::string &s, char c) {
  return absl::StrContains(s, c);
}
static bool ParseObjectTypesFromFile(const char *filename,
                                     ObjectTypeVector *parsed_out) {
  static const std::regex emptyline_re("^[ \t]*(#.*)?");
  static const std::regex toplevel_object_re("^([a-zA-Z0-9_]+):");

  // For now, let's just read up to the first type and leave out alternatives
  static const std::regex property_re(
      "^[ \t]+([a-zA-Z_<]+)([\\?\\+]*):[ ]*([a-zA-Z0-9_]+)[ ]*(=[ \t]*(.+))?");

  Location current_location = {filename, 0};
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

    if (std::regex_match(line, emptyline_re)) continue;

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

static bool ValidateTypes(ObjectTypeVector *object_types) {
  std::unordered_map<std::string, ObjectType *> typeByName;

  for (auto &o : *object_types) {
    // We only insert types as they come, so that we can make sure they are
    // used after being defined.
    auto inserted = typeByName.insert({o->name, o});
    if (!inserted.second) {
      std::cerr << o->location << "Duplicate name; previous defined in "
                << inserted.first->second->location << "\n";
      return false;
    }

    // Resolve superclasses
    for (const auto &e : o->extends) {
      const auto &found = typeByName.find(e);
      if (found == typeByName.end()) {
        std::cerr << o->location << "Unknown superclass " << e << "\n";
        return false;
      }
      o->superclasses.push_back(found->second);
    }

    for (auto &p : o->properties) {
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
    for (const auto &p : o->properties) {
      auto inserted = my_property_names.insert({p.name, &p});
      if (inserted.second) continue;
      std::cerr << p.location << "In class '" << o->name
                << "' same name property '" << p.name << "' defined here\n"
                << inserted.first->second->location << "  ... and here\n";
      return false;
    }
    for (const auto &s : o->superclasses) {
      for (const auto &sp : s->properties) {
        auto inserted = my_property_names.insert({sp.name, &sp});
        if (inserted.second) continue;
        const bool is_owner_superclass = (inserted.first->second->owner != o);
        std::cerr << o->location << o->name << " has duplicate property '"
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

std::unique_ptr<ObjectTypeVector> LoadObjectTypes(const char *filename) {
  std::unique_ptr<ObjectTypeVector> result(new ObjectTypeVector());

  if (!ParseObjectTypesFromFile(filename, result.get())) return nullptr;
  if (!ValidateTypes(result.get())) return nullptr;
  return result;
}

void GenerateCode(const char *filename, const char *nlohmann_json_include,
                  const char *gen_namespace, const ObjectTypeVector &objects,
                  FILE *out) {
  fprintf(out, "// Don't modify. Generated from %s\n", filename);
  fprintf(out,
          "#pragma once\n"
          "#include <string>\n"
          "#include <vector>\n");
  fprintf(out, "#include %s\n\n", nlohmann_json_include);

  if (gen_namespace) {
    fprintf(out, "namespace %s {\n", gen_namespace);
  }
  for (const auto &o : objects) {
    fprintf(out, "struct %s", o->name.c_str());
    bool is_first = true;
    for (const auto &e : o->extends) {
      fprintf(out, "%s", is_first ? " :" : ",");
      fprintf(out, " public %s", e.c_str());
      is_first = false;
    }
    fprintf(out, " {\n");
    for (const auto &p : o->properties) {
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
        fprintf(out, "  std::vector<%s> %s", type.c_str(), p.name.c_str());
      } else {
        fprintf(out, "  %s %s", type.c_str(), p.name.c_str());
      }
      if (!p.default_value.empty())
        fprintf(out, " = %s", p.default_value.c_str());
      fprintf(out, ";\n");
      if (p.is_optional) {
        fprintf(out, "  bool has_%s = false;  // optional property\n",
                p.name.c_str());
      }
    }

    // nlohmann::json serialization
    fprintf(out, "\n");
    fprintf(out, "  void Deserialize(const nlohmann::json &j) {\n");
    for (const auto &e : o->extends) {
      fprintf(out, "    %s::Deserialize(j);\n", e.c_str());
    }
    for (const auto &p : o->properties) {
      int indent = 4;
      std::string access_call = "j.at(\"" + p.name + "\")";
      std::string access_deref = access_call + ".";
      if (p.is_optional) {
        fprintf(out,
                "%*sif (auto found = j.find(\"%s\"); found != j.end()) {\n",
                indent, "", p.name.c_str());
        indent += 4;
        fprintf(out, "%*shas_%s = true;\n", indent, "", p.name.c_str());
        access_call = "*found";
        access_deref = "found->";
      }
      if (p.object_type == nullptr || p.is_array) {
        fprintf(out, "%*s%sget_to(%s);\n", indent, "", access_deref.c_str(),
                p.name.c_str());
      } else {
        fprintf(out, "%*s%s.Deserialize(%s);\n", indent, "", p.name.c_str(),
                access_call.c_str());
      }
      if (p.is_optional) {
        fprintf(out, "%*s}\n", indent - 4, "");
      }
    }
    fprintf(out, "  }\n");

    fprintf(out, "  void Serialize(nlohmann::json *j) const {\n");
    for (const auto &e : o->extends) {
      fprintf(out, "    %s::Serialize(j);\n", e.c_str());
    }
    for (const auto &p : o->properties) {
      int indent = 4;
      if (p.is_optional) {
        fprintf(out, "%*sif (has_%s)", indent, "", p.name.c_str());
        indent = 1;
      }
      if (p.object_type == nullptr || p.is_array) {
        fprintf(out, "%*s(*j)[\"%s\"] = %s;\n", indent, "", p.name.c_str(),
                p.name.c_str());
      } else {
        fprintf(out, "%*s%s.Serialize(&(*j)[\"%s\"]);\n", indent, "",
                p.name.c_str(), p.name.c_str());
      }
    }
    fprintf(out, "  }\n");

    fprintf(out, "};\n");  // End of struct

    // functions that are picked up by the nlohmann::json serializer
    // We could generate template code once for all to_json/from_json that take
    // a T obj, but to limit method lookup confusion for other objects that
    // might interact with the json library, let's be explicit for each struct
    fprintf(out,
            "inline void to_json(nlohmann::json &j, const %s &obj) "
            "{ obj.Serialize(&j); }\n",
            o->name.c_str());
    fprintf(out,
            "inline void from_json(const nlohmann::json &j, %s &obj) "
            "{ obj.Deserialize(j); }\n\n",
            o->name.c_str());
  }

  if (gen_namespace) {
    fprintf(out, "}  // %s\n", gen_namespace);
  }
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    fprintf(stderr, "usage: %s [options] <protocol-spec-yaml>\n", argv[0]);
    fprintf(stderr,
            "Options:\n"
            "  -o <filename>     : Output to filename\n"
            "  -j <json-include> : Include path to json.hpp, "
            "including brackets <> or quotes \"\" around.\n"
            "                      Default: '<nlohmann/json.hpp>'\n"
            "  -n <namespace>    : Namespace to generate structs into.\n");
    return 1;
  }

  const char *out_filename = nullptr;
  const char *nlohmann_json_include = "<nlohmann/json.hpp>";
  const char *generated_namespace = nullptr;
  int opt;
  while ((opt = getopt(argc, argv, "o:j:n:")) != -1) {
    switch (opt) {
      case 'o':
        out_filename = optarg;
        break;
      case 'j':
        nlohmann_json_include = optarg;
        break;
      case 'n':
        generated_namespace = optarg;
        break;
      default:
        fprintf(stderr, "Invalid option\n");
        return 1;
    }
  }

  const char *schema_filename = argv[optind];
  auto objects = LoadObjectTypes(schema_filename);
  if (!objects) {
    fprintf(stderr, "Couldn't parse spec\n");
    return 2;
  }

  FILE *out = stdout;
  if (out_filename) {
    out = fopen(out_filename, "w");
    if (!out) {
      perror("opening output file");
      return 3;
    }
  }

  GenerateCode(schema_filename, nlohmann_json_include, generated_namespace,
               *objects, out);
}
