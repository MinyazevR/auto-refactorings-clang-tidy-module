#ifndef LLVM_CLANG_TOOLS_EXTRA_CLANG_TIDY_AUTOREFACTORINGS_RENAMEGHIDRAMEMBERVARIABLE_H
#define LLVM_CLANG_TOOLS_EXTRA_CLANG_TIDY_AUTOREFACTORINGS_RENAMEGHIDRAMEMBERVARIABLE_H

#include "../ClangTidyCheck.h"

namespace clang::tidy::autorefactorings {

class RenameGhidraMemberVariable : public ClangTidyCheck {
public:
  RenameGhidraMemberVariable(StringRef Name, ClangTidyContext *Context);
  bool isLanguageVersionSupported(const LangOptions &LangOpts) const override {
    return true;
  }
  void registerMatchers(ast_matchers::MatchFinder *Finder) override;
  void check(const ast_matchers::MatchFinder::MatchResult &Result) override;
};
}; // namespace clang::tidy::autorefactorings

#endif // LLVM_CLANG_TOOLS_EXTRA_CLANG_TIDY_AUTOREFACTORINGS_RENAMEGHIDRAMEMBERVARIABLE_H
