#ifndef LLVM_CLANG_TOOLS_EXTRA_CLANG_TIDY_AUTOREFACTORINGS_COMMAINIFCHECKER_H
#define LLVM_CLANG_TOOLS_EXTRA_CLANG_TIDY_AUTOREFACTORINGS_COMMAINIFCHECKER_H

#include "../ClangTidyCheck.h"

namespace clang::tidy::autorefactorings {

class CommaInIfChecker : public ClangTidyCheck {
public:
  CommaInIfChecker(StringRef Name, ClangTidyContext *Context);
  bool isLanguageVersionSupported(const LangOptions &LangOpts) const override {
    return LangOpts.C99;
  }
  //   void storeOptions(ClangTidyOptions::OptionMap &Opts) override;
  void registerMatchers(ast_matchers::MatchFinder *Finder) override;
  void check(const ast_matchers::MatchFinder::MatchResult &Result) override;
};
}; // namespace clang::tidy::autorefactorings

#endif // LLVM_CLANG_TOOLS_EXTRA_CLANG_TIDY_AUTOREFACTORINGS_COMMAINIFCHECKER_H
