#pragma once

#include <mutex>

namespace httplib {
class Server;
}

namespace library {

class LibraryService;

/// Registers the REST API under /api on the given server.
/// All service calls are serialised through `mutex` (SQLite connection is single-threaded).
void registerApiRoutes(httplib::Server& server, LibraryService& service, std::mutex& mutex);

}  // namespace library
