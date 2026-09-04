#include "MainWindow.h"
#include <Application.h>
#include <LayoutBuilder.h>
#include <PopUpMenu.h>
#include <MenuItem.h>
#include <Clipboard.h>
#include <SeparatorView.h>
#include <CheckBox.h>
#include <Directory.h>
#include <File.h>
#include <Node.h>
#include <FindDirectory.h>
#include <Alert.h>
#include <StringView.h>
#include <Notification.h> 
#include <cstdio>
#include <unistd.h>
#include <stdlib.h>

// --- Folder Dialog ---
class FolderDialog : public BWindow {
public:
    FolderDialog(bool* create, bool* neverAsk, sem_id sem) 
        : BWindow(BRect(0,0,1,1), "Downloads Folder", B_MODAL_WINDOW, 
                  B_NOT_RESIZABLE | B_AUTO_UPDATE_SIZE_LIMITS | B_CLOSE_ON_ESCAPE),
          fCreate(create), fNeverAsk(neverAsk), fSem(sem) {
        BLayoutBuilder::Group<>(this, B_VERTICAL, B_USE_DEFAULT_SPACING)
            .SetInsets(B_USE_WINDOW_INSETS)
            .Add(new BStringView("m", "The Downloads folder does not exist. Create it?"))
            .Add(fCheck = new BCheckBox("n", "Don't ask again", nullptr))
            .AddGroup(B_HORIZONTAL).AddGlue()
                .Add(new BButton("c", "Cancel", new BMessage('no')))
                .Add(new BButton("ok", "Create", new BMessage('yes')))
            .End();
        CenterOnScreen();
    }
    void MessageReceived(BMessage* m) {
        if (m->what == 'yes' || m->what == 'no') {
            *fCreate = (m->what == 'yes'); *fNeverAsk = (fCheck->Value() == B_CONTROL_ON);
            delete_sem(fSem); Quit();
        }
    }
    bool QuitRequested() { delete_sem(fSem); return true; }
private: sem_id fSem; bool* fCreate; bool* fNeverAsk; BCheckBox* fCheck;
};

// --- MainWindow ---

MainWindow::MainWindow() 
    : BWindow(BRect(100, 100, 750, 650), "HaikuDownloader", B_TITLED_WINDOW, 
              B_ASYNCHRONOUS_CONTROLS | B_AUTO_UPDATE_SIZE_LIMITS),
      fDownloadThread(-1), fIsDownloading(false) {

    _LoadSettings();

    // 1. Top Controls
    fUrlInput = new BTextControl("url", "URL:", "", nullptr);
    // Note: SetCueText was removed for compatibility as discussed
    
    fPasteBtn = new BButton("paste", "Paste", new BMessage(MSG_PASTE));
    
    // 2. Format and Quality Selectors
    BPopUpMenu* fmtMenu = new BPopUpMenu("f");
    BMessage* mVid = new BMessage(MSG_MODE_CHANGED); mVid->AddInt32("i", 0);
    BMessage* mAud = new BMessage(MSG_MODE_CHANGED); mAud->AddInt32("i", 1);
    fmtMenu->AddItem(new BMenuItem("Video (MP4/MKV)", mVid));
    fmtMenu->AddItem(new BMenuItem("Audio Only", mAud));
    fmtMenu->ItemAt(0)->SetMarked(true);
    fFormatMenu = new BMenuField("Mode:", fmtMenu);

    BPopUpMenu* qMenu = new BPopUpMenu("q");
    qMenu->AddItem(new BMenuItem("1080p", nullptr));
    qMenu->AddItem(new BMenuItem("720p", nullptr));
    qMenu->AddItem(new BMenuItem("480p", nullptr));
    qMenu->AddItem(new BMenuItem("360p", nullptr));
    qMenu->ItemAt(1)->SetMarked(true);
    fQualityMenu = new BMenuField("Quality:", qMenu);

    BPopUpMenu* afMenu = new BPopUpMenu("af");
    afMenu->AddItem(new BMenuItem("MP3", nullptr));
    afMenu->AddItem(new BMenuItem("FLAC (Lossless)", nullptr));
    afMenu->AddItem(new BMenuItem("M4A (Native)", nullptr));
    afMenu->ItemAt(0)->SetMarked(true);
    fAudioFormatMenu = new BMenuField("Audio Format:", afMenu);
    fAudioFormatMenu->Hide(); 

    // 3. Progress Bar
    fStatusBar = new BStatusBar("progress", "Waiting...");
    fStatusBar->SetMaxValue(100.0);

    fLogView = new BTextView("log");
    fLogView->MakeEditable(false);
    fScrollView = new BScrollView("s", fLogView, 0, false, true);

    fStopBtn = new BButton("stop", "Stop", new BMessage(MSG_STOP));
    fStopBtn->SetEnabled(false);
    fDownloadBtn = new BButton("down", "Download", new BMessage(MSG_DOWNLOAD));
    fDownloadBtn->MakeDefault(true);

    // Layout
    BLayoutBuilder::Group<>(this, B_VERTICAL, B_USE_SMALL_SPACING)
        .SetInsets(B_USE_WINDOW_INSETS)
        .AddGroup(B_HORIZONTAL, B_USE_DEFAULT_SPACING)
            .Add(fUrlInput, 10).Add(fPasteBtn, 0)
        .End()
        .AddGroup(B_HORIZONTAL, B_USE_DEFAULT_SPACING)
            .Add(fFormatMenu)
            .Add(fQualityMenu)
            .Add(fAudioFormatMenu)
            .AddGlue()
        .End()
        .AddStrut(5).Add(new BSeparatorView(B_HORIZONTAL)).AddStrut(5)
        .Add(fStatusBar)
        .Add(fScrollView, 100)
        .AddGroup(B_HORIZONTAL, B_USE_DEFAULT_SPACING)
            .Add(fStopBtn).AddGlue().Add(fDownloadBtn)
        .End();

    CenterOnScreen();
}

MainWindow::~MainWindow() {}

void MainWindow::MessageReceived(BMessage* m) {
    if (m->WasDropped()) {
        const char* url;
        if (m->FindString("_data", &url) == B_OK || m->FindString("text/plain", &url) == B_OK) {
            fUrlInput->SetText(url);
            return;
        }
    }

    switch (m->what) {
        case MSG_MODE_CHANGED: {
            int32 i; m->FindInt32("i", &i);
            if (i == 1) { fQualityMenu->Hide(); fAudioFormatMenu->Show(); }
            else { fQualityMenu->Show(); fAudioFormatMenu->Hide(); }
            break;
        }
        case MSG_PROGRESS_UPDATE: {
            float percent; m->FindFloat("p", &percent);
            const char* text; m->FindString("t", &text);
            fStatusBar->Update(percent - fStatusBar->CurrentValue(), text);
            break;
        }
        case MSG_PASTE: _PasteFromClipboard(); break;
        case MSG_DOWNLOAD: _CheckUrlAndStart(); break;
        case MSG_STOP: _StopDownload(); break;
        case MSG_OUTPUT_RECEIVED: {
            const char* t; if (m->FindString("text", &t) == B_OK) _UpdateLog(t);
            break;
        }
        case MSG_DOWNLOAD_FINISHED: {
            fIsDownloading = false;
            fStopBtn->SetEnabled(false);
            fDownloadBtn->SetEnabled(true);
            fUrlInput->SetEnabled(true);
            
            if (fDownloadThread != -1) {
                fStatusBar->SetTo(100.0, "Finished!");
                _SendNotification("HaikuDownloader", "Download completed successfully.");
                fDownloadThread = -1;
            }
            break;
        }
        default: BWindow::MessageReceived(m); break;
    }
}

void MainWindow::_UpdateLog(const char* text) {
    fLogView->Insert(text);
    fLogView->ScrollToOffset(fLogView->TextLength());

    BString s(text);
    int32 pos = s.FindFirst("%");
    if (pos != B_ERROR && pos > 4) {
        BString pStr;
        s.CopyInto(pStr, pos-4, 4);
        float p = atof(pStr.String());
        if (p > 0) {
            BMessage msg(MSG_PROGRESS_UPDATE);
            msg.AddFloat("p", p);
            msg.AddString("t", "Downloading...");
            PostMessage(&msg);
        }
    }
}

void MainWindow::_SendNotification(const char* title, const char* content) {
    BNotification notification(B_INFORMATION_NOTIFICATION);
    notification.SetGroup("HaikuDownloader");
    notification.SetTitle(title);
    notification.SetContent(content);
    notification.Send();
}

void MainWindow::_CheckUrlAndStart() {
    BString url = fUrlInput->Text(); url.Trim();
    if (url.IsEmpty()) return;
    if (!_EnsureDownloadPath()) return;

    fLogView->SetText(""); 
    fStatusBar->SetTo(0, "Starting...");

    if (url.FindFirst("list=") != B_ERROR) {
        BString check = "yt-dlp --ignore-config --flat-playlist --playlist-items 1..2 --get-id \"";
        check << url << "\"";
        FILE* p = popen(check.String(), "r"); int count = 0; char b[128];
        if (p) { while (fgets(b, sizeof(b), p)) count++; pclose(p); }
        if (count > 1) {
            (new BAlert("!", "Playlists are not allowed.", "OK"))->Go();
            fDownloadBtn->SetEnabled(true); fUrlInput->SetEnabled(true);
            return;
        }
    }
    _StartDownload();
}

void MainWindow::_StartDownload() {
    fIsDownloading = true; 
    fStopBtn->SetEnabled(true);
    fDownloadBtn->SetEnabled(false); 
    fUrlInput->SetEnabled(false);
    fDownloadThread = spawn_thread(_DownloadRunner, "dl", B_NORMAL_PRIORITY, this);
    if (fDownloadThread >= B_OK) resume_thread(fDownloadThread);
}

void MainWindow::_StopDownload() {
    if ((new BAlert("?", "Cancel download?", "No", "Yes"))->Go() == 1) {
        system("pkill -9 yt-dlp"); 
        fDownloadThread = -1;
        snooze(200000); 
        _CleanupCorruptFiles();
        fIsDownloading = false; fStopBtn->SetEnabled(false);
        fDownloadBtn->SetEnabled(true); fUrlInput->SetEnabled(true);
        fStatusBar->SetTo(0, "Canceled.");
    }
}

void MainWindow::_CleanupCorruptFiles() {
    BDirectory d(fFinalDownloadPath.String()); BEntry e;
    while (d.GetNextEntry(&e) == B_OK) {
        char n[B_FILE_NAME_LENGTH]; e.GetName(n); BString s(n);
        if (s.EndsWith(".part") || s.EndsWith(".ytdl")) e.Remove();
    }
}

int32 MainWindow::_DownloadRunner(void* data) {
    MainWindow* win = (MainWindow*)data;
    win->Lock();
    BString url = win->fUrlInput->Text();
    BString path = win->fFinalDownloadPath;
    int32 mode = win->fFormatMenu->Menu()->IndexOf(win->fFormatMenu->Menu()->FindMarked());
    int32 qIdx = win->fQualityMenu->Menu()->IndexOf(win->fQualityMenu->Menu()->FindMarked());
    int32 aFmt = win->fAudioFormatMenu->Menu()->IndexOf(win->fAudioFormatMenu->Menu()->FindMarked());
    win->Unlock();

    BString cmd = "yt-dlp --newline --progress --no-playlist -P \"";
    cmd << path << "\" ";
    
    if (mode == 1) {
        const char* formats[] = {"mp3", "flac", "m4a"};
        cmd << "-x --audio-format " << formats[aFmt] << " ";
    } else {
        const char* res[] = {"1080", "720", "480", "360"};
        cmd << "-f \"bestvideo[height<=" << res[qIdx] << "]+bestaudio/best\" --merge-output-format mp4 ";
    }
    
    cmd << "\"" << url << "\" 2>&1";

    FILE* pipe = popen(cmd.String(), "r");
    if (pipe) {
        char buffer[512];
        while (fgets(buffer, sizeof(buffer), pipe)) {
            if (win->fDownloadThread == -1) break;
            BMessage* msg = new BMessage(MSG_OUTPUT_RECEIVED);
            msg->AddString("text", buffer);
            win->PostMessage(msg);
        }
        pclose(pipe);
    }
    win->PostMessage(MSG_DOWNLOAD_FINISHED);
    return 0;
}

void MainWindow::_LoadSettings() {
    BPath p; if (find_directory(B_USER_SETTINGS_DIRECTORY, &p) == B_OK) {
        p.Append("HaikuDownloader_settings"); BFile f(p.Path(), B_READ_ONLY); BMessage s;
        if (s.Unflatten(&f) == B_OK) { fNeverAskAgain = s.GetBool("n", false); fAlwaysCreateFolder = s.GetBool("a", false); }
    }
}

void MainWindow::_SaveSettings() {
    BPath p; if (find_directory(B_USER_SETTINGS_DIRECTORY, &p) == B_OK) {
        p.Append("HaikuDownloader_settings"); BFile f(p.Path(), B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
        BMessage s; s.AddBool("n", fNeverAskAgain); s.AddBool("a", fAlwaysCreateFolder); s.Flatten(&f);
    }
}

bool MainWindow::_EnsureDownloadPath() {
    BPath p; find_directory(B_USER_DIRECTORY, &p); BPath dl = p; dl.Append("Downloads");
    BEntry e(dl.Path()); if (e.Exists()) { fFinalDownloadPath = dl.Path(); return true; }
    bool create = fAlwaysCreateFolder;
    if (!fNeverAskAgain) {
        sem_id s = create_sem(0, "w"); (new FolderDialog(&create, &fNeverAskAgain, s))->Show();
        acquire_sem(s); if (fNeverAskAgain) { fAlwaysCreateFolder = create; _SaveSettings(); }
    }
    fFinalDownloadPath = create ? dl.Path() : p.Path();
    if (create) create_directory(dl.Path(), 0777);
    return true;
}

void MainWindow::_PasteFromClipboard() {
    if (be_clipboard->Lock()) {
        BMessage* d = be_clipboard->Data(); const char* t; ssize_t l;
        if (d->FindData("text/plain", B_MIME_TYPE, (const void**)&t, &l) == B_OK) fUrlInput->SetText(BString(t, l).String());
        be_clipboard->Unlock();
    }
}

bool MainWindow::QuitRequested() { be_app->PostMessage(B_QUIT_REQUESTED); return true; }