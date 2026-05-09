#include <Iris/Backend/module.hpp>
#include <Iris/Frontend/lexer.hpp>
#include <Iris/Frontend/parser.hpp>
#include <Iris/Frontend/resolver.hpp>
#include <Iris/Runtime/builtins.hpp>
#include <Iris/Runtime/vm.hpp>
#include <algorithm>
#include <fstream>
#include <sstream>

namespace Iris::Backend {

std::vector<std::filesystem::path>
ModuleLoader::getModuleSearchPaths(const std::filesystem::path &importingFile) {
  std::vector<std::filesystem::path> paths;

  paths.push_back(".");
  paths.push_back(importingFile.parent_path());

  std::filesystem::path projectRoot = importingFile.parent_path().parent_path();
  if (projectRoot != ".")
    paths.push_back(projectRoot);

  const char *envPath = std::getenv("IRIS_MODULE_PATH");
  if (envPath) {
    std::istringstream iss(envPath);
    std::string path;
#ifdef _WIN32
    const char separator = ';';
#else
    const char separator = ':';
#endif
    while (std::getline(iss, path, separator)) {
      if (!path.empty())
        paths.push_back(path);
    }
  }

  return paths;
}

std::optional<std::filesystem::path>
ModuleLoader::findPackageDir(const std::string &collection,
                             const std::vector<std::string> &segments,
                             const std::filesystem::path &importingFile) {
  auto searchPaths = getModuleSearchPaths(importingFile);

  for (const auto &root : searchPaths) {
    std::filesystem::path dir = root / collection;
    for (const auto &seg : segments)
      dir /= seg;

    if (std::filesystem::exists(dir) && std::filesystem::is_directory(dir))
      return dir;
  }

  return std::nullopt;
}

static std::string makeImportKey(const std::string &collection,
                                 const std::vector<std::string> &segments) {
  std::string key = collection + ":";
  for (size_t i = 0; i < segments.size(); ++i) {
    if (i > 0)
      key += '/';
    key += segments[i];
  }
  return key;
}

std::vector<LoadedPackage>
ModuleLoader::loadImports(std::span<const Frontend::ExprPtr> program,
                          const std::filesystem::path &importingFile) {
  std::vector<LoadedPackage> result;

  for (const Frontend::ExprPtr &expr : program) {
    const Frontend::Import *imp = std::get_if<Frontend::Import>(&expr->val);
    if (!imp)
      continue;

    std::string key = makeImportKey(imp->collection, imp->pathSegments);

    if (m_loaded.count(key)) {
      LoadedPackage cached = m_loaded.at(key);
      cached.localName = imp->localName;
      result.push_back(std::move(cached));
      continue;
    }

    auto packageDirOpt =
        findPackageDir(imp->collection, imp->pathSegments, importingFile);

    if (!packageDirOpt.has_value()) {
      std::string displayPath = imp->collection;
      for (const auto &seg : imp->pathSegments)
        displayPath += '/' + seg;

      m_bag.emit(Common::Diagnostic{
          .severity = Common::Severity::Error,
          .code = std::nullopt,
          .hint = std::string("create a directory '" + displayPath +
                              "/' in one of the module search paths, "
                              "or set IRIS_MODULE_PATH"),
          .message = "package '" + key + "' not found",
          .filename = importingFile.string(),
          .sourceLine = {},
          .span = imp->span,
      });
      continue;
    }

    LoadedPackage pkg =
        loadPackage(key, imp->localName, *packageDirOpt, importingFile);

    m_loaded[key] = pkg;
    result.push_back(std::move(pkg));
  }

  return result;
}

LoadedPackage
ModuleLoader::loadPackage(const std::string &key, const std::string &localName,
                          const std::filesystem::path &packageDir,
                          const std::filesystem::path &importingFile) {
  if (m_visiting.count(key)) {
    m_bag.emit(Common::Diagnostic{
        .severity = Common::Severity::Error,
        .code = std::nullopt,
        .hint = std::string("circular imports are not allowed"),
        .message = "circular import detected for package '" + key + "'",
        .filename = importingFile.string(),
        .sourceLine = {},
        .span = {0, 0, 0, 0},
    });
    return LoadedPackage{key, localName, {}};
  }

  m_visiting.insert(key);

  LoadedPackage pkg{key, localName, {}};

  for (const auto &entry : std::filesystem::directory_iterator(packageDir)) {
    if (!entry.is_regular_file())
      continue;
    if (entry.path().extension() != ".is")
      continue;

    auto compiled = compilePackageFile(entry.path(), key);
    if (!compiled)
      continue;

    for (ExportedBinding &b : compiled->exports) {
      auto it = std::find_if(
          pkg.exports.begin(), pkg.exports.end(),
          [&](const ExportedBinding &e) { return e.name == b.name; });
      if (it != pkg.exports.end())
        *it = std::move(b);
      else
        pkg.exports.push_back(std::move(b));
    }
  }

  m_visiting.erase(key);
  return pkg;
}

std::optional<LoadedPackage>
ModuleLoader::compilePackageFile(const std::filesystem::path &filePath,
                                 const std::string &key) {
  std::ifstream file{filePath};
  if (!file) {
    m_bag.emit(Common::Diagnostic{
        .severity = Common::Severity::Error,
        .code = std::nullopt,
        .hint = std::nullopt,
        .message = "could not open package file '" + filePath.string() + "'",
        .filename = filePath.string(),
        .sourceLine = {},
        .span = {0, 0, 0, 0},
    });
    return std::nullopt;
  }

  std::ostringstream buf;
  buf << file.rdbuf();
  std::string source = buf.str();

  Common::DiagnosticBag fileBag(m_opts);
  std::string pathStr = filePath.string();

  Frontend::Lexer lexer(source, pathStr, fileBag);
  auto tokens = lexer.tokenize();
  if (fileBag.hasErrors()) {
    fileBag.printSummary();
    return std::nullopt;
  }

  Frontend::Parser parser(tokens, pathStr, source, fileBag);
  auto ast = parser.parse();
  if (fileBag.hasErrors()) {
    fileBag.printSummary();
    return std::nullopt;
  }

  loadImports(ast, filePath);
  if (m_bag.hasErrors())
    return std::nullopt;

  Frontend::Resolver resolver(pathStr, source, fileBag);

  std::vector<std::pair<uint32_t, std::string>> builtinRegs;
  for (const std::string &name : Runtime::kBuiltinNames) {
    uint32_t idx = resolver.declareExternal(name);
    builtinRegs.emplace_back(idx, name);
  }

  resolver.resolve(ast);
  if (fileBag.hasErrors()) {
    fileBag.printSummary();
    return std::nullopt;
  }

  Compiler compiler(pathStr, source, fileBag);

  for (const auto &[idx, name] : builtinRegs)
    compiler.registerBuiltin(idx, name);

  auto chunk = compiler.compile(ast);
  if (fileBag.hasErrors()) {
    fileBag.printSummary();
    return std::nullopt;
  }

  Runtime::VM vm(pathStr, source, fileBag);
  auto _ = vm.run(chunk);
  if (fileBag.hasErrors()) {
    fileBag.printSummary();
    return std::nullopt;
  }

  LoadedPackage pkg{key, "", {}};
  for (auto &[name, value] : vm.exportedBindings())
    pkg.exports.push_back(ExportedBinding{name, value});

  return pkg;
}
} // namespace Iris::Backend