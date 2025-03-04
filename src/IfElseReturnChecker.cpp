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

  IfElseReturnChecker *mChecker;
  const MatchFinder::MatchResult &mResult;
  clang::CFG &mCfg;

public:
  IfStmtVisitor(IfElseReturnChecker *checker,
                const MatchFinder::MatchResult &Result, clang::CFG &cfg)
      : mChecker(checker), mResult(Result), mCfg(cfg) {}

  bool VisitIfStmt(clang::IfStmt *ifStmt) {

    if (ifStmt->hasElseStorage()) {
      if (isa<clang::IfStmt>(ifStmt->getElse())) {
        return true;
      }

      mChecker->runInternal(ifStmt, mResult, mCfg);
      return true;
    }
    return RecursiveASTVisitor::VisitIfStmt(ifStmt);
  }

  bool TraverseStmt(Stmt *stmt) {
    if (!stmt)
      return true;

    if (auto *ifStmt = dyn_cast<clang::IfStmt>(stmt)) {
      ifStmt->dumpPretty(*mResult.Context);

      if (auto *thenStmt = ifStmt->getThen()) {
        TraverseStmt(thenStmt);
      }
      if (auto *elseStmt = ifStmt->getElse()) {
        TraverseStmt(elseStmt);
      }
      return VisitIfStmt(ifStmt);
    }

    return RecursiveASTVisitor::TraverseStmt(stmt);
  }
};
} // namespace

/// This function is designed to get the length of the token to handle offsets
/// correctly.
static int getTokenLenght(SourceLocation loc,
                          const clang::ASTContext &context) {
  return clang::Lexer::MeasureTokenLength(loc, context.getSourceManager(),
                                          context.getLangOpts());
};

/// Since a block can potentially contain several Stmts, it is checked
/// that all of them are included in the comprehensive (in our case, in IfStmt)
static bool isBlockInCurrentIfStmt(const clang::CFGBlock *block,
                                   const Stmt *stmt,
                                   const SourceManager *manager) {

  for (auto &&blockElement : block->Elements) {
    auto cfgStmtElement = blockElement.getAs<CFGStmt>();
    if (!cfgStmtElement) {
      return false;
    }

    auto *cfgStmtElementStmt = cfgStmtElement->getStmt();
    if (stmt->getBeginLoc() >
            manager->getExpansionLoc(cfgStmtElementStmt->getBeginLoc()) ||
        stmt->getEndLoc() <
            manager->getExpansionLoc(cfgStmtElementStmt->getEndLoc())) {
      return false;
    }
  }

  return true;
}

/// Checking that the block does not include any meaningful logic other than
/// ReturnStmt
static bool isOnlyReturnBlock(const clang::CFGBlock *block,
                              const clang::SourceManager *manager) {
  auto returnBlock = block->back();

  if (auto returnBlockCFGStmt = returnBlock.getAs<CFGStmt>()) {
    if (auto returnBlockStmt =
            dyn_cast<ReturnStmt>(returnBlockCFGStmt->getStmt())) {
      return isBlockInCurrentIfStmt(block, returnBlockStmt, manager);
    }
    return false;
  }
  return false;
}

/// Get the source text using CharRange and SourceManager
static llvm::StringRef get_source_text(const clang::CharSourceRange &range,
                                       const clang::SourceManager *manager,
                                       const clang::ASTContext *context) {

  auto langOptions = context->getLangOpts();
  auto start = manager->getSpellingLoc(range.getBegin());
  auto end = clang::Lexer::getLocForEndOfToken(
      manager->getSpellingLoc(range.getEnd()), 0, *manager, langOptions);
  return clang::Lexer::getSourceText(
      clang::CharSourceRange::getTokenRange(clang::SourceRange{start, end}),
      *manager, langOptions);
}

/// Get Interrupt Stmt of block
static const Stmt *getIterruptStatement(const clang::CFGBlock *block) {
  auto iterruptInstruction = block->back();
  if (auto returnBlockCFGStmt = iterruptInstruction.getAs<CFGStmt>()) {
    auto *answer = dyn_cast<ReturnStmt>(returnBlockCFGStmt->getStmt());
    return answer;
  }
  return nullptr;
}

/// Get the latest Stmt in scope if it is Compound Stmt
static const Stmt *getLastStmt(const Stmt *stmt) {
  if (auto *cmdCompound = dyn_cast<CompoundStmt>(stmt)) {
    return cmdCompound->body_back();
  }
  return nullptr;
}

/// Returns true if it is an exit block for the entire CFG,
/// a ReturnBlock, or a Block that does not return control.
static bool isOnlyIterruptionBlock(const clang::CFGBlock *block,
                                   const clang::CFG &cfg,
                                   const clang::SourceManager *manager) {
  return block->getBlockID() == cfg.getExit().getBlockID() ||
         isOnlyReturnBlock(block, manager) || block->hasNoReturnElement();
}

/// It is used to calculate the weight of the Then or Else Compound Stmt of the
/// main IfStmt,
// on the basis of which a decision will be made about flipping the if and else
// bodies
static int getNodeWeight(const Stmt *stmt, clang::SourceManager *manager) {
  auto start = manager->getExpansionRange(stmt->getSourceRange()).getBegin();
  auto end = manager->getExpansionRange(stmt->getSourceRange()).getEnd();
  return manager->getExpansionLineNumber(end) -
         manager->getSpellingLineNumber(start);
}

/// When using spaces, it is difficult to understand what Indent is, so the user
/// is prompted
// to use the Indent parameter in .clang-tidy.
// CheckOptions:
// - key:             if-else-refactor.Indent
//   value:           1
IfElseReturnChecker::IfElseReturnChecker(StringRef Name,
                                         ClangTidyContext *Context)
    : ClangTidyCheck(Name, Context), mIndent(Options.get("Indent", 4)),
      mNeedShift(Options.get("NeedShift", false)),
      mRewrite(std::make_unique<Rewriter>()) {}

void IfElseReturnChecker::registerMatchers(MatchFinder *Finder) {
  Finder->addMatcher(
      functionDecl(isDefinition(),
                   unless(anyOf(isDefaulted(), isDeleted(), isWeak())))
          .bind("functionDecl"),
      this);
}

void IfElseReturnChecker::runInternal(
    IfStmt *ifStmt, const ast_matchers::MatchFinder::MatchResult &Result,
    clang::CFG &cfg) {

  auto ifLocation = ifStmt->getBeginLoc();
  auto manager = Result.SourceManager;

  if (manager->isMacroArgExpansion(ifLocation) ||
      manager->isMacroBodyExpansion(ifLocation) ||
      manager->isInExternCSystemHeader(ifLocation) ||
      manager->isInSystemHeader(ifLocation) ||
      manager->isInSystemMacro(ifLocation)) {
    return;
  }

  auto *thenStmt = ifStmt->getThen();
  auto *elseStmt = ifStmt->getElse();

  bool isThenFirst =
      getNodeWeight(thenStmt, manager) <= getNodeWeight(elseStmt, manager);
  auto *targetStmt = isThenFirst ? thenStmt : elseStmt;

  auto functionDecl =
      Result.Nodes.getNodeAs<clang::FunctionDecl>("functionDecl");

  std::unique_ptr<ParentMap> parentMap(new ParentMap(functionDecl->getBody()));
  std::unique_ptr<clang::CFGStmtMap> stmtToBlockMap(
      clang::CFGStmtMap::Build(&cfg, parentMap.get()));

  auto context = Result.Context;

  if (mNeedShift) {
    if (!addIterruptionBlockToStmts(targetStmt, cfg, manager,
                                    stmtToBlockMap.get(), context)) {
      mNeedShift = false;
    }
  }

  if (!context || !isThenFirst) {
    if (!reversCondition(ifStmt, manager)) {
      return;
    }
    reverseStmt(ifStmt, *context, manager, mNeedShift);
  } else {
    if (!mNeedShift) {
      return;
    }
    if (auto *compoundElseStmt = dyn_cast<CompoundStmt>(elseStmt)) {

      auto elseLBracLoc = compoundElseStmt->getLBracLoc();
      auto elseRBracLoc = compoundElseStmt->getRBracLoc();

      auto left = manager->getExpansionLoc(elseLBracLoc)
                      .getLocWithOffset(getTokenLenght(elseLBracLoc, *context));
      auto right = manager->getExpansionLoc(elseRBracLoc).getLocWithOffset(-1);

      auto elseText = mRewrite->getRewrittenText(
          CharSourceRange::getTokenRange(left, right));
      // : get_source_text(CharSourceRange::getTokenRange(left, right), manager,
      // context).str();

      // auto mainIndent =
      // clang::Lexer::getIndentationForLine(ifStmt->getBeginLoc(), *manager);
      indentBlock(compoundElseStmt, elseText, mIndent);
    }

    if (auto *compoundThenStmt = dyn_cast<CompoundStmt>(thenStmt)) {
      auto elseLength = getTokenLenght(ifStmt->getElseLoc(), *context);
      clang::SourceRange range(
          manager->getExpansionLoc(
              compoundThenStmt->getRBracLoc().getLocWithOffset(1)),
          ifStmt->getElseLoc().getLocWithOffset(elseLength));
      mFixList.push_back(FixItHint::CreateRemoval(range));
      mRewrite->RemoveText(range);
    } else {
      mFixList.push_back(FixItHint::CreateRemoval(ifStmt->getElseLoc()));
      mRewrite->RemoveText(ifStmt->getElseLoc());
    }
  }

  DiagnosticBuilder Diag = diag(ifStmt->getBeginLoc(), "azaazazaza");

  for (auto &&fix : mFixList) {

    Diag << fix;
  }

  mFixList.clear();
  return;
}

void IfElseReturnChecker::storeOptions(ClangTidyOptions::OptionMap &Opts) {
  Options.store(Opts, "Indent", mIndent);
  Options.store(Opts, "NeedShift", mNeedShift);
}

void IfElseReturnChecker::check(const MatchFinder::MatchResult &Result) {
  auto langOptions = Result.Context->getLangOpts();
  auto manager = Result.SourceManager;

  mRewrite->setSourceMgr(*manager, langOptions);

  auto functionDecl =
      Result.Nodes.getNodeAs<clang::FunctionDecl>("functionDecl");

  auto cfg = CFG::buildCFG(functionDecl, functionDecl->getBody(),
                           Result.Context, CFG::BuildOptions());

  IfStmtVisitor Visitor(this, Result, *cfg.get());
  if (Visitor.TraverseDecl(const_cast<clang::FunctionDecl *>(functionDecl))) {
    // Rewrite.overwriteChangedFiles();
  }
}

/// if (cond) ----> if (!(cond))
/// if (x > y) ----> if (x <= y)
bool IfElseReturnChecker::reversCondition(const IfStmt *ifStmt,
                                          const clang::SourceManager *manager) {

  // if ifStmt is init as if (auto x = ....)
  if (ifStmt->hasInitStorage()) {
    return false;
  }

  // A beautiful negation that is not worth using for C++ when it is
  // possible to analyze for overloaded operators.

  // if (auto booleanCondition = dyn_cast<BinaryOperator>(condition)) {
  //     auto opcode = booleanCondition->getOpcode();

  //     if (booleanCondition->isComparisonOp(opcode)) {
  //         if (auto negateOpCode =
  //         booleanCondition->negateComparisonOp(opcode)) {
  //             booleanCondition->getBeginLoc();
  //             booleanCondition->getExprLoc();
  //             booleanCondition->getEndLoc();
  //             auto opcodeStr = BinaryOperator::getOpcodeStr(negateOpCode);
  //             llvm::errs() << "opCodeStr\n";
  //             llvm::errs() << opcodeStr;
  //             Rewrite.ReplaceText(booleanCondition->getOperatorLoc(),
  //             opcodeStr); return true;
  //         }
  //     }
  // }

  // Negating the condition
  auto *condition = ifStmt->getCond();
  if (auto then = dyn_cast<CompoundStmt>(ifStmt->getThen())) {
    auto expansionBeginLoc = manager->getExpansionLoc(condition->getBeginLoc());
    auto expansionEndLoc = manager->getExpansionLoc(then->getLBracLoc());

    mRewrite->InsertText(expansionBeginLoc, "!(");
    mRewrite->InsertText(expansionEndLoc.getLocWithOffset(-1), ")");
    mFixList.push_back(FixItHint::CreateInsertion(expansionBeginLoc, "!("));
    mFixList.push_back(
        FixItHint::CreateInsertion(expansionEndLoc.getLocWithOffset(-1), ")"));
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
static const clang::CFGBlock *
getItteruptBlockForStmt(const clang::CFG &cfg,
                        const clang::SourceManager *manager, const Stmt *stmt,
                        const clang::ASTContext *context,
                        const clang::CFGStmtMap *stmtToBlockMap) {
  auto exitBlock = cfg.getExit();
  std::set<const clang::CFGBlock *> frontier;
  frontier.insert(&exitBlock);
  for (auto &&block : exitBlock.preds()) {
    if (!block) {
      return nullptr;
    }
    if (isOnlyIterruptionBlock(block, cfg, manager)) {
      frontier.insert(block);
    }
  }

  CFGReverseBlockReachabilityAnalysis analysis(cfg);
  const clang::CFGBlock *mBlock = nullptr;

  auto *cmpdStmt = dyn_cast<CompoundStmt>(stmt);
  if (!cmpdStmt) {
    return nullptr;
  }
  auto fstStmt = *cmpdStmt->body_begin();
  if (!fstStmt) {
    return nullptr;
  }

  auto fstBlock = stmtToBlockMap->getBlock(fstStmt);

  if (!fstBlock) {
    return nullptr;
  }

  for (auto &&block : frontier) {
    if (!block) {
      return nullptr;
    }
    for (auto blockPred : block->preds()) {
      auto isBlockInCurrentStmt =
          isBlockInCurrentIfStmt(blockPred, stmt, manager);
      auto currentBlockIsReachable = analysis.isReachable(fstBlock, blockPred);
      if (!isOnlyIterruptionBlock(blockPred, cfg, manager)) {
        if (!isBlockInCurrentStmt && currentBlockIsReachable) {
          return nullptr;
        }
        continue;
      }
      if (!isBlockInCurrentStmt && currentBlockIsReachable) {
        if (mBlock) {
          return nullptr;
        }
        mBlock = blockPred;
      }
    }
  }

  return mBlock;
}

/// Adds to the source text the text of the block that needs to be tightened to
/// interrupt the branch.
void IfElseReturnChecker::appendStmt(const CompoundStmt *stmt,
                                     const Stmt *stmtToAdd,
                                     const clang::SourceManager *manager,
                                     const clang::ASTContext *context) {

  auto *lastStmt = stmt->body_back();

  // Get source text for new stmt
  auto ref =
      get_source_text(manager->getExpansionRange(stmtToAdd->getSourceRange()),
                      manager, context);

  // Get range and location for lastStmt in CompoundStmt
  auto lastStmtRange = manager->getExpansionRange(lastStmt->getSourceRange());
  auto last_token_loc = manager->getSpellingLoc(lastStmtRange.getEnd());

  // Take a transfer for the last stmt to use it to insert a new one
  auto indent = clang::Lexer::getIndentationForLine(
      manager->getExpansionLoc(lastStmt->getBeginLoc()), *manager);

  // You need to insert a new instruction immediately after the last one,
  // so you need to find the place to use ";"
  SourceLocation semi_loc(clang::Lexer::findLocationAfterToken(
      last_token_loc, clang::tok::semi, *manager, context->getLangOpts(),
      false));

  if (semi_loc.isInvalid()) {
    semi_loc = clang::Lexer::getLocForEndOfToken(last_token_loc, 0, *manager,
                                                 context->getLangOpts());
  }

  std::string t = "\n" + indent.str() + ref.str();

  mRewrite->InsertTextAfterToken(semi_loc, t);
  mFixList.push_back(FixItHint::CreateInsertion(semi_loc, t));
}

/// It's not the most efficient way to move a block to the left by the specified
/// number of characters.
void IfElseReturnChecker::indentBlock(const CompoundStmt *stmt,
                                      const std::string &string,
                                      unsigned long mainIndent) {
  std::vector<std::string> vector{"\n"};
  std::stringstream stream(string);
  std::string token;
  bool first = true;
  while (std::getline(stream, token, '\n')) {
    if (first) {
      if (token.empty() || token.find_first_not_of(" \t") == std::string::npos)
        continue;
    }
    first = false;
    auto stringStart = token.find_first_not_of(" \t");

    if (stringStart == std::string::npos || stringStart < mainIndent) {
      vector.push_back(token);
      continue;
    }

    auto trimToken = token.erase(0, mainIndent);
    vector.push_back(trimToken);
  }

  auto lastString = vector.back();
  if (lastString.find_first_not_of(" \t") == std::string::npos) {
    vector.pop_back();
  }

  std::stringstream inputStream;
  copy(vector.begin(), vector.end(),
       std::ostream_iterator<std::string>(inputStream, "\n"));
  // Rewrite.ReplaceText(stmt->getSourceRange(), inputStream.str());

  auto text = inputStream.str();
  text.pop_back();

  mRewrite->RemoveText(stmt->getSourceRange());
  mRewrite->InsertText(stmt->getBeginLoc(), text);

  mFixList.push_back(FixItHint::CreateRemoval(stmt->getSourceRange()));
  mFixList.push_back(FixItHint::CreateInsertion(stmt->getBeginLoc(), text));
}

static clang::CharSourceRange
getCompoundStmtRange(const CompoundStmt *stmt, const clang::ASTContext &context,
                     const SourceManager *manager, unsigned long indent = 1) {
  auto LBracLoc = stmt->getLBracLoc();
  auto RBracLoc = stmt->getRBracLoc();

  auto leftBorder = manager->getExpansionLoc(LBracLoc).getLocWithOffset(
      getTokenLenght(LBracLoc, context));
  auto rightBorder = manager->getExpansionLoc(RBracLoc);

  return CharSourceRange::getTokenRange(leftBorder.getLocWithOffset(-indent),
                                        rightBorder.getLocWithOffset(indent));
}

/// Swap the then and else branches
void IfElseReturnChecker::reverseStmt(const IfStmt *ifStmt,
                                      const clang::ASTContext &context,
                                      const clang::SourceManager *manager,
                                      bool needShift) {

  auto *elseStmt = ifStmt->getElse();
  auto *thenStmt = ifStmt->getThen();

  // auto thenRange = thenStmt->getSourceRange();
  // auto elseRange = elseStmt->getSourceRange();

  clang::CharSourceRange ifTokenRange;
  clang::CharSourceRange elseTokenRange;

  auto *compoundIfStmt = dyn_cast<CompoundStmt>(thenStmt);
  auto *compoundElseStmt = dyn_cast<CompoundStmt>(elseStmt);

  if (!compoundIfStmt) {
    ifTokenRange = CharSourceRange::getTokenRange(thenStmt->getSourceRange());
  } else {
    auto withBrackets = !compoundElseStmt;
    ifTokenRange =
        withBrackets
            ? getCompoundStmtRange(compoundIfStmt, context, manager)
            : getCompoundStmtRange(compoundIfStmt, context, manager, -1);
  }

  if (!compoundElseStmt) {
    auto elseLenght = getTokenLenght(ifStmt->getElseLoc(), context);
    auto elseStartLocation = ifStmt->getElseLoc().getLocWithOffset(elseLenght);
    auto elseEndLocation = manager->getExpansionLoc(ifStmt->getEndLoc());

    SourceLocation semi_loc(clang::Lexer::findLocationAfterToken(
        elseEndLocation, clang::tok::semi, *manager, context.getLangOpts(),
        false));

    if (semi_loc.isInvalid()) {
      semi_loc = elseEndLocation;
    }

    elseTokenRange =
        CharSourceRange::getTokenRange({elseStartLocation, semi_loc});
  } else {
    auto withBrackets = !compoundIfStmt;
    elseTokenRange =
        withBrackets
            ? getCompoundStmtRange(compoundElseStmt, context, manager)
            : getCompoundStmtRange(compoundElseStmt, context, manager, -1);
  }

  // auto ifLBracLoc = compoundIfStmt->getLBracLoc();
  // auto ifRBracLoc = compoundIfStmt->getRBracLoc();

  // auto leftBorderOfThen = manager->getExpansionLoc(ifLBracLoc)
  //                             .getLocWithOffset(getTokenLenght(ifLBracLoc,
  //                             context));
  // auto rightBorderOfThen = manager->getExpansionLoc(ifRBracLoc);

  // auto indent = clang::Lexer::getIndentationForLine(rightBorderOfThen,
  // *manager); auto offset = indent.size() + 1; auto startOfRBracStringLoc =
  // rightBorderOfThen.getLocWithOffset(-offset); auto str =
  // Rewrite.getRewrittenText({startOfRBracStringLoc,
  //                             rightBorderOfThen.getLocWithOffset(-1)});

  // auto ifTokenRange =
  // CharSourceRange::getTokenRange(leftBorderOfThen.getLocWithOffset(1),
  //                                      rightBorderOfThen.getLocWithOffset(-1));
  auto elseText = mRewrite->getRewrittenText(elseTokenRange);
  //  : get_source_text(elseTokenRange, manager, &context).str();

  auto ifText = mRewrite->getRewrittenText(ifTokenRange);
  // auto mainIndent =
  // clang::Lexer::getIndentationForLine(ifStmt->getBeginLoc(), *manager);

  if (needShift) {
    indentBlock(compoundElseStmt, ifText, mIndent);
    if (compoundIfStmt) {
      auto elseLength = getTokenLenght(ifStmt->getElseLoc(), context);
      clang::SourceRange range(
          manager->getExpansionLoc(
              compoundIfStmt->getRBracLoc().getLocWithOffset(1)),
          ifStmt->getElseLoc().getLocWithOffset(elseLength));

      mRewrite->RemoveText(range);
      mFixList.push_back(FixItHint::CreateRemoval(range));
    } else {
      mRewrite->RemoveText(ifStmt->getElseLoc());
      mFixList.push_back(FixItHint::CreateRemoval(ifStmt->getElseLoc()));
    }
  } else {
    mRewrite->ReplaceText(elseTokenRange, ifText);
    mFixList.push_back(FixItHint::CreateReplacement(elseTokenRange, ifText));
  }
  mRewrite->ReplaceText(ifTokenRange, elseText);
  mFixList.push_back(FixItHint::CreateReplacement(ifTokenRange, elseText));
}

bool IfElseReturnChecker::addIterruptionBlockToStmts(
    const Stmt *stmt, const clang::CFG &cfg,
    const clang::SourceManager *manager,
    const clang::CFGStmtMap *stmtToBlockMap, const clang::ASTContext *context) {
  if (auto lastStmt = getLastStmt(stmt)) {
    if (isa<ReturnStmt>(lastStmt)) {
      return true;
    }
  }

  if (auto block = getItteruptBlockForStmt(cfg, manager, stmt, context,
                                           stmtToBlockMap)) {
    auto *iterruptionBlockStmt = getIterruptStatement(block);
    appendStmt(dyn_cast<CompoundStmt>(stmt), iterruptionBlockStmt, manager,
               context);
    return true;
  }

  return false;
}
