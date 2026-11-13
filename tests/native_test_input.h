#ifndef NATIVE_TEST_INPUT_H
#define NATIVE_TEST_INPUT_H
/* Posts messages ONLY to this test executable's own GLFW window. */
int native_test_text(void *own_window,const char *text);
int native_test_close(void *own_window);
#endif
