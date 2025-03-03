#include "CommaInIfChecker.h"

using namespace clang::ast_matchers;
using namespace clang::tidy::autorefactorings;

CommaInIfChecker::CommaInIfChecker(
        StringRef Name, ClangTidyContext *Context)
        : ClangTidyCheck(Name, Context)
         {}

void CommaInIfChecker::check(const MatchFinder::MatchResult &Result) {
     auto conditionExpr = Result.Nodes.getNodeAs<clang::Expr>("conditionExpr");
     diag(conditionExpr->getBeginLoc(), "It looks like you are using comma in the if condition.");
}

void CommaInIfChecker::registerMatchers(MatchFinder *Finder) {
     Finder->addMatcher(
          ifStmt(hasCondition(expr(hasDescendant(binaryOperator(hasOperatorName(","),
                               unless(hasAncestor(callExpr()))).bind("commaOperator"))).bind("conditionExpr")))
                              .bind("ifStmt"), this);
}