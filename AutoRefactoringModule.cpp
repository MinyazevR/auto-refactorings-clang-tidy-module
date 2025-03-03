#include "../ClangTidy.h"
#include "../ClangTidyModule.h"
#include "../ClangTidyModuleRegistry.h"
#include "IfElseReturnChecker.h"
#include "CommaInIfChecker.h"


namespace clang::tidy {
namespace autorefactorings {

/// A module containing checks of the AutoRefactoringModule
class AutoRefactoringModule : public ClangTidyModule {
public:
  void addCheckFactories(ClangTidyCheckFactories &CheckFactories) override {
    CheckFactories.registerCheck<IfElseReturnChecker>("if-else-refactor");
    CheckFactories.registerCheck<CommaInIfChecker>("if-comma-refactor");
  }
};

// Register the AutoRefactoringModule using this statically initialized variable.
static ClangTidyModuleRegistry::Add<AutoRefactoringModule>
    X("auto-refactoring-module", "Adds checks for the AutoRefactoringModule.");

} // namespace autorefactorings

// This anchor is used to force the linker to link in the generated object file
// and thus register the AutoRefactoringModule.
volatile int AutoRefactoringModuleAnchorSource = 0;

} // namespace clang::tidy
