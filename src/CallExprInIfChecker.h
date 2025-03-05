#ifndef LLVM_CLANG_TOOLS_EXTRA_CLANG_TIDY_AUTOREFACTORINGS_CALLEXPRINIFCHECKER_H
#define LLVM_CLANG_TOOLS_EXTRA_CLANG_TIDY_AUTOREFACTORINGS_CALLEXPRINIFCHECKER_H

#include "../ClangTidyCheck.h"

namespace clang::tidy::autorefactorings {

class CallExpInIfChecker : public ClangTidyCheck {
public:
  CallExpInIfChecker(StringRef Name, ClangTidyContext *Context);
  void storeOptions(ClangTidyOptions::OptionMap &Opts) override;
  bool isLanguageVersionSupported(const LangOptions &LangOpts) const override {
    return LangOpts.C99;
  }
  void registerMatchers(ast_matchers::MatchFinder *Finder) override;
  void check(const ast_matchers::MatchFinder::MatchResult &Result) override;

private:
  int VariableCounter = 0;
  std::string VariablePrefix;
  std::string Pattern;
  std::string ReturnTypePattern;
};
}; // namespace clang::tidy::autorefactorings

#endif // LLVM_CLANG_TOOLS_EXTRA_CLANG_TIDY_AUTOREFACTORINGS_CALLEXPRINIFCHECKER_H
