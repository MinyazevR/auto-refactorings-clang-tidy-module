#ifndef LLVM_CLANG_TOOLS_EXTRA_CLANG_TIDY_AUTOREFACTORING_IFELSECHECK_H
#define LLVM_CLANG_TOOLS_EXTRA_CLANG_TIDY_AUTOREFACTORING_IFELSECHECK_H

#include "../ClangTidyCheck.h"

class Rewriter;

namespace clang::tidy::autorefactorings {

class IfElseReturnChecker : public ClangTidyCheck {
public:
  IfElseReturnChecker(StringRef Name, ClangTidyContext *Context);
  bool isLanguageVersionSupported(const LangOptions &LangOpts) const override {
    return LangOpts.C99;
  }
  void storeOptions(ClangTidyOptions::OptionMap &Opts) override;
  void registerMatchers(ast_matchers::MatchFinder *Finder) override;
  void check(const ast_matchers::MatchFinder::MatchResult &Result) override;
  void runInternal(clang::IfStmt *ifStmt,
                   const ast_matchers::MatchFinder::MatchResult &Result,
                   clang::CFG &cfg);

private:
  ulong mIndent = 2;
  bool mNeedShift{};
  std::unique_ptr<Rewriter> mRewrite;
  std::list<clang::FixItHint> mFixList;
  bool reversCondition(const clang::IfStmt *ifStmt,
                       const SourceManager *manager);

  void appendStmt(const clang::CompoundStmt *stmt, const clang::Stmt *stmtToAdd,
                  const clang::SourceManager *manager,
                  const clang::ASTContext *context);

  void indentBlock(const CompoundStmt *stmt, const std::string &string,
                   unsigned long indent);

  void reverseStmt(const clang::IfStmt *ifStmt,
                   const clang::ASTContext &context,
                   const clang::SourceManager *manager, bool needShift);

  bool addIterruptionBlockToStmts(const clang::Stmt *stmt,
                                  const clang::CFG &cfg,
                                  const clang::SourceManager *manager,
                                  const clang::CFGStmtMap *stmtToBlockMap,
                                  const clang::ASTContext *context);
};
}; // namespace clang::tidy::autorefactorings

#endif // LLVM_CLANG_TOOLS_EXTRA_CLANG_TIDY_AUTOREFACTORING_IFELSECHECK_H
