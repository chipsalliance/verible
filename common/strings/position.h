#ifndef VERIBLE_COMMON_STRINGS_POSITION_H_
#define VERIBLE_COMMON_STRINGS_POSITION_H_

#include "absl/strings/string_view.h"

namespace verible {

// Returns the updated column position of text, given a starting column
// position and advancing_text.  Each newline in the advancing_text effectively
// resets the column position back to zero.  All non-newline characters count as
// one space.
int AdvancingTextNewColumnPosition(int old_column_position,
                                   absl::string_view advancing_text);

}  // namespace verible

#endif  // VERIBLE_COMMON_STRINGS_POSITION_H_
