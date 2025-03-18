#include "GoToReturnChecker.h"
#include "AutoRefactoringModuleUtils.h"
#include "clang/AST/ParentMap.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Analysis/CFG.h"
#include "clang/Analysis/CFGStmtMap.h"
#include "clang/Lex/Preprocessor.h"

using namespace clang;
using namespace clang::ast_matchers;
using namespace clang::tidy::autorefactorings;

namespace {

class GotoVisitor : public RecursiveASTVisitor<GotoVisitor> {

  GoToReturnChecker *Checker;
  const MatchFinder::MatchResult &Result;
  clang::CFG &Cfg;

public:
  GotoVisitor(GoToReturnChecker *Checker,
              const MatchFinder::MatchResult &Result, clang::CFG &Cfg)
      : Checker(Checker), Result(Result), Cfg(Cfg) {}

  bool VisitGotoStmt(clang::GotoStmt *CurrentGotoStmt) {
    Checker->runInternal(CurrentGotoStmt, Result, Cfg);
    return true;
  }
};

} // namespace

GoToReturnChecker::GoToReturnChecker(StringRef Name, ClangTidyContext *Context)
    : ClangTidyCheck(Name, Context) {}

void GoToReturnChecker::check(const MatchFinder::MatchResult &Result) {
  const auto *FunctionDecl =
      Result.Nodes.getNodeAs<clang::FunctionDecl>("functionDecl");

  auto Cfg = CFG::buildCFG(FunctionDecl, FunctionDecl->getBody(),
                           Result.Context, CFG::BuildOptions());

  GotoVisitor Visitor(this, Result, *Cfg.get());
  Visitor.TraverseDecl(const_cast<clang::FunctionDecl *>(FunctionDecl));
}

bool GoToReturnChecker::runInternal(
    GotoStmt *GotoStmt, const ast_matchers::MatchFinder::MatchResult &Result,
    clang::CFG &Cfg) {

  const auto *GotoStmtLabel = GotoStmt->getLabel();
  const auto *FunctionDecl =
      Result.Nodes.getNodeAs<clang::FunctionDecl>("functionDecl");

  auto LabelMapIterator = LabelMap.find(GotoStmtLabel->getID());

  if (LabelMapIterator == LabelMap.end()) {
    LabelMap[GotoStmtLabel->getID()] = false;
  } else if (!LabelMapIterator->getSecond()) {
    return false;
  }

  if (!GotoStmtLabel || !FunctionDecl) {
    return false;
  }

  std::unique_ptr<ParentMap> PMap(new ParentMap(FunctionDecl->getBody()));
  std::unique_ptr<clang::CFGStmtMap> StmtToBlockMap(
      clang::CFGStmtMap::Build(&Cfg, PMap.get()));

  const auto *GotoStmtBlock = StmtToBlockMap->getBlock(GotoStmt);
  const auto *GotoStmtLabelStmt = GotoStmtLabel->getStmt();

  if (!GotoStmtBlock || !GotoStmtLabelStmt) {
    return false;
  }

  const auto *GotoStmtLabelBlock = StmtToBlockMap->getBlock(GotoStmt);
  const auto *const GotoStmtLabelStmtBlock =
      StmtToBlockMap->getBlock(GotoStmtLabelStmt);

  if (!GotoStmtLabelBlock || !GotoStmtLabelStmtBlock) {
    return false;
  }

  if (!Utils::isOnlyReturnBlock(GotoStmtLabelStmtBlock, Result.SourceManager)) {
    return false;
  }

  if (GotoStmtLabelStmtBlock->succ_size() != 1) {
    return false;
  }
  auto GotoStmtLabelStmtBlockSuccesor = *(GotoStmtLabelStmtBlock->succ_begin());
  if (!GotoStmtLabelStmtBlockSuccesor ||
      GotoStmtLabelStmtBlockSuccesor->getBlockID() !=
          Cfg.getExit().getBlockID()) {
    return false;
  }

  auto Diag =
      diag(GotoStmt->getBeginLoc(), "It looks like you're using a Goto on a "
                                    "label with a only Return Statement");
  const auto *InterruptStmt =
      Utils::getInterruptStatement(GotoStmtLabelStmtBlock);

  const auto ReturnText = clang::Lexer::getSourceText(
      clang::CharSourceRange::getTokenRange(InterruptStmt->getSourceRange()),
      *Result.SourceManager, clang::LangOptions());

  Diag << FixItHint::CreateReplacement(GotoStmt->getSourceRange(), ReturnText);
  LabelMap[GotoStmtLabel->getID()] = true;
  return true;
}

void GoToReturnChecker::registerMatchers(MatchFinder *Finder) {
  Finder->addMatcher(
      functionDecl(isDefinition(),
                   unless(anyOf(isDefaulted(), isDeleted(), isWeak())))
          .bind("functionDecl"),
      this);
}
