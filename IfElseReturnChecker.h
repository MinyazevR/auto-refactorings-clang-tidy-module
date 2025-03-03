#ifndef LLVM_CLANG_TOOLS_EXTRA_CLANG_TIDY_AUTOREFACTORING_IFELSECHECK_H
#define LLVM_CLANG_TOOLS_EXTRA_CLANG_TIDY_AUTOREFACTORING_IFELSECHECK_H

#include "../ClangTidyCheck.h"
#include "clang/Rewrite/Core/Rewriter.h"

class ASTContext;
class CFGStmtMap;
class IfStmt;
class CFG;

using namespace clang;

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
  void runInternal(IfStmt *ifStmt,
                   const ast_matchers::MatchFinder::MatchResult &Result,
                   CFG &cfg);

private:
  ulong mIndent = 2;
  bool mNeedShift{};
  Rewriter Rewrite;
  std::list<clang::FixItHint> mFixList;
  bool reversCondition(const IfStmt *ifStmt, const SourceManager *manager);

  void appendStmt(const CompoundStmt *stmt, const Stmt *stmtToAdd,
                  const clang::SourceManager *manager,
                  const clang::ASTContext *context);

  void indentBlock(const CompoundStmt *stmt, const std::string &string,
                   unsigned long indent);

  void reverseStmt(const IfStmt *ifStmt, const clang::ASTContext &context,
                   const clang::SourceManager *manager, bool needShift);

  bool addIterruptionBlockToStmts(const Stmt *stmt, const CFG &cfg,
                                  const clang::SourceManager *manager,
                                  const CFGStmtMap *stmtToBlockMap,
                                  const clang::ASTContext *context);
};
}; // namespace clang::tidy::autorefactorings

#endif // LLVM_CLANG_TOOLS_EXTRA_CLANG_TIDY_AUTOREFACTORING_IFELSECHECK_H
