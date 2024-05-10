#if defined(_WIN32)
#define GUI_API CLAP_WINDOW_API_WIN32
#include <windowsx.h>

struct GUI
{
    HWND window;
    ce::window *win;
    ce::view *view;
};

static int globalOpenGUICount = 0;

static void GUIPaint(MyPlugin *plugin, bool internal)
{
    // if (internal) PluginPaint(plugin, plugin->gui->bits);
    RedrawWindow(plugin->gui->window, 0, 0, RDW_INVALIDATE);
}

LRESULT CALLBACK GUIWindowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    MyPlugin *plugin = (MyPlugin *)GetWindowLongPtr(window, 0);

    if (!plugin)
    {
        return DefWindowProc(window, message, wParam, lParam);
    }

    if (message == WM_PAINT)
    {
        PAINTSTRUCT paint;
        HDC dc = BeginPaint(window, &paint);
        BITMAPINFO info = {{sizeof(BITMAPINFOHEADER), GUI_WIDTH, -GUI_HEIGHT, 1, 32, BI_RGB}};
        // StretchDIBits(dc, 0, 0, GUI_WIDTH, GUI_HEIGHT, 0, 0, GUI_WIDTH, GUI_HEIGHT,
        // plugin->gui->bits, &info, DIB_RGB_COLORS, SRCCOPY);
        EndPaint(window, &paint);
    }
    else if (message == WM_MOUSEMOVE)
    {
        // PluginProcessMouseDrag(plugin, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        GUIPaint(plugin, true);
    }
    else if (message == WM_LBUTTONDOWN)
    {
        SetCapture(window);
        // PluginProcessMousePress(plugin, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        GUIPaint(plugin, true);
    }
    else if (message == WM_LBUTTONUP)
    {
        ReleaseCapture();
        // PluginProcessMouseRelease(plugin);
        GUIPaint(plugin, true);
    }
    else
    {
        return DefWindowProc(window, message, wParam, lParam);
    }

    return 0;
}

static void GUICreate(MyPlugin *plugin)
{
    assert(!plugin->gui);
    plugin->gui = (GUI *)calloc(1, sizeof(GUI));

    if (globalOpenGUICount++ == 0)
    {
        WNDCLASS windowClass = {};
        windowClass.lpfnWndProc = GUIWindowProcedure;
        windowClass.cbWndExtra = sizeof(MyPlugin *);
        windowClass.lpszClassName = (LPCWSTR)pluginDescriptor.id;
        windowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
        windowClass.style = CS_HREDRAW | CS_VREDRAW;
        RegisterClass(&windowClass);
    }

    plugin->gui->window = CreateWindow((LPCWSTR)pluginDescriptor.id, (LPCWSTR)pluginDescriptor.name,
                                       WS_CHILDWINDOW | WS_CLIPSIBLINGS, CW_USEDEFAULT, 0,
                                       GUI_WIDTH, GUI_HEIGHT, GetDesktopWindow(), NULL, NULL, NULL);
    // plugin->gui->bits = (uint32_t *) calloc(1, GUI_WIDTH * GUI_HEIGHT * 4);
    SetWindowLongPtr(plugin->gui->window, 0, (LONG_PTR)plugin);

    // PluginPaint(plugin, plugin->gui->bits);
    plugin->gui->win = new ce::window("Launchpad tuner", 0, ce::rect{0, 0, GUI_WIDTH, GUI_HEIGHT},
                                      plugin->gui->window);
    plugin->gui->win->on_close = []() {};
    plugin->gui->view = new ce::view(*plugin->gui->win);
}

static void GUIDestroy(MyPlugin *plugin)
{
    assert(plugin->gui);
    delete plugin->gui->win;
    delete plugin->gui->view;
    DestroyWindow(plugin->gui->window);
    // free(plugin->gui->bits);
    free(plugin->gui);
    plugin->gui = nullptr;

    if (--globalOpenGUICount == 0)
    {
        UnregisterClass((LPCWSTR)pluginDescriptor.id, NULL);
    }
}

#define GUISetParent(plugin, parent) SetParent((plugin)->gui->window, (HWND)(parent)->win32)
#define GUISetVisible(plugin, visible)                                                             \
    ShowWindow((plugin)->gui->window, (visible) ? SW_SHOW : SW_HIDE)
static void GUIOnPOSIXFD(MyPlugin *) {}
#elif defined(__linux__)
#define GUI_API CLAP_WINDOW_API_X11

#include <iostream>

#include "../libs/elements/lib/host/x11/base_view.cpp"

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
    plugin->gui->win = new ce::window("Launchpad tuner", 0, ce::rect{0, 0, GUI_WIDTH, GUI_HEIGHT},
                                      plugin->gui->window);
    plugin->gui->win->on_close = []() {};
    plugin->gui->view = new ce::view(*plugin->gui->win);
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

#elif defined(__APPLE__)
#define GUI_API CLAP_WINDOW_API_COCOA

struct GUI
{
    ce::window *win;
    ce::view *view;
};

extern "C" void MacSetParent(void *_mainView, void *_parentView);
extern "C" void MacSetVisible(void *_mainView, bool show);

static void GUICreate(MyPlugin *plugin)
{
    assert(!plugin->gui);
    plugin->gui = (GUI *)calloc(1, sizeof(GUI));
    plugin->gui->win = new ce::window("tuneBfree", 0, ce::rect{0, 0, GUI_WIDTH, GUI_HEIGHT});
    plugin->gui->win->on_close = []() {};
    plugin->gui->view = new ce::view(*plugin->gui->win);
}

static void GUIDestroy(MyPlugin *plugin)
{
    assert(plugin->gui);
    free(plugin->gui);
    plugin->gui = nullptr;
}

static void GUISetParent(MyPlugin *plugin, const clap_window_t *parent)
{
    MacSetParent(plugin->gui->view->host(), parent->cocoa);
}
static void GUISetVisible(MyPlugin *plugin, bool visible)
{
    MacSetVisible(plugin->gui->view->host(), visible);
}
static void GUIOnPOSIXFD(MyPlugin *) {}
#endif
