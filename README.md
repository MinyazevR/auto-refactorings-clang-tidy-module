# The clang-tidy module for performing custom code auto-factoring.

## Build
To build the project, run build.sh.
```sh
bash build.sh
```
### Build environment
ENABLE_ASAN: Build with AddressSanitizer and UndefinedBehaviorSanitizer.

LLVM_VERSION: The version of LLVM for which you need to build clang-tidy.

BUILD_TYPE: Build type. As example: Release.

## Tests
To run the tests:
```sh
bash ./tests/run_tests.sh <path-to-clang-tidy>
```
## Usage
```sh
<path-to-clang-tidy> <clang-tidy-options>
```

## Available Checkers
### if-call-refactor
Making function calls from a conditional If statement.

Before:
```c
int function() {
  return 0;
}

int main(int argc, char **argv) {
  if (function()) {
    do_something();
  }
}

```
After: 
```c
int function() {
  return 0;
}

int main(int argc, char **argv) {
  int variable = function();
  if (variable) {
    do_something();
  }
}

```

#### Options:
| Option | Default Value | Description |
| :---      | :---:  |     :---:      |
| UseAuto: boolean   | false | When using C++, it is possible to use automatic type inference with the auto keyword.     |
| UseDeclRefExpr: boolean   | true | Make function calls that do not require declaring a new variable. |
| UseAllCallExpr :boolean | true |  Make all function calls. |
| FromSystemCHeader: boolean | false | Making function calls from system headers |
| VariablePrefix: str | conditionVariable | A prefix for naming variables formed after taking out a function |
| Filter: str (POSIX ERE) | .*| A regular expression that allows you to filter by the name of the functions that need to be processed. |
| IgnorePattern: str (POSIX ERE)| " " |A regular expression that allows you to filter functions by name that do not need to be processed. |
| ReturnTypePattern: str (POSIX ERE)| .* | A regular expression that allows you to filter functions that need to be processed by the type of the returned value. |

An example for the following configuration
```
Checks: '-*,if-call-refactor'
CheckOptions: 
  if-call-refactor.UseAuto: true
  if-call-refactor.UseDeclRefExpr: true
  if-call-refactor.UseAllCallExpr: true
  if-call-refactor.FromSystemCHeader: false
  if-call-refactor.VariablePrefix: variable
```

Before:
```c
#include <stdio.h>
#include <string.h>

void run(char *buf) {
  char *begin_buf = buf;
  if (strncmp(buf, "null", 4) == 0) {
    printf("%d", 1);
  }
}

int *function() {
  return something;
}

int function1() {
  return something;
}

int main(int argc, char **argv) {
  int y;
  if (function1()) {
    do_something();
  }

  if ((y = function()) != NULL)) {
    do_something();
  }
}

```
After: 
```c
#include <stdio.h>
#include <string.h>

void run(char *buf) {
  char *begin_buf = buf;
  if (strncmp(buf, "null", 4) == 0) {
    printf("%d", 1);
  }
}

int *function() {
  return something;
}

int function1() {
  return something;
}

int main(int argc, char **argv) {
  int y;
  auto variable0 = function1();
  if (variable0) {
    do_something();
  }
  y = function();
  if (y != NULL)) {
    do_something();
  }
}

```
### if-comma-refactor
A semicolon expression can be extracted from the if condition.
Before:
```c
if (a, b, c) {
// <...>
}
```
After: 
```c
a;
b;
if (c) {
// <...>
}
```

### if-else-refactor
Reverse the branches of the if statement.
Before:
```c
#include <stdio.h>

int main(int argc, char** argv) {
  if(argc > 1) {
    printf("%d", 1);
    if (argc > 3) {
      printf("%d", 3);
    }
  } else {
    printf("%d", 2);
  }
  return 3;
}
```
After: 
```c
#include <stdio.h>

int main(int argc, char** argv) {
  if(argc <= 1) {
    printf("%d", 2);
    return 3;
  } 

  printf("%d", 1);
  if (argc > 3) {
    printf("%d", 3);
  }
  return 3;
}

```

#### Options:
| Option | Default Value | Description |
| :---      | :---:  |     :---:      |
| Indent: int   | 4 | The indentation used in the project (to move the else block to the left).     |
| NeedShift: boolean   | false | Move else out of the scope. |
| ReverseOnNotUO :boolean | false |  When selecting a new then, the length of the source blocks is taken into account and the smallest is selected, but you can change the way you select the first branch. In this case, then the branch changes if the if condition contains an explicit unary operator "!". |

An example for the following configuration
```
Checks: '-*,if-else-refactor'
CheckOptions: 
  if-else-refactor.NeedShift: false
  if-else-refactor.Indent: 2
```

Before:
```c
#include <stdio.h>

int main(int argc, char** argv) {
  if(argc > 5) {
    printf("%d", 2);
    printf("%d", 3);
  } else {
    printf("%d", 1);
  }
}

```
After: 
```c
#include <stdio.h>

int main(int argc, char** argv) {
  if(!(argc > 5)) {
    printf("%d", 1);
  } else {
    printf("%d", 2);
    printf("%d", 3);
  }
}
```
An example for the following configuration
```
Checks: '-*,if-else-refactor'
CheckOptions: 
  if-else-refactor.NeedShift: true
  if-else-refactor.Indent: 2
```
Before:
```c
#include <stdio.h>

int main(int argc, char** argv) {
  if(argc > 5) {
    printf("%d", 2);
    printf("%d", 3);
  } else {
    printf("%d", 1);
  }
  return 0;
}

```
After: 
```c
#include <stdio.h>

int main(int argc, char** argv) {
  if(!(argc > 5)) {
    printf("%d", 1);
    return 0;
  } 

  printf("%d", 2);
  printf("%d", 3);
  return 0;
}
