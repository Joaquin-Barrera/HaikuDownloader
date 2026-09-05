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
#include <LocaleRoster.h>
#include <cstdio>
#include <unistd.h>
#include <stdlib.h>

bool gIsSpanish = false;

// --- Folder Dialog ---
class FolderDialog : public BWindow {
public:
    FolderDialog(bool* create, bool* neverAsk, sem_id sem) 
        : BWindow(BRect(0,0,1,1), (gIsSpanish ? "Carpeta de Descargas" : "Downloads Folder"), B_MODAL_WINDOW, 
                  B_NOT_RESIZABLE | B_AUTO_UPDATE_SIZE_LIMITS | B_CLOSE_ON_ESCAPE),
          fCreate(create), fNeverAsk(neverAsk), fSem(sem) {
        
        BStringView* msg = new BStringView("m", gIsSpanish ? "La carpeta Downloads no existe. ¿Desea crearla?" 
                                                          : "The Downloads folder does not exist. Create it?");
        fCheck = new BCheckBox("n", gIsSpanish ? "No volver a preguntar" : "Don't ask again", nullptr);
        BButton* btnCancel = new BButton("c", gIsSpanish ? "Cancelar" : "Cancel", new BMessage('no'));
        BButton* btnCreate = new BButton("ok", gIsSpanish ? "Crear" : "Create", new BMessage('yes'));
        btnCreate->MakeDefault(true);

        BLayoutBuilder::Group<>(this, B_VERTICAL, B_USE_DEFAULT_SPACING)
            .SetInsets(B_USE_WINDOW_INSETS)
            .Add(msg).Add(fCheck).AddStrut(5)
            .AddGroup(B_HORIZONTAL).AddGlue().Add(btnCancel).Add(btnCreate).End();
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

    BMessage languages;
    BLocaleRoster::Default()->GetPreferredLanguages(&languages);
    BString lang;
    if (languages.FindString("language", &lang) == B_OK) {
        if (lang.StartsWith("es")) gIsSpanish = true;
    }

    _LoadSettings();

    fUrlInput = new BTextControl("url", "URL:", "", nullptr);
    fPasteBtn = new BButton("paste", gIsSpanish ? "Pegar" : "Paste", new BMessage(MSG_PASTE));
    
    BPopUpMenu* fmtMenu = new BPopUpMenu("f");
    BMessage* mVid = new BMessage(MSG_MODE_CHANGED); mVid->AddInt32("i", 0);
    BMessage* mAud = new BMessage(MSG_MODE_CHANGED); mAud->AddInt32("i", 1);
    fmtMenu->AddItem(new BMenuItem(gIsSpanish ? "Video (MP4/MKV)" : "Video (MP4/MKV)", mVid));
    fmtMenu->AddItem(new BMenuItem(gIsSpanish ? "Solo Audio" : "Audio Only", mAud));
    fmtMenu->ItemAt(0)->SetMarked(true);
    fFormatMenu = new BMenuField(gIsSpanish ? "Modo:" : "Mode:", fmtMenu);

    BPopUpMenu* qMenu = new BPopUpMenu("q");
    qMenu->AddItem(new BMenuItem("1080p", nullptr));
    qMenu->AddItem(new BMenuItem("720p", nullptr));
    qMenu->AddItem(new BMenuItem("480p", nullptr));
    qMenu->AddItem(new BMenuItem("360p", nullptr));
    qMenu->ItemAt(1)->SetMarked(true);
    fQualityMenu = new BMenuField(gIsSpanish ? "Calidad:" : "Quality:", qMenu);

    BPopUpMenu* afMenu = new BPopUpMenu("af");
    afMenu->AddItem(new BMenuItem("MP3", nullptr));
    afMenu->AddItem(new BMenuItem("FLAC", nullptr));
    afMenu->AddItem(new BMenuItem("M4A", nullptr));
    afMenu->ItemAt(0)->SetMarked(true);
    fAudioFormatMenu = new BMenuField(gIsSpanish ? "Formato Audio:" : "Audio Format:", afMenu);
    fAudioFormatMenu->Hide(); 

    fStatusBar = new BStatusBar("progress", gIsSpanish ? "Esperando..." : "Waiting...");
    fStatusBar->SetMaxValue(100.0);
    fLogView = new BTextView("log");
    fLogView->MakeEditable(false);
    fScrollView = new BScrollView("s", fLogView, 0, false, true);

    fStopBtn = new BButton("stop", gIsSpanish ? "Detener" : "Stop", new BMessage(MSG_STOP));
    fStopBtn->SetEnabled(false);
    fDownloadBtn = new BButton("down", gIsSpanish ? "Descargar" : "Download", new BMessage(MSG_DOWNLOAD));
    fDownloadBtn->MakeDefault(true);

    BLayoutBuilder::Group<>(this, B_VERTICAL, B_USE_SMALL_SPACING)
        .SetInsets(B_USE_WINDOW_INSETS)
        .AddGroup(B_HORIZONTAL, B_USE_DEFAULT_SPACING).Add(fUrlInput, 10).Add(fPasteBtn, 0).End()
        .AddGroup(B_HORIZONTAL, B_USE_DEFAULT_SPACING).Add(fFormatMenu).Add(fQualityMenu).Add(fAudioFormatMenu).AddGlue().End()
        .AddStrut(5).Add(new BSeparatorView(B_HORIZONTAL)).AddStrut(5)
        .Add(fStatusBar).Add(fScrollView, 100)
        .AddGroup(B_HORIZONTAL, B_USE_DEFAULT_SPACING).Add(fStopBtn).AddGlue().Add(fDownloadBtn).End();

    CenterOnScreen();
}

MainWindow::~MainWindow() {}

void MainWindow::MessageReceived(BMessage* m) {
    if (m->WasDropped()) {
        const char* url;
        if (m->FindString("_data", &url) == B_OK || m->FindString("text/plain", &url) == B_OK) {
            fUrlInput->SetText(url); return;
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
            float p; m->FindFloat("p", &p); 
            const char* t; 
            if (m->FindString("t", &t) == B_OK) {
                fStatusBar->Update(p - fStatusBar->CurrentValue(), t); 
            }
            break;
        }
        case MSG_PASTE: _PasteFromClipboard(); break;
        case MSG_DOWNLOAD: _CheckUrlAndStart(); break;
        case MSG_STOP: _StopDownload(); break;
        case MSG_OUTPUT_RECEIVED: {
            const char* t; if (m->FindString("text", &t) == B_OK) _UpdateLog(t); break;
        }
        case MSG_DOWNLOAD_FINISHED:
            fIsDownloading = false; fStopBtn->SetEnabled(false);
            fDownloadBtn->SetEnabled(true); fUrlInput->SetEnabled(true);
            if (fDownloadThread != -1) {
                fStatusBar->SetTo(100.0, gIsSpanish ? "¡Finalizado!" : "Finished!");
                _SendNotification("HaikuDownloader", gIsSpanish ? "Descarga completada." : "Download completed.");
                fDownloadThread = -1;
            }
            break;
        default: BWindow::MessageReceived(m); break;
    }
}

void MainWindow::_UpdateLog(const char* text) {
    fLogView->Insert(text); fLogView->ScrollToOffset(fLogView->TextLength());
    BString s(text); int32 pos = s.FindFirst("%");
    if (pos != B_ERROR && pos > 4) {
        BString pStr; s.CopyInto(pStr, pos-4, 4); float p = atof(pStr.String());
        if (p > 0) {
            BMessage msg(MSG_PROGRESS_UPDATE); msg.AddFloat("p", p);
            msg.AddString("t", gIsSpanish ? "Descargando..." : "Downloading...");
            PostMessage(&msg);
        }
    }
}

void MainWindow::_SendNotification(const char* title, const char* content) {
    BNotification n(B_INFORMATION_NOTIFICATION);
    n.SetGroup("HaikuDownloader"); n.SetTitle(title); n.SetContent(content); n.Send();
}

void MainWindow::_CheckUrlAndStart() {
    BString url = fUrlInput->Text(); url.Trim();
    if (url.IsEmpty()) return;
    if (!_EnsureDownloadPath()) return;

    fLogView->SetText(""); 
    fStatusBar->Reset(gIsSpanish ? "Iniciando..." : "Starting...");

    if (url.FindFirst("list=") != B_ERROR) {
        // En la verificación también quitamos webpositive y usamos extractor-args
        BString check = "yt-dlp --ignore-config --extractor-args \"youtube:player_client=ios,android,web\" --flat-playlist --playlist-items 1..2 --get-id \"";
        check << url << "\"";
        FILE* p = popen(check.String(), "r"); int count = 0; char b[128];
        if (p) { while (fgets(b, sizeof(b), p)) count++; pclose(p); }
        if (count > 1) {
            (new BAlert("!", gIsSpanish ? "No se permiten listas." : "Playlists not allowed.", "OK"))->Go();
            fDownloadBtn->SetEnabled(true); fUrlInput->SetEnabled(true); return;
        }
    }
    _StartDownload();
}

void MainWindow::_StartDownload() {
    fIsDownloading = true; fStopBtn->SetEnabled(true);
    fDownloadBtn->SetEnabled(false); fUrlInput->SetEnabled(false);
    fDownloadThread = spawn_thread(_DownloadRunner, "dl", B_NORMAL_PRIORITY, this);
    resume_thread(fDownloadThread);
}

void MainWindow::_StopDownload() {
    BAlert* a = new BAlert("?", gIsSpanish ? "¿Cancelar descarga?" : "Cancel download?", 
                           gIsSpanish ? "No" : "No", gIsSpanish ? "Sí" : "Yes");
    if (a->Go() == 1) {
        system("pkill -9 yt-dlp"); 
        fDownloadThread = -1;
        snooze(200000); 
        _CleanupCorruptFiles();
        _UpdateLog(gIsSpanish ? "\n🛑 Descarga cancelada por el usuario.\n" 
                              : "\n🛑 Download cancelled by the user.\n");
        fIsDownloading = false; fStopBtn->SetEnabled(false);
        fDownloadBtn->SetEnabled(true); fUrlInput->SetEnabled(true);
        fStatusBar->Reset(gIsSpanish ? "Esperando..." : "Waiting...");
    }
}

void MainWindow::_CleanupCorruptFiles() {
    BDirectory d(fFinalDownloadPath.String()); BEntry e;
    while (d.GetNextEntry(&e) == B_OK) {
        char n[B_FILE_NAME_LENGTH]; e.GetName(n); BString s(n);
        if (s.EndsWith(".part") || s.EndsWith(".ytdl")) e.Remove();
    }
}

bool MainWindow::_EnsureDownloadPath() {
    BPath p; find_directory(B_USER_DIRECTORY, &p); BPath dl = p; dl.Append("Downloads");
    BEntry e(dl.Path());
    if (e.Exists() && e.IsDirectory()) { fFinalDownloadPath = dl.Path(); return true; }
    if (fNeverAskAgain) {
        fFinalDownloadPath = fAlwaysCreateFolder ? dl.Path() : p.Path();
        if (fAlwaysCreateFolder) create_directory(dl.Path(), 0777);
        return true;
    }
    bool create = false;
    sem_id s = create_sem(0, "w");
    (new FolderDialog(&create, &fNeverAskAgain, s))->Show();
    acquire_sem(s);
    if (fNeverAskAgain) _SaveSettings(); 
    fFinalDownloadPath = create ? dl.Path() : p.Path();
    if (create) create_directory(dl.Path(), 0777);
    return true;
}

void MainWindow::_LoadSettings() {
    BPath p; if (find_directory(B_USER_SETTINGS_DIRECTORY, &p) == B_OK) {
        p.Append("HaikuDownloader_settings"); BFile f(p.Path(), B_READ_ONLY); BMessage s;
        if (s.Unflatten(&f) == B_OK) {
            fNeverAskAgain = s.GetBool("n", false);
            fAlwaysCreateFolder = s.GetBool("a", false);
        }
    }
}

void MainWindow::_SaveSettings() {
    BPath p; if (find_directory(B_USER_SETTINGS_DIRECTORY, &p) == B_OK) {
        p.Append("HaikuDownloader_settings"); BFile f(p.Path(), B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
        BMessage s; s.AddBool("n", fNeverAskAgain); s.AddBool("a", fAlwaysCreateFolder); s.Flatten(&f);
    }
}

int32 MainWindow::_DownloadRunner(void* data) {
    MainWindow* win = (MainWindow*)data; win->Lock();
    BString url = win->fUrlInput->Text(); BString path = win->fFinalDownloadPath;
    int32 mode = win->fFormatMenu->Menu()->IndexOf(win->fFormatMenu->Menu()->FindMarked());
    int32 qIdx = win->fQualityMenu->Menu()->IndexOf(win->fQualityMenu->Menu()->FindMarked());
    int32 aFmt = win->fAudioFormatMenu->Menu()->IndexOf(win->fAudioFormatMenu->Menu()->FindMarked());
    win->Unlock();

    // --- NUEVA ESTRATEGIA DE BYPASS ---
    BString cmd = "yt-dlp --newline --progress --no-playlist ";
    // Usamos extractor-args para engañar a YT diciendo que somos un dispositivo móvil
    cmd << "--extractor-args \"youtube:player_client=ios,android,web\" ";
    cmd << "--user-agent \"Mozilla/5.0 (iPhone; CPU iPhone OS 16_5 like Mac OS X) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/16.5 Mobile/15E148 Safari/604.1\" ";
    cmd << "-P \"" << path << "\" ";

    if (mode == 1) { 
        const char* f[] = {"mp3", "flac", "m4a"}; 
        cmd << "-x --audio-format " << f[aFmt] << " "; 
    } else { 
        const char* r[] = {"1080", "720", "480", "360"}; 
        cmd << "-f \"bestvideo[height<=" << r[qIdx] << "]+bestaudio/best\" --merge-output-format mp4 "; 
    }

    cmd << "\"" << url << "\" 2>&1";

    FILE* pipe = popen(cmd.String(), "r");
    if (pipe) { 
        char b[512]; 
        while (fgets(b, sizeof(b), pipe)) { 
            if (win->fDownloadThread == -1) break; 
            BMessage msg(MSG_OUTPUT_RECEIVED); 
            msg.AddString("text", b); 
            win->PostMessage(&msg); 
        } 
        pclose(pipe); 
    }
    win->PostMessage(MSG_DOWNLOAD_FINISHED); 
    return 0;
}

void MainWindow::_PasteFromClipboard() {
    if (be_clipboard->Lock()) {
        BMessage* d = be_clipboard->Data(); const char* t; ssize_t l;
        if (d->FindData("text/plain", B_MIME_TYPE, (const void**)&t, &l) == B_OK) fUrlInput->SetText(BString(t, l).String());
        be_clipboard->Unlock();
    }
}

bool MainWindow::QuitRequested() { be_app->PostMessage(B_QUIT_REQUESTED); return true; }