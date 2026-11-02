#include "platform.h"
#include "backend.h"
#include <glad/gl.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <stdio.h>

static GLFWwindow *window;
static bool hidden, keyboard;
static double deadline;

static void glfw_error(int code, const char *message) {
    fprintf(stderr, "SWARM_ERROR glfw %d: %s\n", code, message);
}

bool zrg_glfw_open(bool headless, bool interactive) {
    glfwSetErrorCallback(glfw_error);
    glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);
    if (!glfwInit()) return false;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_VISIBLE, headless ? GLFW_FALSE : GLFW_TRUE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    glfwWindowHint(GLFW_SAMPLES, 0);
    glfwWindowHint(GLFW_SRGB_CAPABLE, GLFW_FALSE);
    window = glfwCreateWindow(ZRG_RENDER_WIDTH, ZRG_RENDER_HEIGHT,
        interactive ? "Swarm Arena - WASD move, Esc close" : "Swarm Arena - bot, Esc close", NULL, NULL);
    if (!window) { glfwTerminate(); return false; }
    glfwMakeContextCurrent(window);
    if (!gladLoadGL((GLADloadfunc)glfwGetProcAddress) || !GLAD_GL_VERSION_3_3) {
        fprintf(stderr, "SWARM_ERROR glfw OpenGL 3.3 core unavailable\n");
        zrg_glfw_close();
        return false;
    }
    glfwSwapInterval(0);
    hidden = headless; keyboard = interactive && !headless;
    deadline = glfwGetTime();
    return true;
}

bool zrg_glfw_poll(void) {
    glfwPollEvents();
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) glfwSetWindowShouldClose(window, GLFW_TRUE);
    return !glfwWindowShouldClose(window);
}

void zrg_glfw_input(int64_t axes[2]) {
    axes[0] = axes[1] = 0;
    if (!keyboard || !glfwGetWindowAttrib(window, GLFW_FOCUSED)) return;
    axes[0] = (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) - (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS);
    axes[1] = (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) - (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS);
}

void zrg_glfw_present(void) {
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    glViewport(0, 0, width, height);
    zrg_backend_present();
    glfwSwapBuffers(window);
    if (!hidden) {
        deadline += 1.0 / 60.0;
        double now = glfwGetTime();
        if (deadline < now) deadline = now;
        while (!glfwWindowShouldClose(window) && (now = glfwGetTime()) < deadline)
            glfwWaitEventsTimeout(deadline - now);
    }
}

void zrg_glfw_close(void) {
    if (window) glfwDestroyWindow(window);
    window = NULL;
    glfwTerminate();
}
