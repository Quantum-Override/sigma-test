1. when test fails, fail message is not output
2. test case teardown outputs `Running Teardown` (remove)
3. any test case case logging (writelnf, etc) is output _before_ the test case entry
4. test case teardown output log before the test case entry
  - it looks like teardown runs before the test case
