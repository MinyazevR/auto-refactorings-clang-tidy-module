option(ENABLE_ASAN "Enable ASAN" OFF)

set(LLVM_LINK_COMPONENTS
  FrontendOpenMP
  Support
  )

if(ENABLE_ASAN)
    add_compile_options(-fsanitize=address -fsanitize=undefined)
    add_link_options(-fsanitize=address -fsanitize=undefined)
endif()

add_clang_library(clangTidyAutoRefactoringModule STATIC
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

if(ENABLE_ASAN)
  clang_target_link_libraries(clangTidyAutoRefactoringModule
    PRIVATE
    asan
    ubsan
  )
endif()

clang_target_link_libraries(clangTidyAutoRefactoringModule
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
)
