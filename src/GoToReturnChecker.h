#ifndef LLVM_CLANG_TOOLS_EXTRA_CLANG_TIDY_AUTOREFACTORINGS_GOTORETURNCHECKER_H
#define LLVM_CLANG_TOOLS_EXTRA_CLANG_TIDY_AUTOREFACTORINGS_GOTORETURNCHECKER_H

#include "../ClangTidyCheck.h"

namespace clang {
class CFG;
class CFGStmtMap;
} // namespace clang

namespace clang::tidy::autorefactorings {

class GoToReturnChecker : public ClangTidyCheck {
public:
  GoToReturnChecker(StringRef Name, ClangTidyContext *Context);
  bool isLanguageVersionSupported(const LangOptions &LangOpts) const override {
    return true;
  }
  bool runInternal(GotoStmt *GotoStmt,
                   const ast_matchers::MatchFinder::MatchResult &Result,
                   clang::CFG &Cfg);
  void registerMatchers(ast_matchers::MatchFinder *Finder) override;
  void check(const ast_matchers::MatchFinder::MatchResult &Result) override;
  using GotoLabelMap = llvm::DenseMap<int64_t, bool>;

private:
  GotoLabelMap LabelMap;
};
}; // namespace clang::tidy::autorefactorings

#endif // LLVM_CLANG_TOOLS_EXTRA_CLANG_TIDY_AUTOREFACTORINGS_GOTORETURNCHECKER_H
