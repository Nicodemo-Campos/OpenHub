#include "app/MainWindow.hpp"

#include <QApplication>
#include <QCoreApplication>

#ifndef OPENHUB_VERSION
#define OPENHUB_VERSION "dev"
#endif

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    QCoreApplication::setApplicationName(QStringLiteral("OpenHub"));
    QCoreApplication::setOrganizationName(QStringLiteral("OpenHub"));
    QCoreApplication::setApplicationVersion(QString::fromLatin1(OPENHUB_VERSION));

    openhub::MainWindow window;
    window.show();

    return app.exec();
}
