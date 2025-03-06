#include "CallExprInIfChecker.h"
#include "clang/Lex/Preprocessor.h"
#include "llvm/Support/Regex.h"

using namespace clang::ast_matchers;
using namespace clang::tidy::autorefactorings;

CallExpInIfChecker::CallExpInIfChecker(StringRef Name,
                                       ClangTidyContext *Context)
    : ClangTidyCheck(Name, Context), UseAuto(Options.get("UseAuto", false)),
      UseDeclRefExpr(Options.get("UseDeclRefExprOptions", true)),
      UseAllCallExpr(Options.get("UseAllCallExpr", true)),
      VariablePrefix(Options.get("VariablePrefix", "conditionVariable")),
      Pattern(Options.get("Filter", ".*")),
      ReturnTypePattern(Options.get("CallReturnTypeFilter", ".*")) {}

void CallExpInIfChecker::storeOptions(ClangTidyOptions::OptionMap &Opts) {
  Options.store(Opts, "Filter", Pattern);
  Options.store(Opts, "CallReturnTypeFilter", ReturnTypePattern);
  Options.store(Opts, "VariablePrefix", VariablePrefix);
  Options.store(Opts, "UseAuto", UseAuto);
  Options.store(Opts, "UseDeclRefExpr", UseDeclRefExpr);
  Options.store(Opts, "UseAllCallExpr", UseAllCallExpr);
}

void CallExpInIfChecker::check(const MatchFinder::MatchResult &Result) {
  const auto *IfStmtNode = Result.Nodes.getNodeAs<clang::IfStmt>("ifStmt");
  const auto *CallExpr = Result.Nodes.getNodeAs<clang::CallExpr>("callExpr");
  const auto *AssigmentExpression = Result.Nodes.getNodeAs<clang::Expr>("Expr");
  const auto *IfStmtDeclRefExpr =
      Result.Nodes.getNodeAs<clang::DeclRefExpr>("declRefExpr");

  auto *AstContext = Result.Context;
  auto *Manager = Result.SourceManager;
  auto ReturnType = CallExpr->getCallReturnType(*AstContext);

  // Checking that CallExpr is in If Condition
  if (!Manager->getExpansionRange(IfStmtNode->getCond()->getSourceRange())
           .getAsRange()
           .fullyContains(CallExpr->getSourceRange())) {
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
  if (const auto *Function = CallExpr->getCalleeDecl()->getAsFunction()) {
    auto CalleeStringName = Function->getNameAsString();
    static llvm::Regex CallExprRegex(Pattern);
    if (!CallExprRegex.match(CalleeStringName)) {
      return;
    }
  }

  std::string ReturnTypeString;

  if (UseAuto) {
    ReturnTypeString = "auto";
  } else {
    auto *Type = ReturnType.getTypePtrOrNull();
    if (!Type || ReturnType.isNull()) {
      return;
    }

    ReturnTypeString = ReturnType.getAsString();

    // Checking type of the returned value
    static llvm::Regex ReturnTypeRegex(ReturnTypePattern);
    if (!ReturnTypeRegex.match(ReturnTypeString)) {
      return;
    }
  }

  // The offset of the new assignment will be like ifStmt
  auto IfStmtIndent = clang::Lexer::getIndentationForLine(
      Manager->getExpansionLoc(IfStmtNode->getBeginLoc()), *Manager);

  if (AssigmentExpression && UseDeclRefExpr) {
    auto AssigmentExpressionSourceCode =
        clang::Lexer::getSourceText(clang::CharSourceRange::getTokenRange(
                                        AssigmentExpression->getSourceRange()),
                                    *Manager, AstContext->getLangOpts());

    if (AssigmentExpressionSourceCode.starts_with('(') &&
        AssigmentExpressionSourceCode.ends_with(')')) {
      AssigmentExpressionSourceCode =
          AssigmentExpressionSourceCode.drop_front(1).drop_back(1);
    }

    auto Str = IfStmtIndent.str() + AssigmentExpressionSourceCode.str() + ";\n";

    if (!IfStmtDeclRefExpr) {
      return;
    }

    std::string VariableName;
    if (const auto *IfStmtDeclRefExprDecl = IfStmtDeclRefExpr->getDecl()) {
      VariableName = IfStmtDeclRefExprDecl->getNameAsString();
    }
    if (VariableName.empty()) {
      return;
    }

    auto Diag =
        diag(AssigmentExpression->getBeginLoc(),
             "It looks like you are using function call in the if condition.");
    Diag << FixItHint::CreateReplacement(AssigmentExpression->getSourceRange(),
                                         VariableName);
    Diag << FixItHint::CreateInsertion(
        IfStmtNode->getBeginLoc().getLocWithOffset(-IfStmtIndent.size()), Str);
    return;
  }
  if (!UseAllCallExpr) {
    return;
  }
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
  Diag << FixItHint::CreateReplacement(CallExpr->getSourceRange(),
                                       VariableName);
  Diag << FixItHint::CreateInsertion(
      IfStmtNode->getBeginLoc().getLocWithOffset(-IfStmtIndent.size()), Str);
}

void CallExpInIfChecker::registerMatchers(MatchFinder *Finder) {
  auto IfStmtCallExprMatcher =
      callExpr(hasAncestor(ifStmt().bind("ifStmt"))).bind("callExpr");
  auto IfStmtCallExprDeclRefMatcher =
      expr(has(binaryOperator(isAssignmentOperator(),
                              hasRHS(IfStmtCallExprMatcher),
                              hasLHS(declRefExpr().bind("declRefExpr")))
                   .bind("binaryOperator")))
          .bind("Expr");

  Finder->addMatcher(IfStmtCallExprDeclRefMatcher, this);
  Finder->addMatcher(IfStmtCallExprMatcher, this);
}