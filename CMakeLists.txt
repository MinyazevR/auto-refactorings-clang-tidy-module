set(LLVM_LINK_COMPONENTS
  FrontendOpenMP
  Support
  )

add_compile_options(-fsanitize=address)
add_link_options(-fsanitize=address)

add_clang_library(AutoRefactoringModule STATIC
  AutoRefactoringModule.cpp
  IfElseReturnChecker.cpp
  CommaInIfChecker.cpp
  LINK_LIBS
  clangTidy
  clangTidyUtils

  DEPENDS
  omp_gen
  ClangDriverOptions
  )

clang_target_link_libraries(AutoRefactoringModule
  PRIVATE
  clangAnalysis
  clangAST
  clangASTMatchers
  clangBasic
  clangLex
  clangSerialization
  clangTooling
  clang
  clangFrontend
  clangParse
  clangSema
  clangEdit
  clangSupport
  clangAPINotes
  clangRewrite
  asan
)

