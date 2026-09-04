#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <Window.h>
#include <TextControl.h>
#include <Button.h>
#include <MenuField.h>
#include <TextView.h>
#include <ScrollView.h>
#include <StatusBar.h>
#include <String.h>
#include <Path.h>
#include <OS.h>

enum {
    MSG_DOWNLOAD = 'down',
    MSG_STOP     = 'stop',
    MSG_PASTE    = 'past',
    MSG_MODE_CHANGED = 'mchg',
    MSG_OUTPUT_RECEIVED = 'outp',
    MSG_PROGRESS_UPDATE = 'prog',
    MSG_DOWNLOAD_FINISHED = 'fini'
};

class MainWindow : public BWindow {
public:
    MainWindow();
    virtual ~MainWindow();
    
    virtual void MessageReceived(BMessage* message) override;
    virtual bool QuitRequested() override;

private:
    void _CheckUrlAndStart();
    void _StartDownload();
    void _StopDownload();
    void _PasteFromClipboard();
    void _UpdateLog(const char* text);
    void _LoadSettings();
    void _SaveSettings();
    bool _EnsureDownloadPath(); 
    void _CleanupCorruptFiles();
    void _SendNotification(const char* title, const char* content);
    void _WriteTrackerAttributes(BPath filePath, BString url);

    static int32 _DownloadRunner(void* data);

    BTextControl* fUrlInput;
    BButton*      fDownloadBtn;
    BButton*      fStopBtn;
    BButton*      fPasteBtn;
    BMenuField*   fFormatMenu;
    BMenuField*   fQualityMenu;
    BMenuField*   fAudioFormatMenu; // Nuevo: formatos de audio extra
    BStatusBar*   fStatusBar;       // Nuevo: Barra de progreso visual
    BTextView*    fLogView;
    BScrollView*  fScrollView;
    
    thread_id     fDownloadThread;
    bool          fIsDownloading;

    bool          fNeverAskAgain;
    bool          fAlwaysCreateFolder;
    BString       fFinalDownloadPath;
};

#endif