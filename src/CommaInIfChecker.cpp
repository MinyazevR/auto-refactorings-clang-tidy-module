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
  auto ConditionExprBeginLocation = Manager->getExpansionLoc(
      ConditionExpr->getBeginLoc().getLocWithOffset(InitOffset));
  const auto ConditionExprEndLocation =
      Manager->getExpansionLoc(ConditionExpr->getEndLoc());

  auto *const AstContext = Result.Context;

  for (auto &&Parent : AstContext->getParents(*IfStmtNode)) {
    if (const auto *ParentStmt = Parent.get<Stmt>()) {
      if (const auto *ParentIfStmt = dyn_cast<IfStmt>(ParentStmt)) {
        if (ParentIfStmt->hasElseStorage() &&
            ParentIfStmt->getElse()->getID(*AstContext) ==
                IfStmtNode->getID(*AstContext)) {
          return;
        }
      }
    }
  }

  clang::SourceLocation CurrentExpressionBeginLocation =
      ConditionExprBeginLocation;
  clang::SourceLocation LastCommaLocation;

  std::list<std::string> Expressions;

  // The offset of the new assignment will be like ifStmt
  auto IfStmtIndent = clang::Lexer::getIndentationForLine(
      Manager->getExpansionLoc(IfStmtNode->getBeginLoc()), *Manager);

  int ParenCounter = 0;
  while (CurrentExpressionBeginLocation.isValid() &&
         CurrentExpressionBeginLocation < ConditionExprEndLocation) {

    auto NextTokenOptional = clang::Lexer::findNextToken(
        Manager->getExpansionLoc(CurrentExpressionBeginLocation), *Manager,
        AstContext->getLangOpts());

    if (NextTokenOptional.has_value()) {
      auto NextToken = NextTokenOptional.value();

      if (NextToken.is(clang::tok::l_paren)) {
        ParenCounter += 1;
      }
      if (NextToken.is(clang::tok::r_paren)) {
        ParenCounter -= 1;
      }

      CurrentExpressionBeginLocation =
          Manager->getExpansionLoc(NextToken.getLocation());
      if (NextToken.getKind() != clang::tok::comma || ParenCounter != 0) {
        continue;
      }
    }
    if (CurrentExpressionBeginLocation.isValid() &&
        CurrentExpressionBeginLocation < ConditionExprEndLocation) {
      auto CurrentSourceRange =
          clang::SourceRange({ConditionExprBeginLocation.getLocWithOffset(1),
                              CurrentExpressionBeginLocation});

      auto CurrentExprSourceCode = clang::Lexer::getSourceText(
          clang::CharSourceRange::getCharRange(CurrentSourceRange), *Manager,
          AstContext->getLangOpts());

      ConditionExprBeginLocation = CurrentExpressionBeginLocation;
      CurrentExprSourceCode = CurrentExprSourceCode.ltrim();
      Expressions.push_back(IfStmtIndent.str() + CurrentExprSourceCode.str() +
                            ";\n");
      ConditionExprBeginLocation = CurrentExpressionBeginLocation;
      LastCommaLocation = CurrentExpressionBeginLocation;
      continue;
    }
    break;
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
        IfStmtNode->getBeginLoc().getLocWithOffset(-IfStmtIndent.size()),
        Expression);
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
  Finder->addMatcher(parenExpr(has(binaryOperator(hasOperatorName(","))),
                               hasAncestor(ifStmt().bind("ifStmt")),
                               unless(hasAncestor(binaryOperator())),
                               unless(hasAncestor(callExpr())))
                         .bind("conditionExpr"),
                     this);

  Finder->addMatcher(binaryOperator(hasOperatorName(","),
                                    hasParent(ifStmt().bind("ifStmt")),
                                    unless(hasAncestor(binaryOperator())),
                                    unless(hasAncestor(parenExpr())))
                         .bind("binaryOperator"),
                     this);
}
