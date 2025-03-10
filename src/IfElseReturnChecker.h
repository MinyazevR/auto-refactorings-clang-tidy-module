#ifndef LLVM_CLANG_TOOLS_EXTRA_CLANG_TIDY_AUTOREFACTORINGS_IFELSERETURNCHECKER_H
#define LLVM_CLANG_TOOLS_EXTRA_CLANG_TIDY_AUTOREFACTORINGS_IFELSERETURNCHECKER_H

#include "../ClangTidyCheck.h"
#include "clang/Rewrite/Core/Rewriter.h"

namespace clang {
class CFG;
class CFGStmtMap;
} // namespace clang

namespace clang::tidy::autorefactorings {

class IfElseReturnChecker : public ClangTidyCheck {
public:
  IfElseReturnChecker(StringRef Name, ClangTidyContext *Context);
  bool isLanguageVersionSupported(const LangOptions &LangOpts) const override {
    return true;
  }
  void storeOptions(ClangTidyOptions::OptionMap &Opts) override;
  void registerMatchers(ast_matchers::MatchFinder *Finder) override;
  void check(const ast_matchers::MatchFinder::MatchResult &Result) override;
  void runInternal(IfStmt *IfStmt,
                   const ast_matchers::MatchFinder::MatchResult &Result,
                   clang::CFG &Cfg);

  void registerPPCallbacks(const SourceManager &SM, Preprocessor *PP,
                           Preprocessor *ModuleExpanderPP) override;
  using PreproccessorEndLocations =
      llvm::DenseMap<FileID, SmallVector<SourceRange>>;

private:
  // The offset required when removing the else block is set by the user in
  // .clang-tidy.
  uint32_t Indent = 2;

  // Whether it is necessary to move the else block is set by the user in
  // .clang-tidy.
  bool NeedShift{};

  bool ReverseOnNotUO{};

  // A rewriter used to track intermediate changes to implement changes to all
  // IfStmts in one pass when reversining surrounding IfStmts, you need to have
  // information about internal ones
  std::unique_ptr<Rewriter> Rewrite;
  SmallVector<clang::FixItHint> FixList;

  PreproccessorEndLocations PPConditionals;

  /// The method required to reverse the ifStmt condition
  /// In the simplest case, it just adds !( beginning of if condition and ) at
  /// the end. The C language is supposed to support a more beautiful style by
  /// type: if (x > y) -> if (x <= y) What is difficult in C++ due to operator
  /// overloading.
  bool reverseCondition(const IfStmt *IfStmt, const SourceManager *Manager,
                        const clang::ASTContext *Context);

  /// Accepts a compound statement and replaces it in the source code with a
  /// string, shifting to the left by indent param.
  void moveBlock(const CompoundStmt *Stmt, const std::string &String,
                 unsigned long Indent);

  /// Add stmt to the original stmt, which is used to pull up blocks.
  ///
  /// For example:
  /// if () {                            if () {
  ///    int x = 1;                          int x = 1;
  /// }               ---------------->      return 1;
  /// else {                             }
  ///    int y = 1;                      else {
  /// }                                      int y = 1;
  /// return 1;                          }
  ///                                    return 1;
  void appendStmt(const CompoundStmt *CmpStmt, const Stmt *StmtToAdd,
                  const SourceManager *Manager, const ASTContext *Context);

  /// A method for reverse IfStmt
  ///
  /// For example:
  /// if () {                            if () {
  ///    int x = 1;                          int y = 1;
  /// }               ---------------->  }
  /// else {                             else {
  ///    int y = 1;                          int x = 1;
  /// }                                  }
  /// return 1;                          return 1;
  void reverseStmt(const IfStmt *IfStmt, const ASTContext &Context,
                   const SourceManager *Manager);

  bool addBlockToStmt(const Stmt *Stmt, const clang::CFG &Cfg,
                      const SourceManager *Manager,
                      const clang::CFGStmtMap *StmtToBlockMap,
                      const ASTContext *Context);
};
}; // namespace clang::tidy::autorefactorings

#endif // LLVM_CLANG_TOOLS_EXTRA_CLANG_TIDY_AUTOREFACTORINGS_IFELSERETURNCHECKER_H
