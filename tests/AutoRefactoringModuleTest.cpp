#include "ClangTidyTest.h"
#include "autorefactorings/CallExprInIfChecker.h"
#include "autorefactorings/CommaInIfChecker.h"
#include "autorefactorings/GoToReturnChecker.h"
#include "autorefactorings/IfElseReturnChecker.h"
#include "gtest/gtest.h"

namespace clang {
namespace tidy {
namespace test {

using autorefactorings::CallExpInIfChecker;
using autorefactorings::CommaInIfChecker;
using autorefactorings::GoToReturnChecker;
using autorefactorings::IfElseReturnChecker;

TEST(IfElseReturnCheckerTest, InputAnalyzeMacroExpansion) {
  ClangTidyOptions Opts;
  Opts.CheckOptions["test-check-0.Indent"] = "2";
  Opts.CheckOptions["test-check-0.NeedShift"] = "false";

  const char *PreCode = R"(
#define CHECK(x, y) if (x >= y)

int main(int argc, char** argv) {
  int x = 1;
  CHECK(argc, 5) {
    x = 2;
    x = 3;
  } else {
    x = 1;
  }
})";

  const char *PostCode = R"(
#define CHECK(x, y) if (x >= y)

int main(int argc, char** argv) {
  int x = 1;
  CHECK(argc, 5) {
    x = 2;
    x = 3;
  } else {
    x = 1;
  }
})";

  EXPECT_EQ(PostCode, runCheckOnCode<IfElseReturnChecker>(
                          PreCode, nullptr, "input.cc", {}, Opts));
}

TEST(IfElseReturnCheckerTest, ReverseUOCondition) {
  ClangTidyOptions Opts;
  Opts.CheckOptions["test-check-0.Indent"] = "2";
  Opts.CheckOptions["test-check-0.NeedShift"] = "false";
  Opts.CheckOptions["test-check-0.ReverseOnNotUO"] = "true";

  const char *PreCode = R"(
int main(int argc, char** argv) {
  int x;
  if(!(argc > 5)) {
    x = 2;
    x = 3;
  } else {
    x = 1;
  }
  x = 4;
})";

  const char *PostCode = R"(
int main(int argc, char** argv) {
  int x;
  if(argc > 5) {
    x = 1;
  } else {
    x = 2;
    x = 3;
  }
  x = 4;
})";

  EXPECT_EQ(PostCode, runCheckOnCode<IfElseReturnChecker>(
                          PreCode, nullptr, "input.cc", {}, Opts));
}

TEST(IfElseReturnCheckerTest, InputMacroNegateCheck) {
  ClangTidyOptions Opts;
  Opts.CheckOptions["test-check-0.Indent"] = "2";
  Opts.CheckOptions["test-check-0.NeedShift"] = "false";

  const char *PreCode = R"(
#define CHECK(x, y) (x >= y)

int main(int argc, char** argv) {
  int x = 1;
  if(CHECK(argc, 5)) {
    x = 2;
    x = 3;
  } else {
    x = 1;
  }
})";

  const char *PostCode = R"(
#define CHECK(x, y) (x >= y)

int main(int argc, char** argv) {
  int x = 1;
  if(!(CHECK(argc, 5))) {
    x = 1;
  } else {
    x = 2;
    x = 3;
  }
})";

  EXPECT_EQ(PostCode, runCheckOnCode<IfElseReturnChecker>(
                          PreCode, nullptr, "input.cc", {}, Opts));
}

TEST(IfElseReturnCheckerTest, InputRecursiveReverse) {
  ClangTidyOptions Opts;
  Opts.CheckOptions["test-check-0.Indent"] = "2";
  Opts.CheckOptions["test-check-0.NeedShift"] = "false";

  const char *PreCode = R"(
int main(int argc, char** argv) {
  int x = 1;
  if(argc > 5) {
    x = 1;
    if(argc > 7) {
      int z = 123;
      int u = 345;
      int d = 100;
    } else {
      int a;
      int y;
    }
  } else {
    x = 2;
    x = 3;
  }
  return 228;
})";

  const char *PostCode = R"(
int main(int argc, char** argv) {
  int x = 1;
  if(!(argc > 5)) {
    x = 2;
    x = 3;
  } else {
    x = 1;
    if(!(argc > 7)) {
      int a;
      int y;
    } else {
      int z = 123;
      int u = 345;
      int d = 100;
    }
  }
  return 228;
})";

  EXPECT_EQ(PostCode, runCheckOnCode<IfElseReturnChecker>(
                          PreCode, nullptr, "input.cc", {}, Opts));

  ClangTidyOptions Opts1;
  Opts1.CheckOptions["test-check-0.Indent"] = "2";
  Opts1.CheckOptions["test-check-0.NeedShift"] = "true";

  PostCode = R"(
int main(int argc, char** argv) {
  int x = 1;
  if(!(argc > 5)) {
    x = 2;
    x = 3;
    return 228;
  } 

  x = 1;
  if(!(argc > 7)) {
    int a;
    int y;
    return 228;
  } 

  int z = 123;
  int u = 345;
  int d = 100;
  return 228;
})";

  EXPECT_EQ(PostCode, runCheckOnCode<IfElseReturnChecker>(
                          PreCode, nullptr, "input1.cc", {}, Opts1));
}

TEST(IfElseReturnCheckerTest, InputReverseWithoutNested) {
  ClangTidyOptions Opts;
  Opts.CheckOptions["if-else-refactor.Indent"] = "2";
  Opts.CheckOptions["if-else-refactor.NeedShift"] = "false";

  const char *PreCode = R"(
int main(int argc, char** argv) {
  int x = 1;
  if(argc > 5) {
    x = 1;
    if(argc > 7) {
      int z = 123;
      int u = 345;
    } else {
      int x;
      int y;
      int u = x + y;
    } 
  } else {
    x = 2;
    x = 3;
  }
  return 228;
})";

  const char *PostCode = R"(
int main(int argc, char** argv) {
  int x = 1;
  if(!(argc > 5)) {
    x = 2;
    x = 3;
  } else {
    x = 1;
    if(argc > 7) {
      int z = 123;
      int u = 345;
    } else {
      int x;
      int y;
      int u = x + y;
    } 
  }
  return 228;
})";

  EXPECT_EQ(PostCode, runCheckOnCode<IfElseReturnChecker>(
                          PreCode, nullptr, "input.cc", {}, Opts));

  ClangTidyOptions Opts1;
  Opts1.CheckOptions["test-check-0.Indent"] = "2";
  Opts1.CheckOptions["test-check-0.NeedShift"] = "true";

  PostCode = R"(
int main(int argc, char** argv) {
  int x = 1;
  if(!(argc > 5)) {
    x = 2;
    x = 3;
    return 228;
  } 

  x = 1;
  if(argc > 7) {
    int z = 123;
    int u = 345;
    return 228;
  } 

  int x;
  int y;
  int u = x + y; 
  return 228;
})";

  EXPECT_EQ(PostCode, runCheckOnCode<IfElseReturnChecker>(
                          PreCode, nullptr, "input.cc", {}, Opts1));
}

TEST(IfElseReturnCheckerTest, InputSimpleIfElseWithCmnSucc) {
  ClangTidyOptions Opts;
  Opts.CheckOptions["test-check-0.Indent"] = "2";
  Opts.CheckOptions["test-check-0.NeedShift"] = "false";

  const char *PreCode = R"(
int main(int argc, char** argv) {
  int x = 1;
  if(argc > 5) {
    x =  2;
    x =  3;
  } else {
    x =  1;
  }
  x = 4;
})";

  const char *PostCode = R"(
int main(int argc, char** argv) {
  int x = 1;
  if(!(argc > 5)) {
    x =  1;
  } else {
    x =  2;
    x =  3;
  }
  x = 4;
})";

  EXPECT_EQ(PostCode, runCheckOnCode<IfElseReturnChecker>(
                          PreCode, nullptr, "input.cc", {}, Opts));
}

TEST(IfElseReturnCheckerTest, InputSimpleIfElseWithoutElseBrack) {
  ClangTidyOptions Opts;
  Opts.CheckOptions["test-check-0.Indent"] = "2";
  Opts.CheckOptions["test-check-0.NeedShift"] = "false";

  const char *PreCode = R"(
int main(int argc, char** argv) {
  int x = 1;
  if(argc > 5) {
    x = 2;
    x = 3;
  } else
    x = 1;
  x = 4;
})";

  const char *PostCode = R"(
int main(int argc, char** argv) {
  int x = 1;
  if(!(argc > 5)) 
    x = 1; else{
    x = 2;
    x = 3;
  }
  x = 4;
})";

  EXPECT_EQ(PostCode, runCheckOnCode<IfElseReturnChecker>(
                          PreCode, nullptr, "input.cc", {}, Opts));
}

TEST(IfElseReturnCheckerTest, InputSimpleIfElseWithReverseAndShiftedSemicolon) {
  ClangTidyOptions Opts;
  Opts.CheckOptions["test-check-0.Indent"] = "2";
  Opts.CheckOptions["test-check-0.NeedShift"] = "false";

  const char *PreCode = R"(
int main(int argc, char** argv) {
  int x = 1;
  if(argc > 5) {
    x = 2;
    int z = 123;
    
    x = 3
    ;
  } else {
    x = 1
    
    ;
  }
})";

  const char *PostCode = R"(
int main(int argc, char** argv) {
  int x = 1;
  if(!(argc > 5)) {
    x = 1
    
    ;
  } else {
    x = 2;
    int z = 123;
    
    x = 3
    ;
  }
})";

  EXPECT_EQ(PostCode, runCheckOnCode<IfElseReturnChecker>(
                          PreCode, nullptr, "input.cc", {}, Opts));
}

TEST(GoToReturnCheckerTest, BasicTest) {
  const char *PreCode = R"(
int lol(int y) {
	if (y > 123) {
		goto LAB3;
	}
	return 1;
LAB3:
	y = 5;
}

int main(int argc, char **argv) {
	if (argc > 5) {
		goto LAB1;
	}
	goto LAB2;
LAB1:
	return 123;
LAB2:
	return 456;
})";

  const char *PostCode = R"(
int lol(int y) {
	if (y > 123) {
		goto LAB3;
	}
	return 1;
LAB3:
	y = 5;
}

int main(int argc, char **argv) {
	if (argc > 5) {
		return 123;
	}
	return 456;
LAB1:
	return 123;
LAB2:
	return 456;
})";

  EXPECT_EQ(PostCode,
            runCheckOnCode<GoToReturnChecker>(PreCode, nullptr, "input.cc", {},
                                              ClangTidyOptions()));
}

TEST(CallExpInIfCheckerTest, AutoAllCallExpr) {
  ClangTidyOptions Opts;
  Opts.CheckOptions["test-check-0.UseAuto"] = "true";
  Opts.CheckOptions["test-check-0.UseDeclRefExpr"] = "true";
  Opts.CheckOptions["test-check-0.UseAllCallExpr"] = "true";

  const char *PreCode = R"(
#define NULL                0

int *lol(int size) {
	return NULL;
}

int kek() {
	return 5;
}

double *ahaha() {
	return NULL;
}

double uhuhu() {
	return 5.0;
}


int main(int argc, char** argv) {
	int *x;
	double *y;
	
	if((x = lol(10)) != NULL)
		*x = 1;

	if (kek()) {
		*x = 2;
	}
	
	if ((y = ahaha()) != NULL) {
		*x = 3;
	}
	
	if (uhuhu()) {
		*x = 4;
	}
})";

  const char *PostCode = R"(
#define NULL                0

int *lol(int size) {
	return NULL;
}

int kek() {
	return 5;
}

double *ahaha() {
	return NULL;
}

double uhuhu() {
	return 5.0;
}


int main(int argc, char** argv) {
	int *x;
	double *y;
	
	x = lol(10);
	if(x != NULL)
		*x = 1;

	auto var1 = kek();
	if (var1) {
		*x = 2;
	}
	
	y = ahaha();
	if (y != NULL) {
		*x = 3;
	}
	
	auto var3 = uhuhu();
	if (var3) {
		*x = 4;
	}
})";

  EXPECT_EQ(PostCode, runCheckOnCode<CallExpInIfChecker>(PreCode, nullptr,
                                                         "input.c", {}, Opts));
}

TEST(CallExpInIfCheckerTest, DeclRefExpr) {
  ClangTidyOptions Opts;
  Opts.CheckOptions["test-check-0.UseAuto"] = "true";
  Opts.CheckOptions["test-check-0.UseDeclRefExpr"] = "true";
  Opts.CheckOptions["test-check-0.UseAllCallExpr"] = "false";

  const char *PreCode = R"(
#define NULL                0

int *lol(int size) {
	return NULL;
}

int kek() {
	return 5;
}

double *ahaha() {
	return NULL;
}

double uhuhu() {
	return 5.0;
}


int main(int argc, char** argv) {

	int *x;
	double *y;
	
	if((x = lol(10)) != NULL)
		*x = 1;

	if (kek()) {
		*x = 2;
	}
	
	if ((y = ahaha()) != NULL) {
		*x = 3;
	}
	
	if (uhuhu()) {
		*x = 4;
	}
})";

  const char *PostCode = R"(
#define NULL                0

int *lol(int size) {
	return NULL;
}

int kek() {
	return 5;
}

double *ahaha() {
	return NULL;
}

double uhuhu() {
	return 5.0;
}


int main(int argc, char** argv) {

	int *x;
	double *y;
	
	x = lol(10);
	if(x != NULL)
		*x = 1;

	if (kek()) {
		*x = 2;
	}
	
	y = ahaha();
	if (y != NULL) {
		*x = 3;
	}
	
	if (uhuhu()) {
		*x = 4;
	}
})";

  EXPECT_EQ(PostCode, runCheckOnCode<CallExpInIfChecker>(PreCode, nullptr,
                                                         "input.c", {}, Opts));
}

TEST(CallExpInIfCheckerTest, TypeFilter) {
  ClangTidyOptions Opts;
  Opts.CheckOptions["test-check-0.IgnoreReturnTypePattern"] =
      "(^double$|^int\ \\*$)";

  const char *PreCode = R"(
#define NULL                0

int *lol(int size) {
	return NULL;
}

int kek() {
	return 5;
}

double *ahaha() {
	return NULL;
}

double uhuhu() {
	return 5.0;
}


int main(int argc, char** argv) {
	int x;
	if(lol(10))
		x = 1;

	if (kek()) {
		x = 2;
	}
	
	if (ahaha()) {
		x = 3;
	}
	
	if (uhuhu()) {
		x = 4;
	}
})";

  const char *PostCode = R"(
#define NULL                0

int *lol(int size) {
	return NULL;
}

int kek() {
	return 5;
}

double *ahaha() {
	return NULL;
}

double uhuhu() {
	return 5.0;
}


int main(int argc, char** argv) {
	int x;
	if(lol(10))
		x = 1;

	int var0 = kek();
	if (var0) {
		x = 2;
	}
	
	double * var1 = ahaha();
	if (var1) {
		x = 3;
	}
	
	if (uhuhu()) {
		x = 4;
	}
})";

  EXPECT_EQ(PostCode, runCheckOnCode<CallExpInIfChecker>(PreCode, nullptr,
                                                         "input.c", {}, Opts));
}

TEST(CallExpInIfCheckerTest, InputCompositionCall) {
  ClangTidyOptions Opts;
  Opts.CheckOptions["test-check-0.VariablePrefix"] = "variable";

  const char *PreCode = R"(
#define NULL                0

int lol() {
	return 0;
}

int kek(int u) {
	return 0 + u;
}

int main(int argc, char** argv) {
	int x;
	if(kek(lol())) {
		x = 1;
	}
})";

  const char *PostCode = R"(
#define NULL                0

int lol() {
	return 0;
}

int kek(int u) {
	return 0 + u;
}

int main(int argc, char** argv) {
	int x;
	int variable0 = kek(lol());
	if(variable0) {
		x = 1;
	}
})";

  EXPECT_EQ(PostCode, runCheckOnCode<CallExpInIfChecker>(PreCode, nullptr,
                                                         "input.c", {}, Opts));
}

TEST(CommaInIfCheckerTest, BasicTest) {
  ClangTidyOptions Opts;

  const char *PreCode = R"(
int lol() {
	return 5;
}

int main(int argc, char **argv) {
	int x;
	if((x = 5, x < 10)) {
		x = 1;
	}
	
	int y;
	int z;
	if(x = 6, y = 5, (x + y) < 10) {
		x = 1;
	}
	
	if((z = 7, x = 6, y = 5, (x + y) - z < 10))
		x = 1;
		
	if((x = lol(), y = 5, (x + y) < 10)) {
		x = 1;
	}
	
	int u;
	int t;
	if((x = lol(), y = 5, (x + y) < 10)
		&& (u = 4, t = 228, (u - t) < 0)) {
		x = 1;
	}
})";

  const char *PostCode = R"(
int lol() {
	return 5;
}

int main(int argc, char **argv) {
	int x;
	x = 5;
	if((x < 10)) {
		x = 1;
	}
	
	int y;
	int z;
	x = 6;
	y = 5;
	if((x + y) < 10) {
		x = 1;
	}
	
	z = 7;
	x = 6;
	y = 5;
	if(((x + y) - z < 10))
		x = 1;
		
	x = lol();
	y = 5;
	if(((x + y) < 10)) {
		x = 1;
	}
	
	int u;
	int t;
	if((x = lol(), y = 5, (x + y) < 10)
		&& (u = 4, t = 228, (u - t) < 0)) {
		x = 1;
	}
})";

  EXPECT_EQ(PostCode, runCheckOnCode<CommaInIfChecker>(PreCode, nullptr,
                                                       "input.c", {}, Opts));
}

} // namespace test
} // namespace tidy
} // namespace clang
