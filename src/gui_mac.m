#import <Foundation/Foundation.h>
#import <Cocoa/Cocoa.h>

void MacSetParent(void *_mainView, void *_parentView) {
    NSView *mainView = (NSView *) _mainView;
    NSView *parentView = (NSView *) _parentView;
    // if (mainView.hasSuperView) [mainView removeFromSuperview];
    [parentView addSubview:mainView];
    // mainView.hasSuperView = true;
}

void MacSetVisible(void *_mainView, bool show) {
    NSView *mainView = (NSView *) _mainView;
    [mainView setHidden:(show ? NO : YES)];
}
