#include "AutoRefactoringModuleUtils.h"
#include "clang/Analysis/CFGStmtMap.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Lex/Preprocessor.h"

using namespace clang::tidy::autorefactorings;

bool Utils::isBlockInCurrentStmt(const clang::CFGBlock *Block,
                                 const Stmt *CurrentStmt,
                                 const SourceManager *Manager) {

  for (auto &&BlockElement : Block->Elements) {
    auto CfgStmtElement = BlockElement.getAs<CFGStmt>();

    if (!CfgStmtElement) {
      return false;
    }

    const auto *CfgStmtElementStmt = CfgStmtElement->getStmt();
    if (CurrentStmt->getBeginLoc() >
            Manager->getExpansionLoc(CfgStmtElementStmt->getBeginLoc()) ||
        CurrentStmt->getEndLoc() <
            Manager->getExpansionLoc(CfgStmtElementStmt->getEndLoc())) {
      return false;
    }
  }
  return true;
}

bool Utils::isOnlyReturnBlock(const clang::CFGBlock *Block,
                              const clang::SourceManager *Manager) {
  if (Block->empty()) {
    return false;
  }

  auto ReturnBlock = Block->back();

  if (auto ReturnBlockCFGStmt = ReturnBlock.getAs<CFGStmt>()) {
    if (const auto *ReturnBlockStmt =
            dyn_cast<ReturnStmt>(ReturnBlockCFGStmt->getStmt())) {
      return Utils::isBlockInCurrentStmt(Block, ReturnBlockStmt, Manager);
    }
    return false;
  }
  return false;
}

const clang::Stmt *Utils::getInterruptStatement(const clang::CFGBlock *Block) {
  auto InterruptInstruction = Block->back();

  if (auto InterruptBlockCFGStmt = InterruptInstruction.getAs<CFGStmt>()) {
    auto *InterruptBlockASTStmt = InterruptBlockCFGStmt->getStmt();

    if (auto *Answer = dyn_cast<ReturnStmt>(InterruptBlockASTStmt)) {
      return Answer;
    }

    if (auto *Answer = dyn_cast<BreakStmt>(InterruptBlockASTStmt)) {
      return Answer;
    }

    if (auto *Answer = dyn_cast<ContinueStmt>(InterruptBlockASTStmt)) {
      return Answer;
    }

    if (auto *Answer = dyn_cast<CXXThrowExpr>(InterruptBlockASTStmt)) {
      return Answer;
    }
  }
  return nullptr;
}

bool Utils::isOnlyIterruptionBlock(const clang::CFGBlock *Block,
                                   const clang::CFG &Cfg,
                                   const clang::SourceManager *Manager) {
  return Block->getBlockID() == Cfg.getExit().getBlockID() ||
         Utils::isOnlyReturnBlock(Block, Manager) ||
         Block->hasNoReturnElement();
}

int Utils::getTokenLenght(SourceLocation Loc,
                          const clang::ASTContext &Context) {
  return clang::Lexer::MeasureTokenLength(Loc, Context.getSourceManager(),
                                          Context.getLangOpts());
};

llvm::StringRef Utils::getSourceText(const clang::SourceRange &Range,
                                     const clang::SourceManager *Manager,
                                     const clang::ASTContext *Context) {
  const auto LangOptions = Context->getLangOpts();
  const auto Start = Manager->getSpellingLoc(Range.getBegin());
  const auto End = clang::Lexer::getLocForEndOfToken(
      Manager->getSpellingLoc(Range.getEnd()), 0, *Manager, LangOptions);
  return clang::Lexer::getSourceText(
      clang::CharSourceRange::getTokenRange(clang::SourceRange{Start, End}),
      *Manager, LangOptions);
}

/// Get the latest Stmt in scope if it is Compound Stmt
const clang::Stmt *Utils::getLastStmt(const Stmt *CurrentStmt) {
  if (auto *CmdCompound = dyn_cast<CompoundStmt>(CurrentStmt)) {
    return CmdCompound->body_back();
  }
  return nullptr;
}