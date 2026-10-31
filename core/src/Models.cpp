#include "library/Models.h"

namespace library {

std::string toString(BookCategory category) {
    switch (category) {
        case BookCategory::Textbook: return "textbook";
        case BookCategory::General: break;
    }
    return "general";
}

std::optional<BookCategory> parseBookCategory(std::string_view text) {
    if (text == "general") return BookCategory::General;
    if (text == "textbook") return BookCategory::Textbook;
    return std::nullopt;
}

}  // namespace library
