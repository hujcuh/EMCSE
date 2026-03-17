#include "stdafx.h"
#include "EMCSE.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    EMCSE window;
    window.show();
    return app.exec();
}
