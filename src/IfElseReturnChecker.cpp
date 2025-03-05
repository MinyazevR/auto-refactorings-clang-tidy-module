#include "IfElseReturnChecker.h"
#include "clang/AST/ParentMap.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Analysis/Analyses/CFGReachabilityAnalysis.h"
#include "clang/Analysis/CFG.h"
#include "clang/Analysis/CFGStmtMap.h"
#include "clang/Lex/Preprocessor.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include <sstream>

using namespace clang;
using namespace clang::ast_matchers;
using namespace clang::tidy::autorefactorings;

namespace {
class IfStmtVisitor final : public RecursiveASTVisitor<IfStmtVisitor> {

  IfElseReturnChecker *Checker;
  const MatchFinder::MatchResult &Result;
  clang::CFG &Cfg;

public:
  IfStmtVisitor(IfElseReturnChecker *Checker,
                const MatchFinder::MatchResult &Result, clang::CFG &Cfg)
      : Checker(Checker), Result(Result), Cfg(Cfg) {}

  bool VisitIfStmt(clang::IfStmt *IfStmt) {

    if (IfStmt->hasElseStorage()) {
      if (isa<clang::IfStmt>(IfStmt->getElse())) {
        return true;
      }

      Checker->runInternal(IfStmt, Result, Cfg);
      return true;
    }
    return RecursiveASTVisitor::VisitIfStmt(IfStmt);
  }

  bool TraverseStmt(Stmt *CurrentStmt) {
    if (!CurrentStmt)
      return true;

    if (auto *IfStmt = dyn_cast<clang::IfStmt>(CurrentStmt)) {
      IfStmt->dumpPretty(*Result.Context);

      if (auto *ThenStmt = IfStmt->getThen()) {
        TraverseStmt(ThenStmt);
      }
      if (auto *ElseStmt = IfStmt->getElse()) {
        TraverseStmt(ElseStmt);
      }
      return VisitIfStmt(IfStmt);
    }

    return RecursiveASTVisitor::TraverseStmt(CurrentStmt);
  }
};
} // namespace

/// This function is designed to get the length of the token to handle offsets
/// correctly.
static int getTokenLenght(SourceLocation Loc,
                          const clang::ASTContext &Context) {
  return clang::Lexer::MeasureTokenLength(Loc, Context.getSourceManager(),
                                          Context.getLangOpts());
};

/// Сheck that any statement that is included in the block belongs to a specific
/// stmt.
static bool isBlockInCurrentIfStmt(const clang::CFGBlock *Block,
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

/// Checking that the block does not include any meaningful logic other than
/// ReturnStmt
static bool isOnlyReturnBlock(const clang::CFGBlock *Block,
                              const clang::SourceManager *Manager) {
  auto ReturnBlock = Block->back();

  if (auto ReturnBlockCFGStmt = ReturnBlock.getAs<CFGStmt>()) {
    if (const auto *ReturnBlockStmt =
            dyn_cast<ReturnStmt>(ReturnBlockCFGStmt->getStmt())) {
      return isBlockInCurrentIfStmt(Block, ReturnBlockStmt, Manager);
    }
    return false;
  }
  return false;
}

/// Get the source text using CharRange and SourceManager
static llvm::StringRef getSourceText(const clang::CharSourceRange &Range,
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

/// Get Interrupt Stmt of block
static const Stmt *getInterruptStatement(const clang::CFGBlock *Block) {
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

/// Get the latest Stmt in scope if it is Compound Stmt
static const Stmt *getLastStmt(const Stmt *CurrentStmt) {
  if (auto *CmdCompound = dyn_cast<CompoundStmt>(CurrentStmt)) {
    return CmdCompound->body_back();
  }
  return nullptr;
}

/// Returns true if it is an exit block for the entire CFG,
/// a ReturnBlock, or a Block that does not return control.
static bool isOnlyIterruptionBlock(const clang::CFGBlock *Block,
                                   const clang::CFG &Cfg,
                                   const clang::SourceManager *Manager) {
  return Block->getBlockID() == Cfg.getExit().getBlockID() ||
         isOnlyReturnBlock(Block, Manager) || Block->hasNoReturnElement();
}

/// It is used to calculate the weight of the Then or Else Compound Stmt of the
/// main IfStmt,
// on the basis of which a decision will be made about flipping the if and else
// bodies
static int getNodeWeight(const Stmt *Node, clang::SourceManager *Manager) {
  auto Start = Manager->getExpansionRange(Node->getSourceRange()).getBegin();
  auto End = Manager->getExpansionRange(Node->getSourceRange()).getEnd();
  return Manager->getExpansionLineNumber(End) -
         Manager->getSpellingLineNumber(Start);
}

/// When using spaces, it is difficult to understand what Indent is, so the user
/// is prompted
// to use the Indent parameter in .clang-tidy.
// CheckOptions:
// - key:             if-else-refactor.Indent
//   value:           1
IfElseReturnChecker::IfElseReturnChecker(StringRef Name,
                                         ClangTidyContext *Context)
    : ClangTidyCheck(Name, Context), Indent(Options.get("Indent", 4)),
      NeedShift(Options.get("NeedShift", false)),
      Rewrite(std::make_unique<Rewriter>()) {}

void IfElseReturnChecker::registerMatchers(MatchFinder *Finder) {
  Finder->addMatcher(
      functionDecl(isDefinition(),
                   unless(anyOf(isDefaulted(), isDeleted(), isWeak())))
          .bind("functionDecl"),
      this);
}

void IfElseReturnChecker::runInternal(
    IfStmt *IfStmt, const ast_matchers::MatchFinder::MatchResult &Result,
    clang::CFG &Cfg) {

  auto IfLocation = IfStmt->getBeginLoc();
  auto *const Manager = Result.SourceManager;

  if (Manager->isMacroArgExpansion(IfLocation) ||
      Manager->isMacroBodyExpansion(IfLocation) ||
      Manager->isInExternCSystemHeader(IfLocation) ||
      Manager->isInSystemHeader(IfLocation) ||
      Manager->isInSystemMacro(IfLocation)) {
    return;
  }

  const auto *ThenStmt = IfStmt->getThen();
  const auto *ElseStmt = IfStmt->getElse();

  bool IsThenFirst =
      getNodeWeight(ThenStmt, Manager) <= getNodeWeight(ElseStmt, Manager);
  auto *TargetStmt = IsThenFirst ? ThenStmt : ElseStmt;

  const auto *FunctionDecl =
      Result.Nodes.getNodeAs<clang::FunctionDecl>("functionDecl");

  std::unique_ptr<ParentMap> PMap(new ParentMap(FunctionDecl->getBody()));
  std::unique_ptr<clang::CFGStmtMap> StmtToBlockMap(
      clang::CFGStmtMap::Build(&Cfg, PMap.get()));

  auto *Context = Result.Context;

  if (NeedShift) {
    if (!addBlockToStmt(TargetStmt, Cfg, Manager, StmtToBlockMap.get(),
                        Context)) {
      NeedShift = false;
    }
  }

  if (!Context || !IsThenFirst) {
    if (!reversCondition(IfStmt, Manager)) {
      return;
    }
    reverseStmt(IfStmt, *Context, Manager);
  } else {
    if (!NeedShift) {
      return;
    }
    if (auto *CompoundElseStmt = dyn_cast<CompoundStmt>(ElseStmt)) {

      auto ElseLBracLoc = CompoundElseStmt->getLBracLoc();
      auto ElseRBracLoc = CompoundElseStmt->getRBracLoc();

      auto Left = Manager->getExpansionLoc(ElseLBracLoc)
                      .getLocWithOffset(getTokenLenght(ElseLBracLoc, *Context));
      auto Right = Manager->getExpansionLoc(ElseRBracLoc).getLocWithOffset(-1);

      auto ElseText = Rewrite->getRewrittenText(
          CharSourceRange::getTokenRange(Left, Right));
      moveBlock(CompoundElseStmt, ElseText, Indent);
    }

    if (auto *CompoundThenStmt = dyn_cast<CompoundStmt>(ThenStmt)) {
      auto ElseLength = getTokenLenght(IfStmt->getElseLoc(), *Context);
      clang::SourceRange Range(
          Manager->getExpansionLoc(
              CompoundThenStmt->getRBracLoc().getLocWithOffset(1)),
          IfStmt->getElseLoc().getLocWithOffset(ElseLength));
      FixList.push_back(FixItHint::CreateRemoval(Range));
      Rewrite->RemoveText(Range);
    } else {
      FixList.push_back(FixItHint::CreateRemoval(IfStmt->getElseLoc()));
      Rewrite->RemoveText(IfStmt->getElseLoc());
    }
  }

  DiagnosticBuilder Diag =
      diag(IfStmt->getBeginLoc(),
           "It seems like it makes sense to swap then and else branches.");

  for (auto &&Fix : FixList) {

    Diag << Fix;
  }

  FixList.clear();
  return;
}

void IfElseReturnChecker::storeOptions(ClangTidyOptions::OptionMap &Opts) {
  Options.store(Opts, "Indent", Indent);
  Options.store(Opts, "NeedShift", NeedShift);
}

void IfElseReturnChecker::check(const MatchFinder::MatchResult &Result) {
  const auto LangOptions = Result.Context->getLangOpts();
  auto *const Manager = Result.SourceManager;

  Rewrite->setSourceMgr(*Manager, LangOptions);

  const auto *FunctionDecl =
      Result.Nodes.getNodeAs<clang::FunctionDecl>("functionDecl");

  auto Cfg = CFG::buildCFG(FunctionDecl, FunctionDecl->getBody(),
                           Result.Context, CFG::BuildOptions());

  IfStmtVisitor Visitor(this, Result, *Cfg.get());
  Visitor.TraverseDecl(const_cast<clang::FunctionDecl *>(FunctionDecl));
}

/// if (cond) ----> if (!(cond))
/// if (x > y) ----> if (x <= y)
bool IfElseReturnChecker::reversCondition(const IfStmt *IfStmt,
                                          const clang::SourceManager *Manager) {

  // if ifStmt is init as if (auto x = ....)
  if (IfStmt->hasInitStorage()) {
    return false;
  }

  // Negating the condition
  const auto *Condition = IfStmt->getCond();
  if (const auto *Then = dyn_cast<CompoundStmt>(IfStmt->getThen())) {
    auto ExpansionBeginLoc = Manager->getExpansionLoc(Condition->getBeginLoc());
    auto ExpansionEndLoc = Manager->getExpansionLoc(Then->getLBracLoc());

    Rewrite->InsertText(ExpansionBeginLoc, "!(");
    Rewrite->InsertText(ExpansionEndLoc.getLocWithOffset(-1), ")");
    FixList.push_back(FixItHint::CreateInsertion(ExpansionBeginLoc, "!("));
    FixList.push_back(
        FixItHint::CreateInsertion(ExpansionEndLoc.getLocWithOffset(-1), ")"));
    return true;
  }

  return false;
}

/// We collect all the blocks that are the boundary of the Stmt, starting with
/// exit. If a block is found that is not included in this Stmt, but is
/// achievable from it, as well as meaningful, the analysis stops, as this means
/// that there are execution paths outside the Stmt, which prevents it from
/// being interrupted in advance with a conservative approach. Otherwise, the
/// block is returned, which must be tightened to interrupt the desired branch.
static const clang::CFGBlock *getInterruptBlockForStmt(
    const clang::CFG &Cfg, const clang::SourceManager *Manager,
    const Stmt *CurrentStmt, const clang::ASTContext *Context,
    const clang::CFGStmtMap *StmtToBlockMap) {

  const auto ExitBlock = Cfg.getExit();
  std::set<const clang::CFGBlock *> Frontier{&ExitBlock};

  for (auto &&Block : ExitBlock.preds()) {
    if (!Block) {
      return nullptr;
    }
    if (isOnlyIterruptionBlock(Block, Cfg, Manager)) {
      Frontier.insert(Block);
    }
  }

  CFGReverseBlockReachabilityAnalysis Analysis(Cfg);
  const clang::CFGBlock *MBlock = nullptr;

  const auto *CmpdStmt = dyn_cast<CompoundStmt>(CurrentStmt);
  if (!CmpdStmt) {
    return nullptr;
  }

  const auto *FstStmt = *CmpdStmt->body_begin();
  if (!FstStmt) {
    return nullptr;
  }

  const auto *FstBlock = StmtToBlockMap->getBlock(FstStmt);

  if (!FstBlock) {
    return nullptr;
  }

  for (auto &&Block : Frontier) {
    if (!Block) {
      return nullptr;
    }
    for (auto BlockPred : Block->preds()) {
      auto IsBlockInCurrentStmt =
          isBlockInCurrentIfStmt(BlockPred, CurrentStmt, Manager);
      auto CurrentBlockIsReachable = Analysis.isReachable(FstBlock, BlockPred);
      if (!isOnlyIterruptionBlock(BlockPred, Cfg, Manager)) {
        if (!IsBlockInCurrentStmt && CurrentBlockIsReachable) {
          return nullptr;
        }
        continue;
      }
      if (!IsBlockInCurrentStmt && CurrentBlockIsReachable) {
        if (MBlock) {
          return nullptr;
        }
        MBlock = BlockPred;
      }
    }
  }

  return MBlock;
}

/// Adds to the source text the text of the block that needs to be tightened to
/// interrupt the branch.
void IfElseReturnChecker::appendStmt(const CompoundStmt *CmdStmt,
                                     const Stmt *StmtToAdd,
                                     const clang::SourceManager *Manager,
                                     const clang::ASTContext *Context) {

  auto *LastStmt = CmdStmt->body_back();

  // Get source text for new stmt
  auto StmtToAddSourceText =
      getSourceText(Manager->getExpansionRange(StmtToAdd->getSourceRange()),
                    Manager, Context);

  // Get range and location for lastStmt in CompoundStmt
  auto LastStmtRange = Manager->getExpansionRange(LastStmt->getSourceRange());
  auto LastTokenLoc = Manager->getSpellingLoc(LastStmtRange.getEnd());

  // Take a transfer for the last stmt to use it to insert a new one
  auto Indent = clang::Lexer::getIndentationForLine(
      Manager->getExpansionLoc(LastStmt->getBeginLoc()), *Manager);

  // You need to insert a new instruction immediately after the last one,
  // so you need to find the place to use ";"
  SourceLocation SemiLoc(clang::Lexer::findLocationAfterToken(
      LastTokenLoc, clang::tok::semi, *Manager, Context->getLangOpts(), false));

  if (SemiLoc.isInvalid()) {
    SemiLoc = clang::Lexer::getLocForEndOfToken(LastTokenLoc, 0, *Manager,
                                                Context->getLangOpts());
  }

  std::string Text = "\n" + Indent.str() + StmtToAddSourceText.str();

  Rewrite->InsertTextAfterToken(SemiLoc, Text);
  FixList.push_back(FixItHint::CreateInsertion(SemiLoc, Text));
}

/// It's not the most efficient way to move a block to the left by the specified
/// number of characters.
void IfElseReturnChecker::moveBlock(const CompoundStmt *Stmt,
                                    const std::string &String,
                                    unsigned long MainIndent) {

  std::vector<std::string> Vector{"\n"};
  std::stringstream Stream(String);
  std::string Token;

  bool First = true;
  while (std::getline(Stream, Token, '\n')) {
    if (First) {
      if (Token.empty() || Token.find_first_not_of(" \t") == std::string::npos)
        continue;
    }
    First = false;
    auto StringStart = Token.find_first_not_of(" \t");

    if (StringStart == std::string::npos || StringStart < MainIndent) {
      Vector.push_back(Token);
      continue;
    }

    auto TrimToken = Token.erase(0, MainIndent);
    Vector.push_back(TrimToken);
  }

  auto LastString = Vector.back();
  if (LastString.find_first_not_of(" \t") == std::string::npos) {
    Vector.pop_back();
  }

  std::stringstream InputStream;
  copy(Vector.begin(), Vector.end(),
       std::ostream_iterator<std::string>(InputStream, "\n"));

  auto Text = InputStream.str();
  Text.pop_back();

  Rewrite->RemoveText(Stmt->getSourceRange());
  Rewrite->InsertText(Stmt->getBeginLoc(), Text);

  FixList.push_back(FixItHint::CreateRemoval(Stmt->getSourceRange()));
  FixList.push_back(FixItHint::CreateInsertion(Stmt->getBeginLoc(), Text));
}

static clang::CharSourceRange
getCompoundStmtRange(const CompoundStmt *Stmt, const clang::ASTContext &Context,
                     const SourceManager *Manager, unsigned long Indent = 1) {
  auto LBracLoc = Stmt->getLBracLoc();
  auto RBracLoc = Stmt->getRBracLoc();

  auto LeftBorder = Manager->getExpansionLoc(LBracLoc).getLocWithOffset(
      getTokenLenght(LBracLoc, Context));
  auto RightBorder = Manager->getExpansionLoc(RBracLoc);

  return CharSourceRange::getTokenRange(LeftBorder.getLocWithOffset(-Indent),
                                        RightBorder.getLocWithOffset(Indent));
}

/// Swap the then and else branches
void IfElseReturnChecker::reverseStmt(const IfStmt *IfStmt,
                                      const clang::ASTContext &Context,
                                      const clang::SourceManager *Manager) {

  auto *ElseStmt = IfStmt->getElse();
  auto *ThenStmt = IfStmt->getThen();

  clang::CharSourceRange IfTokenRange;
  clang::CharSourceRange ElseTokenRange;

  auto *CompoundIfStmt = dyn_cast<CompoundStmt>(ThenStmt);
  auto *CompoundElseStmt = dyn_cast<CompoundStmt>(ElseStmt);

  if (!CompoundIfStmt) {
    IfTokenRange = CharSourceRange::getTokenRange(ThenStmt->getSourceRange());
  } else {
    auto WithBrackets = !CompoundElseStmt;
    IfTokenRange =
        WithBrackets
            ? getCompoundStmtRange(CompoundIfStmt, Context, Manager)
            : getCompoundStmtRange(CompoundIfStmt, Context, Manager, -1);
  }

  if (!CompoundElseStmt) {
    auto ElseLenght = getTokenLenght(IfStmt->getElseLoc(), Context);
    auto ElseStartLocation = IfStmt->getElseLoc().getLocWithOffset(ElseLenght);
    auto ElseEndLocation = Manager->getExpansionLoc(IfStmt->getEndLoc());

    SourceLocation SemiLoc(clang::Lexer::findLocationAfterToken(
        ElseEndLocation, clang::tok::semi, *Manager, Context.getLangOpts(),
        false));

    if (SemiLoc.isInvalid()) {
      SemiLoc = ElseEndLocation;
    }

    ElseTokenRange =
        CharSourceRange::getTokenRange({ElseStartLocation, SemiLoc});
  } else {
    auto WithBrackets = !CompoundIfStmt;
    ElseTokenRange =
        WithBrackets
            ? getCompoundStmtRange(CompoundElseStmt, Context, Manager)
            : getCompoundStmtRange(CompoundElseStmt, Context, Manager, -1);
  }

  auto ElseText = Rewrite->getRewrittenText(ElseTokenRange);
  auto IfText = Rewrite->getRewrittenText(IfTokenRange);
  if (NeedShift) {
    moveBlock(CompoundElseStmt, IfText, Indent);
    if (CompoundIfStmt) {
      auto ElseLength = getTokenLenght(IfStmt->getElseLoc(), Context);
      clang::SourceRange Range(
          Manager->getExpansionLoc(
              CompoundIfStmt->getRBracLoc().getLocWithOffset(1)),
          IfStmt->getElseLoc().getLocWithOffset(ElseLength));

      Rewrite->RemoveText(Range);
      FixList.push_back(FixItHint::CreateRemoval(Range));
    } else {
      Rewrite->RemoveText(IfStmt->getElseLoc());
      FixList.push_back(FixItHint::CreateRemoval(IfStmt->getElseLoc()));
    }
  } else {
    Rewrite->ReplaceText(ElseTokenRange, IfText);
    FixList.push_back(FixItHint::CreateReplacement(ElseTokenRange, IfText));
  }
  Rewrite->ReplaceText(IfTokenRange, ElseText);
  FixList.push_back(FixItHint::CreateReplacement(IfTokenRange, ElseText));
}

static bool isInterruptStmt(const Stmt *Stmt) {
  return isa<ReturnStmt>(Stmt) || isa<ContinueStmt>(Stmt) ||
         isa<BreakStmt>(Stmt) || isa<CXXThrowExpr>(Stmt);
}

bool IfElseReturnChecker::addBlockToStmt(
    const Stmt *Stmt, const clang::CFG &Cfg,
    const clang::SourceManager *Manager,
    const clang::CFGStmtMap *StmtToBlockMap, const clang::ASTContext *Context) {
  if (const auto *LastStmt = getLastStmt(Stmt)) {
    if (isInterruptStmt(LastStmt)) {
      return true;
    }
  }

  if (const auto *Block = getInterruptBlockForStmt(Cfg, Manager, Stmt, Context,
                                                   StmtToBlockMap)) {
    auto *InterruptionBlockStmt = getInterruptStatement(Block);
    appendStmt(dyn_cast<CompoundStmt>(Stmt), InterruptionBlockStmt, Manager,
               Context);
    return true;
  }

  return false;
}
