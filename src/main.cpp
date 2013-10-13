// Copyright (C) 2012, Chris J. Foster and the other authors and contributors
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// * Redistributions of source code must retain the above copyright notice,
//   this list of conditions and the following disclaimer.
// * Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions and the following disclaimer in the documentation
//   and/or other materials provided with the distribution.
// * Neither the name of the software's owners nor the names of its
//   contributors may be used to endorse or promote products derived from this
//   software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
// (This is the BSD 3-clause license)

#include "mainwindow.h"
#include "ptview.h"

#include <QtCore/QTimer>
#include <QtGui/QApplication>
#include <QtOpenGL/QGLFormat>

#include "argparse.h"
#include "config.h"
#include "displazserver.h"


QStringList g_initialFileNames;
static int storeFileName (int argc, const char *argv[])
{
    for(int i = 0; i < argc; ++i)
        g_initialFileNames.push_back (argv[i]);
    return 0;
}


int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    ArgParse::ArgParse ap;
    bool printVersion = false;
    bool printHelp = false;
    int maxPointCount = 200000000;
    std::string serverName = "default";
    bool remoteMode = true;

    ap.options(
        "displaz - view a LAS point cloud\n"
        "Usage: displaz [opts] [file1.las ...]",
        "%*", storeFileName, "",
        "--maxpoints %d", &maxPointCount, "Maximum number of points to load at a time",
        "--noremote %!",  &remoteMode,    "Don't attempt to open files in existing window",
        "--servername %s", &serverName,   "Name of displaz instance to message on startup",
        "--version",      &printVersion,  "Print version number",
        "--help",         &printHelp,     "Print command line usage help",
        NULL
    );

    if(ap.parse(argc, const_cast<const char**>(argv)) < 0)
    {
        std::cerr << ap.geterror() << std::endl;
        ap.usage();
        return EXIT_FAILURE;
    }

    if (printVersion)
    {
        std::cout << "version " DISPLAZ_VERSION_STRING "\n";
        return EXIT_SUCCESS;
    }
    if (printHelp)
    {
        ap.usage();
        return EXIT_SUCCESS;
    }

    QString socketName = QString::fromStdString("displaz-ipc-" + serverName);
    if (remoteMode)
    {
        QDir currentDir = QDir::current();
        QLocalSocket socket;
        // Attempt to locate a running displaz instance
        socket.connectToServer(socketName);
        if (socket.waitForConnected(100))
        {
            if (g_initialFileNames.empty())
            {
                std::cerr << "No files to open in existing window, exiting.\n";
                return EXIT_FAILURE;
            }
            QByteArray command("OPEN_FILES");
            for (int i = 0; i < g_initialFileNames.size(); ++i)
            {
                command += "\n";
                command += currentDir.absoluteFilePath(g_initialFileNames[i]).toUtf8();
            }
            socket.write(command);
            socket.disconnectFromServer();
            socket.waitForDisconnected(10000);
            return EXIT_SUCCESS;
        }
    }
    // If we didn't find any other running instance, start up a server to
    // accept incoming connections, if desired
    std::unique_ptr<DisplazServer> server;
    if (remoteMode)
        server.reset(new DisplazServer(socketName));

    // Multisampled antialiasing - this makes rendered point clouds look much
    // nicer, but also makes the render much slower, especially on lower
    // powered graphics cards.
    //QGLFormat f = QGLFormat::defaultFormat();
    //f.setSampleBuffers(true);
    //QGLFormat::setDefaultFormat(f);

    PointViewerMainWindow window;
    if (remoteMode)
    {
        QObject::connect(server.get(), SIGNAL(messageReceived(QByteArray)),
                         &window, SLOT(runCommand(QByteArray)));
    }
    window.captureStdout();
    window.pointView().setMaxPointCount(maxPointCount);
    window.show();

    // Open initial files only after event loop has started.  Use a small but
    // nonzero timeout to give the GUI a chance to draw itself which makes
    // things slightly less ugly.
    QTimer::singleShot(200, &window, SLOT(openInitialFiles()));

    return app.exec();
}

