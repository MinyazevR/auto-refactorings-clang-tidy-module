#include "CommaInIfChecker.h"
#include "clang/Lex/Preprocessor.h"

using namespace clang::ast_matchers;
using namespace clang::tidy::autorefactorings;

CommaInIfChecker::CommaInIfChecker(StringRef Name, ClangTidyContext *Context)
    : ClangTidyCheck(Name, Context) {}

void CommaInIfChecker::check(const MatchFinder::MatchResult &Result) {
  const auto *ConditionExpr =
      Result.Nodes.getNodeAs<clang::Expr>("conditionExpr");
  const auto *IfStmtNode = Result.Nodes.getNodeAs<clang::IfStmt>("ifStmt");
  const auto *FirstBinaryOperator =
      Result.Nodes.getNodeAs<clang::BinaryOperator>("binaryOperator");
  auto *const Manager = Result.SourceManager;

  if (FirstBinaryOperator && !ConditionExpr) {
    ConditionExpr = IfStmtNode->getCond();
  }

  // Checking that ConditionExpr is in If Condition
  if (!Manager->getExpansionRange(IfStmtNode->getCond()->getSourceRange())
           .getAsRange()
           .fullyContains(ConditionExpr->getSourceRange())) {
    return;
  }

  auto InitOffset = FirstBinaryOperator ? -1 : 0;
  auto ConditionExprBeginLocation =
      ConditionExpr->getBeginLoc().getLocWithOffset(InitOffset);
  const auto ConditionExprEndLocation = ConditionExpr->getEndLoc();

  auto *const AstContext = Result.Context;
  clang::SourceLocation CurrentExpressionBeginLocation =
      ConditionExprBeginLocation;
  clang::SourceLocation LastCommaLocation;

  std::list<std::string> Expressions;

  // The offset of the new assignment will be like ifStmt
  auto IfStmtIndent = clang::Lexer::getIndentationForLine(
      Manager->getExpansionLoc(IfStmtNode->getBeginLoc()), *Manager);

  while (CurrentExpressionBeginLocation.isValid() &&
         CurrentExpressionBeginLocation < ConditionExprEndLocation) {

    auto NextTokenOptional = clang::Lexer::findNextToken(
        CurrentExpressionBeginLocation, *Manager, AstContext->getLangOpts());

    if (NextTokenOptional.has_value()) {
      auto NextToken = NextTokenOptional.value();
      CurrentExpressionBeginLocation = NextToken.getLocation();
      if (NextToken.getKind() != clang::tok::comma) {
        continue;
      }
    }
    if (CurrentExpressionBeginLocation.isValid() &&
        CurrentExpressionBeginLocation < ConditionExprEndLocation) {
      auto CurrentSourceRange = clang::SourceRange(
          {ConditionExprBeginLocation.getLocWithOffset(1),
           CurrentExpressionBeginLocation.getLocWithOffset(-1)});

      auto CurrentExprSourceCode = clang::Lexer::getSourceText(
          clang::CharSourceRange::getTokenRange(CurrentSourceRange), *Manager,
          AstContext->getLangOpts());

      ConditionExprBeginLocation = CurrentExpressionBeginLocation;
      CurrentExprSourceCode = CurrentExprSourceCode.ltrim();
      Expressions.push_back(IfStmtIndent.str() + CurrentExprSourceCode.str() +
                            ";\n");
      ConditionExprBeginLocation = CurrentExpressionBeginLocation;
      LastCommaLocation = CurrentExpressionBeginLocation;
    }
  }

  if (LastCommaLocation.isInvalid() ||
      LastCommaLocation >= ConditionExprEndLocation) {
    return;
  }

  int Offset = FirstBinaryOperator ? 0 : 1;
  auto LastSourceRange =
      clang::SourceRange({LastCommaLocation.getLocWithOffset(1),
                          ConditionExprEndLocation.getLocWithOffset(-Offset)});
  auto LastExpressionsSourceCode =
      clang::Lexer::getSourceText(
          clang::CharSourceRange::getTokenRange(LastSourceRange), *Manager,
          AstContext->getLangOpts())
          .ltrim()
          .str();

  auto Diag = diag(ConditionExpr->getBeginLoc(),
                   "It looks like you are using comma in the if condition.");

  for (auto &&Expression : Expressions) {
    Diag << FixItHint::CreateInsertion(
        IfStmtNode->getBeginLoc().getLocWithOffset(-1), Expression);
  }
  auto ConditionExprSourceRange = ConditionExpr->getSourceRange();
  auto ExprBeginLocation = ConditionExprSourceRange.getBegin();
  auto ExprEndLocation = ConditionExprSourceRange.getEnd();
  auto NewSourceRange =
      clang::SourceRange({ExprBeginLocation.getLocWithOffset(Offset),
                          ExprEndLocation.getLocWithOffset(-Offset)});
  Diag << FixItHint::CreateReplacement(NewSourceRange,
                                       LastExpressionsSourceCode);
}

void CommaInIfChecker::registerMatchers(MatchFinder *Finder) {
  Finder->addMatcher(
      parenExpr(hasDescendant(binaryOperator(hasOperatorName(","))),
                hasAncestor(ifStmt().bind("ifStmt")),
                unless(hasAncestor(callExpr())),
                unless(hasAncestor(parenExpr())))
          .bind("conditionExpr"),
      this);

  Finder->addMatcher(binaryOperator(hasOperatorName(","),
                                    hasParent(ifStmt().bind("ifStmt")),
                                    unless(hasAncestor(binaryOperator())),
                                    unless(hasAncestor(parenExpr())))
                         .bind("binaryOperator"),
                     this);
}
