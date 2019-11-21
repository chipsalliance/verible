#include "common/strings/position.h"

#include "absl/strings/string_view.h"

namespace verible {

int AdvancingTextNewColumnPosition(int old_column_position,
                                   absl::string_view advancing_text) {
  const auto last_newline = advancing_text.find_last_of('\n');
  if (last_newline == absl::string_view::npos) {
    // No newlines, so treat every character as one column position,
    // even tabs.
    return old_column_position + advancing_text.length();
  }
  // Count characters after the last newline.
  return advancing_text.length() - last_newline - 1;
}

}  // namespace verible
