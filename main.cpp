#include "mainwindow.h"
#include "mainworker.h"

#include <QApplication>
#include <QThreadPool>
#include <QStandardPaths>

#include "zoptions.h"
#include "zlog.h"

using namespace LibChaos;

#define OPT_VERBOSE "verbose"
#define OPT_FAKE    "fake"
#define OPT_DEVEL   "devel"

const ZArray<ZOptions::OptDef> optdef = {
    { OPT_VERBOSE,  'v', ZOptions::NONE },
    { OPT_FAKE,     0, ZOptions::NONE },    // Add a fake device to device list
    { OPT_DEVEL,    0, ZOptions::NONE },    // Show developer commands and options
};

#define TERM_RESET  "\x1b[m"
#define TERM_RED    "\x1b[31m"
#define TERM_PURPLE "\x1b[35m"

int main(int argc, char *argv[]){
    QApplication::setOrganizationName("pok3r-custom");
    QApplication::setApplicationName("pok3rconf");
    QApplication::setApplicationVersion("0.1");

    ZPath lgf = ZPath(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation).toStdString()) + "logs" + ZLog::genLogFileName("pok3rconf_");
    ZLog::defaultWorker()->logLevelFile(ZLog::INFO, lgf, "[%time%] N %log%");
    ZLog::defaultWorker()->logLevelFile(ZLog::DEBUG, lgf, "[%time%] %thread% D [%function%|%file%:%line%] %log%");
    ZLog::defaultWorker()->logLevelFile(ZLog::ERRORS, lgf, "[%time%] %thread% E [%function%|%file%:%line%] %log%");

    DLOG("Pok3rConf: " _POK3RCONF_DESCRIBE);

    ZOptions options(optdef);
    if(!options.parse(argc, argv))
        return -2;

    ZLog::defaultWorker()->logLevelStdOut(ZLog::INFO, "[%clock%] N %log%");
    ZLog::defaultWorker()->logLevelStdErr(ZLog::ERRORS, TERM_RED "[%clock%] E %log%" TERM_RESET);
    if(options.getOpts().contains(OPT_VERBOSE)){
        ZLog::defaultWorker()->logLevelStdOut(ZLog::DEBUG, TERM_PURPLE "[%clock%] D %log%" TERM_RESET);
    }

    QApplication app(argc, argv);

    MainWindow window(options.getOpts().contains(OPT_DEVEL));
    MainWorker worker(options.getOpts().contains(OPT_FAKE));
    QThread thread;

    // connect worker slots
    qRegisterMetaType<zu64>("zu64");
    qRegisterMetaType<ZString>("ZString");
    qRegisterMetaType<ZArray<KeyboardDevice>>("ZArray<KeyboardDevice>");
    qRegisterMetaType<KeyboardCommand>("KeyboardCommand");
    qRegisterMetaType<ZPointer<Keymap>>("ZPointer<Keymap>");

    // connect worker and window
    QObject::connect(&window, SIGNAL(doRescan()), &worker, SLOT(onDoRescan()));
    QObject::connect(&worker, SIGNAL(rescanDone(ZArray<KeyboardDevice>)), &window, SLOT(onRescanDone(ZArray<KeyboardDevice>)));
    QObject::connect(&window, SIGNAL(kbCommand(zu64,KeyboardCommand,QVariant,QVariant)), &worker, SLOT(onKbCommand(zu64,KeyboardCommand,QVariant,QVariant)));
    QObject::connect(&window, SIGNAL(kbKmUpdate(zu64,ZPointer<Keymap>)), &worker, SLOT(onKbKmUpdate(zu64,ZPointer<Keymap>)));
    QObject::connect(&worker, SIGNAL(commandDone(KeyboardCommand,bool)), &window, SLOT(onCommandDone(KeyboardCommand,bool)));
    QObject::connect(&worker, SIGNAL(keymapUpdate(zu64,ZPointer<Keymap>)), &window, SLOT(onKeymapUpdate(zu64,ZPointer<Keymap>)));
    QObject::connect(&worker, SIGNAL(statusUpdate(ZString)), &window, SLOT(onStatusUpdate(ZString)));
    QObject::connect(&worker, SIGNAL(progressUpdate(int,int)), &window, SLOT(onProgressUpdate(int,int)));
    QObject::connect(&worker, SIGNAL(checkedForUpdate()), &window, SLOT(on_rescanButton_clicked()));

    // start scan when thread starts
    QObject::connect(&thread, SIGNAL(started()), &worker, SLOT(onStartup()));
    //QObject::connect(&thread, SIGNAL(started()), &window, SLOT(on_rescanButton_clicked()));

    // start thread
    worker.moveToThread(&thread);
    thread.start();

    window.show();
    int ret = app.exec();

    // stop thread
    thread.quit();
    if(!thread.wait(1000)){
        ELOG("Terminating thread");
        thread.terminate();
    }

    return ret;
}
