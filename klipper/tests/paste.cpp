#include <QGuiApplication>
#include <QDebug>

#include "../systemclipboard/systemclipboard.h"

int main(int argc, char ** argv)
{
    QGuiApplication app(argc, argv);
    auto clip = SystemClipboard::instance();
    QObject::connect(clip, &SystemClipboard::changed, &app, [clip](QClipboard::Mode mode) {
        if (mode != QClipboard::Clipboard) {
            return;
        }
        auto dbg = qDebug();
        dbg << "New clipboard content: ";

        if (clip->mimeData(QClipboard::Clipboard)) {
            dbg << clip->mimeData(QClipboard::Clipboard)->text();
        } else {
            dbg << "[empty]";
        }
    });

    qDebug() << "Watching for new clipboard content...";

    app.exec();
}
