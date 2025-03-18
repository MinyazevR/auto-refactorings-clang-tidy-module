#include "RenameGhidraMemberVariable.h"
#include "clang/Lex/Preprocessor.h"
#include "llvm/Support/Regex.h"
#include "AutoRefactoringModuleUtils.h"
#include "clang/AST/RecordLayout.h"

using namespace clang::ast_matchers;
using namespace clang::tidy::autorefactorings;

RenameGhidraMemberVariable::RenameGhidraMemberVariable(
        StringRef Name, ClangTidyContext *Context)
        : ClangTidyCheck(Name, Context)
         {}

void RenameGhidraMemberVariable::check(const MatchFinder::MatchResult &Result) {
    const auto* CurrentDeclRefExpr = Result.Nodes.getNodeAs<clang::DeclRefExpr>("declRefExpr");

    if (!CurrentDeclRefExpr) {
        return;
    }

    auto *AstContext = Result.Context;
    auto *Manager = Result.SourceManager;
    
    for (auto &&Parent: AstContext->getParents(*CurrentDeclRefExpr)) {
        if (const auto *ParentStmt = Parent.get<Stmt>()) {
            if (auto *ParentIfStmt = dyn_cast<clang::RecoveryExpr>(ParentStmt)) {
                auto SubExpressions = ParentIfStmt->subExpressions();

                if (SubExpressions.size() != 1 
                ||  (*SubExpressions.begin())->getID(*AstContext) 
                        != CurrentDeclRefExpr->getID(*AstContext)) {
                    return;
                }

                auto DeclRefEndLocation = CurrentDeclRefExpr->getEndLoc();
                auto Type = CurrentDeclRefExpr->getType();
                clang::tok::TokenKind TokenKind;
                if (const PointerType *ptrType = Type->getAs<PointerType>()) {
                    Type = ptrType->getPointeeType();
                    TokenKind = tok::arrow;
                } else {
                    TokenKind = tok::period;
                }
                
                auto StructDecl = Type->getAsRecordDecl();
                if (!StructDecl) {
                    return;
                }

                auto static MemberExprNamePattern = "_[0-9]+_";
                static llvm::Regex Regex(MemberExprNamePattern);

                SourceLocation ExpansionEndLoc = clang::Lexer::findLocationAfterToken(
                    Manager->getExpansionLoc(DeclRefEndLocation),
                    TokenKind, *Manager, AstContext->getLangOpts(), false);

                if (ExpansionEndLoc.isInvalid()) {
                    return;
                }
                auto ExpansionTokenRange = clang::SourceRange(DeclRefEndLocation, ExpansionEndLoc);

                auto AssigmentExpressionSourceCode = 
                        Utils::getSourceText({DeclRefEndLocation, ExpansionEndLoc},
                         Manager, AstContext);

                SmallVector<StringRef> matches;
                if (!Regex.match(AssigmentExpressionSourceCode, &matches)) {
                    return;
                }
                auto Match = matches[0].drop_back(1).drop_front(1);
                auto Align = std::stoul(Match.str());
                auto DeclRefBeginLocation = CurrentDeclRefExpr->getBeginLoc();

                auto DeclRefExpressionSourceCode = 
                        Utils::getSourceText({DeclRefBeginLocation, DeclRefEndLocation},
                         Manager, AstContext);

                std::string StringForReplace;
                const clang::ASTRecordLayout &TypeLayout(AstContext->getASTRecordLayout(StructDecl));
                for (auto &&field: StructDecl->fields()) {
                    // FIXME;
                    auto IntFieldAlign = TypeLayout.getFieldOffset(field->getFieldIndex())
                                                                 / AstContext->getCharWidth();

                    if (IntFieldAlign == Align) {
                        StringForReplace = 
                            DeclRefExpressionSourceCode.str() + field->getNameAsString();
                            auto Diag = diag(DeclRefBeginLocation, "It looks like accessing an Enum/Structure field.");
                            Diag << FixItHint::CreateReplacement(
                                    clang::CharSourceRange::getTokenRange(ExpansionTokenRange)
                                            , StringForReplace);
                    }
                    if (IntFieldAlign >= Align) {
                        return;
                    }
                }
            }
        }
    }
}

void RenameGhidraMemberVariable::registerMatchers(MatchFinder *Finder) {
    Finder->addMatcher(declRefExpr().bind("declRefExpr"), this);
}
