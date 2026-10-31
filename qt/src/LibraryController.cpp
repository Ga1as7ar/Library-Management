#include "LibraryController.h"

namespace ui {

LibraryController::LibraryController(std::unique_ptr<library::LibraryService> service, QObject* parent)
    : QObject(parent), service_(std::move(service)) {}

}  // namespace ui
