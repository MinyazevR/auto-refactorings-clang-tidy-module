#ifndef LLVM_CLANG_TOOLS_EXTRA_CLANG_TIDY_AUTOREFACTORINGS_AUTOREFACTORINGMODULEUTILS_H
#define LLVM_CLANG_TOOLS_EXTRA_CLANG_TIDY_AUTOREFACTORINGS_AUTOREFACTORINGMODULEUTILS_H

namespace clang {
class CFG;
class CFGStmtMap;
class CFGBlock;
class Stmt;
class SourceManager;
class ASTContext;
class SourceLocation;
class SourceRange;
} // namespace clang

namespace llvm {
class StringRef;
} // namespace llvm

namespace clang::tidy::autorefactorings {

class Utils {

public:
  /// Сheck that any statement that is included in the block belongs to a
  /// specific stmt.
  static bool isBlockInCurrentStmt(const clang::CFGBlock *Block,
                                   const Stmt *CurrentStmt,
                                   const SourceManager *Manager);

  /// Checking that the block does not include any meaningful logic other than
  /// ReturnStmt
  static bool isOnlyReturnBlock(const clang::CFGBlock *Block,
                                const clang::SourceManager *Manager);

  static const Stmt *getInterruptStatement(const CFGBlock *Block);

  /// Returns true if it is an exit block for the entire CFG,
  /// a ReturnBlock, or a Block that does not return control.
  static bool isOnlyIterruptionBlock(const CFGBlock *Block, const CFG &Cfg,
                                     const SourceManager *Manager);

  /// This function is designed to get the length of the token to handle offsets
  /// correctly.
  static int getTokenLenght(SourceLocation Loc, const ASTContext &Context);

  /// Get the source text using CharRange and SourceManager
  static llvm::StringRef getSourceText(const SourceRange &Range,
                                       const SourceManager *Manager,
                                       const ASTContext *Context);

  /// Get the latest Stmt in scope if it is Compound Stmt
  static const Stmt *getLastStmt(const Stmt *CurrentStmt);
};

}; // namespace clang::tidy::autorefactorings

#endif // LLVM_CLANG_TOOLS_EXTRA_CLANG_TIDY_AUTOREFACTORINGS_AUTOREFACTORINGMODULEUTILS_H