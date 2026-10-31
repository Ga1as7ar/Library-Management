#include <httplib/httplib.h>

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>

#include "ApiRoutes.h"
#include "library/LibraryService.h"

namespace {

struct Options {
    std::string host = "127.0.0.1";
    int port = 8080;
    std::string database = "library.db";
    std::string publicDir = LIBRARY_WEB_PUBLIC_DIR;  // injected by CMake
    bool seed = true;
};

void printUsage() {
    std::cout << "用法: library_web [--host 127.0.0.1] [--port 8080] [--db library.db] [--public DIR] [--no-seed]\n";
}

Options parseArgs(int argc, char** argv) {
    Options options;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        const auto next = [&]() -> std::string {
            if (i + 1 >= argc) {
                std::cerr << "缺少参数值: " << arg << '\n';
                std::exit(2);
            }
            return argv[++i];
        };
        if (arg == "--host") options.host = next();
        else if (arg == "--port") options.port = std::stoi(next());
        else if (arg == "--db") options.database = next();
        else if (arg == "--public") options.publicDir = next();
        else if (arg == "--no-seed") options.seed = false;
        else if (arg == "-h" || arg == "--help") {
            printUsage();
            std::exit(0);
        } else {
            std::cerr << "未知参数: " << arg << '\n';
            printUsage();
            std::exit(2);
        }
    }
    return options;
}

}  // namespace

int main(int argc, char** argv) {
    const Options options = parseArgs(argc, argv);

    try {
        auto database = std::make_shared<library::Database>(options.database);
        library::LibraryService service(database);
        if (options.seed && service.seedIfEmpty()) std::cout << "已写入示例数据\n";

        httplib::Server server;
        std::mutex serviceMutex;
        library::registerApiRoutes(server, service, serviceMutex);

        // no-cache = always revalidate, so edits to the static UI show up on refresh.
        const httplib::Headers staticHeaders = {{"Cache-Control", "no-cache"}};
        if (!std::filesystem::is_directory(options.publicDir) ||
            !server.set_mount_point("/", options.publicDir, staticHeaders)) {
            std::cerr << "静态资源目录不存在: " << options.publicDir << '\n';
            return 1;
        }
        server.set_file_extension_and_mimetype_mapping("js", "text/javascript; charset=utf-8");
        server.set_logger([](const httplib::Request& req, const httplib::Response& res) {
            if (req.path.rfind("/api/", 0) == 0) std::cout << req.method << ' ' << req.path << " -> " << res.status << '\n';
        });

        std::cout << "图书管理系统 Web 版已启动: http://" << options.host << ':' << options.port << "\n"
                  << "数据库: " << std::filesystem::absolute(options.database).string() << std::endl;
        if (!server.listen(options.host, options.port)) {
            std::cerr << "无法监听 " << options.host << ':' << options.port << '\n';
            return 1;
        }
    } catch (const std::exception& error) {
        std::cerr << "启动失败: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
