#include "CallExprInIfChecker.h"
#include "clang/Lex/Preprocessor.h"
#include "llvm/Support/Regex.h"

using namespace clang::ast_matchers;
using namespace clang::tidy::autorefactorings;

CallExpInIfChecker::CallExpInIfChecker(StringRef Name,
                                       ClangTidyContext *Context)
    : ClangTidyCheck(Name, Context),
      VariablePrefix(Options.get("VariablePrefix", "conditionVariable")),
      Pattern(Options.get("Filter", ".*")),
      ReturnTypePattern(Options.get("CallReturnTypeFilter", ".*")) {}

void CallExpInIfChecker::storeOptions(ClangTidyOptions::OptionMap &Opts) {
  Options.store(Opts, "Filter", Pattern);
  Options.store(Opts, "CallReturnTypeFilter", ReturnTypePattern);
  Options.store(Opts, "VariablePrefix", VariablePrefix);
}

void CallExpInIfChecker::check(const MatchFinder::MatchResult &Result) {
  const auto *IfStmtNode = Result.Nodes.getNodeAs<clang::IfStmt>("ifStmt");
  const auto *CallExpr = Result.Nodes.getNodeAs<clang::CallExpr>("callExpr");

  auto *AstContext = Result.Context;
  auto *Manager = Result.SourceManager;
  auto ReturnType = CallExpr->getCallReturnType(*AstContext);

  // Checking that CallExpr is in If Condition
  if (!IfStmtNode->getCond()->getSourceRange().fullyContains(
          CallExpr->getSourceRange())) {
    return;
  }

  // We don't want to check if with init storage
  if (IfStmtNode->hasInitStorage()) {
    return;
  }

  // We don't check "else if" condition
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

  // Checking based on the function name
  static llvm::Regex CallExprRegex(Pattern);
  auto CalleeStringName =
      CallExpr->getCalleeDecl()->getAsFunction()->getNameAsString();
  if (!CallExprRegex.match(CalleeStringName)) {
    return;
  }

  auto ReturnTypeString = ReturnType.getAsString();
  // Checking type of the returned value
  static llvm::Regex ReturnTypeRegex(ReturnTypePattern);
  if (!ReturnTypeRegex.match(ReturnTypeString)) {
    return;
  }

  // The offset of the new assignment will be like ifStmt
  auto IfStmtIndent = clang::Lexer::getIndentationForLine(
      Manager->getExpansionLoc(IfStmtNode->getBeginLoc()), *Manager);

  auto CallExprSourceCode = clang::Lexer::getSourceText(
      clang::CharSourceRange::getTokenRange(CallExpr->getSourceRange()),
      *Manager, AstContext->getLangOpts());

  auto VariableName = VariablePrefix + std::to_string(VariableCounter);
  VariableCounter += 1;

  auto Str = IfStmtIndent.str() + ReturnTypeString + " " + VariableName +
             " = " + CallExprSourceCode.str() + ";\n";

  auto Diag =
      diag(CallExpr->getBeginLoc(),
           "It looks like you are using function call in the if condition.");
  Diag << FixItHint::CreateInsertion(
      IfStmtNode->getBeginLoc().getLocWithOffset(-1), Str);
  Diag << FixItHint::CreateReplacement(CallExpr->getSourceRange(),
                                       VariableName);
}

void CallExpInIfChecker::registerMatchers(MatchFinder *Finder) {
  Finder->addMatcher(
      callExpr(hasAncestor(ifStmt().bind("ifStmt"))).bind("callExpr"), this);
}