//Main entry point for the video player application, initializing QML and network video source
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include "VideoFrameProvider.h"
#include "NetworkVideoSource.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    QQmlApplicationEngine engine;

    // Create VideoFrameProvider instance and set as context property
    VideoFrameProvider* provider = new VideoFrameProvider(&engine);
    engine.rootContext()->setContextProperty("videoFrameProvider", provider);

    // Create and start network video source
    auto videoSource = std::make_unique<NetworkVideoSource>();
    videoSource->start("192.168.31.13", 12345, [provider](const char* data, int size, int width, int height) {
        provider->presentFrame(QByteArray(data, size),
                             width,
                             height,
                             QVideoFrameFormat::Format_YUV420P);
    });

    const QUrl url(u"qrc:/VideoPlayer/Main.qml"_qs);
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);
    engine.load(url);

    // No need to manually delete after using smart pointers
    QObject::connect(&app, &QGuiApplication::aboutToQuit, [&videoSource]() {
        videoSource.reset();
    });

    return app.exec();
}
