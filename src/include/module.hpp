#pragma once

#include "src/include/compiler.hpp"
#include "src/include/error.hpp"

#include <filesystem>
#include <unordered_set>
#include <vector>

struct ExportedBinding {
  std::string name;
  Value value;
};

struct LoadedPackage {
  std::string name;
  std::string localName;
  std::vector<ExportedBinding> exports;
};

class ModuleLoader {
public:
  explicit ModuleLoader(const CompilerOptions &opts, DiagnosticBag &bag)
      : m_opts(opts), m_bag(bag) {}

  std::vector<LoadedPackage>
  loadImports(std::span<const ExprPtr> program,
              const std::filesystem::path &importingFile);

private:
  const CompilerOptions &m_opts;
  DiagnosticBag &m_bag;

  std::unordered_set<std::string> m_visiting;
  std::unordered_map<std::string, LoadedPackage> m_loaded;

  LoadedPackage loadPackage(const std::string &key,
                            const std::string &localName,
                            const std::filesystem::path &packageDir,
                            const std::filesystem::path &importingFile);

  std::optional<LoadedPackage>
  compilePackageFile(const std::filesystem::path &filePath,
                     const std::string &packageName);

  std::vector<std::filesystem::path>
  getModuleSearchPaths(const std::filesystem::path &importingFile);

  std::optional<std::filesystem::path>
  findPackageDir(const std::string &collection,
                 const std::vector<std::string> &segments,
                 const std::filesystem::path &importingFile);
};
