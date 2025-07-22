#include "SourceHandler.hpp"
#include "ConfigUtils.hpp"
#include <cstring>

#include <glob.h>
#include <memory>
#include <spdlog/spdlog.h>
#include <wordexp.h>

namespace hyprquery {

Hyprlang::CConfig *SourceHandler::s_pConfig = nullptr;
std::string SourceHandler::s_configDir = "";
bool SourceHandler::s_initialized = false;

void SourceHandler::setConfigDir(const std::string &dir) { s_configDir = dir; }

std::string SourceHandler::getConfigDir() { return s_configDir; }

bool SourceHandler::isInitialized() { return s_initialized; }

std::string SourceHandler::expandEnvVars(const std::string &path) {

  if (!path.empty() && path[0] == '~') {
    const char *home = getenv("HOME");
    if (home)
      return std::string(home) + path.substr(1);
  }
  return path;
}

std::vector<std::filesystem::path>
SourceHandler::resolvePath(const std::string &filePath) {
  std::vector<std::filesystem::path> paths;

  std::string normalizedPath = ConfigUtils::normalizePath(filePath);

  spdlog::debug("Normalized path: {}", normalizedPath);

  if (normalizedPath.empty()) {
    spdlog::error("Path is empty after normalization: {}", filePath);
    return paths;
  }

  glob_t glob_result;
  memset(&glob_result, 0, sizeof(glob_t));

  int ret = glob(normalizedPath.c_str(), GLOB_TILDE | GLOB_BRACE, nullptr,
                 &glob_result);

  if (ret != 0) {
    if (ret == GLOB_NOMATCH) {
      spdlog::warn("No matches found for path: {}", normalizedPath);
    } else {
      spdlog::error("Glob error for pattern: {}", normalizedPath);
    }

    std::filesystem::path fallbackPath(normalizedPath);
    if (std::filesystem::exists(fallbackPath.parent_path())) {
      paths.push_back(fallbackPath);
    }
    globfree(&glob_result);
    return paths;
  }

  for (size_t i = 0; i < glob_result.gl_pathc; ++i) {
    std::string pathStr = glob_result.gl_pathv[i];
    std::filesystem::path fsPath(pathStr);
    if (fsPath.is_relative() || fsPath.string().find("./") == 0) {
      fsPath = std::filesystem::weakly_canonical(s_configDir / fsPath);
    } else {
      fsPath = std::filesystem::weakly_canonical(fsPath);
    }
    if (std::filesystem::exists(fsPath.parent_path())) {
      paths.push_back(fsPath);
    } else {
      spdlog::warn("Directory does not exist: {}",
                   fsPath.parent_path().string());
    }
  }
  globfree(&glob_result);

  spdlog::debug("Resolved paths: ");
  for (const auto &p : paths) {
    spdlog::debug("PATHS: {}  ::: {}", s_configDir, p.string());
  }

  return paths;
}

Hyprlang::CParseResult SourceHandler::handleSource(const char *command,
                                                   const char *rawpath) {
  Hyprlang::CParseResult result;
  std::string path = rawpath;

  if (path.length() < 2) {
    result.setError("source= path too short or empty");
    return result;
  }

  std::unique_ptr<glob_t, void (*)(glob_t *)> glob_buf{
      static_cast<glob_t *>(calloc(1, sizeof(glob_t))), [](glob_t *g) {
        if (g) {
          globfree(g);
          free(g);
        }
      }};

  std::string absPath;
  if (path[0] == '~') {
    const char *home = getenv("HOME");
    absPath = home ? std::string(home) + path.substr(1) : path;
  } else if (path[0] != '/') {
    absPath = s_configDir + "/" + path;
  } else {
    absPath = path;
  }

  int r = glob(absPath.c_str(), GLOB_TILDE, nullptr, glob_buf.get());
  if (r != 0) {
    std::string err = std::string("source= globbing error: ") +
                      (r == GLOB_NOMATCH   ? "found no match"
                       : r == GLOB_ABORTED ? "read error"
                                           : "out of memory");
    spdlog::error("{}", err);
    result.setError(err.c_str());
    return result;
  }

  std::string errorsFromParsing;

  for (size_t i = 0; i < glob_buf->gl_pathc; i++) {
    std::string value = glob_buf->gl_pathv[i];
    if (!std::filesystem::is_regular_file(value)) {
      if (std::filesystem::exists(value)) {
        spdlog::warn("source= skipping non-file {}", value);
        continue;
      }
      std::string err = "source= file " + value + " doesn't exist!";
      spdlog::error("{}", err);
      result.setError(err.c_str());
      return result;
    }

    std::string configDirBackup = s_configDir;
    s_configDir = std::filesystem::path(value).parent_path().string();

    auto parseResult = s_pConfig->parseFile(value.c_str());

    s_configDir = configDirBackup;

    if (parseResult.error && errorsFromParsing.empty())
      errorsFromParsing += parseResult.getError();
  }

  if (!errorsFromParsing.empty())
    result.setError(errorsFromParsing.c_str());
  return result;
}

void SourceHandler::registerHandler(Hyprlang::CConfig *config) {
  s_pConfig = config;
  s_initialized = true;

  Hyprlang::SHandlerOptions options;
  options.allowFlags = false;

  config->registerHandler(
      [](const char *command, const char *value) -> Hyprlang::CParseResult {
        return SourceHandler::handleSource(command, value);
      },
      "source", options);

  spdlog::debug("Registered source handler");
}

} // namespace hyprquery