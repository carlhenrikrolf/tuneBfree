#define GUI_API CLAP_WINDOW_API_X11

#include <iostream>

#include <elements.hpp>

#include "../libs/elements/lib/host/x11/base_view.cpp"

namespace ce = cycfi::elements;

struct GUI
{
    Display *display;
    Window window;
    ce::window *win;
    ce::view *view;
};

static void GUICreate(MyPlugin *plugin)
{
#ifdef DEBUG_PRINT
    fprintf(stderr, "GUICreate\n");
#endif
    assert(!plugin->gui);
    plugin->gui = (GUI *)calloc(1, sizeof(GUI));

    plugin->gui->display = XOpenDisplay(NULL);
    XSetWindowAttributes attributes = {};
    plugin->gui->window = XCreateWindow(
        plugin->gui->display, DefaultRootWindow(plugin->gui->display), 0, 0, GUI_WIDTH, GUI_HEIGHT,
        0, 0, InputOutput, CopyFromParent, CWOverrideRedirect, &attributes);
    Atom embedInfoAtom = XInternAtom(plugin->gui->display, "_XEMBED_INFO", 0);
    uint32_t embedInfoData[2] = {0 /* version */, 0 /* not mapped */};
    XChangeProperty(plugin->gui->display, plugin->gui->window, embedInfoAtom, embedInfoAtom, 32,
                    PropModeReplace, (uint8_t *)embedInfoData, 2);
    XSizeHints sizeHints = {};
    sizeHints.flags = PMinSize | PMaxSize;
    sizeHints.min_width = sizeHints.max_width = GUI_WIDTH;
    sizeHints.min_height = sizeHints.max_height = GUI_HEIGHT;
    XSetWMNormalHints(plugin->gui->display, plugin->gui->window, &sizeHints);
    XStoreName(plugin->gui->display, plugin->gui->window, pluginDescriptor.name);
    XSelectInput(plugin->gui->display, plugin->gui->window,
                 SubstructureNotifyMask | ExposureMask | PointerMotionMask | ButtonPressMask |
                     ButtonReleaseMask | KeyPressMask | KeyReleaseMask | StructureNotifyMask |
                     EnterWindowMask | LeaveWindowMask | ButtonMotionMask | KeymapStateMask |
                     FocusChangeMask | PropertyChangeMask);
}

static void GUIDestroy(MyPlugin *plugin)
{
#ifdef DEBUG_PRINT
    fprintf(stderr, "GUIDestroy\n");
#endif
    assert(plugin->gui);

    if (plugin->hostPOSIXFDSupport && plugin->hostPOSIXFDSupport->unregister_fd)
    {
        plugin->hostPOSIXFDSupport->unregister_fd(plugin->host,
                                                  ConnectionNumber(plugin->gui->display));
    }

    delete plugin->gui->win;
    delete plugin->gui->view;
    // XCloseDisplay(plugin->gui->display);

    free(plugin->gui);
    plugin->gui = nullptr;
}

static void GUISetParent(MyPlugin *plugin, const clap_window_t *window)
{
#ifdef DEBUG_PRINT
    fprintf(stderr, "GUISetParent\n");
#endif
    XReparentWindow(plugin->gui->display, plugin->gui->window, (Window)window->x11, 0, 0);
    XFlush(plugin->gui->display);
}

static void GUISetVisible(MyPlugin *plugin, bool visible)
{
#ifdef DEBUG_PRINT
    fprintf(stderr, "GUISetVisible\n");
#endif
    if (visible)
        XMapRaised(plugin->gui->display, plugin->gui->window);
    else
        XUnmapWindow(plugin->gui->display, plugin->gui->window);
    XFlush(plugin->gui->display);
}

static void GUIPaint(MyPlugin *plugin, bool internal)
{
#ifdef DEBUG_PRINT
    fprintf(stderr, "GUIPaint\n");
#endif
}

static void GUIOnPOSIXFD(MyPlugin *plugin)
{
#ifdef DEBUG_PRINT
    fprintf(stderr, "GUIOnPOSIXFD\n");
#endif

    Display *display = ce::get_display();
    XFlush(display);

    while (XPending(display))
    {
        XEvent event;
        XNextEvent(display, &event);
        const XEvent &e = (const XEvent &)event;
        ce::on_event(plugin->gui->view, e);
        plugin->gui->view->poll();
        XFlush(display);
    }
}
