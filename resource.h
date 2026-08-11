#pragma once

// NOTE: keep this file pure ASCII (English comments only).
// VS resource editor saves it using the system ANSI codepage; Chinese
// comments would turn into mojibake and trigger C4828 warnings.

// Icon
#define IDI_APP_ICON                    101

// Dialog
#define IDD_MAIN_DIALOG                 102

// Main dialog controls
#define IDC_BTN_NETWORK                 1001   // unlock network
#define IDC_BTN_USB                     1002   // unlock usb
#define IDC_BTN_JIYU                    1003   // start/kill jiyu
#define IDC_BTN_KEYBOARD                1004   // unlock keyboard
#define IDC_BTN_WINDOWIZE               1005   // windowize/fullscreen broadcast
#define IDC_CHK_AUTO                    1006   // auto windowize broadcast
#define IDC_CHK_HOTKEY                  1007   // CTRL+Q hotkey
#define IDC_LOG                         1008   // log edit box
#define IDC_STATUS                      1009   // status text
#define IDC_LINK_WEBSITE                1010   // website link
#define IDC_BTN_SUSPEND                 1011   // suspend/resume jiyu
#define IDC_BTN_BLACKSCREEN             1012   // exit black screen
#define IDC_CHK_TOPMOST                 1013   // topmost window
#define IDC_CHK_ANTICAPTURE             1014   // prevent screen capture
#define IDC_CHK_HOTKEY_W                1015   // CTRL+W hotkey
#define IDC_BTN_RESTORE                 1016   // restore limits

// Timer
#define TIMER_PERIODIC                  1      // periodic poll every 2s
