#include "../ClangTidy.h"
#include "../ClangTidyModule.h"
#include "../ClangTidyModuleRegistry.h"
#include "CallExprInIfChecker.h"
#include "CommaInIfChecker.h"
#include "GoToReturnChecker.h"
#include "IfElseReturnChecker.h"

namespace clang::tidy {
namespace autorefactorings {

/// A module containing checks of the AutoRefactoringModule
class AutoRefactoringModule : public ClangTidyModule {
public:
  void addCheckFactories(ClangTidyCheckFactories &CheckFactories) override {
    CheckFactories.registerCheck<IfElseReturnChecker>("if-else-refactor");
    CheckFactories.registerCheck<CommaInIfChecker>("if-comma-refactor");
    CheckFactories.registerCheck<CallExpInIfChecker>("if-call-refactor");
    CheckFactories.registerCheck<GoToReturnChecker>("goto-return-checker");
  }
};

// Register the AutoRefactoringModule using this statically initialized
// variable.
static ClangTidyModuleRegistry::Add<AutoRefactoringModule>
    X("auto-refactoring-module", "Adds checks for the AutoRefactoringModule.");

} // namespace autorefactorings

// This anchor is used to force the linker to link in the generated object file
// and thus register the AutoRefactoringModule.
volatile int AutoRefactoringModuleAnchorSource = 0;

} // namespace clang::tidy
